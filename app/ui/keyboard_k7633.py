"""
Kasten „AUP" — Bildschirmtastatur K7633, Kundentastatur, Schluesselschalter, Lampen
===================================================================================

Oberflaeche der Option *Einstellungen > Allgemein > AUP (Sparkasse)* (Entwurf 33 N10e,
Kern: ``core/cards/aup/``, C-ABI ``k1520_aup_*``, Analyse ``doc/a5130_k8924/aup_analyse.md``).
Den Kasten gibt es nur mit gesteckter Karte; die vorhandene Tastatur (K7636/K7637) bleibt
unberuehrt.  Kein Tastenkuerzel (die Kuerzeltabelle des Handbuchs ist ein Vertrag), und die
Wirtstastatur ist NICHT zugeordnet: der Beleg gibt keine Zuordnung her, geraten wird nicht.

**Tastenlage.**  Die K7633 ist ein Positionsraster Zeile A-F x Spalte 01-13 (Tastennamen des
Belegs, ``k1520_aup_tastenname``); gezeichnet wird dieses Raster, NICHT die Fotogeometrie
(die Beschriftung der Kappen steht nur auf dem Foto).  Jede Taste zeigt ihren Positionsnamen
und den Code ohne Alpha; der Tooltip nennt Position und beide Codes.  Die Kundentastatur ist
ein Ziffernblock; die Anordnung (7-8-9 oben) ist Darstellung, die Positionen 3CH-47H sind
die des Belegs.  Die beiden Tasten F3D/F3F tragen beide den Code 19H (Bedeutung offen).

**Schluesselschalter** SS1/SS2 sind keine Tasten, sondern Schalter (``k1520_aup_schluessel``,
jede Drehung - ein UND aus - sendet AAH bzw. A9H); ihre Stellung wird nach Neuaufbau, ``restore`` und Start aus
dem Kern gelesen (``k1520_aup_schluessel_stand``).

**Lampen** (``k1520_aup_lampen``): SEL, ALPHA, GET und Fehler.  Die Fehlerlampe BLINKT -
das ist Sache der Oberflaeche (Kommandobit 5 sagt nur ein/aus).  **Hupe**: steigt der Zaehler
``k1520_aup_hupe``, blitzt die Anzeige kurz und ein Ton kommt ueber ``QApplication.beep()``
(keine neue Abhaengigkeit; faellt der Ton aus, bleibt die optische Anzeige).
"""

from __future__ import annotations

import re
from typing import Dict, Tuple

from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtWidgets import (
    QApplication, QCheckBox, QGridLayout, QGroupBox, QHBoxLayout, QLabel, QPushButton,
    QVBoxLayout, QWidget,
)

from app.core_binding.k1520 import K1520Emulator, kern_sperre

K7633, KUNDE = K1520Emulator.AUP_K7633, K1520Emulator.AUP_KUNDE
LAMPE_SEL = K1520Emulator.AUP_LAMPE_SEL
LAMPE_ALPHA = K1520Emulator.AUP_LAMPE_ALPHA
LAMPE_GET = K1520Emulator.AUP_LAMPE_GET
LAMPE_FEHLER = K1520Emulator.AUP_LAMPE_FEHLER

#: Blinktakt der Fehlerlampe (halbe Periode) und Dauer des Hupenblitzes, Millisekunden.
BLINK_MS = 400
HUPE_MS = 300

_NAME = re.compile(r"^([A-F])(\d\d)$")
_AN = "background:#d23b30;color:white;border-radius:8px;"
_AUS = "background:#555;color:#bbb;border-radius:8px;"


def raster() -> Dict[Tuple[int, int], int]:
    """K7633: ``(Zeile 0-5, Spalte 1-13) -> Position`` aus den Tastennamen des Kerns."""
    lage = {}
    for pos in range(0x48):
        m = _NAME.match(K1520Emulator.aup_tastenname(K7633, pos))
        if m:
            lage[("ABCDEF".index(m.group(1)), int(m.group(2)))] = pos
    return lage


def kundentasten() -> Dict[str, int]:
    """Kundentastatur: ``Name -> Position`` (Positionen 38H-47H, nur benannte)."""
    return {n: p for p in range(0x38, 0x48)
            if (n := K1520Emulator.aup_tastenname(KUNDE, p))}


#: Anordnung des Ziffernblocks (Darstellung); unbekannte Namen kommen hinten dran.
_KUNDE_ZEILEN = (("7", "8", "9"), ("4", "5", "6"), ("1", "2", "3"), ("0", "F3D", "F3F"))


class _Lampe(QLabel):
    def __init__(self, text: str, tipp: str):
        super().__init__(text)
        self.setToolTip(tipp)
        self.setMinimumWidth(56)
        self.setAlignment(Qt.AlignCenter)
        self.setMargin(3)
        self.setzen(False)

    def setzen(self, an: bool):
        self.setStyleSheet(_AN if an else _AUS)
        self.setProperty("an", bool(an))


class AupWidget(QWidget):
    """Der Kasten.  ``set_emulator`` bei jedem Neuaufbau der Maschine, ``aktualisieren`` im
    Anzeigetakt des Hauptfensters (nur bei sichtbarem Kasten)."""

    #: ``(tastatur, position, gedrueckt)`` - nach dem Aufruf des Kerns, fuer Mitleser.
    taste = Signal(int, int, bool)
    schluesselGedreht = Signal(int, bool)   # (n, an): Stellung geaendert (Kasten oder Menueaktion)

    def __init__(self, emulator, parent=None):
        super().__init__(parent)
        self.emulator = emulator
        self._gehalten = set()
        self._hupe = 0                  # zuletzt gesehener Zaehler
        self._blink_an = True
        self._blitz = False
        self._guard = False

        wurzel = QVBoxLayout(self)

        # ── Kopf: Schluessel und Lampen ──────────────────────────────────────
        kopf = QHBoxLayout()
        self.schluessel = {}
        for n in (1, 2):
            c = QCheckBox(f"SS{n}")
            c.setToolTip(f"Schlüsselschalter SS{n}: jede Drehung (ein und aus) sendet "
                         f"{'AAH' if n == 1 else 'A9H'} an die ZRE")
            c.toggled.connect(lambda an, n=n: self._schluessel(n, an))
            c.toggled.connect(lambda an, n=n: self.schluesselGedreht.emit(n, bool(an)))
            self.schluessel[n] = c
            kopf.addWidget(c)
        kopf.addStretch(1)
        self.lampen = {
            LAMPE_SEL: _Lampe("SEL", "Lampe SEL (Kommandobit 0)"),
            LAMPE_ALPHA: _Lampe("ALPHA", "Lampe ALPHA (Kommandobit 1)"),
            LAMPE_GET: _Lampe("GET", "LED der Kundentastatur: GET aktiv (Kommandobit 4)"),
            LAMPE_FEHLER: _Lampe("Fehler", "Fehlerlampe, blinkt (Kommandobit 5)"),
        }
        for w in self.lampen.values():
            kopf.addWidget(w)
        self.hupe_anzeige = _Lampe("Hupe", "Akustisches Signal (Kommandobit 6)")
        kopf.addWidget(self.hupe_anzeige)
        wurzel.addLayout(kopf)

        # ── K7633 ────────────────────────────────────────────────────────────
        box = QGroupBox("Tastatur K7633 (Positionsraster Zeile A–F, Spalte 01–13)")
        g = QGridLayout(box)
        self.tasten: Dict[Tuple[int, int], QPushButton] = {}
        for (zeile, spalte), pos in sorted(raster().items()):
            g.addWidget(self._taste(K7633, pos), zeile, spalte - 1)
        wurzel.addWidget(box)

        # ── Kundentastatur ───────────────────────────────────────────────────
        kbox = QGroupBox("Kundentastatur")
        kg = QGridLayout(kbox)
        namen = kundentasten()
        for z, zeile in enumerate(_KUNDE_ZEILEN):
            for s, name in enumerate(zeile):
                if name in namen:
                    kg.addWidget(self._taste(KUNDE, namen.pop(name)), z, s)
        for i, (name, pos) in enumerate(sorted(namen.items())):      # Rest, falls der Kern mehr kennt
            kg.addWidget(self._taste(KUNDE, pos), len(_KUNDE_ZEILEN) + i // 3, i % 3)
        zeile_unten = QHBoxLayout()
        zeile_unten.addWidget(kbox)
        zeile_unten.addStretch(1)
        wurzel.addLayout(zeile_unten)
        wurzel.addStretch(1)

        # Blinktakt der Fehlerlampe.  Der Takt laeuft nur, solange der Kasten sichtbar ist.
        self._takt = QTimer(self)
        self._takt.setInterval(BLINK_MS)
        self._takt.timeout.connect(self._blinken)

    # ── Tasten ────────────────────────────────────────────────────────────────

    def _taste(self, tastatur: int, pos: int) -> QPushButton:
        name = K1520Emulator.aup_tastenname(tastatur, pos)
        ohne = K1520Emulator.aup_code(tastatur, pos, False)
        mit = K1520Emulator.aup_code(tastatur, pos, True)
        b = QPushButton(name if ohne <= 0 else f"{name}\n{ohne:02X}")
        b.setMinimumSize(44, 40)
        b.setFocusPolicy(Qt.NoFocus)        # die Wirtstastatur bleibt beim Bildschirm
        b.setToolTip(f"{name}: Position {pos:02X}H, Code {ohne:02X}H ohne / {mit:02X}H mit Alpha"
                     if ohne > 0 else f"{name}: Position {pos:02X}H, kein Code (00H)")
        b.pressed.connect(lambda t=tastatur, p=pos: self._druck(t, p, True))
        b.released.connect(lambda t=tastatur, p=pos: self._druck(t, p, False))
        self.tasten[(tastatur, pos)] = b
        return b

    def _druck(self, tastatur: int, pos: int, gedrueckt: bool):
        if gedrueckt:
            self._gehalten.add((tastatur, pos))
        elif (tastatur, pos) not in self._gehalten:
            return                      # Loslassen ohne Druck (z. B. nach set_emulator)
        else:
            self._gehalten.discard((tastatur, pos))
        with kern_sperre():
            self.emulator.aup_taste(tastatur, pos, gedrueckt)
        self.taste.emit(tastatur, pos, gedrueckt)

    def alle_loslassen(self):
        """Alle gehaltenen Tasten loesen (Maschine wird getauscht, Kasten verschwindet)."""
        for t, p in sorted(self._gehalten):
            with kern_sperre():
                self.emulator.aup_taste(t, p, False)
        self._gehalten.clear()

    # ── Schluessel ────────────────────────────────────────────────────────────

    def _schluessel(self, n: int, an: bool):
        if self._guard:
            return
        with kern_sperre():
            self.emulator.aup_schluessel(n, bool(an))

    # ── Anzeige ───────────────────────────────────────────────────────────────

    def set_emulator(self, emulator):
        """Neue Maschine: gehaltene Tasten gehoeren der alten, Schluesselstand kommt neu."""
        self._gehalten.clear()
        self.emulator = emulator
        # Grundlinie des Hupenzaehlers: was vor dem Tausch/Zeigen gehupt wurde, ertoent nicht nachtraeglich.
        with kern_sperre():
            self._hupe = emulator.aup_hupe if emulator.aup_installed else 0
        self.aktualisieren()

    def showEvent(self, e):
        super().showEvent(e)
        self._takt.start()

    def hideEvent(self, e):
        super().hideEvent(e)
        self._takt.stop()

    def _blinken(self):
        self._blink_an = not self._blink_an
        self._lampen_zeigen()

    def aktualisieren(self):
        """Schluessel, Lampen und Hupe aus dem Kern lesen."""
        with kern_sperre():
            gesteckt = bool(self.emulator.aup_installed)
            lampen = self.emulator.aup_lampen if gesteckt else -1
            hupe = self.emulator.aup_hupe if gesteckt else 0
            stand = {n: self.emulator.aup_schluessel_stand(n) for n in (1, 2)} if gesteckt else {}
        self._guard = True
        for n, c in self.schluessel.items():
            c.setEnabled(gesteckt)
            c.setChecked(bool(stand.get(n)))
        self._guard = False
        self._lampen_bits = max(lampen, 0)
        self._lampen_zeigen()
        if hupe != self._hupe:
            neu = hupe > self._hupe
            self._hupe = hupe
            if neu:
                self._hupen()

    def _lampen_zeigen(self):
        bits = getattr(self, "_lampen_bits", 0)
        for bit, lampe in self.lampen.items():
            an = bool(bits & bit)
            if bit == LAMPE_FEHLER:
                an = an and self._blink_an      # blinkend: nur in der "an"-Halbperiode
            lampe.setzen(an)
        self.hupe_anzeige.setzen(self._blitz)

    def _hupen(self):
        self._blitz = True
        self._lampen_zeigen()
        QTimer.singleShot(HUPE_MS, self._blitz_ende)
        self._ton()

    def _blitz_ende(self):
        self._blitz = False
        self._lampen_zeigen()

    def _ton(self):
        """Kurzer Ton; ohne Tonausgabe nur die optische Anzeige (Tests ersetzen die Methode)."""
        try:
            QApplication.beep()
        except Exception:
            pass

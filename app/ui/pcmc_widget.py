"""Kasten „PCMC“ — die PC-Gegenstelle zum EPROM „mit PCMC-Erweiterung“ des MC80.30 (Issue #24).

Der Kasten steht als Reiter neben dem Kassettenlaufwerk und erscheint, wenn die Gegenstelle
eingeschaltet ist (*Maschine ▸ PCMC-Gegenstelle*, auch in der Symbolleiste).  Einschalten
heißt: die IFSS des MC80 (Reiter „Schnittstellen“) wird ein Telnet-Server auf der Schleife
und dieser Kasten hängt sich als Client daran (``app/pcmc.py``); Ausschalten stellt die
vorherige Einstellung der IFSS wieder her.  Eine schon anderweitig belegte IFSS wird nicht
angefasst.

Zwei Richtungen, Bedienung am MC80 im Programm PCMC (Menü ``E`` bzw. ``S``):

* **Zum MC80 senden** — eine .bin laden; trägt sie den Adresskopf (4 Byte), erscheinen
  Start- und Endadresse zum Ändern, sonst bleiben die Felder leer und müssen ausgefüllt werden.
* **Vom MC80 holen** — Start- und Endadresse eintragen, Datei wählen; gespeichert wird mit
  Adresskopf, damit sie später an dieselbe Stelle zurückgeht.

Eine Hex-Ansicht gibt es nicht (Festlegung des Tickets).  Kein Tastenkürzel (die Kürzeltabelle
des Handbuchs ist ein Vertrag).
"""

from __future__ import annotations

import os
import socket
import threading
from typing import Callable, Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QFileDialog, QFormLayout, QGroupBox, QHBoxLayout, QLabel, QLineEdit, QMessageBox,
    QProgressBar, QPushButton, QVBoxLayout, QWidget,
)

from app import paths, pcmc
from app.core_binding.k1520 import (
    SER_AUS, SER_LAUSCHT, SER_SERVER, SER_TELNET,
)

FILTER = "MC80-Programme (*.bin);;Alle Dateien (*)"
HOST = "127.0.0.1"


def _hex(text: str) -> Optional[int]:
    """Adresse aus einem Eingabefeld (hexadezimal, auch mit ``0x``/``H``); ``None`` wenn leer/ungültig."""
    t = text.strip().lower().removeprefix("0x").removesuffix("h").strip()
    if not t:
        return None
    try:
        v = int(t, 16)
    except ValueError:
        return None
    return v if 0 <= v <= 0xFFFF else None


def _adressfeld() -> QLineEdit:
    f = QLineEdit()
    f.setMaxLength(6)
    f.setMaximumWidth(90)
    f.setPlaceholderText("0000")
    return f


def _freier_port() -> int:
    with socket.socket() as s:
        s.bind((HOST, 0))
        return s.getsockname()[1]


class PcmcWidget(QWidget):
    #: Eine Zeile fuer die Statuszeile des Fensters.
    meldung = Signal(str)
    #: Aus dem Arbeitsfaden: (fertig, gesamt) und das Ende (Fehlertext, leer = geglueckt).
    _fortschritt = Signal(int, int)
    _ende = Signal(str, str)
    #: Die Gegenstelle wurde ein- bzw. ausgeschaltet (fuer die Aktion in Menue/Leiste).
    umgeschaltet = Signal(bool)

    def __init__(self, emulator, parent=None):
        super().__init__(parent)
        self.emulator = emulator
        self._an = False
        self._rom = False              # steckt das PCMC-EPROM?
        self._leitung: Optional[pcmc.PcmcLeitung] = None
        self._port = 0
        self._vorher = None            # IFSS-Einstellung vor dem Einschalten (None = nicht von uns)
        self._abbruch = threading.Event()
        self._faden: Optional[threading.Thread] = None
        self._daten: Optional[bytes] = None    # geladene Datei (ohne Kopf)
        self._ordner = ""
        #: Hauptfenster: liefert die gemerkte Konfiguration des IFSS-Blocks (siehe `fuer_konfig`).
        self.block_lesen: Callable[[], Optional[dict]] = lambda: None
        self._vorher_block: Optional[dict] = None

        lay = QVBoxLayout(self)
        self.status = QLabel()
        self.status.setWordWrap(True)
        lay.addWidget(self.status)

        # ── zum MC80 ────────────────────────────────────────────────────────
        g = QGroupBox("Zum MC80 senden  (am MC80: E = FILE VOM PC EMPFANGEN)")
        f = QFormLayout(g)
        z = QHBoxLayout()
        self.datei = QLineEdit()
        self.datei.setReadOnly(True)
        self.datei.setPlaceholderText("[keine Datei]")
        self.laden_knopf = QPushButton("Datei laden…")
        self.laden_knopf.setToolTip("Eine .bin-Datei laden; mit Adresskopf (4 Byte) werden "
                                    "Start- und Endadresse eingetragen")
        self.laden_knopf.clicked.connect(self.datei_waehlen)
        z.addWidget(self.datei, 1)
        z.addWidget(self.laden_knopf)
        f.addRow(z)
        self.s_start, self.s_ende = _adressfeld(), _adressfeld()
        self.s_start.setToolTip("Ladeadresse im MC80 (hexadezimal)")
        self.s_ende.setToolTip("Letzte Adresse (hexadezimal); Start + Dateilänge − 1")
        a = QHBoxLayout()
        a.addWidget(self.s_start)
        a.addWidget(QLabel("bis"))
        a.addWidget(self.s_ende)
        self.s_laenge = QLabel("")
        a.addWidget(self.s_laenge)
        a.addStretch(1)
        f.addRow("Startadresse (hex):", a)
        self.s_hinweis = QLabel("")
        self.s_hinweis.setWordWrap(True)
        f.addRow(self.s_hinweis)
        self.senden_knopf = QPushButton("Senden")
        self.senden_knopf.clicked.connect(self.senden)
        f.addRow(self.senden_knopf)
        lay.addWidget(g)
        self.s_start.textChanged.connect(self._start_geaendert)
        self.s_ende.textChanged.connect(self._pruefen)

        # ── vom MC80 ────────────────────────────────────────────────────────
        h = QGroupBox("Vom MC80 holen  (am MC80: S = FILE ZUM PC SENDEN)")
        f2 = QFormLayout(h)
        self.h_start, self.h_ende = _adressfeld(), _adressfeld()
        b = QHBoxLayout()
        b.addWidget(self.h_start)
        b.addWidget(QLabel("bis"))
        b.addWidget(self.h_ende)
        b.addStretch(1)
        f2.addRow("Startadresse (hex):", b)
        self.holen_knopf = QPushButton("Empfangen…")
        self.holen_knopf.setToolTip("Zieldatei wählen und den Bereich aus dem MC80 holen; "
                                    "die Datei bekommt den Adresskopf (4 Byte)")
        self.holen_knopf.clicked.connect(self.holen)
        f2.addRow(self.holen_knopf)
        lay.addWidget(h)
        self.h_start.textChanged.connect(self._pruefen)
        self.h_ende.textChanged.connect(self._pruefen)

        # ── Fortschritt ────────────────────────────────────────────────────
        self.balken = QProgressBar()
        self.balken.setRange(0, 100)
        self.balken.setVisible(False)
        self.abbrechen_knopf = QPushButton("Abbrechen")
        self.abbrechen_knopf.setVisible(False)
        self.abbrechen_knopf.clicked.connect(self._abbruch.set)
        lay.addWidget(self.balken)
        lay.addWidget(self.abbrechen_knopf)
        lay.addStretch(1)

        self._fortschritt.connect(self._zeige_fortschritt, Qt.QueuedConnection)
        self._ende.connect(self._uebertragung_ende, Qt.QueuedConnection)
        self.aktualisieren()

    # ── Maschine ─────────────────────────────────────────────────────────────

    @property
    def an(self) -> bool:
        return self._an

    def set_emulator(self, emulator) -> None:
        """Neue Maschine: die Leitung zur alten ist weg, ihre IFSS-Einstellung auch."""
        self._leitung_schliessen()
        self.emulator = emulator
        self._vorher = None
        self._vorher_block = None
        self._an = False
        self.aktualisieren()

    def set_rom(self, vorhanden: bool) -> None:
        """Steckt das EPROM „mit PCMC-Erweiterung“ (Einstellungen ▸ Allgemein)?"""
        self._rom = bool(vorhanden)
        self.aktualisieren()

    def _ifss(self) -> Optional[int]:
        """Index der IFSS im Hub der Maschine (``None``: die Maschine hat keine)."""
        emu = self.emulator
        if emu is None:
            return None
        for i in range(emu.serial_count()):
            info = emu.serial_info(i)
            if info and info.name == "IFSS":
                return i
        return None

    # ── Ein- und Ausschalten ─────────────────────────────────────────────────

    def einschalten(self) -> bool:
        """IFSS als Telnet-Server auf der Schleife öffnen und anhängen; ``False`` mit Meldung
        (Statuszeile + Kasten), wenn das nicht geht."""
        if self._an:
            return True
        i = self._ifss()
        if i is None:
            return self._fehler("Diese Maschine hat keine IFSS — PCMC gibt es nur am MC80.30.")
        emu = self.emulator
        st = emu.serial_status(i)
        k = emu.serial_config(i)
        if st is None or k is None:
            return self._fehler("Die IFSS ist nicht abfragbar.")
        if st.zustand == SER_AUS:
            vorher_block = self.block_lesen()
            port = _freier_port()
            if not emu.serial_configure(i, betriebsart=SER_TELNET, rolle=SER_SERVER, host=HOST,
                                        port=port, loop=False, xonxoff=False, rtscts_bruecke=False):
                return self._fehler("Die IFSS lässt sich nicht für PCMC einstellen.")
            if not emu.serial_start(i):
                msg = (emu.serial_status(i) or st).meldung
                self._vorher_zurueck(i, k)
                return self._fehler(f"Die IFSS lässt sich nicht öffnen: {msg}")
            self._vorher, self._vorher_block = k, vorher_block
            self._port = port
        elif st.zustand == SER_LAUSCHT and k.betriebsart == SER_TELNET and k.rolle == SER_SERVER:
            # Ein vom Anwender (oder einem frueheren Lauf) geoeffneter Telnet-Server: mitbenutzen.
            self._vorher, self._vorher_block = None, None
            self._port = st.port_aktiv
        else:
            return self._fehler("Die IFSS ist belegt (Reiter „Schnittstellen“) — erst beenden.")
        try:
            self._leitung = pcmc.PcmcLeitung(HOST, self._port, self._abbruch.is_set)
        except (OSError, pcmc.PcmcFehler) as e:
            self._ausschalten_ifss()
            return self._fehler(f"Keine Verbindung zur IFSS: {e}")
        self._an = True
        self.aktualisieren()
        self.umgeschaltet.emit(True)
        return True

    def ausschalten(self) -> None:
        """Leitung zu, IFSS wie vorher."""
        if not self._an and self._leitung is None:
            return
        self._abbruch.set()
        if self._faden is not None:
            self._faden.join(2)
        self._leitung_schliessen()
        self._ausschalten_ifss()
        self._an = False
        self.aktualisieren()
        self.umgeschaltet.emit(False)

    def _ausschalten_ifss(self) -> None:
        i = self._ifss()
        if i is not None and self._vorher is not None and self.emulator is not None:
            self.emulator.serial_stop(i)
            self._vorher_zurueck(i, self._vorher)
        self._vorher = None
        self._vorher_block = None

    def _vorher_zurueck(self, i: int, k) -> None:
        self.emulator.serial_configure(
            i, betriebsart=k.betriebsart, rolle=k.rolle, host=k.host, port=k.port, loop=k.loop,
            rtscts_bruecke=k.rtscts_bruecke, xonxoff=k.xonxoff, taktquelle=k.taktquelle,
            datei=k.datei)

    def _leitung_schliessen(self) -> None:
        if self._leitung is not None:
            self._leitung.schliessen()
            self._leitung = None

    def fuer_konfig(self, schnittstellen: dict) -> dict:
        """Den IFSS-Block der Konfiguration auf den Stand VOR dem Einschalten zurücksetzen:
        sonst käme die Gegenstelle beim nächsten Start als offener Telnet-Server wieder."""
        if self._an and self._vorher_block is not None:
            i = self._ifss()
            info = self.emulator.serial_info(i) if i is not None else None
            if info and info.name in schnittstellen:
                schnittstellen = dict(schnittstellen)
                schnittstellen[info.name] = self._vorher_block
        return schnittstellen

    def _fehler(self, text: str) -> bool:
        self.status.setText(text)
        self.meldung.emit("PCMC: " + text)
        return False

    # ── Anzeige ──────────────────────────────────────────────────────────────

    def aktualisieren(self) -> None:
        if not self._an:
            self.status.setText("Gegenstelle aus (Maschine ▸ PCMC-Gegenstelle).")
        elif not self._rom:
            self.status.setText(
                f"Verbunden mit der IFSS (Port {self._port}) — aber im MC80 steckt kein "
                "PCMC-EPROM (Einstellungen ▸ Allgemein ▸ PCMC-Erweiterung).")
        else:
            self.status.setText(f"Verbunden mit der IFSS (Port {self._port}), 4800 Bd 8N1.  "
                                "Am MC80: Kommando PCMC, dann E oder S.")
        laeuft = self._faden is not None and self._faden.is_alive()
        frei = self._an and not laeuft
        self.laden_knopf.setEnabled(not laeuft)
        self.holen_knopf.setEnabled(frei and self._bereich(self.h_start, self.h_ende) is not None)
        self.senden_knopf.setEnabled(frei and self._sendebereich() is not None)
        self.balken.setVisible(laeuft)
        self.abbrechen_knopf.setVisible(laeuft)

    # ── Eingaben ─────────────────────────────────────────────────────────────

    @staticmethod
    def _bereich(a: QLineEdit, e: QLineEdit):
        s, z = _hex(a.text()), _hex(e.text())
        return (s, z) if s is not None and z is not None and s <= z else None

    def _sendebereich(self):
        """(start, ende) der zu sendenden Daten, wenn die Eingaben zur Datei passen."""
        if not self._daten:
            return None
        b = self._bereich(self.s_start, self.s_ende)
        if b is None or b[1] - b[0] + 1 > len(self._daten):
            return None
        return b

    def _start_geaendert(self) -> None:
        """Eine neue Startadresse verschiebt die Endadresse mit (Dateilänge bleibt)."""
        s = _hex(self.s_start.text())
        if self._daten and s is not None and not self.s_ende.hasFocus():
            e = s + len(self._daten) - 1
            self.s_ende.blockSignals(True)
            self.s_ende.setText(f"{e:04X}" if e <= 0xFFFF else "")
            self.s_ende.blockSignals(False)
        self._pruefen()

    def _pruefen(self) -> None:
        hinweis = ""
        if self._daten:
            b = self._bereich(self.s_start, self.s_ende)
            if b is not None:
                n = b[1] - b[0] + 1
                self.s_laenge.setText(f"{n} Byte")
                if n > len(self._daten):
                    hinweis = f"Der Bereich ist größer als die Datei ({len(self._daten)} Byte)."
                elif n < len(self._daten):
                    hinweis = f"Es werden nur die ersten {n} von {len(self._daten)} Byte gesendet."
            else:
                self.s_laenge.setText("")
                if self.s_start.text() and self.s_ende.text():
                    hinweis = "Adressen ungültig (hexadezimal, Start ≤ Ende)."
                else:
                    hinweis = "Start- und Endadresse eintragen — die Datei hat keinen Adresskopf."
        self.s_hinweis.setText(hinweis)
        self.aktualisieren()

    # ── Datei laden ──────────────────────────────────────────────────────────

    def datei_laden(self, pfad: str) -> bool:
        """*pfad* laden; trägt sie einen Adresskopf, kommen Start/Ende in die Felder."""
        try:
            with open(pfad, "rb") as f:
                roh = f.read()
        except OSError as e:
            QMessageBox.warning(self, "PCMC", f"Die Datei lässt sich nicht lesen:\n{e}")
            return False
        if not roh:
            QMessageBox.warning(self, "PCMC", "Die Datei ist leer.")
            return False
        self._ordner = os.path.dirname(pfad)
        kopf = pcmc.kopf_lesen(roh)
        self._daten = roh[pcmc.KOPF:] if kopf else roh
        self.datei.setText(pfad)
        for feld in (self.s_start, self.s_ende):
            feld.blockSignals(True)
        if kopf:
            self.s_start.setText(f"{kopf[0]:04X}")
            self.s_ende.setText(f"{kopf[1]:04X}")
        else:
            self.s_start.clear()
            self.s_ende.clear()
        for feld in (self.s_start, self.s_ende):
            feld.blockSignals(False)
        self._pruefen()
        return True

    def datei_waehlen(self) -> None:
        pfad, _ = QFileDialog.getOpenFileName(self, "Datei für den MC80", self._start_ordner(),
                                              FILTER)
        if pfad:
            self.datei_laden(pfad)

    def _start_ordner(self) -> str:
        return self._ordner or str(paths.user_disks_dir())

    # ── Uebertragen ───────────────────────────────────────────────────────────

    def senden(self) -> None:
        b = self._sendebereich()
        if b is None or not self._an:
            return
        daten = self._daten[: b[1] - b[0] + 1]
        self._starte(lambda l, fp: pcmc.senden(l.lesen, l.schreiben, b[0], daten, fortschritt=fp),
                     "senden", f"{len(daten)} Byte nach {b[0]:04X}H gesendet.")

    def holen(self, pfad: Optional[str] = None) -> None:
        b = self._bereich(self.h_start, self.h_ende)
        if b is None or not self._an:
            return
        if not pfad:
            pfad, _ = QFileDialog.getSaveFileName(
                self, "Bereich aus dem MC80 speichern als", self._start_ordner(), FILTER)
            if not pfad:
                return
            if "." not in pfad.rsplit("/", 1)[-1]:
                pfad += ".bin"

        def lauf(l, fp):
            daten = pcmc.holen(l.lesen, l.schreiben, b[0], b[1], fortschritt=fp)
            with open(pfad, "wb") as f:
                f.write(pcmc.mit_kopf(b[0], daten))

        self._starte(lauf, "holen", f"{b[1] - b[0] + 1} Byte von {b[0]:04X}H gespeichert ({pfad}).")

    def _starte(self, arbeit, art: str, erfolg: str) -> None:
        if self._faden is not None and self._faden.is_alive():
            return
        if self._leitung is None:
            try:
                self._leitung = pcmc.PcmcLeitung(HOST, self._port, self._abbruch.is_set)
            except (OSError, pcmc.PcmcFehler) as e:
                self._fehler(f"Keine Verbindung zur IFSS: {e}")
                return
        self._abbruch.clear()
        leitung = self._leitung

        def faden():
            fehler = ""
            try:
                leitung.leeren()      # Reste (Telnet-Verhandlung, alte Bytes) weg
                arbeit(leitung, lambda n, g: self._fortschritt.emit(n, g))
            except (pcmc.PcmcFehler, OSError) as e:
                fehler = str(e)
            self._ende.emit(fehler, erfolg)

        self.balken.setValue(0)
        self._faden = threading.Thread(target=faden, name="pcmc", daemon=True)
        self._faden.start()
        self.aktualisieren()

    def _zeige_fortschritt(self, n: int, gesamt: int) -> None:
        self.balken.setValue(int(100 * n / max(1, gesamt)))

    def _uebertragung_ende(self, fehler: str, erfolg: str) -> None:
        if self._faden is not None:
            self._faden.join(1)
        self._faden = None
        if fehler:
            if "geschlossen" in fehler or "unterbrochen" in fehler:
                self._leitung_schliessen()      # beim naechsten Mal neu verbinden
            self.status.setText(fehler)
            self.meldung.emit("PCMC: " + fehler)
            if fehler != "Abgebrochen":
                QMessageBox.warning(self, "PCMC", fehler)
        else:
            self.meldung.emit("PCMC: " + erfolg)
        self.aktualisieren()

    def warte_auf_ende(self, frist: float = 60.0) -> None:
        """Für Tests: den Arbeitsfaden abwarten (die Ende-Meldung kommt über die Ereignisschleife)."""
        if self._faden is not None:
            self._faden.join(frist)

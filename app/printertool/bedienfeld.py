"""Bedienfeld des Druckers - aus einer :class:`~app.printertool.typen.Typbeschreibung` gebaut.

Tasten, Schalter und Lampen stehen als Daten in ``typen.py``; dieses Widget weiss nichts vom
SD 1157.  Es meldet Bedienung ueber Signale (``taste``, ``schalter``, ``netz``) und zieht seinen
Anzeigezustand im Takt aus dem Kern nach (``aktualisieren``): **der Kern ist die Quelle der
Wahrheit** (wie `app/ui/serial_widget.py`).

Der Netzschalter gehoert der Oberflaeche: "Aus" haelt die Maschine an (alle Lampen dunkel, das
Blatt bleibt), "Ein" ist ein Neustart (/RESET) - Entwurf 49 §4.5.
"""

from __future__ import annotations

from typing import Dict, Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QComboBox, QFrame, QGridLayout, QGroupBox, QHBoxLayout, QLabel, QPushButton, QVBoxLayout, QWidget,
)

from app.printertool.typen import Typbeschreibung
from app.ui.status_bar import PanelLamp


class Bedienfeld(QWidget):
    taste = Signal(int, bool)          # (kern_id, gedrueckt) - Taster
    schalter = Signal(int, int)        # (kern_id, an/Stellung) - rastende Schalter, Drehschalter
    netz = Signal(bool)                # Netzschalter
    neustart = Signal()                # Knopf "Neustart jetzt" im Hinweis

    def __init__(self, beschreibung: Typbeschreibung, parent=None):
        super().__init__(parent)
        self.beschreibung = beschreibung
        self._tasten: Dict[str, QPushButton] = {}
        self._lampen: Dict[str, PanelLamp] = {}
        self._netz_an = True
        self._laden = False                      # True, solange WIR Schalter nachziehen
        self._baue()

    # ── Aufbau ───────────────────────────────────────────────────────────────

    def _baue(self) -> None:
        b = self.beschreibung
        lay = QVBoxLayout(self)
        lay.setContentsMargins(6, 6, 6, 6)

        # Netzschalter + Lampen
        kopf = QGroupBox("Netz und Anzeigen")
        kl = QHBoxLayout(kopf)
        self.netzschalter = QPushButton("Netz")
        self.netzschalter.setCheckable(True)
        self.netzschalter.setChecked(True)
        self.netzschalter.setToolTip("Netzschalter.  Aus: der Drucker steht, das Blatt bleibt.  Ein: Neustart "
                                     "(/RESET) - die Firmware liest Tasten und Schalter dabei neu.")
        self.netzschalter.toggled.connect(self._netz_umgeschaltet)
        kl.addWidget(self.netzschalter)
        for l in b.lampen:
            lampe = PanelLamp(l.name, l.farbe, l.tipp)
            self._lampen[l.name] = lampe
            sp = QVBoxLayout()
            sp.setSpacing(1)
            sp.addWidget(lampe, 0, Qt.AlignHCenter)
            sp.addWidget(QLabel(l.name), 0, Qt.AlignHCenter)
            kl.addLayout(sp)
        kl.addStretch()
        lay.addWidget(kopf)

        # Tasten: Taster und rastende Schalter
        tg = QGroupBox("Tasten und Schalter")
        gl = QGridLayout(tg)
        for i, t in enumerate(b.tasten):
            knopf = QPushButton(t.text)
            knopf.setToolTip(t.tipp)
            knopf.setFocusPolicy(Qt.NoFocus)
            knopf.setMinimumHeight(30)
            if t.art == "rastend":
                knopf.setCheckable(True)
                knopf.toggled.connect(lambda an, k=t.kern_id: None if self._laden else self.schalter.emit(k, an))
            else:
                knopf.pressed.connect(lambda k=t.kern_id: self.taste.emit(k, True))
                knopf.released.connect(lambda k=t.kern_id: self.taste.emit(k, False))
            self._tasten[t.name] = knopf
            gl.addWidget(knopf, i // 2, i % 2)
        lay.addWidget(tg)

        # Drehschalter Betriebsart und DIL-Schalter (K6316)
        self.betriebsart: Optional[QComboBox] = None
        if b.betriebsarten:
            bg = QGroupBox("Betriebsart")
            bl = QHBoxLayout(bg)
            self.betriebsart = QComboBox()
            for wert, text in b.betriebsarten:
                self.betriebsart.addItem(text, wert)
            self.betriebsart.setToolTip("Drehschalter im Drucker.  Die Firmware liest ihn beim Netz-Ein (und bei der "
                                        "Geraetekennung): Aenderung wirkt nach dem Neustart.  Nur in der Stellung "
                                        "Schneiden schneidet der Drucker (S5+S4); Aufwickeln sperrt Papier rueckwaerts.")
            self.betriebsart.currentIndexChanged.connect(
                lambda _i: None if self._laden else self.schalter.emit(b.betriebsart_id, int(self.betriebsart.currentData())))
            bl.addWidget(self.betriebsart)
            lay.addWidget(bg)
        if b.dil:
            dg = QGroupBox(b.dil_titel)
            dl = QGridLayout(dg)
            for i, t in enumerate(b.dil):
                knopf = QPushButton(t.text)
                knopf.setCheckable(True)
                knopf.setToolTip(t.tipp)
                knopf.setFocusPolicy(Qt.NoFocus)
                knopf.toggled.connect(lambda an, k=t.kern_id: None if self._laden else self.schalter.emit(k, an))
                self._tasten[t.name] = knopf
                dl.addWidget(knopf, i // 4, i % 4)
            lay.addWidget(dg)

        # Hinweise (Gastverhalten, nicht wegerklaert)
        self.hinweise = QLabel()
        self.hinweise.setWordWrap(True)
        self.hinweise.setTextFormat(Qt.RichText)
        text = [f"<b>{t.text}:</b> {t.hinweis}" for t in b.tasten if t.hinweis]
        if b.stand_hinweis:
            text.append(b.stand_hinweis)
        self.hinweise.setText("<small>" + "<br>".join(text) + "</small>")
        lay.addWidget(self.hinweise)

        # Warnzeile mit Knopf, wenn ONLINE und Lampe auseinanderlaufen
        self.warnung = QFrame()
        self.warnung.setFrameShape(QFrame.StyledPanel)
        wl = QHBoxLayout(self.warnung)
        wl.setContentsMargins(6, 3, 6, 3)
        self.warntext = QLabel()
        self.warntext.setWordWrap(True)
        wl.addWidget(self.warntext, 1)
        self.neustart_knopf = QPushButton("Neustart jetzt")
        self.neustart_knopf.setFocusPolicy(Qt.NoFocus)
        self.neustart_knopf.clicked.connect(self.neustart)
        wl.addWidget(self.neustart_knopf)
        self.warnung.setVisible(False)
        lay.addWidget(self.warnung)
        lay.addStretch()

    # ── Zugriff fuer Tests und Fenster ───────────────────────────────────────

    def knopf(self, name: str) -> QPushButton:
        return self._tasten[name]

    def lampe(self, name: str) -> PanelLamp:
        return self._lampen[name]

    def netz_an(self) -> bool:
        return self._netz_an

    # ── Netz ─────────────────────────────────────────────────────────────────

    def set_netz(self, an: bool) -> None:
        """Netzschalter von aussen setzen (ohne Signal)."""
        self._netz_an = bool(an)
        self.netzschalter.blockSignals(True)
        self.netzschalter.setChecked(self._netz_an)
        self.netzschalter.blockSignals(False)
        for k in self._tasten.values():
            k.setEnabled(self._netz_an)
        if not self._netz_an:
            for lampe in self._lampen.values():
                lampe.set_an(False)
            self.warnung.setVisible(False)

    def _netz_umgeschaltet(self, an: bool) -> None:
        self._netz_an = bool(an)
        for k in self._tasten.values():
            k.setEnabled(self._netz_an)
        self.netz.emit(self._netz_an)

    # ── Takt: Anzeige aus dem Kern ───────────────────────────────────────────

    def aktualisieren(self, emu) -> None:
        if emu is None:
            return
        b = self.beschreibung
        self._laden = True
        try:
            for t in b.tasten:
                if t.art == "rastend":
                    w = emu.printer_switch_get(t.kern_id)
                    k = self._tasten[t.name]
                    if w >= 0 and k.isChecked() != bool(w):
                        k.blockSignals(True)
                        k.setChecked(bool(w))
                        k.blockSignals(False)
            for t in b.dil:
                w = emu.printer_switch_get(t.kern_id)
                k = self._tasten[t.name]
                if w >= 0 and k.isChecked() != bool(w):
                    k.blockSignals(True)
                    k.setChecked(bool(w))
                    k.blockSignals(False)
            if self.betriebsart is not None:
                w = emu.printer_switch_get(b.betriebsart_id)
                i = self.betriebsart.findData(w)
                if i >= 0 and self.betriebsart.currentIndex() != i:
                    self.betriebsart.blockSignals(True)
                    self.betriebsart.setCurrentIndex(i)
                    self.betriebsart.blockSignals(False)
        finally:
            self._laden = False
        if self._netz_an:
            for l in b.lampen:
                an = True if l.kern_id is None else emu.printer_lamp(l.kern_id) == 1
                self._lampen[l.name].set_an(an)
        self._warnung_aktualisieren(emu)

    def _warnung_aktualisieren(self, emu) -> None:
        """ONLINE-Schalter und ONLINE-Lampe laufen auseinander: die Firmware liest den Schalter nur beim Anlauf."""
        online = next((t for t in self.beschreibung.tasten if t.name == "online"), None)
        lampe = next((l for l in self.beschreibung.lampen if l.name == "ONLINE"), None)
        if online is None or lampe is None or not self._netz_an:
            self.warnung.setVisible(False)
            return
        schalter = emu.printer_switch_get(online.kern_id)
        lamp = emu.printer_lamp(lampe.kern_id)
        if schalter == 0 and lamp == 1:
            self.warntext.setText("<b>ONLINE ist aus, die Lampe leuchtet noch.</b> Die Firmware liest ONLINE nur "
                                  "beim Einschalten: TEST wirkt erst nach einem Neustart.")
            self.warnung.setVisible(True)
        elif schalter == 1 and lamp == 0:
            self.warntext.setText("<b>ONLINE ist an, die Lampe ist aus.</b> Der Drucker wurde offline eingeschaltet; "
                                  "die Lampe folgt dem Schalter erst nach einem Neustart.")
            self.warnung.setVisible(True)
        else:
            self.warnung.setVisible(False)

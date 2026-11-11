"""
K1520 Emulator - Zeilen „IDE-Platte (GIDE)“ und „GIDE-Uhr“ unter *Einstellungen ▸ Allgemein*
=============================================================================================

Zeilen im Reiter *Allgemein* (flach, ohne Rahmen, untereinander wie die uebrigen Einstellungen;
doc/design/50_gun_gide.md, AP G6):

* :class:`GideGruppe` -- Kästchen „GIDE-Karte stecken", E/A-Basis (Auswahlfeld mit Belegt-Markierung),
  Fehlertext des Kerns.
* :class:`GideRtcGruppe` -- die Uhr RTC 72421 im Stil der P8000-Gruppe *RTC*: Kästchen, Zeitversatz
  (0 / −28 / −40 Jahre), Uhrzeit des Gastsystems, Wirtszeit (PC-Uhr), „Uhr nach Wirtszeit stellen".

Das PLATTENABBILD (Wählen / Neue Platte / Auswerfen) steht NICHT hier, sondern als Kasten im Reiter
*Laufwerke* (:mod:`app.ui.gide_platte_widget`), wie die Winchesterplatte des P8000.

Beide Gruppen sammeln nur Eingaben und melden sie als TEIL-Zustand im Sinne von :mod:`app.gide`;
das Hauptfenster setzt ihn mit dem Rest zusammen, steckt die Karte (Neuaufbau der Maschine wie bei
RAF/Kassette) und zeigt Kernfehler an.  Die Gruppen kennen den Kern nicht.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QFormLayout, QHBoxLayout, QLabel, QPushButton, QWidget,
)

from app import gide


class GideGruppe(QWidget):
    """Zeile „IDE-Karte (GIDE)“: Kästchen und Basis (+ Fehlertext) -- flach, ohne Rahmen."""

    #: Teil-Zustand ``{enabled, basis}`` nach einer Eingabe des Anwenders.
    gideChanged = Signal(dict)

    def __init__(self, basis_vorgabe: int, parent=None):
        super().__init__(parent)
        self._guard = False
        self._basis_vorgabe = int(basis_vorgabe)
        self._gesteckt = False
        form = QFormLayout(self)
        form.setContentsMargins(0, 0, 0, 0)

        self.karte_box = QCheckBox("GIDE-Karte stecken", self)
        self.karte_box.setToolTip(gide.TIPP_KARTE)
        self.basis_combo = QComboBox(self)
        for b in gide.BASEN:
            self.basis_combo.addItem(gide.basis_text(b), b)
        self.basis_combo.setToolTip(gide.TIPP_BASIS)
        zeile = QHBoxLayout()
        zeile.addWidget(self.karte_box)
        zeile.addWidget(QLabel("E/A-Basis:", self))
        zeile.addWidget(self.basis_combo, 1)
        form.addRow("IDE-Platte (GIDE):", zeile)

        self.fehler_label = QLabel("", self)
        self.fehler_label.setStyleSheet("color: #c00;")
        self.fehler_label.setWordWrap(True)
        self.fehler_label.setVisible(False)
        form.addRow(self.fehler_label)

        self.karte_box.toggled.connect(self._aenderung)
        self.basis_combo.currentIndexChanged.connect(self._aenderung)
        self.set_zustand(gide.vorgabe(self._basis_vorgabe))

    # ── Zustand ──────────────────────────────────────────────────────────────

    def zustand(self) -> dict:
        """Teil-Zustand (``enabled``, ``basis``) im Sinne von :mod:`app.gide`."""
        return {"enabled": self.karte_box.isChecked(), "basis": int(self.basis_combo.currentData())}

    def set_zustand(self, z: dict, fehler: str = "", gesteckt: Optional[bool] = None) -> None:
        """Eingaben setzen (ohne Signal).  *gesteckt*: die Karte steckt WIRKLICH (sonst bleibt der Stand)."""
        self._guard = True
        self.karte_box.setChecked(bool(z.get("enabled")))
        idx = self.basis_combo.findData(int(z.get("basis", self._basis_vorgabe)))
        self.basis_combo.setCurrentIndex(idx if idx >= 0 else self.basis_combo.findData(self._basis_vorgabe))
        self._guard = False
        if gesteckt is not None:
            self._gesteckt = bool(gesteckt)
        self.set_fehler(fehler)

    def set_basen(self, zeilen) -> None:
        """Auswahlfeld der Basis beschriften (:func:`app.kartenwahl.eintraege`): belegte Eintraege
        tragen ihren Besitzer im Text und sind nicht waehlbar.  Die Auswahl bleibt, wie sie ist."""
        modell = self.basis_combo.model()
        for z in zeilen:
            i = self.basis_combo.findData(z.basis)
            if i < 0:
                continue
            self.basis_combo.setItemText(i, z.text)
            item = modell.item(i)
            if item is not None:
                item.setEnabled(not z.belegt)

    def set_fehler(self, text: str) -> None:
        """Fehlertext des Kerns unter der Gruppe (leer = ausblenden)."""
        self.fehler_label.setText(text or "")
        self.fehler_label.setVisible(bool(text))

    def gesteckt(self) -> bool:
        return self._gesteckt

    def _aenderung(self, *_args) -> None:
        if not self._guard:
            self.gideChanged.emit(self.zustand())


class GideRtcGruppe(QWidget):
    """Die Uhr der GIDE im Stil der P8000-Zeilen *RTC*: Haken, Zeitversatz, Uhrzeit, Stellen."""

    #: Teil-Zustand ``{rtc, rtc_versatz}`` nach einer Eingabe des Anwenders.
    rtcChanged = Signal(dict)
    #: „Uhr nach Wirtszeit stellen" gedrueckt.
    rtcStellenGewuenscht = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._guard = False
        form = QFormLayout(self)
        form.setContentsMargins(0, 0, 0, 0)

        self.rtc_box = QCheckBox("RTC 72421", self)
        self.rtc_box.setToolTip(gide.TIPP_RTC)
        self.versatz_combo = QComboBox(self)
        for wert, text in gide.VERSAETZE:
            self.versatz_combo.addItem(text, wert)
        self.versatz_combo.setToolTip(gide.TIPP_VERSATZ)
        zeile = QHBoxLayout()
        zeile.addWidget(self.rtc_box)
        zeile.addWidget(self.versatz_combo, 1)
        form.addRow("GIDE-Uhr:", zeile)

        # Zeitquelle ist immer die PC-Uhr (UTC); angezeigt werden Gast- und Wirtszeit in EINER Zeile,
        # daneben der Knopf zum Stellen.
        self.rtc_zeit_label = QLabel("—", self)
        self.rtc_zeit_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self.wirt_label = QLabel("—", self)
        self.wirt_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self.stellen_knopf = QPushButton("Nach PC-Uhr stellen", self)
        self.stellen_knopf.setToolTip(
            "Stellt die Uhr der Karte auf die jetzige Zeit des PCs (UTC; der gewählte Zeitversatz wirkt wie "
            "beim Lesen).  Liegt die neue Zeit vor der bisherigen, wird vorher gefragt.  Von selbst stellt "
            "sich die Uhr nur beim allerersten Start; danach läuft sie auf ihrer Batterie weiter.")
        self.stellen_knopf.clicked.connect(lambda _c=False: self.rtcStellenGewuenscht.emit())
        z2 = QHBoxLayout()
        z2.addWidget(QLabel("Gast:", self))
        z2.addWidget(self.rtc_zeit_label)
        z2.addWidget(QLabel("PC:", self))
        z2.addWidget(self.wirt_label)
        z2.addStretch(1)
        z2.addWidget(self.stellen_knopf)
        form.addRow("Uhrzeit (UTC):", z2)

        self.rtc_box.toggled.connect(self._aenderung)
        self.versatz_combo.currentIndexChanged.connect(self._aenderung)
        self.set_zustand(gide.vorgabe(0))

    def zustand(self) -> dict:
        return {"rtc": self.rtc_box.isChecked(), "rtc_versatz": str(self.versatz_combo.currentData())}

    def set_zustand(self, z: dict, aktiv: Optional[bool] = None) -> None:
        """Eingaben setzen (ohne Signal).  *aktiv*: Karte gewuenscht -- ohne sie ist die Gruppe grau."""
        self._guard = True
        self.rtc_box.setChecked(z.get("rtc") is not False)
        i = self.versatz_combo.findData(str(z.get("rtc_versatz", gide.VERSATZ_VORGABE)))
        self.versatz_combo.setCurrentIndex(i if i >= 0 else self.versatz_combo.findData(gide.VERSATZ_VORGABE))
        self._guard = False
        if aktiv is None:
            aktiv = bool(z.get("enabled"))
        self.set_aktiv(aktiv)

    def set_aktiv(self, an: bool) -> None:
        """Ohne gewuenschte Karte gibt es nichts zu stellen."""
        self.rtc_box.setEnabled(an)
        self.versatz_combo.setEnabled(an and self.rtc_box.isChecked())
        self.stellen_knopf.setEnabled(an and self.rtc_box.isChecked())

    def set_anzeige(self, gast: str, wirt: str = "") -> None:
        self.rtc_zeit_label.setText(gast or "—")
        self.wirt_label.setText(wirt or "—")

    def _aenderung(self, *_args) -> None:
        self.versatz_combo.setEnabled(self.rtc_box.isEnabled() and self.rtc_box.isChecked())
        self.stellen_knopf.setEnabled(self.rtc_box.isEnabled() and self.rtc_box.isChecked())
        if not self._guard:
            self.rtcChanged.emit(self.zustand())

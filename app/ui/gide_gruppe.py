"""
K1520 Emulator - Gruppe „IDE-Platte (GIDE)" unter *Einstellungen ▸ Allgemein*
=============================================================================

Eine Gruppe der Reihe „Erweiterungskarten" (doc/design/50_gun_gide.md, AP G6).  Sie
sammelt NUR die Eingaben und meldet sie als Zustand (:mod:`app.gide`); das Hauptfenster
steckt die Karte (Neuaufbau der Maschine wie bei RAF/Kassette), legt Abbilder ein und
zeigt Kernfehler an.  Die Gruppe selbst kennt den Kern nicht.

Plattenabbild-Bedienung (Wählen / Neue Platte / Auswerfen) ist nur bei GESTECKTER Karte
frei (Muster Kassette): Einlegen und Auswerfen wirken sofort am Kern.  Dialoge laufen über
die kleinen Methoden ``_datei_waehlen``/``_neue_platte_abfragen``/``_frage`` — Tests ersetzen
sie, ein ``exec()`` stuende headless bis zum Zeitueberlauf.
"""

from __future__ import annotations

import os
from typing import Optional, Tuple

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QDialog, QDialogButtonBox, QFileDialog, QFormLayout, QGroupBox,
    QHBoxLayout, QLabel, QLineEdit, QMessageBox, QPushButton, QVBoxLayout, QWidget,
)

from app import gide


class NeuePlatteDialog(QDialog):
    """Name und Groesse einer neuen Platte (Abbild als Rohdatei, sparsam angelegt)."""

    def __init__(self, ordner: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Neue Platte")
        lay = QVBoxLayout(self)
        form = QFormLayout()
        self.name_edit = QLineEdit(os.path.join(ordner, "gide.img"), self)
        form.addRow("Datei:", self.name_edit)
        self.groesse_combo = QComboBox(self)
        for mb in gide.GROESSEN_MB:
            self.groesse_combo.addItem(f"{mb} MB", mb)
        self.groesse_combo.setCurrentIndex(self.groesse_combo.findData(gide.GROESSE_VORGABE_MB))
        form.addRow("Größe:", self.groesse_combo)
        self.gz_box = QCheckBox("gepackt (.img.gz)", self)
        self.gz_box.setToolTip("Hängt .gz an; die Datei wird erst beim Auswerfen oder Beenden geschrieben.")
        form.addRow("", self.gz_box)
        lay.addLayout(form)
        hinweis = QLabel(gide.HINWEIS_GROESSE, self)
        hinweis.setWordWrap(True)
        lay.addWidget(hinweis)
        tasten = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel, self)
        tasten.accepted.connect(self.accept)
        tasten.rejected.connect(self.reject)
        lay.addWidget(tasten)

    def ergebnis(self) -> Tuple[str, int]:
        pfad = self.name_edit.text().strip()
        if pfad and self.gz_box.isChecked() and not pfad.lower().endswith(".gz"):
            pfad += ".gz"
        return pfad, int(self.groesse_combo.currentData())


class GideGruppe(QGroupBox):
    """Kästchen, Basis, Plattenabbild und RTC der GIDE."""

    #: Zustand (:func:`app.gide.normalize`) nach einer Eingabe des Anwenders.
    gideChanged = Signal(dict)
    #: „Neue Platte" bestaetigt: (Pfad, MB); das Hauptfenster legt an und legt ein.
    neuePlatteGewuenscht = Signal(str, int)

    def __init__(self, basis_vorgabe: int, parent=None):
        super().__init__("IDE-Platte (GIDE)", parent)
        self._guard = False
        self._basis_vorgabe = int(basis_vorgabe)
        self._gesteckt = False
        form = QFormLayout(self)

        self.karte_box = QCheckBox("GIDE-Karte stecken", self)
        self.karte_box.setToolTip(gide.TIPP_KARTE)
        form.addRow(self.karte_box)

        self.basis_combo = QComboBox(self)
        for b in gide.BASEN:
            self.basis_combo.addItem(gide.basis_text(b), b)
        self.basis_combo.setToolTip(gide.TIPP_BASIS)
        form.addRow("E/A-Basis:", self.basis_combo)

        self.abbild_edit = QLineEdit(self)
        self.abbild_edit.setPlaceholderText("kein Plattenabbild")
        self.abbild_edit.setToolTip(gide.TIPP_ABBILD)
        self.waehlen_knopf = QPushButton("Wählen …", self)
        self.neu_knopf = QPushButton("Neue Platte …", self)
        self.auswerfen_knopf = QPushButton("Auswerfen", self)
        zeile = QWidget(self)
        zl = QHBoxLayout(zeile)
        zl.setContentsMargins(0, 0, 0, 0)
        for w in (self.abbild_edit, self.waehlen_knopf, self.neu_knopf, self.auswerfen_knopf):
            zl.addWidget(w, 1 if w is self.abbild_edit else 0)
        form.addRow("Plattenabbild:", zeile)

        self.rtc_box = QCheckBox("RTC 72421 auf der Karte", self)
        self.rtc_box.setToolTip(gide.TIPP_RTC)
        form.addRow(self.rtc_box)
        self.rtc_zeit_label = QLabel("", self)
        form.addRow("Uhr der Karte (UTC):", self.rtc_zeit_label)

        self.fehler_label = QLabel("", self)
        self.fehler_label.setStyleSheet("color: #c00;")
        self.fehler_label.setWordWrap(True)
        self.fehler_label.setVisible(False)
        form.addRow(self.fehler_label)

        self.karte_box.toggled.connect(self._aenderung)
        self.basis_combo.currentIndexChanged.connect(self._aenderung)
        self.rtc_box.toggled.connect(self._aenderung)
        self.abbild_edit.editingFinished.connect(self._aenderung)
        self.waehlen_knopf.clicked.connect(lambda _c=False: self._on_waehlen())
        self.neu_knopf.clicked.connect(lambda _c=False: self._on_neu())
        self.auswerfen_knopf.clicked.connect(lambda _c=False: self._on_auswerfen())
        self.set_zustand(gide.vorgabe(self._basis_vorgabe))

    # ── Zustand ──────────────────────────────────────────────────────────────

    def zustand(self) -> dict:
        """Die Eingaben als Zustand im Sinne von :mod:`app.gide`."""
        return {"enabled": self.karte_box.isChecked(),
                "basis": int(self.basis_combo.currentData()),
                "image": self.abbild_edit.text().strip(),
                "rtc": self.rtc_box.isChecked()}

    def set_zustand(self, z: dict, fehler: str = "", gesteckt: Optional[bool] = None) -> None:
        """Eingaben setzen (ohne Signal).  *gesteckt*: die Karte steckt WIRKLICH (sonst bleibt der Stand)."""
        self._guard = True
        self.karte_box.setChecked(bool(z.get("enabled")))
        idx = self.basis_combo.findData(int(z.get("basis", self._basis_vorgabe)))
        self.basis_combo.setCurrentIndex(idx if idx >= 0 else self.basis_combo.findData(self._basis_vorgabe))
        self.abbild_edit.setText(str(z.get("image") or ""))
        self.rtc_box.setChecked(z.get("rtc") is not False)
        self._guard = False
        if gesteckt is not None:
            self._gesteckt = bool(gesteckt)
        self.set_fehler(fehler)
        self._knoepfe()

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

    def set_rtc_anzeige(self, text: str) -> None:
        self.rtc_zeit_label.setText(text or "—")

    def gesteckt(self) -> bool:
        return self._gesteckt

    def _knoepfe(self) -> None:
        """Plattenbedienung nur mit gesteckter Karte; Auswerfen nur mit Abbild."""
        g = self._gesteckt
        self.abbild_edit.setEnabled(g)
        self.waehlen_knopf.setEnabled(g)
        self.neu_knopf.setEnabled(g)
        self.auswerfen_knopf.setEnabled(g and bool(self.abbild_edit.text().strip()))
        self.rtc_zeit_label.setEnabled(g)

    # ── Bedienung ────────────────────────────────────────────────────────────

    def _aenderung(self, *_args) -> None:
        self._knoepfe()
        if not self._guard:
            self.gideChanged.emit(self.zustand())

    def _datei_waehlen(self) -> str:
        pfad, _ = QFileDialog.getOpenFileName(
            self, "Plattenabbild einlegen", gide.platten_ordner(),
            "Plattenabbilder (*.img *.img.gz *.hdf *.bin);;Alle Dateien (*)")
        return pfad

    def _neue_platte_abfragen(self) -> Optional[Tuple[str, int]]:
        ordner = gide.platten_ordner()
        dlg = NeuePlatteDialog(ordner, self)
        if dlg.exec() != QDialog.Accepted:
            return None
        return dlg.ergebnis()

    def _frage(self, titel: str, text: str) -> bool:
        return QMessageBox.question(self, titel, text, QMessageBox.Yes | QMessageBox.No,
                                    QMessageBox.No) == QMessageBox.Yes

    def _on_waehlen(self) -> None:
        pfad = self._datei_waehlen()
        if pfad:
            self.abbild_edit.setText(pfad)
            self._aenderung()

    def _on_auswerfen(self) -> None:
        self.abbild_edit.setText("")
        self._aenderung()

    def _on_neu(self) -> None:
        wahl = self._neue_platte_abfragen()
        if not wahl or not wahl[0]:
            return
        self.neue_platte(*wahl)

    def neue_platte(self, pfad: str, mbyte: int) -> None:
        """Eine neue Platte anlegen lassen — eine VORHANDENE Datei wird nie überschrieben.

        Ist der Name vergeben, gibt es nur die Rückfrage, ob stattdessen diese Platte
        eingelegt werden soll (die Datei bleibt unberührt).
        """
        pfad = os.path.abspath(os.path.expanduser(pfad))
        if os.path.exists(pfad):
            if self._frage("Neue Platte",
                           f"Die Datei {pfad} gibt es schon.  Sie wird NICHT überschrieben.\n\n"
                           "Stattdessen diese Platte einlegen?"):
                self.abbild_edit.setText(pfad)
                self._aenderung()
            return
        self.neuePlatteGewuenscht.emit(pfad, int(mbyte))

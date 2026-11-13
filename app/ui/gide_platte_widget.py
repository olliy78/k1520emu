"""
K1520 Emulator - Kasten „IDE-Platte (GIDE)" im Reiter *Laufwerke*
=================================================================

Das Plattenabbild der GIDE steht -- wie die Winchesterplatte des P8000 (:mod:`app.ui.platten_widget`) --
unter den Disketten, nicht bei den Einstellungen.  Der Kasten ist nur sichtbar, solange die GIDE-Karte
GESTECKT ist (das Hauptfenster schaltet ihn); Karte, Basis und Uhr stehen im eigenen Kasten
(:mod:`app.ui.gide_gruppe`).

Plattenabbilder sind **immer gepackt** (``.img.gz``, wie die P8000-Platten): eine leere 256-MB-Platte
belegt rund 250 KB.  „Neue Platte …" legt nur ``.img.gz`` an (die Endung wird erzwungen); ein rohes Abbild
wird beim Wählen abgelehnt, mit Hinweistext (:data:`app.gide.HINWEIS_NUR_GEPACKT`).  Der Kern kann
Rohabbilder weiterhin lesen.

Das Widget kennt den Kern nicht: es meldet Wünsche als Signale (``abbildGewaehlt``,
``neuePlatteGewuenscht``), das Hauptfenster legt an / ein / aus.  Dialoge laufen über die kleinen Methoden
``_datei_waehlen`` / ``_neue_platte_abfragen`` / ``_frage`` -- Tests ersetzen sie, ein ``exec()`` stuende
headless bis zum Zeitueberlauf.
"""

from __future__ import annotations

import os
from typing import Optional, Tuple

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QComboBox, QDialog, QDialogButtonBox, QFileDialog, QFormLayout, QHBoxLayout, QLabel, QLineEdit,
    QMessageBox, QPushButton, QVBoxLayout, QWidget,
)

from app import gide


class NeuePlatteDialog(QDialog):
    """Name und Groesse einer neuen Platte (Abbild immer gepackt, ``.img.gz``)."""

    def __init__(self, ordner: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Neue Platte")
        lay = QVBoxLayout(self)
        form = QFormLayout()
        self.name_edit = QLineEdit(os.path.join(ordner, "gide" + gide.ENDUNG), self)
        self.name_edit.setToolTip("Die Endung .img.gz wird erzwungen (Plattenabbilder sind immer gepackt).")
        form.addRow("Datei:", self.name_edit)
        self.groesse_combo = QComboBox(self)
        for mb in gide.GROESSEN_MB:
            self.groesse_combo.addItem(f"{mb} MB", mb)
        self.groesse_combo.setCurrentIndex(self.groesse_combo.findData(gide.GROESSE_VORGABE_MB))
        form.addRow("Größe:", self.groesse_combo)
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
        return (gide.gz_pfad(pfad) if pfad else ""), int(self.groesse_combo.currentData())


class GidePlatteWidget(QWidget):
    """Wählen, Neu anlegen und Auswerfen des Plattenabbilds der GIDE."""

    #: Der Anwender will dieses Abbild einlegen (``""`` = auswerfen).
    abbildGewaehlt = Signal(str)
    #: „Neue Platte" bestaetigt: (Pfad ``.img.gz``, MB); das Hauptfenster legt an und legt ein.
    neuePlatteGewuenscht = Signal(str, int)
    #: Hinweis fuer die Statuszeile des Fensters.
    meldung = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._image = ""
        self._gesteckt = False
        lay = QVBoxLayout(self)
        lay.setContentsMargins(6, 6, 6, 6)
        self.titel = QLabel("<b>IDE-Platte (GIDE)</b>", self)
        lay.addWidget(self.titel)
        self.name = QLabel(self)
        self.name.setWordWrap(True)
        self.name.setTextInteractionFlags(Qt.TextSelectableByMouse)
        lay.addWidget(self.name)
        self.hinweis = QLabel(self)
        self.hinweis.setWordWrap(True)
        self.hinweis.setStyleSheet("color: gray;")
        lay.addWidget(self.hinweis)
        self.fehler_label = QLabel(self)
        self.fehler_label.setWordWrap(True)
        self.fehler_label.setStyleSheet("color: #c00;")
        self.fehler_label.setVisible(False)
        lay.addWidget(self.fehler_label)
        reihe = QHBoxLayout()
        self.waehlen_knopf = QPushButton("Einlegen …", self)
        self.neu_knopf = QPushButton("Neue Platte …", self)
        self.auswerfen_knopf = QPushButton("Auswerfen", self)
        for k in (self.waehlen_knopf, self.neu_knopf, self.auswerfen_knopf):
            k.setFocusPolicy(Qt.NoFocus)
            reihe.addWidget(k)
        lay.addLayout(reihe)
        self.waehlen_knopf.setToolTip("Ein vorhandenes gepacktes Plattenabbild (.img.gz) einlegen")
        self.neu_knopf.setToolTip("Eine neue, leere Platte anlegen (gepackt, Vorgabe 256 MB)")
        self.auswerfen_knopf.setToolTip("Das Abbild auswerfen (geänderte Sektoren werden vorher zurückgeschrieben)")
        self.waehlen_knopf.clicked.connect(lambda _c=False: self._on_waehlen())
        self.neu_knopf.clicked.connect(lambda _c=False: self._on_neu())
        self.auswerfen_knopf.clicked.connect(lambda _c=False: self._on_auswerfen())
        self._anzeigen()

    # ── Zustand ──────────────────────────────────────────────────────────────

    def pfad(self) -> str:
        return self._image

    def gesteckt(self) -> bool:
        return self._gesteckt

    def set_zustand(self, image: str, gesteckt: Optional[bool] = None, fehler: str = "") -> None:
        """Anzeige setzen (ohne Signal): *image* = eingelegter Pfad, *gesteckt* = Karte steckt WIRKLICH."""
        self._image = str(image or "")
        if gesteckt is not None:
            self._gesteckt = bool(gesteckt)
        self.set_fehler(fehler)
        self._anzeigen()

    def set_fehler(self, text: str) -> None:
        self.fehler_label.setText(text or "")
        self.fehler_label.setVisible(bool(text))

    def _anzeigen(self) -> None:
        g = self._gesteckt
        for k in (self.waehlen_knopf, self.neu_knopf):
            k.setEnabled(g)
        self.auswerfen_knopf.setEnabled(g and bool(self._image))
        if not self._image:
            self.name.setText("keine Platte eingelegt")
            self.name.setToolTip("")
            self.hinweis.setText(
                "„Neue Platte …“ legt ein leeres gepacktes Abbild (.img.gz) an, „Einlegen …“ nimmt ein "
                "vorhandenes.  Der Poppe-Treiber braucht mindestens 256 MB.")
        else:
            self.name.setText(os.path.basename(self._image))
            self.name.setToolTip(self._image)
            self.hinweis.setText("Gepackt gespeichert; Änderungen gehen von selbst in die Datei "
                                 "(nach einer Schreibpause, sofort beim Auswerfen und Beenden).")

    # ── Bedienung ────────────────────────────────────────────────────────────

    def _datei_waehlen(self) -> str:
        pfad, _ = QFileDialog.getOpenFileName(
            self, "Plattenabbild einlegen", gide.platten_ordner(),
            "Gepackte Plattenabbilder (*.img.gz *.gz)")
        return pfad

    def _neue_platte_abfragen(self) -> Optional[Tuple[str, int]]:
        dlg = NeuePlatteDialog(gide.platten_ordner(), self)
        if dlg.exec() != QDialog.Accepted:
            return None
        return dlg.ergebnis()

    def _frage(self, titel: str, text: str) -> bool:
        return QMessageBox.question(self, titel, text, QMessageBox.Yes | QMessageBox.No,
                                    QMessageBox.No) == QMessageBox.Yes

    def abbild_einlegen(self, pfad: str) -> bool:
        """Ein vorhandenes Abbild einlegen -- ein rohes wird mit Hinweistext abgelehnt (nicht stumm)."""
        pfad = os.path.abspath(os.path.expanduser(pfad))
        grund = gide.abbild_pruefen(pfad)
        if grund:
            self.set_fehler(grund)
            self.meldung.emit(grund)
            return False
        self.set_fehler("")
        self.abbildGewaehlt.emit(pfad)
        return True

    def _on_waehlen(self) -> None:
        pfad = self._datei_waehlen()
        if pfad:
            self.abbild_einlegen(pfad)

    def _on_auswerfen(self) -> None:
        self.set_fehler("")
        self.abbildGewaehlt.emit("")

    def _on_neu(self) -> None:
        wahl = self._neue_platte_abfragen()
        if not wahl or not wahl[0]:
            return
        self.neue_platte(*wahl)

    def neue_platte(self, pfad: str, mbyte: int) -> None:
        """Eine neue (gepackte) Platte anlegen lassen -- eine VORHANDENE Datei wird nie überschrieben.

        Die Endung ``.img.gz`` wird erzwungen.  Ist der Name vergeben, gibt es nur die Rückfrage, ob
        stattdessen diese Platte eingelegt werden soll (die Datei bleibt unberührt).
        """
        pfad = os.path.abspath(os.path.expanduser(gide.gz_pfad(pfad)))
        if os.path.exists(pfad):
            if self._frage("Neue Platte",
                           f"Die Datei {pfad} gibt es schon.  Sie wird NICHT überschrieben.\n\n"
                           "Stattdessen diese Platte einlegen?"):
                self.abbild_einlegen(pfad)
            return
        self.set_fehler("")
        self.neuePlatteGewuenscht.emit(pfad, int(mbyte))

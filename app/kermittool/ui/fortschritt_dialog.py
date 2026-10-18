"""Fortschrittsdialog einer Uebertragung — mit Abbruch.

Nicht blockierend (``show``, kein ``exec``): die Uebertragung laeuft im Arbeitsfaden, das
Fenster bekommt die Zahlen ueber Signale.  Der erste Klick auf *Abbrechen* beendet geordnet
(Gruppenabbruch, die Verbindung bleibt benutzbar); reagiert die Gegenstelle nicht, beendet
der zweite Klick (*Hart abbrechen*) die Sitzung.
"""

from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import QDialog, QLabel, QProgressBar, QPushButton, QVBoxLayout


class FortschrittDialog(QDialog):
    abbrechen = Signal(bool)        # hart?

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Übertragung")
        self.setWindowModality(Qt.WindowModal)
        self.resize(420, 10)
        self.titel = QLabel("")
        self.datei = QLabel("")
        self.balken = QProgressBar()
        self.balken.setTextVisible(True)
        self.knopf = QPushButton("Abbrechen")
        self.knopf.clicked.connect(self._klick)
        lay = QVBoxLayout(self)
        lay.addWidget(self.titel)
        lay.addWidget(self.datei)
        lay.addWidget(self.balken)
        lay.addWidget(self.knopf)
        self._stufe = 0

    def beginne(self, titel: str) -> None:
        self._stufe = 0
        self.knopf.setText("Abbrechen")
        self.knopf.setEnabled(True)
        self.titel.setText(titel)
        self.datei.setText("")
        self.balken.setRange(0, 0)          # unbestimmt, bis eine Groesse bekannt ist
        self.show()

    def setze(self, datei: str, b: int, g: int) -> None:
        self.datei.setText(datei)
        if g and g > 0:
            self.balken.setRange(0, 1000)
            self.balken.setValue(int(1000 * min(b, g) / g))
            self.balken.setFormat(f"{b} / {g} Bytes")
        else:
            self.balken.setRange(0, 0)

    def ende(self) -> None:
        self.hide()

    def _klick(self) -> None:
        hart = self._stufe >= 1
        self._stufe += 1
        self.knopf.setText("Hart abbrechen")
        self.datei.setText("Wird abgebrochen …")
        self.abbrechen.emit(hart)

    def closeEvent(self, event):            # noqa: N802
        event.ignore()                      # nur ueber den Knopf (sonst bliebe der Faden unbemerkt)

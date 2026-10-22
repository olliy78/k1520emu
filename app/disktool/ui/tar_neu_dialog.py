"""„Neue tar-Diskette / neuer tar-Satz…" — Format und Art waehlen (Entwurf 37, AP T4).

Das Format kommt aus ``DiskTool.tar_formate()`` (einheitliche Katalogformate, nicht
nur 720K).  Es gilt fuer den ganzen Satz.  Zwei Arten:

* **Einzelne Diskette**: die Datei entsteht sofort unter genau dem gewaehlten Namen;
  waechst der Inhalt ueber eine Diskette, wird beim Speichern ein nummerierter Satz.
* **Satz**: nichts entsteht, bis gespeichert wird; die Abbilder heissen von Anfang an
  ``name1``, ``name2`` ….
"""

from __future__ import annotations

from typing import List, Optional

from PySide6.QtWidgets import (
    QButtonGroup, QComboBox, QDialog, QDialogButtonBox, QLabel, QRadioButton,
    QVBoxLayout,
)

from app.disktool.ui.tar_satz_speichern_dialog import groesse_text

EINZEL = "einzel"
SATZ = "satz"


class TarNeuDialog(QDialog):
    """Formatwahl fuer eine neue tar-Diskette bzw. einen neuen tar-Satz."""

    def __init__(self, formate: List[dict], parent=None):
        super().__init__(parent)
        self.setWindowTitle("Neue tar-Diskette / neuer tar-Satz")
        self.setMinimumWidth(480)
        self.formate = formate

        lay = QVBoxLayout(self)
        lay.addWidget(QLabel("Format der Disketten (gilt für den ganzen Satz):"))
        self.wahl = QComboBox()
        for f in formate:
            self.wahl.addItem(f"{f['beschreibung']} — {groesse_text(f['bytes'])}  ({f['name']})",
                              f["name"])
        # 720K ist der Regelfall der P8000-Sammlung.
        for i, f in enumerate(formate):
            if f["name"] == "k5601_9x512":
                self.wahl.setCurrentIndex(i)
        lay.addWidget(self.wahl)

        self.r_einzel = QRadioButton("Einzelne Diskette (wächst bei Bedarf zu einem Satz)")
        self.r_satz = QRadioButton("Satz aus nummerierten Abbildern (name1, name2 …)")
        self.r_einzel.setChecked(True)
        gruppe = QButtonGroup(self)
        gruppe.addButton(self.r_einzel)
        gruppe.addButton(self.r_satz)
        lay.addWidget(self.r_einzel)
        lay.addWidget(self.r_satz)

        self.knoepfe = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        self.knoepfe.button(QDialogButtonBox.Ok).setText("Weiter…")
        self.knoepfe.button(QDialogButtonBox.Cancel).setText("Abbrechen")
        self.knoepfe.accepted.connect(self.accept)
        self.knoepfe.rejected.connect(self.reject)
        lay.addWidget(self.knoepfe)

    def format(self) -> str:
        return self.wahl.currentData()

    def art(self) -> str:
        return SATZ if self.r_satz.isChecked() else EINZEL

    def info(self) -> Optional[dict]:
        """Die Angaben zum gewaehlten Format (``img``/``hfe``: welche Container gehen)."""
        return next((f for f in self.formate if f["name"] == self.format()), None)

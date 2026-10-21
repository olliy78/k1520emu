"""Popup „Anmeldung" (K11) — die Gegenstelle verlangt Benutzername und Kennwort.

Erscheint beim Verbinden als Client, wenn die Gegenstelle ``login:`` zeigt und fuer die
Verbindung keine (vollstaendigen) Zugangsdaten vorliegen, und ERNEUT nach jeder Ablehnung
(Name und Kennwort mit den eben eingegebenen Werten vorbelegt, Hinweis „Anmeldung abgelehnt").

Der Haken *Zugangsdaten fuer diese Verbindung speichern* legt Name und Kennwort im KLARTEXT in
den Verbindungseintrag der ``kermit.yaml``.  Entscheidung des Anwenders (2026-10-22): kein
Schutz der Datei, keine Verschleierung — das Kennwort hat nur eine funktionale Aufgabe.  Der
Dialog sagt das offen (Label unter dem Haken und Tooltip).
"""

from __future__ import annotations

from typing import Optional, Tuple

from PySide6.QtWidgets import (
    QCheckBox, QDialog, QDialogButtonBox, QFormLayout, QLabel, QLineEdit, QVBoxLayout,
)

#: Vorschlag als Hinweis (nicht als stilles Vorausfuellen).
VORSCHLAG_HINWEIS = "Möglicherweise ist es einfach kermit / kermit."

SPEICHERN_TEXT = "Zugangsdaten für diese Verbindung speichern"

#: Neben dem Haken (auch als Tooltip) — gemeinsam mit dem Verbindungsdialog.
KLARTEXT_HINWEIS = ("Achtung: Das Kennwort wird dabei im Klartext in der Konfigurationsdatei (kermit.yaml) "
                    "gespeichert, ohne Schutz der Datei.  Es dient nur der Anmeldung an dieser Gegenstelle.")


class AnmeldeDialog(QDialog):
    def __init__(self, parent, versuch: int = 0, gelesen: str = "", name: str = "", kennwort: str = "",
                 speichern: bool = False, gegenstelle: str = ""):
        super().__init__(parent)
        self.setWindowTitle("Anmeldung")
        self.resize(440, 10)
        titel = "Die Gegenstelle verlangt eine Anmeldung."
        if gegenstelle:
            titel = f"{gegenstelle} verlangt eine Anmeldung."
        self.kopf = QLabel(titel)
        self.kopf.setWordWrap(True)
        self.abgelehnt = QLabel("Anmeldung abgelehnt.  Bitte erneut versuchen.")
        self.abgelehnt.setStyleSheet("color: #a33;")
        self.abgelehnt.setVisible(versuch > 0)
        self.abgelehnt.setToolTip(gelesen.strip()[-200:])
        self.name = QLineEdit(name)
        self.kennwort = QLineEdit(kennwort)
        self.kennwort.setEchoMode(QLineEdit.Password)
        self.vorschlag = QLabel(VORSCHLAG_HINWEIS)
        self.vorschlag.setWordWrap(True)
        self.speichern = QCheckBox(SPEICHERN_TEXT)
        self.speichern.setChecked(speichern)
        self.speichern.setToolTip(KLARTEXT_HINWEIS)
        self.klartext = QLabel(KLARTEXT_HINWEIS)
        self.klartext.setWordWrap(True)
        self.klartext.setStyleSheet("color: #886600;")
        self.meldung = QLabel("")
        self.meldung.setStyleSheet("color: #a33;")

        form = QFormLayout()
        form.addRow("Benutzername", self.name)
        form.addRow("Kennwort", self.kennwort)
        self.knoepfe = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        self.knoepfe.button(QDialogButtonBox.Ok).setText("Anmelden")
        self.knoepfe.button(QDialogButtonBox.Cancel).setText("Abbrechen")
        self.knoepfe.accepted.connect(self._annehmen)
        self.knoepfe.rejected.connect(self.reject)
        lay = QVBoxLayout(self)
        lay.addWidget(self.kopf)
        lay.addWidget(self.abgelehnt)
        lay.addLayout(form)
        lay.addWidget(self.vorschlag)
        lay.addWidget(self.speichern)
        lay.addWidget(self.klartext)
        lay.addWidget(self.meldung)
        lay.addWidget(self.knoepfe)
        (self.kennwort if name else self.name).setFocus()

    def ergebnis(self) -> Tuple[str, str, bool]:
        """(Name, Kennwort, speichern)."""
        return self.name.text().strip(), self.kennwort.text(), self.speichern.isChecked()

    def _annehmen(self) -> None:
        if not self.name.text().strip():
            self.meldung.setText("Bitte einen Benutzernamen eintragen.")
            return
        self.accept()


def fragen(parent, versuch: int, gelesen: str, name: str, kennwort: str, speichern: bool,
           gegenstelle: str = "") -> Optional[Tuple[str, str, bool]]:
    """Das Popup modal zeigen; ``None`` = Abbrechen."""
    d = AnmeldeDialog(parent, versuch, gelesen, name, kennwort, speichern, gegenstelle)
    if d.exec() != QDialog.Accepted:
        return None
    return d.ergebnis()

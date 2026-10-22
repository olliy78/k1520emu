"""Rueckfrage vor dem Speichern eines tar-Satzes (Entwurf 37, AP T4).

Ein tar-Satz lebt als EIN Strom im Speicher; erst das Speichern verteilt ihn auf
Abbilder gleichen Formats (``name1.hfe``, ``name2.hfe`` …).  Weil dabei Dateien
entstehen und vorhandene ersetzt werden, zeigt der Dialog VORHER, was geschehen wird:
Anzahl und Namen der Abbilder, was ueberschrieben wird (mit Sicherung ``~``) und
welche hoeheren Nummern liegenbleiben.  Der Kern rechnet (``tar_satz_planen``), der
Dialog zeigt nur an und plant bei jeder Aenderung des Basisnamens neu.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtWidgets import (
    QDialog, QDialogButtonBox, QFileDialog, QHBoxLayout, QLabel, QLineEdit,
    QListWidget, QPushButton, QVBoxLayout,
)

from app.core_binding.k1520disk import K1520DiskError


def groesse_text(n: int) -> str:
    """Bytes lesbar: ``737280`` -> ``720 KiB``."""
    if n >= 1 << 20:
        return f"{n / (1 << 20):.1f} MiB"
    if n >= 1024:
        return f"{n / 1024:.0f} KiB"
    return f"{n} Byte"


def plan_text(plan: dict) -> str:
    """Der Satz ueber das Ergebnis der Planung (ohne Qt, damit pruefbar)."""
    n = plan["abbilder"]
    wie = "ein Abbild" if n == 1 else f"{n} Abbilder"
    return (f"Der Inhalt ({groesse_text(plan['strom'])}) wird auf {wie} zu je "
            f"{groesse_text(plan['kapazitaet'])} verteilt.")


class TarSatzSpeichernDialog(QDialog):
    """Namen und Umfang des Speicherns zeigen und bestaetigen lassen."""

    def __init__(self, tool, parent=None, basis: Optional[str] = None,
                 namen_aendern: bool = False):
        super().__init__(parent)
        self.tool = tool
        self.setWindowTitle("tar-Satz speichern")
        self.setMinimumWidth(560)

        lay = QVBoxLayout(self)
        lay.addWidget(QLabel(
            "Der tar-Satz wird auf Abbilder gleichen Formats geschrieben.  Die Nummer "
            "steht am Ende des Namens vor der Endung (<i>name1.hfe</i>, <i>name2.hfe</i> …); "
            "die Endung bestimmt den Container."))

        zeile = QHBoxLayout()
        zeile.addWidget(QLabel("Basisname:"))
        self.f_basis = QLineEdit(basis if basis is not None else tool.tar_satz_basis)
        self.f_basis.textChanged.connect(self._planen)
        zeile.addWidget(self.f_basis, 1)
        self.b_aendern = QPushButton("Ändern…")
        self.b_aendern.clicked.connect(self._waehlen)
        zeile.addWidget(self.b_aendern)
        lay.addLayout(zeile)

        self.l_plan = QLabel("")
        self.l_plan.setWordWrap(True)
        lay.addWidget(self.l_plan)
        self.liste = QListWidget()
        self.liste.setMaximumHeight(120)
        lay.addWidget(self.liste)
        self.l_ueber = QLabel("")
        self.l_ueber.setWordWrap(True)
        lay.addWidget(self.l_ueber)
        self.l_alt = QLabel("")
        self.l_alt.setWordWrap(True)
        lay.addWidget(self.l_alt)
        self.l_fehler = QLabel("")
        self.l_fehler.setWordWrap(True)
        self.l_fehler.setStyleSheet("color: #b00020;")
        lay.addWidget(self.l_fehler)

        self.knoepfe = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        self.knoepfe.button(QDialogButtonBox.Ok).setText("Speichern")
        self.knoepfe.button(QDialogButtonBox.Cancel).setText("Abbrechen")
        self.knoepfe.accepted.connect(self.accept)
        self.knoepfe.rejected.connect(self.reject)
        lay.addWidget(self.knoepfe)

        self.plan: Optional[dict] = None
        self._planen()
        if namen_aendern:
            self.f_basis.setFocus()

    def _waehlen(self) -> None:
        pfad, _ = QFileDialog.getSaveFileName(
            self, "Basisname des Satzes", self.f_basis.text(),
            "Abbilder (*.hfe *.dmk *.img *.cqm)")
        if pfad:
            self.f_basis.setText(pfad)

    def basis(self) -> str:
        """Der gewaehlte Basisname (mit Endung)."""
        return self.f_basis.text().strip()

    def _planen(self) -> None:
        """Beim Tippen neu planen; ein Fehler sperrt „Speichern" und nennt den Grund."""
        self.liste.clear()
        self.l_ueber.clear()
        self.l_alt.clear()
        self.l_fehler.clear()
        try:
            self.plan = self.tool.tar_satz_planen(self.basis() or None)
        except K1520DiskError as e:
            self.plan = None
            self.l_plan.setText("")
            self.l_fehler.setText(str(e))
            self.knoepfe.button(QDialogButtonBox.Ok).setEnabled(False)
            return
        p = self.plan
        self.knoepfe.button(QDialogButtonBox.Ok).setEnabled(True)
        self.l_plan.setText(plan_text(p))
        self.liste.addItems(p["namen"])
        if p["ueberschreibt"]:
            self.l_ueber.setText(
                f"<b>Es gibt schon:</b> {len(p['ueberschreibt'])} dieser Dateien.  Sie werden "
                "überschrieben; beim ersten Mal entsteht vorher eine Sicherung „<name>~“.")
        if p["altlasten"]:
            self.l_alt.setText(
                "<b>Bleiben liegen</b> (höhere Nummern, gehören nicht mehr zum Satz — gelöscht "
                "wird nichts): " + ", ".join(p["altlasten"]))

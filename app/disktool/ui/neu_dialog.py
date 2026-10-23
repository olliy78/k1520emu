"""„Neue Diskette" — Dateisystem waehlen, bei UNIX-Typen die Inode-Zahl.

Bei WEGA/MUTOS (``wega_mutos``) erscheint unter der Auswahl die Zeile
„Abweichende Anzahl von Inodes": das Zahlenfeld zeigt zunaechst die Vorgabe
(Bloecke/25 Inode-Bloecke, wie ``mkfs``) und ist gesperrt; erst mit dem Haken laesst sich
eine andere Zahl eintragen (z. B. 272 fuer eine bis zum Rand gefuellte Originaldiskette).
Der Haken sitzt NICHT bei anderen Dateisystemen — dort gibt es keine Inode-Liste.
"""

from __future__ import annotations

from typing import List, Tuple

from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QDialog, QDialogButtonBox, QHBoxLayout, QLabel, QSpinBox,
    QVBoxLayout, QWidget,
)

from app.core_binding.k1520disk import filesystems, unix_standard_inodes


class NeuDialog(QDialog):
    """Dateisystemwahl fuer eine neue Diskette (mit optionaler Inode-Zahl)."""

    def __init__(self, eintraege: List[Tuple[str, str]], parent=None):
        super().__init__(parent)
        self.setWindowTitle("Neue Diskette")
        self.setMinimumWidth(520)
        self._typ = {f.name: f.type for f in filesystems()}

        lay = QVBoxLayout(self)
        lay.addWidget(QLabel("Dateisystem:"))
        self.wahl = QComboBox()
        for name, text in eintraege:
            self.wahl.addItem(text, name)
        lay.addWidget(self.wahl)

        # Nur fuer UNIX-Dateisysteme sichtbar
        self.unix = QWidget()
        zeile = QHBoxLayout(self.unix)
        zeile.setContentsMargins(0, 0, 0, 0)
        self.haken = QCheckBox("Abweichende Anzahl von Inodes")
        self.haken.setToolTip(
            "Die Inode-Liste begrenzt die Zahl der Dateien (je Datei, Verzeichnis und Gerät eine "
            "Inode) und belegt Blöcke, die für Daten fehlen.\nVorgabe wie mkfs: Blöcke / 25 "
            "Blöcke für die Inode-Liste.")
        self.zahl = QSpinBox()
        self.zahl.setRange(1, 65500 * 16)
        self.zahl.setEnabled(False)
        zeile.addWidget(self.haken)
        zeile.addWidget(self.zahl)
        zeile.addStretch(1)
        lay.addWidget(self.unix)

        self.knoepfe = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        self.knoepfe.button(QDialogButtonBox.Ok).setText("Weiter…")
        self.knoepfe.button(QDialogButtonBox.Cancel).setText("Abbrechen")
        self.knoepfe.accepted.connect(self.accept)
        self.knoepfe.rejected.connect(self.reject)
        lay.addWidget(self.knoepfe)

        self.wahl.currentIndexChanged.connect(self._fs_gewaehlt)
        self.haken.toggled.connect(self._haken_gesetzt)
        self._fs_gewaehlt()

    # ── Zustand ──────────────────────────────────────────────────────────────
    def dateisystem(self) -> str:
        return self.wahl.currentData()

    def inodes(self) -> int:
        """Gewaehlte Mindestzahl Inodes; 0 = Vorgabe (kein Haken oder kein UNIX-Typ)."""
        if self.unix.isHidden() or not self.haken.isChecked():
            return 0
        return int(self.zahl.value())

    # ── intern ───────────────────────────────────────────────────────────────
    def _vorgabe(self) -> int:
        return unix_standard_inodes(self.dateisystem())

    def _fs_gewaehlt(self) -> None:
        ist_unix = self._typ.get(self.dateisystem()) == "wega_mutos"
        self.unix.setVisible(ist_unix)
        self.haken.setChecked(False)          # eine neue Wahl beginnt wieder bei der Vorgabe
        self.zahl.setValue(max(1, self._vorgabe()))
        self.zahl.setEnabled(False)

    def _haken_gesetzt(self, an: bool) -> None:
        self.zahl.setEnabled(an)
        if not an:
            self.zahl.setValue(max(1, self._vorgabe()))

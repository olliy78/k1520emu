"""„Neue Diskette" — Dateisystem waehlen, bei UNIX-Typen die Inode-Zahl.

Bei WEGA/MUTOS (``wega_mutos``) erscheint unter der Auswahl die Zeile
„Abweichende Anzahl von Inodes": das Zahlenfeld zeigt zunaechst die Vorgabe
(Bloecke/25 Inode-Bloecke, wie ``mkfs``) und ist gesperrt; erst mit dem Haken laesst sich
eine andere Zahl eintragen (z. B. 272 fuer eine bis zum Rand gefuellte Originaldiskette).
Der Haken sitzt NICHT bei anderen Dateisystemen — dort gibt es keine Inode-Liste.

Darunter „Bootdiskette erstellen" (Ticket #25): Haken plus Auswahlfeld mit den
hinterlegten Bootabbildern des gewaehlten Dateisystems (``bootsektor.yaml``,
:mod:`app.disktool.bootsektoren`), der erste Treffer vorgewaehlt, als letzter Eintrag
„Eigene .bin-Datei auswählen…" (das fruehere Verhalten).  Gibt es fuer das Dateisystem
keinen Eintrag, ist der Haken gesperrt.  Der Dateidialog fuer die neue Diskette kommt
erst NACH „Diskette anlegen" — der Dialog selbst fragt nur ab, was angelegt wird.
"""

from __future__ import annotations

from pathlib import Path
from typing import Dict, List, Optional, Tuple

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QDialog, QDialogButtonBox, QFileDialog, QHBoxLayout, QLabel,
    QSpinBox, QVBoxLayout, QWidget,
)

from app.core_binding.k1520disk import filesystems, unix_standard_inodes
from app.disktool.bootsektoren import Bootabbild

#: Datenwert des letzten Eintrags im Bootabbild-Feld: eine eigene .bin-Datei.
EIGENE_DATEI = "__eigene_bin__"
#: Text dieses Eintrags (Oberflaeche).
EIGENE_DATEI_TEXT = "Eigene .bin-Datei auswählen…"


class NeuDialog(QDialog):
    """Dateisystemwahl fuer eine neue Diskette (mit optionaler Inode-Zahl)."""

    def __init__(self, eintraege: List[Tuple[str, str]],
                 bootabbilder: Optional[Dict[str, List[Bootabbild]]] = None,
                 parent=None, ordner: str = ""):
        super().__init__(parent)
        self._bootabbilder = bootabbilder or {}
        self._ordner = ordner              # Startordner fuer eine eigene .bin
        self._eigene: Optional[Path] = None
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

        # Bootdiskette: Haken + hinterlegte Abbilder des gewaehlten Dateisystems
        boot = QHBoxLayout()
        self.boot_haken = QCheckBox("Bootdiskette erstellen")
        self.boot_haken.setToolTip(
            "Schreibt ein Bootabbild in die Systemspuren — erst das macht die Diskette "
            "startfähig.\nDie Auswahl kommt aus bootsektor.yaml; eigene Einträge gehören "
            "in die bootsektor.yaml im Konfigurationsordner.")
        self.boot_wahl = QComboBox()
        self.boot_wahl.setSizeAdjustPolicy(QComboBox.AdjustToMinimumContentsLengthWithIcon)
        self.boot_wahl.setMinimumContentsLength(28)
        boot.addWidget(self.boot_haken)
        boot.addWidget(self.boot_wahl, 1)
        lay.addLayout(boot)

        self.knoepfe = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        self.knoepfe.button(QDialogButtonBox.Ok).setText("Diskette anlegen")
        self.knoepfe.button(QDialogButtonBox.Cancel).setText("Abbrechen")
        self.knoepfe.accepted.connect(self._anlegen)
        self.knoepfe.rejected.connect(self.reject)
        lay.addWidget(self.knoepfe)

        self.wahl.currentIndexChanged.connect(self._fs_gewaehlt)
        self.haken.toggled.connect(self._haken_gesetzt)
        self.boot_haken.toggled.connect(self.boot_wahl.setEnabled)
        self.boot_wahl.activated.connect(self._boot_gewaehlt)
        self._fs_gewaehlt()

    # ── Zustand ──────────────────────────────────────────────────────────────
    def dateisystem(self) -> str:
        return self.wahl.currentData()

    def inodes(self) -> int:
        """Gewaehlte Mindestzahl Inodes; 0 = Vorgabe (kein Haken oder kein UNIX-Typ)."""
        if self.unix.isHidden() or not self.haken.isChecked():
            return 0
        return int(self.zahl.value())

    def bootabbild(self) -> Optional[Path]:
        """Gewaehltes Bootabbild; ``None`` = gewoehnliche Diskette (kein Haken)."""
        if not self.boot_haken.isEnabled() or not self.boot_haken.isChecked():
            return None
        d = self.boot_wahl.currentData()
        if d == EIGENE_DATEI:
            return self._eigene
        return Path(d) if d else None

    # ── intern ───────────────────────────────────────────────────────────────
    def _boot_fuellen(self) -> None:
        """Auswahlfeld fuer das gewaehlte Dateisystem neu fuellen, Treffer 1 vorgewaehlt."""
        treffer = self._bootabbilder.get(self.dateisystem(), [])
        self.boot_wahl.clear()
        self._eigene = None
        for e in treffer:
            self.boot_wahl.addItem(e.description, str(e.path))
            # Tooltip: wo die Datei liegt (eigene Eintraege des Anwenders erkennbar)
            self.boot_wahl.setItemData(self.boot_wahl.count() - 1, str(e.path), Qt.ToolTipRole)
        if treffer:
            self.boot_wahl.addItem(EIGENE_DATEI_TEXT, EIGENE_DATEI)
            self.boot_wahl.setCurrentIndex(0)
        self.boot_haken.setChecked(False)
        self.boot_haken.setEnabled(bool(treffer))
        self.boot_wahl.setEnabled(False)

    def _eigene_waehlen(self) -> bool:
        pfad, _ = QFileDialog.getOpenFileName(
            self, "Bootabbild auswählen", self._ordner,
            "Bootabbild (*.bin);;Alle Dateien (*)")
        if not pfad:
            return False
        self._eigene = Path(pfad)
        i = self.boot_wahl.findData(EIGENE_DATEI)
        self.boot_wahl.setItemText(i, f"Eigene: {self._eigene.name}")
        self.boot_wahl.setItemData(i, pfad, Qt.ToolTipRole)
        return True

    def _boot_gewaehlt(self, index: int) -> None:
        # Die eigene .bin wird gleich bei der Wahl erfragt.  Abgebrochen: eine frueher
        # gewaehlte eigene Datei bleibt, sonst zurueck auf Treffer 1.
        if self.boot_wahl.itemData(index) != EIGENE_DATEI:
            return
        if not self._eigene_waehlen() and self._eigene is None:
            self.boot_wahl.setCurrentIndex(0)

    def _anlegen(self) -> None:
        # „Eigene" gewaehlt, aber noch keine Datei: jetzt fragen; ohne Datei bleibt der Dialog offen.
        if (self.boot_haken.isChecked() and self.boot_wahl.currentData() == EIGENE_DATEI
                and self._eigene is None and not self._eigene_waehlen()):
            return
        self.accept()

    def _vorgabe(self) -> int:
        return unix_standard_inodes(self.dateisystem())

    def _fs_gewaehlt(self) -> None:
        ist_unix = self._typ.get(self.dateisystem()) == "wega_mutos"
        self.unix.setVisible(ist_unix)
        self.haken.setChecked(False)          # eine neue Wahl beginnt wieder bei der Vorgabe
        self.zahl.setValue(max(1, self._vorgabe()))
        self.zahl.setEnabled(False)
        self._boot_fuellen()

    def _haken_gesetzt(self, an: bool) -> None:
        self.zahl.setEnabled(an)
        if not an:
            self.zahl.setValue(max(1, self._vorgabe()))

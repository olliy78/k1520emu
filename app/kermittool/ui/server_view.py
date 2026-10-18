"""Linke Seite im Server-Modus: Uebertragungen und Ereignisse statt eines Verzeichnisses.

Eine Zeile je Ereignis; eine Dateiuebertragung bekommt EINE Zeile, die nachgefuehrt wird
(Bytes, Zustand), solange Fortschritt fuer dieselbe Datei kommt.
"""

from __future__ import annotations

import time
from typing import Optional

from PySide6.QtWidgets import QAbstractItemView, QTreeWidget, QTreeWidgetItem


class ServerView(QTreeWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setHeaderLabels(["Zeit", "Art", "Text / Datei", "Bytes", "Zustand"])
        self.setRootIsDecorated(False)
        self.setUniformRowHeights(True)
        self.setAlternatingRowColors(True)
        self.setSelectionMode(QAbstractItemView.NoSelection)
        self.setColumnWidth(0, 70)
        self.setColumnWidth(1, 80)
        self.setColumnWidth(2, 260)
        self._zeile: Optional[QTreeWidgetItem] = None
        self._datei = ""

    @staticmethod
    def _zeit() -> str:
        return time.strftime("%H:%M:%S")

    def ereignis(self, text: str) -> None:
        self._zeile = None
        self._datei = ""
        it = QTreeWidgetItem(self, [self._zeit(), "Ereignis", text, "", ""])
        self.scrollToItem(it)

    def fortschritt(self, datei: str, b: int, g: int) -> None:
        """Eine Datei laeuft: Zeile anlegen oder nachfuehren; ``b >= g`` = fertig."""
        if self._zeile is None or datei != self._datei or self._zeile.text(4) == "fertig":
            self._zeile = QTreeWidgetItem(self, [self._zeit(), "Datei", datei, "", "läuft"])
            self._datei = datei
        z = self._zeile
        z.setText(3, f"{b} / {g}" if g > 0 else str(b))
        if g > 0 and b >= g:
            z.setText(4, "fertig")
        self.scrollToItem(z)

    def zeilen(self) -> list:
        """Inhalt als Liste von Tupeln (fuer Tests)."""
        return [tuple(self.topLevelItem(i).text(c) for c in range(5)) for i in range(self.topLevelItemCount())]

    def leeren(self) -> None:
        self.clear()
        self._zeile = None
        self._datei = ""

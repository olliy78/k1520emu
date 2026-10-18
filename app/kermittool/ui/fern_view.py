"""Linke Seite (Client-Modus): das Verzeichnis des Servers.

Zeigt den ``ls -l``-Text der Gegenstelle, zerlegt von :func:`app.kermittool.binding.parse_ls`
(Name/Art/Groesse/Datum/Rechte).  Ein Doppelklick auf ein Verzeichnis oder ``..`` wechselt
(``G C``), auf eine Datei holt sie.  Dateien lassen sich auf die lokale Seite ziehen; hier
angenommen werden lokale Dateien (-> senden).
"""

from __future__ import annotations

from typing import List, Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QDrag
from PySide6.QtWidgets import (
    QAbstractItemView, QHBoxLayout, QLabel, QTreeWidget, QTreeWidgetItem, QVBoxLayout, QWidget,
)

from app.ui_icons import icon
from app.kermittool.ui import ablage

ART_ROLE = Qt.UserRole + 1
NAME_ROLE = Qt.UserRole + 2


def _datum(e: dict) -> str:
    if e.get("monat", -1) is None or e.get("monat", -1) < 0:
        return e.get("datum", "") or ""
    jahr = e.get("jahr", -1)
    zeit = f"{e['stunde']:02d}:{e['minute']:02d}" if e.get("stunde", -1) >= 0 else ""
    kopf = f"{jahr:04d}-" if jahr and jahr > 0 else ""
    return f"{kopf}{e['monat']:02d}-{e['tag']:02d} {zeit}".strip()


class _Baum(QTreeWidget):
    ablage_dateien = Signal(list)       # lokale Pfade -> senden
    aktiviert_name = Signal(str, str)   # (Art, Name)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAcceptDrops(True)
        self.setDragEnabled(True)
        self.setDragDropMode(QAbstractItemView.DragDrop)
        self.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.setUniformRowHeights(True)
        self.setAlternatingRowColors(True)
        self.setRootIsDecorated(False)
        self.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.setHeaderLabels(["Name", "Größe", "Datum", "Rechte"])
        self.setColumnWidth(0, 240)
        self.itemActivated.connect(
            lambda it, _c: self.aktiviert_name.emit(it.data(0, ART_ROLE) or "-", it.data(0, NAME_ROLE) or ""))

    def dateinamen(self) -> List[str]:
        return [i.data(0, NAME_ROLE) for i in self.selectedItems()
                if i.data(0, ART_ROLE) not in ("d", "up") and i.data(0, NAME_ROLE)]

    def startDrag(self, actions):               # noqa: N802
        namen = self.dateinamen()
        if not namen:
            return
        drag = QDrag(self)
        drag.setMimeData(ablage.fern_mime(namen))
        drag.exec(Qt.CopyAction)

    def dragEnterEvent(self, event):            # noqa: N802
        if ablage.lokale_dateien_aus(event.mimeData()):
            event.acceptProposedAction()
        else:
            event.ignore()

    def dragMoveEvent(self, event):             # noqa: N802
        self.dragEnterEvent(event)

    def dropEvent(self, event):                 # noqa: N802
        dateien = ablage.lokale_dateien_aus(event.mimeData())
        if not dateien:
            event.ignore()
            return
        event.acceptProposedAction()
        self.ablage_dateien.emit(dateien)


class FernView(QWidget):
    """Verzeichnisliste des Servers mit Pfadzeile."""

    wechseln = Signal(str)              # Verzeichnisname oder ".."
    holen = Signal(list)                # Dateinamen (Doppelklick)
    senden = Signal(list)               # lokale Pfade (Ablage)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.pfad = QLabel("Nicht verbunden")
        self.pfad.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self.pfad.setToolTip("Verzeichnis auf dem Server (relativ zum Start der Sitzung)")
        self.baum = _Baum()
        self.baum.ablage_dateien.connect(self.senden)
        self.baum.aktiviert_name.connect(self._aktiviert)
        kopf = QHBoxLayout()
        kopf.setContentsMargins(0, 0, 0, 0)
        kopf.addWidget(self.pfad, 1)
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(2)
        lay.addLayout(kopf)
        lay.addWidget(self.baum, 1)
        self._aktionen: list = []
        self.baum.setContextMenuPolicy(Qt.CustomContextMenu)
        self.baum.customContextMenuRequested.connect(self._menue)

    def setze_aktionen(self, *aktionen) -> None:
        self._aktionen = list(aktionen)

    def _menue(self, punkt) -> None:
        from PySide6.QtWidgets import QMenu
        if not self._aktionen:
            return
        m = QMenu(self)
        for a in self._aktionen:
            m.addSeparator() if a is None else m.addAction(a)
        m.exec(self.baum.viewport().mapToGlobal(punkt))

    def leeren(self, text: str = "Nicht verbunden") -> None:
        self.baum.clear()
        self.pfad.setText(text)

    def zeige(self, eintraege: list, pfad_text: str) -> None:
        """Verzeichnisinhalt anzeigen: ``..`` zuerst, Ordner vor Dateien, dann nach Name."""
        self.baum.clear()
        self.pfad.setText(pfad_text)
        auf = QTreeWidgetItem(self.baum, ["..", "", "", ""])
        auf.setData(0, ART_ROLE, "up")
        auf.setData(0, NAME_ROLE, "..")
        auf.setIcon(0, icon("folder"))
        for e in sorted(eintraege, key=lambda e: (e.get("art") != "d", e.get("name", "").lower())):
            art = e.get("art", "-")
            name = e.get("name", "")
            gr = e.get("groesse", -1)
            it = QTreeWidgetItem(self.baum, [name + ("/" if art == "d" else ""),
                                             "" if art == "d" or gr is None or gr < 0 else str(gr),
                                             _datum(e), e.get("rechte", "") or ""])
            it.setData(0, ART_ROLE, art)
            it.setData(0, NAME_ROLE, name)
            if art == "d":
                it.setIcon(0, icon("folder"))
            it.setTextAlignment(1, Qt.AlignRight | Qt.AlignVCenter)

    def dateinamen(self) -> List[str]:
        """Die ausgewaehlten DATEIEN (Ordner und ``..`` fallen weg)."""
        return self.baum.dateinamen()

    def _aktiviert(self, art: str, name: str) -> None:
        if art in ("d", "up"):
            self.wechseln.emit(name)
        elif name:
            self.holen.emit([name])

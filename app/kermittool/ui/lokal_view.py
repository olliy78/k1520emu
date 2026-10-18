"""Rechte Seite: ein lokaler Ordner — Adresszeile, Ordnerknopf, Dateiliste.

Doppelklick auf einen Ordner (oder ``..``) wechselt, auf eine Datei sendet sie.  Dateien lassen
sich auf die Serverseite ziehen; hier angenommen werden Dateien VOM Server (-> holen in den
angezeigten Ordner).  Der Ordner wird hier nicht veraendert (kein Umbenennen/Loeschen).
"""

from __future__ import annotations

from pathlib import Path
from typing import List, Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QDrag
from PySide6.QtWidgets import (
    QAbstractItemView, QHBoxLayout, QLineEdit, QToolButton, QTreeWidget, QTreeWidgetItem,
    QVBoxLayout, QWidget,
)

from app.ui_icons import icon
from app.kermittool.ui import ablage

PFAD_ROLE = Qt.UserRole + 1
NAV_ROLE = Qt.UserRole + 2


class _Baum(QTreeWidget):
    ablage_fern = Signal(list)          # Servernamen -> holen
    nach_oben = Signal()

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
        self.setHeaderLabels(["Name", "Größe"])
        self.setColumnWidth(0, 260)

    def pfade(self) -> List[str]:
        return [i.data(0, PFAD_ROLE) for i in self.selectedItems() if i.data(0, PFAD_ROLE)]

    def startDrag(self, actions):               # noqa: N802
        pfade = self.pfade()
        if not pfade:
            return
        drag = QDrag(self)
        drag.setMimeData(ablage.lokal_mime(pfade))
        drag.exec(Qt.CopyAction)

    def dragEnterEvent(self, event):            # noqa: N802
        if ablage.fern_namen_aus(event.mimeData()):
            event.acceptProposedAction()
        else:
            event.ignore()

    def dragMoveEvent(self, event):             # noqa: N802
        self.dragEnterEvent(event)

    def dropEvent(self, event):                 # noqa: N802
        namen = ablage.fern_namen_aus(event.mimeData())
        if not namen:
            event.ignore()
            return
        event.acceptProposedAction()
        self.ablage_fern.emit(namen)

    def keyPressEvent(self, event):             # noqa: N802
        if event.key() == Qt.Key_Backspace:
            self.nach_oben.emit()
            return
        super().keyPressEvent(event)


class LokalView(QWidget):
    """Lokale Ordnerseite."""

    ordner_geaendert = Signal(str)
    ordner_waehlen = Signal()
    senden = Signal(list)               # lokale Pfade (Doppelklick auf eine Datei)
    holen = Signal(list)                # Servernamen (Ablage von links)
    hinweis = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._ordner: Optional[Path] = None
        self.adresse = QLineEdit()
        self.adresse.setPlaceholderText("Pfad eingeben und mit Eingabe bestätigen")
        self.adresse.setToolTip("Lokaler Ordner — änderbar, Eingabe wechselt dorthin")
        self.adresse.returnPressed.connect(self._adresse_uebernehmen)
        self.knopf = QToolButton()
        self.knopf.setIcon(icon("folder"))
        self.knopf.setAutoRaise(True)
        self.knopf.setToolTip("Ordner im Dialog wählen")
        self.knopf.clicked.connect(self.ordner_waehlen)

        kopf = QHBoxLayout()
        kopf.setContentsMargins(0, 0, 0, 0)
        kopf.setSpacing(2)
        kopf.addWidget(self.adresse, 1)
        kopf.addWidget(self.knopf)

        self.baum = _Baum()
        self.baum.ablage_fern.connect(self.holen)
        self.baum.nach_oben.connect(self.hinauf)
        self.baum.itemActivated.connect(self._aktiviert)
        self._aktionen: list = []
        self.baum.setContextMenuPolicy(Qt.CustomContextMenu)
        self.baum.customContextMenuRequested.connect(self._menue)

        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(2)
        lay.addLayout(kopf)
        lay.addWidget(self.baum, 1)

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

    @property
    def ordner(self) -> Optional[Path]:
        return self._ordner

    def set_ordner(self, pfad) -> None:
        self._ordner = Path(pfad).expanduser() if pfad else None
        self.adresse.setText(str(self._ordner) if self._ordner else "")
        self.aktualisieren()
        if self._ordner:
            self.ordner_geaendert.emit(str(self._ordner))

    def hinauf(self) -> None:
        if self._ordner is not None and self._ordner.parent != self._ordner:
            self.set_ordner(self._ordner.parent)

    def _adresse_uebernehmen(self) -> None:
        text = self.adresse.text().strip()
        ziel = Path(text).expanduser() if text else None
        if ziel is not None and ziel.is_dir():
            self.set_ordner(ziel)
            return
        if text:
            self.hinweis.emit(f"Kein Ordner: {text}")
        self.adresse.setText(str(self._ordner) if self._ordner else "")

    def _aktiviert(self, item, _spalte) -> None:
        ziel = item.data(0, NAV_ROLE)
        if ziel:
            self.set_ordner(ziel)
            return
        pfad = item.data(0, PFAD_ROLE)
        if pfad:
            self.senden.emit([pfad])

    def aktualisieren(self) -> None:
        self.baum.clear()
        if not self._ordner or not self._ordner.is_dir():
            return
        if self._ordner.parent != self._ordner:
            auf = QTreeWidgetItem(self.baum, ["..", ""])
            auf.setData(0, NAV_ROLE, str(self._ordner.parent))
            auf.setIcon(0, icon("folder"))
        try:
            eintraege = sorted(self._ordner.iterdir(), key=lambda p: (not p.is_dir(), p.name.lower()))
        except OSError as e:
            self.hinweis.emit(f"Ordner nicht lesbar: {self._ordner} ({e.strerror})")
            return
        for p in eintraege:
            if p.is_dir():
                it = QTreeWidgetItem(self.baum, [p.name + "/", ""])
                it.setData(0, NAV_ROLE, str(p))
                it.setIcon(0, icon("folder"))
            else:
                try:
                    gr = str(p.stat().st_size)
                except OSError:
                    gr = "?"
                it = QTreeWidgetItem(self.baum, [p.name, gr])
                it.setData(0, PFAD_ROLE, str(p))
                it.setTextAlignment(1, Qt.AlignRight | Qt.AlignVCenter)

    def dateien(self) -> List[str]:
        """Die ausgewaehlten DATEIEN (Ordner fallen weg)."""
        return self.baum.pfade()

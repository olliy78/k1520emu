"""Linke Hälfte: der Inhalt der Diskette.

Beide Seiten einer UDOS-Diskette stehen in **einer** Liste, nach Seite gruppiert —
es wird nicht umgeschaltet (doc/design/13_k1520disktool.md §9.1).  Bei einer
CP/M-Diskette entfällt die Gruppierung ersatzlos.

Die Ansicht hält **keinen** eigenen Verzeichnisstand: `set_disk()` bekommt bei
jedem Aufruf die frisch gelesene Liste (§9.3).

Eine Ausnahme: enthält eine GEÖFFNETE Diskette keine Dateien — leer formatiert,
oder mit einem Format geöffnet, das nicht zu ihr passt —, steht das im leeren
Feld („Keine Dateien gefunden").  Ohne diesen Satz sieht die Hälfte genauso aus
wie vor dem Öffnen, und der Anwender weiss nicht, ob er auf etwas wartet.

Sie trägt seit dem Umbau §20 auch **keine Meldungen** mehr: Pfad und Format stehen
im Kopfbereich des Fensters, Auffälligkeiten im Meldungsstreifen, die Belegung
rechts in der Statuszeile.  Hier ist nur noch die Liste.
"""

from __future__ import annotations

from typing import List, Optional

from PySide6.QtCore import Qt, QMimeData, Signal
from PySide6.QtGui import QDrag, QPainter, QPalette
from PySide6.QtWidgets import (
    QAbstractItemView, QHeaderView, QLabel, QMenu, QTreeWidget, QTreeWidgetItem,
    QVBoxLayout, QWidget,
)

from app.disktool.ui.schreibsperre import ist_platte

#: Eigener MIME-Typ fuer das Ziehen von der Diskette in einen Ordner.
#: Endungen, die das DiskTool als Abbild oeffnet (Gross-/Kleinschreibung egal).
# `.img.gz`: P8000-Plattenabbilder liegen als `.img.gz` vor (Entwurf 37).
ABBILD_ENDUNGEN = (".hfe", ".dmk", ".img", ".cqm", ".td0", ".imd", ".img.gz")

DISK_MIME = "application/x-k1520-diskfiles"

#: Rolle, unter der die eindeutige Bezeichnung („Side1/NAME") am Eintrag haengt.
REF_ROLE = Qt.UserRole + 1
VOLUME_ROLE = Qt.UserRole + 2
#: Gesetzt (True) an Ordnern einer Platte — dort kann man etwas hineinlegen.
DIR_ROLE = Qt.UserRole + 3

#: Spaltenkoepfe der Diskettenansicht und der Plattenansicht (UNIX-Angaben).
DISK_SPALTEN = ["Name", "Typ", "Größe", "Eigensch.", "Datum"]
PLATTE_SPALTEN = ["Name", "Art", "Größe", "Rechte", "uid", "gid", "Geändert"]


class _Tree(QTreeWidget):
    """Baum mit Ziehen (zum Ordner) und Ablegen (Dateien aus dem Dateimanager)."""

    files_dropped = Signal(list, int)   # (Pfade, Ziel-Seite)
    image_dropped = Signal(str)         # Abbilddatei von aussen -> oeffnen
    abgelehnt = Signal(str)             # Grund (Statuszeile)

    #: Satz fuer die leere Liste — gesetzt von :meth:`DiskView.set_disk`.
    leer_text = ""
    #: Ordner einer Platte (Referenz), auf den zuletzt abgelegt wurde ("" = auf keinen).
    drop_ziel = ""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAcceptDrops(True)
        self.setDragEnabled(True)
        self.setDragDropMode(QAbstractItemView.DragDrop)
        self.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.setRootIsDecorated(True)
        self.setUniformRowHeights(True)
        self.setAlternatingRowColors(True)
        self.setSortingEnabled(False)

    # ── Der leere Fall sagt, dass er leer IST ───────────────────────────────

    def paintEvent(self, event):  # noqa: N802
        """Den Satz mittig ins leere Feld schreiben (wie Qts eigene Ansichten).

        Kein Platzhalter-Eintrag: der liesse sich auswählen, ziehen und zählen —
        ein Text im Hintergrund kann das alles nicht.
        """
        super().paintEvent(event)
        if not self.leer_text:
            return
        maler = QPainter(self.viewport())
        maler.setPen(self.palette().color(QPalette.Disabled, QPalette.Text))
        maler.drawText(self.viewport().rect(), Qt.AlignCenter, self.leer_text)

    # ── Ziehen: die ausgewaehlten Dateien als Referenzliste ──────────────────

    def startDrag(self, actions):  # noqa: N802 (Qt-Namensschema)
        refs = [r for r in (i.data(0, REF_ROLE) for i in self.selectedItems()) if r]
        if not refs:
            return
        mime = QMimeData()
        mime.setData(DISK_MIME, "\n".join(refs).encode("utf-8"))
        drag = QDrag(self)
        drag.setMimeData(mime)
        drag.exec(Qt.CopyAction)

    # ── Ablegen: Dateien aus dem Dateimanager ───────────────────────────────

    def _urteil(self, mime):
        """Was wuerde ein Ablegen von `mime` hier bewirken?

        Rueckgabe ``(art, pfade, grund)``; ``art`` ist ``"abbild"``, ``"dateien"``
        oder ``None`` (abgelehnt, `grund` sagt warum).  Regel: eine Datei mit
        Abbild-Endung, die von AUSSEN faellt, wird GEOEFFNET (nicht auf die
        Diskette geschrieben); es darf dann aber nur diese eine Datei sein.
        """
        if not mime.hasUrls():
            return None, [], ""
        urls = mime.urls()
        pfade = [u.toLocalFile() for u in urls if u.isLocalFile()]
        if not pfade or len(pfade) != len(urls):
            return None, [], "Nur lokale Dateien lassen sich ablegen."
        abbilder = [p for p in pfade if p.lower().endswith(ABBILD_ENDUNGEN)]
        if abbilder:
            if len(pfade) != 1:
                return None, [], ("Es lässt sich nur ein Abbild auf einmal öffnen "
                                  "(und nicht zusammen mit anderen Dateien).")
            return "abbild", pfade, ""
        return "dateien", pfade, ""

    def dragEnterEvent(self, event):  # noqa: N802
        art, _, grund = self._urteil(event.mimeData())
        if art:
            event.acceptProposedAction()
        else:
            event.ignore()
            if grund:
                self.abgelehnt.emit(grund)

    def dragMoveEvent(self, event):  # noqa: N802
        self.dragEnterEvent(event)

    def dropEvent(self, event):  # noqa: N802
        art, pfade, grund = self._urteil(event.mimeData())
        if art is None:
            event.ignore()
            if grund:
                self.abgelehnt.emit(grund)
            return
        if art == "abbild":
            event.acceptProposedAction()
            self.image_dropped.emit(pfade[0])
            return
        # Auf welche Seite wurde abgelegt?  Die Gruppe unter dem Mauszeiger
        # entscheidet; ohne Treffer die erste Seite.
        ziel = 0
        item = self.itemAt(event.position().toPoint())
        # Bei einer Platte zaehlt der Ordner unter dem Mauszeiger (oder der naechste
        # darueber), nicht nur die Partition.
        self.drop_ziel = ""
        probe = item
        while probe is not None:
            if probe.data(0, DIR_ROLE):
                self.drop_ziel = probe.data(0, REF_ROLE) or ""
                break
            probe = probe.parent()
        while item is not None:
            v = item.data(0, VOLUME_ROLE)
            if v is not None:
                ziel = int(v)
                break
            item = item.parent()
        self.files_dropped.emit(pfade, ziel)
        event.acceptProposedAction()


class DiskView(QWidget):
    """Diskettenseite des Fensters: Überschrift und Liste."""

    files_dropped = Signal(list, int)
    image_dropped = Signal(str)
    abgelehnt = Signal(str)
    #: Rechtsklick → „Eigenschaften…" bzw. Doppelklick auf eine Datei.
    properties_requested = Signal(str)     # Referenz („Side1/NAME")
    extract_requested = Signal(list)       # Referenzen
    erase_requested = Signal(list)         # Referenzen

    def __init__(self, parent=None):
        super().__init__(parent)
        self._aktionen = []

        self.titel = QLabel("Diskette")
        schrift = self.titel.font()
        schrift.setBold(True)
        self.titel.setFont(schrift)

        self.tree = _Tree()
        self.tree.setHeaderLabels(DISK_SPALTEN)
        # Der Name nimmt, was uebrig ist; die vier schmalen Spalten richten sich
        # nach ihrem Inhalt — sonst wird das Datum abgeschnitten und die Liste
        # bekommt einen waagerechten Rollbalken, obwohl Platz da waere.
        self._spaltenbreiten()
        self.tree.setTextElideMode(Qt.ElideMiddle)
        self.tree.files_dropped.connect(self.files_dropped)
        self.tree.image_dropped.connect(self.image_dropped)
        self.tree.abgelehnt.connect(self.abgelehnt)
        self.tree.setContextMenuPolicy(Qt.CustomContextMenu)
        self.tree.customContextMenuRequested.connect(self._kontextmenue)
        self.tree.itemDoubleClicked.connect(self._doppelklick)

        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(2)
        lay.addWidget(self.titel)
        lay.addWidget(self.tree, 1)

    def _spaltenbreiten(self) -> None:
        kopfzeile = self.tree.header()
        kopfzeile.setSectionResizeMode(0, QHeaderView.Stretch)
        for spalte in range(1, self.tree.columnCount()):
            kopfzeile.setSectionResizeMode(spalte, QHeaderView.ResizeToContents)

    # ── Inhalt setzen ───────────────────────────────────────────────────────

    def setze_aktionen(self, *aktionen) -> None:
        """Die Aktionen des Kontextmenüs — dieselben wie in Menü und Leiste.

        ``None`` steht für einen Trenner.
        """
        self._aktionen = list(aktionen)

    def clear(self) -> None:
        self.tree.clear()
        self.tree.leer_text = ""     # ohne Diskette ist leer keine Auskunft
        self.titel.setText("Diskette")
        self._spalten_setzen(DISK_SPALTEN)

    def _spalten_setzen(self, koepfe) -> None:
        if [self.tree.headerItem().text(i)
                for i in range(self.tree.columnCount())] != list(koepfe):
            self.tree.setHeaderLabels(list(koepfe))
            self._spaltenbreiten()

    def set_disk(self, tool, entries) -> None:
        """Baum aus einer geöffneten Diskette füllen.

        Args:
            tool: ``DiskTool`` (oder None)
            entries: die **frisch gelesene** Liste von ``Entry``
        """
        self.tree.clear()
        if tool is None:
            self.clear()
            return

        if ist_platte(tool):
            self._set_platte(tool, entries)
            return
        self._spalten_setzen(DISK_SPALTEN)
        self.titel.setText(f"Diskette — {tool.filesystem}")
        # Leer formatiert oder mit unpassendem Format geoeffnet: beides sieht hier
        # gleich aus, und beides muss beim Namen genannt werden.
        self.tree.leer_text = "" if entries else "Keine Dateien gefunden"

        mehrseitig = tool.volume_count > 1
        gruppen = {}
        if mehrseitig:
            for v in tool.volumes():
                g = QTreeWidgetItem(self.tree, [f"{v.dir}   {v.label}", "", "", "",
                                                f"{v.free // 1024} KB frei"])
                g.setData(0, VOLUME_ROLE, v.index)
                g.setFirstColumnSpanned(False)
                g.setExpanded(True)
                gruppen[v.index] = g

        self._zeilen = {}
        for nr, e in enumerate(entries):
            eltern = gruppen.get(e.volume, self.tree) if mehrseitig else self.tree
            item = QTreeWidgetItem(eltern, self._spalten(e))
            item.setData(0, REF_ROLE, e.ref)
            item.setData(0, VOLUME_ROLE, e.volume)
            if e.damaged:
                item.setText(1, "DEFEKT")
            # Zeile ueber die LAUFENDE NUMMER merken: genau darueber spricht die
            # Bibliothek beim Nachtragen (`load_entry_details(i)`), und Namen sind
            # bei UDOS ueber beide Seiten hinweg nicht eindeutig.
            self._zeilen[nr] = item

    def _set_platte(self, tool, entries) -> None:
        """Plattenbaum: die Partitionen md0… als oberste Ordner, darunter echte Ordner.

        Die Namen des Kerns sind volle Pfade (``md2/etc/passwd``); der Baum wird aus
        ihnen gebaut.  Die Wurzelzeile einer Partition traegt Rolle, Datentraegername
        und freien Platz — wie die Gruppenzeile einer zweiseitigen Diskette.
        """
        self._spalten_setzen(PLATTE_SPALTEN)
        self.titel.setText("Platte — WEGA (P8000)")
        self.tree.leer_text = "" if entries else "Keine Dateien gefunden"
        volumes = {v.dir: v for v in tool.volumes()}
        self._zeilen = {}
        knoten = {}
        # Eltern vor Kindern: nach Pfadteilen sortieren (die Reihenfolge des Kerns
        # ist nicht zugesichert).
        geordnet = sorted(enumerate(entries),
                          key=lambda p: [t.casefold() for t in p[1].name.split("/")])
        for nr, e in geordnet:
            teile = e.name.split("/")
            eltern = knoten.get("/".join(teile[:-1])) if len(teile) > 1 else None
            spalten = self._platte_spalten(e, teile[-1], volumes.get(e.name), tool)
            item = QTreeWidgetItem(eltern if eltern is not None else self.tree, spalten)
            item.setData(0, REF_ROLE, e.ref)
            item.setData(0, VOLUME_ROLE, e.volume)
            if e.type == "d":
                item.setData(0, DIR_ROLE, True)
                knoten[e.name] = item
                if len(teile) == 1:
                    item.setExpanded(True)
            if e.damaged:
                item.setText(1, "DEFEKT")
            self._zeilen[nr] = item

    @staticmethod
    def _platte_spalten(e, kurz: str, volume, tool) -> List[str]:
        """Name, Art, Groesse, Rechte, uid, gid, Zeit — wie ``ls -l``."""
        if volume is not None:
            rolle = tool.volume_role(volume.index)
            kopf = f"{kurz}   {rolle} {volume.label}".rstrip()
            return [kopf, e.type, "", e.attrs, str(e.unix_uid), str(e.unix_gid),
                    f"{volume.free // 1024} KB frei"]
        return [kurz, e.type, "" if e.type == "d" else f"{e.size}", e.attrs,
                str(e.unix_uid), str(e.unix_gid), e.date]

    def platte_ziel(self) -> str:
        """Ordner der Platte, in den „Schreiben" legt: der angeklickte Ordner, bei einer
        Datei deren Ordner.  Leer, wenn nichts Passendes ausgewaehlt ist."""
        item = self.tree.currentItem()
        if item is None or not item.isSelected():
            gewaehlt = self.tree.selectedItems()
            item = gewaehlt[0] if len(gewaehlt) == 1 else None
        while item is not None:
            if item.data(0, DIR_ROLE):
                return item.data(0, REF_ROLE) or ""
            item = item.parent()
        return ""

    @staticmethod
    def _spalten(e) -> List[str]:
        """Die fünf Spalten eines Eintrags.

        Solange die Angaben aus dem Kopfsektor fehlen (UDOS an einer physischen
        Diskette), steht dort ein Gedankenstrich — **nicht** „0 Byte": eine Null
        wäre eine Behauptung, der Strich sagt „noch nicht gelesen".
        """
        if not getattr(e, "details_loaded", True):
            return [e.name, "…", "…", "", ""]
        return [e.name, e.type, f"{e.size}", e.attrs, e.date]

    def eintrag_auffrischen(self, nr: int, e) -> None:
        """Eine Zeile mit nachgetragenen Angaben neu beschriften."""
        item = getattr(self, "_zeilen", {}).get(nr)
        if item is None:
            return
        for spalte, text in enumerate(self._spalten(e)):
            item.setText(spalte, text)
        if e.damaged:
            item.setText(1, "DEFEKT")

    # ── Kontextmenue ─────────────────────────────────────────────────────────

    def _kontextmenue(self, punkt) -> None:
        """Rechtsklick auf eine Datei — dieselben Aktionen wie im Menü.

        Auf einer Gruppenzeile (der Seite) oder im Leeren gibt es nichts zu tun —
        dann bleibt das Menü weg statt mit toten Einträgen zu erscheinen.
        """
        item = self.tree.itemAt(punkt)
        if item is not None and item.data(0, REF_ROLE):
            # Der angeklickte Eintrag zaehlt, auch wenn die Auswahl woanders steht.
            if item.data(0, REF_ROLE) not in self.selected_refs():
                self.tree.clearSelection()
                item.setSelected(True)
                self.tree.setCurrentItem(item)
        if not self.selected_refs() or not self._aktionen:
            return

        menue = QMenu(self)
        for a in self._aktionen:
            menue.addSeparator() if a is None else menue.addAction(a)
        menue.exec(self.tree.viewport().mapToGlobal(punkt))

    def _doppelklick(self, item, spalte) -> None:
        ref = item.data(0, REF_ROLE)
        if ref:
            self.properties_requested.emit(ref)

    # ── Auswahl ─────────────────────────────────────────────────────────────

    def selected_refs(self) -> List[str]:
        """Eindeutige Bezeichnungen der ausgewählten Dateien (`Side1/NAME`)."""
        return [r for r in (i.data(0, REF_ROLE) for i in self.tree.selectedItems()) if r]

    def current_volume(self) -> int:
        """Seite der aktuellen Auswahl (0, wenn nichts ausgewählt ist)."""
        item = self.tree.currentItem()
        while item is not None:
            v = item.data(0, VOLUME_ROLE)
            if v is not None:
                return int(v)
            item = item.parent()
        return 0

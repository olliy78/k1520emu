"""Ausgabe des Blatts: PDF und System-Druck ueber EINE Zeichenroutine (Entwurf 49 §4.5).

``blatt_zeichnen`` malt einen Zeilenbereich des Blatts auf einen ``QPainter``; PDF
(``QPdfWriter``) und Druck (``QPrinter``) rufen dieselbe Funktion, nur der Massstab
(Painter-Einheiten je Zoll) und der Ausschnitt unterscheiden sich.

**Punkte sind gefuellte Kreise in Mechanikmass, ohne Glaettung der Lage.**  Der Kern liefert
Pixel (x: 1/200 Zoll, y: 1/144 Zoll, Raster aus ``printer_geometry``).  Ein Nadelpunkt belegt
dort eine Zeile und zwei Pixel; zwei in der Zeichenzelle benachbarte Punkte liegen Pixel an Pixel.
Eine Folge gesetzter Pixel wird deshalb in Punkte von ``PUNKT_BREITE_PX`` Pixeln zerlegt und
jeder als Kreis vom Durchmesser ``Breite / xdpi * PUNKT_FAKTOR`` Zoll gemalt.  Beides ist
**Darstellung** (Mechanikmass des Punktes ist ``[?]``, Merkposten 3), kein Rasterwert des Kerns.

**Seitenumbruch** an der Formularlaenge (``formular_zoll``, Vorgabe 12 Zoll wie im Kern); leere
Seiten am Ende entfallen.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterator, List, Tuple

from PySide6.QtCore import QMarginsF, QPointF, QRectF, QSizeF, Qt
from PySide6.QtGui import QBrush, QColor, QPageLayout, QPageSize, QPainter, QPdfWriter
from PySide6.QtPrintSupport import QPrinter

#: Breite eines Nadelpunkts in Blattpixeln (Darstellung).
PUNKT_BREITE_PX = 2
#: Durchmesser des Kreises im Verhaeltnis zur Punktbreite (Darstellung).
PUNKT_FAKTOR = 1.25
#: Aufloesung der PDF-Seite in Punkten je Zoll.
PDF_DPI = 600


@dataclass
class BlattKopie:
    """Ein Abzug des Blatts: ``daten`` = ``hoehe`` Zeilen zu ``(breite+7)//8`` Byte, Bit 7 links."""
    breite: int
    hoehe: int
    daten: bytes
    xdpi: int = 200
    ydpi: int = 144

    @property
    def zb(self) -> int:
        return (self.breite + 7) // 8

    def formular_zeilen(self, zoll: float) -> int:
        return max(1, int(round(zoll * self.ydpi)))


def punkte(kopie: BlattKopie, y0: int, y1: int) -> Iterator[Tuple[float, int, float]]:
    """Alle Nadelpunkte der Zeilen ``y0 <= y < y1`` als ``(x_mitte_px, zeile, breite_px)``."""
    zb = kopie.zb
    y1 = min(y1, kopie.hoehe)
    for y in range(max(0, y0), y1):
        zeile = kopie.daten[y * zb:(y + 1) * zb]
        if not any(zeile):
            continue
        lauf_start = -1
        for i, b in enumerate(zeile):
            if b == 0:
                if lauf_start >= 0:
                    yield from _zerlegen(lauf_start, i * 8, y)
                    lauf_start = -1
                continue
            for bit in range(8):
                gesetzt = b & (0x80 >> bit)
                x = i * 8 + bit
                if gesetzt and lauf_start < 0:
                    lauf_start = x
                elif not gesetzt and lauf_start >= 0:
                    yield from _zerlegen(lauf_start, x, y)
                    lauf_start = -1
        if lauf_start >= 0:
            yield from _zerlegen(lauf_start, zb * 8, y)


def _zerlegen(x0: int, x1: int, y: int):
    """Eine Folge gesetzter Pixel [x0, x1) in Punkte zu je etwa ``PUNKT_BREITE_PX``."""
    laenge = x1 - x0
    n = max(1, int(round(laenge / PUNKT_BREITE_PX)))
    breite = laenge / n
    for k in range(n):
        yield (x0 + (k + 0.5) * breite, y, breite)


def punkte_zaehlen(kopie: BlattKopie, y0: int = 0, y1: int = 1 << 30) -> int:
    return sum(1 for _ in punkte(kopie, y0, y1))


def seiten(kopie: BlattKopie, formular_zeilen: int) -> List[Tuple[int, int]]:
    """Zeilenbereiche der Seiten; leere Seiten (ausser der ersten) entfallen."""
    out = []
    for y0 in range(0, max(1, kopie.hoehe), formular_zeilen):
        y1 = y0 + formular_zeilen
        if not out or punkte_zaehlen(kopie, y0, y1) > 0:
            out.append((y0, y1))
    return out


def blatt_zeichnen(painter: QPainter, kopie: BlattKopie, y0: int, y1: int,
                   einheiten_je_zoll: float, ursprung: QPointF = QPointF(0, 0)) -> int:
    """Die Zeilen [y0, y1) als schwarze Kreise malen; Rueckgabe = Zahl der Punkte.

    ``einheiten_je_zoll``: Painter-Einheiten je Zoll (PDF: ``PDF_DPI``; Drucker: Aufloesung mal
    Einpassfaktor).  ``ursprung``: wo die linke obere Ecke des Ausschnitts landet.
    """
    painter.save()
    painter.setPen(Qt.NoPen)
    painter.setBrush(QBrush(QColor(0, 0, 0)))
    painter.setRenderHint(QPainter.Antialiasing, True)
    n = 0
    sx = einheiten_je_zoll / kopie.xdpi
    sy = einheiten_je_zoll / kopie.ydpi
    for x, y, breite in punkte(kopie, y0, y1):
        d = breite * sx * PUNKT_FAKTOR
        painter.drawEllipse(QPointF(ursprung.x() + x * sx, ursprung.y() + (y - y0 + 0.5) * sy), d / 2, d / 2)
        n += 1
    painter.restore()
    return n


def pdf_speichern(pfad: str, kopie: BlattKopie, formular_zoll: float = 12, titel: str = "K1520 Printer") -> int:
    """Das Blatt als PDF (eine Seite je Formular); Rueckgabe = Zahl der gemalten Punkte."""
    w = QPdfWriter(pfad)
    w.setTitle(titel)
    w.setCreator("K1520 Printer")
    breite_zoll = kopie.breite / float(kopie.xdpi)
    w.setPageSize(QPageSize(QSizeF(breite_zoll, float(formular_zoll)), QPageSize.Inch))
    w.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout.Inch)
    w.setResolution(PDF_DPI)
    painter = QPainter(w)
    gesamt = 0
    fz = kopie.formular_zeilen(formular_zoll)
    for i, (y0, y1) in enumerate(seiten(kopie, fz)):
        if i:
            w.newPage()
        gesamt += blatt_zeichnen(painter, kopie, y0, y1, PDF_DPI)
    painter.end()
    return gesamt


def drucken(printer, kopie: BlattKopie, formular_zoll: float = 12) -> int:
    """Auf einen ``QPrinter``: Blattbreite wird auf die Seitenbreite eingepasst (nie vergroessert),
    der Seitenumbruch liegt an der Formularlaenge, hoechstens aber an der Seitenhoehe."""
    painter = QPainter(printer)
    if not painter.isActive():
        return 0
    res = printer.resolution()
    seite = printer.pageRect(QPrinter.DevicePixel)
    nutz_b, nutz_h = seite.width(), seite.height()
    faktor = min(1.0, nutz_b / (kopie.breite / float(kopie.xdpi) * res))
    einheiten = res * faktor                                       # Druckerpunkte je Blattzoll
    max_zeilen = max(1, int(nutz_h / einheiten * kopie.ydpi))
    fz = min(kopie.formular_zeilen(formular_zoll), max_zeilen)
    gesamt = 0
    for i, (y0, y1) in enumerate(seiten(kopie, fz)):
        if i:
            printer.newPage()
        gesamt += blatt_zeichnen(painter, kopie, y0, y1, einheiten)
    painter.end()
    return gesamt

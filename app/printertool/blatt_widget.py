"""BlattWidget: das Papierblatt des Druckers (Entwurf 49 §4.5) - weisses Blatt, schwarze Punkte.

Kein ``ScreenWidget`` und kein OpenGL: ein einfaches ``QWidget``, das ein Bitbild zeichnet.

**Datenhaltung.**  Das Widget haelt eine eigene Kopie des Blatts (1 Bit je Punkt, Zeilen auf
4 Byte aufgefuellt, damit ``QImage.Format_Mono`` sie unmittelbar nimmt).  Der Kern kennt nur
einen Versionszaehler, keine Aenderungsliste: holt wird deshalb nicht jedes Mal das ganze
Blatt, sondern die Zeilen ab ``min(Kopfzeile vorher, Kopfzeile jetzt) - ZEILEN_VORLAUF``.  Punkte
entstehen nur in der Naehe der Nadeln, und die Nadeln stehen zwischen zwei Abfragen nicht
ausserhalb dieses Bereichs (Papier und Kopf bewegen sich in beschraenkten Schritten); die ganze
Kopie wird nur beim ersten Lesen, nach "Blatt leeren" und bei Breitenaenderung neu geholt.

**Ansicht.**  ``zoom`` = Bildschirmpixel je Blattpixel.  Im Modus "Einpassen" folgt der Zoom der
Fensterbreite.  "Blatt folgen" haelt die Kopfzeile bei 80 % der Hoehe: das Blatt schiebt sich beim
Druck zeilenweise nach oben.  Der Kopfmarker steht an der x-Position des Wagens (Band ueber den
neun Nadeln + Dreieck auf der Schiene am unteren Rand).

**Masse kommen aus dem Kern** (``printer_geometry``/``printer_sheet_size``): kein Rasterwert ist
hier fest verdrahtet.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtCore import QPointF, QRect, QRectF, Qt, Signal
from PySide6.QtGui import QBrush, QColor, QImage, QPainter, QPen, QPolygonF
from PySide6.QtWidgets import QWidget

#: So viele Zeilen oberhalb der Kopfzeile werden bei jeder Abfrage erneut gelesen.
ZEILEN_VORLAUF = 48
#: Wohin die Kopfzeile bei "Blatt folgen" faellt (Anteil der Ansichtshoehe von oben).
FOLGE_ANTEIL = 0.8
#: Hoehe der Schiene (Kopfbahn) am unteren Rand in Bildschirmpixeln.
SCHIENE = 16
#: Nadelband des Kopfes: 9 Nadeln im Abstand von 2 Zeilen (Darstellung, keine Mechanikangabe).
NADELBAND_ZEILEN = 18
ZOOM_MIN, ZOOM_MAX = 0.05, 8.0


class BlattWidget(QWidget):
    """Zeichnet das Blatt; ``aktualisieren(emu)`` zieht Blatt und Kopf aus dem Kern nach."""

    #: Ansicht (Ausschnitt, Zoom, Blattgroesse) hat sich geaendert - die Rollbalken ziehen nach.
    ansicht_geaendert = Signal()
    #: Der Anwender hat von Hand gerollt oder gezoomt: "Blatt folgen" ist damit aus.
    folgen_beendet = Signal()
    #: Kopf bewegt: (x_pixel, y_zeile, lauf) - fuer die Statuszeile.
    kopf_geaendert = Signal(int, int, int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(320, 240)
        self.setFocusPolicy(Qt.NoFocus)
        self.setAutoFillBackground(False)
        self._breite = 0                 # Blattpixel
        self._hoehe = 0                  # gelesene Zeilen
        self._zb = 0                     # Bytes je Zeile (Kern)
        self._stride = 0                 # Bytes je Zeile (hier, auf 4 aufgefuellt)
        self._daten = bytearray()
        self._version = -1
        self._xdpi, self._ydpi = 200, 144
        self._kopf = (0, 0, 0)
        self._kopf_y_vorher = 0
        self._zoom = 0.5
        self._einpassen = True
        self._folgen = True
        self._ox = 0.0                   # Blattkoordinaten der linken oberen Ecke der Ansicht
        self._oy = 0.0
        self._formular_zeilen = 0        # 0 = keine Perforation zeichnen
        self._schnitte = []              # Zeilen der Schnitte (K6316)
        self._kopf_sichtbar = True
        self._farbe_punkt = QColor(0, 0, 0)

    # ── Abfragen ─────────────────────────────────────────────────────────────

    def blatt_breite(self) -> int:
        return self._breite

    def blatt_hoehe(self) -> int:
        return self._hoehe

    def raster(self) -> tuple:
        return (self._xdpi, self._ydpi)

    def kopf(self) -> tuple:
        return self._kopf

    def zoom(self) -> float:
        return self._zoom

    def einpassen(self) -> bool:
        return self._einpassen

    def folgen(self) -> bool:
        return self._folgen

    def ansicht_ursprung(self) -> tuple:
        return (self._ox, self._oy)

    def punkt_gesetzt(self, x: int, y: int) -> bool:
        if not (0 <= x < self._breite and 0 <= y < self._hoehe):
            return False
        return bool(self._daten[y * self._stride + (x >> 3)] & (0x80 >> (x & 7)))

    def punkte_zaehlen(self) -> int:
        return sum(bin(b).count("1") for b in self._daten)

    def kopie(self):
        """Eine Kopie fuer die Ausgabe (PDF/Druck): :class:`app.printertool.ausgabe.BlattKopie`."""
        from app.printertool.ausgabe import BlattKopie
        zeilen = bytearray()
        for y in range(self._hoehe):
            zeilen += self._daten[y * self._stride:y * self._stride + self._zb]
        return BlattKopie(self._breite, self._hoehe, bytes(zeilen), self._xdpi, self._ydpi)

    # ── Einstellung ──────────────────────────────────────────────────────────

    def set_formular_zeilen(self, zeilen: int) -> None:
        zeilen = max(0, int(zeilen))
        if zeilen != self._formular_zeilen:
            self._formular_zeilen = zeilen
            self.update()

    def set_kopf_sichtbar(self, sichtbar: bool) -> None:
        self._kopf_sichtbar = bool(sichtbar)
        self.update()

    def set_folgen(self, an: bool) -> None:
        an = bool(an)
        if an != self._folgen:
            self._folgen = an
        if an:
            self._nachfuehren()
        self.ansicht_geaendert.emit()
        self.update()

    def set_einpassen(self, an: bool) -> None:
        self._einpassen = bool(an)
        self._zoom_einpassen()
        self._nachfuehren()
        self.ansicht_geaendert.emit()
        self.update()

    def set_zoom(self, zoom: float, *, von_hand: bool = False) -> None:
        """Zoom setzen (schaltet "Einpassen" ab)."""
        zoom = min(ZOOM_MAX, max(ZOOM_MIN, float(zoom)))
        self._einpassen = False
        if abs(zoom - self._zoom) > 1e-9:
            self._zoom = zoom
        if von_hand:
            self.folgen_beendet.emit()
        else:
            self._nachfuehren()
        self._begrenzen()
        self.ansicht_geaendert.emit()
        self.update()

    def rollen_nach(self, ox: float, oy: float) -> None:
        self._ox, self._oy = float(ox), float(oy)
        self._begrenzen()
        self.update()

    def ansicht_masse(self) -> tuple:
        """(sichtbare Blattbreite, sichtbare Blatthoehe) in Blattpixeln."""
        z = self._zoom if self._zoom > 0 else 1.0
        return (self.width() / z, max(0, self.height() - SCHIENE) / z)

    # ── Abgleich mit dem Kern ────────────────────────────────────────────────

    def leeren(self) -> None:
        """Lokale Kopie verwerfen (Blatt geleert / neue Maschine)."""
        self._breite = self._hoehe = self._zb = self._stride = 0
        self._daten = bytearray()
        self._version = -1
        self._kopf_y_vorher = 0
        self._schnitte = []
        self._oy = 0.0
        self.ansicht_geaendert.emit()
        self.update()

    def aktualisieren(self, emu) -> bool:
        """Blatt und Kopf aus ``emu`` nachziehen; ``True`` bei sichtbarer Aenderung."""
        geaendert = False
        geom = emu.printer_geometry()
        if geom is not None:
            self._xdpi, self._ydpi = geom[0], geom[1]
        kopf = emu.printer_head()
        if kopf is not None and kopf != self._kopf:
            self._kopf = kopf
            self.kopf_geaendert.emit(*kopf)
            geaendert = True
        ver = emu.printer_sheet_version()
        if ver != self._version:
            groesse = emu.printer_sheet_size()
            if groesse is not None:
                self._blatt_lesen(emu, groesse, kopf[1] if kopf else 0)
                self._schnitte = emu.printer_sheet_cuts()      # K6316: Schneideinrichtung (sonst leer)
                self._version = ver
                geaendert = True
        if kopf is not None:
            self._kopf_y_vorher = kopf[1]
        if geaendert:
            self._nachfuehren()
            self.ansicht_geaendert.emit()
            self.update()
        return geaendert

    def _blatt_lesen(self, emu, groesse, kopf_y: int) -> None:
        breite, hoehe = groesse
        if breite != self._breite or hoehe < self._hoehe:        # neue Breite / Blatt geleert
            self._breite, self._hoehe = breite, 0
            self._zb = (breite + 7) // 8
            self._stride = (self._zb + 3) & ~3
            self._daten = bytearray()
        ab = min(self._hoehe, max(0, min(self._kopf_y_vorher, kopf_y) - ZEILEN_VORLAUF))
        zeilen = emu.printer_sheet_rows(ab, hoehe - ab)
        n = len(zeilen) // self._zb if self._zb else 0
        if hoehe > self._hoehe:
            self._daten.extend(bytes((hoehe - self._hoehe) * self._stride))
            self._hoehe = hoehe
        zb, stride = self._zb, self._stride
        for i in range(n):
            o = (ab + i) * stride
            self._daten[o:o + zb] = zeilen[i * zb:(i + 1) * zb]

    # ── Ansicht ──────────────────────────────────────────────────────────────

    def _zoom_einpassen(self) -> None:
        if self._einpassen and self._breite > 0:
            self._zoom = min(ZOOM_MAX, max(ZOOM_MIN, self.width() / float(self._breite)))

    def _begrenzen(self) -> None:
        sb, sh = self.ansicht_masse()
        max_ox = max(0.0, self._breite - sb)
        max_oy = max(0.0, self._hoehe + NADELBAND_ZEILEN - sh * 0.2)
        self._ox = min(max(0.0, self._ox), max_ox)
        self._oy = min(max(0.0, self._oy), max_oy)

    def _nachfuehren(self) -> None:
        """Bei "Blatt folgen" Kopfzeile und -spalte in die Ansicht holen."""
        self._zoom_einpassen()
        if not self._folgen or self._breite <= 0:
            self._begrenzen()
            return
        sb, sh = self.ansicht_masse()
        self._oy = max(0.0, self._kopf[1] - FOLGE_ANTEIL * sh)
        x = self._kopf[0]
        if sb < self._breite and not (self._ox + 0.05 * sb <= x <= self._ox + 0.95 * sb):
            self._ox = x - sb / 2.0
        elif sb >= self._breite:
            self._ox = 0.0
        self._begrenzen()

    def resizeEvent(self, ev):
        super().resizeEvent(ev)
        self._nachfuehren()
        self.ansicht_geaendert.emit()

    def wheelEvent(self, ev):
        delta = ev.angleDelta().y()
        if ev.modifiers() & Qt.ControlModifier:
            self.set_zoom(self._zoom * (1.15 if delta > 0 else 1 / 1.15), von_hand=True)
        else:
            self.folgen_beendet.emit()
            self._oy -= (delta / 120.0) * 60.0 / max(self._zoom, 0.01)
            self._begrenzen()
            self.ansicht_geaendert.emit()
            self.update()
        ev.accept()

    # ── Zeichnen ─────────────────────────────────────────────────────────────

    def paintEvent(self, ev):
        p = QPainter(self)
        p.fillRect(self.rect(), QColor(58, 60, 64))                       # Tischplatte
        z = self._zoom
        if self._breite > 0 and z > 0:
            self._blatt_zeichnen(p, z)
        self._schiene_zeichnen(p, z)
        p.end()

    def _blatt_zeichnen(self, p: QPainter, z: float) -> None:
        sh = self.height() - SCHIENE
        # Blatt (weiss): von Zeile 0 bis zur gelesenen Hoehe, sichtbarer Teil
        links = -self._ox * z
        oben = -self._oy * z
        blatt = QRectF(links, oben, self._breite * z, max(self._hoehe, 1) * z)
        sicht = QRectF(0, 0, self.width(), sh)
        p.save()
        p.setClipRect(sicht)
        p.fillRect(blatt.intersected(sicht), QColor(255, 255, 255))
        # Punkte: nur der sichtbare Ausschnitt, ohne Glaettung beim Vergroessern, mit Glaettung beim Verkleinern
        y0 = max(0, int(self._oy))
        y1 = min(self._hoehe, int(self._oy + sh / z) + 2)
        x0 = max(0, int(self._ox))
        x1 = min(self._breite, int(self._ox + self.width() / z) + 2)
        if y1 > y0 and x1 > x0:
            roh = QImage(self._daten, self._breite, self._hoehe, self._stride, QImage.Format_Mono)
            roh.setColorTable([0xFFFFFFFF, 0xFF000000])
            teil = roh.copy(QRect(x0, y0, x1 - x0, y1 - y0))
            del roh                                                    # gibt den Puffer wieder frei
            p.setRenderHint(QPainter.SmoothPixmapTransform, z < 1.0)
            p.drawImage(QRectF(links + x0 * z, oben + y0 * z, (x1 - x0) * z, (y1 - y0) * z), teil)
        # Perforation an den Formulargrenzen
        if self._formular_zeilen > 0:
            p.setPen(QPen(QColor(150, 150, 150), 1, Qt.DashLine))
            k = (int(self._oy) // self._formular_zeilen + 1) * self._formular_zeilen
            while (k - self._oy) * z < sh:
                if k <= self._hoehe:
                    y = oben + k * z
                    p.drawLine(QPointF(max(0.0, links), y), QPointF(min(self.width(), links + self._breite * z), y))
                k += self._formular_zeilen
        # Schnittkanten (K6316): durchgezogen rot, das Papier ist dort getrennt
        if self._schnitte:
            p.setPen(QPen(QColor(200, 40, 40), 1, Qt.SolidLine))
            for k in self._schnitte:
                y = oben + k * z
                if 0 <= y <= self.height():
                    p.drawLine(QPointF(max(0.0, links), y), QPointF(min(self.width(), links + self._breite * z), y))
        # Kopfband ueber den Nadeln
        if self._kopf_sichtbar:
            kx, ky, lauf = self._kopf
            band = QRectF(links + (kx - 3) * z, oben + ky * z, max(6 * z, 6.0), NADELBAND_ZEILEN * z)
            farbe = QColor(220, 60, 40, 150 if lauf else 90)
            p.fillRect(band.intersected(sicht), farbe)
        p.restore()

    def _schiene_zeichnen(self, p: QPainter, z: float) -> None:
        """Schiene am unteren Rand mit Dreieck an der x-Position des Kopfes."""
        y0 = self.height() - SCHIENE
        p.fillRect(QRectF(0, y0, self.width(), SCHIENE), QColor(34, 36, 40))
        if not self._kopf_sichtbar or self._breite <= 0:
            return
        x = (self._kopf[0] - self._ox) * z
        if -8 <= x <= self.width() + 8:
            p.setPen(Qt.NoPen)
            p.setBrush(QBrush(QColor(255, 120, 90) if self._kopf[2] else QColor(200, 200, 200)))
            p.drawPolygon(QPolygonF([QPointF(x, y0 + 1), QPointF(x - 6, y0 + SCHIENE - 2),
                                     QPointF(x + 6, y0 + SCHIENE - 2)]))

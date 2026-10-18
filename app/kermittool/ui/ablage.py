"""Ziehen und Ablegen — die Datenformate und die Hilfen, die aus ihnen Dateilisten machen.

Getrennt vom Widget, damit sich die Auswertung OHNE Drag&Drop-Ereignisse testen laesst
(Qt erzeugt die ``QMimeData`` ohne Fenster, das Fallenlassen selbst nicht).

* Von der Serverseite (links) gezogen wird ``MIME_FERN``: die Namen, einer je Zeile.
* Von der lokalen Seite (rechts) gezogen wird ``MIME_LOKAL`` (Pfade, einer je Zeile) UND
  zusaetzlich ``text/uri-list``, damit sich Dateien auch in den Dateimanager ziehen lassen.
* Angenommen werden auf der Serverseite lokale Dateien (aus der rechten Seite oder von
  aussen, z. B. aus dem Dateimanager); auf der lokalen Seite nur Serverdateien.
"""

from __future__ import annotations

from pathlib import Path
from typing import List

from PySide6.QtCore import QMimeData, QUrl

MIME_FERN = "application/x-k1520kermit-fern"
MIME_LOKAL = "application/x-k1520kermit-lokal"


def fern_mime(namen: List[str]) -> QMimeData:
    """Mime-Daten fuer Dateinamen des Servers."""
    m = QMimeData()
    m.setData(MIME_FERN, "\n".join(namen).encode("utf-8"))
    return m


def lokal_mime(pfade: List[str]) -> QMimeData:
    """Mime-Daten fuer lokale Dateipfade (mit URL-Liste fuer den Dateimanager)."""
    m = QMimeData()
    m.setData(MIME_LOKAL, "\n".join(pfade).encode("utf-8"))
    m.setUrls([QUrl.fromLocalFile(p) for p in pfade])
    return m


def fern_namen_aus(mime: QMimeData) -> List[str]:
    """Die Servernamen aus einem Zug von der linken Seite (leer, wenn es keiner war)."""
    if not mime.hasFormat(MIME_FERN):
        return []
    roh = bytes(mime.data(MIME_FERN)).decode("utf-8", errors="replace")
    return [z for z in roh.split("\n") if z]


def lokale_dateien_aus(mime: QMimeData) -> List[str]:
    """Die lokalen DATEIEN aus einem Zug (rechte Seite oder von aussen).

    Ordner und nicht vorhandene Pfade fallen weg — Kermit sendet Dateien, und ein
    Ordner liesse sich nur rekursiv uebertragen, was das Protokoll nicht kennt.
    """
    pfade: List[str] = []
    if mime.hasFormat(MIME_LOKAL):
        roh = bytes(mime.data(MIME_LOKAL)).decode("utf-8", errors="replace")
        pfade = [z for z in roh.split("\n") if z]
    elif mime.hasUrls():
        pfade = [u.toLocalFile() for u in mime.urls() if u.isLocalFile()]
    return [p for p in pfade if Path(p).is_file()]

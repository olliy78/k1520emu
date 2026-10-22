"""Uhrenkarte (RTC) des P8000 in der Oberflaeche (Entwurf 37, AP R3).

Hier stehen nur Namen, Texte und die Darstellung der Zeit; GERECHNET wird im Kern
(Startregel, Jahresversatz, Maschinenzeit -- ``k1520_rtc_*``).  Das Modul importiert
kein Qt (``app/profil.py`` braucht es vor PySide6).

Zeitzone (R4b): die Karte laeuft IMMER in UTC (``rtc_utc=1``); WEGA liest sie als GMT und
rechnet mit seiner ``TZ`` selbst in Ortszeit um.  Die Oberflaeche zeigt deshalb den UTC-Wert der
Karte in der Ortszeit des Wirts (``ortszeit()``), also das, was ``date`` unter WEGA zeigt.

Kodierung der Zeiten (wie die C-ABI): Kalenderzeit als Unix-Sekunden, als waere die
Wanduhr UTC.  ``rtc_zeit()``/``rtc_startzeit()`` liefern GASTSICHTIG (mit Jahresversatz),
``rtc_stellen()``/``rtc_stellen_datei()`` nehmen die Zeit im Jahr des Wirts (unversetzt).
"""

from __future__ import annotations

import calendar
import datetime
import os
import time
from typing import Tuple

from app import instanz, paths

#: Schluessel der beiden Hardwarefelder (``general.rtc`` / ``general.rtc_versatz``).
SCHLUESSEL_TYP = "rtc"
SCHLUESSEL_VERSATZ = "rtc_versatz"

KEINE = "keine"
U130X = "u130x"
RTC72421 = "rtc72421"

#: ``(Wert, Anzeige)``; der Wert ist zugleich der Kernparameter ``rtc=``.
TYPEN: Tuple[Tuple[str, str], ...] = (
    (KEINE, "keine"),
    (U130X, "Original U130x"),
    (RTC72421, "Epson RTC-72421"),
)
#: ``(Wert, Anzeige)``; der Wert ist zugleich der Kernparameter ``rtc_jahresversatz=``.
VERSAETZE: Tuple[Tuple[str, str], ...] = (
    ("0", "Kein Zeitversatz"),
    ("-28", "−28 Jahre (gleicher Wochentag)"),
    ("-40", "−40 Jahre"),
)
#: Vorgabe der Auslieferung (nicht die des Kerns, der ohne Angabe keine Karte steckt).
VORGABE_TYP = U130X
VORGABE_VERSATZ = "-40"

TIPP_TYP = (
    "Uhrenkarte des 16-Bit-Teils.  Original U130x: die Karte, die der WEGA-Kern 3.2 erkennt.  "
    "Epson RTC-72421: Nachbau 2009-2016; braucht den Treiber sa.timer / timer 1.1 von pofo.de.  "
    "Ein Wechsel erzeugt die Maschine neu (wie ein Kaltstart).")
TIPP_VERSATZ = (
    "Die Uhr zeigt dem Gast die Wirtszeit um diese Jahre zurück (nur was der Gast liest und schreibt "
    "wird gerechnet).  Alle drei Werte sind für beide Karten wählbar.\n"
    "Ohne Versatz lässt sich die U130x unter WEGA nicht mit sa.timer stellen (der Treiber nimmt nur "
    "1980–2019 an).  −28 Jahre trifft im Wirtsjahr 2028 den Jahr-2000-Fehler der Stellroutinen von "
    "sa.timer (beide Karten).  Harte Grenze ist 2038 (WEGA rechnet mit 32 Bit): ohne Versatz bis 2037, "
    "mit −28 bis 2065, mit −40 bis 2077.  −28 erhält Wochentag und Schaltjahrmuster, −40 nur das "
    "Schaltjahrmuster.  Nur mit gesteckter Karte; „Uhr nach Wirtszeit stellen“ ist davon nicht betroffen.")


def zustand_pfad(programm: str) -> str:
    """Batteriedatei der Uhr (``rtc_<programm>[-<instanz>].txt`` im Konfigurationsordner).

    Je Instanz (``--instance``) eine eigene Datei: zwei Rechner teilten sich sonst eine Batterie.
    """
    n = instanz.name()
    stamm = f"rtc_{programm}" + (f"-{n}" if n else "")
    return os.path.join(str(paths.config_dir()), stamm + ".txt")


def versatz_jahre(wert) -> int:
    """Der Versatz als Zahl; Unbekanntes = 0."""
    try:
        return int(str(wert).strip())
    except ValueError:
        return 0


#: Die Oberflaeche betreibt die Karte immer in UTC (Kernparameter ``rtc_utc``, ``k1520_rtc_startzeit``).
UTC = True


def wirtszeit_kodiert() -> int:
    """Jetzt, UTC in der Kodierung der C-ABI (Unix-Sekunden)."""
    return int(time.time())


def ortszeit(t: int, versatz: int = 0) -> int:
    """Kartenwert *t* (gastsichtig, UTC kodiert) in der Ortszeit des Wirts, wieder so kodiert.

    Der Zonenversatz wird am WIRTSJAHR-Zeitpunkt bestimmt (``t`` minus Jahresversatz), nicht
    am Gastjahr: 1990 kennt die Zeitzonenrechnung des Wirts (Sommerzeitregeln) womoeglich anders.
    Bei Fehlern (Wertebereich) bleibt *t* unveraendert.
    """
    if t is None or t < 0:
        return t
    try:
        wirt = um_jahre(int(t), -int(versatz))
        return int(t) + int(time.localtime(wirt).tm_gmtoff or 0)
    except (OverflowError, OSError, ValueError):
        return t


def anzeige(t: int) -> str:
    """``TT.MM.JJJJ hh:mm`` -- ``unbekannt`` bei ``t < 0`` (keine Karte)."""
    if t is None or t < 0:
        return "unbekannt"
    try:
        return time.strftime("%d.%m.%Y %H:%M", time.gmtime(int(t)))
    except (OverflowError, OSError, ValueError):
        return "unbekannt"


def um_jahre(t: int, jahre: int) -> int:
    """*t* um *jahre* verschoben (29.2. wird 28.2.).  Nur fuer den Vergleich vor dem Stellen."""
    d = datetime.datetime.fromtimestamp(t, datetime.timezone.utc)
    try:
        d = d.replace(year=d.year + jahre)
    except ValueError:
        d = d.replace(year=d.year + jahre, day=28)
    return calendar.timegm(d.timetuple())

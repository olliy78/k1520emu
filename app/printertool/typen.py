"""Druckertypen und ihr Bedienfeld als DATEN (Entwurf 49 §4.5).

Das Bedienfeld (`bedienfeld.py`) wird aus einer :class:`Typbeschreibung` gebaut, nicht
von Hand: ein weiterer Drucker (K6316 mit seinem Drehschalter, den DIL-Schaltern und
den Lampen H1-H5) braucht eine neue Beschreibung hier und keine Aenderung am Fenster.

Das Modul importiert kein Qt und keinen Kern: `--help` und die Tests lesen es vorher.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Tuple


@dataclass(frozen=True)
class Taste:
    """Eine Taste oder ein Schalter des Bedienfelds.

    ``art``: ``"taster"`` (gedrueckt halten, Taste des Kerns), ``"rastend"`` (Schalter des Kerns).
    ``kern_id``: ``DRUCKER_TASTE_*`` bzw. ``DRUCKER_SW_*`` der Bindung.
    ``hinweis``: Zeile unter dem Bedienfeld, wenn das Gastverhalten etwas Unerwartetes hat.
    """
    name: str
    text: str
    tipp: str
    art: str
    kern_id: int
    hinweis: str = ""


@dataclass(frozen=True)
class Lampe:
    """Eine Lampe: ``kern_id`` = ``DRUCKER_LAMPE_*`` oder ``None`` (= Netzlampe, die Oberflaeche kennt sie)."""
    name: str
    farbe: str
    tipp: str
    kern_id: Optional[int]


@dataclass(frozen=True)
class Typbeschreibung:
    schluessel: str            # Eintrag in der Konfiguration (general.model)
    titel: str                 # Anzeige im Auswahlfeld und in der Statuszeile
    kern_maschine: str         # K1520Emulator(machine=...)
    tasten: Tuple[Taste, ...]
    lampen: Tuple[Lampe, ...]
    #: Schnittstellen in der Reihenfolge des Kerns (Hub-Index) -> Anzeigename.
    schnittstellen: Tuple[str, ...] = ()
    #: Nenntakt in Hz (Statuszeile, Tempo).
    nenntakt_hz: int = 2_457_600


SD1157_264 = Typbeschreibung(
    schluessel="sd1157",
    titel="SD 1157/264",
    kern_maschine="sd1157",
    tasten=(
        Taste("online", "ONLINE", "Online/Offline.  Offline druckt der Drucker nicht und nimmt PV1/PV2/TEST an.",
              "rastend", 0,
              "Die Firmware liest ONLINE für TEST nur beim Einschalten: TEST wirkt, wenn der Drucker "
              "OFFLINE eingeschaltet wurde (ONLINE aus, dann Neustart)."),
        Taste("pv1", "PV1", "Papiervorschub: kurz = eine Zeile, lange gehalten (ab etwa 0,85 s) = "
                            "Formularvorschub.  Nur offline wirksam.", "taster", 1),
        Taste("pv2", "PV2", "Papiervorschub Bahn 2.  Der SD 1157/264 hat nur eine Bahn; die Taste "
                            "gehört zum Zweibahntyp /267.", "taster", 2),
        Taste("test", "TEST", "Selbsttest.  Wirkt nur im Offline-Zustand, der beim Einschalten bestand "
                              "(ONLINE aus, dann Neustart) - Gastverhalten der Firmware, kein Emulatorfehler.",
              "taster", 3),
        Taste("kopf", "Kopf an", "Druckkopf dauernd angeschwenkt (sonst nur während des Drucks).",
              "rastend", 1),
        Taste("lpi6", "6 lpi", "Zeilenabstand: an = 6 lpi, aus = 8 lpi.  Die Firmware liest die Taste nur "
                               "beim Einschalten: Änderung wirkt nach dem Neustart.", "rastend", 2),
        Taste("papier", "Papier da", "Papier eingelegt.  Aus = Papierende (/PE).", "rastend", 3),
    ),
    lampen=(
        Lampe("Netz", "#37d34a", "Netz: der Drucker ist eingeschaltet.", None),
        Lampe("ONLINE", "#37d34a", "ONLINE: der Drucker ist bereit und nimmt Daten an.", 0),
        Lampe("FEHLER", "#e53935", "FEHLER: Havarie (Farbband) oder Zeitüberschreitung beim Papiervorschub.", 1),
    ),
    schnittstellen=("IFSS", "V.24"),
)

#: Die waehlbaren Typen (in der Reihenfolge des Auswahlfelds).
TYPEN = (SD1157_264,)

#: Typen, die der Entwurf kennt, der Kern aber noch nicht faehrt: (Schluessel, Anzeige, Begruendung).
GESPERRT = (
    ("sd1157-267", "SD 1157/267", "Die zweite Bahn (Zweibahndrucker) folgt."),
    ("k6316-ifss", "K6316 (IFSS)", "In Arbeit: die Steuerung des K6316 ist noch nicht gebaut."),
    ("k6316-v24", "K6316 (V.24)", "In Arbeit: die Steuerung des K6316 ist noch nicht gebaut."),
    ("k6313", "K6313", "Für den K6313 liegt kein EPROM-Abzug vor."),
    ("sd1152-257", "SD 1152/257 (Typenrad)", "In Vorbereitung."),
)

VORGABE = SD1157_264


def typ(schluessel: str) -> Typbeschreibung:
    """Die Beschreibung zum Schluessel; ein unbekannter oder gesperrter wird zur Vorgabe."""
    for t in TYPEN:
        if t.schluessel == schluessel:
            return t
    return VORGABE


#: ROM-Wahl des SD 1157: (Schluessel, Anzeige, Erklaerung).  Merkposten Nr. 17.
ROM_WAHL = (
    ("repariert", "repariert (Vorgabe)",
     "Der Original-Abzug leert den Eingabepuffer nie (Byte 0D34H); die reparierte Fassung ist nur dort "
     "um ein Byte geändert.  Mit ihr arbeitet der Drucker an der Schnittstelle zuverlässig."),
    ("original", "Original-Abzug",
     "Der unveränderte EPROM-Abzug.  Der Selbsttest läuft; über die Schnittstelle bleibt der "
     "Eingabepuffer nach dem ersten Zeichen voll (vermutlich ein Lesefehler des Abzugs)."),
)

SCHNITTSTELLEN = (("ifss", "IFSS"), ("v24", "V.24"))


def konfig_text(schnittstelle: str = "ifss", rom: str = "repariert") -> str:
    """Konfigurationstext fuer ``k1520_create_printer`` (Unbekanntes -> Vorgabe)."""
    schnittstelle = schnittstelle if schnittstelle in ("ifss", "v24") else "ifss"
    rom = rom if rom in ("repariert", "original") else "repariert"
    return f"schnittstelle={schnittstelle},rom={rom}"

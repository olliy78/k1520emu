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
    #: Drehschalter Betriebsart (K6316): Kern-Schalter und Stellungen (Wert, Text); leer = keiner.
    betriebsart_id: Optional[int] = None
    betriebsarten: Tuple[Tuple[int, str], ...] = ()
    #: DIL-Schalter (K6316): als rastende Schalter, im Bedienfeld in eigener Gruppe.
    dil: Tuple[Taste, ...] = ()
    #: False: das Geraet hat weder ROM-Wahl noch Schnittstellenwahl (K6316: das Modul folgt dem Typ).
    hat_rom_wahl: bool = True
    #: Schnittstelle, die der Typ festlegt (``None`` = wahlbar).
    schnittstelle_fest: Optional[str] = None
    #: Kern-Schalter fuer "ONLINE" (SD 1157: rastender Schalter); ``None`` = nur ueber die Taste (K6316 S1).
    hat_online_schalter: bool = True


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
                            "ohne Wirkung (die zweite Bahn des Typs /267 wird nicht nachgebildet).", "taster", 2),
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

#: DIL-Schalter S11..S26 des K6316 mit dem, was belegt ist: Handbuch 5.8.2 (S13, S14), V.24-ROM (liest die Zeilen AM2/AM3/AM4
#: der Diodenmatrix, hardware.md §5) und der Adapterplatte (Bestueckungsvarianten).  Alles andere ist als [?] gekennzeichnet.
_DIL_TEXT = {
    11: "S11 (Block S11-S14): wirkt nur, wenn der ganze Block S11-S14 bestueckt ist [?].",
    12: "S12 (Block S11-S14) [?]; am V.24-ROM an der Zeile AM4 gelesen.",
    13: "S13: Zeichengeneratorumschaltung (nur V.24/IFSP, Handbuch 5.8.2).  Aus: Haupt- und Nebenzeichensatz nach Befehlsbeschreibung; "
        "Ein: Codepositionen 20H-5FH aus dem Hauptzeichensatz, 60H-7EH aus dem Nebenzeichensatz.",
    14: "S14 (Handbuch 5.8.2): Ein = Endlospapiertransport in jeder Druckposition (Leitklappen schwenken nicht ab); "
        "Aus = nur wenn der Wagen am linken oder rechten Rand steht.",
    15: "S15 [?], Zeile AM4.", 16: "S16 [?], Zeile AM4.",
    17: "S17 (V.24-ROM 197FH): Ein = XON/XOFF-Protokoll (DC1/DC3), Aus = DTR-Protokoll; ohne S17 sendet das V.24-ROM kein DC1.",
    18: "S18 (V.24-ROM 0137H) [?]: Duplex/Halbduplex oder Paritaet, Zeile AM3.",
    19: "S19 (V.24-ROM 010CH) [?], Zeile AM3.", 20: "S20 (V.24-ROM 0107H) [?], Zeile AM3.", 21: "S21 (V.24-ROM 00F6H) [?], Zeile AM3.",
    22: "S22 (V.24-ROM 00FEH) [?], Zeile AM2.",
    23: "S23 (V.24-ROM 0120H) [?], Zeile AM2.",
    24: "S24: Bit 2 des Baudcodes (V.24): Zeitkonstante 2^Code; 000 = 9600 Bd ... 110 = 150 Bd, 111 = 200 Bd.",
    25: "S25: Bit 1 des Baudcodes (V.24).", 26: "S26: Bit 0 des Baudcodes (V.24).",
}
# Kern-Schalter 16..31 = S11..S26 (k1520_api.h K1520_PRINTER_SW_DIL_BASIS)
_DIL_TASTEN = tuple(Taste(f"s{n}", f"S{n}", _DIL_TEXT[n], "rastend", 16 + n - 11) for n in range(11, 27))

_K6316_TASTEN = (
    Taste("s1", "S1 ON/OFF-LINE", "Taste 1: schaltet zwischen ON-LINE und OFF-LINE um (Lampe H5).  Nur OFF-LINE sind die anderen "
          "Tasten wirksam.  Beim Netz-Ein gehalten: Lichtschrankentest.", "taster", 11),
    Taste("s2", "S2 LF \u25B2", "Taste 2: Papiervorschub vorwaerts (kurz: 1/6 Zoll, gehalten: Dauervorschub).  "
          "Beim Netz-Ein gehalten: Hexadezimaldarstellung.", "taster", 12),
    Taste("s3", "S3 LF \u25BC", "Taste 3: Papiervorschub rueckwaerts (1/12 Zoll; in der Betriebsart Aufwickeln gesperrt).  "
          "Beim Netz-Ein gehalten: Test der Vorsteckeinrichtung.", "taster", 13),
    Taste("s4", "S4 BA", "Taste 4: Belegauswurf.  Mit S5: Schneidvorgang (nur in der Betriebsart Schneiden).", "taster",
          14),
    Taste("s5", "S5 \u2702", "Taste 5: Schneiden (zusammen mit S4).  Beim Netz-Ein gehalten, bis der Ausdruck beginnt: Selbsttest "
          "mit Druck; mit S4: Selbsttest ohne Druck.", "taster", 15),
    Taste("kopf", "Kopf angeschwenkt", "Einstellhebel Druckkopf.  Abgeschwenkt: die Synchronisation beim Netz-Ein unterbleibt, "
          "H2 blinkt (Bedienfehler).", "rastend", 1),
    Taste("haube", "Haube zu", "Vordere Abdeckhaube geschlossen.  Offen: keine Synchronisation, H2 blinkt.", "rastend",
          4),
    Taste("papier", "Papier da", "Endlospapier eingelegt.  Aus = Papierende (H3).", "rastend", 3),
    Taste("beleg_o", "Beleg oben", "Papier vor der oberen Lichtschranke im Vorsteckschacht (FSO).", "rastend",
          6),
    Taste("beleg_u", "Beleg unten", "Papier vor der unteren Lichtschranke (FSU).", "rastend", 7),
)
_K6316_LAMPEN = (
    Lampe("Netz", "#37d34a", "Netz: der Drucker ist eingeschaltet.", None),
    Lampe("TEST", "#e6c229", "H1 TEST: Test- oder Selbsttestfunktion.", 2),
    Lampe("FEHLER", "#e53935", "H2 FEHLER: dauernd = technischer Fehler (Wagen blockiert, Spannung); blinkend = Bedienfehler "
          "(Haube offen, Kopf abgeschwenkt).", 1),
    Lampe("PAPIER", "#e6c229", "H3 PAPIER: Papierende.", 3),
    Lampe("BEREIT", "#37d34a", "H4 BEREIT: die Betriebsspannungen sind in Ordnung.", 4),
    Lampe("ONLINE", "#37d34a", "H5 ON-LINE: der Drucker ist bereit und nimmt Daten an.", 0),
)
_K6316_BETRIEBSARTEN = ((0, "Aufwickeln"), (1, "Schneiden"), (2, "Endlosverarbeitung"))


def _k6316(schluessel: str, titel: str, kern: str, schnittstelle: str, namen: Tuple[str, ...]) -> Typbeschreibung:
    return Typbeschreibung(
        schluessel=schluessel, titel=titel, kern_maschine=kern, tasten=_K6316_TASTEN, lampen=_K6316_LAMPEN,
        schnittstellen=namen, betriebsart_id=5, betriebsarten=_K6316_BETRIEBSARTEN, dil=_DIL_TASTEN,
        hat_rom_wahl=False, schnittstelle_fest=schnittstelle, hat_online_schalter=False)


K6316_IFSS = _k6316("k6316-ifss", "K6316 (IFSS)", "k6316-ifss", "ifss", ("IFSS", "V.24"))
K6316_V24 = _k6316("k6316-v24", "K6316 (V.24)", "k6316-v24", "v24", ("IFSS", "V.24"))

#: Die waehlbaren Typen (in der Reihenfolge des Auswahlfelds).
TYPEN = (SD1157_264, K6316_IFSS, K6316_V24)

#: Typen, die der Entwurf kennt, der Kern aber noch nicht faehrt: (Schluessel, Anzeige, Begruendung).
GESPERRT = (
    ("sd1157-267", "SD 1157/267", "Nicht vorgesehen (zweite Papierbahn wird nicht nachgebildet)."),
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


def konfig_text(schnittstelle: str = "ifss", rom: str = "repariert", beschreibung: Optional[Typbeschreibung] = None,
                betriebsart: str = "endlos", dil: int = 0) -> str:
    """Konfigurationstext fuer ``k1520_create_printer`` (Unbekanntes -> Vorgabe).

    Beim K6316 folgt das Schnittstellenmodul dem Typ; der Kern kennt dort ``betriebsart`` und ``dil``."""
    if beschreibung is not None and beschreibung.betriebsart_id is not None:
        betriebsart = betriebsart if betriebsart in ("aufwickeln", "schneiden", "endlos") else "endlos"
        return f"betriebsart={betriebsart},dil={int(dil) & 0xFFFF}"
    schnittstelle = schnittstelle if schnittstelle in ("ifss", "v24") else "ifss"
    rom = rom if rom in ("repariert", "original") else "repariert"
    return f"schnittstelle={schnittstelle},rom={rom}"

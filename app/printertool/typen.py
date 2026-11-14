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
    #: Vorsteckschacht mit Beleg als eigenem Blatt (K6316, Bahn 1 des Kerns; hardware.md §11).
    hat_vorsteck: bool = False
    #: DIL-Schalter: erster Kern-Schalter, Anzahl, Vorgabemaske (Bit i = i-ter Schalter der Gruppe), Titel der Gruppe.
    dil_basis: int = 16
    dil_anzahl: int = 16
    dil_vorgabe: int = 0
    dil_titel: str = "DIL-Schalter S11 - S26 (Ein = geschlossen)"
    #: Zeile unter dem Bedienfeld mit dem Stand der Nachbildung (K6313: Hardware aus der Firmware abgelesen).
    stand_hinweis: str = ""


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
        hat_rom_wahl=False, schnittstelle_fest=schnittstelle, hat_online_schalter=False, hat_vorsteck=True)


K6316_IFSS = _k6316("k6316-ifss", "K6316 (IFSS)", "k6316-ifss", "ifss", ("IFSS", "V.24"))
K6316_V24 = _k6316("k6316-v24", "K6316 (V.24)", "k6316-v24", "v24", ("IFSS", "V.24"))

# ── K6313 (WP8c, Entwurf 49 §11): CCB ohne E/A-Dekoder, V.24-Kassette, 14 DIL-Paare in der Matrix 4 x 8 ──────────────────────────
# Beschriftung: Handbuch/Servicehandbuch (hardware.md §5.1).  Hinter dem Text steht, wie sicher die ZELLE der Matrix ist:
# [F] = aus der Firmware abgelesen (Auswertung im ROM), [?] = aus dem Muster der Matrix geschlossen, nicht belegt.
_K6313_DIL_TEXT = {
    (5, 1): "5-1 Haubenkontakt: muss OFF bleiben (Service).  Zelle [?].",
    (5, 2): "5-2 nicht benutzt.  Die Firmware verlangt beim Einschalten 5-2 = ON, 5-1 = OFF (Zeile 2/3 der Spalte 3, ROM 018CH) [?: Zelle aus dem Muster].",
    (6, 1): "6-1 Papierendekontakt: OFF wirksam, ON nicht wirksam [F].",
    (6, 2): "6-2 Summer: OFF wirksam, ON nicht wirksam.  Zelle [?]; die Nachbildung hat keinen Summer.",
    (7, 1): "7-1 automatische Zeilenschaltung: ON = LF bei CR.  Zelle [?].",
    (7, 2): "7-2 automatischer Wagenruecklauf: OFF = CR bei LF.  Zelle [?].",
    (8, 1): "8-1 Zeichensatz (Tabelle 1 des Handbuchs) [F].", (8, 2): "8-2 Zeichensatz (Tabelle 1) [F].",
    (9, 1): "9-1 Zeichensatz (Tabelle 1) [F].", (9, 2): "9-2 Zeichensatz (9-2 ON = US ASCII mit Yen) [F].",
    (10, 1): "10-1 Schriftart (Tabelle 2: Pica/Elite/komprimiert/Fett).  Zelle [?].", (10, 2): "10-2 Schriftart (Tabelle 2).  Zelle [?].",
    (11, 1): "11-1 Nulldarstellung: OFF = 0, ON = Ø [F].", (11, 2): "11-2 Zeilenabstand: OFF = 1/6 Zoll, ON = 1/8 Zoll [F].",
    (12, 1): "12-1 Formatlaenge (Tabelle 3: 12/11/6/5,5 Zoll) [F].", (12, 2): "12-2 Formatlaenge (Tabelle 3) [F].",
    (13, 1): "13-1 1 Zoll Papiervorschub ueber die Falzkante: OFF wirksam, ON nicht wirksam [F].",
    (13, 2): "13-2 Steuercodes kompatibel zu EPSON (ON).  Zelle [?].",
    (14, 1): "14-1 nicht benutzt (V.24-Kassette).  Zelle [?].",
    (14, 2): "14-2 Baudrate (mit 15-1, 15-2): 9600 aus aus aus, 4800 aus ein aus, 2400 ein ein aus, 1200 aus aus ein, 600 ein aus ein, 300 aus ein ein, 200 ein aus aus, 150 ein ein ein [F].",
    (15, 1): "15-1 Baudrate (siehe 14-2) [F].", (15, 2): "15-2 Baudrate (siehe 14-2) [F].",
    (16, 1): "16-1 Paritaet: OFF mit, ON ohne [F].", (16, 2): "16-2 Paritaet: OFF ungerade, ON gerade [F].",
    (17, 1): "17-1 Protokoll: OFF DTR, ON XON/XOFF [F].",
    (17, 2): "17-2 DTR: RTS Ein/Aus; XON/XOFF: Duplex/Halbduplex [F].",
    (18, 1): "18-1 Datenbits: OFF 7, ON 8 [F].", (18, 2): "18-2 Stopbits: OFF mindestens 1, ON mindestens 2 [F].",
}
# Kern-Schalter 32..59 = 5-1, 5-2, 6-1 ... 18-2 (k1520_api.h K1520_PRINTER_SW_K6313_DIL_BASIS)
_K6313_DIL = tuple(Taste(f"d{p}_{n}", f"{p}-{n}", _K6313_DIL_TEXT[(p, n)], "rastend", 32 + (p - 5) * 2 + (n - 1))
                   for p in range(5, 19) for n in (1, 2))
#: Vorgabe: 5-2 ON (Normalstellung beim Einschalten [?]) und 13-2 ON (EPSON), Rest OFF.
_K6313_DIL_VORGABE = (1 << 1) | (1 << ((13 - 5) * 2 + 1))

_K6313_BASIS = dict(
    tasten=(
        Taste("t1", "ON/OFF LINE", "Taste ON/OFF LINE: schaltet zwischen ON LINE und OFF LINE um (LED ON LINE).  Zeile 4 der Tastenspalte [?]; "
              "ON/OFF LINE loescht LON (ROM 02B6H) [F].", "taster", 21),
        Taste("t2", "LF \u25B2", "Taste LINE FEED vorwaerts (gehalten: Dauervorschub).  Zeile 2 der Tastenspalte [?].  Beim Netz-Ein gehalten: "
              "Selbsttest mit Kennungszeile (LF vorwaerts beim Netz-Ein gehalten, am ROM belegt).  Zusammen mit FF beim Netz-Ein gehalten: "
              "Hexdump (jedes empfangene Byte als Hexzahl, Handbuch 1.4.5).", "taster", 22),
        Taste("t3", "LF \u25BC", "Taste LINE FEED rueckwaerts (Zeile 1 der Tastenspalte [?]).  Beim Netz-Ein gehalten: Selbsttest ohne Kennungszeile.",
              "taster", 23),
        Taste("t4", "FF", "Taste FORM FEED (Zeile 3 der Tastenspalte [?]).  Beim Netz-Ein zusammen mit LF vorwaerts gehalten: Hexdump.", "taster", 24),
        Taste("papier", "Papier da", "Endlospapier eingelegt.  Aus = Papierende (LED PE; Polaritaet des Papierendekontakts [?]).",
              "rastend", 3),
    ),
    lampen=(
        Lampe("POW", "#37d34a", "POW: der Drucker ist eingeschaltet.", None),
        Lampe("ON LINE", "#e6c229", "ON LINE: der Drucker ist bereit und nimmt Daten an (D104 A6).", 0),
        Lampe("ERR", "#e53935", "ERR: Fehler (D105 A4; Polaritaet [?]).", 1),
        Lampe("PE", "#e6c229", "PE: Papierende.", 3),
    ),
    schnittstellen=("IFSS", "V.24"),
    hat_rom_wahl=False, hat_online_schalter=False,
    dil=_K6313_DIL, dil_basis=32, dil_anzahl=28, dil_vorgabe=_K6313_DIL_VORGABE,
    dil_titel="DIL-Schalter 5-1 bis 18-2 (ON = geschlossen)",
    stand_hinweis="Hardware aus der Firmware erschlossen (Fassung kurz, V.24-Kassette): die Belegung der Tasten- und DIL-Matrix ist am ROM "
                  "abgelesen, nicht an einem Plan; mit [?] gekennzeichnete Angaben sind Annahmen.  Das Druckbild (Text ueber die Schnittstelle, "
                  "Selbsttest, Fett, Breit, Elite, komprimiert, Unterstreichen) stimmt Punkt fuer Punkt mit dem Zeichengenerator im ROM ueberein; Abzuege mit 16 KiB "
                  "(30835007, 30835207) laufen im Speicherplan lang (RAM bei F000H).  Nicht belegt: Schraeg-, Hoch-/Tiefschrift, Doppeldruck.",
)
K6313 = Typbeschreibung(schluessel="k6313", titel="K6313 (V.24)", kern_maschine="k6313", schnittstelle_fest="v24", **_K6313_BASIS)
#: IFSS-Kassette (WP8g, Merkposten Nr. 36): dieselbe Karte und dieselben Abzuege (Stromschleife-Etikett "CL"), die Daten laufen ueber SIO-Kanal A wie am
#: V.24, aber ohne Steuerleitungen.  Es gibt keinen Plan der Kassette: Wirkungsweise der Stromschleife [?].
K6313_IFSS = Typbeschreibung(schluessel="k6313-ifss", titel="K6313 (IFSS)", kern_maschine="k6313-ifss", schnittstelle_fest="ifss",
                             **{**_K6313_BASIS, "stand_hinweis": _K6313_BASIS["stand_hinweis"].replace(
                                 "V.24-Kassette", "IFSS-Kassette (Stromschleife, ohne Plan [?])")})


# ── K6311 (WP8e/WP8f, Entwurf 49 §11): KEIN Schaltplan - Hardware nur aus dem ROM 12128062 erschlossen [?] ───────────────────────────
# Die Karte ist die des K6313 (E/A 7CH-FBH), die Belegung der Bits aber eine andere (Merkposten drucker.md Nr. 34/35): vier Tasten (ON LINE an D104 A2,
# drei weitere an D105 A4-A6), Lampen ON LINE (D105 A2) und ERR (D105 A3), Papierende-Sensor D104 A6; KEINE DIL-Schalter (Baud/Format kommen ueber BCH/BDH).
# Das Druckbild stimmt Punkt fuer Punkt mit dem Zeichengenerator im ROM ueberein; die Zuordnung der Bits ist erschlossen.
_K6311_BASIS = dict(
    tasten=(
        Taste("t1", "ON/OFF LINE", "Taste ON/OFF LINE (D104 A2, aktiv low): schaltet zwischen ON LINE und OFF LINE um (Lampe ON LINE).  "
              "Zuordnung aus dem ROM [?].", "taster", 21),
        Taste("t2", "TEST", "Taste D105 A4: im OFF-LINE-Zustand gedrueckt startet sie den Selbsttest (Kennungszeile und Zeichenumlauf, ROM 0264H); "
              "beim Netz-Ein zusammen mit der Taste PAPIER VOR gehalten: Hexdump-Modus (ROM 00D1H-00DCH).  Beschriftung [?].", "taster", 22),
        Taste("t3", "PAPIER \u25B2", "Taste D105 A5: schiebt das Papier im OFF-LINE-Zustand rueckwaerts, solange sie gehalten wird (ROM 1126H).  Beschriftung [?].",
              "taster", 23),
        Taste("t4", "PAPIER \u25BC", "Taste D105 A6: faehrt den Wagen nach rechts und schiebt das Papier im OFF-LINE-Zustand vorwaerts "
              "(Zeilen zu 1/6 Zoll), solange sie gehalten wird (ROM 120DH).  Beschriftung [?].", "taster", 24),
        Taste("papier", "Papier da", "Endlospapier eingelegt.  Aus = Papierende (D104 A6 = 1): die Firmware tastet den Sensor je vier Papierschritte ab "
              "und geht offline.  Rolle des Sensors erschlossen [?].", "rastend", 3),
    ),
    lampen=(
        Lampe("POW", "#37d34a", "POW: der Drucker ist eingeschaltet.", None),
        Lampe("ON LINE", "#e6c229", "ON LINE (D105 A2): der Drucker ist bereit und nimmt Daten an.  Lampenrolle erschlossen [?].", 0),
        Lampe("ERR", "#e53935", "ERR (D105 A3): Wachhund des Wagens (Stillstand) oder Haltepunkt der Firmware.  Lampenrolle erschlossen [?].", 1),
    ),
    schnittstellen=("IFSS", "V.24"),
    hat_rom_wahl=False, hat_online_schalter=False,
    dil_basis=0, dil_anzahl=0,
    stand_hinweis="<b>Hardware erschlossen [?]:</b> zum K6311 gibt es keinen Schaltplan; die Belegung von Wagen, Papiermotor, Nadeln, Tasten und "
                  "Lampen ist aus dem ROM abgelesen.  Das Druckbild (Text ueber V.24, Selbsttest) stimmt Punkt fuer Punkt mit dem Zeichengenerator im ROM "
                  "ueberein.  Nicht nachgebildet: Centronics, Summer; Fettdruck/Unterstreichen sind nicht gegen ein Orakel geprueft.",
)
K6311 = Typbeschreibung(schluessel="k6311", titel="K6311 (V.24) - Hardware erschlossen [?]", kern_maschine="k6311", schnittstelle_fest="v24",
                        **_K6311_BASIS)
#: IFSS (WP8g): nur der Abzug 13325212 (8 KiB, Daten ueber SIO-Kanal B); Selbsttest/Text stimmen mit dem Zeichengenerator im ROM ueberein.
K6311_IFSS = Typbeschreibung(schluessel="k6311-ifss", titel="K6311 (IFSS) - Hardware erschlossen [?]", kern_maschine="k6311-ifss",
                             schnittstelle_fest="ifss", **_K6311_BASIS)


#: Die waehlbaren Typen (in der Reihenfolge des Auswahlfelds).
TYPEN = (SD1157_264, K6316_IFSS, K6316_V24, K6313, K6313_IFSS, K6311, K6311_IFSS)

#: Typen, die der Entwurf kennt, der Kern aber noch nicht faehrt: (Schluessel, Anzeige, Begruendung).
GESPERRT = (
    ("sd1157-267", "SD 1157/267", "Nicht vorgesehen (zweite Papierbahn wird nicht nachgebildet)."),
    ("sd1152-257", "SD 1152/257 (Typenrad)", "In Vorbereitung."),
)

VORGABE = SD1157_264


def typ(schluessel: str) -> Typbeschreibung:
    """Die Beschreibung zum Schluessel; ein unbekannter oder gesperrter wird zur Vorgabe."""
    for t in TYPEN:
        if t.schluessel == schluessel:
            return t
    return VORGABE


#: Die ROM-Wahl steht im Katalog data/drucker_roms.yaml (app/printertool/rom_katalog.py), nicht mehr hier (Entwurf 49 §11.2).

SCHNITTSTELLEN = (("ifss", "IFSS"), ("v24", "V.24"))


def konfig_text(schnittstelle: str = "ifss", rom: str = "repariert", beschreibung: Optional[Typbeschreibung] = None,
                betriebsart: str = "endlos", dil: int = 0, romdatei: Optional[str] = None) -> str:
    """Konfigurationstext fuer ``k1520_create_printer`` (Unbekanntes -> Vorgabe).

    Beim K6316 folgt das Schnittstellenmodul dem Typ; der Kern kennt dort ``betriebsart`` und ``dil``.
    ``romdatei`` (Pfad aus dem ROM-Katalog) hat Vorrang vor dem eingebetteten Abbild; ohne Katalog bleibt
    am SD 1157 ``rom=`` (Alias) als Rueckfall.  Ein Komma im Pfad trennt die Liste und ist nicht darstellbar."""
    zusatz = f",romdatei={romdatei}" if romdatei and "," not in romdatei else ""
    if beschreibung is not None and beschreibung.schluessel in ("k6311", "k6311-ifss"):
        return zusatz[1:]   # nur romdatei=; BCH/BDH (schalter_baud/schalter_format) haben Vorgaben im Kern
    if beschreibung is not None and beschreibung.schluessel in ("k6313", "k6313-ifss"):
        return f"dil={int(dil) & ((1 << beschreibung.dil_anzahl) - 1)}{zusatz}"
    if beschreibung is not None and beschreibung.betriebsart_id is not None:
        betriebsart = betriebsart if betriebsart in ("aufwickeln", "schneiden", "endlos") else "endlos"
        return f"betriebsart={betriebsart},dil={int(dil) & 0xFFFF}{zusatz}"
    schnittstelle = schnittstelle if schnittstelle in ("ifss", "v24") else "ifss"
    rom = rom if rom in ("repariert", "original") else "repariert"
    return f"schnittstelle={schnittstelle},rom={rom}{zusatz}"

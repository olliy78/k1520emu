"""
K1520 Emulator - IDE-Platte GIDE mit Uhr RTC 72421 (Einstellungen, Konfiguration)
=================================================================================

Single source of truth fuer die GIDE-Karte in der Oberflaeche, nach demselben Schnitt
wie :mod:`app.raf` und :mod:`app.rtc`: Namen, Texte, Normalisierung der Konfiguration.
Das Modul importiert kein Qt.  Entwurf: ``doc/design/50_gun_gide.md`` (AP G6),
Merkposten ``doc/merkposten/gun.md``.

**Konfiguration** (``general.gide``, ein Abschnitt)::

    general:
      gide:
        enabled: false      # Karte gesteckt?  (Vorgabe aus)
        basis: D0           # E/A-Basis, Hochnibble-Jumper, Hex (00..F0)
        image: ''           # Plattenabbild ('' = keins)
        rtc: true           # RTC 72421 bestueckt

Ein fehlender Abschnitt heisst „nicht anfassen" (Auslieferungsvorgabe aelterer Dateien);
ein unbekannter/kaputter Einzelwert faellt auf die Vorgabe zurueck.

**Warum die Basis eine Wahl ist:** die Jumperstellung der echten Karten ist unbekannt
(Anwenderentscheid 2026-11-10).  Das Auswahlfeld bildet 00H..F0H ab (:mod:`app.kartenwahl`, belegte
Eintraege sind markiert und nicht waehlbar); die Vorauswahl ist ueberall 90H (94H..9FH ist auf allen
Maschinen frei und kollidiert weder mit K5020 noch EPROMmer D0H..D4H noch RAF).  Die uebliche Basis der
Community, D0H (Poppe-Treiber GIDEHPD0, Kingstener gidn@D0h), ist waehlbar, wo sie frei ist.  Ein Konflikt
mit einer Karte, die der Auswahl entgeht, wird vom KERN beim Stecken gemeldet -- die Oberflaeche zeigt den
Fehlertext.
"""

from __future__ import annotations

import os
from typing import Optional

from app import instanz, kartenwahl, paths

#: Schluessel in ``general`` der Konfiguration.
SCHLUESSEL = "gide"

#: Wahl der Basis: Hochnibble 00H..F0H in 10H-Schritten (Vielfaches von 10H, Kern verlangt es).
BASEN = tuple(kartenwahl.GIDE.basen())

#: Plattengroessen von „Neue Platte" in MB; Vorgabe 256 (Poppe-Treiber braucht >= 504000 Sektoren).
GROESSEN_MB = (8, 16, 32, 64, 128, 256)
GROESSE_VORGABE_MB = 256

TIPP_KARTE = (
    "Steckt die IDE-Karte GIDE (nach Tilmann Reh) mit einer ATA-Platte und optional der Uhr RTC 72421.  "
    "Die Karte belegt die E/A-Adressen Basis+4 bis Basis+0FH.  Ein Wechsel schaltet die Maschine aus "
    "und neu ein.")
TIPP_BASIS = (
    "Hochnibble der E/A-Adresse (Jumper A4–A7 der Karte).  Die Jumperstellung der echten Karten ist "
    "nicht bekannt; vorbelegt ist 90H (frei auf allen Rechnern), üblich in der Praxis D0H (Treiber "
    "GIDEHPD0, gidn@D0h).  Belegte Einträge (Kassette K5020, EPROMmer, andere Karten) sind markiert und "
    "nicht wählbar.  Ein Wechsel schaltet die Maschine aus und neu ein.")
TIPP_ABBILD = (
    "Plattenabbild (Rohabbild; die Endung .gz packt es).  Auswerfen schreibt sofort zurück.  "
    "Der Poppe-Treiber braucht mindestens 504000 Sektoren (rund 258 MB), der Kingstener-Treiber "
    "kommt mit 32 MB aus.")
TIPP_RTC = (
    "Uhr RTC 72421 auf der Karte (Basis+5).  Sie zeigt UTC wie die Uhr des P8000 und läuft in "
    "Maschinenzeit; der Stand liegt in einer Batteriedatei im Konfigurationsordner.")

#: Hinweis im „Neue Platte"-Dialog.
HINWEIS_GROESSE = (
    "Der Treiber von Poppe braucht mindestens 504000 Sektoren (256 MB); der von Kingstener kommt mit "
    "32 MB aus.  Die Datei wird sparsam angelegt (belegt nur, was beschrieben wird).")


basis_text = kartenwahl.basis_text


def basis_lesen(wert, vorgabe: int) -> int:
    """Basis aus der Konfiguration: Hexzeichen ``"D0"``/``"D0H"``/``"0xD0"``.

    Auch eine von Hand ohne Anfuehrungszeichen geschriebene Zahl (``basis: 90`` liest YAML als
    die Zahl 90) gilt als die geschriebenen Hexziffern -- geschrieben wird die Basis stets als
    Zeichenkette.  Ungueltig (kein Vielfaches von 10H, nicht lesbar) -> *vorgabe*.
    """
    try:
        if isinstance(wert, bool):
            raise ValueError
        t = str(wert).strip().lower().removeprefix("0x").removesuffix("h")
        n = int(t, 16)
    except (ValueError, TypeError):
        return vorgabe
    return n if n in BASEN else vorgabe


def vorgabe(basis_vorgabe: int) -> dict:
    """Der Zustand ohne Karte: aus, Basis der Maschine, kein Abbild, RTC bestueckt."""
    return {"enabled": False, "basis": int(basis_vorgabe), "image": "", "rtc": True}


def normalize(wert, basis_vorgabe: int, alt: Optional[dict] = None) -> dict:
    """Ein ``general.gide``-Abschnitt als gueltiger Zustand.

    Nur ein echtes ``true`` steckt die Karte.  Kein Abschnitt (kein ``dict``) -> *alt*
    (oder die Vorgabe): der Aufrufer entscheidet vorher, ob er „nicht anfassen" meint.
    """
    z = dict(alt) if alt else vorgabe(basis_vorgabe)
    if not isinstance(wert, dict):
        return z
    z["enabled"] = wert.get("enabled") is True
    z["basis"] = basis_lesen(wert.get("basis", z["basis"]), basis_vorgabe)
    img = wert.get("image", "")
    z["image"] = str(img).strip() if isinstance(img, (str, os.PathLike)) else ""
    z["rtc"] = wert.get("rtc") is not False        # nur ein echtes false nimmt die RTC weg
    return z


def fuer_konfig(zustand: dict) -> dict:
    """Der Zustand, wie er in die Konfigurationsdatei kommt (Basis als Hexzeichen)."""
    return {"enabled": bool(zustand["enabled"]),
            "basis": f"{int(zustand['basis']) & 0xFF:02X}",
            "image": str(zustand.get("image") or ""),
            "rtc": bool(zustand["rtc"])}


def batterie_pfad(programm: str) -> str:
    """Batteriedatei der RTC: ``gide_rtc_<programm>.dat`` im Konfigurationsordner.

    Je Programm eine eigene (der Ordner ist fuer alle Programme derselbe); bei einer
    Mehrinstanz (``--instance``) traegt der Name die Instanz, wie bei der P8000-Uhr.
    """
    n = instanz.name()
    stamm = f"gide_rtc_{programm}" + (f"-{n}" if n else "")
    return os.path.join(str(paths.config_dir()), stamm + ".dat")


def platten_ordner() -> str:
    """Standardordner fuer Plattenabbilder: ``<Dokumentenordner>/K1520emu/GIDE_Platten``."""
    return str(paths.user_data_dir() / "GIDE_Platten")


def kern_param(zustand: dict, programm: str) -> Optional[dict]:
    """Der ``gide=``-Parameter fuer ``K1520Emulator`` (``None`` = keine Karte)."""
    if not zustand.get("enabled"):
        return None
    return {"basis": int(zustand["basis"]), "rtc": bool(zustand["rtc"]),
            "abbild": str(zustand.get("image") or ""),
            "rtc_zustand": batterie_pfad(programm) if zustand["rtc"] else ""}

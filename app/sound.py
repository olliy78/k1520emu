"""
K1520 Emulator - SOUND-Karte K1520 (AY-3-8910/8912 + CTC) (Einstellungen, Konfiguration)
=========================================================================================

Single source of truth fuer die SOUND-Karte in der Oberflaeche, nach demselben Schnitt wie
:mod:`app.netz` und :mod:`app.usb`.  Das Modul importiert kein Qt.  Entwurf:
``doc/design/51_soundkarte_k1520.md`` (AP S8).

**Konfiguration** (``general.sound``; Lautstaerke und Stumm stehen daneben, weil sie auch ohne
Karte gelten, wenn man sie spaeter steckt)::

    general:
      sound:
        enabled: false          # Karte gesteckt?  (Vorgabe aus)
        basis: '38'             # E/A-Basis, Vielfaches von 8, Hex (PC 1715W: E0)
        chip: ay8910            # ay8910 | ay8912
        takt: quarz_halbe       # quarz_halbe = Quarz/2 (1,79 MHz) | quarz | system
        quarz_hz: 3579545
        kanal: abc              # abc | acb | mono
        tiefpass: false         # C16/C17 bestueckt
        kaskade: [false, false, false]   # TO0->TR1, TO1->TR2, TO2->TR3
      lautstaerke: 80           # Prozent
      stumm: false

Ein fehlender Abschnitt heisst „nicht anfassen"; ein kaputter Einzelwert faellt auf die Vorgabe zurueck.
"""

from __future__ import annotations

from typing import Optional

from app import kartenwahl

SCHLUESSEL = "sound"
SCHLUESSEL_LAUTSTAERKE = "lautstaerke"
SCHLUESSEL_STUMM = "stumm"
LAUTSTAERKE_VORGABE = 80

#: (Konfigurationswert, Anzeige)
CHIPS = (("ay8910", "AY-3-8910"), ("ay8912", "AY-3-8912"))
TAKTE = (("quarz_halbe", "Quarz/2 = 1,79 MHz"), ("quarz", "Quarz"), ("system", "Systemtakt"))
KANAELE = (("abc", "Stereo ABC"), ("acb", "Stereo ACB"), ("mono", "Mono"))
QUARZ_VORGABE = 3_579_545
#: Anzeige der drei Kaskadenbruecken (Bit 0..2)
KASKADEN = ("TO0 → TR1", "TO1 → TR2", "TO2 → TR3")

#: Anzeige von ``audio_status()``
STATUS_TEXT = {"aus": "aus", "stumm": "stumm", "hoerbar": "hörbar", "kein_geraet": "kein Gerät"}

TIPP_KARTE = (
    "Steckt die SOUND-Karte K1520 (Tonbaustein AY-3-8910/8912 mit CTC).  Hörbar ist sie nur bei "
    "Echtzeit (Takt 1×).  Ein Wechsel schaltet die Maschine aus und neu ein.")
TIPP_CHIP = "AY-3-8910 hat zwei 8-Bit-Tore (A und B); der AY-3-8912 nur eines (A)."
TIPP_BASIS = (
    "E/A-Basis der acht Tore der Karte (Vielfache von 8).  Vorgewählt ist 38H, am PC 1715W "
    "E0H (dort sind 38H–3FH belegt).  Belegte Einträge sind markiert und nicht wählbar.  Ein Wechsel "
    "schaltet die Maschine aus und neu ein.")
TIPP_TAKT = (
    "Takt des AY (Brücke JP2): Quarz/2 (Vorgabe, 1,79 MHz), Quarz oder Systemtakt der Maschine.  "
    "Ein Wechsel schaltet die Maschine aus und neu ein.")
TIPP_QUARZ = "Frequenz des Quarzes der Karte in Hertz (Vorgabe 3 579 545)."
TIPP_KANAL = "Zuordnung der drei AY-Kanäle zu links/rechts: Stereo ABC, Stereo ACB oder Mono."
TIPP_TIEFPASS = "Die Kondensatoren C16/C17 der Karte sind bestückt: der Ton klingt dumpfer."
TIPP_KASKADE = (
    "Hardwarebrücken der CTC-Kanäle der Karte: der Ausgang ZC/TO eines Kanals speist den Takteingang "
    "des nächsten.")


def _hz_lesen(wert, vorgabe: int = QUARZ_VORGABE) -> int:
    try:
        if isinstance(wert, bool):
            raise ValueError
        n = int(wert)
    except (ValueError, TypeError):
        return vorgabe
    return n if 100_000 <= n <= 20_000_000 else vorgabe


def _wahl(wert, tabelle, vorgabe: str) -> str:
    w = str(wert).strip().lower() if isinstance(wert, str) else ""
    return w if w in {k for k, _ in tabelle} else vorgabe


def _maske(wert, vorgabe: int = 0) -> int:
    """Kaskade aus der Konfiguration (Liste aus drei Wahrheitswerten) als Bitmaske."""
    if not isinstance(wert, (list, tuple)) or len(wert) != 3:
        return vorgabe
    return sum(1 << i for i, b in enumerate(wert) if b is True)


def basis_lesen(wert, vorgabe: int) -> int:
    return kartenwahl.basis_lesen(kartenwahl.SOUND, wert, vorgabe)


def vorgabe(basis_vorgabe: int) -> dict:
    return {"enabled": False, "basis": int(basis_vorgabe), "chip": "ay8910", "takt": "quarz_halbe",
            "quarz_hz": QUARZ_VORGABE, "kanal": "abc", "tiefpass": False, "kaskade": 0}


def normalize(wert, basis_vorgabe: int, alt: Optional[dict] = None) -> dict:
    """Ein ``general.sound``-Abschnitt als gueltiger Zustand (nur ein echtes ``true`` steckt die Karte)."""
    z = dict(alt) if alt else vorgabe(basis_vorgabe)
    if not isinstance(wert, dict):
        return z
    z["enabled"] = wert.get("enabled") is True
    z["basis"] = basis_lesen(wert.get("basis", z["basis"]), basis_vorgabe)
    z["chip"] = _wahl(wert.get("chip"), CHIPS, "ay8910")
    z["takt"] = _wahl(wert.get("takt"), TAKTE, "quarz_halbe")
    z["quarz_hz"] = _hz_lesen(wert.get("quarz_hz"))
    z["kanal"] = _wahl(wert.get("kanal"), KANAELE, "abc")
    z["tiefpass"] = wert.get("tiefpass") is True
    z["kaskade"] = _maske(wert.get("kaskade"))
    return z


def fuer_konfig(zustand: dict) -> dict:
    return {"enabled": bool(zustand["enabled"]),
            "basis": f"{int(zustand['basis']) & 0xFF:02X}",
            "chip": zustand["chip"], "takt": zustand["takt"],
            "quarz_hz": int(zustand["quarz_hz"]), "kanal": zustand["kanal"],
            "tiefpass": bool(zustand["tiefpass"]),
            "kaskade": [bool(int(zustand["kaskade"]) >> i & 1) for i in range(3)]}


def kern_param(zustand: dict) -> Optional[dict]:
    """Der ``sound=``-Parameter fuer ``K1520Emulator`` (``None`` = keine Karte)."""
    if not zustand.get("enabled"):
        return None
    return {"basis": int(zustand["basis"]), "chip": zustand["chip"], "takt": zustand["takt"],
            "quarz_hz": int(zustand["quarz_hz"]), "kanal": zustand["kanal"],
            "tiefpass": bool(zustand["tiefpass"]), "kaskade": int(zustand["kaskade"])}


def lautstaerke_lesen(wert) -> int:
    try:
        if isinstance(wert, bool):
            raise ValueError
        n = int(wert)
    except (ValueError, TypeError):
        return LAUTSTAERKE_VORGABE
    return n if 0 <= n <= 100 else LAUTSTAERKE_VORGABE

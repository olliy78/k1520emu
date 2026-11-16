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
        chip: ay8910            # immer ay8910 (Oberflaeche ohne Wahl)
        ay_takt_hz: 1789772     # Takt am AY in Hz (= Quarz 3 579 545 / 2, Stueckliste)
        kanal: acb              # acb (Vorgabe) | abc | mono
        tiefpass_hz: 24000      # C16/C17: Grenzfrequenz in Hz (3000|8000|16000|24000), 24000 = ungefiltert (alt: ``tiefpass: true`` = 8000)
        kaskade: [true, true, true]      # TO0->TR1, TO1->TR2, TO2->TR3 (ab Werk gesteckt)
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
KANAELE = (("acb", "Stereo ACB"), ("abc", "Stereo ABC"), ("mono", "Mono"))
QUARZ_VORGABE = 3_579_545
#: Vorgabe des AY-Takts: Quarz halbiert (Stueckliste, JP2 Q1/2)
AY_TAKT_VORGABE = QUARZ_VORGABE // 2
AY_TAKT_MIN, AY_TAKT_MAX = 500_000, 4_000_000
#: Nenntakt, wenn die Umrechnung eines alten ``takt: system`` keinen Maschinentakt kennt
NENNTAKT_VORGABE = 2_457_600
#: Wahl des Tiefpasses C16/C17 in Hz; die Grenze 8 kHz entspricht dem alten Haken, 24 kHz = ungefiltert
TIEFPASS_WERTE = (3000, 8000, 16000, 24000)
TIEFPASS_VORGABE = 24000
TIEFPASS_ALT_AN = 8000
#: Vorgabe der Kaskade: alle drei Bruecken gesteckt (SKTEST4 Menue 3 haengt sonst)
KASKADE_VORGABE = 7
#: Anzeige der drei Kaskadenbruecken (Bit 0..2)
KASKADEN = ("TO0 → TR1", "TO1 → TR2", "TO2 → TR3")

#: Anzeige von ``audio_status()``
STATUS_TEXT = {"aus": "aus", "stumm": "stumm", "hoerbar": "hörbar", "kein_geraet": "kein Gerät"}

TIPP_KARTE = (
    "Steckt die SOUND-Karte K1520 (Tonbaustein AY-3-8910/8912 mit CTC).  Hörbar ist sie bei "
    "jedem Takt außer „unbegrenzt“ (der AY läuft in echter Zeit).  Ein Wechsel schaltet die Maschine aus und neu ein.")
TIPP_BASIS = (
    "E/A-Basis der acht Tore der Karte (Vielfache von 8).  Vorgewählt ist 38H, am PC 1715W "
    "E0H (dort sind 38H–3FH belegt).  Belegte Einträge sind markiert und nicht wählbar.  Ein Wechsel "
    "schaltet die Maschine aus und neu ein.")
TIPP_TAKT = (
    "Takt am AY in MHz (Vorgabe 1,789772 = Quarz 3,579545 MHz halbiert, so die Stückliste).  Die echte Karte kann per "
    "Brücke JP2 auch den vollen Quarztakt oder den Systemtakt der Maschine nehmen — dann hier diese Frequenz "
    "eintragen.  Ein höherer Takt lässt Lieder höher klingen.  Die Änderung wirkt sofort.")
TIPP_KANAL = "Zuordnung der drei AY-Kanäle zu links/rechts: Stereo ACB (Vorgabe), Stereo ABC oder Mono."
TIPP_TIEFPASS = (
    "Tiefpass (Kondensatoren C16/C17): dämpft die Höhen steil (24 dB je Oktave), je tiefer die Grenzfrequenz, desto weicher klingt der Ton.  "
    "24 kHz = ungefiltert (Vorgabe).  Der Ausgang dämpft ohnehin oberhalb von etwa 8 kHz gegen Aliasing, 16 und 24 kHz klingen deshalb fast gleich.")
TIPP_KASKADE = (
    "Hardwarebrücken der CTC-Kanäle der Karte: der Ausgang ZC/TO eines Kanals speist den Takteingang "
    "des nächsten.  Ab Werk alle drei gesteckt (Testprogramme wie SKTEST4 setzen das voraus).")


def _ay_takt_lesen(wert, vorgabe: int = AY_TAKT_VORGABE) -> int:
    try:
        if isinstance(wert, bool):
            raise ValueError
        n = int(wert)
    except (ValueError, TypeError):
        return vorgabe
    return n if AY_TAKT_MIN <= n <= AY_TAKT_MAX else vorgabe


def _ay_takt_aus_alt(wert: dict, nenntakt_hz: int, vorgabe: int) -> int:
    """Alte Eintraege ``takt``/``quarz_hz`` (vor ``ay_takt_hz``) in den AY-Takt umrechnen."""
    takt = wert.get("takt")
    if takt == "system":
        return _ay_takt_lesen(nenntakt_hz, vorgabe)
    try:
        q = int(wert.get("quarz_hz", QUARZ_VORGABE))
    except (ValueError, TypeError):
        return vorgabe
    if isinstance(wert.get("quarz_hz"), bool):
        return vorgabe
    return _ay_takt_lesen(q if takt == "quarz" else q // 2, vorgabe)


def _wahl(wert, tabelle, vorgabe: str) -> str:
    w = str(wert).strip().lower() if isinstance(wert, str) else ""
    return w if w in {k for k, _ in tabelle} else vorgabe


def tiefpass_lesen(wert: dict, vorgabe: int = TIEFPASS_VORGABE) -> int:
    """Tiefpass in Hz aus einem Abschnitt: ``tiefpass_hz``; alter Schluessel ``tiefpass`` (true = 8000, false = 24000).

    Fehlend/kaputt/0/false = Vorgabe (24000, ungefiltert), true = 8000, jeder andere Wert der naechste der vier Stufen."""
    # alter Schluessel ``tiefpass`` kennt nur Wahrheitswerte (andere Werte dort = Vorgabe)
    n = wert.get("tiefpass_hz") if "tiefpass_hz" in wert else wert.get("tiefpass", None)
    if "tiefpass_hz" not in wert and n is not True:
        return vorgabe
    if n is True:
        return TIEFPASS_ALT_AN
    if isinstance(n, bool) or not isinstance(n, int) or n <= 0:
        return vorgabe
    return min(TIEFPASS_WERTE, key=lambda w: abs(w - n))


def _maske(wert, vorgabe: int = KASKADE_VORGABE) -> int:
    """Kaskade aus der Konfiguration (Liste aus drei Wahrheitswerten) als Bitmaske."""
    if not isinstance(wert, (list, tuple)) or len(wert) != 3:
        return vorgabe
    return sum(1 << i for i, b in enumerate(wert) if b is True)


def basis_lesen(wert, vorgabe: int) -> int:
    return kartenwahl.basis_lesen(kartenwahl.SOUND, wert, vorgabe)


def vorgabe(basis_vorgabe: int) -> dict:
    return {"enabled": False, "basis": int(basis_vorgabe), "chip": "ay8910",
            "ay_takt_hz": AY_TAKT_VORGABE, "kanal": "acb", "tiefpass_hz": TIEFPASS_VORGABE,
            "kaskade": KASKADE_VORGABE}


def normalize(wert, basis_vorgabe: int, alt: Optional[dict] = None, nenntakt_hz: int = NENNTAKT_VORGABE) -> dict:
    """Ein ``general.sound``-Abschnitt als gueltiger Zustand (nur ein echtes ``true`` steckt die Karte)."""
    z = dict(alt) if alt else vorgabe(basis_vorgabe)
    if not isinstance(wert, dict):
        return z
    z["enabled"] = wert.get("enabled") is True
    z["basis"] = basis_lesen(wert.get("basis", z["basis"]), basis_vorgabe)
    z["chip"] = "ay8910"            # Oberflaeche: immer AY-3-8910 (ein alter Eintrag "ay8912" wird nicht uebernommen)
    if "ay_takt_hz" in wert:
        z["ay_takt_hz"] = _ay_takt_lesen(wert.get("ay_takt_hz"))
    elif "takt" in wert or "quarz_hz" in wert:      # alter Eintrag: umrechnen, nicht mehr schreiben
        z["ay_takt_hz"] = _ay_takt_aus_alt(wert, nenntakt_hz, AY_TAKT_VORGABE)
    else:
        z["ay_takt_hz"] = AY_TAKT_VORGABE
    z["kanal"] = _wahl(wert.get("kanal"), KANAELE, "acb")
    z["tiefpass_hz"] = tiefpass_lesen(wert)
    z["kaskade"] = _maske(wert.get("kaskade"))
    return z


def fuer_konfig(zustand: dict) -> dict:
    return {"enabled": bool(zustand["enabled"]),
            "basis": f"{int(zustand['basis']) & 0xFF:02X}",
            "chip": zustand["chip"], "ay_takt_hz": int(zustand["ay_takt_hz"]),
            "kanal": zustand["kanal"], "tiefpass_hz": int(zustand["tiefpass_hz"]),
            "kaskade": [bool(int(zustand["kaskade"]) >> i & 1) for i in range(3)]}


def kern_param(zustand: dict) -> Optional[dict]:
    """Der ``sound=``-Parameter fuer ``K1520Emulator`` (``None`` = keine Karte)."""
    if not zustand.get("enabled"):
        return None
    return {"basis": int(zustand["basis"]), "chip": zustand["chip"], "ay_takt_hz": int(zustand["ay_takt_hz"]),
            "kanal": zustand["kanal"], "tiefpass": int(zustand["tiefpass_hz"]), "kaskade": int(zustand["kaskade"])}


def lautstaerke_lesen(wert) -> int:
    try:
        if isinstance(wert, bool):
            raise ValueError
        n = int(wert)
    except (ValueError, TypeError):
        return LAUTSTAERKE_VORGABE
    return n if 0 <= n <= 100 else LAUTSTAERKE_VORGABE

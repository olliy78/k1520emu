"""
K1520 Emulator - Netzwerkkarte KCNet (Z80-PIO + W5100) (Einstellungen, Konfiguration)
====================================================================================

Single source of truth fuer die Netzkarte in der Oberflaeche (:mod:`app.gide`, :mod:`app.usb` sind
die Geschwister).  Das Modul importiert kein Qt.  Entwurf ``doc/design/50_gun_gide.md`` (AP N3).

**Konfiguration** (``general.net``)::

    general:
      net:
        enabled: false      # Karte gesteckt?  (Vorgabe aus)
        basis: C0           # E/A-Basis, Vielfaches von 4, Hex
        nur_loopback: false # true = der Gast erreicht nur den Wirt selbst
        link: true          # Netzwerkkabel steckt

**Vorgabe ``nur_loopback`` = aus** (Anwenderentscheid): der emulierte Rechner darf Verbindungen ins
echte Netz aufbauen.  Die Tests laufen dagegen immer auf Loopback.
"""

from __future__ import annotations

from typing import Optional

from app import kartenwahl

SCHLUESSEL = "net"

TIPP_KARTE = (
    "Steckt die Netzwerkkarte KCNet (Z80-PIO mit Netzwerkbaustein W5100, am Wirtsnetz).  Die Basis muss "
    "zur Gastsoftware (TFTP, CPMNET) passen.  Ein Wechsel schaltet die Maschine aus und neu ein.")
TIPP_BASIS = (
    "E/A-Basis der vier Tore der Karte (Vielfache von 4).  Üblich: C0H oder 28H (Jumper der "
    "K1520-Netzkarte), 88H bei der GUN — 88H/89H gehören aber der RAM-Floppy RAF, solange diese "
    "steckt.  Belegte Einträge sind markiert und nicht wählbar.  Ein Wechsel schaltet die Maschine aus "
    "und neu ein.")
TIPP_LOOPBACK = (
    "Mit Haken erreicht der emulierte Rechner nur den Wirt selbst (127.0.0.1).  OHNE Haken (Vorgabe) "
    "kann er Verbindungen ins echte Netz aufbauen und annehmen — das ist gewollt, heißt aber: was im "
    "Gast läuft, ist im Netz sichtbar.  Ein Wechsel schaltet die Maschine aus und neu ein.")
TIPP_LINK = "Netzwerkkabel verbunden (sofort wirksam, ohne Neustart): ohne Kabel meldet die Karte „offline“."


def basis_lesen(wert, vorgabe: int) -> int:
    return kartenwahl.basis_lesen(kartenwahl.NETZ, wert, vorgabe)


def vorgabe(basis_vorgabe: int) -> dict:
    return {"enabled": False, "basis": int(basis_vorgabe), "nur_loopback": False, "link": True}


def normalize(wert, basis_vorgabe: int, alt: Optional[dict] = None) -> dict:
    """Ein ``general.net``-Abschnitt als gueltiger Zustand (nur ein echtes ``true`` steckt die Karte)."""
    z = dict(alt) if alt else vorgabe(basis_vorgabe)
    if not isinstance(wert, dict):
        return z
    z["enabled"] = wert.get("enabled") is True
    z["basis"] = basis_lesen(wert.get("basis", z["basis"]), basis_vorgabe)
    z["nur_loopback"] = wert.get("nur_loopback") is True
    z["link"] = wert.get("link") is not False
    return z


def fuer_konfig(zustand: dict) -> dict:
    return {"enabled": bool(zustand["enabled"]),
            "basis": f"{int(zustand['basis']) & 0xFF:02X}",
            "nur_loopback": bool(zustand["nur_loopback"]),
            "link": bool(zustand["link"])}


def kern_param(zustand: dict) -> Optional[dict]:
    """Der ``net=``-Parameter fuer ``K1520Emulator`` (``None`` = keine Karte)."""
    if not zustand.get("enabled"):
        return None
    return {"basis": int(zustand["basis"]), "nur_loopback": bool(zustand["nur_loopback"]),
            "link": bool(zustand["link"])}

"""
K1520 Emulator - USB-Karte der GUN (Z80-PIO + VDIP1) mit Stick-Ordner (Einstellungen, Konfiguration)
==================================================================================================

Single source of truth fuer die USB-Karte in der Oberflaeche, nach demselben Schnitt wie
:mod:`app.gide`.  Das Modul importiert kein Qt.  Entwurf: ``doc/design/50_gun_gide.md`` (AP U3),
Merkposten ``doc/merkposten/gun.md``.

**Konfiguration** (``general.usb``)::

    general:
      usb:
        enabled: false          # Karte gesteckt?  (Vorgabe aus)
        basis: '84'             # E/A-Basis, Vielfaches von 4, Hex
        ordner: ''              # Stick-Ordner ('' = <Dokumente>/K1520emu/USB-Stick)
        schreibgeschuetzt: false

Der „Stick" ist ein Wirtsverzeichnis; der Kern sperrt ihn ein (nur 8.3-Namen, nie ausserhalb,
Symlinks unsichtbar).  Ein fehlender Abschnitt heisst „nicht anfassen"; ein kaputter Einzelwert
faellt auf die Vorgabe zurueck.
"""

from __future__ import annotations

import os
from typing import Optional

from app import kartenwahl, paths

SCHLUESSEL = "usb"

TIPP_KARTE = (
    "Steckt die USB-Karte der GUN (Z80-PIO mit Speicherstick-Baustein VDIP1).  Der Stick ist ein Ordner "
    "des Wirtsrechners.  Ein Wechsel schaltet die Maschine aus und neu ein.")
TIPP_BASIS = (
    "E/A-Basis der vier Tore der Karte (Vielfache von 4).  Die GUN hat fest 84H; am K8915 V2 und am "
    "PRG 710 ist 80H–87H belegt, dort ist 8CH vorgewählt.  Belegte Einträge (RAM-Floppy 88H/89H, "
    "Kassette, andere Karten) sind markiert und nicht wählbar.  Ein Wechsel schaltet die Maschine aus "
    "und neu ein.")
TIPP_ORDNER = (
    "Wirtsordner, der als Stick im Gast erscheint.  ACHTUNG: das Gastsystem sieht NUR diesen Ordner "
    "(nur Dateinamen im 8.3-Format, nichts außerhalb, Verknüpfungen bleiben unsichtbar) und darf darin "
    "schreiben und löschen, sofern der Stick nicht schreibgeschützt ist.  Der Ordner wird bei Bedarf "
    "angelegt.  Ein Wechsel wirkt sofort, ohne Neustart (wie ein anderer Stick).")
TIPP_SCHUTZ = "Schreibschutz des Sticks (sofort wirksam; der Stick wird dabei neu gesteckt)."
TIPP_STICK = "Zieht den Stick bzw. steckt ihn wieder ein — wirkt sofort, ohne Neustart, wie am echten Gerät."


def basis_lesen(wert, vorgabe: int) -> int:
    return kartenwahl.basis_lesen(kartenwahl.USB, wert, vorgabe)


def ordner_vorgabe() -> str:
    """``<Dokumentenordner>/K1520emu/USB-Stick``."""
    return str(paths.user_data_dir() / "USB-Stick")


def ordner_wirksam(zustand: dict) -> str:
    """Der gemeinte Stick-Ordner (leer in der Konfiguration = die Vorgabe)."""
    o = str(zustand.get("ordner") or "").strip()
    return os.path.abspath(os.path.expanduser(o)) if o else ordner_vorgabe()


def vorgabe(basis_vorgabe: int) -> dict:
    return {"enabled": False, "basis": int(basis_vorgabe), "ordner": "", "schreibgeschuetzt": False}


def normalize(wert, basis_vorgabe: int, alt: Optional[dict] = None) -> dict:
    """Ein ``general.usb``-Abschnitt als gueltiger Zustand (nur ein echtes ``true`` steckt die Karte)."""
    z = dict(alt) if alt else vorgabe(basis_vorgabe)
    if not isinstance(wert, dict):
        return z
    z["enabled"] = wert.get("enabled") is True
    z["basis"] = basis_lesen(wert.get("basis", z["basis"]), basis_vorgabe)
    o = wert.get("ordner", "")
    z["ordner"] = str(o).strip() if isinstance(o, (str, os.PathLike)) else ""
    z["schreibgeschuetzt"] = wert.get("schreibgeschuetzt") is True
    return z


def fuer_konfig(zustand: dict) -> dict:
    return {"enabled": bool(zustand["enabled"]),
            "basis": f"{int(zustand['basis']) & 0xFF:02X}",
            "ordner": str(zustand.get("ordner") or ""),
            "schreibgeschuetzt": bool(zustand.get("schreibgeschuetzt"))}


def kern_param(zustand: dict) -> Optional[dict]:
    """Der ``usb=``-Parameter fuer ``K1520Emulator`` (``None`` = keine Karte)."""
    if not zustand.get("enabled"):
        return None
    return {"basis": int(zustand["basis"]), "ordner": ordner_wirksam(zustand),
            "schreibgeschuetzt": bool(zustand.get("schreibgeschuetzt"))}

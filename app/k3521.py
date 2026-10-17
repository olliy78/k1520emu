"""K3521.30 (akkugepuffertes 4-KB-RAM) als Option des A5120 — Auswahl, Pruefung, Ablage.

Entwurf 33 N10h.  Bis zu vier Karten liegen LUECKENLOS hintereinander ab einer Startadresse
im 4-KB-Raster (Karte i bei ``start + i * 1000H``).  Die Ablage jeder Karte ist eine Rohdatei
(4096 Byte) im Benutzer-Konfigurationsordner, je Programm und Kartennummer.
"""

import os
from typing import List, Optional, Tuple

from app import paths

#: Hoechstzahl der Karten (Kern: ``Config::k3521_weitere`` + 1).
MAX_KARTEN = 4
#: Vorgabe der Startadresse (CP/A-Kaltstart laesst 7000H-AFFFH in Ruhe, Merkposten Nr. 17).
START_VORGABE = 0x8000
RASTER = 0x1000


def pruefe(anzahl: int, start_text) -> Tuple[Optional[int], str]:
    """``(start, "")`` bei gueltiger Eingabe, sonst ``(None, Grund)``.

    Gueltig: Hex-Zahl (mit oder ohne ``0x``/``H``), im 4-KB-Raster, und die Karten enden
    spaetestens bei FFFFH.  Mit 0 Karten zaehlt nur die Form (Hex, Raster).
    """
    t = str(start_text).strip().lower()
    if t.endswith("h"):
        t = t[:-1]
    if t.startswith("0x"):
        t = t[2:]
    if not t or len(t) > 4 or any(c not in "0123456789abcdef" for c in t):
        return None, "keine Hex-Adresse (0000 bis F000)"
    start = int(t, 16)
    if start % RASTER:
        return None, "nicht im 4-KB-Raster (0, 1000, 2000 ... F000)"
    if anzahl > 0 and start + anzahl * RASTER > 0x10000:
        return None, f"{anzahl} Karte(n) ab {start:04X}H enden hinter FFFFH"
    return start, ""


def basen(anzahl: int, start: int) -> List[int]:
    """Basisadressen der Karten 0..anzahl-1."""
    return [start + i * RASTER for i in range(max(0, min(int(anzahl), MAX_KARTEN)))]


def konfig_wert(anzahl: int, start: int) -> Optional[str]:
    """Wert fuer ``konfig["k3521"]`` (``8000+9000``) oder ``None`` ohne Karten."""
    b = basen(anzahl, start)
    return "+".join(f"{x:04x}" for x in b) if b else None


def ablage_pfad(programm: str, n: int) -> str:
    """Ablage der Karte *n* dieses Programms (``k3521_<programm>_<n>.bin``)."""
    return os.path.join(str(paths.config_dir()), f"k3521_{programm}_{n}.bin")

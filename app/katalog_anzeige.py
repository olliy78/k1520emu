"""Anzeige von Katalogeintraegen in Auswahllisten (Entwurf 47, Schritt 6) — ohne Qt.

Eine Auswahlzeile heisst ``Kapazitaet · Rechner · Titel``; der technische Schluessel
(``scp1715_ds80_5x1024_boot``) steht nur im Tooltip und in den Daten des Eintrags.
Gemeinsam fuer Emulator, DiskTool und Tests, damit alle Listen gleich aussehen und
gleich sortieren.
"""

from __future__ import annotations

import re
from typing import Iterable, Sequence, Tuple

TRENNER = " · "          # " · "
ALLE_RECHNER = "alle"


def _teile(x) -> str:
    return str(x).strip() if x else ""


def rechner_text(rechner) -> str:
    """Rechnerliste (Folge oder Text) als ``A, B``; leere Namen entfallen."""
    if isinstance(rechner, str):
        rechner = [rechner]
    return ", ".join(r for r in (_teile(x) for x in (rechner or ())) if r)


def zeile(kapazitaet: str = "", rechner=(), titel: str = "") -> str:
    """``800K · PC 1715 · SCP 1715 Systemdiskette``; leere Teile fallen samt Trenner weg."""
    teile = [_teile(kapazitaet), rechner_text(rechner), _teile(titel)]
    return TRENNER.join(t for t in teile if t)


def tooltip(schluessel: str, beschreibung: str = "", rechner=()) -> str:
    """Tooltip mit dem technischen Namen (und, wo vorhanden, dem Langtext)."""
    z = [f"Technischer Name: {schluessel}"]
    r = rechner_text(rechner)
    if r:
        z.append(f"Rechner: {r}")
    if _teile(beschreibung):
        z.append(_teile(beschreibung))
    return "\n".join(z)


_KAP = re.compile(r"^\s*([0-9]+(?:[.,][0-9]+)?)\s*([KkMmGg])?", re.UNICODE)


def kapazitaet_kb(text: str) -> float:
    """``'800K'`` -> 800, ``'1,44M'`` -> 1474.56; nicht lesbar/leer -> ``inf`` (ans Ende)."""
    m = _KAP.match(text or "")
    if not m:
        return float("inf")
    wert = float(m.group(1).replace(",", "."))
    einheit = (m.group(2) or "K").upper()
    return wert * {"K": 1.0, "M": 1024.0, "G": 1024.0 * 1024.0}[einheit]


def sortierschluessel(kapazitaet: str = "", rechner=(), titel: str = "") -> Tuple:
    """Kapazitaet numerisch, dann Rechner, dann Titel (ohne Gross-/Kleinschreibung)."""
    return (kapazitaet_kb(kapazitaet), rechner_text(rechner).lower(), _teile(titel).lower())


def alle_rechner(eintraege: Iterable) -> Sequence[str]:
    """Alle in ``eintraege`` (Objekte mit ``.machines``) vorkommenden Rechner, sortiert."""
    return sorted({m for e in eintraege for m in (getattr(e, "machines", ()) or ()) if m})


def passt_zu_rechner(machines, rechner: str) -> bool:
    """Filter „Fuer Rechner": ``alle``/leer = alles; leeres ``machines`` (unbekannt) nur dort."""
    if not rechner or rechner == ALLE_RECHNER:
        return True
    return rechner in (machines or ())


def profil_zeile(f) -> str:
    """Auswahlzeile eines Dateisystemprofils (Objekt mit capacity/machines/title/description/name)."""
    return zeile(f.capacity, f.machines, f.title) or f.description or f.name

"""Hinterlegte Bootabbilder des DiskTools (Ticket #25).

Eine Bootdiskette entsteht aus einem leeren Dateisystem plus einem Byteband in den
**Systemspuren**.  Dieses Byteband musste man frueher erst mit „Bootabbild sichern"
aus einer anderen Diskette holen; jetzt liegen die bekannten Bytebaender fertig bei
und stehen in einem Katalog ``bootsektor.yaml``::

    version: 1
    bootsektoren:
      - description: "CP/A am A5120 (K5601)"   # erscheint im Auswahlfeld
        filesystem:  cpa780                     # Dateisystem aus formats.yaml
        file:        bootsektoren/a5120_cpa780.bin   # relativ zu DIESER Datei

Gelesen werden zwei Dateien, in dieser Reihenfolge: der mitgelieferte Katalog
(:func:`app.paths.bootsektor_file`, Installation vor Quellbaum) und die Ergaenzung
des Anwenders (:func:`app.paths.bootsektor_user_file`).  Die Ergaenzung kommt HINZU;
ein Update der Installation laesst sie stehen.

Ein Eintrag, der nicht taugt (unbekanntes Dateisystem, fehlende Datei, groesser als
die Systemspuren), wird NICHT angeboten, sondern als Warnung gemeldet — ein Abbild,
das erst beim Anlegen scheitert, waere die schlechtere Auskunft.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

import yaml

from app import paths


@dataclass(frozen=True)
class Bootabbild:
    """Ein Katalogeintrag."""
    description: str    #: Text im Auswahlfeld
    filesystem: str     #: Dateisystem (Profilname aus formats.yaml)
    path: Path          #: die .bin-Datei (aufgeloest)
    quelle: Path        #: aus welcher bootsektor.yaml
    eigen: bool = False  #: aus der Ergaenzung des Anwenders


def _eintraege_aus(datei: Path, eigen: bool) -> Tuple[List[Bootabbild], List[str]]:
    """Eine ``bootsektor.yaml`` lesen; Formfehler werden Warnungen, kein Abbruch."""
    try:
        roh = yaml.safe_load(datei.read_text(encoding="utf-8")) or {}
    except (OSError, yaml.YAMLError) as e:
        return [], [f"{datei}: nicht lesbar ({e})"]
    liste = roh.get("bootsektoren") if isinstance(roh, dict) else None
    if not isinstance(liste, list):
        return [], [f"{datei}: Liste 'bootsektoren:' fehlt"]

    aus, warn = [], []
    for i, e in enumerate(liste, 1):
        if not isinstance(e, dict):
            warn.append(f"{datei}: Eintrag {i} ist keine Zuordnung")
            continue
        text = str(e.get("description") or "").strip()
        fs = str(e.get("filesystem") or "").strip()
        rel = str(e.get("file") or "").strip()
        if not (text and fs and rel):
            warn.append(f"{datei}: Eintrag {i} braucht description, filesystem und file")
            continue
        p = Path(rel).expanduser()
        if not p.is_absolute():
            p = datei.parent / p
        aus.append(Bootabbild(text, fs, p, datei, eigen))
    return aus, warn


def laden(dateien: Optional[Iterable[Tuple[Path, bool]]] = None
          ) -> Tuple[List[Bootabbild], List[str]]:
    """Alle Eintraege (mitgeliefert, dann eigene) und die Warnungen beim Lesen.

    ``dateien`` = Paare (Pfad, eigen); Vorgabe: mitgelieferter Katalog + Ergaenzung.
    """
    if dateien is None:
        dateien = []
        mit = paths.bootsektor_file()
        if mit:
            dateien.append((mit, False))
        eigen = paths.bootsektor_user_file()
        if eigen.is_file():
            dateien.append((eigen, True))
    alle, warn = [], []
    for datei, eigen in dateien:
        e, w = _eintraege_aus(Path(datei), eigen)
        alle += e
        warn += w
    return alle, warn


def grund(eintrag: Bootabbild, kapazitaet: Dict[str, int]) -> str:
    """Warum ein Eintrag nicht taugt — ``""``, wenn er taugt.

    ``kapazitaet`` = Dateisystem → Byte in seinen Systemspuren (0 = nicht bootfaehig),
    wie sie die Bibliothek nennt (:func:`app.core_binding.k1520disk.filesystems`).
    """
    if eintrag.filesystem not in kapazitaet:
        return f"Dateisystem '{eintrag.filesystem}' gibt es im Formatkatalog nicht"
    platz = kapazitaet[eintrag.filesystem]
    if platz <= 0:
        return f"'{eintrag.filesystem}' hat keine Systemspuren"
    if not eintrag.path.is_file():
        return f"Datei fehlt: {eintrag.path}"
    groesse = eintrag.path.stat().st_size
    if groesse == 0:
        return f"Datei ist leer: {eintrag.path}"
    if groesse > platz:
        return (f"{eintrag.path.name} ist {groesse} Byte gross, die Systemspuren von "
                f"'{eintrag.filesystem}' fassen {platz}")
    return ""


def brauchbare(kapazitaet: Dict[str, int],
               dateien: Optional[Iterable[Tuple[Path, bool]]] = None
               ) -> Tuple[Dict[str, List[Bootabbild]], List[str]]:
    """Nach Dateisystem gruppierte, BRAUCHBARE Eintraege in Katalogreihenfolge + Warnungen."""
    alle, warn = laden(dateien)
    nach_fs: Dict[str, List[Bootabbild]] = {}
    for e in alle:
        g = grund(e, kapazitaet)
        if g:
            warn.append(f"{e.quelle}: „{e.description}“ übergangen — {g}")
            continue
        nach_fs.setdefault(e.filesystem, []).append(e)
    return nach_fs, warn

"""Katalog der EPROM-Saetze der Nadeldrucker (Entwurf 49 §11.2, Merkposten drucker.md Nr. 31).

Datei ``drucker_roms.yaml`` (Aufloesung: :func:`app.paths.printer_roms_file`), die ROM-Dateien liegen im
Ordner :func:`app.paths.printer_eproms_dir`.  Der Kern kennt den Katalog NICHT; er bekommt nur den Pfad
(``romdatei=``).  Dieses Modul ist reines Python ohne Qt (der Waechter laeuft ohne Oberflaeche).
"""

from __future__ import annotations

import hashlib
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import yaml

from app import paths

TYPEN = ("sd1157", "k6316-ifss", "k6316-v24", "k6313", "k6313-ifss", "k6311", "k6311-ifss")
GUETEN = ("ok", "unvollstaendig", "defekt", "unbekannt")
SCHNITTSTELLEN = ("ifss", "v24")
FORMAT_VERSION = 1
_SCHLUESSEL = re.compile(r"^[a-z0-9][a-z0-9_.-]*$")
_MD5 = re.compile(r"^[0-9a-f]{32}$")
#: Pflichtfelder (Name -> Typ).
_PFLICHT = {"schluessel": str, "typ": str, "titel": str, "datei": str, "groesse": int, "md5": str,
            "guete": str, "vorgabe": bool}


@dataclass(frozen=True)
class RomEintrag:
    schluessel: str
    typ: str
    titel: str
    datei: str
    groesse: int
    md5: str
    guete: str
    vorgabe: bool
    pgm: str = ""
    schnittstellen: Tuple[str, ...] = ()
    zeichensatz: str = ""
    hinweis: str = ""
    aliases: Tuple[str, ...] = field(default_factory=tuple)

    def zeile(self) -> str:
        """Text im Auswahlfeld (die Vorgabe steht dort schon im Titel des Katalogs oder wird markiert)."""
        return self.titel

    def pfad(self, ordner: Optional[Path] = None) -> Optional[Path]:
        """Absoluter Pfad der Datei, ``None`` wenn sie fehlt."""
        ordner = ordner or paths.printer_eproms_dir()
        if ordner is None:
            return None
        p = Path(ordner) / self.datei
        return p if p.is_file() else None


def pruefen(daten) -> List[str]:
    """Schema-Pruefung der geladenen YAML-Daten; liefert die Fehler als Texte (leer = in Ordnung).

    Prueft Aufbau, Wertebereiche, Eindeutigkeit der Schluessel und Aliase, genau eine Vorgabe je Typ.
    Dateien und MD5 prueft :func:`dateien_pruefen`, weil sie einen Ordner brauchen."""
    fehler: List[str] = []
    if not isinstance(daten, dict):
        return ["Katalog: oberste Ebene ist keine Abbildung"]
    if daten.get("version") != FORMAT_VERSION:
        fehler.append(f"Katalog: version = {daten.get('version')!r}, erwartet {FORMAT_VERSION}")
    liste = daten.get("eintraege")
    if not isinstance(liste, list) or not liste:
        return fehler + ["Katalog: 'eintraege' fehlt oder ist leer"]
    schluessel: Dict[str, int] = {}
    vorgaben: Dict[str, int] = {}
    alias_je_typ: Dict[Tuple[str, str], str] = {}
    for i, e in enumerate(liste):
        name = f"Eintrag {i + 1}"
        if not isinstance(e, dict):
            fehler.append(f"{name}: keine Abbildung")
            continue
        name = f"Eintrag {i + 1} ({e.get('schluessel')})"
        for feld, typ in _PFLICHT.items():
            wert = e.get(feld)
            if feld not in e:
                fehler.append(f"{name}: Feld '{feld}' fehlt")
            elif not isinstance(wert, typ) or (typ is int and isinstance(wert, bool)):
                fehler.append(f"{name}: Feld '{feld}' hat den falschen Typ ({type(wert).__name__})")
        s, t = e.get("schluessel"), e.get("typ")
        if isinstance(s, str):
            if not _SCHLUESSEL.match(s):
                fehler.append(f"{name}: Schluessel '{s}' nicht erlaubt (a-z, 0-9, _ . -)")
            if s in schluessel:
                fehler.append(f"{name}: Schluessel '{s}' doppelt")
            schluessel[s] = i
        if isinstance(t, str) and t not in TYPEN:
            fehler.append(f"{name}: typ '{t}' unbekannt ({', '.join(TYPEN)})")
        if isinstance(e.get("md5"), str) and not _MD5.match(e["md5"]):
            fehler.append(f"{name}: md5 ist keine 32-stellige Kleinbuchstaben-Hexzahl")
        if isinstance(e.get("guete"), str) and e["guete"] not in GUETEN:
            fehler.append(f"{name}: guete '{e['guete']}' unbekannt ({', '.join(GUETEN)})")
        d = e.get("datei")
        if isinstance(d, str) and (d.startswith("/") or ".." in Path(d).parts or "\\" in d or "," in d):
            fehler.append(f"{name}: datei '{d}' muss relativ und ohne '..', '\\' und ',' sein")
        if isinstance(e.get("groesse"), int) and e["groesse"] <= 0:
            fehler.append(f"{name}: groesse muss positiv sein")
        for liste_name, erlaubt in (("schnittstellen", SCHNITTSTELLEN),):
            w = e.get(liste_name, [])
            if not isinstance(w, list) or any(x not in erlaubt for x in w):
                fehler.append(f"{name}: '{liste_name}' muss eine Liste aus {erlaubt} sein")
        for feld in ("pgm", "zeichensatz", "hinweis"):
            if feld in e and not isinstance(e[feld], str):
                fehler.append(f"{name}: '{feld}' muss Text sein")
        al = e.get("aliases", [])
        if not isinstance(al, list) or any(not isinstance(a, str) or not a for a in al):
            fehler.append(f"{name}: 'aliases' muss eine Liste von Texten sein")
            al = []
        if isinstance(t, str):
            if e.get("vorgabe") is True:
                vorgaben[t] = vorgaben.get(t, 0) + 1
            for a in al:
                if (t, a) in alias_je_typ:
                    fehler.append(f"{name}: Alias '{a}' ist schon Alias von {alias_je_typ[(t, a)]}")
                alias_je_typ[(t, a)] = str(s)
    for (t, a), s in alias_je_typ.items():
        if a in schluessel and str(a) != s:
            fehler.append(f"Alias '{a}' ({t}) kollidiert mit einem Schluessel")
    for t in {e.get("typ") for e in liste if isinstance(e, dict) and isinstance(e.get("typ"), str)}:
        if vorgaben.get(t, 0) != 1:
            fehler.append(f"Typ {t}: {vorgaben.get(t, 0)} Vorgaben, erwartet genau eine")
    return fehler


def dateien_pruefen(eintraege: List[RomEintrag], ordner: Optional[Path] = None) -> List[str]:
    """Jede Datei muss da sein, ``groesse`` Bytes haben und die angegebene MD5 tragen."""
    fehler: List[str] = []
    for e in eintraege:
        p = e.pfad(ordner)
        if p is None:
            fehler.append(f"{e.schluessel}: Datei '{e.datei}' fehlt")
            continue
        roh = p.read_bytes()
        if len(roh) != e.groesse:
            fehler.append(f"{e.schluessel}: {len(roh)} Bytes, im Katalog {e.groesse}")
        if hashlib.md5(roh).hexdigest() != e.md5:
            fehler.append(f"{e.schluessel}: MD5 {hashlib.md5(roh).hexdigest()}, im Katalog {e.md5}")
    return fehler


def _eintrag(e: dict) -> RomEintrag:
    return RomEintrag(
        schluessel=e["schluessel"], typ=e["typ"], titel=e["titel"], datei=e["datei"], groesse=e["groesse"],
        md5=e["md5"], guete=e["guete"], vorgabe=e["vorgabe"], pgm=e.get("pgm", ""),
        schnittstellen=tuple(e.get("schnittstellen", ())), zeichensatz=e.get("zeichensatz", ""),
        hinweis=e.get("hinweis", ""), aliases=tuple(e.get("aliases", ())))


def laden(pfad: Optional[Path] = None) -> List[RomEintrag]:
    """Katalog lesen.  Ohne Datei oder bei Schemafehlern: leere Liste (der Drucker laeuft dann mit dem
    eingebetteten Abbild des Kerns weiter; die Fehler stehen in :func:`fehler`)."""
    eintraege, _ = _laden(pfad)
    return eintraege


def fehler(pfad: Optional[Path] = None) -> List[str]:
    """Die Schemafehler des Katalogs (leer = in Ordnung oder Datei fehlt)."""
    return _laden(pfad)[1]


def _laden(pfad: Optional[Path]) -> Tuple[List[RomEintrag], List[str]]:
    pfad = pfad or paths.printer_roms_file()
    if pfad is None:
        return [], []
    try:
        with open(pfad, "r", encoding="utf-8") as f:
            daten = yaml.safe_load(f)
    except (OSError, yaml.YAMLError) as e:
        return [], [f"{pfad}: {e}"]
    f_ = pruefen(daten)
    if f_:
        return [], f_
    return [_eintrag(e) for e in daten["eintraege"]], []


def fuer_typ(typ: str, eintraege: Optional[List[RomEintrag]] = None) -> List[RomEintrag]:
    """Die Eintraege eines Typs in Katalogreihenfolge."""
    eintraege = laden() if eintraege is None else eintraege
    return [e for e in eintraege if e.typ == typ]


def vorgabe(typ: str, eintraege: Optional[List[RomEintrag]] = None) -> Optional[RomEintrag]:
    return next((e for e in fuer_typ(typ, eintraege) if e.vorgabe), None)


def aufloesen(wahl, typ: str, eintraege: Optional[List[RomEintrag]] = None) -> Optional[RomEintrag]:
    """Schluessel oder Alias -> Eintrag dieses Typs; Unbekanntes -> Vorgabe des Typs (``None`` ohne Katalog)."""
    liste = fuer_typ(typ, eintraege)
    for e in liste:
        if wahl == e.schluessel or wahl in e.aliases:
            return e
    return next((e for e in liste if e.vorgabe), None)

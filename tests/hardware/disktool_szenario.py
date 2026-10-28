"""DiskTool-Szenarien je Dateisystem — dieselben Schritte gegen Datei UND Laufwerk.

Gebraucht von ``tests/hardware/test_h9_disktool_dateisysteme.py`` (echte Diskette) und
``tests/python/test_disktool_physisch_gegenprobe.py`` (Ersatzlaufwerk ueber einer .hfe,
in der Regression).  Kein Geraet, kein Qt — nur ``DiskTool``.

Das Orakel ist das DiskTool selbst an einer DATEIKOPIE desselben Abbilds: dieselben
Aenderungen an Datei und Laufwerk muessen dasselbe ergeben — Verzeichnis, Dateibytes,
Befund der Pruefung und **jeden Sektor** samt Anhang hinter der Daten-CRC (dort fuehrt
UDOS seine Verkettung).
"""

from __future__ import annotations

import os
import shutil
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

PROJECT_ROOT = Path(__file__).resolve().parents[2]
FIXTURES = PROJECT_ROOT / "tests" / "fixtures" / "disks"

#: Feste Zeit: Aenderungszeit der Quelldateien UND Uhr des WEGA-Dateisystems
#: (``K1520_FS_UHR``, Superblock/Verzeichnis-Inodes) — sonst unterschieden sich Orakel
#: und Laufwerk um die Sekunden zwischen den beiden Laeufen.
MTIME = 567_993_600            # 1988-01-01 00:00 UTC
os.environ.setdefault("K1520_FS_UHR", str(MTIME))


def muster(n: int, saat: int) -> bytes:
    """*n* Byte, je Saat verschieden, Vielfaches von 128 (CP/M fuellt sonst auf)."""
    return bytes(((i * 131 + saat * 17 + (i >> 7)) & 0xFF) for i in range(n))


@dataclass
class Szenario:
    """Ein Dateisystem, sein Abbild und die Aenderungen, die daran geprueft werden."""
    name: str
    abbild: Path                        # .hfe (Aufspieler und Ersatzlaufwerk lesen HFE)
    filesystem: str
    schritte: Callable                  # (DiskTool, arbeitsordner) -> None
    #: Namen, die nach den Schritten da sein bzw. fehlen muessen (Pruefung der Pruefung).
    erwartet_da: list = field(default_factory=list)
    erwartet_weg: list = field(default_factory=list)


def _quelle(ordner: Path, name: str, inhalt: bytes) -> Path:
    p = ordner / name.replace("/", "_")
    p.write_bytes(inhalt)
    os.utime(p, (MTIME, MTIME))
    return p


# ── CP/A ────────────────────────────────────────────────────────────────────

def _cpa(d, ordner: Path) -> None:
    d.insert(_quelle(ordner, "KLEIN.TXT", b"K1520 HW\r\n" + b"\x1a" * 118), "KLEIN.TXT")
    d.insert(_quelle(ordner, "MITTEL.DAT", muster(3072, 1)), "MITTEL.DAT")
    d.insert(_quelle(ordner, "GROSS.DAT", muster(40_960, 2)), "GROSS.DAT")   # > 16 KiB: 3 Extents
    d.insert(_quelle(ordner, "MITTEL.DAT", muster(5120, 3)), "MITTEL.DAT", overwrite=True)
    d.erase("KLEIN.TXT")
    d.set_cpm_attrs("GROSS.DAT", read_only=True)


# ── UDOS (Verkettung im Gap hinter der Daten-CRC) ───────────────────────────

def _udos_beiblatt(ordner: Path, name: str, vorlage: str) -> None:
    zeilen = [z for z in vorlage.splitlines() if z and not z.startswith("#")]
    zeilen = [f"name={name}" if z.startswith("name=") else z for z in zeilen]
    (ordner / f"{name}.fileinfo").write_text("\n".join(zeilen) + "\n", encoding="utf-8")


def _udos(d, ordner: Path) -> None:
    # Vorlage fuer die Kopfsektorangaben: das erste Programm (Typ P) der ersten Seite.
    vorlage_ordner = ordner / "_vorlage"
    vorlage_ordner.mkdir(exist_ok=True)
    prog = next(e for e in d.list() if e.type == "P")
    seite = prog.side_prefix                        # 'Side0/' oder ''
    d.extract(seite + prog.name, vorlage_ordner / "vorlage")
    vorlage = (vorlage_ordner / "vorlage.fileinfo").read_text(encoding="utf-8")
    for name, inhalt in (("HWKLEIN", muster(256, 4)), ("HWGROSS", muster(20_480, 5))):
        _quelle(ordner, name, inhalt)
        _udos_beiblatt(ordner, name, vorlage)
        d.insert(ordner / name, f"{seite}{name}")
    _quelle(ordner, "HWKLEIN", muster(3072, 6))      # ueberschreiben, laenger als vorher
    d.insert(ordner / "HWKLEIN", f"{seite}HWKLEIN", overwrite=True)
    d.erase(seite + prog.name)
    d.set_udos_attrs(f"{seite}HWGROSS", properties="W")


# ── WEGA (UNIX-Dateisystem des P8000) ───────────────────────────────────────

def _wega(d, ordner: Path) -> None:
    d.mkdir("hwtest/unter")
    d.insert(_quelle(ordner, "klein", muster(100 * 128, 7)[:1000]), "hwtest/klein")
    # > 10 Bloecke a 512: einfach indirekt
    d.insert(_quelle(ordner, "gross", muster(80 * 1024, 8)), "hwtest/unter/gross")
    d.insert(_quelle(ordner, "klein", muster(6144, 9)), "hwtest/klein", overwrite=True)
    opfer = next(e for e in d.list() if not e.name.startswith("hwtest") and e.size > 0
                 and e.type != "d")
    d.erase(opfer.side_prefix + opfer.name)
    d.set_unix_attrs("hwtest/unter/gross", mode=0o600, mtime=MTIME)


def szenarien(abbilder: Path) -> list:
    """Die drei Szenarien.  *abbilder*: Ordner fuer umgewandelte Abbilder (.dmk -> .hfe)."""
    return [
        Szenario("cpa", FIXTURES / "cpa_cpa780_k5601_clock.hfe", "cpa780", _cpa,
                 erwartet_da=["MITTEL.DAT", "GROSS.DAT"], erwartet_weg=["KLEIN.TXT"]),
        # Nicht udos_boot_scp.hfe: dem Mitschnitt fehlt Sektor 13 auf Spur 51/0, und genau
        # dorthin legt der Belegungsplan die erste neue Datei (das DiskTool verweigert dann
        # zu Recht die ganze Operation).
        Szenario("udos", PROJECT_ROOT / "disks/a5120_udos43/a5120_udos43_k5601_entwickler.hfe",
                 "udos_ds77", _udos, erwartet_da=["HWKLEIN", "HWGROSS"]),
        Szenario("wega", als_hfe(PROJECT_ROOT / "disks/p8000_wega30/p8000_wega30_doc1.dmk",
                                 abbilder / "wega_doc1.hfe"),
                 "wega720", _wega,
                 erwartet_da=["hwtest/klein", "hwtest/unter/gross"]),
    ]


def als_hfe(quelle: Path, ziel: Path) -> Path:
    """Ein Abbild in .hfe umwandeln (einmal je Lauf)."""
    if quelle.suffix == ".hfe":
        return quelle
    if not ziel.exists():
        from app.core_binding.k1520disk import DiskTool
        ziel.parent.mkdir(parents=True, exist_ok=True)
        with DiskTool.open(quelle) as d:
            d.save_as(ziel)
    return ziel


# ── Befund ──────────────────────────────────────────────────────────────────

def verzeichnis(d) -> dict:
    """``{name: (size, type, attrs, user)}`` — was ein Anwender in der Liste sieht."""
    return {e.side_prefix + e.name: (e.size, e.type, e.attrs, e.user) for e in d.list()}


def dateien(d, ordner: Path) -> dict:
    """Alle regulaeren Dateien herausholen: ``{name: bytes}``."""
    ordner.mkdir(parents=True, exist_ok=True)
    out = {}
    for i, e in enumerate(d.list()):
        if e.type == "d":
            continue
        ziel = ordner / f"{i:04d}"
        d.extract(e.side_prefix + e.name, ziel)
        out[e.side_prefix + e.name] = ziel.read_bytes()
    return out


def sektoren(d) -> dict:
    """``{(cyl, head, n): (id, cyl_id, head_id, daten, anhang, crc)}`` ueber die ganze Diskette."""
    out = {}
    cyls, heads = d.medium_cylinders, d.medium_heads
    for c in range(cyls):
        for h in range(heads):
            t = d.track(c, h)
            n = 0
            for sp in t.spans:
                if sp.index < 0:
                    continue
                out[(c, h, n)] = (sp.id, sp.cyl, sp.head, d.sector_data(c, h, sp.index),
                                  d.sector_tail(c, h, sp.index), sp.data_crc_ok)
                n += 1
    return out


def befund(d, voll: bool = True) -> list:
    """Kennungen der Pruefung (``fsck``), sortiert — ein Vertrag, vergleichbar."""
    return sorted(f.id for f in d.check(voll=voll).findings)


def anwenden(d, sz: Szenario, ordner: Path) -> None:
    ordner.mkdir(parents=True, exist_ok=True)
    sz.schritte(d, ordner)
    d.flush()


def datei_orakel(sz: Szenario, ordner: Path):
    """Dasselbe Szenario an einer Dateikopie.  Returns: (vorher, nachher) als Befund-dicts."""
    from app.core_binding.k1520disk import DiskTool
    kopie = ordner / f"orakel_{sz.name}{sz.abbild.suffix}"
    shutil.copy(sz.abbild, kopie)
    with DiskTool.open(kopie, read_only=False) as d:
        vorher = aufnahme(d, ordner / "orakel_vorher")
        anwenden(d, sz, ordner / "orakel_quellen")
    with DiskTool.open(kopie) as d:
        nachher = aufnahme(d, ordner / "orakel_nachher")
    return vorher, nachher


def aufnahme(d, ordner: Path) -> dict:
    """Alles, was verglichen wird, in einem dict."""
    return dict(format=d.format, filesystem=d.filesystem, verzeichnis=verzeichnis(d),
                dateien=dateien(d, ordner), befund=befund(d), sektoren=sektoren(d))


def unterschiede(a: dict, b: dict) -> list:
    """Lesbare Liste der Abweichungen zweier Aufnahmen (leer = gleich)."""
    out = []
    for k in ("format", "filesystem", "befund"):
        if a[k] != b[k]:
            out.append(f"{k}: {a[k]!r} != {b[k]!r}")
    va, vb = a["verzeichnis"], b["verzeichnis"]
    for n in sorted(set(va) | set(vb)):
        if va.get(n) != vb.get(n):
            out.append(f"Verzeichnis {n}: {va.get(n)} != {vb.get(n)}")
    da, db = a["dateien"], b["dateien"]
    for n in sorted(set(da) | set(db)):
        if da.get(n) != db.get(n):
            out.append(f"Datei {n}: {len(da.get(n) or b'')} B != {len(db.get(n) or b'')} B")
    sa, sb = a["sektoren"], b["sektoren"]
    abweichend = [k for k in sorted(set(sa) | set(sb)) if sa.get(k) != sb.get(k)]
    if abweichend:
        out.append(f"{len(abweichend)} Sektoren verschieden, erste: {abweichend[:8]}")
    return out


# ── Grenzfall: kein Platz ───────────────────────────────────────────────────

def zielname(d, sz: Szenario, basis: str) -> str:
    """Ein neuer Dateiname, wie das Dateisystem ihn erwartet."""
    if sz.name == "cpa":
        return f"{basis}.DAT"
    if sz.name == "udos":
        return d.list()[0].side_prefix + basis
    return basis.lower()


def zu_gross(d, sz: Szenario, ordner: Path) -> Path:
    """Eine Quelldatei, die auf keine der Disketten passt (1 MiB), samt UDOS-Beiblatt."""
    ordner.mkdir(parents=True, exist_ok=True)
    name = zielname(d, sz, "RIESIG").split("/")[-1]
    quelle = _quelle(ordner, name, muster(1 << 20, 10))
    if sz.name == "udos":
        prog = next(e for e in d.list() if e.type == "P")
        v = ordner / "_vorlage"
        v.mkdir(exist_ok=True)
        d.extract(prog.side_prefix + prog.name, v / "vorlage")
        _udos_beiblatt(ordner, name, (v / "vorlage.fileinfo").read_text(encoding="utf-8"))
    return quelle

"""MicroDOS 1715 (Rostocker Anpassung, CP/M-2.2-kompatibel) am PC 1715.

Waechter fuer doc/merkposten/microdos1715.md: die ausgelieferte Systemdiskette (vom DiskTool gebaut:
`create --boot` mit Bootkopf + MICRODOS.SYS) bootet bis "A>", und tools/microdos/ablauf1715.py baut aus
einer Leerdiskette (FORMAT15 -> MSYSG15A) eine selbst bootende Systemdiskette.
"""
import os
import shutil
import sys

from conftest import PROJECT_ROOT, requires_core

pytestmark = requires_core

sys.path.insert(0, str(PROJECT_ROOT / "tools" / "microdos"))
import ablauf1715  # noqa: E402

SYSTEMDISKETTE = PROJECT_ROOT / "disks" / "pc1715_microdos" / "pc1715_microdos_k5601_system.hfe"


def test_systemdiskette_bootet_bis_zum_prompt(tmp_path):
    a = tmp_path / "a.hfe"
    shutil.copy(SYSTEMDISKETTE, a)
    s = ablauf1715.Sitzung(str(a))
    s.kaltstart_bis_prompt()
    verz = s.kommando("D")
    assert "MICRODOS.SYS" in verz and "FORMAT15.COM" in verz and "MSYSG15A.COM" in verz, verz


def test_leerdiskette_formatieren_generieren_und_booten(tmp_path):
    ziel = ablauf1715.bauen(str(tmp_path), mit_bedienprobe=True)
    assert os.path.getsize(ziel) > 1_000_000
    # Die neue Diskette traegt MICRODOS.SYS (und die DIR.TXT der Bedienprobe), aber keine Hilfsprogramme
    verz = ablauf1715.probe_kaltstart(ziel)
    assert "MICRODOS.SYS" in verz and "FORMAT15" not in verz, verz

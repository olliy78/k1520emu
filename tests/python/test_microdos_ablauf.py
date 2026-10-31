"""MicroDOS 1526 (Rostocker Anpassung, CP/M-2.2-kompatibel) am A5120.

Waechter fuer doc/merkposten/microdos.md: die ausgelieferten Systemdisketten booten mit der dort
genannten Bestueckung bis "A>", und tools/microdos/ablauf.py baut aus einer Leerdiskette
(FORMAT20 -> MSYSG20) eine selbst bootende Systemdiskette.
"""
import os
import shutil
import sys

import pytest

from conftest import PROJECT_ROOT, requires_core

pytestmark = requires_core

sys.path.insert(0, str(PROJECT_ROOT / "tools" / "microdos"))
import ablauf  # noqa: E402

ORDNER = PROJECT_ROOT / "disks" / "a5120_microdos1526"

# (Datei, Tastatur, Laufwerke)
FAELLE = [
    ("a5120_microdos26_k5601_seriell.hfe", "k7637", None),
    ("a5120_microdos26_k5601_parallel.hfe", "k7636", None),
    ("a5120_microdos26_k5600_10_seriell.hfe", "k7637", ["K5600.10"] * 2 + [None, None]),
    ("a5120_microdos26_k5600_10_parallel.hfe", "k7636", ["K5600.10"] * 2 + [None, None]),
]


@pytest.mark.parametrize("datei,tastatur,lw", FAELLE, ids=[f[0] for f in FAELLE])
def test_systemdiskette_bootet_bis_zum_prompt(tmp_path, datei, tastatur, lw):
    a = tmp_path / "a.hfe"
    shutil.copy(ORDNER / datei, a)
    s = ablauf.Sitzung(tastatur, str(a), laufwerke=lw)
    s.kaltstart_bis_prompt()
    verz = s.kommando("D")
    assert "MicroDOS.SYS" in verz and "FORMAT20.COM" in verz, verz


def test_leerdiskette_formatieren_generieren_und_booten(tmp_path):
    ziel = ablauf.bauen(str(tmp_path), tastatur="k7637")
    assert os.path.getsize(ziel) > 1_000_000
    # Die neue Diskette traegt MicroDOS.SYS und sonst nichts
    verz = ablauf.probe_kaltstart(ziel, "k7637")
    assert "MicroDOS.SYS" in verz and "FORMAT20" not in verz, verz

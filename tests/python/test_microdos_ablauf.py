"""MicroDOS 1526 (Rostocker Anpassung, CP/M-2.2-kompatibel) am A5120.

Waechter fuer doc/merkposten/microdos.md: die ausgelieferten Systemdisketten (disks/a5120_microdos1526/, gebaut von
tools/microdos/build.py aus tools/microdos/original/) booten mit der dort genannten Bestueckung bis "A>" und tragen
ihre Beigaben, `build.py --check` haelt sie auf dem Stand des Baus (Erkennung microdos800/200, fsck ohne Warnung),
und tools/microdos/ablauf.py baut aus einer Leerdiskette (FORMAT20 -> MSYSG20) eine selbst bootende Systemdiskette.
"""
import os
import shutil
import sys

import pytest

from conftest import PROJECT_ROOT, requires_core

pytestmark = requires_core

sys.path.insert(0, str(PROJECT_ROOT / "tools" / "microdos"))
import ablauf  # noqa: E402
import build  # noqa: E402

ORDNER = PROJECT_ROOT / "disks" / "a5120_microdos1526"   # gebaut (build.py)

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
    assert "MicroDOS.SYS" in verz and "FORMAT20" in verz and "MSYSG20" in verz, verz
    # INITIAL.SUB fehlt absichtlich: kein MKEY20-Halt, der Prompt kam ohne Return
    assert "INITIAL" not in verz, verz
    if "k5601" in datei:
        assert "DIP     .COM" in verz, verz


DISKTOOL = PROJECT_ROOT / "build" / "k1520disktool"


@pytest.mark.skipif(not DISKTOOL.exists(), reason="build/k1520disktool fehlt")
def test_build_check_haelt_die_disketten_auf_dem_stand_des_baus(capsys):
    """`build.py --check`: byteglich zum Bau aus original/, Erkennung, fsck ohne Warnung/Fehler/Gefahr.

    Schlaegt an, sobald sich Bau (ablauf.py/build.py), Originale oder Emulatorverhalten aendern -- dann
    `python3 -I tools/microdos/build.py --tool build/k1520disktool` neu laufen lassen und die .hfe committen."""
    ok = build.pruefen(str(PROJECT_ROOT / "disks"), [str(DISKTOOL)])
    assert ok, capsys.readouterr().out


def test_leerdiskette_formatieren_generieren_und_booten(tmp_path):
    ziel = ablauf.bauen(str(tmp_path), tastatur="k7637")
    assert os.path.getsize(ziel) > 1_000_000
    # Die neue Diskette traegt MicroDOS.SYS und sonst nichts
    verz = ablauf.probe_kaltstart(ziel, "k7637")
    assert "MicroDOS.SYS" in verz and "FORMAT20" not in verz, verz

"""Waechter fuer die Aenderungs-Pipeline von mutos_io (Entwurf 40, M5/M6):

Quelle aendern -> Datei + .fileinfo bauen -> Ladediskette bauen -> im Emulator booten -> die
Aenderung ist auf dem Bildschirm zu sehen.  Das ist der Weg, den man bei jeder Aenderung an
tools/mutos8000/mutos_io/src/mutosio.mac geht (README dort).

Uebersprungen, wenn die CP/M-Werkzeugkette (CPA_Workbench) oder build/k1520dbg / k1520disktool fehlen."""

import os
import subprocess
import sys

import pytest

from conftest import PROJECT_ROOT

IO = PROJECT_ROOT / "tools" / "mutos8000" / "mutos_io"
DBG = PROJECT_ROOT / "build" / "k1520dbg"
DISKTOOL = PROJECT_ROOT / "build" / "k1520disktool"

sys.path.insert(0, str(PROJECT_ROOT / "tools"))
import cpm_bau  # noqa: E402

pytestmark = [
    pytest.mark.skipif(not cpm_bau.werkzeugkette_da(), reason="CP/M-Werkzeugkette (CPA_Workbench) fehlt"),
    pytest.mark.skipif(not (DBG.exists() and DISKTOOL.exists()), reason="k1520dbg/k1520disktool nicht gebaut"),
]


def test_aenderung_der_quelle_erscheint_auf_dem_bildschirm(tmp_path):
    text = (IO / "src" / "mutosio.mac").read_text()
    alt = "DB\t'remove mutos8000 boot - disk'"
    assert text.count(alt) == 1
    quelle = tmp_path / "mutosio.mac"
    quelle.write_text(text.replace(alt, "DB\t'REMOVE mutos8000 boot - disk'"))   # gleiche Laenge
    hfe = tmp_path / "ladediskette.hfe"
    env = dict(os.environ, K1520_DISKTOOL=str(DISKTOOL))
    r = subprocess.run([sys.executable, "-I", str(IO / "ladediskette.py"), "diskette", str(hfe),
                        "--laufwerke", "k5601", "--quelle", str(quelle)],
                       capture_output=True, text=True, timeout=600, env=env)
    assert r.returncode == 0, r.stdout + r.stderr
    skript = "g 80000000\nkeys BOOT\\sMUTOS\\r\ng 100000000\nscreen\nq\n"
    r = subprocess.run([str(DBG), "--em", "em256", "--laufwerke", "K5601,K5601,K5601", str(hfe)],
                       input=skript, capture_output=True, text=True, timeout=900)
    ausgabe = r.stdout + r.stderr      # der Debugger schreibt den Bildschirm auf stderr
    assert "REMOVE mutos8000 boot - disk" in ausgabe, ausgabe[-2000:]
    assert "remove mutos8000 boot - disk" not in ausgabe

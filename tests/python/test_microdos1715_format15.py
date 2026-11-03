"""MicroDOS 1715: FORMAT15-Menueeintraege 3, 5, 1 auf einer echten Leerdiskette in B: (K5601).

FORMAT15: Laufwerk B, Format 0-2 einseitig (16x256, 9x512, 5x1024), 3-5 doppelseitig, je 80 Spuren; danach Neustart.
Doppelseitige Formate liest das Gastsystem nach SELDSK15 (Liste 6/7) zurueck (D B: = no files).  BEFUND: nach SELDSK15 mit einem
EINSEITIGEN 80-Spur-Format (Liste 4/5) haengt jedes D B: (kein Ergebnis, kein Fehler, > 1 Mrd. Takte); dort wird deshalb nur
das Formatieren selbst (FORMATTING COMPLETE) und die DiskTool-Erkennung geprueft.
Merkposten: doc/merkposten/microdos1715.md ("FORMAT15-Menueeintraege").  Aufteilung auf zwei Module wegen der 300-s-Grenze.
"""
import os
import shutil
import subprocess
import sys

import pytest

from conftest import PROJECT_ROOT, requires_core

pytestmark = requires_core

sys.path.insert(0, str(PROJECT_ROOT / "tools" / "microdos"))
import ablauf1715  # noqa: E402

ORIGINAL = PROJECT_ROOT / "disks" / "pc1715_microdos" / "pc1715_microdos_k5601_system.hfe"
DISKTOOL = PROJECT_ROOT / "build" / "k1520disktool"

# (FORMAT15-Eintrag, SELDSK15-Format fuer die Rueckprobe im Gast oder None, DiskTool-Dateisystem oder None)
FAELLE = [("3", "6", "scpx640"), ("5", "7", "cpa800"), ("1", None, None)]


@pytest.mark.parametrize("eintrag,sfmt,erwartet", FAELLE, ids=["Eintrag" + f[0] for f in FAELLE])
def test_format15_menueeintrag(tmp_path, eintrag, sfmt, erwartet):
    a = tmp_path / "a.hfe"
    b = tmp_path / "b.hfe"
    shutil.copy(ORIGINAL, a)
    s = ablauf1715.Sitzung(str(a), str(b))
    s.kaltstart_bis_prompt()
    ablauf1715.formatiere(s, format_nr=eintrag)            # endet mit "FORMATTING COMPLETE" und Neustart
    if sfmt:
        ablauf1715.seldsk15(s, "B", "2", "3", sfmt)
        assert "no files" in s.kommando("D B:")
    s.lauf(10_000_000)
    s.emu.flush_disks()
    if erwartet and DISKTOOL.exists():
        env = dict(os.environ, LD_LIBRARY_PATH=str(PROJECT_ROOT / "build"))
        r = subprocess.run([str(DISKTOOL), "ls", str(b)], capture_output=True, text=True, env=env)
        assert erwartet in (r.stdout + r.stderr).split("\n")[0], r.stdout + r.stderr

"""MicroDOS 1526: jeder Menueeintrag von FORMAT20 auf einer echten Leerdiskette (Gegenstueck zur format_matrix der CP/A-Welt).

Je Laufwerkstyp der Generierung (SELDSK20: MFS 1.2/1.4/1.6, MF 3200, MF 6400) steht das Formatmenue fest; der Emulator
bekommt das passende Laufwerksprofil fuer B:.  Geprueft wird: FORMAT20 meldet "Formatierung beendet", das Gastsystem liest die
Diskette danach mit SELDSK20-Format (D B: = no files), und das DiskTool erkennt die Geometrie (soweit eindeutig).
Merkposten: doc/merkposten/microdos.md ("FORMAT20-Menues").
"""
import os
import shutil
import subprocess
import sys

import pytest

from conftest import PROJECT_ROOT, requires_core

pytestmark = requires_core

sys.path.insert(0, str(PROJECT_ROOT / "tools" / "microdos"))
import ablauf  # noqa: E402

DISKTOOL = PROJECT_ROOT / "build" / "k1520disktool"
QUELLE = os.path.join(ablauf.ORIGINALE, "MicroDOS1526s.hfe")
HW = {1: "K5600.10", 2: "K5600.20", 3: "K5601", 4: "MF3200", 5: "MF6400"}

# (Laufwerkstyp, FORMAT20-Eintrag, SELDSK20-Formatnummer oder None, Text im Menue, DiskTool-Dateisystem oder None)
FAELLE = [
    (1, 1, 1, "26 *  128 * 40 * 1", None), (1, 2, 2, "16 *  256 * 40 * 1", None), (1, 3, 4, "5 * 1024 * 40 * 1", None),
    (2, 1, 5, "16 *  256 * 80 * 1", None), (2, 2, None, "9 *  512 * 80 * 1", None), (2, 3, 6, "5 * 1024 * 80 * 1", None),
    (3, 1, 7, "16 *  256 * 80 * 2", "scpx640"), (3, 2, None, "9 *  512 * 80 * 2", None), (3, 3, 8, "5 * 1024 * 80 * 2", "cpa800"),
    (4, 1, 1, "26 *  128 * 77 * 1", None), (4, 3, 2, "4 * 1024 * 77 * 1", None),
    (5, 1, 1, "26 *  128 * 77 * 1", None), (5, 2, 2, "4 * 1024 * 77 * 1", None),
    # BEFUND: 8 x 1024 x 77 (8"-MFM) wird bis etwa Spur 52 formatiert, danach meldet FORMAT20 "Defekte Spuren !"
    (5, 3, 4, "8 * 1024 * 77 * 1", "DEFEKT"),
]


def _ids(f):
    return "%s-Eintrag%d" % (HW[f[0]], f[1])


@pytest.mark.parametrize("typ,eintrag,sfmt,menuetext,erwartet", FAELLE, ids=[_ids(f) for f in FAELLE])
def test_format20_menueeintrag(tmp_path, typ, eintrag, sfmt, menuetext, erwartet):
    a = tmp_path / "a.hfe"
    b = tmp_path / "b.hfe"
    shutil.copy(QUELLE, a)
    s = ablauf.Sitzung("k7637", str(a), str(b), laufwerke=["K5601", HW[typ], None, None])
    s.kaltstart_bis_prompt()
    s.tippe("SELDSK20\r")
    for erwartet_text, antwort in (("Welches logische", "B"), ("physische", "2"), ("Laufwerkstyp", str(typ)),
                                   ("Formate sind", str(sfmt or 1))):
        assert s.bis(erwartet_text), s.bild()
        s.tippe(antwort)
        s.lauf(3_000_000)
    assert s.bis("Wiederholung"), s.bild()
    s.tippe("N")
    assert s.bis_prompt()
    s.tippe("FORMAT20\r")
    assert s.bis("Auf welchem Laufwerk")
    s.tippe("B")
    assert s.bis("Wie soll die")
    assert menuetext in s.bild(), s.bild()                 # der Menueeintrag steht so im Menue
    s.tippe("%d\r" % eintrag)
    assert s.bis("Richtig (J/N)")
    s.tippe("J")
    if erwartet == "DEFEKT":
        assert s.bis("Defekte Spuren", frist=900_000_000), s.bild()
        return
    assert s.bis("Formatierung beendet", frist=900_000_000), s.bild()
    assert "Bad" not in s.bild() and "Defekte" not in s.bild(), s.bild()
    s.tippe("N")
    assert s.bis_prompt()
    if sfmt:
        assert "no files" in s.kommando("D B:")           # das Gastsystem liest die frisch formatierte Diskette
    s.lauf(10_000_000)
    s.emu.flush_disks()
    if erwartet and DISKTOOL.exists():
        env = dict(os.environ, LD_LIBRARY_PATH=str(PROJECT_ROOT / "build"))
        r = subprocess.run([str(DISKTOOL), "ls", str(b)], capture_output=True, text=True, env=env)
        assert erwartet in (r.stdout + r.stderr).split("\n")[0], r.stdout + r.stderr

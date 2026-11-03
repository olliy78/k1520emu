"""MicroDOS 1526: MSYSG20 fuer andere Bildschirm-/Tastatur-/Sektorgroessen-Bestueckung generieren und booten (Merkposten microdos.md).

Jede Diskette entsteht wie beim Anwender (FORMAT20 -> MSYSG20 auf einer Leerdiskette, ablauf.bauen).
"""
import os
import shutil
import subprocess
import sys
import time

import pytest

from conftest import PROJECT_ROOT, requires_core

pytestmark = requires_core

sys.path.insert(0, str(PROJECT_ROOT / "tools" / "microdos"))
import ablauf  # noqa: E402

DISKTOOL = PROJECT_ROOT / "build" / "k1520disktool"
braucht_disktool = pytest.mark.skipif(not DISKTOOL.exists(), reason="build/k1520disktool fehlt")



def _bauen(tmp_path, name, **gen):
    return ablauf.bauen(str(tmp_path), tastatur="k7637", probe=False, generierung=gen, dateiname=name + ".hfe")


def _boot(tmp_path, pfad, tastatur="k7637", **kw):
    a = tmp_path / "boot.hfe"
    shutil.copy(pfad, a)
    s = ablauf.Sitzung(tastatur, str(a), **kw)
    s.kaltstart_bis_prompt()
    return s


# ── Bildschirm und Tastatur ──────────────────────────────────────────────────

def test_bildschirm_k7023_1k_bootet_mit_der_option_bildschirm_k7023(tmp_path):
    pfad = _bauen(tmp_path, "k7023", bildschirm="1")
    s = _boot(tmp_path, pfad, konfig={"bildschirm": "k7023"})
    assert "48K MicroDOS Vers 2.6" in s.bild()
    assert "MicroDOS.SYS" in s.kommando("D")


def test_tastatur_k7636_generiert_bootet_und_nimmt_eingaben_an(tmp_path):
    pfad = _bauen(tmp_path, "k7636", tastatur="k7636")
    s = _boot(tmp_path, pfad, tastatur="k7636")
    assert "MicroDOS.SYS" in s.kommando("D")


def test_tastatur_k7634_wird_generiert_aber_der_emulator_hat_keine_k7634(tmp_path):
    """NICHT MODELLIERT: K7634.54/.51 gibt es am A5120 im Emulator nicht (nur K7637, K7636).  Das erzeugte System bootet,
    die Eingabe geht nicht durch (der Schirm fuellt sich mit Prompts)."""
    pfad = _bauen(tmp_path, "k7634", tastatur="k7634_54")
    assert pfad.endswith("k7634.hfe") and os.path.getsize(pfad) > 1_000_000
    a = tmp_path / "b.hfe"
    shutil.copy(pfad, a)
    s = ablauf.Sitzung("k7637", str(a))
    assert s.bis("MicroDOS Vers", frist=400_000_000)
    s.lauf(30_000_000)
    assert s.bild().count("A>") > 5, s.bild()


# ── 128-Byte-Sektoren (Lade-ROM V0.6, msysgen.txt Anhang B) ──────────────────

def test_systemdiskette_mit_128_byte_sektoren_bootet_mit_dem_lade_rom_v06(tmp_path):
    """K5600.10-Laufwerke: FORMAT20 Eintrag 1 (26 x 128 x 40 x 1), SELDSK20 B auf dasselbe Format, MSYSG20 mit Format 1
    fuer A und B -> MicroDOS.SYS auf der 128-Byte-Diskette; Kaltstart mit Lade-ROM V0.6 (und dem Vorgabe-ROM)."""
    lw = ["K5600.10"] * 2 + [None, None]
    a = tmp_path / "a.hfe"
    b = tmp_path / "b.hfe"
    shutil.copy(ablauf.ORIGINALE + "/MicroDOS1526s_5600.hfe", a)
    s = ablauf.Sitzung("k7637", str(a), str(b), laufwerke=lw)
    s.kaltstart_bis_prompt()
    ablauf.formatiere(s, "B", format_nr="1")
    s.tippe("SELDSK20\r")
    for erwartet, antwort in (("Welches logische", "B"), ("physische", "2"), ("Laufwerkstyp", "1"), ("Formate sind", "1")):
        assert s.bis(erwartet)
        s.tippe(antwort)
        s.lauf(3_000_000)
    assert s.bis("Wiederholung"); s.tippe("N")
    assert s.bis_prompt()
    ablauf.generiere(s, laufwerke=[("A", "1", "1", "1"), ("B", "2", "1", "1")])
    assert "MicroDOS.SYS" in s.kommando("D B:")
    s.lauf(10_000_000)
    s.emu.flush_disks()
    for laderom in ("v06", None):
        k = tmp_path / ("k_%s.hfe" % laderom)
        shutil.copy(b, k)
        t = ablauf.Sitzung("k7637", str(k), laufwerke=lw, laderom=laderom)
        t.kaltstart_bis_prompt()
        assert "MicroDOS.SYS" in t.kommando("D")
    # DiskTool: Profil microdos128 (positiver Nachweis = SYL-Lader 0.6), lesen, schreiben, pruefen -- die Diskette bootet danach
    if not DISKTOOL.exists():
        return
    env = dict(os.environ, LD_LIBRARY_PATH=str(PROJECT_ROOT / "build"))

    def tool(*a):
        r = subprocess.run([str(DISKTOOL), *map(str, a)], capture_output=True, text=True, env=env)
        return r.returncode, r.stdout + r.stderr

    rc, aus = tool("info", b)
    assert rc == 0 and "microdos128" in aus, aus
    rc, aus = tool("ls", "-l", b)
    assert "MicroDOS.SYS" in aus and "16384" in aus, aus
    rc, aus = tool("fsck", b, "--full")
    assert rc == 0 and "ohne Befund" in aus and "cpm.system" not in aus, aus
    # Bootabbild lesen (Lader 256 B + Systemdatei) geht; schreiben ist hier nicht moeglich (Lage nur gemessen)
    assert tool("boot-get", b, tmp_path / "boot128.bin")[0] == 0
    assert (tmp_path / "boot128.bin").stat().st_size == 256 + 16384
    # put: Block 18 (von MSYSG20 nicht eingetragen, aber Systembereich) wird nicht vergeben
    daten = tmp_path / "GROSS.DAT"
    daten.write_bytes(bytes(range(256)) * 200)                  # 50 KB
    c = tmp_path / "c.hfe"
    shutil.copy(b, c)
    assert tool("put", c, daten)[0] == 0
    t = ablauf.Sitzung("k7637", str(c), laufwerke=lw, laderom="v06")
    t.kaltstart_bis_prompt()
    bild = t.kommando("D")
    assert "MicroDOS.SYS" in bild and "GROSS" in bild, bild
    assert tool("fsck", c, "--full")[1].count("Fehler") == 0

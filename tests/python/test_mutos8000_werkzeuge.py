"""Waechter fuer tools/mutos8000/ (Entwurf 39, 4.7.6, 4.8): die Auswertung von M8KROOT bleibt lauffaehig
und liefert den dokumentierten Befund; das eingecheckte Quell-Archiv der rekonstruierten Wurzel baut
dieselbe Diskette (bis auf die Anlagezeit), und eine getauschte Datei im Archiv landet auf der Diskette."""

import os
import shutil
import subprocess
import sys
import zipfile

import pytest

from conftest import PROJECT_ROOT

TOOLS = PROJECT_ROOT / "tools" / "mutos8000"
DISKS = PROJECT_ROOT / "disks" / "a5120_mutos8000"
ARCHIV = DISKS / "a5120_mutos8000_m8k_root.zip"
WURZEL = DISKS / "a5120_mutos8000_m8k_root_rekonstruiert.hfe"
DISKTOOL = PROJECT_ROOT / "build" / "k1520disktool"

braucht_disktool = pytest.mark.skipif(not DISKTOOL.exists(), reason="k1520disktool nicht gebaut")


def lauf(skript, *args):
    r = subprocess.run([sys.executable, "-I", str(TOOLS / skript), *args],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stdout + r.stderr
    return r.stdout


def test_m8kroot_befund_stimmt_mit_dem_entwurf_ueberein():
    out = lauf("m8kroot_lesen.py")
    assert "isize 15 fsize 250" in out
    assert "inodes belegt 45" in out
    assert "root mode 40777" in out
    assert "/etc/passwd" in out and "VOLL" in out


def test_skripte_zeigen_ihre_hilfe():
    for s in ("m8kroot_lesen.py", "wurzel_quellbaum.py", "wurzel_pruefen.py", "bau_wurzel.py",
              "bau_aus_archiv.py", "archiv_erzeugen.py"):
        assert "usage" in lauf(s, "--help")


# ─── Quell-Archiv ────────────────────────────────────────────────────────────

def test_archiv_enthaelt_bauangaben_ladecode_und_den_ganzen_baum():
    with zipfile.ZipFile(ARCHIV) as z:
        namen = z.namelist()
        # ohne Abbild: ein angepasstes Archiv soll nicht neben einem veralteten Abbild stehen
        assert not any(n.endswith(".hfe") for n in namen)
        angaben = z.read("bauangaben.yaml").decode("utf-8")
        assert len(z.read("zylinder0.bin")) == 26 * 128
        blatt = z.read("dateien/wega-dateiangaben.txt").decode("utf-8")
        assert "dateien/dev/" in namen            # leeres Verzeichnis bleibt sichtbar
    assert "fs: mutos8k_fm" in angaben and "inodes: 104" in angaben
    assert "bloecke: 250" in angaben and "zylinder0: zylinder0.bin" in angaben
    assert "AUSLAGERUNGSBEREICH" in angaben       # der Kommentar zu 250 ist der Sinn der Zahl
    zeilen = [z for z in blatt.splitlines() if z and not z.startswith("#")]
    # 19 Geraeteknoten (c/b mit rdev), Wurzel ".", Rechte inkl. uid/gid/mtime je Zeile
    assert sum(" rdev=" in z for z in zeilen) == 19
    assert any(z.startswith("dev/rk2 art=b ") and "rdev=4,2" in z for z in zeilen)
    assert any(z.startswith(". art=d ") for z in zeilen)
    assert all("mtime=" in z and "uid=" in z and "gid=" in z for z in zeilen)


def test_bauangaben_werden_geprueft(tmp_path):
    sys.path.insert(0, str(TOOLS))
    import bau_aus_archiv
    f = tmp_path / "bauangaben.yaml"
    f.write_text("fs: mutos8k_fm\ninodes: viele\nbloecke: 250\nzylinder0: z.bin\n")
    with pytest.raises(SystemExit, match="Zahl"):
        bau_aus_archiv.bauangaben_lesen(str(f))
    f.write_text("fs: mutos8k_fm\n")
    with pytest.raises(SystemExit, match="fehlt"):
        bau_aus_archiv.bauangaben_lesen(str(f))


# ─── Bau aus dem Archiv ──────────────────────────────────────────────────────

def _lesen(disktool, hfe, tmp_path, name):
    sys.path.insert(0, str(TOOLS))
    import m8k
    td0 = tmp_path / (name + ".td0")
    subprocess.run([str(disktool), "save-as", str(hfe), str(td0)], check=True, capture_output=True)
    return m8k.Dateisystem(str(td0))


def _datei(fs, pfad):
    ino = 2
    for teil in pfad.split("/"):
        ino = dict((n, i) for i, n in fs.eintraege(ino))[teil]
    return fs.inhalt(ino)


def _ohne_anlagezeit(fs):
    """Alle Bloecke; die Anlagezeit (s_time im Superblock, Zeiten der Inode 1) ist das einzig Fluechtige."""
    bl = {b: bytearray(fs.dev.block(b)) for b in range(fs.fsize)}
    bl[1][416:420] = bytes(4)
    bl[2][48:64] = bytes(16)
    return {b: bytes(v) for b, v in bl.items()}


@braucht_disktool
def test_bau_aus_archiv_ergibt_die_eingecheckte_wurzel_und_eine_getauschte_datei_kommt_an(tmp_path, monkeypatch):
    monkeypatch.setenv("K1520_DISKTOOL", str(DISKTOOL))
    sys.path.insert(0, str(TOOLS))
    import bau_aus_archiv

    # 1. Archiv -> Diskette: bitgleich zur eingecheckten (bis auf die Anlagezeit)
    neu = tmp_path / "neu.hfe"
    bau_aus_archiv.bauen(str(ARCHIV), str(neu), ausgabe=lambda *_: None)
    fs_neu = _lesen(DISKTOOL, neu, tmp_path, "neu")
    fs_alt = _lesen(DISKTOOL, WURZEL, tmp_path, "alt")
    assert (fs_neu.isize, fs_neu.fsize) == (15, 250)
    assert _ohne_anlagezeit(fs_neu) == _ohne_anlagezeit(fs_alt)

    # 2. Ordner statt Archiv, Datei tauschen -> neue Diskette mit der neuen Datei, sonst gleich
    ordner = tmp_path / "ausgepackt"
    with zipfile.ZipFile(ARCHIV) as z:
        z.extractall(ordner)
    motd = ordner / "dateien" / "etc" / "motd"
    alt_motd = motd.read_bytes()
    motd.write_bytes(b"Hallo aus dem angepassten Archiv\n")
    geaendert = tmp_path / "geaendert.hfe"
    bau_aus_archiv.bauen(str(ordner), str(geaendert), ausgabe=lambda *_: None)
    fs_neu2 = _lesen(DISKTOOL, geaendert, tmp_path, "geaendert")
    assert _datei(fs_neu2, "etc/motd") == b"Hallo aus dem angepassten Archiv\n"
    assert _datei(fs_neu, "etc/motd") == alt_motd
    assert _datei(fs_neu2, "bin/sh") == _datei(fs_neu, "bin/sh")

    # 3. Die eingecheckte Diskette ist nur mit --ueberschreiben Ziel; ein vorhandenes Ziel ebenso
    with pytest.raises(SystemExit, match="ueberschreiben"):
        bau_aus_archiv.bauen(str(ARCHIV), str(WURZEL), ausgabe=lambda *_: None)
    with pytest.raises(SystemExit, match="ueberschreiben"):
        bau_aus_archiv.bauen(str(ARCHIV), str(neu), ausgabe=lambda *_: None)
    shutil.rmtree(ordner)

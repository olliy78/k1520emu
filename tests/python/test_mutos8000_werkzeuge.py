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
LADE_ARCHIV = DISKS / "a5120_mutos8000_boot_udos.zip"
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
              "bau_aus_archiv.py", "archiv_erzeugen.py", "bau_ladediskette.py"):
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
    assert "fs_mfm: mutos8k_mfm" in angaben      # dieselbe Wurzel fuer MFM-Laufwerke
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
    bl[1][414:418] = bytes(4)       # s_time (Z8000 big endian, Offset 414)
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


# ─── Wurzel in MFM (K5601) ───────────────────────────────────────────────────

@braucht_disktool
def test_wurzel_fuer_mfm_laufwerke_hat_dieselbe_blockabbildung(tmp_path, monkeypatch):
    monkeypatch.setenv("K1520_DISKTOOL", str(DISKTOOL))
    sys.path.insert(0, str(TOOLS))
    import bau_aus_archiv
    mfm = tmp_path / "mfm.hfe"
    bau_aus_archiv.bauen(str(ARCHIV), str(mfm), ausgabe=lambda *_: None, laufwerke="k5601")
    info = subprocess.run([str(DISKTOOL), "info", str(mfm)], capture_output=True, text=True).stdout
    assert "Format:      udos_ss77" in info and "Dateisystem: mutos8k_mfm" in info     # MFM, nicht mf3200_26x128
    # Blockabbildung gleich der FM-Fassung (nur die Anlagezeit ist fluechtig) ...
    assert _ohne_anlagezeit(_lesen(DISKTOOL, mfm, tmp_path, "mfm")) == \
        _ohne_anlagezeit(_lesen(DISKTOOL, WURZEL, tmp_path, "fm"))
    # ... und die eingecheckte MFM-Wurzel traegt denselben Baum
    assert _ohne_anlagezeit(_lesen(DISKTOOL, mfm, tmp_path, "mfm2")) == \
        _ohne_anlagezeit(_lesen(DISKTOOL, DISKS / "a5120_mutos8000_m8k_root_rekonstruiert_mfm.hfe", tmp_path, "ein"))
    with pytest.raises(SystemExit, match="laufwerke"):
        bau_aus_archiv.bauen(str(ARCHIV), str(tmp_path / "x.hfe"), ausgabe=lambda *_: None, laufwerke="mf6400")


# ─── UDOS-Ladedisketten aus dem Archiv ───────────────────────────────────────

# Variante -> (Katalogformat der Aufzeichnung, Laufwerkstyp-Konstante, eingecheckte Diskette)
VARIANTEN = {
    "mf3200": ("mf3200_26x128", 0x2A, "a5120_mutos8000_boot_udos_mf3200.hfe"),
    "k5601": ("udos_ss77", 0x3F, "a5120_mutos8000_boot_udos_k5601.hfe"),
    "mf3200_k5601": ("mf3200_26x128", 0x3E, "a5120_mutos8000_boot_udos_mf3200_k5601.hfe"),
}
MUSTER = bytes.fromhex("3E2AA232CA0F")      # LD A,2AH / AND D / LD (0FCAH),A


def test_ladedisketten_archiv_ist_vollstaendig():
    with zipfile.ZipFile(LADE_ARCHIV) as z:
        namen = z.namelist()
        assert not any(n.endswith((".hfe", ".dmk")) for n in namen)      # kein Abbild: sonst veraltet es
        angaben = z.read("bauangaben.yaml").decode("utf-8")
        sysspur = z.read("systemspuren.bin")
        blatt = z.read("dateien/udos-dateiangaben.txt").decode("utf-8")
        nutz = sorted(n[len("dateien/"):] for n in namen
                      if n.startswith("dateien/") and not n.endswith((".fileinfo", "udos-dateiangaben.txt")))
        # 19 UDOS-Dateien: 18 stehen als Dateien im Archiv, DIRECTORY entsteht mit der Diskette
        assert len(nutz) == 18 and "DIRECTORY" not in nutz
        for n in nutz:
            assert "dateien/%s.fileinfo" % n in namen
        zeilen = [z_ for z_ in blatt.splitlines() if z_ and not z_.startswith("#")]
        assert sorted(z_.split()[0] for z_ in zeilen) == nutz
        assert all(" typ=" in z_ and " segment=" in z_ and " mem=" in z_ and " erst=" in z_ and " geaend=" in z_
                   for z_ in zeilen)
        assert any(z_.startswith("mutos_io ") and "segs=9000+022D,9400+0EB0,A400+01A8,C8B6+00B2" in z_
                   for z_ in zeilen)                              # alle vier Segmente
        # leere Datumsvermerke sind AUSGESCHRIEBEN (`-` hiesse bei put: heutiges Datum, Bau nicht reproduzierbar)
        assert not any(" erst=- " in z_ or z_.endswith(" erst=-") or z_.endswith("geaend=-") for z_ in zeilen)
        assert any("geaend=______" in z_ or "erst=______" in z_ for z_ in zeilen)
    # Systemspuren: 104 Sektoren (Spur 0-2 und Bootspur) mit je 4 B Sektorkontrollblock
    assert len(sysspur) == 104 * 132
    # die Laufwerkstyp-Konstante steht GENAU einmal in den Systemspuren, mit dem Erwartungswert 2AH
    assert sysspur.count(MUSTER) == 1
    # Zylinder 2, Sektor-ID 1, Offset 120/121 (Spur 0 und 1 haben je 26 Sektoren)
    stelle = 52 * 132 + 120
    assert sysspur[stelle:stelle + 2] == b"\x3e\x2a" and sysspur.find(MUSTER) == stelle
    for zeile in ("art: ladediskette", "fs_fm: udos_ss77_fm", "fs_mfm: udos_ss77", "typ_erwartet: 2A",
                  "variante_mf3200: fm 2A", "variante_k5601: mfm 3F", "variante_mf3200_k5601: fm 3E"):
        assert zeile in angaben


def test_ladedisketten_bauangaben_werden_geprueft(tmp_path):
    sys.path.insert(0, str(TOOLS))
    import bau_ladediskette as bl
    daten = bytearray(b"\x00" * 40 + MUSTER + b"\x00" * 40)
    assert bl.muster_stelle(daten, "3E??A232CA0F", 0x2A) == 41
    with pytest.raises(SystemExit, match="erwartet 3FH|erwartet 3F"):      # falscher Erwartungswert: kein Patch
        bl.muster_stelle(daten, "3E??A232CA0F", 0x3F)
    with pytest.raises(SystemExit, match="0-mal"):
        bl.muster_stelle(bytearray(100), "3E??A232CA0F", 0x2A)
    with pytest.raises(SystemExit, match="2-mal"):
        bl.muster_stelle(daten + daten, "3E??A232CA0F", 0x2A)
    with pytest.raises(SystemExit, match="genau einem"):
        bl.muster_stelle(daten, "3E2AA232CA0F", 0x2A)
    with pytest.raises(SystemExit, match="variante_k5601"):
        bl.variante_lesen({"variante_mf3200": "fm 2A"}, "k5601")
    with pytest.raises(SystemExit, match="variante_k5601"):
        bl.variante_lesen({"variante_k5601": "fm"}, "k5601")


@braucht_disktool
@pytest.mark.parametrize("variante", sorted(VARIANTEN))
def test_ladediskette_aus_dem_archiv_ist_bitgleich_der_eingecheckten(tmp_path, monkeypatch, variante):
    monkeypatch.setenv("K1520_DISKTOOL", str(DISKTOOL))
    sys.path.insert(0, str(TOOLS))
    import bau_ladediskette
    format_, konstante, eingecheckt = VARIANTEN[variante]
    neu = tmp_path / "neu.hfe"
    bau_ladediskette.bauen(str(LADE_ARCHIV), str(neu), variante, ausgabe=lambda *_: None)
    # kein Zeitstempel im Bau (UDOS-Daten stehen im Archiv): die Datei ist Byte fuer Byte die eingecheckte
    assert neu.read_bytes() == (DISKS / eingecheckt).read_bytes()
    info = subprocess.run([str(DISKTOOL), "info", str(neu)], capture_output=True, text=True).stdout
    assert "Format:      " + format_ in info                          # FM bzw. MFM, wie die Variante verlangt
    bin_ = tmp_path / "sys.bin"
    subprocess.run([str(DISKTOOL), "boot-get", str(neu), str(bin_)], check=True, capture_output=True)
    daten = bin_.read_bytes()
    stelle = 52 * 132 + 120                                           # Zylinder 2, Sektor-ID 1, Offset 120/121
    assert daten[stelle:stelle + 2] == bytes([0x3E, konstante])
    pruef = subprocess.run([str(DISKTOOL), "check", str(neu), "--full"], capture_output=True, text=True)
    assert pruef.returncode == 0 and "ohne Befund" in pruef.stdout


@braucht_disktool
def test_ladediskette_getauschte_datei_kommt_an_und_schutzregeln(tmp_path, monkeypatch):
    monkeypatch.setenv("K1520_DISKTOOL", str(DISKTOOL))
    sys.path.insert(0, str(TOOLS))
    import bau_ladediskette
    ordner = tmp_path / "ausgepackt"
    with zipfile.ZipFile(LADE_ARCHIV) as z:
        z.extractall(ordner)
    os_init = ordner / "dateien" / "OS.INIT"
    alt = os_init.read_bytes()
    neu_inhalt = alt.replace(b"LOADER", b"LADER ")                    # gleiche Laenge, anderer Text
    assert neu_inhalt != alt
    os_init.write_bytes(neu_inhalt)
    ziel = tmp_path / "geaendert.hfe"
    bau_ladediskette.bauen(str(ordner), str(ziel), "k5601", ausgabe=lambda *_: None)
    holen = tmp_path / "geholt"
    subprocess.run([str(DISKTOOL), "get", str(ziel), "OS.INIT", "ZDOS", "--to", str(holen)],
                   check=True, capture_output=True)
    assert (holen / "OS.INIT").read_bytes() == neu_inhalt
    assert (holen / "ZDOS").read_bytes() == (ordner / "dateien" / "ZDOS").read_bytes()

    # Schutz: vorhandenes Ziel, eingecheckte Diskette, .dmk (DMK-Schreiber macht bei FM falsche CRCs)
    with pytest.raises(SystemExit, match="ueberschreiben"):
        bau_ladediskette.bauen(str(ordner), str(ziel), "k5601", ausgabe=lambda *_: None)
    with pytest.raises(SystemExit, match="ueberschreiben"):
        bau_ladediskette.bauen(str(ordner), str(DISKS / VARIANTEN["k5601"][2]), "k5601", ausgabe=lambda *_: None)
    with pytest.raises(SystemExit, match="hfe"):
        bau_ladediskette.bauen(str(ordner), str(tmp_path / "x.dmk"), "k5601", ausgabe=lambda *_: None)
    # Abweichender Erwartungswert in den Bauangaben: der Bau bricht ab, bevor irgendetwas geschrieben wird
    angaben = ordner / "bauangaben.yaml"
    angaben.write_text(angaben.read_text().replace("typ_erwartet: 2A", "typ_erwartet: 3F"))
    nie = tmp_path / "nie.hfe"
    with pytest.raises(SystemExit, match="erwartet 3F"):
        bau_ladediskette.bauen(str(ordner), str(nie), "k5601", ausgabe=lambda *_: None)
    assert not nie.exists()

"""k1520disktool an GIDE-Platten (Poppe / Kingstener): ls/info/check/fsck/get/put/rm/format/create (Kommandozeile).

Temporaere Abbilder; das Golden-Fixture nur als Kopie.  Doku: tools/k1520disktool.md, doc/merkposten/disktool.md.
"""

import subprocess
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[2]
CLI = PROJECT_ROOT / "build" / "k1520disktool"
FIXTURE = PROJECT_ROOT / "tests" / "fixtures" / "gide" / "poppe_cpa.img.gz"

pytestmark = pytest.mark.skipif(not CLI.exists(), reason="k1520disktool nicht gebaut")


def lauf(*args, ok=True):
    r = subprocess.run([str(CLI), *map(str, args)], capture_output=True, text=True, timeout=120)
    if ok:
        assert r.returncode == 0, f"{args}: rc={r.returncode}\n{r.stdout}\n{r.stderr}"
    return r


def muster(n):
    return bytes((i * 7 + (i >> 8)) & 0xFF for i in range(n))


def test_poppe_anlegen_put_ls_get_rm_fsck(tmp_path):
    img = tmp_path / "p.img"
    r = lauf("create", img, "--gide-layout", "poppe")
    assert "8 Laufwerk(e) formatiert" in r.stdout
    quelle = tmp_path / "GROSS.DAT"
    quelle.write_bytes(muster(30000))
    lauf("put", img, quelle, "--as", "lw3")
    lauf("put", img, quelle, "--as", "lw0/ZWEI.DAT", "--no-backup")
    ls = lauf("ls", img)
    assert ls.stdout.split() == ["lw0/ZWEI.DAT", "lw3/GROSS.DAT"]
    assert "poppe" in ls.stderr and "eindeutig" in ls.stderr
    ziel = tmp_path / "aus"
    lauf("get", img, "lw3/*", "--to", ziel)
    assert (ziel / "lw3" / "GROSS.DAT").read_bytes()[:30000] == muster(30000)
    info = lauf("info", img)
    assert "lw0: 1 Dateien" in info.stdout and "2 Systemspuren" in info.stdout
    assert lauf("fsck", img, "--full").stdout.rstrip().endswith("ohne Befund")
    lauf("rm", img, "GROSS.DAT", "--laufwerk", "3")
    assert lauf("ls", img).stdout.split() == ["lw0/ZWEI.DAT"]


def test_kingstener_und_layoutwahl(tmp_path):
    img = tmp_path / "k.img"
    lauf("create", img, "--gide-layout", "kingstener")
    quelle = tmp_path / "A.TXT"
    quelle.write_bytes(b"abc" * 1000)
    lauf("put", img, quelle)                       # Kingstener: ein Laufwerk, kein --as noetig
    assert lauf("ls", img).stdout.split() == ["lw0/A.TXT"]
    assert "kingstener" in lauf("info", img).stdout


def test_auto_raet_nicht_und_schreibt_nicht(tmp_path):
    img = tmp_path / "leer.img"
    lauf("create", img, "--gide-layout", "poppe", "--laufwerk", "5")   # nur lw5 formatiert
    # unformatiertes Fremdabbild: nichts erkannt -> Exit 2, nichts geschrieben
    roh = tmp_path / "roh.img"
    with open(roh, "wb") as f:
        f.truncate(66000 * 512)
    r = lauf("ls", roh, ok=False)
    assert r.returncode == 2 and "--gide-layout" in r.stderr
    r = lauf("put", roh, img, "--as", "lw0", ok=False)
    assert r.returncode == 2
    # ausdruecklich + format
    lauf("format", roh, "--gide-layout", "kingstener")
    lauf("put", roh, muster_datei(tmp_path))


def muster_datei(tmp_path):
    p = tmp_path / "M.COM"
    p.write_bytes(muster(500))
    return p


def test_format_verweigert_ueber_daten_ohne_force(tmp_path):
    img = tmp_path / "f.img"
    lauf("create", img, "--gide-layout", "poppe")
    lauf("put", img, muster_datei(tmp_path), "--as", "lw1")
    r = lauf("format", img, "--laufwerk", "1", ok=False)             # ohne ausdrueckliches Layout: nie
    assert r.returncode != 0 and "ausdruecklich" in r.stderr
    r = lauf("format", img, "--laufwerk", "1", "--gide-layout", "poppe", ok=False)   # Dateien da: nur mit --force
    assert r.returncode != 0 and "erzwingen" in r.stderr
    assert lauf("ls", img).stdout.split() == ["lw1/M.COM"]
    lauf("format", img, "--laufwerk", "1", "--gide-layout", "poppe", "--force")
    assert lauf("ls", img).stdout.split() == []


def test_fremde_sperre_verbietet_schreiben(tmp_path):
    import os
    img = tmp_path / "s.img"
    lauf("create", img, "--gide-layout", "poppe")
    Path(str(img) + ".lock").write_text(f"{os.getpid()}\n")
    r = lauf("put", img, muster_datei(tmp_path), "--as", "lw0", ok=False)
    assert r.returncode != 0 and "gesperrt" in r.stderr
    assert "gesperrt" in lauf("ls", img).stderr             # Lesen geht, mit Hinweis


def test_golden_fixture_vom_poppe_treiber(tmp_path):
    if not FIXTURE.exists():
        pytest.skip("Golden-Fixture fehlt")
    kopie = tmp_path / "g.img.gz"
    kopie.write_bytes(FIXTURE.read_bytes())
    assert lauf("ls", kopie).stdout.split() == ["lw0/NUTZ.BIN", "lw1/HALLO.TXT"]
    lauf("get", kopie, "HALLO.TXT", "--to", tmp_path / "x")
    assert (tmp_path / "x" / "lw1" / "HALLO.TXT").read_bytes().startswith(b"Hallo von Poppe")
    assert "ohne Befund" in lauf("check", kopie, "--full").stdout


def test_mkdir_und_attr_gibt_es_nicht(tmp_path):
    img = tmp_path / "m.img"
    lauf("create", img, "--gide-layout", "poppe", "--laufwerk", "0")
    r = lauf("mkdir", img, "lw0/x", ok=False)
    assert r.returncode != 0 and "gibt es fuer GIDE" in r.stderr

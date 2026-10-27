"""Waechter fuer tools/mutos8000/kernel/objformat.py und finde_signaturen.py (AP K2,
doc/mutos_kernel/objformat.md): a.out-/Archivformat der MUTOS-Werkzeuge lesen und bytegleich
zurueckschreiben, Symbole, Signatursuche (Gegenprobe am gelinkten bin/ld, Kernel mutos_1216)."""

import subprocess
import sys
import zipfile

import pytest

from conftest import PROJECT_ROOT

KERNEL_DIR = PROJECT_ROOT / "tools" / "mutos8000" / "kernel"
DISKS = PROJECT_ROOT / "disks" / "a5120_mutos8000"

sys.path.insert(0, str(KERNEL_DIR))
import objformat as O          # noqa: E402
import finde_signaturen as F   # noqa: E402


def _zip(name, eintrag):
    with zipfile.ZipFile(DISKS / ("a5120_mutos8000_%s.zip" % name)) as z:
        return z.read("dateien/" + eintrag)


@pytest.fixture(scope="module")
def libc():
    return _zip("best_usr", "lib/libc.a")


def test_libc_archiv_rundlauf_bytegleich(libc):
    a = O.Archiv.lesen(libc)
    assert len(a.mitglieder) == 140
    assert a.schreiben() == libc
    for m in a.mitglieder:
        assert O.Objekt.lesen(m.daten).schreiben() == m.daten, m.name


@pytest.mark.parametrize("name", ["lib/crt0.o", "lib/mcrt0.o"])
def test_startobjekte_rundlauf(name):
    b = _zip("best_usr", name)
    o = O.Objekt.lesen(b)
    assert o.schreiben() == b and o.rest == b"" and o.magic == 0xE807


def test_crt0_relokation_und_symbole():
    o = O.Objekt.lesen(_zip("best_usr", "lib/crt0.o"))
    r = {off: rl for off, rl in o.relocs("text")}
    assert r[0x20].art == O.R_BSS and r[0x24].art == O.R_EXT and r[0x24].sym == 1   # CALL _main
    assert r[0x2C].art == O.R_EXT and r[0x2C].sym == 0                              # CALL _exit
    assert o.syms[0].name == b"_exit" and o.syms[0].typ == O.N_EXT
    assert o.syms[1].name == b"_main" and o.syms[1].typ == O.N_EXT
    assert any(s.name == b"_environ" and s.basistyp == O.N_BSS and s.ext for s in o.syms)


def test_libc_symbole_plausibel(libc):
    a = O.Archiv.lesen(libc)
    printf = next(m for m in a.mitglieder if m.name == b"printf.o").objekt()
    nm = O.nm_zeilen(printf)
    assert any(z.endswith(" T _printf") for z in nm)
    assert any(z.endswith(" U __doprnt") or z.endswith(" U _doprnt") for z in nm) or any("doprnt" in z for z in nm)
    # alle Namen sind druckbares ASCII, Relokationssymbole zeigen in die Tabelle
    for m in a.mitglieder:
        o = m.objekt()
        for s in o.syms:
            assert all(32 <= c < 127 for c in s.name), (m.name, s.name)
        for t in ("text", "data"):
            for _off, rl in o.relocs(t):
                if rl.art == O.R_EXT:
                    assert rl.sym < len(o.syms) and o.syms[rl.sym].ext


def test_ld_ausgabe_hat_crt0_am_textanfang():
    ld = _zip("best_usr", "bin/ld")
    crt0 = O.Objekt.lesen(_zip("best_usr", "lib/crt0.o"))
    kopf = O.Objekt.lesen(ld)
    assert kopf.flag & 1 and len(ld) == 16 + len(kopf.text) + len(kopf.data)    # gestrippt, keine Relokation
    t, mask = crt0.maskierter_text()
    assert all(mask[i] or t[i] == kopf.text[i] for i in range(len(t)))


def test_signatursuche_gegenprobe_am_gelinkten_ld(libc):
    ld = _zip("best_usr", "bin/ld")
    treffer = {n: tr for n, _l, tr, schwach in F.suche(libc, ld[16:]) if tr and not schwach}
    for n in ("printf.o", "stty.o", "strncmp.o", "signal.o", "csv.o", "sbrk.o"):
        assert n in treffer, n
    # die aus der Archivreihenfolge folgende Lage: stty.o (52 B) liegt unmittelbar vor strncmp.o
    assert treffer["stty.o"][0] + 52 == treffer["strncmp.o"][0]


def test_kernel_teilt_nur_die_csv_konvention(libc):
    k = _zip("boot_udos", "mutos_1216")
    assert len(k) == 36352
    assert not [n for n, _l, tr, s in F.suche(libc, k) if tr and not s]      # keine exakte libc-Funktion
    ae = {n: (st, a) for n, _l, a, st in F.aehnlich(libc, k, 0.8, 24)}
    assert ae["csv.o"][0] == 0x5F8


def test_signaturliste_im_repo_ist_aktuell(libc):
    soll = (PROJECT_ROOT / "doc" / "mutos_kernel" / "libc_signaturen.txt").read_text()
    assert soll == F.liste(libc)


def test_cli_dump_nm_arliste(tmp_path):
    arch = tmp_path / "libc.a"
    arch.write_bytes(_zip("best_usr", "lib/libc.a"))
    crt0 = tmp_path / "crt0.o"
    crt0.write_bytes(_zip("best_usr", "lib/crt0.o"))
    def lauf(*a):
        r = subprocess.run([sys.executable, "-I", str(KERNEL_DIR / "objformat.py"), *map(str, a)],
                           capture_output=True, text=True, timeout=60)
        assert r.returncode == 0, r.stderr
        return r.stdout
    assert "printf.o" in lauf("ar-list", arch)
    assert "U _main" in lauf("nm", crt0)
    assert "magic e807" in lauf("dump", crt0)
    lauf("ar-extract", arch, tmp_path / "x")
    assert (tmp_path / "x" / "strlen.o").read_bytes() == O.Archiv.lesen(arch.read_bytes()).mitglieder[
        [m.name for m in O.Archiv.lesen(arch.read_bytes()).mitglieder].index(b"strlen.o")].daten

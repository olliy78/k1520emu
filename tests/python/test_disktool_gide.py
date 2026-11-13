"""GIDE-Platte im DiskTool: C-ABI `k1520d_gide_*` und `GideTool` (Poppe / Kingstener).

Alles an temporaeren Abbildern (Layout ausdruecklich angelegt, sparse); das Golden-Fixture vom Poppe-Treiber
(`tests/fixtures/gide/poppe_cpa.img.gz`) wird nur als KOPIE geoeffnet.  Doku: doc/merkposten/disktool.md (GIDE-Platten).
"""

import shutil
from pathlib import Path

import pytest

from app.core_binding.k1520disk import (
    GideTool, K1520DiskError, gide_anlegen, gide_layout_nummer, ist_gide_kandidat, ist_platte,
)

FIXTURE = Path(__file__).resolve().parents[1] / "fixtures" / "gide" / "poppe_cpa.img.gz"


def muster(n, start=1):
    return bytes((start + i * 7 + (i >> 8)) & 0xFF for i in range(n))


@pytest.fixture
def poppe(tmp_path):
    p = tmp_path / "poppe.img"
    gide_anlegen(p, "poppe")
    t = GideTool.open(p, read_only=False, layout="poppe")
    t.set_backup(False)
    t.format_laufwerk(0)
    t.format_laufwerk(1)
    yield t
    t.close()


def test_every_gide_function_is_in_header_and_binding():
    """Der Abgleich Header <-> Bibliothek <-> ctypes steht in test_disk_c_api; hier nur, dass die GIDE-Funktionen mitgezaehlt werden."""
    from test_disk_c_api import header_functions
    fn = {f for f in header_functions() if f.startswith("k1520d_gide_")}
    assert len(fn) >= 40
    for f in ("k1520d_gide_open", "k1520d_gide_list", "k1520d_gide_write", "k1520d_gide_format", "k1520d_gide_flush"):
        assert f in fn


def test_layout_namen_sind_ein_vertrag_mit_der_c_abi():
    assert (gide_layout_nummer("auto"), gide_layout_nummer("poppe"), gide_layout_nummer("kingstener")) == (0, 1, 2)
    with pytest.raises(K1520DiskError):
        gide_layout_nummer("quark")


def test_kandidat_nach_groesse(tmp_path):
    klein = tmp_path / "klein.img"
    klein.write_bytes(b"\0" * 737280)
    assert not ist_gide_kandidat(klein)
    gross = tmp_path / "gross.img"
    gide_anlegen(gross, "kingstener")
    assert ist_gide_kandidat(gross)
    assert not ist_platte(gross)            # keine P8000-Platte


def test_rundlauf_poppe_zwei_laufwerke(poppe, tmp_path):
    a, b = muster(25000), muster(300, 9)
    poppe.write("lw0/GROSS.DAT", a)
    poppe.write("lw1/KLEIN.COM", b)
    poppe.flush()
    assert poppe.layout == "poppe" and poppe.layout_ausdruecklich
    namen = sorted(e.name for e in poppe.list() if e.type != "d")
    assert namen == ["lw0/GROSS.DAT", "lw1/KLEIN.COM"]
    assert [e.name for e in poppe.list() if e.type == "d"] == ["lw0", "lw1"]       # Laufwerkszeilen fuer den Baum
    assert poppe.read("lw0/GROSS.DAT")[:25000] == a
    poppe.set_read_only(True)
    poppe.close()
    # frisch, Auto-Erkennung, nur lesend
    with GideTool.open(poppe.path) as t:
        assert t.layout == "poppe" and t.layout_sicher and not t.layout_mehrdeutig
        assert t.read_only
        assert t.read("lw1/KLEIN.COM")[:300] == b
        assert [v.dir for v in t.volumes()] == ["lw0", "lw1"]
        assert not t.laufwerk_formatiert(2)
        assert t.check(True)[0] <= 0 or t.check(True)[0] == -1
        with pytest.raises(K1520DiskError, match="schreibgesch"):
            t.write("lw0/X.COM", b"x")


def test_extract_und_insert(poppe, tmp_path):
    quelle = tmp_path / "q" / "DATEI.TXT"
    quelle.parent.mkdir()
    quelle.write_bytes(b"Hallo Platte\r\n" * 50)
    poppe.insert(quelle, "lw1/DATEI.TXT")
    ziel = tmp_path / "aus"
    poppe.extract("", ziel)
    assert (ziel / "lw1" / "DATEI.TXT").read_bytes().startswith(b"Hallo Platte\r\n")
    with pytest.raises(K1520DiskError):
        poppe.insert(quelle, "lw1/DATEI.TXT")          # existiert, kein overwrite
    poppe.insert(quelle, "lw1/DATEI.TXT", overwrite=True)
    poppe.erase("lw1/DATEI.TXT")
    assert [e.name for e in poppe.list() if e.type != "d"] == []
    with pytest.raises(K1520DiskError, match="Ordner"):
        poppe.mkdir("lw0/x")


def test_kingstener_einlaufwerk_ohne_praefix(tmp_path):
    p = tmp_path / "king.img"
    gide_anlegen(p, "kingstener")
    with GideTool.open(p, read_only=False, layout="kingstener") as t:
        t.set_backup(False)
        t.format_laufwerk(0)
        t.write("DATEI.COM", muster(900))
        t.flush()
    with GideTool.open(p) as t:
        assert t.layout == "kingstener"
        assert [e.name for e in t.list() if e.type != "d"] == ["lw0/DATEI.COM"]
        assert t.read("DATEI.COM")[:900] == muster(900)


def test_auto_raet_nicht_bei_unformatierter_platte(tmp_path):
    p = tmp_path / "leer.img.gz"
    gide_anlegen(p, "poppe")
    with pytest.raises(K1520DiskError, match="gide-layout"):
        GideTool.open(p)
    # ausdruecklich geht, und das Formatieren eines FREMD-formatierten Bereichs wird verweigert
    with GideTool.open(p, read_only=False, layout="poppe") as t:
        t.set_backup(False)
        t.format_laufwerk(2)
        t.write("lw2/A.COM", b"abc")
        t.flush()
    with GideTool.open(p, read_only=False, layout="poppe") as t:
        with pytest.raises(K1520DiskError, match="erzwingen"):
            t.format_laufwerk(2)


def test_fremde_sperre_verbietet_schreiben(tmp_path):
    import os
    p = tmp_path / "gesperrt.img"
    gide_anlegen(p, "poppe")
    with GideTool.open(p, read_only=False, layout="poppe") as t:
        t.set_backup(False)
        t.format_laufwerk(0)
        t.flush()
    Path(str(p) + ".lock").write_text(f"{os.getpid()}\n")      # eine lebende Sperre = laufender Emulator
    with pytest.raises(K1520DiskError, match="gesperrt"):
        GideTool.open(p, read_only=False)
    with GideTool.open(p) as t:
        assert t.fremd_gesperrt and t.read_only
        with pytest.raises(K1520DiskError, match="gesperrt"):
            t.set_read_only(False)


def test_golden_fixture_vom_poppe_treiber(tmp_path):
    if not FIXTURE.exists():
        pytest.skip("Golden-Fixture fehlt")
    kopie = tmp_path / "golden.img.gz"
    shutil.copyfile(FIXTURE, kopie)
    with GideTool.open(kopie) as t:
        assert t.layout == "poppe" and t.layout_sicher
        assert sorted(e.name for e in t.list() if e.type != "d") == ["lw0/NUTZ.BIN", "lw1/HALLO.TXT"]
        assert t.read("lw1/HALLO.TXT").startswith(b"Hallo von Poppe, geschrieben im Emulator.\r\n")
        assert t.read("lw0/NUTZ.BIN")[:20000] == bytes((i * 7 + (i >> 8)) & 0xFF for i in range(20000))

"""P8000-Platte im DiskTool: C-ABI `k1520d_platte_*` und `PlatteTool` (Entwurf 37, AP P2).

Gearbeitet wird an einer KOPIE der ausgelieferten WEGA-3.0-Platte (`.img.gz`, Partitionen
md0/md2/md3/md4); md3 (/tmp) ist leer und dient als Schreibziel.  Nie die eingecheckte Platte
selbst oeffnen — ein Schreibzugriff veraendert sie.
"""

import gzip
import os
import shutil

import pytest

from conftest import ausgelieferte_diskette, requires_disk

pytestmark = requires_disk

STAND = 400000000          # fester Zeitstempel (Sekunden seit 1970, GMT)


@pytest.fixture
def platte_pfad(tmp_path):
    quelle = ausgelieferte_diskette("p8000_wega30_platte.k5504.img.gz")
    ziel = tmp_path / "platte.k5504.img.gz"
    shutil.copyfile(quelle, ziel)
    return str(ziel)


@pytest.fixture
def platte(platte_pfad):
    from app.core_binding.k1520disk import PlatteTool
    t = PlatteTool.open(platte_pfad)
    yield t
    t.close()


@pytest.fixture
def platte_rw(platte_pfad):
    from app.core_binding.k1520disk import PlatteTool
    t = PlatteTool.open(platte_pfad, read_only=False)
    t.set_backup(False)
    yield t
    t.close()


def test_every_platte_function_is_in_header_and_binding():
    """Der Abgleich Header ↔ Bibliothek ↔ ctypes steht in test_disk_c_api; hier nur,
    dass die Platten-Funktionen dabei wirklich mitgezaehlt werden."""
    from test_disk_c_api import header_functions
    plat = {f for f in header_functions() if f.startswith("k1520d_platte_")}
    assert len(plat) >= 30
    for f in ("k1520d_platte_open", "k1520d_platte_list", "k1520d_platte_insert",
              "k1520d_platte_extract", "k1520d_platte_flush"):
        assert f in plat


def test_ist_platte_unterscheidet_platte_von_diskette(platte_pfad):
    from app.core_binding.k1520disk import ist_platte, PlatteTool
    assert ist_platte(platte_pfad)
    assert PlatteTool.ist_platte(platte_pfad)
    diskette = ausgelieferte_diskette("p8000_wega30_root1.dmk")
    assert not ist_platte(diskette)


def test_oeffnen_einer_diskette_scheitert_mit_grund():
    from app.core_binding.k1520disk import PlatteTool, K1520DiskError
    with pytest.raises(K1520DiskError) as e:
        PlatteTool.open(ausgelieferte_diskette("p8000_wega30_root1.dmk"))
    assert str(e.value)


def test_platte_ist_schreibgeschuetzt_und_zeigt_partitionen(platte):
    assert platte.read_only
    assert platte.ist_unix and platte.ist_wega and platte.has_filesystem
    assert platte.filesystem_type == "wega"
    dirs = [v.dir for v in platte.volumes()]
    assert dirs[:2] == ["md0", "md2"] and "md3" in dirs
    assert platte.volume_role(dirs.index("md3")) == "/tmp"
    assert all(v.total > 0 for v in platte.volumes())


def test_verzeichnis_traegt_volle_pfade_und_unix_angaben(platte):
    liste = platte.list()
    namen = {e.name: e for e in liste}
    assert namen["md0"].type == "d"
    pw = namen["md2/etc/passwd"]
    assert pw.type == "-" and pw.ist_unix and pw.size > 0
    assert pw.unix_inode > 0 and pw.unix_nlink >= 1
    assert pw.side_dir == "" and pw.ref == pw.name
    assert namen["md2/etc"].type == "d"


def test_lesen_liefert_die_bytes(platte, tmp_path):
    daten = platte.read("md2/etc/passwd")
    eintrag = next(e for e in platte.list() if e.name == "md2/etc/passwd")
    assert len(daten) == eintrag.size and daten.startswith(b"wega:")


def test_lesen_einer_fehlenden_datei_wirft(platte):
    from app.core_binding.k1520disk import K1520DiskError
    with pytest.raises(K1520DiskError):
        platte.read("md2/etc/gibt_es_nicht")
    with pytest.raises(K1520DiskError):
        platte.read("etc/passwd")          # Partition fehlt


def test_schreibschutz_lehnt_jede_aenderung_ab(platte):
    from app.core_binding.k1520disk import K1520DiskError
    with pytest.raises(K1520DiskError, match="schreibgeschuetzt"):
        platte.write("md3/x", b"abc")
    with pytest.raises(K1520DiskError):
        platte.mkdir("md3/ordner")
    with pytest.raises(K1520DiskError):
        platte.erase("md2/etc/passwd")
    with pytest.raises(K1520DiskError):
        platte.set_unix_attrs("md2/etc/passwd", mode=0o600)
    assert not platte.dirty


def test_schreiben_lesen_loeschen_und_zurueckschreiben(platte_rw, platte_pfad):
    from app.core_binding.k1520disk import PlatteTool, K1520DiskError
    nutz = bytes(range(256)) * 20
    platte_rw.mkdir("md3/neu")
    platte_rw.write("md3/neu/daten", nutz, mode=0o640, uid=7, gid=3, mtime=STAND)
    assert platte_rw.dirty
    assert platte_rw.read("md3/neu/daten") == nutz

    e = next(x for x in platte_rw.list() if x.name == "md3/neu/daten")
    assert (e.unix_mode & 0o7777, e.unix_uid, e.unix_gid, e.unix_mtime) == (0o640, 7, 3, STAND)
    assert e.size == len(nutz)

    with pytest.raises(K1520DiskError):                 # ohne overwrite
        platte_rw.write("md3/neu/daten", b"x")
    platte_rw.write("md3/neu/daten", b"zwei", overwrite=True)
    assert platte_rw.read("md3/neu/daten") == b"zwei"

    platte_rw.flush()
    assert not platte_rw.dirty
    platte_rw.close()

    # In der Art der Quelle zurueckgeschrieben: weiter gzip, und frisch lesbar.
    with open(platte_pfad, "rb") as f:
        assert f.read(2) == b"\x1f\x8b"
    with PlatteTool.open(platte_pfad) as neu:
        assert neu.read("md3/neu/daten") == b"zwei"
        neu.set_read_only(False)
        neu.set_backup(False)
        neu.erase("md3/neu/daten")
        assert all(x.name != "md3/neu/daten" for x in neu.list())


def test_backup_entsteht_vor_dem_ersten_zurueckschreiben(platte_pfad):
    from app.core_binding.k1520disk import PlatteTool
    with PlatteTool.open(platte_pfad, read_only=False) as t:
        t.write("md3/probe", b"1")
        t.flush()
    assert os.path.exists(platte_pfad + "~")


def test_attribute_aendern(platte_rw):
    platte_rw.set_unix_attrs("md2/etc/passwd", mode=0o600, uid=5, gid=6, mtime=STAND)
    e = next(x for x in platte_rw.list() if x.name == "md2/etc/passwd")
    assert (e.unix_mode & 0o7777, e.unix_uid, e.unix_gid, e.unix_mtime) == (0o600, 5, 6, STAND)
    platte_rw.set_unix_attrs("md2/etc/passwd", uid=9)          # nur ein Feld
    e = next(x for x in platte_rw.list() if x.name == "md2/etc/passwd")
    assert (e.unix_mode & 0o7777, e.unix_uid, e.unix_gid) == (0o600, 9, 6)


def test_extract_und_insert_mit_fileinfo(platte_rw, tmp_path):
    ziel = tmp_path / "aus"
    platte_rw.extract("md2/etc/passwd", ziel)
    pw = ziel / "md2" / "etc" / "passwd"
    assert pw.read_bytes() == platte_rw.read("md2/etc/passwd")
    assert (ziel / "md2" / "etc" / "passwd.fileinfo").exists()

    # Zurueck in die frische Partition md3; Modus/Besitzer kommen aus dem .fileinfo.
    platte_rw.set_unix_attrs("md2/etc/passwd", mode=0o640, uid=4, gid=2, mtime=STAND)
    ziel2 = tmp_path / "aus2"
    platte_rw.extract("md2/etc/passwd", ziel2)
    platte_rw.insert(ziel2 / "md2" / "etc" / "passwd", "md3/passwd_kopie")
    e = next(x for x in platte_rw.list() if x.name == "md3/passwd_kopie")
    assert (e.unix_mode & 0o7777, e.unix_uid, e.unix_gid, e.unix_mtime) == (0o640, 4, 2, STAND)
    assert platte_rw.read("md3/passwd_kopie") == pw.read_bytes()


def test_ordner_einfuegen_und_ganz_herausholen(platte_rw, tmp_path):
    quelle = tmp_path / "q"
    (quelle / "unter").mkdir(parents=True)
    (quelle / "a.txt").write_bytes(b"A" * 1000)
    (quelle / "unter" / "b.txt").write_bytes(b"B" * 5)
    platte_rw.insert(quelle, "md3/baum")
    namen = {e.name for e in platte_rw.list()}
    assert {"md3/baum", "md3/baum/a.txt", "md3/baum/unter/b.txt"} <= namen

    aus = tmp_path / "aus"
    platte_rw.extract("md3/baum", aus)
    assert (aus / "md3" / "baum" / "unter" / "b.txt").read_bytes() == b"B" * 5

    with pytest.raises(Exception):
        platte_rw.insert(quelle / "a.txt", "a.txt")            # ohne Partition


def test_ganze_platte_extrahieren_nur_partition(platte, tmp_path):
    aus = tmp_path / "md3_aus"
    platte.extract("md3", aus)
    assert (aus / "md3").is_dir()


def test_gz_abbild_bleibt_unveraendert_solange_nichts_geschrieben(platte_pfad):
    from app.core_binding.k1520disk import PlatteTool
    vorher = open(platte_pfad, "rb").read()
    with PlatteTool.open(platte_pfad) as t:
        t.list()
        t.read("md2/etc/passwd")
    assert open(platte_pfad, "rb").read() == vorher
    gzip.open(platte_pfad).read(512)           # bleibt ein gueltiges gzip

"""MS-DOS-(FAT12-)Disketten im DiskTool: Binding, Baumansicht, Eigenschaften, Ordner, DOS SYS, Bootsektoren.

Der Kern wird in `tests/unit/filesystem/test_fat_fs.cpp` geprueft; hier steht, was die
Oberflaeche und die ctypes-Bindung daraus machen.  Alles laeuft an **selbst angelegten** Disketten.
"""

import datetime

import pytest

from conftest import PROJECT_ROOT, requires_disk

pytestmark = requires_disk

pytest.importorskip("PySide6")
yaml = pytest.importorskip("yaml")


@pytest.fixture(scope="module")
def qt_app(qapp):
    yield qapp


@pytest.fixture
def window(qt_app):
    from app.disktool.ui.main_window import MainWindow
    w = MainWindow()
    yield w
    w._close_tool()
    w.deleteLater()
    qt_app.processEvents()


@pytest.fixture
def fat_disk(tmp_path):
    """Eine 720K-FAT-Diskette mit Datei, langem Namen und zwei Ordnerebenen."""
    from app.core_binding.k1520disk import DiskTool

    quelle = tmp_path / "quelle"
    (quelle / "sub" / "tief").mkdir(parents=True)
    (quelle / "leer").mkdir()
    (quelle / "readme.txt").write_bytes(b"hallo\r\n")
    (quelle / "Eine lange Datei.Text").write_bytes(b"lang" * 100)
    (quelle / "sub" / "a.dat").write_bytes(bytes(range(256)) * 9)
    (quelle / "sub" / "tief" / "b.bin").write_bytes(b"\x00\x01\x02")

    pfad = tmp_path / "dos.img"
    with DiskTool.create(pfad, "dos_ds80_9x512", "PROBE") as d:
        d.insert_all(quelle)
        d.flush()
    return pfad


def _eintrag(tool, name):
    return next(e for e in tool.list() if e.name == name)


# ─── Binding ─────────────────────────────────────────────────────────────────

def test_filesystem_family_is_fat(fat_disk):
    from app.core_binding.k1520disk import DiskTool
    with DiskTool.open(fat_disk) as d:
        assert d.filesystem == "dos_ds80_9x512"
        assert d.filesystem_type == "fat"
        assert d.ist_fat and d.hat_baum
        assert not (d.ist_udos or d.ist_wega or d.ist_cpm or d.ist_tar)
        namen = {e.name for e in d.list()}
        assert {"sub", "sub/tief", "sub/tief/b.bin", "sub/a.dat", "leer", "readme.txt",
                "Eine lange Datei.Text"} <= namen
        assert _eintrag(d, "sub").type == "d"
        assert _eintrag(d, "readme.txt").type == ""
        lang = _eintrag(d, "Eine lange Datei.Text")
        assert lang.short_name == "EINELA~1.TEX" and lang.size == 400
        assert d.volumes()[0].label == "PROBE"


def test_dos_attrs_roundtrip_and_validation(fat_disk):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    jetzt = int(datetime.datetime(2001, 2, 3, 4, 5, 6).timestamp())
    with DiskTool.open(fat_disk, read_only=False) as d:
        d.set_dos_attrs("readme.txt", read_only=True, hidden=True, mtime=jetzt)
        d.flush()
        e = _eintrag(d, "readme.txt")
        assert "RO" in e.attrs and "HID" in e.attrs and e.read_only and e.hidden_attr
        assert e.date == "2001-02-03 04:05:06"
        # R/O schuetzt vor dem Loeschen
        with pytest.raises(K1520DiskError, match="schreibgeschuetzt"):
            d.erase("readme.txt")
        d.set_dos_attrs("readme.txt", read_only=False)
        assert not _eintrag(d, "readme.txt").read_only
        with pytest.raises(K1520DiskError):
            d.set_dos_attrs("gibt-es-nicht.txt", read_only=True)


def test_extract_keeps_the_dos_timestamp(fat_disk, tmp_path):
    import os
    from app.core_binding.k1520disk import DiskTool
    jetzt = int(datetime.datetime(1999, 12, 31, 23, 59, 58).timestamp())
    with DiskTool.open(fat_disk, read_only=False) as d:
        d.set_dos_attrs("sub/a.dat", mtime=jetzt)
        d.flush()
        d.extract_all(tmp_path / "aus")
    t = os.stat(tmp_path / "aus" / "sub" / "a.dat").st_mtime
    assert abs(t - jetzt) <= 2
    assert (tmp_path / "aus" / "leer").is_dir()
    assert (tmp_path / "aus" / "Eine lange Datei.Text").read_bytes() == b"lang" * 100


def test_lfn_switch(tmp_path):
    from app.core_binding.k1520disk import DiskTool
    pfad = tmp_path / "x.img"
    src = tmp_path / "Lange Datei Name.txt"
    src.write_bytes(b"x")
    with DiskTool.create(pfad, "dos_ds80_9x512", "") as d:
        d.set_fat_lfn(0)
        d.insert(src, "Lange Datei Name.txt")
        d.flush()
        assert [e.name for e in d.list()] == ["LANGED~1.TXT"]
    with DiskTool.create(tmp_path / "y.img", "dos_ds80_9x512", "") as d:
        d.set_fat_lfn(1)
        d.insert(src, "Lange Datei Name.txt")
        d.flush()
        assert [e.name for e in d.list()] == ["Lange Datei Name.txt"]


# ─── DOS SYS ─────────────────────────────────────────────────────────────────

def _systemdiskette(tmp_path):
    from app.core_binding.k1520disk import DiskTool
    bild = bytearray(512)
    bild[0:3] = b"\xEB\x34\x90"
    bild[3:11] = b"DCP  3.3"
    for i in range(0x36, 0x1E0):
        bild[i] = (i * 5) & 0xFF
    bild[510:512] = b"\x55\xAA"
    bin_ = tmp_path / "boot.bin"
    bin_.write_bytes(bytes(bild))
    pfad = tmp_path / "sys.img"
    with DiskTool.create(pfad, "dos_ds80_9x512", "SYSQUELLE", boot_image=bin_) as d:
        d.set_fat_lfn(0)
        for name, n in (("IO.SYS", 3000), ("DCDOS.SYS", 7000), ("COMMAND.COM", 500)):
            tmp = tmp_path / name
            tmp.write_bytes(bytes(range(256)) * (n // 256) + b"\x07" * (n % 256))
            d.insert(tmp, name)
        d.set_dos_attrs("IO.SYS", read_only=True, hidden=True, system=True)
        d.set_dos_attrs("DCDOS.SYS", read_only=True, hidden=True, system=True)
        d.flush()
    return pfad, bytes(bild)


def test_dos_sys_makes_a_boot_disk(tmp_path):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    quelle_pfad, bild = _systemdiskette(tmp_path)
    ziel_pfad = tmp_path / "ziel.img"
    with DiskTool.create(ziel_pfad, "dos_ds40_9x512", "NEU") as ziel, DiskTool.open(quelle_pfad) as quelle:
        bericht = ziel.dos_sys(quelle)
        assert "IO.SYS" in bericht and "DCDOS.SYS" in bericht and "WARNUNG" not in bericht
        ziel.flush()
        assert [e.name for e in ziel.list()] == ["IO.SYS", "DCDOS.SYS"]      # COMMAND.COM ist keine Systemdatei
        # der Bootsektor ist der der Quelle - mit dem BPB der 360K-Diskette
        ziel.read_boot_image(tmp_path / "b.bin")
        b = (tmp_path / "b.bin").read_bytes()
        assert b[3:11] == b"DCP  3.3" and b[21] == 0xFD
        assert b[0x36:0x1E0] == bild[0x36:0x1E0]
        # zweiter Versuch: das Ziel ist nicht mehr leer
        with pytest.raises(K1520DiskError, match="nicht leer"):
            ziel.dos_sys(quelle)


def test_dos_sys_requires_system_files(tmp_path):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    with DiskTool.create(tmp_path / "a.img", "dos_ds80_9x512", "") as a, DiskTool.create(tmp_path / "b.img", "dos_ds80_9x512", "") as b:
        with pytest.raises(K1520DiskError, match="Systemdiskette"):
            a.dos_sys(b)


# ─── Bootsektor-Katalog ──────────────────────────────────────────────────────

def test_boot_catalog_offers_the_dos_loaders_for_every_size():
    from app.core_binding.k1520disk import filesystems
    from app.disktool import bootsektoren
    kap = {f.name: f.boot_capacity for f in filesystems()}
    nach_fs, warn = bootsektoren.brauchbare(kap)
    assert not [w for w in warn if "boot_dos" in w], warn
    for fs in ("dos_ss40_8x512", "dos_ss40_9x512", "dos_ds40_8x512", "dos_ds40_9x512", "dos_ss80_9x512", "dos_ds80_8x512", "dos_ds80_9x512", "dos_ds80_10x512",
               "dos_ds80_15x512_hd", "dos_ds80_18x512_hd", "dos_ds80_36x512_ed"):
        texte = [e.description for e in nach_fs.get(fs, [])]
        assert any("DCP 3.3" in t for t in texte), (fs, texte)
        assert any("MDISK" in t for t in texte), (fs, texte)
    # jeder Eintrag ist ein ganzer Bootsektor
    for e in nach_fs["dos_ds80_9x512"]:
        assert e.path.stat().st_size == 512
        d = e.path.read_bytes()
        assert d[510:512] == b"\x55\xAA" and d[0] in (0xEB, 0xE9)


@pytest.mark.parametrize("bin_", sorted((PROJECT_ROOT / "data" / "bootsektoren").glob("boot_dos_*.bin")))
@pytest.mark.parametrize("fs", ["dos_ds80_9x512", "dos_ds40_9x512", "dos_ds80_18x512_hd"])
def test_create_with_every_hinterlegte_loader(tmp_path, bin_, fs):
    from app.core_binding.k1520disk import DiskTool
    pfad = tmp_path / "b.img"
    with DiskTool.create(pfad, fs, "BOOT", boot_image=bin_) as d:
        d.flush()
    with DiskTool.open(pfad) as d:
        assert d.filesystem == fs
        d.read_boot_image(tmp_path / "out.bin")
    out = (tmp_path / "out.bin").read_bytes()
    orig = bin_.read_bytes()
    prog = 2 + orig[1]
    assert out[prog:510] == orig[prog:510], "der Lader bleibt byteweise, wie er hinterlegt ist"
    assert out[3:11] == orig[3:11]


# ─── Oberflaeche ─────────────────────────────────────────────────────────────

def test_view_builds_a_folder_tree(window, fat_disk):
    from PySide6.QtCore import Qt
    assert window.open_image(str(fat_disk))
    baum = window.disk_view.tree

    def wurzel_namen():
        return [baum.topLevelItem(i).text(0) for i in range(baum.topLevelItemCount())]

    namen = wurzel_namen()
    assert "sub" in namen and "leer" in namen and "readme.txt" in namen
    sub = next(baum.topLevelItem(i) for i in range(baum.topLevelItemCount())
               if baum.topLevelItem(i).text(0) == "sub")
    assert sub.text(1) == "Ordner"
    kinder = [sub.child(i).text(0) for i in range(sub.childCount())]
    assert "a.dat" in kinder and "tief" in kinder


def test_properties_dialog_is_the_fat_one(window, fat_disk):
    from app.core_binding.k1520disk import DiskTool
    from app.disktool.ui.properties_dialog import PropertiesDialog
    with DiskTool.open(fat_disk, read_only=False) as d:
        e = _eintrag(d, "readme.txt")
        dlg = PropertiesDialog(d, e)
        assert dlg.art == "fat"
        assert dlg.aenderungen() == {}
        dlg.f_ro.setChecked(True)
        dlg.f_hid.setChecked(True)
        dlg.f_zeit.setText("2005-05-05 05:05:04")
        aend = dlg.aenderungen()
        assert aend["read_only"] is True and aend["hidden"] is True
        assert aend["mtime"] == int(datetime.datetime(2005, 5, 5, 5, 5, 4).timestamp())
        assert dlg.uebernehmen()
        neu = _eintrag(d, "readme.txt")
        assert neu.read_only and neu.hidden_attr and neu.date == "2005-05-05 05:05:04"
        # ungueltige Zeit wird erklaert, nichts geschrieben
        dlg2 = PropertiesDialog(d, neu)
        dlg2.f_zeit.setText("gestern")
        with pytest.raises(ValueError, match="kein Zeitpunkt"):
            dlg2.aenderungen()

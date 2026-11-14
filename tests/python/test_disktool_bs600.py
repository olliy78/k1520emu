"""BS600 (PRG 700/710/710-1) im DiskTool: Binding, C-ABI, Eigenschaften, Reorganisieren, Pruefung, Rettung, Neuanlage.

Der Kern wird in `tests/unit/filesystem/test_bs600_fs.cpp` und `test_bs600_check.cpp` geprueft; hier steht, was die
ctypes-Bindung und die Oberflaeche daraus machen.  Die Originalabzuege (`tests/fixtures/bs600/`) werden nie direkt
geoeffnet, sondern als Kopie.
"""

import shutil

import pytest

from conftest import PROJECT_ROOT, requires_disk

pytestmark = requires_disk

pytest.importorskip("PySide6")

FIXTURES = PROJECT_ROOT / "tests" / "fixtures" / "bs600"


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
def bs620(tmp_path):
    ziel = tmp_path / "bs620.hfe"
    shutil.copy(FIXTURES / "bs620_prg710-1.hfe", ziel)
    return ziel


@pytest.fixture
def leer(tmp_path):
    """Eine frisch angelegte BS600-Diskette mit drei Dateien (A, B, C) im Ordner."""
    from app.core_binding.k1520disk import DiskTool
    quelle = tmp_path / "q"
    quelle.mkdir()
    (quelle / "a.bin").write_bytes(bytes(range(256)) * 3)
    (quelle / "b.bin").write_bytes(bytes(range(200)))
    (quelle / "c.bin").write_bytes(bytes(range(100)) * 5)
    pfad = tmp_path / "neu.hfe"
    with DiskTool.create(pfad, "bs600_ss40_15x256_boot", "TEST") as d:
        for n in ("a", "b", "c"):
            d.insert(quelle / f"{n}.bin", f"{n}.bin")        # die Endung faellt weg
        d.flush()
    return pfad


def _eintrag(tool, name):
    return next(e for e in tool.list() if e.name == name)


# ─── Binding / C-ABI ─────────────────────────────────────────────────────────

def test_the_original_is_read_with_all_its_fields(bs620):
    from app.core_binding.k1520disk import DiskTool
    with DiskTool.open(bs620) as d:
        assert d.filesystem == "bs600_ss40_15x256_boot"
        assert d.filesystem_type == "bs600" and d.ist_bs600
        assert not (d.ist_udos or d.ist_wega or d.ist_cpm or d.ist_fat or d.ist_sios)
        assert len(d.list()) == 28
        e = _eintrag(d, "BS620")
        assert (e.type, e.low_addr, e.high_addr, e.entry) == ("P", 0x6D00, 0xD4FF, 0x6D5F)
        assert (e.created, e.date, e.extent) == ("KRU", "20.12.88", "06/15-13/13")
        # das Zielgeraet des Urladers steht als Satz (PRG 700 waere „BOOTSTRAP“)
        assert "710-1" in d.bs600_urlader and "SYL FOR BS620" in d.bs600_urlader


def test_diskette_info_names_the_loader_device(window, bs620):
    from app.disktool.ui.disk_info_dialog import angaben_text
    assert window.open_image(bs620)
    text = angaben_text(window.tool)
    assert "Urlader:" in text and "PRG 710-1" in text
    assert "bs600" in text


def test_the_prg700_original_names_its_device(tmp_path):
    from app.core_binding.k1520disk import DiskTool
    ziel = tmp_path / "bs600.hfe"
    shutil.copy(FIXTURES / "bs600_prg700.hfe", ziel)
    with DiskTool.open(ziel) as d:
        assert len(d.list()) == 25
        assert "PRG 700" in d.bs600_urlader and "NICHT" in d.bs600_urlader


def test_attrs_rename_reorganize_roundtrip(leer):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    with DiskTool.open(leer, read_only=False) as d:
        assert [e.name for e in d.list()] == ["A", "B", "C"]
        d.set_bs600_attrs("A", type="L", date="01.02.88", author="KRU", load=0x6D00, entry=0x6D5F)
        a = _eintrag(d, "A")
        assert (a.type, a.date, a.created, a.low_addr, a.entry) == ("L", "01.02.88", "KRU", 0x6D00, 0x6D5F)
        assert a.high_addr == 0x6D00 + 768 - 1                  # die Laenge bleibt, das Ende wandert mit
        with pytest.raises(K1520DiskError):
            d.set_bs600_attrs("A", type="X")
        with pytest.raises(K1520DiskError):
            d.set_bs600_attrs("GIBTS", author="X")
        d.rename("B", "NEU")
        assert [e.name for e in d.list()] == ["A", "NEU", "C"]
        with pytest.raises(K1520DiskError):
            d.rename("NEU", "A")                                 # Name belegt
        with pytest.raises(K1520DiskError):
            d.rename("NEU", "ZU.LANG")

        d.erase("NEU")
        assert d.bs600_reorganize() == 1                         # C rueckt auf
        assert d.bs600_reorganize() == 0
        c = _eintrag(d, "C")
        assert c.extent == "01/04-01/05"                         # A: 3 Sektoren, C: 2 (500 B)
        d.flush()
    with DiskTool.open(leer) as d:
        assert _eintrag(d, "C").extent == "01/04-01/05"
        assert d.check(voll=True).findings == ()                 # nach allem sauber


def test_reorganize_only_on_bs600(tmp_path):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    with DiskTool.create(tmp_path / "x.img", "dos_ds80_9x512", "X") as d:
        with pytest.raises(K1520DiskError, match="BS600"):
            d.bs600_reorganize()


def test_check_and_repair_through_the_binding(leer):
    from app.core_binding.k1520disk import DiskTool
    with DiskTool.open(leer, read_only=False) as d:
        d.erase("B")
        # Eine Luecke ist ein Hinweis mit einem (nicht empfohlenen) Vorschlag: Reorganisieren.
        bericht = d.check()
        f = [x for x in bericht.findings if x.id == "bs600.luecke"]
        assert len(f) == 1 and f[0].schwere == "Hinweis" and f[0].repairs
        assert not f[0].repairs[0].recommended
        n = next(i for i, x in enumerate(bericht.findings) if x.id == "bs600.luecke")
        assert d.apply_repairs([(n, 0)]) == 1
        assert d.findings().findings == ()
        assert _eintrag(d, "C").extent == "01/04-01/05"


def test_recover_finds_the_content_of_a_deleted_file(leer, tmp_path):
    from app.core_binding.k1520disk import DiskTool
    with DiskTool.open(leer, read_only=False) as d:
        d.erase("B")
        bericht = d.recover_scan(voll=True)
        assert len(bericht.finds) == 1
        f = bericht.finds[0]
        assert not f.restorable and f.name == "" and f.blocked_why
        assert f.size == 256                                     # 200 B belegen einen Sektor
        roh = d.recover_preview(0, 200)
        assert roh == bytes(range(200))


# ─── Oberflaeche ─────────────────────────────────────────────────────────────

def test_list_shows_the_directory_fields(window, bs620):
    assert window.open_image(bs620)
    baum = window.disk_view.tree
    koepfe = [baum.headerItem().text(i) for i in range(baum.columnCount())]
    assert koepfe == ["Name", "Typ", "Größe", "Lage", "Laden", "Ende", "Start", "Datum", "Signum"]
    zeilen = {baum.topLevelItem(i).text(0): [baum.topLevelItem(i).text(k) for k in range(baum.columnCount())]
              for i in range(baum.topLevelItemCount())}
    assert len(zeilen) == 28
    assert zeilen["BS620"][1:] == ["P", str(0xD4FF - 0x6D00 + 1), "06/15-13/13", "6D00", "D4FF", "6D5F", "20.12.88", "KRU"]
    assert zeilen["SD1152"][6] == "—"                            # Startadresse 0000 = keine


def test_properties_dialog_is_the_bs600_one(window, leer):
    from app.core_binding.k1520disk import DiskTool
    from app.disktool.ui.properties_dialog import PropertiesDialog
    with DiskTool.open(leer, read_only=False) as d:
        e = _eintrag(d, "A")
        dlg = PropertiesDialog(d, e)
        assert dlg.art == "bs600"
        assert dlg.aenderungen() == {}
        dlg.f_typ6.setCurrentIndex(dlg.f_typ6.findData("L"))
        dlg.f_datum6.setText("05.11.24")
        dlg.f_signum6.setText("OL")
        dlg.f_lade6.setText("3000")
        dlg.f_start6.setText("3010")
        dlg.f_name.setText("prog")
        assert dlg.aenderungen() == {"name": "PROG", "type": "L", "date": "05.11.24", "author": "OL",
                                     "load": 0x3000, "entry": 0x3010}
        assert dlg.uebernehmen()
        neu = _eintrag(d, "PROG")
        assert (neu.type, neu.date, neu.created, neu.low_addr, neu.entry) == ("L", "05.11.24", "OL", 0x3000, 0x3010)
        # ungueltige Adresse wird erklaert, nichts geschrieben
        dlg2 = PropertiesDialog(d, neu)
        dlg2.f_lade6.setText("ZZZZ")
        with pytest.raises(ValueError, match="hexadezimal"):
            dlg2.aenderungen()


def test_reorganize_action_roundtrip_in_the_window(window, leer, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    assert window.open_image(leer)
    assert not window.act_reorganisieren.isEnabled(), "schreibgeschuetzt geoeffnet: gesperrt"
    assert window.act_reorganisieren.shortcut().isEmpty(), "die Kuerzeltabelle des Handbuchs ist ein Vertrag"
    window.act_schreibschutz.setChecked(False)
    assert window.act_reorganisieren.isEnabled()
    window.tool.erase("B")
    window._reload()
    # ueber den Handler: erst die Rueckfrage (hier beantwortet)
    monkeypatch.setattr(QMessageBox, "question", lambda *a, **k: QMessageBox.Yes)
    window.act_reorganisieren.trigger()
    assert _eintrag(window.tool, "C").extent == "01/04-01/05"
    assert window.reorganisieren() == 0
    # Rundlauf: Eigenschaften, Pruefung und Rettung stehen offen, der Diskeditor oeffnet sich an der gemischten Geometrie
    assert window.act_reparieren.isEnabled() and window.act_wiederherstellen.isEnabled()
    ed = window.open_disk_editor()
    assert ed is not None
    ed.close()


def test_reorganize_is_gone_on_other_filesystems(window, tmp_path):
    from app.core_binding.k1520disk import DiskTool
    pfad = tmp_path / "dos.img"
    with DiskTool.create(pfad, "dos_ds80_9x512", "X") as d:
        d.flush()
    assert window.open_image(pfad)
    assert not window.act_reorganisieren.isEnabled()


def test_repair_dialog_offers_only_data_safe_repairs_preselected(window, leer):
    from PySide6.QtCore import Qt
    from app.disktool.ui.fsck_dialog import FsckDialog
    assert window.open_image(leer)
    window.act_schreibschutz.setChecked(False)
    window.tool.erase("B")
    window._reload()
    dlg = FsckDialog(window.tool, window)
    eintraege = [dlg.baum.topLevelItem(i) for i in range(dlg.baum.topLevelItemCount())]
    assert eintraege, "die Luecke ist ein Hinweis"
    assert all(e.checkState(0) == Qt.Unchecked for e in eintraege if e.flags() & Qt.ItemIsUserCheckable)


# ─── Neuanlage ───────────────────────────────────────────────────────────────

def test_the_catalog_offers_both_loaders_for_bs600():
    from app.core_binding.k1520disk import filesystems
    from app.disktool import bootsektoren
    kap = {f.name: f.boot_capacity for f in filesystems()}
    nach_fs, warn = bootsektoren.brauchbare(kap)
    assert not [w for w in warn if "bs6" in w.lower()], warn
    texte = [e.description for e in nach_fs.get("bs600_ss40_15x256_boot", [])]
    assert any("PRG 710-1" in t for t in texte), texte
    assert any("PRG 700" in t and "nicht PRG 710" in t for t in texte), texte


def test_new_bs620_system_disk_carries_the_loader_and_names_its_device(tmp_path):
    from app.core_binding.k1520disk import DiskTool
    bin_ = PROJECT_ROOT / "data" / "bootsektoren" / "boot_bs620_prg710-1.bin"
    pfad = tmp_path / "neu620.hfe"
    with DiskTool.create(pfad, "bs600_ss40_15x256_boot", "BS620", boot_image=bin_) as d:
        d.flush()
    with DiskTool.open(pfad) as d:
        assert "710-1" in d.bs600_urlader
        d.read_boot_image(tmp_path / "o.bin")
    assert (tmp_path / "o.bin").read_bytes() == bin_.read_bytes()


def test_handbook_describes_bs600():
    text = (PROJECT_ROOT / "app" / "disktool" / "help" / "handbuch.md").read_text(encoding="utf-8")
    assert "## BS600" in text
    assert "reorganisieren" in text.lower()
    assert "BOOTSTRAP FOR BS600" in text


# ─── Kommandozeile ───────────────────────────────────────────────────────────

CLI = PROJECT_ROOT / "build" / "k1520disktool"


def _cli(*args):
    import subprocess
    return subprocess.run([str(CLI), *map(str, args)], capture_output=True, text=True, timeout=120)


@pytest.mark.skipif(not CLI.exists(), reason="build/k1520disktool fehlt")
def test_cli_prg_flag_binds_the_loader_to_its_device(tmp_path):
    boot = PROJECT_ROOT / "data" / "bootsektoren"
    syl, bootstrap = boot / "boot_bs620_prg710-1.bin", boot / "boot_bs600_prg700.bin"

    ok = _cli("create", tmp_path / "a.hfe", "--fs", "bs600_ss40_15x256_boot", "--boot", syl, "--prg", "710-1")
    assert ok.returncode == 0, ok.stderr
    assert "PRG 710-1" in ok.stdout                       # der Hinweis nach dem Einspielen

    # PRG-700-Urlader fuer ein PRG 710-1: abgewiesen, ehe etwas geschrieben wird
    falsch = _cli("create", tmp_path / "b.hfe", "--fs", "bs600_ss40_15x256_boot", "--boot", bootstrap, "--prg", "710-1")
    assert falsch.returncode != 0 and "PRG 700" in falsch.stderr
    assert not (tmp_path / "b.hfe").exists()
    umgekehrt = _cli("create", tmp_path / "c.hfe", "--fs", "bs600_ss40_15x256_boot", "--boot", syl, "--prg", "700")
    assert umgekehrt.returncode != 0 and "BOOTSTRAP" in umgekehrt.stderr

    # ohne --prg wird jeder echte Urlader angenommen und sein Geraet genannt
    p7 = _cli("create", tmp_path / "d.hfe", "--fs", "bs600_ss40_15x256_boot", "--boot", bootstrap)
    assert p7.returncode == 0 and "PRG 700" in p7.stdout
    info = _cli("info", tmp_path / "d.hfe")
    assert "PRG 700" in info.stdout and "NICHT am PRG 710" in info.stdout
    chk = _cli("check", tmp_path / "d.hfe")
    assert "bs600.urlader.prg700" in chk.stdout

    # boot-put prueft ebenso
    bp = _cli("boot-put", tmp_path / "a.hfe", bootstrap, "--prg", "710-1")
    assert bp.returncode != 0

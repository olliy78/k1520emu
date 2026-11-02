"""MicroDOS 1526 / 1715 im DiskTool: Oberflaeche, Archiv (mit Rueckweg), Rettung, Pruefung.

Waechter fuer doc/merkposten/microdos.md / microdos1715.md / disktool.md (MicroDOS).  Gearbeitet wird
an frisch erzeugten Disketten (`create --boot`), nie an einer eingecheckten.  Die Bootfaehigkeit im
Emulator prueft `test_microdos_bootabbilder.py`.
"""

import zipfile
from pathlib import Path

import pytest

from conftest import PROJECT_ROOT, requires_disk

pytestmark = requires_disk

pytest.importorskip("PySide6")

BOOT = PROJECT_ROOT / "data" / "bootsektoren"

#: (Dateisystem, Bootabbild, Name der Systemdatei) -- alle hinterlegten Abbilder
PROFILE = [
    ("microdos800", "boot_microdos_1526.bin", "MicroDOS.SYS"),
    ("microdos800", "boot_microdos_1526_k7636.bin", "MicroDOS.SYS"),
    ("microdos200", "boot_microdos_1526_200k.bin", "MicroDOS.SYS"),
    ("microdos200", "boot_microdos_1526_200k_k7636.bin", "MicroDOS.SYS"),
    ("microdos128", "boot_microdos_1526_128.bin", "MicroDOS.SYS"),
    ("microdos1715", "boot_microdos_1715.bin", "MICRODOS.SYS"),
]
IDS = [p[1] for p in PROFILE]


@pytest.fixture(scope="module")
def qt_app(qapp):
    yield qapp


@pytest.fixture
def window(qt_app):
    from PySide6.QtCore import QCoreApplication, QEvent
    from app.disktool.ui.main_window import MainWindow
    w = MainWindow()
    yield w
    w._close_tool()
    w.deleteLater()
    qt_app.processEvents()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)


def _neu(pfad, fs, bin_):
    from app.core_binding.k1520disk import DiskTool
    DiskTool.create(pfad, fs, "", str(BOOT / bin_)).close()
    return pfad


def _muster(n, salz):
    return bytes((i * 7 + salz) & 0xFF for i in range(n))


def _namen(window):
    return {e.name: e for e in window.tool.list()}


def _dialog(window, name):
    from app.disktool.ui.properties_dialog import PropertiesDialog
    return PropertiesDialog(window.tool, _namen(window)[name], window)


# ─── Neue Diskette ueber den Dialog ──────────────────────────────────────────

@pytest.mark.parametrize("fs,bin_,sysname", PROFILE, ids=IDS)
def test_neue_diskette_mit_jedem_bootabbild(window, tmp_path, monkeypatch, fs, bin_, sysname):
    """Dialog „Neue Diskette": Dateisystem + jedes hinterlegte Abbild -> erkannte, saubere Systemdiskette."""
    from PySide6.QtWidgets import QDialog, QInputDialog
    from app.disktool.ui import main_window as mw
    from app.disktool.ui.neu_dialog import NeuDialog

    ziel = tmp_path / "neu.hfe"

    def exec_(dlg):
        i = dlg.wahl.findData(fs)
        assert i >= 0, fs
        dlg.wahl.setCurrentIndex(i)
        assert dlg.boot_haken.isEnabled()
        dlg.boot_haken.setChecked(True)
        texte = [dlg.boot_wahl.itemText(k) for k in range(dlg.boot_wahl.count())]
        for k in range(dlg.boot_wahl.count()):
            dlg.boot_wahl.setCurrentIndex(k)
            if dlg.bootabbild() is not None and Path(dlg.bootabbild()).name == bin_:
                break
        else:
            raise AssertionError(f"{bin_} fehlt im Auswahlfeld: {texte}")
        return QDialog.Accepted

    monkeypatch.setattr(NeuDialog, "exec", exec_)
    monkeypatch.setattr(mw.MainWindow, "_neue_datei_waehlen", lambda self, *a, **k: str(ziel))
    monkeypatch.setattr(QInputDialog, "getText", lambda *a, **k: ("", True))
    monkeypatch.setattr(mw.MainWindow, "_darf_verwerfen", lambda self: True)

    window._neu_dialog()
    assert window.tool is not None, window.protokoll.toPlainText()
    assert window.tool.filesystem == fs
    assert window.tool.unambiguous
    assert list(_namen(window)) == [sysname]
    assert fs in window.kopf.angaben.text()
    assert not window.tool.check(voll=True).findings
    soll = (BOOT / bin_).read_bytes()
    zurueck = tmp_path / "zurueck.bin"
    window.tool.read_boot_image(zurueck)
    assert zurueck.read_bytes()[:len(soll)] == soll


def test_beschreibung_nennt_tastatur_und_laufwerke(qt_app):
    """Das System ist fest auf Tastatur und Laufwerke generiert: der Dialog sagt es."""
    from app.disktool import bootsektoren
    from app.core_binding.k1520disk import filesystems
    nach_fs, _ = bootsektoren.brauchbare({f.name: f.boot_capacity for f in filesystems()},
                                         [(PROJECT_ROOT / "data" / "bootsektor.yaml", False)])
    t800 = [e.description for e in nach_fs["microdos800"]]
    t200 = [e.description for e in nach_fs["microdos200"]]
    assert any("K7637" in t and "K5601" in t for t in t800), t800
    assert any("K7636" in t and "K5601" in t for t in t800), t800
    assert any("K7637" in t and "K5600.10" in t for t in t200), t200
    assert any("K7636" in t and "K5600.10" in t for t in t200), t200
    assert not any("K5600.20" in t for t in t200 + t800)
    t128 = [e.description for e in nach_fs["microdos128"]]
    assert any("K7637" in t and "K5600.10" in t and "V0.6" in t for t in t128), t128


# ─── Bedienung: Einfuegen, Attribute, Loeschen, Pruefen, Retten ─────────────────

@pytest.mark.parametrize("fs,bin_,sysname", PROFILE, ids=IDS)
def test_einfuegen_attribute_loeschen_pruefen_retten(window, tmp_path, monkeypatch, fs, bin_, sysname):
    from PySide6.QtWidgets import QFileDialog, QInputDialog, QMessageBox
    from app.disktool.ui.fsck_dialog import FsckDialog
    from app.disktool.ui.recover_dialog import RecoverDialog

    monkeypatch.setattr(QMessageBox, "information", staticmethod(lambda *a, **k: None))
    pfad = _neu(tmp_path / "d.hfe", fs, bin_)
    boot_vorher = tmp_path / "boot_vorher.bin"
    from app.core_binding.k1520disk import DiskTool
    with DiskTool.open(pfad) as d:
        d.read_boot_image(boot_vorher)

    assert window.open_image(str(pfad))
    window.set_read_only(False)
    assert window.tool.filesystem == fs

    # Einfuegen
    quelle = tmp_path / "q"
    quelle.mkdir()
    inhalt = {"A.BIN": _muster(3000, 1), "B.COM": _muster(1200, 2), "C.BIN": _muster(500, 3)}
    for n, b in inhalt.items():
        (quelle / n).write_bytes(b)
    assert window.insert_paths([str(quelle / n) for n in inhalt])
    namen = _namen(window)
    assert set(namen) == {sysname, *inhalt}
    assert not window.tool.check(voll=True).findings

    # Attribute und Nutzerbereich im Eigenschaften-Dialog
    d = _dialog(window, "A.BIN")
    assert not d.udos and not d.wega
    d.f_ro.setChecked(True)
    d.f_arc.setChecked(True)
    d.f_user.setValue(3)
    assert d.uebernehmen()
    namen = _namen(window)
    assert "A.BIN" not in namen and "3:A.BIN" in namen
    assert "RO" in namen["3:A.BIN"].attrs and "ARC" in namen["3:A.BIN"].attrs
    d = _dialog(window, "B.COM")
    d.f_sys.setChecked(True)
    assert d.uebernehmen()
    assert "SYS" in _namen(window)["B.COM"].attrs

    # Die Systemdatei: Eigenschaften nur ansehen, nichts aendert sich ohne Zutun
    assert _dialog(window, sysname).aenderungen() == {}

    # Loeschen (R/O-Dateien lassen sich auf der Diskette loeschen -- das Werkzeug ist kein BDOS)
    assert window.erase_refs(["C.BIN"])
    assert "C.BIN" not in _namen(window)

    # Pruefdialog
    FsckDialog(window.tool, window)
    # nur der Hinweis auf den wiederherstellbaren Rest -- sonst nichts
    assert [f.id for f in window.tool.check(voll=True).findings] == ["cpm.dir.geloescht"]

    # Rettungsdialog: C.BIN ist geloescht, muss gefunden und eintragbar sein; die Systemdatei nie
    dlg = RecoverDialog(window.tool, window, zielordner=str(tmp_path), log=window.log)
    funde = [f for f in dlg.bericht.finds if f.name == "C.BIN"]
    assert len(funde) == 1, [(f.name, f.suggestion) for f in dlg.bericht.finds]
    assert funde[0].quality == 2 and funde[0].restorable, funde[0].detail
    assert not [f for f in dlg.bericht.finds if f.name.upper() == "MICRODOS.SYS"]
    dlg.baum.setCurrentItem(dlg.baum.topLevelItem(dlg.bericht.finds.index(funde[0])))
    monkeypatch.setattr(QInputDialog, "getText", staticmethod(lambda *a, **k: ("C.BIN", True)))
    ziel = tmp_path / "gerettet.bin"
    monkeypatch.setattr(QFileDialog, "getSaveFileName", staticmethod(lambda *a, **k: (str(ziel), "")))
    dlg._retten()
    assert ziel.read_bytes()[:500] == inhalt["C.BIN"]
    dlg._wiederherstellen()
    assert dlg.wiederhergestellt == 1
    assert "C.BIN" in _namen(window)
    assert not window.tool.check(voll=True).findings

    # Lader und Systemdatei sind nach alledem unversehrt
    window.save()
    boot_nachher = tmp_path / "boot_nachher.bin"
    window.tool.read_boot_image(boot_nachher)
    assert boot_nachher.read_bytes() == boot_vorher.read_bytes()


# ─── Archiv mit Rueckweg ───────────────────────────────────────────────────────────

@pytest.mark.parametrize("fs,bin_,sysname", [PROFILE[0], PROFILE[2], PROFILE[5]], ids=["800", "200", "1715"])
def test_archiv_traegt_dateien_attribute_und_den_rueckweg(window, tmp_path, fs, bin_, sysname):
    """ZIP = Abbild + `dateien/` + `cpm-dateiangaben.txt`; die Beiblattangaben (Nutzerbereich, R/O,
    SYS, ARC) kommen beim Einfuegen des ausgepackten Ordners auf eine neue Diskette zurueck."""
    from app.core_binding.k1520disk import DiskTool
    pfad = _neu(tmp_path / "quelle.hfe", fs, bin_)
    inhalt = {"A.BIN": _muster(3000, 1), "B.COM": _muster(1300, 2), "NUR.LES": _muster(200, 3)}
    quelle = tmp_path / "q"
    quelle.mkdir()
    for n, b in inhalt.items():
        (quelle / n).write_bytes(b)
    assert window.open_image(str(pfad))
    window.set_read_only(False)
    assert window.insert_paths([str(quelle / n) for n in inhalt])
    for name, ro, sy, arc, user in (("A.BIN", False, False, True, 2), ("B.COM", False, True, False, 0),
                                    ("NUR.LES", True, False, False, 0)):
        d = _dialog(window, name)
        d.f_ro.setChecked(ro)
        d.f_sys.setChecked(sy)
        d.f_arc.setChecked(arc)
        d.f_user.setValue(user)
        assert d.uebernehmen(), name
    vorher = {e.name: (e.attrs, e.size) for e in window.tool.list()}
    assert vorher["2:A.BIN"][0] == "ARC" and vorher["B.COM"][0] == "SYS" and vorher["NUR.LES"][0] == "RO"

    zip_pfad = tmp_path / "archiv.zip"
    assert window.archive(zip_pfad, bezeichnung="MicroDOS Test")
    with zipfile.ZipFile(zip_pfad) as z:
        namen = z.namelist()
        assert any(n.endswith(".hfe") for n in namen), namen
        assert "dateien/cpm-dateiangaben.txt" in namen, namen
        blatt = z.read("dateien/cpm-dateiangaben.txt").decode("utf-8")
        # das Beiblatt fuehrt nur Dateien mit Angaben: MICRODOS.SYS (1715) traegt SYS, MicroDOS.SYS (1526) nicht
        assert (sysname in blatt) == (fs == "microdos1715")
        assert f"dateien/{sysname}" in namen
        assert {"dateien/B.COM", "dateien/NUR.LES"} <= set(namen)
        auspack = tmp_path / "aus"
        z.extractall(auspack)

    # Das archivierte Abbild ist dieselbe Diskette: gleiche Erkennung, gleiche Dateiliste und Angaben
    hfe = next(p for p in auspack.glob("*.hfe"))
    with DiskTool.open(hfe) as a:
        assert a.filesystem == fs
        assert {e.name: (e.attrs, e.size) for e in a.list()} == vorher

    # Rueckweg: Ordner `dateien/` auf eine neue Systemdiskette -- die Systemdatei steht schon dort
    (auspack / "dateien" / sysname).unlink()
    for rest in (auspack / "dateien").glob(sysname + "*"):
        rest.unlink()
    ziel = _neu(tmp_path / "ziel.hfe", fs, bin_)
    with DiskTool.open(ziel, read_only=False) as t:
        t.insert_all(auspack / "dateien")
        t.flush()
        nachher = {e.name: (e.attrs, e.size) for e in t.list()}
        assert not t.check(voll=True).findings
    assert nachher == vorher
    with DiskTool.open(ziel) as t:
        (tmp_path / "ex").mkdir()
        t.extract("2:A.BIN", tmp_path / "ex" / "A.BIN")
    assert (tmp_path / "ex" / "A.BIN").read_bytes()[:3000] == inhalt["A.BIN"]

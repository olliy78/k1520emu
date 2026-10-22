"""TeleDisk (.td0) und ImageDisk (.imd) im Laufwerkskasten des Emulators (PySide6, headless).

Wie ``.cqm`` (test_cqm_emulator_gui.py): schreibbar, kein Formatdialog, Kopfdaten und
Kommentar als Tooltip, Hinweise je Programmprofil in der Hinweiszeile (kein Meldungsfenster),
„Speichern unter…" als .td0/.imd nur, wenn das Medium es traegt.
"""

import pytest

from conftest import requires_core
from cqm_bau import leeres_720k
import imd_bau
import td0_pack

pytestmark = requires_core

pytest.importorskip("PySide6", reason="PySide6 nicht installiert")


@pytest.fixture
def drives(qapp):
    from app.ui.drive_widget import DriveWidget
    from app.core_binding.k1520 import K1520Emulator
    emu = K1520Emulator()
    w = DriveWidget(emu)
    yield w
    w.deleteLater()
    del emu


@pytest.fixture
def td0_datei(tmp_path):
    p = tmp_path / "test.td0"
    p.write_bytes(td0_pack.beispiel())
    return str(p)


@pytest.fixture
def imd_datei(tmp_path):
    daten = bytes([0xE5]) * 512
    spur = {"mode": 5, "cyl": 0, "head": 0, "n": 2,
            "sectors": [{"c": 0, "h": 0, "r": r, "typ": imd_bau.typ_aus_flags(daten, False, False, False),
                         "data": daten} for r in range(1, 10)]}
    p = tmp_path / "test.imd"
    p.write_bytes(imd_bau.baue([spur], kommentar=b"Testkommentar IMD"))
    return str(p)


@pytest.fixture
def cqm_datei(tmp_path):
    p = tmp_path / "quelle.cqm"
    p.write_bytes(leeres_720k(b"Quelle"))
    return str(p)


def test_filters_know_td0_and_imd(qapp):
    from app.ui.drive_widget import DriveWidget as D
    for flt in (D.DISK_FILTER, D.BLANK_FILTER, D.SAVE_FILTER_ALL, D.SAVE_FILTER_CQM):
        assert "*.td0" in flt and "*.imd" in flt
    assert "td0" not in D.SAVE_FILTER_BITSTREAM and "imd" not in D.SAVE_FILTER_BITSTREAM


@pytest.mark.parametrize("art,fixture", [("td0", "td0_datei"), ("imd", "imd_datei")])
def test_mounted_writable_without_message_box(drives, request, monkeypatch, art, fixture):
    from PySide6.QtWidgets import QMessageBox
    monkeypatch.setattr(QMessageBox, "critical", staticmethod(
        lambda *a, **k: pytest.fail("Meldungsfenster beim Einlegen")))
    datei = request.getfixturevalue(fixture)
    assert drives.mount_path(0, datei)
    panel = drives._panels[0]
    emu = drives.emulator
    assert emu.disk_container(0) == art
    assert not emu.is_disk_write_protected(0)
    assert panel._wp_check.isEnabled() and not panel._wp_check.isChecked()
    panel._wp_check.setChecked(True)
    assert emu.is_disk_write_protected(0)
    panel._wp_check.setChecked(False)
    assert not emu.is_disk_write_protected(0)


def test_td0_tooltip_shows_header_and_comment(drives, td0_datei):
    assert drives.mount_path(0, td0_datei)
    tipp = drives._panels[0]._path_display.toolTip()
    assert "TeleDisk-Abbild" in tipp
    assert "Version: 1.5" in tipp and "Datenrate: 250" in tipp and "Sektoren: 4" in tipp
    assert "Testvektor" in tipp and "zweite Zeile" in tipp


def test_imd_tooltip_shows_header_and_comment(drives, imd_datei):
    assert drives.mount_path(0, imd_datei)
    tipp = drives._panels[0]._path_display.toolTip()
    assert "ImageDisk-Abbild" in tipp and "Testkommentar IMD" in tipp
    assert "Sektoren: 9" in tipp and "Spuren: 1" in tipp


def test_no_format_dialog(drives, td0_datei, monkeypatch):
    from PySide6.QtWidgets import QFileDialog
    monkeypatch.setattr(QFileDialog, "getOpenFileName",
                        staticmethod(lambda *a, **k: (td0_datei, "")))
    import app.ui.drive_widget as dw

    def nie(*a, **k):
        raise AssertionError("Formatdialog fuer .td0")
    monkeypatch.setattr(dw.DriveWidget, "_frage_img_format", nie)
    drives._panels[0]._toggle_btn.click()
    assert drives.emulator.disk_container(0) == "td0"


@pytest.mark.parametrize("maschine,udos", [
    ("a5120", True), ("prg710", True), ("k8915", False), ("pc1715", False),
    ("p8000", False), ("mc80", False)])
def test_hints_follow_the_program_profile(drives, td0_datei, imd_datei, maschine, udos):
    from app.profil import profil
    p = profil(maschine)
    drives.td0_hinweise, drives.imd_hinweise = p.td0_hinweise, p.imd_hinweise
    assert drives.mount_path(0, td0_datei)
    text = drives._panels[0]._notice_label.text()
    assert ("mit UDOS nicht verwendbar" in text) == udos
    assert ("unkomprimiert" in text) == udos
    assert drives.mount_path(1, imd_datei)
    text = drives._panels[1]._notice_label.text()
    assert ("mit UDOS nicht verwendbar" in text) == udos
    assert "unkomprimiert" not in text


def test_profiles_carry_the_hints():
    from app.profil import profil
    for m in ("a5120", "prg710"):
        assert len(profil(m).td0_hinweise) == 2 and len(profil(m).imd_hinweise) == 2
    for m in ("k8915", "pc1715", "p8000", "mc80"):
        assert profil(m).td0_hinweise == () and profil(m).imd_hinweise == ()


def test_main_window_hands_the_hints_to_the_drive_box():
    import inspect
    from app.ui import main_window
    src = inspect.getsource(main_window)
    assert "drives_widget.td0_hinweise" in src and "drives_widget.imd_hinweise" in src


def test_no_td0_hints_for_other_containers(drives, tmp_path):
    from app.profil import profil
    drives.td0_hinweise = profil("a5120").td0_hinweise
    ziel = str(tmp_path / "leer.hfe")
    assert drives.emulator.create_disk(0, ziel, "", False)
    drives._mounts[0] = (ziel, "", False)
    drives._update_notice(0)
    assert "TeleDisk" not in drives._panels[0]._notice_label.text()


@pytest.mark.parametrize("endung,art", [("td0", "td0"), ("imd", "imd")])
def test_save_as_roundtrip_and_refusal(drives, cqm_datei, tmp_path, monkeypatch, endung, art):
    from PySide6.QtWidgets import QFileDialog, QMessageBox
    meldungen = []
    monkeypatch.setattr(QMessageBox, "critical",
                        staticmethod(lambda *a, **k: meldungen.append(a[2])))

    def ziel(pfad):
        monkeypatch.setattr(QFileDialog, "getSaveFileName",
                            staticmethod(lambda *a, **k: (pfad, "")))

    assert drives.mount_path(0, cqm_datei)
    neu = tmp_path / f"kopie.{endung}"
    ziel(str(neu))
    drives._panels[0]._saveas_btn.click()
    assert not meldungen and neu.exists()
    assert drives.emulator.disk_container(0) == art
    assert drives._panels[0]._path_display.toolTip()   # Container-Tooltip folgt dem neuen Container

    # Leerdiskette (unformatiert): klare Meldung VOR dem Kern, Bindung bleibt
    leer = str(tmp_path / "leer.hfe")
    assert drives.emulator.create_disk(1, leer, "", False)
    drives.mount_path(1, leer)
    ziel(str(tmp_path / f"leer.{endung}"))
    drives._panels[1]._saveas_btn.click()
    assert meldungen and "unformatiert" in meldungen[-1]
    assert not (tmp_path / f"leer.{endung}").exists()
    assert drives.emulator.disk_container(1) == "hfe"

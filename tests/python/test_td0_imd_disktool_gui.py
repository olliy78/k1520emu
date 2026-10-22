"""TeleDisk (.td0) und ImageDisk (.imd) im k1520DiskTool (PySide6, headless).

Wie ``.cqm`` (test_cqm_disktool_gui.py): Filter, Oeffnen + Verzeichnis, Info-Dialog mit
Container-Angaben, Tooltip am Dateinamen, Schreibsperre-Logik (nicht gesperrt), Speichern
und Speichern unter, UDOS-Ablehnung vor dem Kern.
"""

import pytest

from conftest import requires_disk
import imd_bau
import td0_pack

pytestmark = requires_disk

pytest.importorskip("PySide6")


@pytest.fixture(scope="module")
def qt_app():
    from PySide6.QtWidgets import QApplication
    yield QApplication.instance() or QApplication([])


@pytest.fixture
def window(qt_app):
    from app.disktool.ui.main_window import MainWindow
    w = MainWindow()
    yield w
    w._close_tool()


@pytest.fixture
def td0_datei(tmp_path):
    p = tmp_path / "test.td0"
    p.write_bytes(td0_pack.beispiel())
    return p


@pytest.fixture
def imd_datei(tmp_path):
    daten = bytes([0xE5]) * 512
    spur = {"mode": 5, "cyl": 0, "head": 0, "n": 2,
            "sectors": [{"c": 0, "h": 0, "r": r, "typ": imd_bau.typ_aus_flags(daten, False, False, False),
                         "data": daten} for r in range(1, 10)]}
    p = tmp_path / "test.imd"
    p.write_bytes(imd_bau.baue([spur], kommentar=b"Testkommentar IMD"))
    return p


@pytest.fixture(params=["td0", "imd"])
def cpm_abbild(request, tmp_path):
    """Echtes, beschreibbares Abbild mit CP/M-Dateisystem (im Kern angelegt)."""
    from app.core_binding.k1520disk import DiskTool
    p = tmp_path / f"arbeit.{request.param}"
    DiskTool.create(p, "cpa800", "TEST").close()
    return p


def test_filters_know_td0_and_imd(window):
    assert "*.td0" in window.OEFFNEN_FILTER and "*.imd" in window.OEFFNEN_FILTER
    import inspect
    for dialog in (window._tar_neu_dialog, window._neue_diskette, window.save_as_dialog) \
            if all(hasattr(window, n) for n in ("_tar_neu_dialog", "_neue_diskette", "save_as_dialog")) \
            else ():
        quelle = inspect.getsource(dialog)
        assert "*.td0" in quelle and "*.imd" in quelle
    from app.disktool.ui.disk_view import ABBILD_ENDUNGEN
    assert ".td0" in ABBILD_ENDUNGEN and ".imd" in ABBILD_ENDUNGEN
    from app.disktool.ui import main_window
    quelle = inspect.getsource(main_window)
    assert quelle.count("TeleDisk-Abbild (*.td0)") >= 3
    assert quelle.count("ImageDisk-Abbild (*.imd)") >= 3


def test_open_td0_lists_container_info(window, td0_datei):
    assert window.open_image(str(td0_datei))
    t = window.tool
    assert t.ist_td0 and not t.ist_imd and not t.ist_cqm
    info = t.container_info()
    assert info["type"] == "td0" and info["version"] == "1.5" and info["rate"] == "250"
    from app.disktool.ui.disk_info_dialog import angaben_text
    text = angaben_text(t)
    for erwartet in ("TeleDisk (.td0)", "Version", "Datenrate", "Sektoren", "Kommentar:",
                     "Testvektor", "zweite Zeile"):
        assert erwartet in text, erwartet
    assert "Testvektor" in window.kopf.pfad.toolTip()
    assert "TeleDisk-Abbild" in window.kopf.pfad.toolTip()


def test_open_imd_lists_container_info(window, imd_datei):
    assert window.open_image(str(imd_datei))
    t = window.tool
    assert t.ist_imd and not t.ist_td0
    from app.disktool.ui.disk_info_dialog import angaben_text
    text = angaben_text(t)
    assert "ImageDisk (.imd)" in text and "Testkommentar IMD" in text and "Spuren" in text
    assert "ImageDisk-Abbild" in window.kopf.pfad.toolTip()


def test_warnings_appear_in_info_text(window, td0_datei, monkeypatch):
    """Warnungen des Kerns (Zeilen mit Zeilenumbruch) stehen im Info-Dialog und im Tooltip."""
    assert window.open_image(str(td0_datei))
    t = window.tool
    echt = t.container_info()
    falsch = dict(echt, warnings="Sektor 3: Pruefsumme\nSpur 1: Pseudosektor")
    monkeypatch.setattr(type(t), "container_info", lambda self: falsch)
    from app.disktool.ui.disk_info_dialog import angaben_text
    text = angaben_text(t)
    assert "Warnungen:" in text and "Pseudosektor" in text


def test_can_be_changed_saved_and_reloaded(window, cpm_abbild, tmp_path):
    art = cpm_abbild.suffix[1:]
    assert window.open_image(str(cpm_abbild))
    window.set_read_only(False)
    assert window.act_schreibschutz.isEnabled() and window.act_speichern.isEnabled()
    quelle = tmp_path / "HALLO.TXT"
    quelle.write_bytes(b"Hallo Container\r\n" * 50)
    window.tool.insert(quelle, "HALLO.TXT")
    assert window.save()
    window._close_tool()
    assert window.open_image(str(cpm_abbild))
    assert window.tool.container_info()["type"] == art
    assert [e.name for e in window.tool.list()] == ["HALLO.TXT"]
    ziel = tmp_path / "zurueck.txt"
    window.tool.extract("HALLO.TXT", ziel)
    assert ziel.read_bytes().startswith(b"Hallo Container")


@pytest.mark.parametrize("endung", ["td0", "imd"])
def test_save_as_and_create(window, cpm_abbild, tmp_path, endung):
    assert window.open_image(str(cpm_abbild))
    ziel = tmp_path / f"zweite.{endung}"
    assert window.save_as(ziel) and ziel.exists()
    assert window.tool.container_info()["type"] == endung
    neu = tmp_path / f"neu.{endung}"
    assert window.create_disk(neu, "cpa800", "OK")
    assert window.tool.container_info()["type"] == endung and not window.tool.read_only


def test_udos_is_refused_before_the_core(tmp_path):
    from app.disktool.ui.main_window import MainWindow
    for endung, name in (("td0", "TeleDisk"), ("imd", "ImageDisk")):
        grund = MainWindow._container_grund(tmp_path / f"x.{endung}", "udos_ds77")
        assert f"{name} kann die Sektorkontrollblöcke von UDOS nicht speichern" in grund
        assert MainWindow._container_grund(tmp_path / f"x.{endung}", "udos1715") == ""
        assert MainWindow._container_grund(tmp_path / f"x.{endung}", "cpa800") == ""


def test_tar_formats_expose_td0_and_imd_keys():
    from app.core_binding.k1520disk import DiskTool
    formate = DiskTool.tar_formate()
    assert formate and all("td0" in f and "imd" in f for f in formate)

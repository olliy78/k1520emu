"""OS/M in der Oberflaeche des DiskTool (doc/design/34_osm.md, AP 6).

OS/M ist CP/M auf den SCP-Formaten; die Oberflaeche braucht dafuer nichts Eigenes.
Geprueft wird, was der Anwender davon sieht: verstaendliche Namen im Dialog
„Neue Diskette", der Hinweis „ebenso gut osm800" im Protokoll, die Anzeige
einer OS/M-Systemdiskette und der Befund `osm.fdconf` im Pruefdialog.
Die Abbilder entstehen aus Bytes (kein Echtabbild noetig).
"""

import pytest

from conftest import requires_disk

pytestmark = requires_disk

pytest.importorskip("PySide6")

# Urlader-Merkmale (CpmFileSystem::osmUrlader): Code, FDCONF bei 0x7E, Kennung bei 0x80.
URLADER_CODE = bytes.fromhex("FD21480C3100403A7E0C")
TYP_FDCONF = {"osm640": 0x55, "osm800": 0x77}
SYSTEM_BYTES = {"osm640": 20 * 256 * 2, "osm800": 40 * 256}   # >= OSMLEN (28H x 256 B)


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


def _bootabbild(pfad, profil, fdconf=None):
    b = bytearray(SYSTEM_BYTES[profil])
    b[:len(URLADER_CODE)] = URLADER_CODE
    b[0x7E] = TYP_FDCONF[profil] if fdconf is None else fdconf
    b[0x80:0x88] = b"P8000SYS"
    pfad.write_bytes(bytes(b))
    return pfad


def test_neu_dialog_bietet_osm_mit_verstaendlicher_beschriftung(qt_app):
    from app.disktool.ui.main_window import neu_eintraege
    text = dict(neu_eintraege())
    for name in ("osm640", "osm800"):
        assert name in text
        assert text[name].startswith(f"{name} — OS/M (P8000)"), text[name]
    assert "Format 5" in text["osm640"] and "Format 7" in text["osm800"]


def test_leere_800k_nennt_osm800_im_protokoll(window, tmp_path):
    from app.core_binding.k1520disk import DiskTool
    pfad = tmp_path / "leer800.hfe"
    DiskTool.create(pfad, "cpa800", "LEER").close()
    assert window.open_image(str(pfad))
    protokoll = window.protokoll.toPlainText()
    assert "osm800" in protokoll, protokoll


def test_osm_systemdiskette_wird_als_osm_angezeigt(window, tmp_path):
    from app.core_binding.k1520disk import DiskTool
    for profil in ("osm640", "osm800"):
        boot = _bootabbild(tmp_path / f"{profil}.bin", profil)
        pfad = tmp_path / f"{profil}.img"
        DiskTool.create(pfad, profil, "SYS", boot).close()
        assert window.open_image(str(pfad))
        assert window.tool.filesystem == profil
        assert window.tool.unambiguous
        assert profil in window.kopf.angaben.text()
        assert "R/O" not in window.statusBar().currentMessage()


def test_falscher_fdconf_steht_im_pruefdialog(window, tmp_path):
    from app.core_binding.k1520disk import DiskTool
    from app.disktool.ui.fsck_dialog import FsckDialog
    boot = _bootabbild(tmp_path / "ok.bin", "osm800")
    pfad = tmp_path / "falsch.img"
    DiskTool.create(pfad, "osm800", "SYS", boot).close()
    roh = bytearray(pfad.read_bytes())
    assert roh[0x7E] == 0x77
    roh[0x7E] = 0x55                      # Typ 5 auf einer 5x1024-Diskette
    pfad.write_bytes(bytes(roh))

    assert window.open_image(str(pfad))
    assert window.tool.filesystem == "osm800"
    ids = [f.id for f in window.tool.findings().findings]
    assert "osm.fdconf" in ids
    protokoll = window.protokoll.toPlainText()
    assert "osm.fdconf" in protokoll

    dlg = FsckDialog(window.tool, window)
    sichtbar = " ".join(dlg.baum.topLevelItem(i).text(c)
                        for i in range(dlg.baum.topLevelItemCount())
                        for c in range(dlg.baum.columnCount()))
    assert "osm.fdconf" in sichtbar or "FDCONF" in sichtbar, sichtbar

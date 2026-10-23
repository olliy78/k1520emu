"""„Neue Diskette": Zeile „Abweichende Anzahl von Inodes" bei UNIX-Dateisystemen."""

import pytest

from conftest import requires_disk

pytestmark = requires_disk

pytest.importorskip("PySide6")


@pytest.fixture(scope="module")
def qt_app():
    from PySide6.QtWidgets import QApplication
    yield QApplication.instance() or QApplication([])


def _dialog(qt_app):
    from app.disktool.ui.main_window import neu_eintraege
    from app.disktool.ui.neu_dialog import NeuDialog
    return NeuDialog(neu_eintraege())


def _waehle(dlg, name):
    dlg.wahl.setCurrentIndex(dlg.wahl.findData(name))


def test_haken_nur_bei_unix_dateisystemen(qt_app):
    dlg = _dialog(qt_app)
    _waehle(dlg, "cpa800")
    assert dlg.unix.isHidden() and dlg.inodes() == 0
    _waehle(dlg, "mutos1700_720")
    assert not dlg.unix.isHidden()


def test_vorgabe_gesperrt_bis_der_haken_gesetzt_ist(qt_app):
    from app.core_binding.k1520disk import unix_standard_inodes
    dlg = _dialog(qt_app)
    _waehle(dlg, "mutos1700_720")
    assert unix_standard_inodes("mutos1700_720") == 448
    assert dlg.zahl.value() == 448 and not dlg.zahl.isEnabled()
    assert dlg.inodes() == 0                      # Vorgabe: nichts uebersteuert
    dlg.haken.setChecked(True)
    assert dlg.zahl.isEnabled()
    dlg.zahl.setValue(272)
    assert dlg.inodes() == 272
    dlg.haken.setChecked(False)                   # zurueck: Vorgabe wieder da, gesperrt
    assert dlg.zahl.value() == 448 and not dlg.zahl.isEnabled() and dlg.inodes() == 0


def test_dateisystemwechsel_setzt_die_vorgabe_neu(qt_app):
    dlg = _dialog(qt_app)
    _waehle(dlg, "mutos1700_720")
    dlg.haken.setChecked(True)
    dlg.zahl.setValue(272)
    _waehle(dlg, "mutos77")
    assert not dlg.haken.isChecked() and dlg.zahl.value() == 192 and dlg.inodes() == 0


def test_angelegte_diskette_traegt_die_gewaehlte_inode_zahl(tmp_path):
    from app.core_binding.k1520disk import DiskTool
    # 272 Inodes = 17 Inode-Bloecke: drei Bloecke (3 KB) mehr Daten als bei der Vorgabe (28 Bloecke)
    a = DiskTool.create(tmp_path / "a.dmk", "mutos1700_720", "A")
    b = DiskTool.create(tmp_path / "b.dmk", "mutos1700_720", "B", None, 272)
    frei_a, frei_b = a.volumes()[0].free, b.volumes()[0].free
    a.close(); b.close()
    assert frei_b - frei_a == (28 - 17) * 1024

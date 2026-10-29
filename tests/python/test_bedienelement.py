"""Bedienelement (Sicherheitsstecker) der K7637 — Bindung und Oberflaeche (AP S3b, Entwurf 43).

Die Tastaturbytes (TYP + A9H/AAH/ABH) pruefen die C++-Waechter (`K7637Bediensicherung*`);
hier: der Weg C-ABI -> ``K1520Emulator`` und die Einstellung/Aktion im ``a5120emu``.
"""

from pathlib import Path

import pytest

from conftest import requires_core

pytestmark = requires_core


def _emu(**kw):
    from app.core_binding.k1520 import K1520Emulator
    return K1520Emulator(**kw)


# ── Bindung ──────────────────────────────────────────────────────────────────

def test_vorgabe_ist_ohne_element():
    e = _emu()
    assert e.bedienelement() == 0


def test_stecken_und_ziehen_ueber_python():
    e = _emu()
    assert e.set_bedienelement(2)
    assert e.bedienelement() == 2
    assert e.set_bedienelement(0)
    assert e.bedienelement() == 0
    assert not e.set_bedienelement(7)          # ungueltige Kennung
    assert e.bedienelement() == 0


def test_kaltstartvorgabe_ueber_konfigtext():
    assert _emu(konfig={"bedienelement": 5}).bedienelement() == 5
    e = _emu(konfig={"bedienelement": "3", "bedienelement_geraet": "falsch"})
    assert e.bedienelement() == 3


def test_unbekannte_kennung_im_konfigtext():
    with pytest.raises(ValueError):
        _emu(konfig={"bedienelement": 9})


def test_k7636_hat_kein_element():
    e = _emu(konfig={"tastatur": "k7636"})
    assert e.bedienelement() == -1
    assert not e.set_bedienelement(1)


def test_andere_maschine_meldet_nicht_vorhanden():
    e = _emu(machine="k8915")
    assert e.bedienelement() == -1
    assert not e.set_bedienelement(1)


# ── Oberflaeche ──────────────────────────────────────────────────────────────

pytest.importorskip("PySide6", reason="PySide6 nicht installiert")


@pytest.fixture
def window(qapp):
    from app import config_io
    from app.ui.main_window import MainWindow

    gemerkt = Path(config_io.default_config_path())
    if gemerkt.exists():
        gemerkt.unlink()
    w = MainWindow()
    w.show()
    qapp.processEvents()
    yield w
    w.close()
    qapp.processEvents()


def _waehlen(window, qapp, wert):
    box = window.settings_widget.hardware_combos["bedienelement"]
    box.setCurrentIndex(box.findData(wert))
    qapp.processEvents()


def test_vorgabe_keins_und_aktion_gesperrt(window):
    assert window._hardware["bedienelement"] == "0"
    assert window.emulator.bedienelement() == 0
    assert not window.act_bedienelement.isEnabled()
    assert not window.act_bedienelement.shortcut().toString()      # kein Kuerzel


def test_einstellung_wirkt_und_aktion_schaltet(window, qapp):
    _waehlen(window, qapp, "1")                  # 01 Chefschalter, Kaltstart
    assert window.emulator.bedienelement() == 1
    assert window.act_bedienelement.isEnabled() and window.act_bedienelement.isChecked()
    window.act_bedienelement.setChecked(False)   # ziehen
    assert window.emulator.bedienelement() == 0
    window.act_bedienelement.setChecked(True)    # stecken
    assert window.emulator.bedienelement() == 1
    _waehlen(window, qapp, "0")
    assert window.emulator.bedienelement() == 0
    assert not window.act_bedienelement.isEnabled()


def test_einstellung_wird_gespeichert(window, qapp):
    _waehlen(window, qapp, "6")
    assert window._gather_config()["general"]["bedienelement"] == "6"


def test_k7636_graut_das_feld_aus(window, qapp):
    box = window.settings_widget.hardware_combos["tastatur"]
    box.setCurrentIndex(box.findData("k7636"))
    qapp.processEvents()
    assert not window.profil.hardware_wirkt("bedienelement", window._model, window._hardware)
    assert not window.act_bedienelement.isEnabled()

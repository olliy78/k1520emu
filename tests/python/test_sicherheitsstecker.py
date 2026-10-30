"""Sicherheitsstecker (Sicherheitsstecker) der K7637 — Bindung und Oberflaeche (AP S3b, Entwurf 43).

Die Tastaturbytes (TYP + A9H/AAH/ABH) pruefen die C++-Waechter (`K7637Sicherheitsstecker*`);
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
    assert e.sicherheitsstecker() == 0


def test_stecken_und_ziehen_ueber_python():
    e = _emu()
    assert e.set_sicherheitsstecker(2)
    assert e.sicherheitsstecker() == 2
    assert e.set_sicherheitsstecker(0)
    assert e.sicherheitsstecker() == 0
    assert not e.set_sicherheitsstecker(7)          # ungueltige Kennung
    assert e.sicherheitsstecker() == 0


def test_kaltstartvorgabe_ueber_konfigtext():
    assert _emu(konfig={"sicherheitsstecker": 5}).sicherheitsstecker() == 5
    e = _emu(konfig={"sicherheitsstecker": "3", "sicherheitsstecker_geraet": "falsch"})
    assert e.sicherheitsstecker() == 3


def test_unbekannte_kennung_im_konfigtext():
    with pytest.raises(ValueError):
        _emu(konfig={"sicherheitsstecker": 9})


def test_k7636_nimmt_den_stecker_an():
    # S4: die K7636 meldet dieselben Codes (Annahme [?], kein ROM-Abzug).
    e = _emu(konfig={"tastatur": "k7636"})
    assert e.sicherheitsstecker() == 0
    assert e.set_sicherheitsstecker(1)
    assert e.sicherheitsstecker() == 1


def test_andere_maschine_meldet_nicht_vorhanden():
    e = _emu(machine="k8915")
    assert e.sicherheitsstecker() == -1
    assert not e.set_sicherheitsstecker(1)


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
    box = window.settings_widget.hardware_combos["sicherheitsstecker"]
    box.setCurrentIndex(box.findData(wert))
    qapp.processEvents()


def test_alter_konfigschluessel_bedienelement_wird_noch_gelesen(window):
    """Der Zweig lief schon beim Anwender mit `general.bedienelement`: Rueckfall beim Laden."""
    hw = window.profil.hardware_normalisieren({"bedienelement": "4"}, "a5120")
    assert hw["sicherheitsstecker"] == "4"
    hw = window.profil.hardware_normalisieren({"bedienelement": "4", "sicherheitsstecker": "2"}, "a5120")
    assert hw["sicherheitsstecker"] == "2"           # der neue Schluessel gewinnt


def test_vorgabe_keins_und_aktion_gesperrt(window):
    assert window._hardware["sicherheitsstecker"] == "0"
    assert window.emulator.sicherheitsstecker() == 0
    assert not window.act_sicherheitsstecker.isEnabled()
    assert not window.act_sicherheitsstecker.shortcut().toString()      # kein Kuerzel


def test_einstellung_wirkt_und_aktion_schaltet(window, qapp):
    _waehlen(window, qapp, "1")                  # 01 Chefschalter, Kaltstart
    assert window.emulator.sicherheitsstecker() == 1
    assert window.act_sicherheitsstecker.isEnabled() and window.act_sicherheitsstecker.isChecked()
    window.act_sicherheitsstecker.setChecked(False)   # ziehen
    assert window.emulator.sicherheitsstecker() == 0
    window.act_sicherheitsstecker.setChecked(True)    # stecken
    assert window.emulator.sicherheitsstecker() == 1
    _waehlen(window, qapp, "0")
    assert window.emulator.sicherheitsstecker() == 0
    assert not window.act_sicherheitsstecker.isEnabled()


def test_einstellung_wird_gespeichert(window, qapp):
    _waehlen(window, qapp, "6")
    assert window._gather_config()["general"]["sicherheitsstecker"] == "6"


def test_k7636_laesst_das_feld_wirken(window, qapp):
    # S4: auch die K7636 nimmt den Stecker an (Annahme [?], gleiche Codes wie die K7637).
    box = window.settings_widget.hardware_combos["tastatur"]
    box.setCurrentIndex(box.findData("k7636"))
    qapp.processEvents()
    assert window.profil.hardware_wirkt("sicherheitsstecker", window._model, window._hardware)

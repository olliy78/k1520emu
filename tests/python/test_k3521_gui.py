"""K3521.30 in der Oberflaeche (Entwurf 33 N10h): Anzahl + Startadresse, Pruefung, Modellvorgabe,
Konfiguration, Ablagedatei je Karte.  Die Karten selbst pruefen `K3521.*`/`py_k3521`."""

from pathlib import Path

import pytest
import yaml

from conftest import requires_core

pytestmark = requires_core


@pytest.fixture
def konfig_ordner(tmp_path, monkeypatch):
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "xdg"))
    from app import config_io
    ordner = Path(config_io.default_config_dir())
    ordner.mkdir(parents=True, exist_ok=True)
    return ordner


def _fenster(qapp):
    from app import profil
    from app.ui.main_window import MainWindow
    w = MainWindow(None, profil=profil.profil("a5120"))
    w.show()
    qapp.processEvents()
    return w


def _zu(w, qapp):
    w.close()
    qapp.processEvents()


def _anzahl(w, qapp, n):
    w.settings_widget.k3521_combo.setCurrentIndex(w.settings_widget.k3521_combo.findData(n))
    qapp.processEvents()


def _start(w, qapp, text):
    w.settings_widget.k3521_start_edit.setText(text)
    w.settings_widget.k3521_start_edit.editingFinished.emit()
    qapp.processEvents()


# ── reine Pruefung ───────────────────────────────────────────────────────────

@pytest.mark.parametrize("anzahl,text,start", [
    (1, "8000", 0x8000), (0, "f000", 0xF000), (4, "0", 0), (2, "9000h", 0x9000), (4, "0xC000", 0xC000),
])
def test_pruefung_gueltig(anzahl, text, start):
    from app import k3521
    assert k3521.pruefe(anzahl, text) == (start, "")


@pytest.mark.parametrize("anzahl,text", [
    (1, ""), (1, "xyz"), (1, "8800"), (1, "10000"), (2, "F000"), (4, "D000"),
])
def test_pruefung_ungueltig(anzahl, text):
    from app import k3521
    start, grund = k3521.pruefe(anzahl, text)
    assert start is None and grund


def test_basen_und_konfigwert():
    from app import k3521
    assert k3521.basen(3, 0x8000) == [0x8000, 0x9000, 0xA000]
    assert k3521.konfig_wert(2, 0x8000) == "8000+9000"
    assert k3521.konfig_wert(0, 0x8000) is None


# ── Fenster ──────────────────────────────────────────────────────────────────

def test_vorgabe_keine_bei_allen_modellen_und_nur_a5120_bietet_sie(qapp, konfig_ordner):
    from app import profil
    assert profil.profil("a5120").k3521_wahl
    for m in ("k8915", "prg710", "pc1715", "p8000", "mc80"):
        assert not profil.profil(m).k3521_wahl
    w = _fenster(qapp)
    try:
        sw = w.settings_widget
        assert sw.k3521_value() == (0, 0x8000) and w.emulator.k3521_anzahl == 0
        for modell in ("a5120", "a5130", "k8924"):
            i = sw.model_combo.findData(modell)
            sw.model_combo.setCurrentIndex(i)
            sw.model_combo.activated.emit(i)
            qapp.processEvents()
            assert w.emulator.k3521_anzahl == 0
    finally:
        _zu(w, qapp)


def test_anzahl_und_start_bauen_die_karten_lueckenlos(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _anzahl(w, qapp, 2)
        assert w.emulator.k3521_anzahl == 2
        assert (w.emulator.k3521_basis_n(0), w.emulator.k3521_basis_n(1)) == (0x8000, 0x9000)
        _start(w, qapp, "4000")
        assert (w.emulator.k3521_basis_n(0), w.emulator.k3521_basis_n(1)) == (0x4000, 0x5000)
        assert w.settings_widget.k3521_start_edit.text() == "4000"
    finally:
        _zu(w, qapp)


def test_ungueltige_eingabe_wird_sichtbar_abgewiesen(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        sw = w.settings_widget
        _anzahl(w, qapp, 1)
        alt = w.emulator
        for text in ("xyz", "8800", "10000"):
            _start(w, qapp, text)
            assert not sw.k3521_hinweis.isHidden() and "abgewiesen" in sw.k3521_hinweis.text()
            assert sw.k3521_start_edit.text() == "8000"
            assert w.emulator is alt and sw.k3521_value() == (1, 0x8000)
        # drei Karten ab E000 enden hinter FFFFH: Anzahl zurueck
        _start(w, qapp, "e000")
        assert sw.k3521_value() == (1, 0xE000)
        _anzahl(w, qapp, 3)
        assert sw.k3521_value() == (1, 0xE000) and sw.k3521_combo.currentData() == 1
        assert not sw.k3521_hinweis.isHidden()
    finally:
        _zu(w, qapp)


def test_konfiguration_merkt_die_karten_und_eine_datei_je_karte(qapp, konfig_ordner):
    from app import k3521
    w = _fenster(qapp)
    try:
        _anzahl(w, qapp, 2)
        w.emulator.k3521_flush_n(0)
        w.emulator.k3521_flush_n(1)
        for n in (0, 1):
            p = Path(k3521.ablage_pfad(w.profil.programm, n))
            assert p.parent == konfig_ordner and p.exists() and p.stat().st_size == 4096
        w._autosave_now()
        general = yaml.safe_load(Path(w._konfig_pfad()).read_text(encoding="utf-8"))["general"]
        assert general["k3521_anzahl"] == 2 and general["k3521_start"] == "8000"
    finally:
        _zu(w, qapp)
    w = _fenster(qapp)
    try:
        assert w.emulator.k3521_anzahl == 2
        assert w.settings_widget.k3521_value() == (2, 0x8000)
    finally:
        _zu(w, qapp)


def test_inhalt_ueberlebt_den_neuaufbau_ueber_die_datei(qapp, konfig_ordner):
    from app import k3521
    w = _fenster(qapp)
    try:
        _anzahl(w, qapp, 1)
        w.emulator.mem_write(0x8123, 0x5A)
        w.emulator.k3521_flush_n(0)
        assert w.emulator.k3521_peek_n(0, 0x123) == 0x5A
        datei = Path(k3521.ablage_pfad(w.profil.programm, 0))
        assert datei.read_bytes()[0x123] == 0x5A
        # Eine neue Maschine liest die Karte beim Binden aus der Datei (vor dem ersten Lauf).
        from app.core_binding.k1520 import K1520Emulator
        e = K1520Emulator(konfig={"k3521": "8000+9000"})
        e.k3521_datei_n(0, datei)
        assert e.k3521_peek_n(0, 0x123) == 0x5A and e.k3521_peek_n(1, 0x123) == 0xFF
    finally:
        _zu(w, qapp)


def test_auslieferung_traegt_keine_k3521_werte(qapp, konfig_ordner):
    from app import config_io, profil
    vorgabe = config_io.standard_konfiguration(profil.profil("a5120"))
    assert not any(k.startswith("k3521") for k in vorgabe.get("general", {}))

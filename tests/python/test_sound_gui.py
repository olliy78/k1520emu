"""SOUND-Karte K1520 in der Oberflaeche (Entwurf 51, AP S8).

Abschnitt *Einstellungen ▸ Allgemein ▸ Soundkarte* in allen Programmen ausser p8000emu, Vorauswahl der
Basis, Konfigurationsrundlauf (``general.sound``), Konflikt-Anzeige, Statuszeilenfeld ``Ton``, Echtzeit-
Meldung an den Kern beim Tempowechsel, Tonaktionen ohne Tastenkuerzel.  Die Tonausgabe ist hier immer das
Stumm-Backend (``K1520_AUDIO=stumm``); Dateidialoge werden umgangen (ein ``exec()`` stuende headless).
"""

from pathlib import Path

import pytest
import yaml

from conftest import requires_core

pytestmark = requires_core

MIT_KARTE = ["a5120", "k8915", "prg710", "pc1715", "mc80"]
BASIS = {"a5120": 0x38, "k8915": 0x38, "prg710": 0x38, "pc1715": 0x38, "mc80": 0x38}


@pytest.fixture(autouse=True)
def umgebung(tmp_path, monkeypatch):
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "xdg"))
    monkeypatch.setenv("K1520_DATA", str(tmp_path / "daten"))
    monkeypatch.setenv("K1520_AUDIO", "stumm")
    from app import config_io
    Path(config_io.default_config_dir()).mkdir(parents=True, exist_ok=True)
    from PySide6.QtWidgets import QMessageBox
    for n in ("critical", "warning", "information", "question"):
        monkeypatch.setattr(QMessageBox, n, staticmethod(
            lambda *a, **k: pytest.fail("unerwartetes Meldungsfenster")))


def _fenster(qapp, maschine="a5120"):
    from app import profil
    from app.ui.main_window import MainWindow
    w = MainWindow(None, profil=profil.profil(maschine))
    w.show()
    qapp.processEvents()
    return w


def _zu(w, qapp):
    w.close()
    qapp.processEvents()


@pytest.mark.parametrize("maschine", MIT_KARTE + ["p8000"])
def test_abschnitt_nur_an_steckplatzfaehigen_profilen(qapp, maschine):
    w = _fenster(qapp, maschine)
    try:
        g = w.settings_widget.sound_gruppe
        if maschine == "p8000":
            assert g.isHidden() and not hasattr(w, "act_ton_stumm")
            assert not w.profil.sound_wahl
            return
        assert not g.isHidden()
        assert not g.karte_box.isChecked() and not w.emulator.sound_present()
        assert g.basis_combo.currentData() == BASIS[maschine]
        assert g.basis_combo.count() == 32                       # Vielfache von 8
        assert g.chip_combo.currentData() == "ay8910"
        assert g.takt_combo.currentData() == "quarz_halbe" and g.quarz_spin.value() == 3_579_545
        assert g.kanal_combo.currentData() == "acb" and not g.tiefpass_box.isChecked()
        assert [b.isChecked() for b in g.kaskade_boxen] == [False] * 3
        assert [b.text() for b in g.kaskade_boxen] == ["TO0 → TR1", "TO1 → TR2", "TO2 → TR3"]
    finally:
        _zu(w, qapp)


def test_pc1715w_nimmt_die_vorauswahl_e0(qapp):
    from app import profil
    p = profil.profil("pc1715")
    assert p.karten_vorauswahl("sound", "pc1715w") == 0xE0 and p.karten_vorauswahl("sound", "pc1715") == 0x38
    w = _fenster(qapp, "pc1715")
    try:
        w.settings_widget.model_combo.setCurrentIndex(w.settings_widget.model_combo.findData("pc1715w"))
        qapp.processEvents()
        g = w.settings_widget.sound_gruppe
        g.karte_box.setChecked(True)
        qapp.processEvents()
        assert w.emulator.sound_present() and w._sound["basis"] == 0xE0
    finally:
        _zu(w, qapp)


def test_stecken_aendert_die_maschine_und_die_konfiguration_haelt_es(qapp):
    from app import config_io, profil
    w = _fenster(qapp, "a5120")
    try:
        g = w.settings_widget.sound_gruppe
        g.chip_combo.setCurrentIndex(g.chip_combo.findData("ay8912"))
        g.kanal_combo.setCurrentIndex(g.kanal_combo.findData("mono"))
        g.takt_combo.setCurrentIndex(g.takt_combo.findData("quarz"))
        g.tiefpass_box.setChecked(True)
        g.kaskade_boxen[1].setChecked(True)
        g.karte_box.setChecked(True)
        qapp.processEvents()
        assert w.emulator.sound_present()
        assert w._sound["chip"] == "ay8912" and w._sound["kaskade"] == 2
        w._lautstaerke_setzen(35)
        w.act_ton_stumm.setChecked(True)
        w._autosave_now()
    finally:
        _zu(w, qapp)
    datei = Path(config_io.default_config_dir()) / profil.profil("a5120").konfig_datei
    general = yaml.safe_load(datei.read_text(encoding="utf-8"))["general"]
    assert general["sound"] == {"enabled": True, "basis": "38", "chip": "ay8912", "takt": "quarz",
                                "quarz_hz": 3579545, "kanal": "mono", "tiefpass": True,
                                "kaskade": [False, True, False]}
    assert general["lautstaerke"] == 35 and general["stumm"] is True
    w2 = _fenster(qapp, "a5120")
    try:
        assert w2.emulator.sound_present() and w2.act_ton_stumm.isChecked()
        assert w2.settings_widget.sound_gruppe.chip_combo.currentData() == "ay8912"
        assert w2._lautstaerke == 35
        regs = w2.emulator.sound_regs()
        assert regs is not None and len(regs) == 16
    finally:
        _zu(w2, qapp)


def test_fehlender_abschnitt_und_kaputte_werte_fallen_auf_die_vorgabe():
    from app import sound
    assert sound.normalize(None, 0x38) == sound.vorgabe(0x38)
    z = sound.normalize({"enabled": "ja", "basis": "39", "chip": "xx", "takt": 5, "quarz_hz": "viel",
                         "kanal": None, "tiefpass": 1, "kaskade": [True]}, 0x38)
    assert z == sound.vorgabe(0x38)
    assert sound.kern_param(z) is None
    assert sound.lautstaerke_lesen(250) == sound.LAUTSTAERKE_VORGABE


def test_belegte_basis_ist_markiert_und_nicht_waehlbar(qapp):
    w = _fenster(qapp, "a5120")
    try:
        g = w.settings_widget.sound_gruppe
        i = g.basis_combo.findData(0x00)           # 00H..07H: feste Tore der ZRE
        assert "belegt" in g.basis_combo.itemText(i)
        assert not g.basis_combo.model().item(i).isEnabled()
        assert "belegt" not in g.basis_combo.itemText(g.basis_combo.findData(0x38))
    finally:
        _zu(w, qapp)


def test_konflikt_mit_der_gewaehlten_gide_wird_angezeigt(qapp):
    from app import kartenwahl
    # GIDE D0H belegt D4H..DFH; Soundkarten auf D0H (Tore D0H..D7H) und D8H kollidieren im Dialog mit ihr,
    # auch ungesteckt; E0H bleibt frei.
    zeilen = {z.basis: z for z in kartenwahl.eintraege(kartenwahl.SOUND, None, {"gide": 0xD0})}
    assert zeilen[0xD8].belegt == "GIDE" and "belegt: GIDE" in zeilen[0xD8].text
    assert zeilen[0xD0].belegt == "GIDE"
    assert zeilen[0xE0].belegt == "" and zeilen[0xC8].belegt == ""


def test_statuszeile_zeigt_den_ton_nur_mit_karte(qapp):
    w = _fenster(qapp, "a5120")
    try:
        feld = w.status_widget.ton
        w._update_ton()
        assert feld.isHidden()
        w.settings_widget.sound_gruppe.karte_box.setChecked(True)
        qapp.processEvents()
        w._update_ton()
        assert not feld.isHidden()
        assert feld.text.text() == "Ton: stumm"                 # Stumm-Backend
        assert w.emulator.audio_status() == "stumm"
        w.settings_widget.sound_gruppe.karte_box.setChecked(False)
        qapp.processEvents()
        w._update_ton()
        assert feld.isHidden()
    finally:
        _zu(w, qapp)


def test_tempowechsel_meldet_echtzeit_an_den_kern(qapp):
    w = _fenster(qapp, "a5120")
    try:
        w.settings_widget.sound_gruppe.karte_box.setChecked(True)
        qapp.processEvents()
        gemeldet = []
        w.emulator.audio_echtzeit = lambda an: gemeldet.append(bool(an))
        w._apply_speed(5.0)
        w._apply_speed(1.0)
        assert gemeldet == [False, True]
    finally:
        _zu(w, qapp)


def test_stumm_und_lautstaerke_gehen_an_den_kern(qapp):
    w = _fenster(qapp, "a5120")
    try:
        w.settings_widget.sound_gruppe.karte_box.setChecked(True)
        qapp.processEvents()
        spion = []
        w.emulator.audio_stumm = lambda an: spion.append(("stumm", bool(an)))
        w.emulator.audio_lautstaerke = lambda v: spion.append(("laut", round(v, 2)))
        w.act_ton_stumm.setChecked(True)
        w._lautstaerke_setzen(40)
        w._lautstaerke_setzen(500)
        assert spion == [("stumm", True), ("laut", 0.4), ("laut", 1.0)]
    finally:
        _zu(w, qapp)


def test_tonaktionen_ohne_kuerzel_und_nur_mit_karte_bedienbar(qapp):
    w = _fenster(qapp, "a5120")
    try:
        for n in ("ton_stumm", "ton_lautstaerke", "ton_mitschnitt"):
            a = getattr(w, f"act_{n}")
            assert a.shortcut().isEmpty()
            assert not a.isEnabled()
        w.settings_widget.sound_gruppe.karte_box.setChecked(True)
        qapp.processEvents()
        for n in ("ton_stumm", "ton_lautstaerke", "ton_mitschnitt"):
            assert getattr(w, f"act_{n}").isEnabled()
    finally:
        _zu(w, qapp)


def test_mitschnitt_schreibt_eine_wav_datei(qapp, tmp_path, monkeypatch):
    from PySide6.QtWidgets import QFileDialog
    ziel = tmp_path / "ton.wav"
    monkeypatch.setattr(QFileDialog, "getSaveFileName", staticmethod(lambda *a, **k: (str(ziel), "")))
    w = _fenster(qapp, "a5120")
    try:
        w.settings_widget.sound_gruppe.karte_box.setChecked(True)
        qapp.processEvents()
        w.act_ton_mitschnitt.trigger()
        assert w._wav_aktiv and "beenden" in w.act_ton_mitschnitt.text()
        w.act_ton_mitschnitt.trigger()
        assert not w._wav_aktiv
        assert ziel.exists() and ziel.read_bytes()[:4] == b"RIFF"
    finally:
        _zu(w, qapp)


def test_takt_des_a5120_entspricht_dem_kern(qapp):
    from app import profil
    from app.core_binding.k1520 import K1520Emulator
    for name in ("a5120", "k8915", "prg710", "pc1715"):
        p = profil.profil(name)
        m = p.modell_maschine(p.standard_modell())
        assert p.modell_nenntakt(p.standard_modell())[0] == K1520Emulator(machine=m).nenntakt_hz(), name

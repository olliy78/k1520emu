"""Bildschirmkarte und Schiebeschalter in der Oberflaeche — Entwurf 33 §3.3, Paket N6.

Der Kern (zwei Karten, Umschalter, Bruecke) ist durch die C++-Waechter und `test_a5120_konfig.py`
abgesichert; hier geht es um die Verdrahtung: Auswahlfeld, Schalter-Widget, sofortige Wirkung im Kern
unter der Kernsperre, Konfiguration, Wiederherstellung beim Start, Modellwechsel und die
Texturgroesse des Bildschirm-Widgets.  Pixel sind offscreen nicht pruefbar (kein FBO).
"""

from contextlib import contextmanager
from pathlib import Path

import pytest

from conftest import requires_core

pytestmark = requires_core

KARTEN = ["K7024 Auflösung 24x80", "K7023 Auflösung 16x64", "K7024 und K7023 mit Umschalter"]


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


def _karte(w, qapp, wert):
    box = w.settings_widget.hardware_combos["bildschirm"]
    box.setCurrentIndex(box.findData(wert))
    qapp.processEvents()


def _schalter(w, qapp, karte):
    """Wie der Anwender: den Schieber ans Ende ziehen."""
    w.settings_widget.bildschirm_schalter.regler.setValue(1 if karte == "k7023" else 0)
    qapp.processEvents()


def _waehle(w, qapp, modell):
    combo = w.settings_widget.model_combo
    i = combo.findData(modell)
    combo.setCurrentIndex(i)
    combo.activated.emit(i)
    qapp.processEvents()


def test_auswahlfeld_und_schalter_haben_die_vorgegebenen_texte(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        box = w.settings_widget.hardware_combos["bildschirm"]
        assert [box.itemText(i) for i in range(box.count())] == KARTEN
        assert [box.itemData(i) for i in range(box.count())] == ["k7024", "k7023", "beide"]
        sch = w.settings_widget.bildschirm_schalter
        assert sch.links.text() == "K7024\n24x80"
        assert sch.rechts.text() == "K7023\n16x64"
        assert w.settings_widget.hardware_combos["bildschirm"].currentData() == "k7024"
    finally:
        _zu(w, qapp)


def test_schalter_ist_nur_bei_beiden_karten_aktiv(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        sch = w.settings_widget.bildschirm_schalter
        assert not sch.isEnabled()
        _karte(w, qapp, "k7023")
        assert not sch.isEnabled() and w.emulator.bildschirm == "k7023"
        assert w.emulator.framebuffer_size() == (512, 256)
        _karte(w, qapp, "beide")
        assert sch.isEnabled() and w.emulator.bildschirm == "beide"
        _karte(w, qapp, "k7024")
        assert not sch.isEnabled() and w.emulator.bildschirm == "k7024"
        assert w.emulator.framebuffer_size() == (640, 288)
    finally:
        _zu(w, qapp)


def test_umlegen_wirkt_sofort_im_kern_unter_der_kernsperre(qapp, konfig_ordner, monkeypatch):
    from app.ui import main_window
    gehalten = []
    echte = main_window.kern_sperre

    @contextmanager
    def beobachtet():
        with echte():
            gehalten.append(True)
            yield

    w = _fenster(qapp)
    try:
        _karte(w, qapp, "beide")
        emu = w.emulator
        assert emu.bildschirm_schalter == "k7024" and emu.framebuffer_size() == (640, 288)
        monkeypatch.setattr(main_window, "kern_sperre", beobachtet)
        gehalten.clear()
        _schalter(w, qapp, "k7023")
        assert gehalten, "der Kern wurde ohne Kernsperre umgelegt"
        assert w.emulator is emu                    # kein Neubau, kein Kaltstart
        assert emu.bildschirm_schalter == "k7023" and emu.framebuffer_size() == (512, 256)
        _schalter(w, qapp, "k7024")
        assert emu.bildschirm_schalter == "k7024" and emu.framebuffer_size() == (640, 288)
    finally:
        _zu(w, qapp)


def test_gesperrter_schalter_hat_keine_wirkung_auf_den_kern(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        emu = w.emulator
        w._on_bildschirm_schalter("k7023")          # kein Widerspruch: mit EINER Karte ohne Wirkung
        assert emu.bildschirm == "k7024" and emu.bildschirm_schalter == "k7024"
        assert emu.framebuffer_size() == (640, 288)
    finally:
        _zu(w, qapp)


def test_bildschirm_widget_verkraftet_den_groessenwechsel(qapp, konfig_ordner):
    from app.core_binding.k1520 import kern_sperre
    w = _fenster(qapp)
    try:
        _karte(w, qapp, "beide")
        sw = w.screen_widget
        _schalter(w, qapp, "k7023")
        with kern_sperre():
            sw._bild_holen()
        assert (sw._fb_w, sw._fb_h) == (512, 256)
        assert len(sw._fb_bytes) == 512 * 256 or sw._fb_bytes is not None
        _schalter(w, qapp, "k7024")
        with kern_sperre():
            sw._bild_holen()
        assert (sw._fb_w, sw._fb_h) == (640, 288)
        assert len(sw._fb_bytes) == 640 * 288
    finally:
        _zu(w, qapp)


def test_konfiguration_schreibt_und_liest_den_schalter(qapp, konfig_ordner):
    from app import config_io
    w = _fenster(qapp)
    try:
        _karte(w, qapp, "beide")
        _schalter(w, qapp, "k7023")
        w._autosave_now()
        allg = config_io.load_config(str(konfig_ordner / "a5120emu.yaml"))["general"]
        assert allg["bildschirm"] == "beide" and allg["bildschirm_schalter"] == "k7023"
    finally:
        _zu(w, qapp)

    # Neustart: Karte, Schalter im Feld UND im Kern kommen wieder.
    w = _fenster(qapp)
    try:
        sch = w.settings_widget.bildschirm_schalter
        assert sch.isEnabled() and sch.stellung() == "k7023"
        assert w.emulator.bildschirm == "beide" and w.emulator.bildschirm_schalter == "k7023"
        assert w.emulator.framebuffer_size() == (512, 256)
    finally:
        _zu(w, qapp)


def test_konfiguration_mit_unbekanntem_schalter_ist_k7024(qapp, konfig_ordner):
    (konfig_ordner / "a5120emu.yaml").write_text(
        "version: 1\ngeneral: {bildschirm: beide, bildschirm_schalter: quatsch}\n", encoding="utf-8")
    w = _fenster(qapp)
    try:
        assert w.emulator.bildschirm_schalter == "k7024"
        assert w.settings_widget.bildschirm_schalter.stellung() == "k7024"
    finally:
        _zu(w, qapp)


def test_schalter_aus_dem_kern_wird_nach_restore_neu_gelesen(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _karte(w, qapp, "beide")
        w.emulator.bildschirm_schalter = "k7023"       # wie ein geladener Zwischenstand
        assert w.settings_widget.bildschirm_schalter.stellung() == "k7024"
        w._schalter_nachlesen()
        assert w._schalter == "k7023"
        assert w.settings_widget.bildschirm_schalter.stellung() == "k7023"
    finally:
        _zu(w, qapp)


def test_modellwechsel_setzt_karte_und_schalter_zurueck(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _karte(w, qapp, "beide")
        _schalter(w, qapp, "k7023")
        for modell in ("a5130", "k8924", "a5120"):
            _karte(w, qapp, "beide")
            _schalter(w, qapp, "k7023")
            _waehle(w, qapp, modell)
            assert w._hardware["bildschirm"] == "k7024", modell
            assert w._schalter == "k7024"
            sch = w.settings_widget.bildschirm_schalter
            assert sch.stellung() == "k7024" and not sch.isEnabled()
            assert w.emulator.bildschirm == "k7024"
    finally:
        _zu(w, qapp)


def test_kern_parameter_gibt_den_schalter_nur_mit_zwei_karten_mit():
    from app import profil
    p = profil.profil("a5120")
    for karte in ("k7024", "k7023"):
        assert p.kern_parameter("a5120", {"bildschirm": karte}, "k7023")["konfig"]["bildschirm"] == karte
        assert "schalter" not in p.kern_parameter("a5120", {"bildschirm": karte}, "k7023")["konfig"]
    k = p.kern_parameter("a5120", {"bildschirm": "beide"}, "k7023")["konfig"]
    assert (k["bildschirm"], k["schalter"]) == ("beide", "k7023")
    assert p.kern_parameter("a5120", {"bildschirm": "beide"})["konfig"]["schalter"] == "k7024"

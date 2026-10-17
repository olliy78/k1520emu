"""AUP 062-8531 in der Oberflaeche (Entwurf 33 N10e).

Der Kern (Registermodell, Tabellen, Lampen) haben ``AUP.*``/``AupMaschine.*``/``py_aup`` im Griff;
hier geht es um die Verdrahtung: Optionsfeld, Kasten nur mit Karte, Tastendruck -> ``aup_taste``
mit Kennung und Position (Ersatzemulator), Schluesselschalter, Lampen, Hupe, Modellwechsel,
Konfiguration.  Ein Ton wird nie wirklich ausgegeben (``_ton`` ist ersetzt).
"""

from pathlib import Path

import pytest
import yaml

from conftest import requires_core

pytestmark = requires_core


class Ersatz:
    """Nur die Teile von ``K1520Emulator``, die der Kasten benutzt."""

    def __init__(self):
        self.aufrufe = []
        self.aup_installed = True
        self.aup_lampen = 0
        self.aup_hupe = 0
        self.stand = {1: False, 2: False}

    def aup_taste(self, tastatur, pos, gedrueckt=True):
        self.aufrufe.append(("taste", tastatur, pos, gedrueckt))
        return True

    def aup_schluessel(self, n, an):
        self.aufrufe.append(("schluessel", n, an))
        self.stand[n] = bool(an)
        return True

    def aup_schluessel_stand(self, n):
        return self.stand[n]


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


def _menue(w, titel):
    for a in w.menuBar().actions():
        if a.text() == titel:
            return list(a.menu().actions())
    raise AssertionError(f"kein Menü {titel}")


def _waehle(w, qapp, modell):
    combo = w.settings_widget.model_combo
    i = combo.findData(modell)
    combo.setCurrentIndex(i)
    combo.activated.emit(i)
    qapp.processEvents()


def _kasten(qapp, ersatz=None):
    from app.ui.keyboard_k7633 import AupWidget
    ersatz = ersatz or Ersatz()
    k = AupWidget(ersatz)
    k.show()
    qapp.processEvents()
    return k, ersatz


# ── Option und Kasten ────────────────────────────────────────────────────────

def test_vorgabe_aus_bei_allen_modellen_und_kasten_folgt_der_karte(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        sw = w.settings_widget
        assert sw.aup_box.text() == "AUP (Sparkasse)"
        for modell in ("a5120", "a5130", "k8924"):
            _waehle(w, qapp, modell)
            assert sw.aup_value() == (False, False, "led")
            assert not w.emulator.aup_installed and not w.aup_dock.isVisible()
            assert not w.act_dock_aup.isVisible()
        assert w.act_dock_aup in _menue(w, "&Ansicht")
        # die vorhandene Tastatur bleibt, was das Modell vorgibt
        assert w.keyboard_widget is not w.aup_widget

        alt = w.emulator
        sw.aup_box.setChecked(True)
        qapp.processEvents()
        assert w.emulator is not alt and w.emulator.aup_installed
        assert not w.emulator.aup_sonder
        assert w.aup_dock.isVisible() and w.act_dock_aup.isVisible()
        assert w.act_dock_aup.shortcut().isEmpty()

        sw.aup_sonder_box.setChecked(True)
        qapp.processEvents()
        assert w.emulator.aup_installed and w.emulator.aup_sonder

        sw.aup_box.setChecked(False)
        qapp.processEvents()
        assert not w.emulator.aup_installed and not w.aup_dock.isVisible()
        assert not sw.aup_sonder_box.isEnabled()
    finally:
        _zu(w, qapp)


def test_nur_das_a5120_profil_bietet_die_option(qapp, konfig_ordner):
    from app import profil
    assert profil.profil("a5120").aup_wahl
    for m in ("k8915", "prg710", "pc1715", "p8000", "mc80"):
        assert not profil.profil(m).aup_wahl


def test_modellwechsel_setzt_die_option_auf_die_vorgabe_zurueck(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        w.settings_widget.aup_box.setChecked(True)
        w.settings_widget.aup_alpha_combo.setCurrentIndex(
            w.settings_widget.aup_alpha_combo.findData("mit"))
        qapp.processEvents()
        assert w.emulator.aup_installed
        _waehle(w, qapp, "k8924")
        assert not w.emulator.aup_installed
        assert w.settings_widget.aup_value() == (False, False, "led")
        assert not w.aup_dock.isVisible()
    finally:
        _zu(w, qapp)


def test_konfiguration_merkt_die_option_und_stellt_sie_wieder_her(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        w.settings_widget.aup_box.setChecked(True)
        w.settings_widget.aup_sonder_box.setChecked(True)
        qapp.processEvents()
        w._autosave_now()
        general = yaml.safe_load(Path(w._konfig_pfad()).read_text(encoding="utf-8"))["general"]
        assert general["aup"] is True and general["aup_sonder"] is True
        assert general["aup_alpha"] == "led"
    finally:
        _zu(w, qapp)
    w = _fenster(qapp)
    try:
        assert w.emulator.aup_installed and w.emulator.aup_sonder
        assert w.settings_widget.aup_value() == (True, True, "led")
        assert w.aup_dock.isVisible()
    finally:
        _zu(w, qapp)


def test_auslieferung_traegt_keine_aup_werte(qapp, konfig_ordner):
    from app import config_io, profil
    vorgabe = config_io.standard_konfiguration(profil.profil("a5120"))
    assert not any(k.startswith("aup") for k in vorgabe.get("general", {}))
    assert "aup" not in vorgabe


# ── Kasten mit Ersatzemulator ────────────────────────────────────────────────

def test_tastenraster_stammt_aus_den_namen_des_kerns(qapp):
    from app.core_binding.k1520 import K1520Emulator
    from app.ui.keyboard_k7633 import KUNDE, K7633, kundentasten, raster
    r = raster()
    assert len(r) == 65                       # 72 Positionen - SS1/SS2 - 5 ohne Taste
    assert r[(0, 1)] == 0x42 and K1520Emulator.aup_tastenname(K7633, 0x42) == "A01"
    assert r[(5, 13)] if (5, 13) in r else True
    assert all(1 <= s <= 13 and 0 <= z <= 5 for z, s in r)
    k = kundentasten()
    assert k["7"] == 0x47 and k["0"] == 0x3E and set("0123456789") <= set(k)
    k_, _ = _kasten(qapp)
    assert len(k_.tasten) == 65 + len(k)
    assert (KUNDE, 0x47) in k_.tasten


def test_tastendruck_ruft_aup_taste_mit_kennung_und_position(qapp):
    from app.ui.keyboard_k7633 import K7633, KUNDE
    k, e = _kasten(qapp)
    k.tasten[(K7633, 0x42)].pressed.emit()
    k.tasten[(K7633, 0x42)].released.emit()
    k.tasten[(KUNDE, 0x47)].pressed.emit()
    k.tasten[(KUNDE, 0x47)].released.emit()
    assert e.aufrufe == [("taste", 0, 0x42, True), ("taste", 0, 0x42, False),
                         ("taste", 1, 0x47, True), ("taste", 1, 0x47, False)]
    # Loslassen ohne Druck geht nicht an den Kern
    k.tasten[(K7633, 0x43)].released.emit()
    assert len(e.aufrufe) == 4
    # Mausdruck auf dem Knopf ist derselbe Weg
    b = k.tasten[(K7633, 0x43)]
    from PySide6.QtTest import QTest
    from PySide6.QtCore import Qt
    QTest.mouseClick(b, Qt.LeftButton)
    assert e.aufrufe[-2:] == [("taste", 0, 0x43, True), ("taste", 0, 0x43, False)]
    assert b.focusPolicy() == Qt.NoFocus


def test_schluesselschalter(qapp):
    k, e = _kasten(qapp)
    k.schluessel[1].setChecked(True)
    k.schluessel[2].setChecked(True)
    k.schluessel[1].setChecked(False)
    assert e.aufrufe == [("schluessel", 1, True), ("schluessel", 2, True), ("schluessel", 1, False)]
    # Stellung nach Restore/Start kommt aus dem Kern, ohne ihn anzufassen
    e.aufrufe.clear()
    e.stand = {1: False, 2: False}
    k.aktualisieren()
    assert not k.schluessel[2].isChecked() and e.aufrufe == []


def test_lampen_und_blinkende_fehlerlampe(qapp):
    from app.ui import keyboard_k7633 as m
    k, e = _kasten(qapp)
    e.aup_lampen = m.LAMPE_SEL | m.LAMPE_GET | m.LAMPE_FEHLER
    k.aktualisieren()
    an = lambda b: k.lampen[b].property("an")     # noqa: E731
    assert an(m.LAMPE_SEL) and an(m.LAMPE_GET) and not an(m.LAMPE_ALPHA)
    zustaende = set()
    for _ in range(4):
        zustaende.add(an(m.LAMPE_FEHLER))
        k._blinken()
    assert zustaende == {True, False}             # sie blinkt
    e.aup_lampen = 0
    k.aktualisieren()
    assert not any(l.property("an") for l in k.lampen.values())


def test_hupe_nur_bei_steigendem_zaehler(qapp, monkeypatch):
    from app.ui.keyboard_k7633 import AupWidget
    toene = []
    monkeypatch.setattr(AupWidget, "_ton", lambda self: toene.append(1))
    e = Ersatz()
    e.aup_hupe = 5
    k, _ = _kasten(qapp, e)
    k.set_emulator(e)                             # Grundlinie 5: kein nachtraeglicher Ton
    k.aktualisieren()
    assert toene == []
    e.aup_hupe = 6
    k.aktualisieren()
    k.aktualisieren()
    assert toene == [1] and k.hupe_anzeige.property("an")
    k._blitz_ende()
    assert not k.hupe_anzeige.property("an")


def test_set_emulator_loest_gehaltene_tasten_nicht_an_der_neuen_maschine(qapp):
    from app.ui.keyboard_k7633 import K7633
    k, e = _kasten(qapp)
    k.tasten[(K7633, 0x42)].pressed.emit()
    k.alle_loslassen()
    assert e.aufrufe[-1] == ("taste", 0, 0x42, False)
    neu = Ersatz()
    k.set_emulator(neu)
    k.tasten[(K7633, 0x42)].released.emit()
    assert neu.aufrufe == []


def test_kein_kuerzel_im_kasten(qapp):
    from PySide6.QtGui import QAction
    k, _ = _kasten(qapp)
    assert all(a.shortcut().isEmpty() for a in k.findChildren(QAction))


def test_echte_maschine_taste_und_schluessel_kommen_an(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        w.settings_widget.aup_box.setChecked(True)
        qapp.processEvents()
        from app.ui.keyboard_k7633 import K7633
        w.aup_widget.schluessel[1].setChecked(True)
        assert w.emulator.aup_schluessel_stand(1) is True
        w.aup_widget.tasten[(K7633, 0x42)].pressed.emit()
        w.aup_widget.tasten[(K7633, 0x42)].released.emit()
    finally:
        _zu(w, qapp)

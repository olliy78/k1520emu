"""USB-Karte (GUN/VDIP1) und Netzwerkkarte (KCNet/W5100) in der Oberflaeche (Entwurf 50, AP U3/N3).

Gruppen *Einstellungen ▸ Allgemein ▸ USB-Stick* und *Netzwerk* in a5120emu, k8915emu, prg710emu und
pc1715emu (nicht p8000emu/mc80emu) nach dem Muster von test_gide_gui.py: Stecken ueber Neuaufbau, jede
Karte faellt bei einem Konflikt FUER SICH zurueck (Fehlertext nur an ihrer Gruppe, die anderen bleiben
gesteckt), Stick stecken/entfernen und Netzkabel wirken sofort, Leuchten folgen dem Zugriff, Konfig-Rundlauf.
Dateidialoge werden umgangen (ein ``exec()`` stuende headless bis zum Zeitueberlauf).
"""

from pathlib import Path

import pytest
import yaml
from PySide6.QtWidgets import QGroupBox

from conftest import requires_core

pytestmark = requires_core

MIT_KARTE = ["a5120", "k8915", "prg710", "pc1715"]
OHNE_KARTE = ["p8000", "mc80"]
USB_BASIS = {"a5120": 0x84, "k8915": 0x84, "prg710": 0x8C, "pc1715": 0x84}
NET_BASIS = {"a5120": 0xC0, "k8915": 0xC0, "prg710": 0x28, "pc1715": 0xC0}


@pytest.fixture
def konfig_ordner(tmp_path, monkeypatch):
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "xdg"))
    monkeypatch.setenv("K1520_DATA", str(tmp_path / "daten"))      # Stick-Vorgabeordner im Tempverzeichnis
    from app import config_io
    ordner = Path(config_io.default_config_dir())
    ordner.mkdir(parents=True, exist_ok=True)
    return ordner


@pytest.fixture(autouse=True)
def kein_meldungsfenster(monkeypatch):
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


def _haken(box, qapp, an=True):
    box.setChecked(an)
    qapp.processEvents()


# ── Profil und Sichtbarkeit ──────────────────────────────────────────────────

@pytest.mark.parametrize("maschine", MIT_KARTE + OHNE_KARTE)
def test_gruppen_nur_in_den_vier_programmen(qapp, konfig_ordner, maschine):
    w = _fenster(qapp, maschine)
    try:
        sw = w.settings_widget
        if maschine in MIT_KARTE:
            assert not sw.karten_box.isHidden()
            u, n = sw.usb_gruppe, sw.netz_gruppe
            assert not isinstance(u, QGroupBox) and not isinstance(n, QGroupBox)   # flache Zeilen, keine Rahmen
            assert not u.karte_box.isChecked() and not w.emulator.usb_installed()
            assert not n.karte_box.isChecked() and not w.emulator.net_installed()
            assert u.basis_combo.currentData() == USB_BASIS[maschine]
            assert n.basis_combo.currentData() == NET_BASIS[maschine]
            assert u.basis_combo.count() == 64 and n.basis_combo.count() == 64
            assert "(GUN)" in u.basis_combo.itemText(u.basis_combo.findData(0x84))
            assert "K1520-Netzkarte" in n.basis_combo.itemText(n.basis_combo.findData(0x28))
            assert "(GUN)" in n.basis_combo.itemText(n.basis_combo.findData(0x88))
            # Vorgabe: Netz ohne Loopback-Haken (der Anwender will das echte Netz), Kabel steckt
            assert not n.loopback_box.isChecked() and n.link_box.isChecked()
            assert not u.stick_knopf.isEnabled()
            assert not w.status_widget.usb.isVisibleTo(w.status_widget)
            assert not w.status_widget.net.isVisibleTo(w.status_widget)
        else:
            assert sw.karten_box.isHidden()
            g = w._gather_config()["general"]
            assert "usb" not in g and "net" not in g
    finally:
        _zu(w, qapp)


# ── Stecken ──────────────────────────────────────────────────────────────────

@pytest.mark.parametrize("maschine", MIT_KARTE)
def test_stecken_baut_die_maschine_mit_den_karten(qapp, konfig_ordner, maschine):
    w = _fenster(qapp, maschine)
    try:
        alt = w.emulator
        _haken(w.settings_widget.usb_gruppe.karte_box, qapp)
        assert w.emulator is not alt and w.emulator.usb_installed() and not w.emulator.net_installed()
        st = w.emulator.usb_status()
        assert st["basis"] == USB_BASIS[maschine] and st["stick_vorhanden"]      # Vorgabeordner wird angelegt
        _haken(w.settings_widget.netz_gruppe.karte_box, qapp)
        assert w.emulator.usb_installed() and w.emulator.net_installed()      # die USB-Karte bleibt gesteckt
        assert w.emulator.net_status()["basis"] == NET_BASIS[maschine]
        assert w.status_widget.usb.isVisibleTo(w.status_widget) and w.status_widget.net.isVisibleTo(w.status_widget)
        _haken(w.settings_widget.usb_gruppe.karte_box, qapp, False)
        assert not w.emulator.usb_installed() and w.emulator.net_installed()
        assert not w.status_widget.usb.isVisibleTo(w.status_widget)
        assert w.status_widget.net.isVisibleTo(w.status_widget)
    finally:
        _zu(w, qapp)


def test_stick_ordner_wird_angelegt(qapp, konfig_ordner, tmp_path):
    from app import usb
    w = _fenster(qapp)
    try:
        ordner = tmp_path / "neu" / "stick"
        w._usb = {**w._usb, "enabled": True, "ordner": str(ordner)}
        w._apply_drive_types(w._drive_types, cold_restart=True)
        qapp.processEvents()
        assert ordner.is_dir() and w.emulator.usb_status()["stick_vorhanden"]
        assert usb.ordner_wirksam({"ordner": ""}).endswith("USB-Stick")
    finally:
        _zu(w, qapp)


def test_basis_wechsel_wirkt_nach_dem_neuaufbau(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _haken(w.settings_widget.usb_gruppe.karte_box, qapp)
        u = w.settings_widget.usb_gruppe
        u.basis_combo.setCurrentIndex(u.basis_combo.findData(0xA4))
        qapp.processEvents()
        assert w.emulator.usb_status()["basis"] == 0xA4
        n = w.settings_widget.netz_gruppe
        _haken(n.karte_box, qapp)
        n.basis_combo.setCurrentIndex(n.basis_combo.findData(0x28))
        qapp.processEvents()
        assert w.emulator.net_status()["basis"] == 0x28
        # die andere Karte steht im Auswahlfeld als belegt: USB A4H (A4..A7) blockiert die Netzbasis A4H
        i = n.basis_combo.findData(0xA4)
        assert "belegt: GUN-USB" in n.basis_combo.itemText(i) and not n.basis_combo.model().item(i).isEnabled()
    finally:
        _zu(w, qapp)


# ── Konflikte: jede Karte faellt fuer sich zurueck ───────────────────────────

def test_netz_auf_raf_basis_zeigt_fehler_nur_an_der_karte(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        w._raf = "raf512"
        w._usb = {**w._usb, "enabled": True}
        w._gide = {**w._gide, "enabled": True}
        w._net = {**w._net, "enabled": True, "basis": 0x88}
        w._apply_drive_types(w._drive_types, cold_restart=True)
        qapp.processEvents()
        sw = w.settings_widget
        assert w.emulator.raf_variant and w.emulator.usb_installed() and w.emulator.gide_installed()
        assert not w.emulator.net_installed()
        assert sw.netz_gruppe.fehler_label.isVisibleTo(sw.netz_gruppe)
        assert "Netzwerkkarte nicht gesteckt" in sw.netz_gruppe.fehler_label.text()
        assert "RAF" in sw.netz_gruppe.fehler_label.text()
        assert not sw.usb_gruppe.fehler_label.isVisibleTo(sw.usb_gruppe)
        assert not sw.gide_gruppe.fehler_label.isVisibleTo(sw.gide_gruppe)
        assert sw.usb_gruppe.karte_box.isChecked() and not sw.netz_gruppe.karte_box.isChecked()
        assert w._net["enabled"] is False
        # das Auswahlfeld markiert 88H als belegt
        i = sw.netz_gruppe.basis_combo.findData(0x88)
        assert "belegt: RAF" in sw.netz_gruppe.basis_combo.itemText(i)
        # Ausweichen auf C0H steckt sie
        sw.netz_gruppe.basis_combo.setCurrentIndex(sw.netz_gruppe.basis_combo.findData(0xC0))
        _haken(sw.netz_gruppe.karte_box, qapp)
        assert w.emulator.net_installed() and w.emulator.usb_installed()
        assert not sw.netz_gruppe.fehler_label.isVisibleTo(sw.netz_gruppe)
    finally:
        _zu(w, qapp)


def test_usb_und_netz_gegeneinander_der_zweite_faellt(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        w._usb = {**w._usb, "enabled": True, "basis": 0xC0}
        w._net = {**w._net, "enabled": True, "basis": 0xC0}
        w._apply_drive_types(w._drive_types, cold_restart=True)
        qapp.processEvents()
        sw = w.settings_widget
        assert w.emulator.usb_installed() and not w.emulator.net_installed()
        assert "Netzwerkkarte nicht gesteckt" in sw.netz_gruppe.fehler_label.text()
        assert not sw.usb_gruppe.fehler_label.isVisibleTo(sw.usb_gruppe)
    finally:
        _zu(w, qapp)


def test_usb_konflikt_k8915_g2_84h_dann_folgt_die_vorauswahl_dem_modell(qapp, konfig_ordner):
    w = _fenster(qapp, "k8915")
    try:
        assert w.settings_widget.usb_gruppe.basis_combo.currentData() == 0x84
        w._on_model_selected("k8915-g2")
        qapp.processEvents()
        assert w._usb["basis"] == 0x8C and w.settings_widget.usb_gruppe.basis_combo.currentData() == 0x8C
        # eine Konfiguration, die auf dem V2 trotzdem 84H verlangt: Fehlertext, Rechner laeuft
        w._usb = {**w._usb, "enabled": True, "basis": 0x84}
        w._apply_drive_types(w._drive_types, cold_restart=True)
        qapp.processEvents()
        assert not w.emulator.usb_installed()
        assert "USB-Karte nicht gesteckt" in w.settings_widget.usb_gruppe.fehler_label.text()
        # mit der Vorauswahl steckt sie
        _haken(w.settings_widget.usb_gruppe.karte_box, qapp, False)
        w.settings_widget.usb_gruppe.basis_combo.setCurrentIndex(
            w.settings_widget.usb_gruppe.basis_combo.findData(0x8C))
        _haken(w.settings_widget.usb_gruppe.karte_box, qapp)
        assert w.emulator.usb_installed() and w.emulator.usb_status()["basis"] == 0x8C
    finally:
        _zu(w, qapp)


# ── sofort wirksam ───────────────────────────────────────────────────────────

def test_stick_entfernen_und_stecken_sofort(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _haken(w.settings_widget.usb_gruppe.karte_box, qapp)
        u = w.settings_widget.usb_gruppe
        emu = w.emulator
        assert u.stick_knopf.isEnabled() and u.stick_knopf.text() == "Stick entfernen"
        u.stick_knopf.click()
        qapp.processEvents()
        assert w.emulator is emu and not emu.usb_status()["stick_vorhanden"]       # kein Neuaufbau
        assert u.stick_knopf.text() == "Stick stecken"
        assert w.status_widget.usb.lampe.zustand() == "leer"
        u.stick_knopf.click()
        qapp.processEvents()
        assert w.emulator is emu and emu.usb_status()["stick_vorhanden"] and u.stick_knopf.text() == "Stick entfernen"
        assert w.status_widget.usb.lampe.zustand() == "belegt"
    finally:
        _zu(w, qapp)


def test_ordner_und_schreibschutz_wirken_ohne_neuaufbau(qapp, konfig_ordner, tmp_path):
    w = _fenster(qapp)
    try:
        _haken(w.settings_widget.usb_gruppe.karte_box, qapp)
        u = w.settings_widget.usb_gruppe
        emu = w.emulator
        ordner = tmp_path / "anderer"
        u.ordner_edit.setText(str(ordner))
        u._aenderung()
        qapp.processEvents()
        assert w.emulator is emu and ordner.is_dir() and emu.usb_status()["stick_vorhanden"]
        assert str(ordner) in w.status_widget.usb.lampe.toolTip()
        u.schutz_box.setChecked(True)
        qapp.processEvents()
        assert w.emulator is emu and w._usb["schreibgeschuetzt"] is True
        # bei gezogenem Stick wird nur gemerkt, nicht gesteckt
        u.stick_knopf.click()
        u.ordner_edit.setText(str(tmp_path / "dritter"))
        u._aenderung()
        qapp.processEvents()
        assert not emu.usb_status()["stick_vorhanden"] and not (tmp_path / "dritter").exists()
        u.stick_knopf.click()
        qapp.processEvents()
        assert emu.usb_status()["stick_vorhanden"] and (tmp_path / "dritter").is_dir()
    finally:
        _zu(w, qapp)


def test_netzkabel_wirkt_sofort(qapp, konfig_ordner, monkeypatch):
    from app.core_binding.k1520 import K1520Emulator
    w = _fenster(qapp)
    try:
        _haken(w.settings_widget.netz_gruppe.karte_box, qapp)
        emu = w.emulator
        gerufen = []
        orig = K1520Emulator.net_set_link
        monkeypatch.setattr(K1520Emulator, "net_set_link", lambda s, ein: gerufen.append(ein) or orig(s, ein))
        n = w.settings_widget.netz_gruppe
        n.link_box.setChecked(False)
        qapp.processEvents()
        assert w.emulator is emu and gerufen == [False] and w._net["link"] is False
        assert w.status_widget.net.lampe.zustand() == "leer" and "abgezogen" in w.status_widget.net.lampe.toolTip()
        n.link_box.setChecked(True)
        qapp.processEvents()
        assert gerufen == [False, True] and w.status_widget.net.lampe.zustand() == "belegt"
    finally:
        _zu(w, qapp)


def test_loopback_baut_neu(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _haken(w.settings_widget.netz_gruppe.karte_box, qapp)
        emu = w.emulator
        w.settings_widget.netz_gruppe.loopback_box.setChecked(True)
        qapp.processEvents()
        assert w.emulator is not emu and w.emulator.net_installed() and w._net["nur_loopback"] is True
        assert "Loopback" in w.status_widget.net.lampe.toolTip()
    finally:
        _zu(w, qapp)


# ── Leuchten ─────────────────────────────────────────────────────────────────

def test_leuchten_folgen_dem_zugriff(qapp, konfig_ordner):
    from app.ui import status_bar
    w = _fenster(qapp)
    try:
        _haken(w.settings_widget.usb_gruppe.karte_box, qapp)
        _haken(w.settings_widget.netz_gruppe.karte_box, qapp)
        usb_l, net_l = w.status_widget.usb, w.status_widget.net
        w._update_usb()
        w._update_net()
        assert usb_l.lampe.zustand() == status_bar.BELEGT and net_l.lampe.zustand() == status_bar.BELEGT
        assert "84H" in usb_l.lampe.toolTip() and "C0H" in net_l.lampe.toolTip()
        assert "echten Netz" in net_l.lampe.toolTip()
        # Die Zaehler des Kerns steigen nur durch Gastprogramme (E/A-Schreiben gibt es in der C-ABI nicht):
        # der Zugriff wird hier ueber die Zaehler nachgestellt.
        emu = w.emulator
        u0, n0 = emu.usb_status, emu.net_status
        emu.usb_status = lambda: {**u0(), "gelesen": u0()["gelesen"] + 1}
        emu.net_status = lambda: {**n0(), "kommandos": n0()["kommandos"] + 1}
        w._update_usb()
        w._update_net()
        assert usb_l.lampe.zustand() == status_bar.ZUGRIFF and net_l.lampe.zustand() == status_bar.ZUGRIFF
        emu.usb_status, emu.net_status = u0, n0
        w._update_usb()
        w._update_net()                                    # Zaehler springt zurueck: zaehlt als Aenderung
        w._update_usb()
        w._update_net()                                    # ohne weiteren Zugriff wieder ruhig
        assert usb_l.lampe.zustand() == status_bar.BELEGT and net_l.lampe.zustand() == status_bar.BELEGT
        assert "Kommandos" in w.settings_widget.netz_gruppe.zaehler_label.text()
    finally:
        _zu(w, qapp)


def test_keine_kuerzel(qapp, konfig_ordner):
    from PySide6.QtGui import QAction
    w = _fenster(qapp)
    try:
        for g in (w.settings_widget.usb_gruppe, w.settings_widget.netz_gruppe):
            for a in g.findChildren(QAction):
                assert a.shortcut().isEmpty(), a.text()
    finally:
        _zu(w, qapp)


# ── Konfiguration ────────────────────────────────────────────────────────────

def test_konfig_rundlauf(qapp, konfig_ordner, tmp_path):
    ordner = tmp_path / "stickordner"
    w = _fenster(qapp)
    try:
        u, n = w.settings_widget.usb_gruppe, w.settings_widget.netz_gruppe
        _haken(u.karte_box, qapp)
        u.basis_combo.setCurrentIndex(u.basis_combo.findData(0xA4))
        qapp.processEvents()
        u.ordner_edit.setText(str(ordner))
        u._aenderung()
        u.schutz_box.setChecked(True)
        _haken(n.karte_box, qapp)
        n.basis_combo.setCurrentIndex(n.basis_combo.findData(0x28))
        qapp.processEvents()
        n.loopback_box.setChecked(True)
        n.link_box.setChecked(False)
        qapp.processEvents()
    finally:
        _zu(w, qapp)
    daten = yaml.safe_load(Path(w._konfig_pfad()).read_text(encoding="utf-8"))
    assert daten["general"]["usb"] == {"enabled": True, "basis": "A4", "ordner": str(ordner),
                                       "schreibgeschuetzt": True}
    assert daten["general"]["net"] == {"enabled": True, "basis": "28", "nur_loopback": True, "link": False}

    w = _fenster(qapp)
    try:
        assert w.emulator.usb_installed() and w.emulator.usb_status()["basis"] == 0xA4
        assert w.emulator.usb_status()["stick_vorhanden"] and ordner.is_dir()
        assert w.emulator.net_installed() and w.emulator.net_status()["basis"] == 0x28
        sw = w.settings_widget
        assert sw.usb_gruppe.fehler_label.text() == "" and sw.netz_gruppe.fehler_label.text() == ""
        assert sw.usb_gruppe.ordner_edit.text() == str(ordner) and sw.usb_gruppe.schutz_box.isChecked()
        assert sw.netz_gruppe.loopback_box.isChecked() and not sw.netz_gruppe.link_box.isChecked()
    finally:
        _zu(w, qapp)


def test_fehlender_abschnitt_laesst_alles_wie_es_ist(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _haken(w.settings_widget.usb_gruppe.karte_box, qapp)
        _haken(w.settings_widget.netz_gruppe.karte_box, qapp)
        daten = w._gather_config()
        del daten["general"]["usb"], daten["general"]["net"]
        w._apply_config(daten)
        assert w._usb["enabled"] is True and w._net["enabled"] is True
        daten["general"]["usb"] = {"enabled": True, "basis": "kaputt", "schreibgeschuetzt": "egal"}
        daten["general"]["net"] = {"enabled": True, "basis": "83", "link": "egal"}
        w._apply_config(daten)
        assert w._usb["basis"] == 0x84 and w._usb["schreibgeschuetzt"] is False       # Vorgabewerte
        assert w._net["basis"] == 0xC0 and w._net["link"] is True and w._net["nur_loopback"] is False
    finally:
        _zu(w, qapp)


@pytest.mark.parametrize("maschine", MIT_KARTE)
def test_auslieferung_hat_beide_karten_aus(qapp, konfig_ordner, maschine):
    from app import config_io, profil
    g = config_io.standard_konfiguration(profil.profil(maschine))["general"]
    assert g["usb"]["enabled"] is False and g["usb"]["ordner"] == "" and g["usb"]["schreibgeschuetzt"] is False
    assert int(g["usb"]["basis"], 16) == USB_BASIS[maschine]
    assert g["net"]["enabled"] is False and g["net"]["nur_loopback"] is False and g["net"]["link"] is True
    assert int(g["net"]["basis"], 16) == NET_BASIS[maschine]


@pytest.mark.parametrize("maschine", OHNE_KARTE)
def test_auslieferung_ohne_karte_hat_keine_abschnitte(maschine):
    from app import config_io, profil
    g = config_io.standard_konfiguration(profil.profil(maschine)).get("general", {})
    assert "usb" not in g and "net" not in g


def test_zuruecksetzen_steckt_die_karten_ab(qapp, konfig_ordner, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    monkeypatch.setattr(QMessageBox, "question", staticmethod(lambda *a, **k: QMessageBox.Yes))
    w = _fenster(qapp)
    try:
        _haken(w.settings_widget.usb_gruppe.karte_box, qapp)
        _haken(w.settings_widget.netz_gruppe.karte_box, qapp)
        assert w.emulator.usb_installed() and w.emulator.net_installed()
        w._standard_zuruecksetzen()
        qapp.processEvents()
        assert not w.emulator.usb_installed() and not w.emulator.net_installed()
        assert not w.settings_widget.usb_gruppe.karte_box.isChecked()
        daten = yaml.safe_load(Path(w._konfig_pfad()).read_text(encoding="utf-8"))
        assert daten["general"]["usb"]["enabled"] is False and daten["general"]["net"]["enabled"] is False
    finally:
        _zu(w, qapp)


def test_alle_drei_karten_gleichzeitig_in_der_vorauswahl(qapp, konfig_ordner):
    """Die Vorauswahlen aller drei Karten lassen sich in der Oberflaeche zugleich stecken."""
    for maschine in MIT_KARTE:
        w = _fenster(qapp, maschine)
        try:
            sw = w.settings_widget
            _haken(sw.gide_gruppe.karte_box, qapp)
            _haken(sw.usb_gruppe.karte_box, qapp)
            _haken(sw.netz_gruppe.karte_box, qapp)
            assert (w.emulator.gide_installed(), w.emulator.usb_installed(), w.emulator.net_installed()) == \
                (True, True, True), maschine
            for g in (sw.gide_gruppe, sw.usb_gruppe, sw.netz_gruppe):
                assert not g.fehler_label.isVisibleTo(g), maschine
        finally:
            _zu(w, qapp)


def test_basis_lesen():
    from app import netz, usb
    for roh, soll in (("84", 0x84), ("84h", 0x84), ("0x88", 0x88), (88, 0x88), ("85", 0x20), ("zz", 0x20),
                      (None, 0x20), (True, 0x20)):
        assert usb.basis_lesen(roh, 0x20) == soll, roh
    assert netz.basis_lesen("C0", 0x20) == 0xC0 and netz.basis_lesen("C1", 0x20) == 0x20


def test_handbuch_beschreibt_usb_und_netz():
    text = (Path(__file__).resolve().parents[2] / "app" / "help" / "handbuch.md").read_text(encoding="utf-8")
    for s in ("## USB-Stick (GUN/VDIP1)", "## Netzwerk (KCNet/W5100)", "UGET", "TFTP", "CPMNET",
              "general.usb", "general.net", "echte Netz"):
        assert s in text, s

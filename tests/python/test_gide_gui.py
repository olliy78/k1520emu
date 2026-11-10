"""IDE-Karte GIDE in der Oberflaeche (doc/design/50_gun_gide.md, AP G6).

Gruppe *Einstellungen ▸ Allgemein ▸ IDE-Platte (GIDE)* in a5120emu, k8915emu, prg710emu und
pc1715emu (nicht p8000emu/mc80emu): Stecken ueber einen Neuaufbau der Maschine (wie RAF/Kassette),
Konfliktfall mit Fehlertext statt Absturz, Neue Platte (nie ueber eine vorhandene Datei),
Konfig-Rundlauf, Zuruecksetzen und die Leuchte „IDE" (folgt dem Zugriff).  Dateidialoge werden
umgangen -- ein ``exec()`` stuende headless bis zum Zeitueberlauf.
"""

import os
from pathlib import Path

import pytest
import yaml

from conftest import requires_core

pytestmark = requires_core

MIT_KARTE = ["a5120", "k8915", "prg710", "pc1715"]
OHNE_KARTE = ["p8000", "mc80"]
BASIS = {m: 0x90 for m in MIT_KARTE}      # Vorauswahl: ueberall 90H


@pytest.fixture
def konfig_ordner(tmp_path, monkeypatch):
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "xdg"))
    from app import config_io
    ordner = Path(config_io.default_config_dir())
    ordner.mkdir(parents=True, exist_ok=True)
    return ordner


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


def _stecken(w, qapp, an=True):
    w.settings_widget.gide_gruppe.karte_box.setChecked(an)
    qapp.processEvents()


# ── Profil und Sichtbarkeit ──────────────────────────────────────────────────

def test_profilfelder():
    from app import profil
    for m in MIT_KARTE:
        p = profil.profil(m)
        assert p.erweiterungskarten and p.gide_basis_vorgabe == BASIS[m]
    for m in OHNE_KARTE:
        assert not profil.profil(m).erweiterungskarten


@pytest.mark.parametrize("maschine", MIT_KARTE + OHNE_KARTE)
def test_gruppe_nur_in_den_vier_programmen(qapp, konfig_ordner, maschine):
    w = _fenster(qapp, maschine)
    try:
        sw = w.settings_widget
        if maschine in MIT_KARTE:
            assert not sw.karten_box.isHidden()
            g = sw.gide_gruppe
            assert g.title() == "IDE-Platte (GIDE)"
            assert not g.karte_box.isChecked() and not w.emulator.gide_installed()
            assert g.basis_combo.currentData() == BASIS[maschine]
            assert g.rtc_box.isChecked()
            assert [g.basis_combo.itemText(i)[:3] for i in (0, 15)] == ["00H", "F0H"]
            assert g.basis_combo.model().item(g.basis_combo.findData(0x90)).isEnabled()
            assert g.basis_combo.count() == 16
            # Plattenbedienung nur mit gesteckter Karte
            assert not g.neu_knopf.isEnabled() and not g.abbild_edit.isEnabled()
            assert not w.status_widget.ide.isVisibleTo(w.status_widget)
        else:
            assert sw.karten_box.isHidden()
            assert "gide" not in w._gather_config()["general"]
    finally:
        _zu(w, qapp)


# ── Stecken ──────────────────────────────────────────────────────────────────

@pytest.mark.parametrize("maschine", MIT_KARTE)
def test_stecken_baut_die_maschine_mit_karte(qapp, konfig_ordner, maschine):
    w = _fenster(qapp, maschine)
    try:
        alt = w.emulator
        _stecken(w, qapp)
        assert w.emulator is not alt and w.emulator.gide_installed()
        st = w.emulator.gide_status()
        assert st["basis"] == BASIS[maschine] and not st["abbild_vorhanden"]
        assert w.emulator.gide_rtc_zeit() is not None
        g = w.settings_widget.gide_gruppe
        assert g.karte_box.isChecked() and g.neu_knopf.isEnabled()
        assert not g.fehler_label.isVisibleTo(g)
        assert w.status_widget.ide.isVisibleTo(w.status_widget)
        _stecken(w, qapp, False)
        assert not w.emulator.gide_installed()
        assert not w.status_widget.ide.isVisibleTo(w.status_widget)
    finally:
        _zu(w, qapp)


def test_basis_und_rtc_wirken_nach_dem_neuaufbau(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        g = w.settings_widget.gide_gruppe
        g.basis_combo.setCurrentIndex(g.basis_combo.findData(0xB0))
        qapp.processEvents()
        assert w.emulator.gide_status()["basis"] == 0xB0
        g.rtc_box.setChecked(False)
        qapp.processEvents()
        assert w.emulator.gide_rtc_zeit() is None
    finally:
        _zu(w, qapp)


def test_konflikt_zeigt_fehlertext_und_karte_bleibt_aus(qapp, konfig_ordner, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    # kein Meldungsfenster: ein exec() stuende headless bis zum Zeitueberlauf
    for n in ("critical", "warning", "information", "question"):
        monkeypatch.setattr(QMessageBox, n, staticmethod(
            lambda *a, **k: pytest.fail("unerwartetes Meldungsfenster")))
    w = _fenster(qapp, "k8915")
    try:
        w.settings_widget.tape_box.setChecked(True)        # Kassette belegt D0H-D7H
        qapp.processEvents()
        assert w.emulator.tape_installed()
        g = w.settings_widget.gide_gruppe
        # Das Auswahlfeld markiert D0H als belegt und sperrt es (Besitzer kommt aus dem Kern) ...
        i = g.basis_combo.findData(0xD0)
        assert "belegt: K5020" in g.basis_combo.itemText(i)
        assert not g.basis_combo.model().item(i).isEnabled()
        assert g.basis_combo.model().item(g.basis_combo.findData(0x90)).isEnabled()
        # ... eine Konfiguration kann es trotzdem verlangen: dann Fehlertext statt Absturz.
        w._gide = {**w._gide, "enabled": True, "basis": 0xD0}
        w._apply_drive_types(w._drive_types, cold_restart=True)
        qapp.processEvents()
        assert not w.emulator.gide_installed()
        assert w.emulator.tape_installed()                 # die Maschine laeuft mit dem Rest weiter
        assert not g.karte_box.isChecked()
        assert g.fehler_label.isVisibleTo(g) and "GIDE nicht gesteckt" in g.fehler_label.text()
        assert w._gide["enabled"] is False
        # Ausweichen auf eine freie Basis steckt sie.
        g.basis_combo.setCurrentIndex(g.basis_combo.findData(0xB0))
        _stecken(w, qapp)
        assert w.emulator.gide_installed() and w.emulator.gide_status()["basis"] == 0xB0
        assert not g.fehler_label.isVisibleTo(g)
    finally:
        _zu(w, qapp)


# ── Neue Platte ──────────────────────────────────────────────────────────────

def test_neue_platte_legt_an_und_legt_ein(qapp, konfig_ordner, tmp_path):
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        g = w.settings_widget.gide_gruppe
        pfad = tmp_path / "neu" / "p.img"
        g.neue_platte(str(pfad), 8)                        # legt auch den Ordner an
        qapp.processEvents()
        assert pfad.is_file() and pfad.stat().st_size == 8 * 1024 * 1024
        st = w.emulator.gide_status()
        assert st["abbild_vorhanden"] and st["sektoren"] == 8 * 1024 * 1024 // 512
        assert w.emulator.gide_image_path() == str(pfad)
        assert g.abbild_edit.text() == str(pfad) and g.auswerfen_knopf.isEnabled()
        assert w._gide["image"] == str(pfad)
        # Auswerfen
        g.auswerfen_knopf.click()
        qapp.processEvents()
        assert not w.emulator.gide_status()["abbild_vorhanden"] and w._gide["image"] == ""
        assert not g.auswerfen_knopf.isEnabled()
    finally:
        _zu(w, qapp)


def test_neue_platte_ueberschreibt_nie(qapp, konfig_ordner, tmp_path, monkeypatch):
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        g = w.settings_widget.gide_gruppe
        pfad = tmp_path / "da.img"
        pfad.write_bytes(b"wichtig")
        fragen = []
        monkeypatch.setattr(g, "_frage", lambda t, x: fragen.append(x) or False)
        g.neue_platte(str(pfad), 8)
        assert fragen and pfad.read_bytes() == b"wichtig"
        assert w._gide["image"] == "" and not w.emulator.gide_status()["abbild_vorhanden"]
        # Auch der Weg ueber den Kern selbst: false, Datei unberuehrt.
        assert not w.emulator.gide_image_create(str(pfad), 8)
        assert pfad.read_bytes() == b"wichtig"
    finally:
        _zu(w, qapp)


def test_neue_platte_dialogwerte(qapp):
    from app import gide
    from app.ui.gide_gruppe import NeuePlatteDialog
    d = NeuePlatteDialog("/x")
    assert d.ergebnis() == (os.path.join("/x", "gide.img"), 256)
    assert [d.groesse_combo.itemData(i) for i in range(d.groesse_combo.count())] == list(gide.GROESSEN_MB)
    d.gz_box.setChecked(True)
    assert d.ergebnis()[0].endswith(".img.gz")


def test_fehlendes_abbild_bei_start_laesst_die_karte_stehen(qapp, konfig_ordner, tmp_path):
    w = _fenster(qapp)
    try:
        w._gide = {**w._gide, "enabled": True, "image": str(tmp_path / "gibtsnicht.img")}
        w._apply_drive_types(w._drive_types, cold_restart=True)
        qapp.processEvents()
        g = w.settings_widget.gide_gruppe
        assert w.emulator.gide_installed() and not w.emulator.gide_status()["abbild_vorhanden"]
        assert "Plattenabbild nicht eingelegt" in g.fehler_label.text()
    finally:
        _zu(w, qapp)


# ── Konfiguration ────────────────────────────────────────────────────────────

def test_konfig_rundlauf(qapp, konfig_ordner, tmp_path):
    img = tmp_path / "r.img"
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        g = w.settings_widget.gide_gruppe
        g.basis_combo.setCurrentIndex(g.basis_combo.findData(0xA0))
        qapp.processEvents()
        g.neue_platte(str(img), 8)
        g.rtc_box.setChecked(False)
        qapp.processEvents()
    finally:
        _zu(w, qapp)
    daten = yaml.safe_load(Path(w._konfig_pfad()).read_text(encoding="utf-8"))
    assert daten["general"]["gide"] == {"enabled": True, "basis": "A0", "image": str(img), "rtc": False}

    w = _fenster(qapp)
    try:
        assert w.emulator.gide_installed() and w.emulator.gide_status()["basis"] == 0xA0
        assert w.settings_widget.gide_gruppe.fehler_label.text() == ""
        assert w.emulator.gide_image_path() == str(img)
        assert w.emulator.gide_rtc_zeit() is None
        g = w.settings_widget.gide_gruppe
        assert g.karte_box.isChecked() and not g.rtc_box.isChecked() and g.abbild_edit.text() == str(img)
    finally:
        _zu(w, qapp)


def test_fehlender_abschnitt_laesst_alles_wie_es_ist(qapp, konfig_ordner):
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        daten = w._gather_config()
        del daten["general"]["gide"]
        w._apply_config(daten)
        assert w._gide["enabled"] is True
        daten["general"]["gide"] = {"enabled": True, "basis": "kaputt", "rtc": "egal"}
        w._apply_config(daten)
        assert w._gide["basis"] == 0x90 and w._gide["rtc"] is True     # Vorgabewerte
    finally:
        _zu(w, qapp)


def test_basis_lesen():
    from app import gide
    for roh, soll in (("D0", 0xD0), ("d0h", 0xD0), ("0x90", 0x90), (90, 0x90), ("91", 0x20), ("zz", 0x20),
                      (None, 0x20), ("1F0", 0x20)):
        assert gide.basis_lesen(roh, 0x20) == soll, roh


@pytest.mark.parametrize("maschine", MIT_KARTE)
def test_auslieferung_hat_die_karte_aus(qapp, konfig_ordner, maschine):
    from app import config_io, profil
    g = config_io.standard_konfiguration(profil.profil(maschine))["general"]["gide"]
    assert g["enabled"] is False and g["image"] == "" and g["rtc"] is True
    assert int(g["basis"], 16) == BASIS[maschine] == 0x90


@pytest.mark.parametrize("maschine", OHNE_KARTE)
def test_auslieferung_ohne_karte_hat_keinen_abschnitt(maschine):
    from app import config_io, profil
    assert "gide" not in config_io.standard_konfiguration(profil.profil(maschine)).get("general", {})


def test_zuruecksetzen_steckt_die_karte_ab(qapp, konfig_ordner, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    monkeypatch.setattr(QMessageBox, "question", staticmethod(lambda *a, **k: QMessageBox.Yes))
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        assert w.emulator.gide_installed()
        w._standard_zuruecksetzen()
        qapp.processEvents()
        assert not w.emulator.gide_installed() and not w.settings_widget.gide_gruppe.karte_box.isChecked()
        daten = yaml.safe_load(Path(w._konfig_pfad()).read_text(encoding="utf-8"))
        assert daten["general"]["gide"]["enabled"] is False
    finally:
        _zu(w, qapp)


def test_batteriedatei_je_programm(konfig_ordner):
    from app import gide
    a, b = gide.batterie_pfad("a5120emu"), gide.batterie_pfad("k8915emu")
    assert a != b and os.path.basename(a) == "gide_rtc_a5120emu.dat"
    assert os.path.dirname(a) == str(konfig_ordner)


# ── Leuchte ──────────────────────────────────────────────────────────────────

def test_leuchte_folgt_dem_zugriff(qapp, konfig_ordner, tmp_path):
    from app.ui import status_bar
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        ide = w.status_widget.ide
        assert ide.isVisibleTo(w.status_widget) and ide.lampe.zustand() == status_bar.LEER
        w.settings_widget.gide_gruppe.neue_platte(str(tmp_path / "l.img"), 8)
        w._update_gide()
        assert ide.lampe.zustand() == status_bar.BELEGT
        assert "l.img" in ide.lampe.toolTip() and "90H" in ide.lampe.toolTip()
        before = w.emulator.gide_status()["taskfile"] + w.emulator.gide_status()["befehle"]
        w.emulator.io_read(0x90 + 0x0F)                    # Gast liest das Statusregister
        st = w.emulator.gide_status()
        assert st["taskfile"] + st["befehle"] > before
        w._update_gide()
        assert ide.lampe.zustand() == status_bar.ZUGRIFF
        w._update_gide()                                   # ohne weiteren Zugriff wieder ruhig
        assert ide.lampe.zustand() == status_bar.BELEGT
    finally:
        _zu(w, qapp)


def test_keine_kuerzel(qapp, konfig_ordner):
    from PySide6.QtGui import QAction
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        for a in w.settings_widget.gide_gruppe.findChildren(QAction):
            assert a.shortcut().isEmpty(), a.text()
    finally:
        _zu(w, qapp)

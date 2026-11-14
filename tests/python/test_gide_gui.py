"""IDE-Karte GIDE in der Oberflaeche (doc/design/50_gun_gide.md, AP G6).

Zeilen *Einstellungen ▸ Allgemein* (Karte, Basis, Uhr -- flach, ohne Rahmen, KEIN Plattenabbildfeld) und
Plattenkasten im Reiter *Laufwerke* in a5120emu, k8915emu, prg710emu und
pc1715emu (nicht p8000emu/mc80emu): Stecken ueber einen Neuaufbau der Maschine (wie RAF/Kassette),
Konfliktfall mit Fehlertext statt Absturz, Neue Platte (nie ueber eine vorhandene Datei),
Konfig-Rundlauf, Zuruecksetzen und die Leuchte „IDE" (folgt dem Zugriff).  Dateidialoge werden
umgangen -- ein ``exec()`` stuende headless bis zum Zeitueberlauf.
"""

import os
from pathlib import Path

import pytest
import yaml
from PySide6.QtWidgets import QGroupBox

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
            r = sw.gide_rtc_gruppe
            assert not isinstance(g, QGroupBox) and not isinstance(r, QGroupBox)    # flache Zeilen
            assert not g.karte_box.isChecked() and not w.emulator.gide_installed()
            assert g.basis_combo.currentData() == BASIS[maschine]
            assert r.rtc_box.isChecked() and r.versatz_combo.currentData() == "-40"
            assert [r.versatz_combo.itemData(i) for i in range(r.versatz_combo.count())] == ["0", "-28", "-40"]
            assert [g.basis_combo.itemText(i)[:3] for i in (0, 15)] == ["00H", "F0H"]
            assert g.basis_combo.model().item(g.basis_combo.findData(0x90)).isEnabled()
            assert g.basis_combo.count() == 16
            # Plattenkasten im Reiter Laufwerke: nur mit gesteckter Karte; die Einstellungen haben kein Abbildfeld
            assert w.gide_platte is not None and not w.gide_platte.isVisibleTo(w.drives_dock.widget())
            assert not hasattr(g, "abbild_edit") and not hasattr(g, "neu_knopf")
            assert w.drives_dock.widget().findChild(type(w.gide_platte)) is w.gide_platte
            assert not w.status_widget.ide.isVisibleTo(w.status_widget)
        else:
            assert sw.karten_box.isHidden() and w.gide_platte is None
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
        assert g.karte_box.isChecked() and w.gide_platte.neu_knopf.isEnabled()
        assert w.gide_platte.isVisibleTo(w.drives_dock.widget())
        assert not g.fehler_label.isVisibleTo(g)
        assert w.status_widget.ide.isVisibleTo(w.status_widget)
        _stecken(w, qapp, False)
        assert not w.emulator.gide_installed()
        assert not w.status_widget.ide.isVisibleTo(w.status_widget)
        assert not w.gide_platte.isVisibleTo(w.drives_dock.widget())
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
        w.settings_widget.gide_rtc_gruppe.rtc_box.setChecked(False)
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


# ── Neue Platte (Plattenkasten im Reiter Laufwerke, nur gepackt) ─────────────

def test_neue_platte_legt_gepackt_an_und_legt_ein(qapp, konfig_ordner, tmp_path):
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        p = w.gide_platte
        pfad = tmp_path / "neu" / "p.img.gz"
        p.neue_platte(str(pfad), 8)                        # legt auch den Ordner an
        qapp.processEvents()
        assert pfad.is_file() and 0 < pfad.stat().st_size < 64 * 1024     # 8 MB Nullen, gepackt
        assert pfad.read_bytes()[:2] == b"\x1f\x8b"
        st = w.emulator.gide_status()
        assert st["abbild_vorhanden"] and st["sektoren"] == 8 * 1024 * 1024 // 512
        assert w.emulator.gide_image_path() == str(pfad)
        assert p.pfad() == str(pfad) and p.auswerfen_knopf.isEnabled() and p.name.text() == "p.img.gz"
        assert w._gide["image"] == str(pfad)
        # Auswerfen
        p.auswerfen_knopf.click()
        qapp.processEvents()
        assert not w.emulator.gide_status()["abbild_vorhanden"] and w._gide["image"] == ""
        assert not p.auswerfen_knopf.isEnabled()
    finally:
        _zu(w, qapp)


@pytest.mark.parametrize("eingabe,soll", [("a/b/x", "a/b/x.img.gz"), ("x.img", "x.img.gz"), ("x.gz", "x.img.gz"),
                                          ("x.img.gz", "x.img.gz"), ("x.IMG.GZ", "x.IMG.GZ")])
def test_endung_wird_erzwungen(eingabe, soll):
    from app import gide
    assert gide.gz_pfad(eingabe) == soll


def test_neue_platte_haengt_die_endung_an(qapp, konfig_ordner, tmp_path):
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        w.gide_platte.neue_platte(str(tmp_path / "ohne_endung"), 8)
        qapp.processEvents()
        assert (tmp_path / "ohne_endung.img.gz").is_file() and not (tmp_path / "ohne_endung").exists()
        assert w.emulator.gide_image_path() == str(tmp_path / "ohne_endung.img.gz")
    finally:
        _zu(w, qapp)


def test_rohes_abbild_wird_mit_hinweis_abgelehnt(qapp, konfig_ordner, tmp_path):
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        roh = tmp_path / "roh.img"
        roh.write_bytes(bytes(512 * 2048))
        meldungen = []
        w.gide_platte.meldung.connect(meldungen.append)
        assert w.gide_platte.abbild_einlegen(str(roh)) is False
        assert not w.emulator.gide_status()["abbild_vorhanden"] and w._gide["image"] == ""
        text = w.gide_platte.fehler_label.text()
        assert "kein gepacktes Abbild" in text and "gzip" in text and w.gide_platte.fehler_label.isVisibleTo(w.gide_platte)
        assert meldungen and "gzip" in meldungen[0]
        # gepackt wird angenommen
        import gzip
        gz = tmp_path / "ok.img.gz"
        gz.write_bytes(gzip.compress(bytes(512 * 2048)))
        assert w.gide_platte.abbild_einlegen(str(gz)) is True
        qapp.processEvents()
        assert w.emulator.gide_image_path() == str(gz)
        assert not w.gide_platte.fehler_label.isVisibleTo(w.gide_platte)
    finally:
        _zu(w, qapp)


def test_neue_platte_ueberschreibt_nie(qapp, konfig_ordner, tmp_path, monkeypatch):
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        p = w.gide_platte
        pfad = tmp_path / "da.img.gz"
        pfad.write_bytes(b"wichtig")
        fragen = []
        monkeypatch.setattr(p, "_frage", lambda t, x: fragen.append(x) or False)
        p.neue_platte(str(pfad), 8)
        assert fragen and pfad.read_bytes() == b"wichtig"
        assert w._gide["image"] == "" and not w.emulator.gide_status()["abbild_vorhanden"]
        # Auch der Weg ueber den Kern selbst: false, Datei unberuehrt.
        assert not w.emulator.gide_image_create(str(pfad), 8)
        assert pfad.read_bytes() == b"wichtig"
    finally:
        _zu(w, qapp)


def test_neue_platte_dialogwerte(qapp):
    from app import gide
    from app.ui.gide_platte_widget import NeuePlatteDialog
    d = NeuePlatteDialog("/x")
    assert d.ergebnis() == (os.path.join("/x", "gide.img.gz"), 256)
    assert [d.groesse_combo.itemData(i) for i in range(d.groesse_combo.count())] == list(gide.GROESSEN_MB)
    d.name_edit.setText("/y/z.img")
    assert d.ergebnis()[0] == "/y/z.img.gz"
    assert not hasattr(d, "gz_box")                        # kein Roh-Zweig mehr


def test_einstellungen_haben_kein_abbildfeld(qapp, konfig_ordner):
    from PySide6.QtWidgets import QLineEdit, QPushButton
    w = _fenster(qapp)
    try:
        sw = w.settings_widget
        for t in (sw.gide_gruppe, sw.gide_rtc_gruppe):
            assert not t.findChildren(QLineEdit)
            assert not [b for b in t.findChildren(QPushButton) if "Platte" in b.text() or "Auswerfen" in b.text()]
    finally:
        _zu(w, qapp)


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
    img = tmp_path / "r.img.gz"
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        g = w.settings_widget.gide_gruppe
        g.basis_combo.setCurrentIndex(g.basis_combo.findData(0xA0))
        qapp.processEvents()
        w.gide_platte.neue_platte(str(img), 8)
        r = w.settings_widget.gide_rtc_gruppe
        r.versatz_combo.setCurrentIndex(r.versatz_combo.findData("-28"))
        qapp.processEvents()
        r.rtc_box.setChecked(False)
        qapp.processEvents()
    finally:
        _zu(w, qapp)
    daten = yaml.safe_load(Path(w._konfig_pfad()).read_text(encoding="utf-8"))
    assert daten["general"]["gide"] == {"enabled": True, "basis": "A0", "image": str(img), "rtc": False,
                                      "rtc_versatz": "-28"}

    w = _fenster(qapp)
    try:
        assert w.emulator.gide_installed() and w.emulator.gide_status()["basis"] == 0xA0
        assert w.settings_widget.gide_gruppe.fehler_label.text() == ""
        assert w.emulator.gide_image_path() == str(img)
        assert w.emulator.gide_rtc_zeit() is None
        g = w.settings_widget.gide_gruppe
        r = w.settings_widget.gide_rtc_gruppe
        assert g.karte_box.isChecked() and not r.rtc_box.isChecked() and w.gide_platte.pfad() == str(img)
        assert r.versatz_combo.currentData() == "-28" and w.gide_platte.isVisibleTo(w.drives_dock.widget())
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
        assert w._gide["rtc_versatz"] == "-40"
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
    assert g["enabled"] is False and g["image"] == "" and g["rtc"] is True and g["rtc_versatz"] == "-40"
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
        w.gide_platte.neue_platte(str(tmp_path / "l.img.gz"), 8)
        w._update_gide()
        assert ide.lampe.zustand() == status_bar.BELEGT
        assert "l.img.gz" in ide.lampe.toolTip() and "90H" in ide.lampe.toolTip()
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


# ── Uhr (Zeilen im P8000-Stil), Zeitversatz, Scrollbarkeit ───────────────────

def test_zeitversatz_wirkt_nach_dem_neuaufbau_und_steht_in_der_konfig(qapp, konfig_ordner):
    import time
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        r = w.settings_widget.gide_rtc_gruppe
        jahr = time.gmtime().tm_year
        for wert, diff in (("-28", 28), ("-40", 40), ("0", 0)):
            r.versatz_combo.setCurrentIndex(r.versatz_combo.findData(wert))
            qapp.processEvents()
            assert w._gide["rtc_versatz"] == wert
            assert time.gmtime(w.emulator.gide_rtc_zeit()).tm_year == jahr - diff
        daten = w._gather_config()["general"]["gide"]
        assert daten["rtc_versatz"] == "0"
    finally:
        _zu(w, qapp)


def test_uhr_zeigt_gast_und_wirtszeit_und_stellt_sich_nach_der_pc_uhr(qapp, konfig_ordner):
    import time
    from app import rtc
    w = _fenster(qapp)
    try:
        _stecken(w, qapp)
        r = w.settings_widget.gide_rtc_gruppe
        w._update_gide_uhr()
        # Gast -40 Jahre, Wirtszeit = jetzt (UTC); Statuszeile mit „RTC: …“
        jahr = time.gmtime().tm_year
        assert str(jahr - 40) in r.rtc_zeit_label.text() and str(jahr) in r.wirt_label.text()
        assert w.status_widget.rtc.isVisibleTo(w.status_widget)
        # Gast-Uhr stellen: zurueckgestellt auf 1990, dann „nach PC-Uhr stellen“ (mit Rueckfrage ueber QMessageBox)
        w.emulator.gide_rtc_stellen(int(time.time()) - 10 * 365 * 86400)
        from PySide6.QtWidgets import QMessageBox
        antworten = []
        orig = QMessageBox.question
        QMessageBox.question = staticmethod(lambda *a, **k: antworten.append(1) or QMessageBox.Yes)
        try:
            r.stellen_knopf.click()
            w._update_gide_uhr()
        finally:
            QMessageBox.question = orig
        assert not antworten       # vorwaerts gestellt: keine Rueckfrage
        assert abs(w.emulator.gide_rtc_zeit() - rtc.um_jahre(int(time.time()), -40)) < 5
        # Ohne RTC-Haken: Anzeige leer, Statusfeld weg
        r.rtc_box.setChecked(False)
        qapp.processEvents()
        w._update_gide_uhr()
        assert r.rtc_zeit_label.text() == "—"
    finally:
        _zu(w, qapp)


def test_einstellungen_sind_senkrecht_scrollbar_und_die_mindesthoehe_haengt_nicht_an_den_zeilen(qapp, konfig_ordner):
    from PySide6.QtCore import Qt
    from PySide6.QtWidgets import QScrollArea
    w = _fenster(qapp)
    try:
        sw = w.settings_widget
        seite = sw.tabs.widget(0)
        assert isinstance(seite, QScrollArea)
        assert seite.horizontalScrollBarPolicy() == Qt.ScrollBarAlwaysOff and seite.widgetResizable()
        inhalt = seite.widget().sizeHint().height()          # so hoch waere die Seite ohne Scrollen
        mindest = sw.minimumSizeHint().height()
        print(f"Einstellungen: Inhalt {inhalt} px, minimumSizeHint {mindest} px, Fenster {w.minimumSizeHint().height()} px")
        assert inhalt > 500 and mindest < inhalt - 250          # deutlich kleiner als der Inhalt
        assert mindest < 400
        # unabhaengig von den Zeilen: Karten ein- und ausblenden aendert die Mindesthoehe nicht
        sw.karten_box.setVisible(False)
        assert abs(sw.minimumSizeHint().height() - mindest) < 5
    finally:
        _zu(w, qapp)


def test_beenden_sichert_die_uhr_in_die_batteriedatei(qapp, konfig_ordner):
    from app import gide
    w = _fenster(qapp)
    _stecken(w, qapp)
    datei = gide.batterie_pfad(w.profil.programm)
    if os.path.exists(datei):
        os.remove(datei)
    w.emulator.gide_rtc_stellen(1_700_000_000)               # stellt UND schreibt
    _zu(w, qapp)
    text = Path(datei).read_text()
    assert text.startswith("K1520-GIDE-RTC 2") and "stand=" in text

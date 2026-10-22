"""P8000: die Uhrenkarte (RTC) in der Oberflaeche (Entwurf 37, AP R3).

Gruppe *RTC* unter *Einstellungen ▸ Allgemein* (Typ, Zeitversatz, Uhrzeit des Gastsystems, Schaltflaeche),
Feld „RTC:“ der Statuszeile, Batteriedatei je Instanz, Weitergabe an den Kern ueber das Programmprofil.
Gerechnet wird im Kern (`test_p8000_rtc.py`); hier geht es um Verdrahtung, Vorgaben und Sperren.
"""

import calendar
import time
from pathlib import Path

import pytest

from conftest import requires_core

pytestmark = requires_core

ZIEL = calendar.timegm((2030, 1, 1, 12, 0, 0))     # Wirtsjahr 2030 -> Gast 1990 bei -40


@pytest.fixture
def umgebung(tmp_path, monkeypatch):
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "xdg"))
    monkeypatch.setenv("K1520_DISKS", str(tmp_path / "disks"))
    monkeypatch.delenv("K1520_INSTANZ", raising=False)
    monkeypatch.delenv("K1520_KONFIG", raising=False)
    from app import config_io
    ordner = Path(config_io.default_config_dir())
    ordner.mkdir(parents=True, exist_ok=True)
    return ordner


def _fenster(qapp, modell=None):
    from app import profil
    from app.ui.main_window import MainWindow
    w = MainWindow(None, profil=profil.profil("p8000"))
    w.run_timer.stop()
    if modell and modell != w._model:
        w._on_model_selected(modell)
        w.run_timer.stop()
    w.show()
    qapp.processEvents()
    return w


def _zu(w, qapp):
    w.close()
    qapp.processEvents()


def _typ(w, wert):
    box = w.settings_widget.hardware_combos["rtc"]
    box.setCurrentIndex(box.findData(wert))


def _versatz(w, wert):
    box = w.settings_widget.hardware_combos["rtc_versatz"]
    box.setCurrentIndex(box.findData(wert))


def _stand(pfad) -> int:
    for z in Path(pfad).read_text().splitlines():
        if z.startswith("stand="):
            return int(z[6:])
    raise AssertionError(Path(pfad).read_text())


def test_felder_und_vorgabe_aus_der_auslieferung(umgebung, qapp):
    from app import rtc
    w = _fenster(qapp)
    try:
        sw = w.settings_widget
        typ, versatz = sw.hardware_combos["rtc"], sw.hardware_combos["rtc_versatz"]
        assert [typ.itemData(i) for i in range(typ.count())] == ["keine", "u130x", "rtc72421"]
        assert [typ.itemText(i) for i in range(typ.count())] == [
            "keine", "Original U130x", "Epson RTC-72421"]
        assert [versatz.itemData(i) for i in range(versatz.count())] == ["0", "-28", "-40"]
        assert [versatz.itemText(i) for i in range(versatz.count())] == [
            "Kein Zeitversatz", "−28 Jahre (gleicher Wochentag)", "−40 Jahre"]
        # Vorgabe der Auslieferung (data/default_config_p8000.yaml), nicht die des Kerns
        assert typ.currentData() == "u130x" and versatz.currentData() == "-40"
        assert w._hardware["rtc"] == "u130x" and w._hardware["rtc_versatz"] == "-40"
        assert typ.isEnabled() and versatz.isEnabled() and sw.rtc_stellen_button.isEnabled()
        # nebeneinander, in EINER Gruppe
        assert typ.parent() is sw.rtc_gruppe and versatz.parent() is sw.rtc_gruppe
        assert typ.geometry().top() == versatz.geometry().top() or not sw.isVisible()
        assert rtc.VORGABE_TYP == "u130x" and rtc.VORGABE_VERSATZ == "-40"
    finally:
        _zu(w, qapp)


def test_die_ausgelieferte_vorgabedatei_traegt_die_uhr(umgebung):
    import yaml
    from app.config_io import standard_konfiguration
    from app import profil
    g = standard_konfiguration(profil.profil("p8000"))["general"]
    assert g["rtc"] == "u130x" and str(g["rtc_versatz"]) == "-40"


def test_versatz_nur_mit_karte_und_taste_dazu(umgebung, qapp):
    w = _fenster(qapp)
    try:
        sw = w.settings_widget
        _typ(w, "keine")
        qapp.processEvents()
        assert not sw.hardware_combos["rtc_versatz"].isEnabled()
        assert not sw.rtc_stellen_button.isEnabled()
        assert sw.hardware_combos["rtc"].isEnabled()
        assert w._hardware["rtc_versatz"] == "-40"            # gemerkt, nur gesperrt
        _typ(w, "rtc72421")
        qapp.processEvents()
        assert sw.hardware_combos["rtc_versatz"].isEnabled() and sw.rtc_stellen_button.isEnabled()
    finally:
        _zu(w, qapp)


def test_nur_8bit_sperrt_beide_und_der_wert_ueberlebt(umgebung, qapp):
    w = _fenster(qapp)
    try:
        sw = w.settings_widget
        _typ(w, "rtc72421")
        _versatz(w, "-28")
        w._on_model_selected("p8000-8-ot")
        qapp.processEvents()
        assert not sw.hardware_combos["rtc"].isEnabled()
        assert not sw.hardware_combos["rtc_versatz"].isEnabled()
        assert not sw.rtc_stellen_button.isEnabled()
        assert w._hardware["rtc"] == "rtc72421" and w._hardware["rtc_versatz"] == "-28"
        assert "rtc" not in w.profil.kern_parameter(w._model, w._hardware)["p8000"]
        assert w.emulator.rtc_zeit() == -1                    # keine Karte am 8-Bit-Aufbau
        assert w.status_widget.rtc.isHidden()
        w._on_model_selected("p8000-ot")                      # zurueck mit 16-Bit-Teil: wirkt wieder
        qapp.processEvents()
        assert sw.hardware_combos["rtc"].isEnabled() and sw.hardware_combos["rtc_versatz"].isEnabled()
        assert sw.hardware_combos["rtc"].currentData() == "rtc72421"
        assert sw.hardware_combos["rtc_versatz"].currentData() == "-28"
        assert w.emulator.rtc_zeit() >= 0
    finally:
        _zu(w, qapp)


def test_terminalbetrieb_blendet_die_gruppe_aus(umgebung, qapp):
    w = _fenster(qapp)
    try:
        sw = w.settings_widget
        assert not sw.rtc_gruppe.isHidden()
        _typ(w, "rtc72421")
        sw.betriebsart_combo.setCurrentIndex(sw.betriebsart_combo.findData("terminal"))
        qapp.processEvents()
        assert sw.rtc_gruppe.isHidden()
        assert w.status_widget.rtc.isHidden()
        sw.betriebsart_combo.setCurrentIndex(sw.betriebsart_combo.findData("computer"))
        qapp.processEvents()
        assert not sw.rtc_gruppe.isHidden()
        assert w._hardware["rtc"] == "rtc72421"
    finally:
        _zu(w, qapp)


def test_konfig_rundreise_und_fehlende_schluessel_sind_die_vorgabe(umgebung, qapp):
    w = _fenster(qapp)
    try:
        _typ(w, "rtc72421")
        _versatz(w, "0")
        daten = w._gather_config()
        assert daten["general"]["rtc"] == "rtc72421" and str(daten["general"]["rtc_versatz"]) == "0"
    finally:
        _zu(w, qapp)
    w2 = _fenster(qapp)
    try:
        w2._apply_config(daten)
        assert w2._hardware["rtc"] == "rtc72421" and w2._hardware["rtc_versatz"] == "0"
        assert w2.settings_widget.hardware_combos["rtc"].currentData() == "rtc72421"
        # aeltere Konfiguration ohne die Schluessel: Vorgabe (U130x / -40)
        alt = {k: v for k, v in daten["general"].items() if not k.startswith("rtc")}
        w2._apply_config({**daten, "general": alt})
        assert w2._hardware["rtc"] == "u130x" and w2._hardware["rtc_versatz"] == "-40"
    finally:
        _zu(w2, qapp)


def test_kern_parameter_traegt_typ_versatz_und_batteriedatei(umgebung, monkeypatch):
    from app import profil, rtc
    p = profil.profil("p8000")
    hw = p.hardware_standard("p8000-ot")
    k = p.kern_parameter("p8000-ot", hw)["p8000"]
    assert k["rtc"] == "u130x" and k["rtc_jahresversatz"] == -40
    assert k["rtc_zustand"] == rtc.zustand_pfad("p8000emu")
    assert Path(k["rtc_zustand"]).parent == Path(rtc.paths.config_dir())
    hw["rtc"], hw["rtc_versatz"] = "rtc72421", "-28"
    k = p.kern_parameter("p8000-16-ot", hw)["p8000"]
    assert k["rtc"] == "rtc72421" and k["rtc_jahresversatz"] == -28
    hw["rtc"] = "keine"
    assert "rtc" not in p.kern_parameter("p8000-ot", hw)["p8000"]
    assert "rtc" not in p.kern_parameter("p8000-8-ot", p.hardware_standard("p8000-8-ot"))["p8000"]
    # je Instanz eine eigene Batteriedatei
    monkeypatch.setenv("K1520_INSTANZ", "platz2")
    assert rtc.zustand_pfad("p8000emu").endswith("rtc_p8000emu-platz2.txt")
    monkeypatch.delenv("K1520_INSTANZ")
    assert rtc.zustand_pfad("p8000emu").endswith("rtc_p8000emu.txt")


def test_die_maschine_traegt_die_karte_mit_dem_versatz(umgebung, qapp):
    w = _fenster(qapp)
    try:
        t = w.emulator.rtc_zeit()
        assert t >= 0
        assert time.gmtime(t).tm_year == time.localtime().tm_year - 40
        _versatz(w, "-28")                                     # Neubau der Maschine
        assert time.gmtime(w.emulator.rtc_zeit()).tm_year == time.localtime().tm_year - 28
    finally:
        _zu(w, qapp)


def test_anzeige_unbekannt_ohne_karte_und_statusfeld_aus(umgebung, qapp):
    w = _fenster(qapp)
    try:
        _typ(w, "keine")
        w._update_rtc()
        assert w.settings_widget.rtc_zeit_label.text() == "unbekannt"
        assert w.status_widget.rtc.isHidden()
        assert w.emulator.rtc_zeit() == -1
    finally:
        _zu(w, qapp)


def test_anzeige_laufend_ausgeschaltet_und_statusfeld(umgebung, qapp):
    from app import rtc
    w = _fenster(qapp)
    try:
        assert w.act_power.isChecked()
        assert w.emulator.rtc_stellen(ZIEL)
        w._update_rtc()
        erwartet = rtc.anzeige(rtc.um_jahre(ZIEL, -40))        # 01.01.1990 12:00
        assert erwartet == "01.01.1990 12:00"
        assert w.settings_widget.rtc_zeit_label.text() == erwartet
        assert w.status_widget.rtc.isVisible()
        assert w.status_widget.rtc.text.text() == "RTC: 01.01.1990 12:00"
        # aus: die Zeit, mit der sie jetzt starten wuerde (Startregel des Kerns, aus der Datei)
        w.act_power.setChecked(False)
        qapp.processEvents()
        w._update_rtc()
        assert w.settings_widget.rtc_zeit_label.text() == erwartet
        assert w.status_widget.rtc.text.text() == "RTC: " + erwartet
        # Datei von aussen auf eine andere Zeit: die Anzeige folgt der Startzeit, nicht der Karte
        from app.core_binding.k1520 import K1520Emulator
        ziel2 = calendar.timegm((2031, 6, 7, 8, 9, 0))
        assert K1520Emulator.rtc_stellen_datei(rtc.zustand_pfad("p8000emu"), ziel2)
        w._update_rtc()
        assert w.settings_widget.rtc_zeit_label.text() == "07.06.1991 08:09"
    finally:
        _zu(w, qapp)


def _frage(monkeypatch, antwort):
    from PySide6.QtWidgets import QMessageBox
    gefragt = []

    def frage(*a, **k):
        gefragt.append([x for x in a if isinstance(x, str)])
        return antwort
    monkeypatch.setattr(QMessageBox, "question", staticmethod(frage))
    return gefragt


def test_stellen_ohne_ruecksprung_fragt_nicht(umgebung, qapp):
    w = _fenster(qapp)
    try:
        assert w.emulator.rtc_stellen(calendar.timegm((2020, 1, 1, 0, 0, 0)))
        w.settings_widget.rtc_stellen_button.click()           # kein Meldungsfenster erlaubt (conftest)
        assert time.gmtime(w.emulator.rtc_zeit()).tm_year == time.localtime().tm_year - 40
        assert abs(w.emulator.rtc_zeit() - (calendar.timegm(time.localtime()) - 40 * 365.2425 * 86400)) < 3 * 86400
    finally:
        _zu(w, qapp)


def test_stellen_mit_ruecksprung_fragt_und_nein_aendert_nichts(umgebung, qapp, monkeypatch):
    w = _fenster(qapp)
    try:
        assert w.emulator.rtc_stellen(ZIEL)
        gefragt = _frage(monkeypatch, 65536)                   # QMessageBox.No
        from PySide6.QtWidgets import QMessageBox
        gefragt = _frage(monkeypatch, QMessageBox.No)
        w.settings_widget.rtc_stellen_button.click()
        assert len(gefragt) == 1
        text = " ".join(gefragt[0])
        assert "01.01.1990 12:00" in text and "make" in text and "Zukunft" in text
        assert w.emulator.rtc_zeit() == rtc_ziel(-40)
        gefragt = _frage(monkeypatch, QMessageBox.Yes)
        w.settings_widget.rtc_stellen_button.click()
        assert len(gefragt) == 1
        assert time.gmtime(w.emulator.rtc_zeit()).tm_year == time.localtime().tm_year - 40
    finally:
        _zu(w, qapp)


def rtc_ziel(versatz):
    from app import rtc
    return rtc.um_jahre(ZIEL, versatz)


def test_stellen_bei_ausgeschalteter_maschine_schreibt_die_batteriedatei(umgebung, qapp, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    from app import rtc
    w = _fenster(qapp)
    try:
        assert w.emulator.rtc_stellen(ZIEL)
        w.act_power.setChecked(False)
        qapp.processEvents()
        _frage(monkeypatch, QMessageBox.Yes)
        w.settings_widget.rtc_stellen_button.click()
        datei = rtc.zustand_pfad("p8000emu")
        assert abs(_stand(datei) - calendar.timegm(time.localtime())) <= 5
        w._update_rtc()
        assert w.settings_widget.rtc_zeit_label.text() == rtc.anzeige(
            rtc.um_jahre(calendar.timegm(time.localtime()), -40))
    finally:
        _zu(w, qapp)


def test_stellen_faellt_ohne_karte_auf_die_datei_zurueck(umgebung, qapp, monkeypatch):
    from app import rtc
    w = _fenster(qapp)
    try:
        monkeypatch.setattr(w.emulator, "rtc_stellen", lambda t: False)
        w.act_power.setChecked(False)
        qapp.processEvents()
        w._on_rtc_stellen()
        assert abs(_stand(rtc.zustand_pfad("p8000emu")) - calendar.timegm(time.localtime())) <= 5
    finally:
        _zu(w, qapp)


def test_stellen_ohne_karte_tut_nichts(umgebung, qapp):
    w = _fenster(qapp, "p8000-8-ot")
    try:
        w._on_rtc_stellen()                                    # kein Fehler, keine Frage
        assert not w.settings_widget.rtc_stellen_button.isEnabled()
    finally:
        _zu(w, qapp)


def test_keine_tastenkuerzel_fuer_die_uhr(umgebung, qapp):
    w = _fenster(qapp)
    try:
        assert w.settings_widget.rtc_stellen_button.shortcut().isEmpty()
    finally:
        _zu(w, qapp)


def test_versatzfeld_nennt_die_grenzen_und_sperrt_keinen_wert(umgebung, qapp):
    """Entscheidung Anwender 2026-10-22: alle drei Versaetze fuer beide Karten waehlbar, Tooltip nennt die Grenzen."""
    w = _fenster(qapp)
    try:
        box = w.settings_widget.hardware_combos["rtc_versatz"]
        tipp = box.toolTip()
        for text in ("1980–2019", "2028", "2038", "2037", "2065", "2077", "sa.timer"):
            assert text in tipp, text
        for typ in ("u130x", "rtc72421"):
            _typ(w, typ)
            for wert in ("0", "-28", "-40"):
                _versatz(w, wert)
                assert w._hardware["rtc_versatz"] == wert and box.isEnabled()
                assert w.emulator.rtc_zeit() >= 0
    finally:
        _zu(w, qapp)


# ── AP R3b: Netz-Aus/Ein und Neubau ──────────────────────────────────────────

def test_neubau_die_alte_karte_dreht_die_batteriedatei_nicht_zurueck(umgebung, qapp):
    """Die neue Maschine liest die Datei beim Anlegen; die alte stirbt erst danach.  Weder der Stand der
    neuen Maschine noch die Datei duerfen durch den Tod der alten zurueckspringen."""
    import gc
    from app import rtc
    w = _fenster(qapp)
    try:
        assert w.emulator.rtc_stellen(ZIEL)
        alt = w.emulator                                       # haelt die alte Maschine am Leben
        _versatz(w, "-28")                                     # Neubau
        assert w.emulator is not alt
        datei = rtc.zustand_pfad("p8000emu")
        neu_stand = w.emulator.rtc_zeit()
        # Datei = Stand der alten beim Neubau (netz_aus), neue Maschine startet damit (Minutengenauigkeit)
        assert abs(_stand(datei) - ZIEL) <= 60
        assert abs(neu_stand - rtc.um_jahre(ZIEL, -28)) <= 60
        # die neue Maschine schreibt die Datei fort (Stellen), danach stirbt die alte
        assert w.emulator.rtc_stellen(ZIEL + 5000)
        assert _stand(datei) == ZIEL + 5000
        del alt
        gc.collect()
        assert _stand(datei) == ZIEL + 5000                   # kein Ruecksprung
        assert w.emulator.rtc_zeit() >= neu_stand
    finally:
        _zu(w, qapp)
    assert _stand(rtc.zustand_pfad("p8000emu")) >= ZIEL + 5000


def test_aus_ein_anzeige_vor_dem_einschalten_ist_die_zeit_danach(umgebung, qapp, monkeypatch):
    """Ausgeschaltet zeigt die Oberflaeche die Startzeit; die Maschine startet mit genau dieser Zeit,
    auch wenn die Wirtszeit in der Aus-Dauer weitergelaufen ist."""
    from app import rtc
    w = _fenster(qapp)
    try:
        assert w.emulator.rtc_stellen(ZIEL)
        w.act_power.setChecked(False)                          # Netz-Aus: Stand + Wirtszeit in die Datei
        qapp.processEvents()
        # 2 Stunden "Aus": die Datei traegt die Wirtszeit des Ausschaltens, wir rueckdatieren sie
        datei = Path(rtc.zustand_pfad("p8000emu"))
        text = datei.read_text().splitlines()
        wirt = int(next(z for z in text if z.startswith("wirt="))[5:]) - 7200
        datei.write_text("\n".join(z if not z.startswith("wirt=") else f"wirt={wirt}" for z in text) + "\n")
        w._update_rtc()
        angezeigt = w.settings_widget.rtc_zeit_label.text()
        assert angezeigt == rtc.anzeige(rtc.um_jahre(ZIEL + 7200, -40))
        w.act_power.setChecked(True)                           # Netz-Ein
        qapp.processEvents()
        w.run_timer.stop()
        assert rtc.anzeige(w.emulator.rtc_zeit()) == angezeigt
    finally:
        _zu(w, qapp)

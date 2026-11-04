"""Ticket #26 — Betriebsart „nur Computer“ im p8000emu: Rechner OHNE Originalterminal, die Konsole tty1
geht ueber die Schnittstellen nach aussen (Entwurf 26 §8).  Verdrahtung, kein Bild."""

import subprocess
import sys

import pytest

from conftest import PROJECT_ROOT, requires_core
from test_p8000_original_gui import _fenster, _zu, umgebung  # noqa: F401  (Fixture + Hilfen)

pytestmark = requires_core


def test_profil_kennt_die_nt_modelle_und_haelt_sie_aus_der_ausstattung_heraus():
    from app import profil
    p = profil.profil("p8000")
    for ot in ("p8000-ot", "p8000-16-ot", "p8000-8-ot"):
        nt = p.ohne_terminal_von(ot)
        assert nt.endswith("-nt") and p.mit_terminal_von(nt) == ot
        assert p.modell_ohne_terminal(nt) and not p.modell_ohne_terminal(ot)
        assert p.modell_hat_rechner(nt) and not p.modell_original_terminal(nt)
        assert p.modell_normalisieren(nt) == nt
        assert p.kern_parameter(nt)["p8000"]["terminal"] == "none"
    assert all(m[0].endswith("-ot") for m in p.ausstattungen())
    assert p.modell_hat_wdc("p8000-nt") and not p.modell_hat_wdc("p8000-16-nt")
    assert p.kern_parameter("p8000-8-nt")["p8000"]["karte16"] == "0"
    assert ("tty1 CONSOLE", 5001) in p.schnittstellen_ports
    assert p.ausstattung_normalisieren("p8000-16-nt") == "p8000-16-ot"


def test_betriebsart_nur_computer_ohne_terminal_mit_ausgegrauten_terminalaktionen(qapp, umgebung):
    w = _fenster(qapp, modell="p8000-16-ot")
    try:
        sw = w.settings_widget
        assert w.act_text_kopieren.isEnabled() and w.keyboard_widget.isEnabled()
        assert sw.betriebsart_combo.findData("computer-only") >= 0
        sw.betriebsart_combo.setCurrentIndex(sw.betriebsart_combo.findData("computer-only"))
        w.run_timer.stop()
        assert w._model == "p8000-16-nt" and w.emulator.machine == "p8000"
        assert w.emulator.term_count() == 0
        # Rechner bleibt voll da, Ausstattung wird gemerkt und bleibt waehlbar
        assert w.status_widget.rechner_sichtbar() and sw.model_combo.isEnabled()
        assert sw.ausstattung_value() == "p8000-16-ot" and sw.model_value() == "p8000-16-nt"
        # Terminalbedienung: ausgegraut (nicht ausgeblendet), mit Grund
        assert not w.act_text_kopieren.isEnabled() and "nur Computer" in w.act_text_kopieren.toolTip()
        assert not w.act_vollbild.isEnabled() and not w.keyboard_widget.isEnabled()
        assert not w.act_dock_keyboard.isEnabled()
        assert not sw.tabs.isTabEnabled(sw.tabs.indexOf(sw._crt_tab))
        assert not sw._mitschrift_zeile.isEnabled()
        assert w.screen_stapel.currentWidget() is w.konsole_extern_hinweis
        assert "tty1" in w.konsole_extern_hinweis.text()
        # tty1 steht zwischen tty0 und tty2, Port-Vorschlag 5001
        namen = [b.info.name for b in w.serial_widget.bloecke()]
        assert namen[:4] == ["tty0", "tty1 CONSOLE", "tty2", "tty3 PRINTER"]
        assert w.emulator.serial_config(1).port == 5001
        assert "Nur Computer" in w.serial_widget.hinweis
        # Zwischenstand und Frontplatte arbeiten weiter
        w._autosave_now()
        from app import config_io
        g = config_io.load_config(str(umgebung / "p8000emu.yaml"))["general"]
        assert g["model"] == "p8000-16-nt" and g["ausstattung"] == "p8000-16-ot"

        # zurueck: alles wieder frei
        sw.betriebsart_combo.setCurrentIndex(sw.betriebsart_combo.findData("computer"))
        w.run_timer.stop()
        assert w._model == "p8000-16-ot" and w.emulator.term_count() == 1
        assert w.act_text_kopieren.isEnabled() and w.act_vollbild.isEnabled()
        assert w.keyboard_widget.isEnabled() and sw.tabs.isTabEnabled(sw.tabs.indexOf(sw._crt_tab))
        assert sw._mitschrift_zeile.isEnabled()
        assert w.screen_stapel.currentWidget() is w.screen_widget
    finally:
        _zu(w, qapp)


def test_gemerkte_betriebsart_nur_computer_wird_geladen(qapp, umgebung):
    w = _fenster(qapp, modell="p8000-nt")
    try:
        w._autosave_now()
    finally:
        _zu(w, qapp)
    w = _fenster(qapp)
    try:
        assert w._model == "p8000-nt"
        assert w.settings_widget.betriebsart_combo.currentData() == "computer-only"
        assert w.settings_widget.ausstattung_value() == "p8000-ot"
        assert not w.act_text_kopieren.isEnabled()
    finally:
        _zu(w, qapp)


def test_start_mit_modus_computer_only(qapp, umgebung, monkeypatch):
    monkeypatch.setenv("K1520_BETRIEBSART", "computer-only")
    w = _fenster(qapp)
    try:
        assert w._model == "p8000-nt"
        assert w.settings_widget.betriebsart_combo.currentData() == "computer-only"
    finally:
        _zu(w, qapp)
    r = subprocess.run([sys.executable, str(PROJECT_ROOT / "app" / "main.py"), "--machine", "p8000",
                        "--mode", "bogus"], capture_output=True, text=True, cwd=str(PROJECT_ROOT))
    assert r.returncode == 2 and "computer-only" in r.stderr
    r = subprocess.run([sys.executable, str(PROJECT_ROOT / "app" / "main.py"), "--machine", "p8000",
                        "--help"], capture_output=True, text=True, cwd=str(PROJECT_ROOT))
    assert "computer-only" in r.stdout

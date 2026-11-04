"""Ticket #26 — Schnittstellennamen ohne Steckerangabe (tty1 CONSOLE / tty3 PRINTER), Aktion „Terminal“
und ``--connect`` des p8000emu.  Es werden keine Prozesse gestartet: der Starter wird ersetzt."""

import subprocess
import sys

import pytest
from PySide6.QtWidgets import QMenu

from conftest import PROJECT_ROOT, requires_core
from test_p8000_original_gui import _fenster, _zu, umgebung  # noqa: F401  (Fixture + Hilfen)

pytestmark = requires_core


@pytest.fixture
def starts(monkeypatch):
    """Ersetzt den Starter anderer Programme; sammelt (Kennung, Argumente)."""
    from app import programme
    liste = []
    monkeypatch.setattr(programme, "programm_starten", lambda kennung, argumente=(): liste.append(
        (kennung, list(argumente))))
    return liste


def _namen(w):
    return [b.info.name for b in w.serial_widget.bloecke()]


def test_namen_ohne_steckerangabe_und_mit_console_printer(qapp, umgebung):
    w = _fenster(qapp, modell="p8000-nt")
    try:
        namen = _namen(w)
        assert namen == ["tty0", "tty1 CONSOLE", "tty2", "tty3 PRINTER", "tty4", "tty5", "tty6", "tty7"]
        assert not any("(X" in n for n in namen)
        assert all(not w.emulator.serial_info(i).stecker for i in range(len(namen)))
    finally:
        _zu(w, qapp)


def test_alte_schnittstellennamen_werden_abgebildet():
    from app import profil
    p = profil.profil("p8000")
    d = {"tty1": {"port": 1}, "tty3": {"port": 3}, "tty4 (X4)": {"port": 4}, "tty0": {"port": 0}}
    n = p.schnittstellen_umbenennen(d)
    assert n == {"tty1 CONSOLE": {"port": 1}, "tty3 PRINTER": {"port": 3}, "tty4": {"port": 4},
                 "tty0": {"port": 0}}
    # ein Eintrag unter dem heutigen Namen geht vor
    n = p.schnittstellen_umbenennen({"tty1": {"port": 1}, "tty1 CONSOLE": {"port": 9}})
    assert n["tty1 CONSOLE"] == {"port": 9}


def test_aktion_terminal_nur_in_computer_und_nur_computer(qapp, umgebung):
    w = _fenster(qapp, modell="p8000-ot")
    try:
        assert w.act_terminal.isVisible() and w.act_terminal.shortcut().isEmpty()
        werkzeuge = [m for m in w.menuBar().findChildren(QMenu)
                     if m.title() == "&Werkzeuge"]
        assert any(w.act_terminal in m.actions() for m in werkzeuge)
        w._on_model_selected("p8000-nt")
        w.run_timer.stop()
        assert w.act_terminal.isVisible()
        w._on_model_selected("p8000-term")
        w.run_timer.stop()
        assert not w.act_terminal.isVisible()
    finally:
        _zu(w, qapp)
    from app import profil
    for m in ("a5120", "k8915", "prg710", "pc1715", "mc80"):
        w = _fenster(qapp, programm=m)
        try:
            assert not hasattr(w, "act_terminal")
        finally:
            _zu(w, qapp)


def test_terminal_nimmt_die_ersten_freien_schnittstellen_und_ueberspringt_den_drucker(
        qapp, umgebung, starts):
    from app.core_binding import k1520 as K
    w = _fenster(qapp, modell="p8000-ot")        # tty1 gehoert dem Originalterminal
    try:
        erwartet = ["tty0", "tty2", "tty4", "tty5", "tty6", "tty7"]     # tty3 PRINTER fehlt
        ports = []
        for n, name in enumerate(erwartet):
            w.act_terminal.trigger()
            assert len(starts) == n + 1
            kennung, args = starts[-1]
            from app import programme
            assert kennung == programme.P8000EMU
            assert args[:2] == ["--mode", "terminal"] and args[2] == "--connect"
            host, _, port = args[3].rpartition(":")
            assert host == "127.0.0.1"
            assert args[4:] == ["--instance", f"terminal-{port}"]
            i = _namen(w).index(name)
            st = w.emulator.serial_status(i)
            assert st.zustand in (K.SER_LAUSCHT, K.SER_VERBUNDEN) and st.port_aktiv == int(port)
            assert w.emulator.serial_config(i).rolle == K.SER_SERVER
            ports.append(int(port))
        assert len(set(ports)) == len(ports)
        # erschoepft: kein weiterer Start, Hinweis in der Statuszeile
        w.act_terminal.trigger()
        assert len(starts) == len(erwartet)
        assert "keine freie Schnittstelle" in w.statusBar().currentMessage()
        # der Drucker blieb unberuehrt
        assert w.emulator.serial_status(_namen(w).index("tty3 PRINTER")).zustand == K.SER_AUS
    finally:
        _zu(w, qapp)


def _port_von(args):
    return int(args[3].rpartition(":")[2])


def test_lauschender_unverbundener_server_wird_ohne_neuen_start_wiederverwendet(qapp, umgebung, starts, monkeypatch):
    from app.core_binding import k1520 as K
    w = _fenster(qapp, modell="p8000-ot")
    try:
        w.act_terminal.trigger()
        port0 = _port_von(starts[-1][1])
        w._terminal_vergeben.clear()                 # das Terminal ist „beendet“: Leitung lauscht, kein Peer
        i0 = _namen(w).index("tty0")
        assert w.emulator.serial_status(i0).zustand == K.SER_LAUSCHT
        aufrufe = []
        echt = w.emulator.serial_start
        monkeypatch.setattr(w.emulator, "serial_start", lambda i: aufrufe.append(i) or echt(i))
        w.act_terminal.trigger()
        assert _port_von(starts[-1][1]) == port0 and starts[-1][1][-1] == f"terminal-{port0}"
        assert aufrufe == []                         # kein neuer serial_start
    finally:
        _zu(w, qapp)


def test_zwei_schnelle_klicks_vergeben_verschiedene_leitungen(qapp, umgebung, starts):
    w = _fenster(qapp, modell="p8000-ot")
    try:
        w.act_terminal.trigger()
        w.act_terminal.trigger()                     # tty0 lauscht, ist aber vorgemerkt
        assert _port_von(starts[0][1]) != _port_von(starts[1][1])
        # nach Ablauf der Vormerkung waere tty0 wieder dran
        w._terminal_vergeben.clear()
        w.act_terminal.trigger()
        assert _port_von(starts[2][1]) == _port_von(starts[0][1])
    finally:
        _zu(w, qapp)


def test_verbundene_leitung_wird_uebersprungen(qapp, umgebung, starts):
    import socket
    w = _fenster(qapp, modell="p8000-ot")
    try:
        w.act_terminal.trigger()
        port0 = _port_von(starts[0][1])
        c = socket.create_connection(("127.0.0.1", port0))
        try:
            import time
            from app.core_binding import k1520 as K
            for _ in range(100):
                if w.emulator.serial_status(_namen(w).index("tty0")).zustand == K.SER_VERBUNDEN:
                    break
                time.sleep(0.05)
            w._terminal_vergeben.clear()
            w.act_terminal.trigger()
            assert _port_von(starts[1][1]) != port0
        finally:
            c.close()
    finally:
        _zu(w, qapp)


def test_connect_start_laeuft_im_normaltakt(qapp, umgebung, monkeypatch):
    monkeypatch.setenv("K1520_BETRIEBSART", "terminal")
    monkeypatch.setenv("K1520_VERBINDEN", "127.0.0.1:5991")
    w = _fenster(qapp)
    try:
        assert w.speed_factor == 1.0 and w.settings_widget.speed_value() == 1.0
    finally:
        _zu(w, qapp)


def test_terminalmodell_hat_den_z8_nenntakt(qapp, umgebung):
    from app import profil
    p = profil.profil("p8000")
    assert p.modell_nenntakt("p8000-term") == (3_686_400, "3,6864 MHz")
    assert p.modell_nenntakt("p8000-ot") == (4_000_000, "4 MHz")
    w = _fenster(qapp, modell="p8000-ot")
    try:
        assert w.CPU_HZ == 4_000_000
        w._on_model_selected("p8000-term")
        w.run_timer.stop()
        assert w.CPU_HZ == 3_686_400 and w.status_widget.nenntakt_text == "3,6864 MHz"
        assert w.run_timer._hz == 3_686_400
        w._on_model_selected("p8000-ot")
        w.run_timer.stop()
        assert w.CPU_HZ == 4_000_000
    finally:
        _zu(w, qapp)


def test_bei_nur_computer_ist_tty1_console_die_erste_freie(qapp, umgebung, starts):
    w = _fenster(qapp, modell="p8000-nt")
    try:
        w.act_terminal.trigger()
        w.act_terminal.trigger()
        assert len(starts) == 2
        i0, i1 = _namen(w).index("tty0"), _namen(w).index("tty1 CONSOLE")
        # erste Auswahl = tty1 CONSOLE (die Konsole zuerst), zweite = tty0 (danach Hub-Reihenfolge)
        assert w.emulator.serial_status(i1).port_aktiv == int(starts[0][1][3].rpartition(":")[2])
        assert w.emulator.serial_status(i0).port_aktiv == int(starts[1][1][3].rpartition(":")[2])
    finally:
        _zu(w, qapp)


def test_connect_wird_geprueft():
    from app import instanz
    assert instanz.verbinden_pruefen("127.0.0.1:5001") == ("127.0.0.1", 5001)
    assert instanz.verbinden_pruefen("[::1]:80") == ("::1", 80)
    for schlecht in ("", "host", "host:", ":5001", "host:0", "host:70000", "host:abc", "a b:5"):
        assert instanz.verbinden_pruefen(schlecht) is None, schlecht


def _main(*args):
    return subprocess.run([sys.executable, str(PROJECT_ROOT / "app" / "main.py"), "--machine", "p8000", *args],
                          capture_output=True, text=True, cwd=str(PROJECT_ROOT))


def test_connect_ungueltig_oder_ohne_terminalbetrieb_ist_exitcode_2():
    r = _main("--mode", "terminal", "--connect", "kaputt")
    assert r.returncode == 2 and "--connect" in r.stderr
    r = _main("--mode", "computer", "--connect", "127.0.0.1:5001")
    assert r.returncode == 2 and "--connect" in r.stderr
    r = _main("--connect", "127.0.0.1:5001")
    assert r.returncode == 2
    r = _main("--help")
    assert "--connect" in r.stdout


def test_terminalmaschine_uebernimmt_host_und_port_nur_fuer_diesen_start(qapp, umgebung, monkeypatch):
    from app.core_binding import k1520 as K
    from app import config_io
    monkeypatch.setenv("K1520_BETRIEBSART", "terminal")
    monkeypatch.setenv("K1520_VERBINDEN", "127.0.0.1:5990")
    w = _fenster(qapp)
    try:
        assert w.profil.modell_art(w._model) == "einheit"
        k = w.emulator.serial_config(0)
        assert (k.betriebsart, k.rolle, k.host, k.port) == (K.SER_TELNET, K.SER_CLIENT, "127.0.0.1", 5990)
        st = w.emulator.serial_status(0)
        assert st.zustand in (K.SER_VERBINDET, K.SER_VERBUNDEN)       # Dauerversuch, kein Server da
        # nicht in die Konfiguration zurueckgeschrieben
        w._autosave_now()
        daten = config_io.load_config(str(umgebung / "p8000emu.yaml"))
        name = w.emulator.serial_info(0).name
        gespeichert = (daten.get("schnittstellen") or {}).get(name, {})
        assert gespeichert.get("port") != 5990
    finally:
        _zu(w, qapp)

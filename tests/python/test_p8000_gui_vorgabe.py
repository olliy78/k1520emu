"""P8000: die AUSLIEFERUNGSKONFIGURATION des p8000emu bis zum Prompt ``*`` des U8000-Monitors.

Fehlerbericht (2026-10-09, Merkposten p8000 Nr. 54): „nach MAXSEG=<..> kein ``*``“.  Die M2-Waechter
(`test_p8000_monitor16.cpp`, `test_p8000_kopplung.cpp`) fahren den Kern mit Kern-Terminal, 1M@0 und
ohne WDC — die Oberflaeche aber mit Originalterminal, 4x256K, WDC 4.2 und ``plattepar=aus``.  Hier
laeuft GENAU das, was das Fenster baut (``MainWindow`` mit leerer Konfiguration ⇒
`data/default_config_p8000.yaml` ⇒ ``Profil.kern_parameter``), bedient wie der Anwender:
Hardwaretest U880 → ``x`` + RETURN → „Press NMI“ → NMI-Taster des Fensters (``_on_nmi``).

* ohne Platte und mit unformatierter Platte (Vorgabe von „Neue Platte…“): ERROR 52 …, MAXSEG, ``*``;
* mit Parametersatz, aber leerem Block 0 (fruehere Standardplatte; die Ursache des Berichts):
  MON16 bootet Block 0 (AUTOBOOT) — kein ``*``, wie am Geraet (``p.boot.s``) — Negativkontrolle;
  der Plattenkasten sagt es, und RETURN statt NMI fuehrt zum ``*``.
"""

import os
from pathlib import Path

import pytest

from conftest import requires_core

pytestmark = requires_core


@pytest.fixture
def umgebung(tmp_path, monkeypatch):
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "xdg"))
    monkeypatch.setenv("K1520_DISKS", str(tmp_path / "disks"))
    from app import config_io
    ordner = Path(config_io.default_config_dir())
    ordner.mkdir(parents=True, exist_ok=True)
    return ordner


#: Die beiden Monitorfassungen, die der Waechter bedient.  3.0 ist die Programmvorgabe (Voreinstellung
#: P8000), 3.1 die der Voreinstellung Compact.  Die Bedienfolge ist dieselbe (Press RETURN → x → Press NMI →
#: NMI → MAXSEG → ``*``), nur Banner und Fehlercodes tragen die Fassung (Merkposten p8000 Nr. 63).
FASSUNGEN = ["3.0", "3.1"]


@pytest.fixture(params=FASSUNGEN)
def fassung(request):
    return request.param


def _fenster(qapp, fassung=None):
    """Fenster mit der Auslieferungskonfiguration; *fassung* stellt (nur wenn sie von der Vorgabe
    abweicht) MON8/MON16 um — alles andere (Index, 4x256K, WDC 4.2, plattepar=aus, Originalterminal)
    bleibt die Auslieferung."""
    from app import profil
    from app.ui.main_window import MainWindow
    w = MainWindow(None, profil=profil.profil("p8000"))
    w.run_timer.stop()                        # die Zeit schaltet der Test
    if fassung is not None:
        w._on_hardware_selected("mon8", fassung)
        w._on_hardware_selected("mon16", fassung)
        w.run_timer.stop()
    w._fassung = fassung or w._hardware["mon16"]
    w.show()
    qapp.processEvents()
    return w


def _zu(w, qapp):
    w.close()
    qapp.processEvents()


def _zurueck_zum_u880_monitor(w):
    """RESET-Taste (Platte bleibt) — ausser bei MON 3.0 am Platinenindex 4 (Voreinstellung Compact): dort bleiben die PIOs der
    Kopplung bei der RESET-Taste stehen, und MON8 3.0 meldet danach „Hardware Error in Connection“ (Merkposten
    p8000 Nr. 63, Waechter ``test_mon30_nach_der_reset_taste_...``).  Dann bleibt nur der Kaltstart (Netzschalter)."""
    if w._fassung == "3.0" and w._hardware.get("index") == "34":
        w._cold_restart()
    else:
        w._on_reset()


def _bis(w, text, budget, schritt=500_000):
    """Laufen, bis *text* (oefter als bisher) im Terminal steht — nach einem Rueckstellen steht die
    alte Meldung noch im Bild."""
    vorher = w.emulator.term_text(0).count(text)
    gelaufen = 0
    while gelaufen < budget:
        gelaufen += w.emulator.run(schritt)
        if w.emulator.term_text(0).count(text) > vorher:
            return True
    return False


def _cursorzeile(w) -> str:
    _spalte, zeile = w.emulator.term_cursor(0)
    return w.emulator.term_text(0).split("\n")[zeile].rstrip()


def _bis_stern(w, budget, schritt=500_000):
    """Bis der Cursor hinter einem ``*`` am Zeilenende steht (Prompt des U8000-Monitors)."""
    gelaufen = 0
    while gelaufen < budget:
        gelaufen += w.emulator.run(schritt)
        if _cursorzeile(w).endswith("*"):
            return True
    return False


def _bis_press_nmi(w):
    """Einschalten → „Press RETURN“ → ``x`` + RETURN → „Press NMI“ (Bedienung des Anwenders)."""
    assert _bis(w, f"U880-Softwaremonitor Version {w._fassung} - Press RETURN", 200_000_000), w.emulator.term_text(0)
    w.emulator.run(20_000_000)                # MON8 liest erst spaeter (Merkposten 26b)
    w.emulator.term_send(0, "x\r")
    assert _bis(w, f"U8000-Softwaremonitor Version {w._fassung} - Press NMI", 40_000_000), w.emulator.term_text(0)
    w.emulator.run(2_000_000)                 # Meldung ganz ueber die Kopplung (Merkposten 15)


def _nmi_bis_maxseg(w):
    assert w._emu_started and w.act_power.isChecked()
    w._on_nmi()                               # der NMI-Taster des Fensters
    assert _bis(w, f"P8000 Hardwaretest U8001 - Version {w._fassung}", 40_000_000), w.emulator.term_text(0)
    assert _bis(w, "MAXSEG=<0F>", 400_000_000), w.emulator.term_text(0)


def test_die_vorgabe_ist_das_vollgeraet_mit_wdc_und_originalterminal(qapp, umgebung):
    w = _fenster(qapp)
    try:
        assert w._model == "p8000-ot"
        kern = w.profil.kern_parameter(w._model, w._hardware)["p8000"]
        assert kern["karte16"] == "1" and kern["wdc"] == "4.2" and kern["terminal"] == "original"
        assert kern["dram"] == "4x256K" and kern["plattepar"] == "aus"
        assert w.emulator.term_kind(0) == 1 and w.platten_widget.pfad() == ""
    finally:
        _zu(w, qapp)


def test_ohne_platte_kommt_der_u8000_monitor_zum_stern(qapp, fassung, umgebung):
    """WDC ohne Laufwerk: der Test meldet ERROR 52/53 (C1/27), danach MAXSEG und ``*``.  Der WDC wartet
    bis 32 s auf Laufwerk 0 (wdc_firmware.md §6) — der Hardwaretest steht entsprechend lange."""
    w = _fenster(qapp, fassung)
    try:
        _bis_press_nmi(w)
        _nmi_bis_maxseg(w)
        assert _bis_stern(w, 20_000_000), w.emulator.term_text(0)
        assert "*** ERROR 52" in w.emulator.term_text(0)
        w.emulator.term_send(0, "O U\r")      # der Prompt nimmt Befehle an
        assert _bis(w, "BOOTING FROM UDOS FLOPPY", 40_000_000), w.emulator.term_text(0)
    finally:
        _zu(w, qapp)


def test_unformatierte_platte_kommt_zum_stern(qapp, fassung, umgebung, tmp_path):
    """„Neue Platte…“ mit der Vorgabe (unformatiert, ohne PAR): ERROR 52 39, dann ``*``."""
    w = _fenster(qapp, fassung)
    try:
        pfad = str(tmp_path / "neu.k5504.img")
        assert w.platten_widget.neu_anlegen(pfad)
        assert "Urlader" not in w.platten_widget.hinweis.text()
        _bis_press_nmi(w)
        _nmi_bis_maxseg(w)
        assert _bis_stern(w, 20_000_000), w.emulator.term_text(0)
        # Ohne PAR antwortet der WDC auf das erste Kommando mit Fehler 39 (Status 7, 1 Byte folgt).  MON16 3.1
        # liest das Fehlerbyte und meldet „52 39“; MON16 3.0 kennt den Fehlerstatus nach dem Kommando nicht
        # (``p.disk.s`` 3.1 hat den Zweig ``ww03`` neu), liest es nie, der WDC bleibt in Status 7 stehen und
        # jedes weitere Kommando laeuft in die Frist: „52 C1“ (Merkposten p8000 Nr. 63).
        erwartet = "*** ERROR 52   39" if fassung == "3.1" else "*** ERROR 52   C1"
        assert erwartet in w.emulator.term_text(0)
    finally:
        _zu(w, qapp)


def test_platte_mit_parametersatz_ohne_urlader_bootet_statt_stern(qapp, fassung, umgebung, tmp_path):
    """Die Ursache des Berichts: eine Platte mit PAR, sonst E5 (fruehere Standardplatte bzw.
    „Formatiert mit Parametersatz“).  Der Hardwaretest besteht, MON16 startet Block 0 (AUTOBOOT,
    ``p.boot.s``) und bleibt ohne ``*`` — Gastverhalten.  Der Kasten warnt; nach Rueckstellen fuehrt
    RETURN statt NMI zum ``*`` (``PTY_INT``/``KOPPEL_INT`` setzt den Prompt bei der ersten Eingabe)."""
    from app.ui import platten_widget as pw
    w = _fenster(qapp, fassung)
    meldungen = []
    w.platten_widget.meldung.connect(meldungen.append)
    try:
        pfad = str(tmp_path / "mit_par.k5504.img")
        assert w.platten_widget.neu_anlegen(pfad, "K5504.50")
        assert pw.ohne_startblock(pfad)
        assert pw.HINWEIS_OHNE_START in w.platten_widget.hinweis.text()
        _bis_press_nmi(w)
        _nmi_bis_maxseg(w)
        assert not _bis_stern(w, 60_000_000), w.emulator.term_text(0)      # Negativkontrolle
        assert "ERROR" not in w.emulator.term_text(0)

        _zurueck_zum_u880_monitor(w)
        _bis_press_nmi(w)
        w.emulator.term_send(0, "\r")          # RETURN statt NMI
        assert _bis_stern(w, 20_000_000), w.emulator.term_text(0)
    finally:
        _zu(w, qapp)
    # Beim naechsten Start mit dieser Platte meldet der Kasten es auch in der Statuszeile.
    w = _fenster(qapp, fassung)
    try:
        assert w.platten_widget.pfad() == pfad
        assert pw.HINWEIS_OHNE_START in w.platten_widget.hinweis.text()
    finally:
        _zu(w, qapp)


@pytest.mark.parametrize("index,erwartet", [("34", False), ("11", True)])
def test_mon30_nach_der_reset_taste_haengt_vom_platinenindex_ab(index, erwartet):
    """MON 3.0 (Programmvorgabe P8000, dort Index 1) nach der RESET-Taste: Am Platinenindex 4 (Compact) behalten die PIOs
    der Kopplung ihren Stand (``PIORESET−`` nur bei PRES−/TRESET−, Entwurf 25) — MON8 3.0 meldet dann beim
    ``x`` „Hardware Error in Connection“, MON8 3.1 kommt zurecht.  Am Index 1 (PIOs an MRESET−) laeuft 3.0 durch.
    Gemessen am Kern, nicht am Geraet: ein Geraetebefund steht aus (Merkposten p8000 Nr. 63).  Faellt dieser
    Test um, hat sich die Rueckstellung der Kopplung geaendert — dann Nr. 63 und die Vorgabe pruefen."""
    from app import profil
    from app.core_binding.k1520 import K1520Emulator
    kern = profil.profil("p8000").kern_parameter("p8000-ot", {"mon8": "3.0", "mon16": "3.0", "index": index})["p8000"]
    e = K1520Emulator(machine="p8000", p8000=kern)

    def bis(text, grenze):
        vorher, gelaufen = e.term_text(0).count(text), 0
        while gelaufen < grenze:
            gelaufen += e.run(500_000)
            if e.term_text(0).count(text) > vorher:
                return True
        return False

    def x_eingeben():
        assert bis("Version 3.0 - Press RETURN", 200_000_000), e.term_text(0)
        e.run(20_000_000)
        e.term_send(0, "x\r")
        return bis("Version 3.0 - Press NMI", 40_000_000)

    assert x_eingeben()                      # nach dem Einschalten geht es immer
    e.reset()                                # RESET-Taste
    assert x_eingeben() == erwartet, e.term_text(0)
    assert ("Hardware Error in Connection" in e.term_text(0)) == (not erwartet)


def test_ohne_startblock_erkennt_nur_par_mit_leerem_block0(tmp_path):
    """Die Erkennung des Kastens: unformatiert ⇒ nein, PAR + E5 ⇒ ja, PAR + Urlader ⇒ nein,
    eine Defektspur auf Zylinder 1 Kopf 0 verschiebt Block 0 um eine Spur."""
    from app.ui import platten_widget as pw
    spur = 18 * 512
    s0 = bytearray(b"\xe5" * 512)
    leer = tmp_path / "leer.img"
    leer.write_bytes(bytes(s0) + b"\xe5" * (12 * spur))
    assert not pw.ohne_startblock(str(leer))                        # kein PAR (unformatiert)
    s0[0:6] = b"DEFEKT"
    s0[6:8] = b"\x00\x00"
    s0[8:11] = b"\xff\xff\xff"
    s0[256:262] = b"PARMTR"
    s0[277:281] = bytes([0x00, 0x04, 5, 18])
    par = tmp_path / "par.img"
    par.write_bytes(bytes(s0) + b"\xe5" * (12 * spur - 512))
    assert pw.ohne_startblock(str(par))
    mit = bytearray(par.read_bytes())
    mit[5 * spur:5 * spur + 4] = b"\x5f\x00\x01\x02"                # irgendein Urlader in Block 0
    (tmp_path / "boot.img").write_bytes(bytes(mit))
    assert not pw.ohne_startblock(str(tmp_path / "boot.img"))
    # BTT: Zylinder 1 Kopf 0 defekt ⇒ Block 0 liegt eine Spur weiter (dort E5)
    s1 = bytearray(mit[:512])
    s1[6:8] = b"\x03\x00"
    s1[8:14] = b"\x00\x01\x00\xff\xff\xff"
    mit[:512] = s1
    (tmp_path / "btt.img").write_bytes(bytes(mit))
    assert pw.ohne_startblock(str(tmp_path / "btt.img"))
    assert not pw.ohne_startblock(str(tmp_path / "gibt_es_nicht.img"))

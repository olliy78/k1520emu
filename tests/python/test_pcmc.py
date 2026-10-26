"""PCMC — Datenaustausch PC <-> MC80.30 ueber die IFSS (Issue #24).

Protokoll und Adresskopf (``app/pcmc.py``), die Bindung (``pcmc=``), und die Oberflaeche
Ende zu Ende: EPROM „mit PCMC-Erweiterung“ steckt, *PCMC-Gegenstelle* einschalten, am MC80
``PCMC`` + ``E``/``S``, Datei hoch- und wieder herunterladen.  Kernseite: ``Mc80Pcmc.*``.
"""

import time

import pytest

from conftest import requires_core


# ── Adresskopf ───────────────────────────────────────────────────────────────

def test_kopf_lesen_und_schreiben_sind_zueinander_invers():
    from app import pcmc
    daten = bytes(range(16))
    roh = pcmc.mit_kopf(0x4000, daten)
    assert roh[:4] == bytes((0x00, 0x40, 0x0F, 0x00))      # Beispiel der Anleitung: lo, hi, Laenge-1
    assert pcmc.kopf_lesen(roh) == (0x4000, 0x400F)
    assert pcmc.kopf_lesen(daten) is None                  # kein stimmiger Kopf
    assert pcmc.kopf_lesen(b"\x00\x40\xFF\x02" + bytes(0x300)) == (0x4000, 0x42FF)   # Anleitung: 00 40 FF 02
    assert pcmc.kopf_lesen(b"") is None and pcmc.kopf_lesen(b"\x00\x40\x00\x00") is None
    with pytest.raises(ValueError):
        pcmc.mit_kopf(0xFFFF, b"\x00\x00")                 # ragt ueber 64 KB hinaus


# ── Protokoll gegen einen Nachbau des MC80 ───────────────────────────────────

class _Mc80:
    """Die Gegenseite so, wie `PCMC_ROM.ASM` sie spricht (nur der Draht, ohne Emulator)."""

    def __init__(self, speicher=None, fehler_bei=None):
        self.ram = dict(speicher or {})
        self.ein = bytearray()          # was der PC schickte
        self.aus = bytearray()          # was der MC80 liefert
        self.modus = None
        self.fehler_bei = fehler_bei

    def empfangen_starten(self):
        self.modus = "E"
        self.kopf = []

    def senden_starten(self):
        self.modus = "S"
        self.kopf = []
        self.cursor = None

    def vom_pc(self, daten: bytes):
        for b in daten:
            if self.modus == "E":
                if len(self.kopf) < 4:
                    self.kopf.append(b)
                    self.aus.append(b)
                    if len(self.kopf) == 4:
                        self.adr = self.kopf[0] | self.kopf[1] << 8
                else:
                    self.ram[self.adr] = b
                    self.adr += 1
                    self.aus.append(b ^ 1 if self.fehler_bei == self.adr else b)
            else:
                if len(self.kopf) < 4:
                    self.kopf.append(b)
                    if len(self.kopf) == 4:
                        self.adr = self.kopf[0] | self.kopf[1] << 8
                        self.aus.append(self.ram.get(self.adr, 0))
                else:
                    assert b == self.ram.get(self.adr, 0), "Bestaetigung falsch"
                    self.adr += 1
                    if self.adr <= (self.kopf[2] | self.kopf[3] << 8):
                        self.aus.append(self.ram.get(self.adr, 0))


def _draht(mc):
    def ein(frist):
        from app.pcmc import PcmcFehler
        if not mc.aus:
            raise PcmcFehler("Keine Antwort vom MC80")
        return mc.aus.pop(0)
    return ein, mc.vom_pc


def test_senden_schickt_kopf_und_daten_mit_echo():
    from app import pcmc
    mc = _Mc80()
    mc.empfangen_starten()
    ein, aus = _draht(mc)
    fort = []
    pcmc.senden(ein, aus, 0x8000, b"\x0D\xFF\x00HALLO", fortschritt=lambda n, g: fort.append((n, g)))
    assert [mc.ram[0x8000 + i] for i in range(8)] == list(b"\x0D\xFF\x00HALLO")
    assert mc.kopf == [0x00, 0x80, 0x07, 0x00]
    assert fort[-1] == (8, 8)


def test_senden_bricht_bei_falschem_echo_ab():
    from app import pcmc
    mc = _Mc80(fehler_bei=0x8003)
    mc.empfangen_starten()
    ein, aus = _draht(mc)
    with pytest.raises(pcmc.PcmcFehler, match="Übertragungsfehler"):
        pcmc.senden(ein, aus, 0x8000, bytes(range(8)))


def test_holen_liest_den_bereich_und_bestaetigt_jedes_byte():
    from app import pcmc
    mc = _Mc80({0x9000 + i: 0x30 + i for i in range(16)})
    mc.senden_starten()
    ein, aus = _draht(mc)
    assert pcmc.holen(ein, aus, 0x9000, 0x900F) == bytes(0x30 + i for i in range(16))
    assert mc.kopf == [0x00, 0x90, 0x0F, 0x90]


def test_ungueltige_bereiche_werden_abgewiesen():
    from app import pcmc
    with pytest.raises(pcmc.PcmcFehler):
        pcmc.holen(lambda f: 0, lambda b: None, 0x9000, 0x8000)
    with pytest.raises(pcmc.PcmcFehler):
        pcmc.senden(lambda f: 0, lambda b: None, 0xFFF0, bytes(32))


# ── Bindung ──────────────────────────────────────────────────────────────────

@requires_core
def test_bindung_steckt_das_pcmc_eprom_nur_auf_wunsch():
    from app.core_binding.k1520 import K1520Emulator
    ohne = K1520Emulator(machine="mc80")
    mit = K1520Emulator(machine="mc80", pcmc=True)
    assert ohne.mem_read(0x2800) == 0x00                   # leere Fassung
    assert mit.mem_read(0x2800) == 0x01 and bytes(mit.mem_read(0x2803 + i) for i in range(4)) == b"PCMC"
    assert mit.mem_read(0x2400) == ohne.mem_read(0x2400)   # 2400H unveraendert (Kettung schon drin)


# ── Oberflaeche, Ende zu Ende ────────────────────────────────────────────────

def _warten(w, qapp, emu, bis, takte=40_000_000):
    """Maschine fahren und Qt bedienen, bis ``bis()`` stimmt (der Arbeitsfaden redet per Socket)."""
    n = 0
    while n < takte and not bis():
        n += emu.run(20_000)
        qapp.processEvents()
        time.sleep(0.0005)
    return bis()


@requires_core
def test_ende_zu_ende_hoch_und_herunterladen(qapp, tmp_path, monkeypatch):
    from test_mc80emu_gui import (_fenster, _init, _tippen_bildschirmtastatur, _zu,
                                  _bis_text, _laufe)
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "xdg"))
    from app import pcmc
    from app.core_binding.k1520 import SER_AUS
    w = _fenster(qapp)
    try:
        assert w.act_pcmc.shortcut().isEmpty() and w.act_pcmc.isCheckable()
        w._on_hardware_selected("pcmc", "1")
        qapp.processEvents()
        assert w.emulator.mem_read(0x2800) == 0x01
        kasten = w.pcmc_widget
        assert not w.pcmc_dock.isVisible()
        w.act_pcmc.setChecked(True)
        qapp.processEvents()
        assert kasten.an and w.pcmc_dock.isVisible()
        ifss = kasten._ifss()
        assert w.emulator.serial_status(ifss).zustand != SER_AUS

        emu = _init(w, qapp)
        _tippen_bildschirmtastatur(w, qapp, "pcmc\n")
        assert _bis_text(emu, "HAUPTMENUE", 6_000_000), emu.screen_text(25)

        # ── hoch: Datei ohne Kopf → Felder leer, Senden gesperrt ─────────────
        daten = bytes([0x0D, 0xFF, 0x00, 0x7F]) + bytes(range(0xA0, 0xB4))
        roh = tmp_path / "ohne_kopf.bin"
        roh.write_bytes(daten)
        assert kasten.datei_laden(str(roh))
        assert kasten.s_start.text() == "" and kasten.s_ende.text() == ""
        assert not kasten.senden_knopf.isEnabled()
        kasten.s_start.setText("8000")                      # Ende folgt aus der Dateilaenge
        assert kasten.s_ende.text() == f"{0x8000 + len(daten) - 1:04X}"
        assert kasten.senden_knopf.isEnabled()

        _tippen_bildschirmtastatur(w, qapp, "e")
        _laufe(emu, 3_000_000)
        kasten.senden()
        assert _warten(w, qapp, emu, lambda: kasten._faden is None), "Übertragung endet nicht"
        assert bytes(emu.mem_read(0x8000 + i) for i in range(len(daten))) == daten
        assert "gesendet" in w.statusBar().currentMessage()

        # ── Datei MIT Kopf: Adressen kommen aus der Datei und sind aenderbar ──
        mit = tmp_path / "mit_kopf.bin"
        mit.write_bytes(pcmc.mit_kopf(0x6000, daten))
        assert kasten.datei_laden(str(mit))
        assert (kasten.s_start.text(), kasten.s_ende.text()) == ("6000", f"{0x6000 + len(daten) - 1:04X}")

        # ── herunter: Bereich holen, Datei bekommt den Kopf ─────────────────
        _laufe(emu, 3_000_000)
        _tippen_bildschirmtastatur(w, qapp, "s")             # beliebige Taste -> Menue, dann S
        _laufe(emu, 3_000_000)
        _tippen_bildschirmtastatur(w, qapp, "s")
        _laufe(emu, 3_000_000)
        assert "Empfang auf PC" in emu.screen_text(25), emu.screen_text(25)
        kasten.h_start.setText("8000")
        kasten.h_ende.setText(f"{0x8000 + len(daten) - 1:04X}")
        ziel = tmp_path / "geholt.bin"
        kasten.holen(str(ziel))
        assert _warten(w, qapp, emu, lambda: kasten._faden is None)
        assert ziel.read_bytes() == pcmc.mit_kopf(0x8000, daten)
        assert pcmc.kopf_lesen(ziel.read_bytes()) == (0x8000, 0x8000 + len(daten) - 1)

        # ── aus: IFSS wie vorher ────────────────────────────────────────────
        w.act_pcmc.setChecked(False)
        qapp.processEvents()
        assert not kasten.an and not w.pcmc_dock.isVisible()
        assert w.emulator.serial_status(ifss).zustand == SER_AUS
    finally:
        _zu(w, qapp)


@requires_core
def test_ohne_ifss_keine_gegenstelle_am_mc80_2x(qapp, tmp_path, monkeypatch):
    from test_mc80emu_gui import _fenster, _zu
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "xdg"))
    w = _fenster(qapp)
    try:
        assert w.act_pcmc.isEnabled()
        w._on_model_selected("mc80-21")
        qapp.processEvents()
        assert not w.act_pcmc.isEnabled() and not w.pcmc_widget.an
    finally:
        _zu(w, qapp)

"""SOUND-Karte K1520 + Tonausgabe ueber C-ABI und Python-Bindung (Entwurf 51, AP S5)."""

import os
import wave

import pytest

from conftest import requires_core

pytestmark = requires_core


@pytest.fixture(autouse=True)
def _stumm(monkeypatch):
    monkeypatch.setenv("K1520_AUDIO", "stumm")


def _emu(**kw):
    from app.core_binding.k1520 import K1520Emulator
    return K1520Emulator(**kw)


def test_ohne_karte():
    emu = _emu(machine="a5120")
    assert not emu.sound_present()
    assert emu.sound_regs() is None and emu.sound_latch() is None
    assert emu.audio_status() == "aus"
    assert emu.audio_start() is False
    assert "SOUND" in emu.audio_fehler()
    assert emu.audio_wav_start("/nicht/da.wav") is False


@pytest.mark.parametrize("maschine", ["a5120", "k8915", "prg710", "pc1715"])
def test_stecken_und_register(maschine):
    emu = _emu(machine=maschine, sound={})
    assert emu.sound_present()
    assert emu.nenntakt_hz() > 1_000_000
    assert emu.sound_regs() == [0] * 16
    assert emu.audio_status() == "aus"
    assert emu.sound_reg_write(7, 0x3E) and emu.sound_reg_write(8, 15)
    r = emu.sound_regs()
    assert r[7] == 0x3E and r[8] == 15
    assert emu.sound_latch() == 8


def test_optionen_und_fehler():
    emu = _emu(machine="a5120", sound={"basis": 0x40, "chip": "ay8912", "takt": "system", "kanal": "mono",
                                       "tiefpass": True, "kaskade": 3})
    assert emu.sound_present()
    with pytest.raises(ValueError):
        _emu(machine="a5120", sound={"basis": 0x3C})
    with pytest.raises(ValueError):
        _emu(machine="a5120", sound={"chip": "gibtsnicht"})
    with pytest.raises(ValueError):
        _emu(machine="p8000", sound={})
    # Tor belegt: die RAF liegt auf 88H/89H
    with pytest.raises(ValueError):
        _emu(machine="a5120", raf="raf512", sound={"basis": 0x88})


def test_doppelt_stecken():
    from app.core_binding import k1520
    emu = _emu(machine="a5120", sound={})
    assert k1520._lib.k1520_sound_install(emu._handle, 0x40, 0, 1, 3579545, 0, 0, 0) == 0
    assert b"bereits" in k1520._lib.k1520_sound_fehler(emu._handle)


def test_wav_mitschrift_frequenz(tmp_path):
    emu = _emu(machine="a5120", sound={})
    wav = tmp_path / "ton.wav"
    assert emu.audio_wav_start(wav), emu.audio_fehler()
    # Ton A, Periode 111 bei Q1/2 = 1789772 Hz -> 1007,8 Hz
    for reg, wert in ((0, 111), (1, 0), (7, 0x3E), (8, 15)):
        assert emu.sound_reg_write(reg, wert)
    takt = emu.nenntakt_hz()
    gefahren = 0
    while gefahren < takt + takt // 5:
        gefahren += emu.run(100_000)
    emu.audio_wav_stop()
    with wave.open(str(wav), "rb") as w:
        assert w.getframerate() == 48000 and w.getnchannels() == 2 and w.getsampwidth() == 2
        roh = w.readframes(w.getnframes())
    import struct
    n = len(roh) // 4
    assert n > 40000
    links = struct.unpack("<" + "hh" * n, roh)[0::2][9600:]   # erste 0,2 s: Gleichanteilfilter schwingt ein
    mittel = sum(links) / len(links)
    nd = [i for i in range(1, len(links)) if links[i - 1] < mittel <= links[i]]
    assert len(nd) > 100, "kein Ton in der WAV"
    hz = (len(nd) - 1) * 48000.0 / (nd[-1] - nd[0])
    assert hz == pytest.approx(1007.8, rel=0.005)


def test_audio_status_nach_start_stumm():
    emu = _emu(machine="a5120", sound={})
    assert emu.audio_start(), emu.audio_fehler()
    assert emu.audio_status() == "stumm"      # K1520_AUDIO=stumm -> Stumm-Backend
    emu.audio_lautstaerke(0.5)
    emu.audio_stumm(True)
    emu.audio_echtzeit(False)
    emu.audio_stop()
    assert emu.audio_status() == "aus"


def _wav_hz(pfad):
    import struct
    with wave.open(str(pfad), "rb") as w:
        roh = w.readframes(w.getnframes())
    n = len(roh) // 4
    links = struct.unpack("<" + "hh" * n, roh)[0::2][9600:]
    mittel = sum(links) / len(links)
    nd = [i for i in range(1, len(links)) if links[i - 1] < mittel <= links[i]]
    assert len(nd) > 100, "kein Ton in der WAV"
    return (len(nd) - 1) * 48000.0 / (nd[-1] - nd[0])


def _fahre(emu, sekunden):
    takt = int(emu.nenntakt_hz() * sekunden)
    gefahren = 0
    while gefahren < takt:
        gefahren += emu.run(100_000)


def test_sound_umstellen_aendert_den_takt_ohne_neue_maschine(tmp_path):
    emu = _emu(machine="a5120", sound={})          # Quarz/2
    for reg, wert in ((0, 111), (1, 0), (7, 0x3E), (8, 15)):
        assert emu.sound_reg_write(reg, wert)
    handle = emu._handle
    w1, w2 = tmp_path / "a.wav", tmp_path / "b.wav"
    assert emu.audio_wav_start(w1), emu.audio_fehler()
    _fahre(emu, 1.2)
    emu.audio_wav_stop()
    emu.sound_umstellen(ay_takt_hz=3_579_545)    # Lauf geht weiter, Register bleiben
    assert emu._handle == handle and emu.sound_regs()[8] == 15
    assert emu.audio_wav_start(w2), emu.audio_fehler()
    _fahre(emu, 1.2)
    emu.audio_wav_stop()
    assert _wav_hz(w1) == pytest.approx(1007.8, rel=0.005)
    assert _wav_hz(w2) == pytest.approx(2015.6, rel=0.005)
    emu.sound_umstellen(kanal="mono", tiefpass=True)   # nicht genannte Schluessel (takt) bleiben
    assert emu._sound_cfg["takt"] == "quarz" and emu._sound_cfg["quarz_hz"] == 3_579_545


def test_sound_umstellen_fehler():
    emu = _emu(machine="a5120", sound={})
    with pytest.raises(ValueError):
        emu.sound_umstellen(basis=0x40)
    with pytest.raises(ValueError):
        emu.sound_umstellen(takt="gibtsnicht")
    with pytest.raises(ValueError):
        emu.sound_umstellen(quarz_hz=0)
    ohne = _emu(machine="a5120")
    with pytest.raises(ValueError, match="SOUND"):
        ohne.sound_umstellen(takt="quarz")


def test_audio_tempo_haelt_die_tonhoehe(tmp_path):
    emu = _emu(machine="a5120", sound={})
    for reg, wert in ((0, 111), (1, 0), (7, 0x3E), (8, 15)):
        assert emu.sound_reg_write(reg, wert)
    emu.audio_tempo(2.0)
    wav = tmp_path / "t2.wav"
    assert emu.audio_wav_start(wav), emu.audio_fehler()
    _fahre(emu, 2.4)            # 1,2 s Wirtszeit bei Faktor 2
    emu.audio_wav_stop()
    assert _wav_hz(wav) == pytest.approx(1007.8, rel=0.005)
    with wave.open(str(wav), "rb") as w:
        assert w.getnframes() == pytest.approx(48000 * 1.2, rel=0.03)   # 48000 je WIRTSsekunde
    emu.audio_tempo(0.0)         # unbegrenzt: Maschinenzeit, kein Absturz
    _fahre(emu, 0.1)

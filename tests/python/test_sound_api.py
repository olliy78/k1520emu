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

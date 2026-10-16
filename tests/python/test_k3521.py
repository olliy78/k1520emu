"""K3521.30 (akkugepuffertes 4-KB-RAM) ueber C-ABI und Bindung — Entwurf 33, Paket N10b.

Die Karte selbst pruefen die C++-Waechter (`K3521.*`, `K3521Maschine.*`); hier geht es um den
WEG: Konfigurationstext <-> ``K1520Emulator(konfig={...})``, die Eigenschaften und Fehlerfaelle.
"""

import pytest

from conftest import requires_core

pytestmark = requires_core


def _emu(**kw):
    from app.core_binding.k1520 import K1520Emulator
    return K1520Emulator(**kw)


def test_ohne_option_gibt_es_keine_karte():
    e = _emu(konfig={})
    assert e.k3521_basis == -1
    assert e.k3521_sue is None
    assert e.k3521_peek(0) == 0xFF
    assert not e.k3521_akku_leer(True)
    assert not e.k3521_flush()
    with pytest.raises(ValueError, match="K3521"):
        e.k3521_datei("/tmp/gibt_es_nicht_k3521.bin")


@pytest.mark.parametrize("adr", ["0000", "4000", "f000"])
def test_basis_im_raster(adr):
    assert _emu(konfig={"k3521": adr}).k3521_basis == int(adr, 16)


def test_aus_ist_keine_karte():
    assert _emu(konfig={"k3521": "aus"}).k3521_basis == -1


@pytest.mark.parametrize("wert", ["0800", "4001", "10000", "xyz"])
def test_basis_ausserhalb_des_rasters_wird_abgelehnt(wert):
    from app.core_binding import k1520
    with pytest.raises(Exception):   # ValueError aus der Bindung bzw. RuntimeError aus dem Kern
        k1520.K1520Emulator(konfig={"k3521": wert})
    # Der Kern selbst meldet den Grund.
    h = k1520._lib.k1520_create_a5120_text(f"k3521={wert}".encode(), None, None, None, None)
    assert not h
    assert b"k3521" in k1520._lib.k1520_last_init_error()


def test_memdi_ohne_karte_ist_ein_fehler():
    from app.core_binding import k1520
    h = k1520._lib.k1520_create_a5120_text(b"k3521_memdi=memdi1", None, None, None, None)
    assert not h
    assert b"k3521_memdi" in k1520._lib.k1520_last_init_error()


def test_sue_ist_ein_zustand_und_der_inhalt_bleibt():
    e = _emu(konfig={"k3521": "4000", "k3521_memdi": "memdi1"})
    assert e.k3521_sue is False
    assert e.k3521_akku_leer(True)
    assert e.k3521_sue is True
    e.reset()
    assert e.k3521_sue is False


def test_datei_rundlauf_ueber_zwei_maschinen(tmp_path):
    pfad = tmp_path / "k3521.bin"
    e = _emu(konfig={"k3521": "4000"})
    e.k3521_datei(pfad)
    e.mem_write(0x4123, 0xA7)
    assert e.k3521_peek(0x123) == 0xA7
    assert e.k3521_flush()
    assert pfad.read_bytes()[0x123] == 0xA7
    del e
    e2 = _emu(konfig={"k3521": "8000"})
    e2.k3521_datei(pfad)
    assert e2.mem_read(0x8123) == 0xA7


def test_falsche_dateigroesse(tmp_path):
    pfad = tmp_path / "k.bin"
    pfad.write_bytes(b"\x00" * 100)
    e = _emu(konfig={"k3521": "4000"})
    with pytest.raises(ValueError, match="4096"):
        e.k3521_datei(pfad)

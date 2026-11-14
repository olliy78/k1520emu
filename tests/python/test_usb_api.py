"""GUN-USB (Z80-PIO + VDIP1, Wirtsverzeichnis als Stick) ueber C-ABI und Python-Bindung (Entwurf 50, AP U2)."""

import pytest

from conftest import requires_core

pytestmark = requires_core

# PRG 710/-1 und K8915 Gen 2 haben 84H..87H bereits belegt (K2521-CTC/PIO) -> dort nur mit freier Basis;
# K8915 V3 belegt nur 80H..83H
MASCHINEN_84 = ["a5120", "pc1715", "k8915"]
MASCHINEN_BELEGT = ["k8915-g2", "prg710", "prg710-1"]


def _emu(**kw):
    from app.core_binding.k1520 import K1520Emulator
    return K1520Emulator(**kw)


@pytest.mark.parametrize("maschine", MASCHINEN_84)
def test_stecken_und_status(maschine, tmp_path):
    emu = _emu(machine=maschine, usb={"ordner": str(tmp_path)})
    assert emu.usb_installed()
    st = emu.usb_status()
    assert st["basis"] == 0x84 and st["stick_vorhanden"] is True
    assert (st["gelesen"], st["geschrieben"], st["resets"]) == (0, 0, 0)


@pytest.mark.parametrize("maschine", MASCHINEN_BELEGT)
def test_zre_belegt_84h_aber_freie_basis_geht(maschine):
    with pytest.raises(ValueError, match="belegt"):
        _emu(machine=maschine, usb=True)
    emu = _emu(machine=maschine, usb={"basis": 0xDC})
    assert emu.usb_status()["basis"] == 0xDC


def test_ohne_karte_nichts():
    emu = _emu(machine="a5120")
    assert not emu.usb_installed()
    assert emu.usb_status() is None
    assert emu.usb_stick_remove() is False
    assert "keine GUN-USB" in emu.usb_error()


def test_stick_stecken_und_ziehen(tmp_path):
    emu = _emu(machine="a5120", usb=True)
    assert emu.usb_status()["stick_vorhanden"] is False
    assert emu.usb_stick_insert(tmp_path)
    assert emu.usb_status()["stick_vorhanden"] is True
    assert emu.usb_stick_remove()
    assert emu.usb_status()["stick_vorhanden"] is False
    assert emu.usb_stick_insert(tmp_path / "gibt_es_nicht") is False
    assert emu.usb_error()


@pytest.mark.parametrize("usb,muster", [
    ({"basis": 0x85}, "Basis"),                    # nicht auf 4 ausgerichtet
    ({"ordner": "/nicht/da"}, "."),                # Stickordner nicht oeffenbar
    ({"basis": 0x88}, "belegt"),                   # RAF auf 88H/89H
])
def test_fehler_sind_valueerror_mit_grund(usb, muster):
    with pytest.raises(ValueError, match=muster):
        _emu(machine="a5120", raf="raf512", usb=usb)


def test_zu_spaet_gesteckt_scheitert():
    from app.core_binding.k1520 import _lib
    emu = _emu(machine="a5120")
    emu.power_on()
    emu.run(100)
    assert not _lib.k1520_usb_install(emu._handle, 0x84, b"", False)
    assert b"vor dem ersten" in _lib.k1520_usb_error(emu._handle)


def test_doppelt_gesteckt_scheitert():
    from app.core_binding.k1520 import _lib
    emu = _emu(machine="a5120", usb=True)
    assert not _lib.k1520_usb_install(emu._handle, 0x90, b"", False)
    assert b"bereits" in _lib.k1520_usb_error(emu._handle)


def test_p8000_hat_keinen_steckplatz():
    with pytest.raises(ValueError, match="Steckplatz"):
        _emu(machine="p8000", usb=True)

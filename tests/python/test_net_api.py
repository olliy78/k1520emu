"""KCNet-Netzkarte (Z80-PIO + ATmega-Modell + W5100) ueber C-ABI und Python-Bindung (Entwurf 50, AP N2)."""

import pytest

from conftest import requires_core

pytestmark = requires_core

# 88H..8BH sind an diesen Maschinen frei (A5120 ohne RAF; PRG 710 und K8915 belegen 80H..87H, PC 1715 anderes)
MASCHINEN = ["a5120", "pc1715", "k8915", "k8915-g2", "prg710", "prg710-1"]


def _emu(**kw):
    from app.core_binding.k1520 import K1520Emulator
    return K1520Emulator(**kw)


@pytest.mark.parametrize("maschine", MASCHINEN)
def test_stecken_und_status(maschine):
    emu = _emu(machine=maschine, net=True)
    assert emu.net_installed()
    st = emu.net_status()
    assert st["basis"] == 0x88
    assert (st["kommandos"], st["fehlerkommandos"], st["bytes_zur_karte"], st["bytes_zum_gast"]) == (0, 0, 0, 0)


def test_ohne_karte_nichts():
    emu = _emu(machine="a5120")
    assert not emu.net_installed()
    assert emu.net_status() is None
    assert emu.net_set_link(True) is False
    assert emu.net_w5100_read(0) == -1
    assert "keine KCNet" in emu.net_error()


def test_w5100_vorgaben_und_mac():
    emu = _emu(machine="a5120", net={"basis": 0xC0})
    assert emu.net_status()["basis"] == 0xC0
    assert emu.net_w5100_read(0x0017) == 0x07 and emu.net_w5100_read(0x0018) == 0xD0     # RTR 07D0H
    assert [emu.net_w5100_read(0x0009 + i) for i in range(6)] == [0x02, 0x4B, 0x43, 0x4E, 0x45, 0x54]
    assert emu.net_w5100_read(0x8017) == 0x07, "Bit 15 wird maskiert"


def test_link_zur_laufzeit():
    emu = _emu(machine="a5120", net={"link": False, "dns": "9.8.7.6"})
    assert emu.net_installed()
    assert emu.net_set_link(True)


@pytest.mark.parametrize("net,muster", [
    ({"basis": 0x89}, "Basis"),               # nicht auf 4 ausgerichtet
    ({"dns": "1.2.3"}, "DNS"),                # keine Adresse
    ({"dns": "1.2.3.400"}, "DNS"),
])
def test_fehler_sind_valueerror_mit_grund(net, muster):
    with pytest.raises(ValueError, match=muster):
        _emu(machine="a5120", net=net)


def test_raf_belegt_88h():
    with pytest.raises(ValueError, match="belegt"):
        _emu(machine="a5120", raf="raf512", net=True)
    assert _emu(machine="a5120", raf="raf512", net={"basis": 0xC0}).net_installed()


def test_zu_spaet_und_doppelt_gesteckt_scheitert():
    from app.core_binding.k1520 import _lib
    emu = _emu(machine="a5120", net=True)
    assert not _lib.k1520_net_install(emu._handle, 0xC0, False, True, None)
    assert b"bereits" in _lib.k1520_net_error(emu._handle)
    spaet = _emu(machine="a5120")
    spaet.power_on()
    spaet.run(100)
    assert not _lib.k1520_net_install(spaet._handle, 0xC0, False, True, None)
    assert b"vor dem ersten" in _lib.k1520_net_error(spaet._handle)

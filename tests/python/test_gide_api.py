"""GIDE (IDE-Karte + RTC 72421) ueber C-ABI und Python-Bindung (Entwurf 50, AP G4)."""

import pytest

from conftest import requires_core

pytestmark = requires_core

MASCHINEN = ["a5120", "k8915", "prg710", "prg710-1", "pc1715"]


def _emu(**kw):
    from app.core_binding.k1520 import K1520Emulator
    return K1520Emulator(**kw)


@pytest.mark.parametrize("maschine", MASCHINEN)
def test_stecken_und_status(maschine):
    emu = _emu(machine=maschine, gide=True)
    assert emu.gide_installed()
    st = emu.gide_status()
    assert st["basis"] == 0x90 and st["lh"] == 0 and st["abbild_vorhanden"] is False
    assert st["befehle"] == 0 and st["letzter_befehl"] == 0
    assert emu.gide_rtc_zeit() > 1_700_000_000   # Wirtszeit beim Erststart


def test_ohne_karte_nichts():
    emu = _emu(machine="a5120")
    assert not emu.gide_installed()
    assert emu.gide_status() is None
    assert emu.gide_rtc_zeit() is None
    assert emu.gide_image_eject() is False
    assert "keine GIDE" in emu.gide_error()


def test_abweichende_basis_und_ohne_rtc():
    emu = _emu(machine="a5120", gide={"basis": 0xB0, "rtc": False})
    assert emu.gide_status()["basis"] == 0xB0
    assert emu.gide_rtc_zeit() is None


def test_abbild_anlegen_einlegen_auswerfen(tmp_path):
    img = tmp_path / "platte.img"
    emu = _emu(machine="a5120", gide=True)
    assert emu.gide_image_create(img, 1)
    assert img.stat().st_size == 1024 * 1024
    assert emu.gide_image_create(img, 1) is False      # vorhandene Datei bleibt unberuehrt
    assert emu.gide_image_mount(img)
    st = emu.gide_status()
    assert st["abbild_vorhanden"] and st["sektoren"] == 2048
    assert emu.gide_image_path() == str(img)
    assert emu.gide_image_eject()
    assert emu.gide_image_path() == ""
    assert emu.gide_status()["abbild_vorhanden"] is False


def test_abbild_beim_anlegen_der_maschine(tmp_path):
    img = tmp_path / "p.img"
    emu = _emu(machine="k8915", gide=True)
    assert emu.gide_image_create(img, 2)
    del emu
    emu2 = _emu(machine="k8915", gide={"abbild": str(img)})
    assert emu2.gide_status()["sektoren"] == 4096


@pytest.mark.parametrize("gide,muster", [
    ({"basis": 0xA0}, "belegt"),            # K8915: A8H gehoert der ZRE
    ({"basis": 0x91}, "Basis"),             # Tiefnibble
    ({"abbild": "/nicht/da/p.img"}, "."),   # Abbild nicht oeffenbar
])
def test_fehler_sind_valueerror_mit_grund(gide, muster):
    with pytest.raises(ValueError, match=muster):
        _emu(machine="k8915", gide=gide)


def test_zu_spaet_gesteckt_scheitert():
    from app.core_binding.k1520 import _lib
    emu = _emu(machine="a5120")
    emu.power_on()
    emu.run(100)
    assert not _lib.k1520_gide_install(emu._handle, 0x90, True, b"", b"")
    assert b"vor dem ersten" in _lib.k1520_gide_error(emu._handle)


def test_p8000_hat_keinen_steckplatz():
    with pytest.raises(ValueError, match="Steckplatz"):
        _emu(machine="p8000", gide=True)


def test_rtc_versatz_stellen_und_netz_aus(tmp_path):
    import time
    from app.core_binding.k1520 import K1520Emulator
    batt = tmp_path / "gide_rtc.dat"
    jahr = time.gmtime().tm_year
    for versatz in (0, -28, -40):
        emu = _emu(machine="a5120", gide={"rtc_versatz": versatz})
        assert time.gmtime(emu.gide_rtc_zeit()).tm_year in (jahr + versatz, jahr + versatz + 1)
    emu = _emu(machine="a5120", gide={"rtc_versatz": -28, "rtc_zustand": str(batt)})
    assert emu.gide_rtc_stellen(1_800_000_000)               # Jahr des Wirts 2027
    assert time.gmtime(emu.gide_rtc_zeit()).tm_year == 1999
    assert emu.gide_rtc_netz_aus()
    # Startzeit ohne Maschine: Stand + Abwesenheit (nie hinter die Wirtszeit), gastsichtig
    t = K1520Emulator.gide_rtc_startzeit(str(batt), -28, True)
    assert time.gmtime(t).tm_year == 1999 and t >= 916_387_200     # Stand 1999-xx (gestellt) + Abwesenheit
    emu2 = _emu(machine="a5120")                              # ohne Karte
    assert emu2.gide_rtc_stellen(1) is False and emu2.gide_rtc_netz_aus() is False
    emu3 = _emu(machine="a5120", gide={"rtc": False})
    assert emu3.gide_rtc_stellen(1) is False and emu3.gide_rtc_netz_aus() is False

"""AUP 062-8531 (Tastaturteil) ueber C-ABI und Bindung — Entwurf 33, Paket N10c.

Das Registermodell selbst pruefen die C++-Waechter (`AUP.*`, `AUPTabelle.*`, `AupMaschine.*`);
hier geht es um den WEG: Konfigurationstext/``aup=True`` <-> Kern, Kennungen (Tastatur 0/1,
Position), Lampen, Hupe, Schluesselschalter und die Ruhewerte ohne Karte.
"""

import pytest

from conftest import requires_core

pytestmark = requires_core


def _emu(**kw):
    from app.core_binding.k1520 import K1520Emulator
    return K1520Emulator(**kw)


def test_ohne_karte_ruhewerte():
    e = _emu()
    assert not e.aup_installed
    assert e.aup_kommando == -1
    assert e.aup_lampen == -1
    assert e.aup_hupe == 0
    assert e.aup_schluessel_stand(1) is None
    assert not e.aup_taste(0, 0x47)
    assert e.io_read(0x41) == 0xFF


@pytest.mark.parametrize("art", ["konfig", "parameter", "install"])
def test_stecken(art):
    if art == "konfig":
        e = _emu(konfig={"aup": True})
    elif art == "parameter":
        e = _emu(aup=True)
    else:
        e = _emu()
        e.aup_install()
    assert e.aup_installed
    assert e.aup_kommando == 0xFF
    assert e.aup_lampen == 0


def test_install_nach_dem_lauf_wird_abgelehnt():
    e = _emu()
    e.run(1000)
    with pytest.raises(ValueError, match="vor dem ersten Lauf"):
        e.aup_install()


def test_nur_am_a5120():
    with pytest.raises(ValueError, match="A5120"):
        _emu(machine="k8915", aup=True)


def test_aup_alpha_ohne_aup_wird_abgelehnt():
    with pytest.raises(RuntimeError, match="aup_alpha"):
        _emu(konfig={"aup_alpha": "mit"})


def test_tabelle_und_namen():
    from app.core_binding.k1520 import K1520Emulator as E
    assert E.aup_code(E.AUP_K7633, 0x47) == 0xC0
    assert E.aup_code(E.AUP_K7633, 0x47, alpha=True) == 0x41
    assert E.aup_code(E.AUP_K7633, 0x00) == 0xAA
    assert E.aup_code(E.AUP_K7633, 0x10) == -1
    assert E.aup_code(E.AUP_KUNDE, 0x40) == 0x35
    assert E.aup_code(2, 0x40) == -1
    assert E.aup_tastenname(E.AUP_K7633, 0x01) == "SS2"
    assert E.aup_tastenname(E.AUP_K7633, 0x47) == "F01"
    assert E.aup_tastenname(E.AUP_KUNDE, 0x40) == "5"
    assert E.aup_tastenname(E.AUP_KUNDE, 0x38) == ""


def test_taste_kommt_beim_gast_an():
    """Ohne Gastprogramm: der Kern liest 41H/42H wie die ZRE (io_read stempelt die Zeit)."""
    e = _emu(aup=True)
    e.power_on()
    assert e.io_read(0x42) == 0x80          # Typzeichen nach /RESET, ohne Gueltigkeitsbit
    assert e.aup_taste(e.AUP_KUNDE, 0x42, True)
    assert e.aup_taste(e.AUP_KUNDE, 0x42, False)
    assert not e.aup_taste(e.AUP_KUNDE, 0x38, True)
    assert e.io_read(0x41) == 0x00          # Zeitstempel, Entprellung laeuft
    e.run(200_000)                          # > 16 ms Maschinenzeit
    assert e.io_read(0x41) == 0x01          # Bit 0 = Kundentastatur (A1, belegt)
    assert e.io_read(0x42) == 0x31
    assert e.io_read(0x41) == 0x00


def test_schluessel_stand():
    e = _emu(aup=True)
    assert e.aup_schluessel_stand(2) is False
    assert e.aup_schluessel(2, True)
    assert e.aup_schluessel_stand(2) is True
    assert not e.aup_schluessel(3, True)
    assert e.aup_schluessel_stand(3) is None


# ─── Sonderschnittstelle (N10d) ─────────────────────────────────────────────

@pytest.mark.parametrize("art", ["konfig", "parameter", "install"])
def test_sonderschnittstelle_stecken(art):
    if art == "konfig":
        e = _emu(konfig={"aup": True, "aup_sonder": True})
    elif art == "parameter":
        e = _emu(aup_sonder=True)       # steckt die AUP mit
    else:
        e = _emu()
        e.aup_install(sonder=True)
    assert e.aup_installed
    assert e.aup_sonder
    assert e.serial_count() == 4
    info = e.serial_info(3)
    assert info.name == "Sonderschnittstelle" and info.stecker == "X2" and not info.v24
    # CTC/SIO antworten auf B0H-B7H (RR0 von Kanal B: Tx-Puffer leer).
    assert e.io_read(0xB7) & 0x04
    assert e.io_read(0x41) == 0x00           # Tastaturteil daneben unberuehrt


def test_ohne_sonderschnittstelle_drei_schnittstellen():
    e = _emu(aup=True)
    assert e.aup_installed and not e.aup_sonder
    assert e.serial_count() == 3
    for port in range(0xB0, 0xB8):
        assert e.io_read(port) == 0xFF
    assert not _emu().aup_sonder


def test_aup_sonder_ohne_aup_wird_abgelehnt():
    with pytest.raises(RuntimeError, match="aup_sonder"):
        _emu(konfig={"aup_sonder": True})


def test_sonder_install_ohne_tastaturteil_nach_dem_lauf_wird_abgelehnt():
    e = _emu()
    e.run(1000)
    with pytest.raises(ValueError, match="vor dem ersten Lauf"):
        e.aup_install(sonder=True)


def test_sonder_nur_am_a5120():
    with pytest.raises(ValueError, match="A5120"):
        _emu(machine="k8915", aup_sonder=True)


def test_sonderschnittstelle_im_hub_einstellbar():
    e = _emu(aup_sonder=True)
    assert e.serial_configure(3, loop=True)
    assert e.serial_config(3).loop
    assert e.serial_status(3) is not None


def test_schluesselschalter_jede_drehung_meldet_den_code():
    e = _emu(aup=True)
    e.power_on()
    assert e.io_read(0x42) == 0x80
    assert e.aup_schluessel(2, True)
    e.io_read(0x41)
    e.run(200_000)
    assert e.io_read(0x41) == 0x02          # Bit 1 = Terminal
    assert e.io_read(0x42) == 0xA9          # SS2 ein
    assert e.aup_schluessel(2, False)
    e.io_read(0x41)
    e.run(200_000)
    assert e.io_read(0x41) == 0x02
    assert e.io_read(0x42) == 0xA9          # SS2 aus: derselbe Code (der Treiber kippt)

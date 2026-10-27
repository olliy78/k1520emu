"""H3 — Emulator bootet: vom Abbild (Bezug) und von der echten Diskette."""

import time

import pytest

from hw_helfer import (FIXTURES, Fahrer, cpa_bis_prompt, schreibe_abbild, sitzung)

CLOCK_HFE = FIXTURES / "cpa_cpa780_k5601_clock.hfe"


def _emu():
    from app.core_binding.k1520 import K1520Emulator
    return K1520Emulator()


@pytest.mark.hw_lesen
def test_h3_1_boot_vom_abbild_als_bezug(abbild_kopie, logdir):
    emu = _emu()
    assert emu.mount_disk(0, str(abbild_kopie("cpa_cpa780_k5601_clock.hfe")), "cpa780", False)
    emu.power_on()
    assert cpa_bis_prompt(Fahrer(emu, "bild", logdir))


@pytest.mark.hw_schreiben
def test_h3_2_abbild_auf_diskette_dann_boot_physisch(freigabe, logdir):
    schreibe_abbild(CLOCK_HFE)
    with sitzung(fuer_emulator=True, read_ahead=False) as s:
        emu = _emu()
        assert emu.mount_physical(0, s.sync, True), emu.last_error()
        emu.power_on()
        f = Fahrer(emu, "physisch", logdir)
        t0 = time.monotonic()
        assert cpa_bis_prompt(f), f.bild()
        print(f"\nA> nach {time.monotonic() - t0:.0f} s")
        st = s.sync.stats
        assert st.tracks_known < st.tracks_total // 2, f"{st.tracks_known} Spuren: Vollabzug?"
        assert st.read_crc_bad == 0 and st.tracks_failed == 0 and st.errors == 0
        del emu

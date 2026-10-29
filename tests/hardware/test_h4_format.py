"""H4 — FORMAT mit Vergleichs-Lesen, Systeminstallation und Kaltstart auf der ECHTEN Diskette.

H4a: die Anwenderkette ueber die ganze Diskette (cpa780).
H4m: die Format-Matrix von format_all.py (K5601 80 DS + Geometrien S/T/U/V/W =
     einseitig / 40 Spuren im Doppelschritt, Kompatibilitaet zu K5600.10/.20), je Format
     Smoke bis Spur 2.  Orakel: dasselbe Skript gegen eine LEERE Abbilddatei; der
     Dekoder von gw muss auf der echten Diskette dieselben Sektorzahlen finden.
"""

import shutil

import pytest

from hw_helfer import (FIXTURES, Fahrer, cpa_bis_prompt, diskette_loeschen,
                       format_und_system, gw, k5601_matrix, laufwerk, sitzung,
                       spurbefund)

pytestmark = pytest.mark.hw_schreiben

BOOT = "cpa_cpa780_k5601_clock.img"


def _emu_mit_boot(tmp_path):
    from app.core_binding.k1520 import K1520Emulator
    a = tmp_path / "a.img"
    shutil.copy(FIXTURES / BOOT, a)
    emu = K1520Emulator()
    assert emu.mount_disk(0, str(a), "cpa780", False), emu.last_error()
    return emu


class TestKetteCpa780:
    """Reihenfolge ist Absicht: jede Stufe setzt die vorige voraus."""

    ok: dict = {}

    def test_h4_1_leerdiskette_formatieren_mit_verify(self, freigabe, tmp_path, logdir):
        diskette_loeschen()
        with sitzung(schreibbar=True, fuer_emulator=True, read_ahead=False) as s:
            emu = _emu_mit_boot(tmp_path)
            assert emu.mount_physical(1, s.sync, False), emu.last_error()
            f = Fahrer(emu, "h4_format", logdir)
            emu.power_on()
            assert cpa_bis_prompt(f), f.bild()
            r = format_und_system(f, "B", cpabcgen=True, sync=s.sync)
            assert r["formatiert"], (r.get("stillstand"), r["bild_format"])
            assert "SPUR DEFEKT" not in r["bild_format"]
            assert r["dir_ok"], r["bild_dir"]
            del emu
            assert s.sync.flush(300_000), s.sync.last_error
            st = s.sync.stats
            print(f"\ngeschrieben {st.writes_done}, geprueft {st.verifies_done}, "
                  f"Vergleich misslungen {st.verify_failed}, Schadstellen {st.tracks_defect}")
            assert st.tracks_defect == 0 and st.verify_failed == 0 and st.errors == 0
        TestKetteCpa780.ok["format"] = True

    def test_h4_2_vollmessung_der_scheibe(self, freigabe):
        """Das Verify des Gastes sieht die Scheibe nicht — ``gw`` schon."""
        assert TestKetteCpa780.ok.get("format"), "H4.1 nicht gruen"
        ist = spurbefund(cyls=80)
        soll = {(c, h): ((26, 26) if (c, h) in ((0, 0), (0, 1), (1, 0)) else (5, 5))
                for c in range(80) for h in (0, 1)}
        fehlt = {k: ist.get(k) for k in soll if ist.get(k) != soll[k]}
        assert not fehlt, f"{len(fehlt)} Spuren abweichend: {sorted(fehlt.items())[:20]}"
        TestKetteCpa780.ok["messung"] = True

    def test_h4_4_kaltstart_von_der_selbst_formatierten_diskette(self, freigabe, logdir):
        assert TestKetteCpa780.ok.get("messung"), "H4.2 nicht gruen"
        from app.core_binding.k1520 import K1520Emulator
        with sitzung(fuer_emulator=True, read_ahead=False) as s:
            emu = K1520Emulator()
            assert emu.mount_physical(0, s.sync, True), emu.last_error()
            emu.power_on()
            f = Fahrer(emu, "h4_kaltstart", logdir)
            assert cpa_bis_prompt(f), f.bild()
            st = s.sync.stats
            assert st.read_crc_bad == 0 and st.tracks_failed == 0
            del emu
        TestKetteCpa780.ok["boot"] = True


def _ids():
    return [z[0] for z in k5601_matrix()]


@pytest.mark.parametrize("zeile", k5601_matrix(), ids=_ids())
def test_h4m_format_matrix(freigabe, zeile, tmp_path, logdir):
    ident, switch, nav, sel = zeile
    # 1. Referenz: dasselbe Skript gegen eine leere Abbilddatei
    from app.core_binding.k1520 import K1520Emulator
    emu = _emu_mit_boot(tmp_path)
    ref = tmp_path / "ref.hfe"
    assert emu.create_disk(1, str(ref), "", False), emu.last_error()
    f = Fahrer(emu, f"{ident}_ref", logdir)
    emu.power_on()
    assert cpa_bis_prompt(f), f.bild()
    r = format_und_system(f, "B", switch_key=switch, menu_nav=nav, format_taste=sel,
                          bis_spur=2, cpabcgen=False)
    emu.run(3_000_000)
    del emu
    if not r["formatiert"]:
        pytest.skip(f"auch auf dem Abbild nicht formatierbar (siehe Matrix): {ident}")
    soll = spurbefund(ref)

    # 2. Dieselbe Folge auf der echten Diskette (Spuren 0..5 vorher leeren)
    p = laufwerk()
    assert gw("erase", "--drive", p["drive"], "--tracks", "c=0-5:h=0-1").returncode == 0
    with sitzung(schreibbar=True, fuer_emulator=True, read_ahead=False) as s:
        emu = _emu_mit_boot(tmp_path)
        assert emu.mount_physical(1, s.sync, False), emu.last_error()
        f = Fahrer(emu, f"{ident}_hw", logdir)
        emu.power_on()
        assert cpa_bis_prompt(f), f.bild()
        r = format_und_system(f, "B", switch_key=switch, menu_nav=nav, format_taste=sel,
                              bis_spur=2, cpabcgen=False, sync=s.sync)
        emu.run(3_000_000)
        del emu
        assert s.sync.flush(120_000), s.sync.last_error
        st = s.sync.stats
        assert st.tracks_defect == 0 and st.verify_failed == 0, s.defect_tracks
    assert r["formatiert"], r["bild_format"]
    ist = spurbefund()
    assert ist == soll, f"Abbild {soll}\nDiskette {ist}"

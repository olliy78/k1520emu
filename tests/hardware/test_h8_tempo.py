"""H8 — Gastsystem in verschiedenem Tempo gegen die echte Diskette.

Gast und Greaseweazle sind durch das Speicherabbild entkoppelt — aber nicht ganz:
* eine UNBEKANNTE Spur blockiert die Maschine, bis der Adapter sie gelesen hat
  (``TrackSync::ensureLoaded``; FORMAT liest vor jedem Zylinder die Kopf-1-Spur);
* geaenderte Spuren laufen auf, wenn der Gast schneller formatiert, als der Adapter
  schreiben + pruef-lesen kann (unbegrenzt: ~80 Spuren Rueckstand am Ende von FORMAT).

Gefahren werden die Tempi der Oberflaeche (``app/emulationstakt.py``): 1x, 10x,
unbegrenzt.  Orakel ist jeweils der Dekoder von ``gw`` auf der Scheibe.
"""

import shutil
import time

import pytest

from hw_helfer import (FIXTURES, Fahrer, cpa_bis_prompt, diskette_loeschen,
                       format_und_system, gw, laufwerk, sitzung, spurbefund)

pytestmark = pytest.mark.hw_schreiben

BOOT = "cpa_cpa780_k5601_clock.img"
BIS_SPUR = 19                    # 10 Zylinder: bei 1x rund eine Minute


def _emu(tmp_path):
    from app.core_binding.k1520 import K1520Emulator
    a = tmp_path / "a.img"
    shutil.copy(FIXTURES / BOOT, a)
    emu = K1520Emulator()
    assert emu.mount_disk(0, str(a), "cpa780", False), emu.last_error()
    return emu


def _soll(cyls: int) -> dict:
    """cpa780: 26 x 128 auf c0h0, c0h1, c1h0, sonst 5 x 1024."""
    return {(c, h): ((26, 26) if (c, h) in ((0, 0), (0, 1), (1, 0)) else (5, 5))
            for c in range(cyls) for h in (0, 1)}


@pytest.mark.parametrize("faktor", [1.0, 10.0, 0.0], ids=["1x", "10x", "unbegrenzt"])
def test_h8_1_format_mit_verify_in_jedem_tempo(freigabe, faktor, tmp_path, logdir):
    p = laufwerk()
    assert gw("erase", "--drive", p["drive"], "--tracks", "c=0-12:h=0-1").returncode == 0
    with sitzung(schreibbar=True, fuer_emulator=True, read_ahead=False) as s:
        emu = _emu(tmp_path)
        assert emu.mount_physical(1, s.sync, False), emu.last_error()
        f = Fahrer(emu, f"tempo_{faktor:g}", logdir, faktor=faktor)
        emu.power_on()
        assert cpa_bis_prompt(f), f.bild()
        t0 = time.monotonic()
        r = format_und_system(f, "B", bis_spur=BIS_SPUR, cpabcgen=False, sync=s.sync)
        dauer = time.monotonic() - t0
        st_ende = s.sync.stats
        del emu
        t1 = time.monotonic()
        assert s.sync.flush(300_000), s.sync.last_error
        st = s.sync.stats
        print(f"\n{faktor:g}x: FORMAT {dauer:.0f} s, Rueckstand am Ende {st_ende.tracks_dirty} "
              f"Spuren, Nachlauf {time.monotonic() - t1:.0f} s; geschrieben {st.writes_done}, "
              f"geprueft {st.verifies_done}, misslungen {st.verify_failed}, Fehler {st.errors} "
              f"{s.sync.last_error}")
        assert r["formatiert"], (r.get("stillstand"), r["bild_format"])
        assert st.errors == 0, s.sync.last_error             # u. a. keine Zeitueberschreitung
        assert st.verify_failed == 0 and st.tracks_defect == 0, s.defect_tracks
    ist = spurbefund(cyls=BIS_SPUR // 2 + 1)
    assert ist == _soll(BIS_SPUR // 2 + 1), ist


def test_h8_2_auswerfen_nach_unbegrenztem_format_verliert_nichts(freigabe, tmp_path, logdir):
    """Wie der Laufwerkskasten auswirft: ``PhysicalSession.close()`` OHNE vorheriges flush.

    Nach FORMAT im unbegrenzten Tempo stehen ~80 Spuren aus (~1 s je Spur).  Das
    Abmelden muss darauf warten — eine verworfene Spur faellt sonst niemandem auf.
    """
    from app.gw import PhysicalSession
    diskette_loeschen()
    s = PhysicalSession.start(writable=True, for_emulator=True, read_ahead=False, **laufwerk())
    try:
        emu = _emu(tmp_path)
        assert emu.mount_physical(1, s.sync, False), emu.last_error()
        f = Fahrer(emu, "auswerfen", logdir)
        emu.power_on()
        assert cpa_bis_prompt(f), f.bild()
        r = format_und_system(f, "B", cpabcgen=False, sync=s.sync)
        assert r["formatiert"], (r.get("stillstand"), r["bild_format"])
        rueckstand = s.sync.stats.tracks_dirty
        print(f"\nRueckstand beim Auswerfen: {rueckstand} Spuren")
        # Genau der Weg der Oberflaeche (DrivesWidget.on_toggle): ERST die Sitzung
        # beenden (schreibt den Rueckstand), DANN aushaengen.  Umgekehrt loest
        # unmount_disk das Medium vom TrackSync, und der Rest ist weg.
        t0 = time.monotonic()
        vollstaendig = s.close()
        print(f"Abmelden {time.monotonic() - t0:.0f} s: {s.nicht_zurueckgeschrieben or 'alles'}")
        emu.unmount_disk(1)
        del emu
    finally:
        s.close()
    assert vollstaendig, s.nicht_zurueckgeschrieben
    ist = spurbefund(cyls=80)
    fehlt = {k: v for k, v in _soll(80).items() if ist.get(k) != v}
    assert not fehlt, f"{len(fehlt)} Spuren nicht wie formatiert: {sorted(fehlt)[:20]}"

"""H2 — Schreibpfad: Spur schreiben, ganze Fixture aufspielen, Pause, Dekoder-Gegenprobe."""

import time
from pathlib import Path

import pytest

from hw_helfer import FIXTURES, laufwerk, scan_spur, schreibe_abbild, spurbefund

pytestmark = [pytest.mark.hw_schreiben]

HFE = FIXTURES / "cpa_cpa780_k5601_clock.hfe"


def test_eine_spur_schreiben_liest_sich_zurueck(freigabe):
    schreibe_abbild(HFE, spuren=[(0, 0), (1, 1)])
    assert scan_spur(0, 0) == (26, 26)
    assert scan_spur(1, 1) == (5, 5)


def test_ganze_fixture_aufspielen_dekodiert_vollstaendig(freigabe):
    """160 Spuren; ``gw``s Dekoder muss JEDEN Sektor finden (Naht, Drehzahlstreckung)."""
    dauer = schreibe_abbild(HFE)
    soll, ist = spurbefund(HFE, cyls=80), spurbefund(cyls=80)
    print(f"\n{dauer:.0f} s")
    fehlt = {k: (soll[k], ist.get(k)) for k in soll if ist.get(k) != soll[k]}
    assert soll and not fehlt, fehlt


def test_schreiben_nach_langer_pause_braucht_keinen_index_wiederholer(freigabe):
    """Issue #13: erster Befehl nach Motorpause.  Motor laeuft 3 s nach, dann 10 s Ruhe."""
    from app.gw import open_device
    from hw_helfer import sys
    sys.path.insert(0, str(HFE.parents[2] / "python"))
    from gw_fake import HfeDevice
    quelle = HfeDevice(HFE)
    p = laufwerk()
    dev = open_device(p["drive"], cell_rate_kbps=p["cell_rate_kbps"])
    try:
        z, n = quelle.read_track(10, 0)
        dev.write_track(10, 0, z, n)
        time.sleep(10)
        dev.write_track(10, 0, z, n)             # darf nicht mit "No Index" enden
    finally:
        dev.deselect()
    assert scan_spur(10, 0) == (5, 5)

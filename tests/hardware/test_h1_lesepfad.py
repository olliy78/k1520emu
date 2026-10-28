"""H1 — Lesepfad gegen die echte Scheibe.  Lesend, aendert die Diskette nicht."""

import time

import pytest

from hw_helfer import gw, laufwerk, scan_spur, sitzung

pytestmark = pytest.mark.hw_lesen


def test_dieselbe_spur_dreimal_gibt_dieselbe_laenge(formatiert):
    from app.gw import open_device
    p = laufwerk()
    dev = open_device(p["drive"], cell_rate_kbps=p["cell_rate_kbps"])
    try:
        n = [dev.read_track(3, 1)[1] for _ in range(3)]
    finally:
        dev.deselect()
    assert max(n) - min(n) < min(n) / 1000, n


def test_ganze_diskette_ueber_den_synchronisierer(formatiert):
    """Alles vorauslesen: keine unlesbare Spur, keine Sektoren mit falscher CRC."""
    with sitzung(read_ahead=True) as s:
        t0 = time.monotonic()
        s.sync.set_read_ahead(True)
        while time.monotonic() - t0 < 600:
            st = s.sync.stats
            if st.tracks_known >= st.tracks_total:
                break
            time.sleep(0.5)
        st = s.sync.stats
        dauer = time.monotonic() - t0
        print(f"\n{st.tracks_known}/{st.tracks_total} Spuren in {dauer:.0f} s, "
              f"crc_bad={st.read_crc_bad}, retries={st.read_retries}")
        assert st.tracks_known == st.tracks_total
        assert st.tracks_failed == 0 and st.errors == 0
        assert st.read_crc_bad == 0, "Sektoren mit falscher CRC auf einer heilen Diskette"


def test_unser_dekoder_und_der_von_gw_sehen_dasselbe(formatiert):
    """Gegenprobe: die Sektorzahl je Spur stimmt mit der von ``gw`` ueberein."""
    for c, h in [(0, 0), (0, 1), (1, 0), (40, 1), (79, 0)]:
        gefunden, erwartet = scan_spur(c, h)
        assert erwartet and gefunden == erwartet, f"c{c}h{h}: {gefunden}/{erwartet}"

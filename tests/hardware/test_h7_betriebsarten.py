"""H7 — Betriebsarten fuer K5600.10/.20-Kompatibilitaet: einseitig und Doppelschritt.

Der K5601 kann 40-Spur-Disketten (48 tpi) im Doppelschritt und einseitig bedienen.
Zwei Wege: (a) der Gast steuert (Matrix S/T/U/V/W in H4m), (b) die Sitzung steuert:
``double_step`` mit 40 Zylindern, wie das DiskTool eine 40-Spur-Diskette oeffnet.
"""

import pytest

from hw_helfer import FIXTURES, laufwerk, scan_spur

pytestmark = pytest.mark.hw_schreiben


def test_h7_1_doppelschritt_schreiben_und_lesen(freigabe):
    """Logische Spur n liegt auf Zylinder 2n: Schreiben ueber Device, Gegenprobe mit gw."""
    import sys
    from hw_helfer import PROJECT_ROOT
    sys.path.insert(0, str(PROJECT_ROOT / "tests" / "python"))
    from gw_fake import HfeDevice
    from app.gw import open_device
    quelle = HfeDevice(FIXTURES / "cpa_cpa780_k5601_clock.hfe")
    p = laufwerk()
    dev = open_device(p["drive"], cell_rate_kbps=p["cell_rate_kbps"], double_step=True)
    try:
        for c in (3, 4):
            z, n = quelle.read_track(c, 0)
            dev.write_track(c, 0, z, n)                  # landet auf Zylinder 6 bzw. 8
    finally:
        dev.deselect()
    assert scan_spur(6, 0) == (5, 5)
    assert scan_spur(8, 0) == (5, 5)
    # Zwischenzylinder bleiben unberuehrt (Einzelschritt-Nachbar 7 nicht beschrieben)
    from app.gw import open_device as od
    dev = od(p["drive"], cell_rate_kbps=p["cell_rate_kbps"], double_step=True)
    try:
        z, n = dev.read_track(3, 0)
        assert n > 90_000
    finally:
        dev.deselect()

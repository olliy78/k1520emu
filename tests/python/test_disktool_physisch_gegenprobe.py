"""DiskTool an einer „physischen" Diskette == DiskTool an einer Datei — CP/A, UDOS, WEGA.

Ohne Hardware: das Ersatzlaufwerk (``gw_fake.HfeDevice``) ueber einer Kopie des Abbilds,
derselbe Weg wie am Greaseweazle (``Sync`` + ``TrackWorker`` + ``open_physical``).  Die
Szenarien liegen in ``tests/hardware/disktool_szenario.py`` und laufen dort auch gegen die
echte Diskette (Entwurf 40, H9).  Orakel ist dieselbe Aenderung an einer Dateikopie:
Verzeichnis, Dateibytes, Pruefbefund und jeder Sektor samt Anhang hinter der Daten-CRC
muessen gleich sein — und geschrieben werden genau die Spuren, die sich geaendert haben.
"""

from __future__ import annotations

import shutil
import sys
from pathlib import Path

import pytest

from conftest import requires_core

pytestmark = requires_core

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "hardware"))
import disktool_szenario as dz  # noqa: E402


def _szenarien():
    import tempfile
    return dz.szenarien(Path(tempfile.gettempdir()) / "k1520_dz_abbilder")


def _sitzung(geraet, schreibbar: bool):
    """Sitzung an einem VORHANDENEN Ersatzlaufwerk — zweite Sitzung = dieselbe Scheibe."""
    from app.gw import Sync, TrackWorker
    from app.gw.session import PhysicalSession
    sync = Sync(num_cyls=geraet.num_cyls, num_heads=geraet.num_heads,
                writable=schreibbar, read_ahead=True)
    w = TrackWorker(sync, geraet, poll_ms=20)
    w.start()
    return PhysicalSession(sync, w, geraet, drive="a", writable=schreibbar, cell_rate_kbps=250)


@pytest.fixture(params=[s.name for s in _szenarien()])
def szenario(request):
    sz = next(s for s in _szenarien() if s.name == request.param)
    if not sz.abbild.exists():
        pytest.skip(f"Abbild fehlt: {sz.abbild}")
    return sz


def test_physisch_geaendert_ist_sektorgleich_mit_der_datei(szenario, tmp_path):
    from gw_fake import HfeDevice
    from app.core_binding.k1520disk import DiskTool

    vorher, soll = dz.datei_orakel(szenario, tmp_path)
    fehlt = [n for n in szenario.erwartet_da
             if not any(k.endswith(n) for k in soll["verzeichnis"])]
    assert not fehlt, f"das Szenario wirkt nicht: {fehlt}"

    hfe = tmp_path / "laufwerk.hfe"
    shutil.copy(szenario.abbild, hfe)
    geraet = HfeDevice(hfe)
    s = _sitzung(geraet, True)
    with DiskTool.open_physical(s.sync, read_only=False) as d:
        assert dz.unterschiede(vorher, dz.aufnahme(d, tmp_path / "lw_vorher")) == []
        dz.anwenden(d, szenario, tmp_path / "lw_quellen")
    assert s.close(), s.nicht_zurueckgeschrieben

    s2 = _sitzung(geraet, False)
    try:
        with DiskTool.open_physical(s2.sync) as d:
            ist = dz.aufnahme(d, tmp_path / "lw_nachher")
    finally:
        s2.close()
    assert dz.unterschiede(soll, ist) == []

    geaendert = {(c, h) for (c, h, _n) in set(vorher["sektoren"]) | set(soll["sektoren"])
                 if vorher["sektoren"].get((c, h, _n)) != soll["sektoren"].get((c, h, _n))}
    assert set(geraet.geschrieben) == geaendert, "nicht genau die geaenderten Spuren geschrieben"


def test_schreibgeschuetzt_geoeffnet_wird_nichts_geschrieben(szenario, tmp_path):
    from gw_fake import HfeDevice
    from app.core_binding.k1520disk import DiskTool, K1520DiskError

    hfe = tmp_path / "laufwerk.hfe"
    shutil.copy(szenario.abbild, hfe)
    geraet = HfeDevice(hfe)
    s = _sitzung(geraet, False)
    try:
        with DiskTool.open_physical(s.sync) as d:
            with pytest.raises(K1520DiskError):
                dz.anwenden(d, szenario, tmp_path / "q")
    finally:
        s.close()
    assert geraet.geschrieben == []


def test_eine_zu_grosse_datei_aendert_die_diskette_nicht(szenario, tmp_path):
    """Transaktion: scheitert das Einfuegen am Platz, bleibt die Scheibe unberuehrt."""
    from gw_fake import HfeDevice
    from app.core_binding.k1520disk import DiskTool, K1520DiskError

    hfe = tmp_path / "laufwerk.hfe"
    shutil.copy(szenario.abbild, hfe)
    geraet = HfeDevice(hfe)
    s = _sitzung(geraet, True)
    with DiskTool.open_physical(s.sync, read_only=False) as d:
        vorher = dz.verzeichnis(d)
        riesig = dz.zu_gross(d, szenario, tmp_path)
        with pytest.raises(K1520DiskError):
            d.insert(riesig, dz.zielname(d, szenario, "RIESIG"))
        d.flush()
        assert dz.verzeichnis(d) == vorher
    assert s.close(), s.nicht_zurueckgeschrieben
    assert geraet.geschrieben == [], "eine gescheiterte Operation hat Spuren geschrieben"

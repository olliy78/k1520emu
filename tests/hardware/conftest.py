"""Hardware-Testgruppe (Entwurf 40): Greaseweazle + K5601 + ueberschreibbare Diskette.

NICHT Teil der Regression — `tests/python/CMakeLists.txt` sammelt nur `tests/python/`.
Aufruf: ``tools/dev.sh test-hardware [--schreiben] [-k AUSDRUCK]``.

Schutzriegel (§4):
  * Schreibstufen (Marke ``hw_schreiben``) laufen nur mit ``--schreiben`` /
    ``K1520_GW_WRITE=1``; ohne sie werden sie abgewaehlt, nicht uebersprungen.
  * Vor dem ersten Schreiben wird die eingelegte Diskette gesichert
    (``out/hw_sicherung/``), ausser ``--keine-sicherung``.
  * ``--zurueckspielen`` schreibt die Sicherung am Ende zurueck.
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from hw_helfer import (OUT, PROJECT_ROOT, gw, laufwerk, schreibe_abbild)  # noqa: E402

LOGROOT = OUT / "hw_log"


def pytest_addoption(parser):
    g = parser.getgroup("hardware")
    g.addoption("--schreiben", action="store_true",
                help="beschreibt die eingelegte Diskette (Schreibstufen)")
    g.addoption("--keine-sicherung", action="store_true",
                help="eingelegte Diskette vor dem Schreiben NICHT sichern")
    g.addoption("--zurueckspielen", action="store_true",
                help="Sicherung am Ende auf die Diskette zurueckschreiben")


def pytest_configure(config):
    config.addinivalue_line("markers", "hw_lesen: lesend, ohne Freigabe")
    config.addinivalue_line("markers", "hw_schreiben: beschreibt die Diskette")


def _schreiben(config) -> bool:
    import os
    return config.getoption("--schreiben") or os.environ.get("K1520_GW_WRITE") == "1"


def pytest_collection_modifyitems(config, items):
    if _schreiben(config):
        return
    weg = [i for i in items if i.get_closest_marker("hw_schreiben")]
    if weg:
        config.hook.pytest_deselected(items=weg)
        items[:] = [i for i in items if i not in weg]


# ── Vorpruefung (H0 als Fixture): fehlt die Hardware, entfaellt die Gruppe ─────

@pytest.fixture(scope="session")
def vorpruefung():
    """Adapter da, Laufwerk dreht, Diskette liegt ein.  Sonst: ``skip`` mit Grund."""
    from app.gw import finde_adapter, open_device, verfuegbar
    if not verfuegbar():
        pytest.skip("Paket 'greaseweazle' fehlt")
    try:
        adapter = finde_adapter()
    except Exception as e:                            # noqa: BLE001
        pytest.skip(f"kein Greaseweazle: {e}")
    p = laufwerk()
    dev = open_device(p["drive"], cell_rate_kbps=p["cell_rate_kbps"])
    try:
        t0 = time.monotonic()
        try:
            zellen, n = dev.read_track(0, 0)
        except Exception as e:                        # noqa: BLE001
            pytest.skip(f"Laufwerk/Diskette antwortet nicht: {e}")
        dauer = time.monotonic() - t0
        # Drehzahl aus dem INDEX (Adaptertakte je Umdrehung), nicht aus der Zellenzahl:
        # auf einer geloeschten Spur dekodiert die PLL Rauschen, die Zellenzahl taugt
        # dort nicht (gemessen 92 500 statt 100 000).
        umdrehung_ms = dev.ticks_per_rev() / dev._usb.sample_freq * 1000
        return dict(adapter=adapter, zellen=n, umdrehung_ms=umdrehung_ms,
                    lesedauer=dauer, **p)
    finally:
        dev.deselect()


@pytest.fixture(scope="session")
def formatiert(vorpruefung):
    """Lesestufen mit Inhaltspruefung brauchen eine FORMATIERTE Diskette.

    Nach einem Schreiblauf kann sie geloescht im Laufwerk liegen; dann ist ein Vergleich
    der Sektorzahlen sinnlos und der Fall wird mit Grund uebersprungen.
    """
    from hw_helfer import spurbefund
    befund = spurbefund(cyls=80)
    leer = [k for k in [(0, 0), (0, 1), (1, 0), (3, 1), (40, 1), (79, 0)]
            if befund.get(k, (0, 0))[0] == 0]
    if leer:
        pytest.skip(f"die eingelegte Diskette ist nicht durchgehend formatiert (ohne Sektor: "
                    f"{leer}) — erst `--schreiben` oder eine formatierte Diskette einlegen")
    return True


@pytest.fixture(scope="session")
def freigabe(request, vorpruefung):
    """Schreibfreigabe + Sicherung.  Liefert den Pfad der Sicherung (oder ``None``)."""
    if not _schreiben(request.config):
        pytest.skip("Schreibstufe ohne --schreiben")
    sicherung = None
    if not request.config.getoption("--keine-sicherung"):
        ziel = OUT / "hw_sicherung"
        ziel.mkdir(parents=True, exist_ok=True)
        sicherung = ziel / time.strftime("%Y%m%d-%H%M%S.hfe")
        p = laufwerk()
        r = gw("read", "--drive", p["drive"], "--tracks",
               f"c=0-{p['num_cyls'] - 1}:h=0-{p['num_heads'] - 1}",
               f"{sicherung}::bitrate={p['cell_rate_kbps']}")
        assert r.returncode == 0 and sicherung.stat().st_size > 0, r.stderr[-400:]
    yield sicherung
    if sicherung is not None and request.config.getoption("--zurueckspielen"):
        schreibe_abbild(sicherung)


@pytest.fixture
def logdir(request) -> Path:
    d = LOGROOT / time.strftime("%Y%m%d") / request.node.name.replace("/", "_")
    d.mkdir(parents=True, exist_ok=True)
    return d


@pytest.fixture
def abbild_kopie(tmp_path):
    """Schreibbare Kopie einer Fixture (der Emulator mountet schreibend)."""
    import shutil
    from hw_helfer import FIXTURES

    def kopie(name: str) -> Path:
        ziel = tmp_path / name
        shutil.copy(FIXTURES / name, ziel)
        return ziel
    return kopie

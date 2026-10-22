"""Gegenprobe fuer ImageDisk (``.imd``): ein UNABHAENGIGER Python-Leser liest, was der Kern schreibt.

* ``imd_bau`` ist nur nach der Clean-Room-Beschreibung (doc/design/38a Teil 1) geschrieben,
  nicht nach dem C++-Kern.  Die Beispiele erzeugt ``ImdGegenprobe.SchreibtBeispiele`` im
  Unit-Test (``build/k1520_test_imd_codec``) samt Inhaltsangabe als JSON, die aus dem
  MEDIUM stammt und nicht aus dem Schreiber.
* Optional (uebersprungen, wenn Greaseweazle fehlt): ``gw convert unser.imd out.img`` muss
  den Inhalt unseres Schreibers wiedergeben.  Die Python-Ebene laeuft ohne ``greaseweazle``
  (CLAUDE.md, Abschnitt Python-Ebene) -- dieser Fall ist eine Zugabe, keine Voraussetzung.
"""

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

from conftest import PROJECT_ROOT
import imd_bau

TESTBIN = PROJECT_ROOT / "build" / "k1520_test_imd_codec"

pytestmark = pytest.mark.skipif(
    not TESTBIN.exists(),
    reason="build/k1520_test_imd_codec nicht gebaut -- vorher `tools/dev.sh build` ausfuehren",
)

KOPF = b"IMD 1.17: 22/10/2026 12:00:00\r\nVektor\r\n\x1a"


# ─── die Vektoren aus 38a, mit dem Python-Schreiber und von Hand ───────────────

def vektor_a():
    return KOPF + bytes([5, 0, 0, 2, 0, 1, 2, 2, 0xE5, 1]) + bytes(range(128))


def vektor_b():
    return (KOPF + bytes([2, 3, 0xC1, 3, 0, 9, 8, 7, 4, 3, 3, 0, 1, 0, 4, 0, 5])
            + bytes([0xAA]) * 128 + bytes([0]))


def vektor_c():
    return KOPF + bytes([3, 1, 0, 0, 2, 3, 2, 0, 1, 2, 1, 2, 0x5A])


def test_leser_versteht_die_drei_vektoren():
    a = imd_bau.lese(vektor_a())
    assert a["kopf"].startswith(b"IMD 1.17")
    (t,) = a["tracks"]
    assert (t["mode"], t["cyl"], t["head"], t["n"]) == (5, 0, 0, 0)
    assert [(s["c"], s["h"], s["r"], s["typ"]) for s in t["sectors"]] == [(0, 0, 1, 2), (0, 0, 2, 1)]
    assert t["sectors"][0]["data"] == b"\xe5" * 128
    assert t["sectors"][1]["data"] == bytes(range(128))

    (t,) = imd_bau.lese(vektor_b())["tracks"]
    assert (t["mode"], t["cyl"], t["head"]) == (2, 3, 1)
    assert [(s["c"], s["h"], s["r"], s["typ"]) for s in t["sectors"]] == [
        (4, 0, 9, 4), (3, 1, 8, 5), (3, 0, 7, 0)]
    assert t["sectors"][0]["data"] == bytes(128)
    assert t["sectors"][1]["data"] == b"\xaa" * 128
    assert t["sectors"][2]["data"] == b""

    leer, zwei = imd_bau.lese(vektor_c())["tracks"]
    assert (leer["cyl"], leer["sectors"]) == (1, [])
    assert (zwei["cyl"], zwei["n"], zwei["sectors"][0]["data"]) == (2, 2, b"\x5a" * 512)


def test_schreiber_gibt_die_vektoren_bytegleich_wieder():
    # Vektor C faellt heraus: seine leere Spur traegt s = 2, Schreiber schreiben dort 0 (38a 1.6)
    for dat in (vektor_a(), vektor_b()):
        abb = imd_bau.lese(dat)
        assert imd_bau.baue(abb["tracks"], kommentar=b"Vektor",
                            kopfzeile=b"IMD 1.17: 22/10/2026 12:00:00") == dat


def test_leser_weist_kaputtes_ab():
    for kaputt in (b"XMD " + vektor_a()[4:], vektor_a()[:60], vektor_a()[:45]):
        with pytest.raises(imd_bau.ImdFehler):
            imd_bau.lese(kaputt)
    mode6 = bytearray(vektor_a()); mode6[40] = 6
    with pytest.raises(imd_bau.ImdFehler):
        imd_bau.lese(bytes(mode6))


# ─── der Kern schreibt, Python liest ───────────────────────────────────────────

@pytest.fixture(scope="module")
def beispiele(tmp_path_factory):
    ziel = tmp_path_factory.mktemp("imd_beispiele")
    env = dict(os.environ, K1520_IMD_AUSGABE=str(ziel))
    r = subprocess.run([str(TESTBIN), "--gtest_filter=ImdGegenprobe.SchreibtBeispiele"],
                       env=env, capture_output=True, text=True, timeout=120)
    assert r.returncode == 0, r.stdout + r.stderr
    dateien = sorted(ziel.glob("*.imd"))
    assert len(dateien) >= 3, r.stdout
    return dateien


def _inhalt(imd: Path):
    return json.loads(imd.with_suffix(".json").read_text())


def test_python_liest_was_der_kern_schreibt(beispiele):
    for imd in beispiele:
        soll = _inhalt(imd)
        abb = imd_bau.lese(imd.read_bytes())
        assert abb["kopf"].startswith(b"IMD 1.17: ")
        assert b"Image file created by k1520emu" in abb["kopf"]

        belegt = [t for t in abb["tracks"] if t["sectors"]]
        leere = [t for t in abb["tracks"] if not t["sectors"]]
        assert len(belegt) == len(soll["tracks"]), imd.name
        # jede Spur der Geometrie steht genau einmal in der Datei (leere als n = 0)
        assert len(abb["tracks"]) == soll["cyls"] * soll["heads"], imd.name
        assert len(leere) == soll["cyls"] * soll["heads"] - len(belegt)

        for t, e in zip(belegt, soll["tracks"]):
            assert (t["cyl"], t["head"]) == (e["cyl"], e["head"])
            assert imd_bau.MODI[t["mode"]][0] == e["enc"], imd.name
            assert len(t["sectors"]) == len(e["sectors"])
            for s, x in zip(t["sectors"], e["sectors"]):       # Reihenfolge = Interleave
                assert t["n"] == x["n"]
                assert (s["c"], s["h"], s["r"]) == (x["c"], x["h"], x["r"])
                assert s["typ"] == imd_bau.typ_aus_flags(
                    bytes.fromhex(x["data"]), bool(x["deleted"]), bool(x["data_error"]),
                    bool(x["no_data"])), (imd.name, x["r"])
                assert s["data"] == bytes.fromhex(x["data"])


# ─── optionaler Prufstand: Greaseweazle ────────────────────────────────────────

def _gw():
    gw = shutil.which("gw") or str(Path(sys.executable).parent / "gw")
    if not Path(gw).exists():
        pytest.skip("Greaseweazle (`gw`) nicht vorhanden")
    return gw


def test_gw_convert_gibt_den_inhalt_unseres_schreibers_wieder(beispiele, tmp_path):
    gw = _gw()
    imd = next(p for p in beispiele if p.stem == "mfm_interleave")
    soll = _inhalt(imd)
    out = tmp_path / "out.img"
    # ibm.720 = 2 x 80 x 9 x 512; gelesen werden nur die beiden Zylinder des Beispiels
    r = subprocess.run([gw, "convert", "--format=ibm.720", "--tracks=c=0-1:h=0-1", str(imd), str(out)],
                       capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        pytest.skip("gw convert lehnt das Format ab: " + (r.stderr or r.stdout)[-300:])
    erwartet = bytearray()
    for t in soll["tracks"]:                      # Zylinder, Kopf, Sektornummer aufsteigend
        for x in sorted(t["sectors"], key=lambda s: s["r"]):
            erwartet += bytes.fromhex(x["data"])
    assert out.read_bytes()[:len(erwartet)] == bytes(erwartet)

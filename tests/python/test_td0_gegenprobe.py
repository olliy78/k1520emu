"""Gegenprobe fuer TeleDisk (``.td0``): unser Leser gegen Greaseweazle.

* ``td0_referenz`` liest mit ``greaseweazle.image.td0`` (Unlicense) -- ein vom Kern UNABHAENGIGER Leser.
  Ohne das Paket ``greaseweazle`` wird uebersprungen (die Python-Ebene laeuft ohne es, CLAUDE.md).
* Mit ``K1520_TD0_DIR=<Ordner>`` (rekursiv ``*.td0``) schreibt ``Td0Gegenprobe.SchreibtSektorlisten`` je
  Datei die Sektorliste aus dem MEDIUM des Kerns; sie wird mit der von gw verglichen: Spur, ID
  (C/H/R), Groesse und FNV-1a-64 der Daten, Sektor fuer Sektor in Spurreihenfolge.
  Zwei bekannte, gewollte Abweichungen: Pseudosektoren (Flag 0x40) lassen wir weg, gw behaelt sie
  (der Vergleich laesst gw-Sektoren aus, solange deren Anzahl dem ``pseudo`` des Kerns entspricht);
  Sektoren ohne Datenblock liefert gw als Nullen, wir fuehren sie ohne Datenfeld.
* Ohne Ordner prueft ein kurzer Fall die Vektoren aus ``td0_pack`` (TD und td) gegen gw.
"""

import json
import os
import subprocess
import sys
from pathlib import Path

import pytest

from conftest import PROJECT_ROOT
import td0_pack

TESTBIN = PROJECT_ROOT / "build" / "k1520_test_td0_codec"

pytestmark = pytest.mark.skipif(
    not TESTBIN.exists(),
    reason="build/k1520_test_td0_codec nicht gebaut -- vorher `tools/dev.sh build` ausfuehren",
)

try:
    import greaseweazle.codec.codec  # noqa: F401  (Importzyklus: nur von hier aus loesbar)
    import greaseweazle.image.td0  # noqa: F401
    from greaseweazle import optimised
    GW_DA = bool(optimised.enabled)
except Exception:   # noqa: BLE001
    GW_DA = False

braucht_gw = pytest.mark.skipif(not GW_DA, reason="greaseweazle (mit optimised-Erweiterung) nicht installiert")

import td0_referenz  # noqa: E402  (importiert gw erst in lese())


def _vergleiche(unser: dict, gw: dict):
    """-> Liste von Abweichungstexten (leer = gleich)."""
    if "fehler" in gw:
        return ["gw lehnt ab: " + gw["fehler"]]
    if "fehler" in unser:
        return ["wir lehnen ab: " + unser["fehler"]]
    fehler = []
    o = unser["sektoren"]
    g = gw["sektoren"]
    # je Spur getrennt, gw darf mehr Sektoren fuehren (Pseudosektoren)
    def je_spur(liste):
        d = {}
        for s in liste:
            d.setdefault((s[0], s[1]), []).append(s)
        return d
    os_, gs = je_spur(o), je_spur(g)
    extra = 0
    for spur, mine in os_.items():
        theirs = gs.get(spur, [])
        i = 0
        for s in mine:
            while i < len(theirs) and theirs[i][2:6] != s[2:6]:
                i += 1
                extra += 1
            if i >= len(theirs):
                fehler.append("Spur %s: Sektor %s fehlt bei gw" % (spur, s[2:6]))
                break
            t = theirs[i]
            i += 1
            nodata = s[6]
            # s = [cyl, head, c, h, r, groesse, nodata, deleted, fnv]
            if not nodata and t[6] != s[8]:
                fehler.append("Spur %s Sektor %s: Daten verschieden" % (spur, s[2:6]))
        extra += len(theirs) - i
    for spur in gs:
        if spur not in os_:
            extra += len(gs[spur])
    if extra != unser.get("pseudo", 0):
        fehler.append("gw fuehrt %d Sektoren mehr (Pseudosektoren bei uns: %d)" % (extra, unser.get("pseudo", 0)))
    return fehler


@braucht_gw
def test_gw_liest_die_vektoren_wie_wir_sie_bauen(tmp_path):
    plain = td0_pack.beispiel()
    for name, dat in (("plain", plain), ("td", td0_pack.td_packen(plain))):
        p = tmp_path / (name + ".td0")
        p.write_bytes(dat)
        r = td0_referenz.lese(str(p))
        assert "fehler" not in r, r
        ids = [(s[0], s[1], s[2], s[3], s[4], s[5]) for s in r["sektoren"]]
        assert ids == [(0, 0, 0, 0, 1, 128), (0, 0, 0, 0, 2, 128), (0, 0, 0, 0, 3, 128), (1, 0, 1, 0, 1, 256)], ids


def _ordner():
    e = os.environ.get("K1520_TD0_DIR")
    return Path(e) if e and Path(e).is_dir() else None


@braucht_gw
@pytest.mark.skipif(_ordner() is None, reason="K1520_TD0_DIR nicht gesetzt")
def test_alle_echten_td0_gleich_greaseweazle(tmp_path):
    ausgabe = tmp_path / "td0_aus"
    env = dict(os.environ, K1520_TD0_AUSGABE=str(ausgabe))
    r = subprocess.run([str(TESTBIN), "--gtest_filter=Td0Gegenprobe.SchreibtSektorlisten"],
                       env=env, capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, r.stdout + r.stderr
    dateien = sorted(ausgabe.glob("*.json"))
    assert dateien, r.stdout
    abweichungen = []
    for j in dateien:
        unser = json.loads(j.read_text())
        pfad = unser["datei"]
        if Path(pfad).stat().st_size == 0:
            assert "fehler" in unser, pfad   # die 0-Byte-Datei lehnen wir mit Grund ab
            continue
        gw = td0_referenz.lese(pfad)
        f = _vergleiche(unser, gw)
        if f:
            abweichungen.append("%s: %s" % (pfad, "; ".join(f[:3])))
    print("\n%d Dateien verglichen, %d Abweichungen" % (len(dateien), len(abweichungen)))
    assert not abweichungen, "\n".join(abweichungen)

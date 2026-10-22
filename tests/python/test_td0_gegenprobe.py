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


# ─── der Kern SCHREIBT (AP T2), Python und Greaseweazle lesen ─────────────────

def _schreibe_beispiele(ziel: Path, mit_korpus: bool = False):
    env = dict(os.environ, K1520_TD0_AUSGABE=str(ziel))
    if not mit_korpus:
        env.pop("K1520_TD0_DIR", None)   # nur die eingebauten Beispiele
    r = subprocess.run([str(TESTBIN), "--gtest_filter=Td0Gegenprobe.SchreibtBeispiele"],
                       env=env, capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, r.stdout + r.stderr


@pytest.fixture(scope="module")
def beispiele(tmp_path_factory):
    ziel = tmp_path_factory.mktemp("td0_beispiele")
    _schreibe_beispiele(ziel)
    dateien = sorted(ziel.glob("*.td0"))
    assert len(dateien) >= 4
    return dateien


def _inhalt(td0: Path):
    return json.loads(td0.with_suffix(".json").read_text())


def test_td0_pack_liest_den_vektor_und_weist_kaputtes_ab():
    a = td0_pack.lese(td0_pack.beispiel())
    assert a["kommentar"] == b"Testvektor\x00zweite Zeile\x00\x00"
    assert [(t["cyl"], t["fm"], len(t["sectors"])) for t in a["tracks"]] == [(0, False, 3), (1, True, 1)]
    assert a["tracks"][0]["sectors"][1]["data"] == b"\xe5" * 128
    for kaputt in (b"XD" + td0_pack.beispiel()[2:], td0_pack.beispiel()[:40]):
        with pytest.raises((td0_pack.Td0Fehler, IndexError)):
            td0_pack.lese(kaputt)
    bad = bytearray(td0_pack.beispiel())
    bad[5] ^= 1                                    # Kopf-CRC
    with pytest.raises(td0_pack.Td0Fehler):
        td0_pack.lese(bytes(bad))


def test_python_liest_was_der_kern_schreibt(beispiele):
    for td0 in beispiele:
        soll = _inhalt(td0)
        abb = td0_pack.lese(td0.read_bytes())
        k = abb["kopf"]
        assert k["version"] == 0x15 and k["volume"] == 0
        assert abb["kommentar"] == b"Image file created by k1520emu\x00" or td0.stem == "pseudo", td0.name
        # jede Spur der Geometrie steht genau einmal, aufsteigend nach (Zylinder, Kopf)
        assert [(t["cyl"], t["head"]) for t in abb["tracks"]] == \
            [(c, h) for c in range(soll["cyls"]) for h in range(soll["heads"])], td0.name
        belegt = [t for t in abb["tracks"] if t["sectors"]]
        assert len(belegt) >= len(soll["tracks"]) or td0.stem == "pseudo"
        echte = {(t["cyl"], t["head"]): t for t in abb["tracks"]}
        alle_fm = all(t["enc"] == "fm" for t in soll["tracks"])
        assert k["fm"] == alle_fm, td0.name
        pseudo = 0
        for e in soll["tracks"]:
            t = echte[(e["cyl"], e["head"])]
            assert t["fm"] == (e["enc"] == "fm"), td0.name        # FM-Bit je Spur
            recs = [s for s in t["sectors"] if not s["flags"] & 0x40]
            pseudo += len(t["sectors"]) - len(recs)
            assert len(recs) == len(e["sectors"]), td0.name
            for s, x in zip(recs, e["sectors"]):                   # Reihenfolge = Interleave
                assert (s["c"], s["h"], s["r"], s["n"]) == (x["c"], x["h"], x["r"], x["n"])
                assert bool(s["flags"] & 0x04) == bool(x["deleted"])
                assert bool(s["flags"] & 0x02) == bool(x["data_error"])
                assert bool(s["flags"] & 0x20) == bool(x["no_data"])
                if x["no_data"]:
                    assert s["data"] is None
                else:
                    assert s["data"] == bytes.fromhex(x["data"])
        assert pseudo == soll["pseudo"], td0.name
        assert abb["ende"][0] == 0xFF


def test_gw_liest_unsere_beispiele(beispiele):
    if not GW_DA:
        pytest.skip("greaseweazle (mit optimised-Erweiterung) nicht installiert")
    for td0 in beispiele:
        soll = _inhalt(td0)
        r = td0_referenz.lese(str(td0))
        assert "fehler" not in r, (td0.name, r)
        gw = r["sektoren"]
        erwartet = [s for t in soll["tracks"] for s in
                    [(t["cyl"], t["head"], x["c"], x["h"], x["r"], 128 << x["n"], x) for x in t["sectors"]]]
        # Pseudosektoren fuehrt gw mit (R >= 100): nur die echten vergleichen
        echte = [g for g in gw if (g[0], g[1], g[2], g[3], g[4]) in
                 {(e[0], e[1], e[2], e[3], e[4]) for e in erwartet}]
        assert len(gw) - len(echte) == soll["pseudo"], td0.name
        assert len(echte) == len(erwartet), td0.name
        for g, e in zip(echte, erwartet):
            assert tuple(g[:6]) == tuple(e[:6]), td0.name
            x = e[6]
            if not x["no_data"]:
                assert g[6] == td0_referenz.fnv1a64(bytes.fromhex(x["data"])), (td0.name, e[:5])


@pytest.mark.skipif(_ordner() is None, reason="K1520_TD0_DIR nicht gesetzt")
def test_gespeicherte_echtdateien_gleich_greaseweazle_und_td0_pack(tmp_path):
    """Jede echte Datei, vom Kern erneut gespeichert: gw liest sie wie das Original, td0_pack prueft alle CRCs."""
    if not GW_DA:
        pytest.skip("greaseweazle (mit optimised-Erweiterung) nicht installiert")
    _schreibe_beispiele(tmp_path, mit_korpus=True)
    paare = sorted((tmp_path / "korpus").glob("*.td0"))
    assert len(paare) >= 60
    abweichungen = []
    for neu in paare:
        orig = json.loads(neu.with_suffix(".json").read_text())["datei"]
        td0_pack.lese(neu.read_bytes())               # alle CRCs, Methoden 0/1 -- unabhaengig vom Kern
        a = td0_referenz.lese(orig)
        b = td0_referenz.lese(str(neu))
        if a != b:
            abweichungen.append("%s: %s" % (orig, a.get("fehler") or b.get("fehler") or "Sektorlisten verschieden"))
    print("\n%d Dateien verglichen, %d Abweichungen" % (len(paare), len(abweichungen)))
    assert not abweichungen, "\n".join(abweichungen)

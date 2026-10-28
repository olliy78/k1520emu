"""H9 — k1520DiskTool an der ECHTEN Diskette: CP/A, UDOS, WEGA.

Je Dateisystem wird das Abbild auf die Diskette gespielt, dann arbeitet das DiskTool
LIVE (``open_physical``, kein Abbild dazwischen).  Orakel ist dieselbe Aenderung an
einer Dateikopie (``disktool_szenario.py``, ohne Hardware auch in
``py_disktool_physisch_gegenprobe``):

* H9.1 live geoeffnet == Datei: Format, Dateisystem, Verzeichnis, Dateibytes, fsck,
  jeder Sektor samt Anhang hinter der Daten-CRC (UDOS-Verkettung);
* H9.2 aendern (einfuegen klein/gross, ueberschreiben, loeschen, Eigenschaften, bei WEGA
  Verzeichnisse) — Rueckfuehrung vollstaendig, jede Spur geprueft, keine Schadstelle,
  GENAU die geaenderten Spuren geschrieben;
* H9.3 frisch neu eingehaengt == Datei-Orakel nach der Aenderung;
* H9.4 unabhaengige Gegenprobe: der Dekoder von ``gw`` findet je Spur dieselben
  Sektoren wie im Orakel-Abbild;
* H9.5 schreibgeschuetzt geoeffnet: Aenderung abgewiesen, nichts geschrieben;
* H9.6 zu grosse Datei: abgewiesen, nichts geschrieben (Transaktion);
* H9.7 Kommandozeile ``--physical``: put / ls / get / rm / check.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys

import pytest

import disktool_szenario as dz
from hw_helfer import (PROJECT_ROOT, OUT, diskette_loeschen, laufwerk, schreibe_abbild,
                       sitzung, spurbefund)

pytestmark = pytest.mark.hw_schreiben

ABBILDER = OUT / "hw_abbilder"


def _szenario(name):
    return next(s for s in dz.szenarien(ABBILDER) if s.name == name)


@pytest.fixture(scope="module", params=["cpa", "udos", "wega"])
def lage(request, freigabe, tmp_path_factory):
    """Abbild aufgespielt + Datei-Orakel gerechnet (einmal je Dateisystem)."""
    sz = _szenario(request.param)
    if not sz.abbild.exists():
        pytest.skip(f"Abbild fehlt: {sz.abbild}")
    ordner = tmp_path_factory.mktemp(f"h9_{sz.name}")
    # Erst loeschen: ein Abbild mit weniger Zylindern (udos_ds77: 77) liesse sonst den
    # alten Inhalt auf den uebrigen stehen — live gelesen waere != Datei.
    diskette_loeschen()
    schreibe_abbild(sz.abbild)
    vorher, nachher = dz.datei_orakel(sz, ordner)
    return dict(sz=sz, ordner=ordner, vorher=vorher, nachher=nachher, ok={})


def _geaenderte_spuren(a: dict, b: dict) -> set:
    sa, sb = a["sektoren"], b["sektoren"]
    return {(c, h) for (c, h, n) in set(sa) | set(sb) if sa.get((c, h, n)) != sb.get((c, h, n))}


def test_h9_1_2_live_oeffnen_und_aendern(lage):
    from app.core_binding.k1520disk import DiskTool
    sz, o = lage["sz"], lage["ordner"]
    with sitzung(schreibbar=True) as s:
        with DiskTool.open_physical(s.sync, read_only=False) as d:
            live = dz.aufnahme(d, o / "live_vorher")
            assert dz.unterschiede(lage["vorher"], live) == [], "live gelesen != Datei"
            dz.anwenden(d, sz, o / "live_quellen")
        # Stats VOR close() lesen — danach ist das Handle freigegeben.
        assert s.sync.flush(600_000), s.sync.last_error
        st = s.sync.stats
    soll = _geaenderte_spuren(lage["vorher"], lage["nachher"])
    print(f"\n{sz.name}: {st.writes_done} Spuren geschrieben (Orakel: {len(soll)} geaendert), "
          f"{st.verifies_done} geprueft, misslungen {st.verify_failed}, "
          f"Lesewiederholungen {st.read_retries}")
    assert st.errors == 0 and st.tracks_defect == 0 and st.verify_failed == 0
    assert st.verifies_done == st.writes_done
    assert st.writes_done == len(soll), "nicht genau die geaenderten Spuren geschrieben"
    lage["ok"]["geaendert"] = True


def test_h9_3_frisch_eingehaengt_wie_die_datei(lage):
    assert lage["ok"].get("geaendert"), "H9.2 nicht gruen"
    from app.core_binding.k1520disk import DiskTool
    with sitzung() as s:
        with DiskTool.open_physical(s.sync) as d:
            ist = dz.aufnahme(d, lage["ordner"] / "live_nachher")
    assert dz.unterschiede(lage["nachher"], ist) == []
    lage["ok"]["frisch"] = True


def test_h9_4_gegenprobe_mit_dem_dekoder_von_gw(lage):
    assert lage["ok"].get("frisch"), "H9.3 nicht gruen"
    kopie = lage["ordner"] / f"orakel_{lage['sz'].name}{lage['sz'].abbild.suffix}"
    soll, ist = spurbefund(kopie, cyls=80), spurbefund(cyls=80)
    fehlt = {k: (soll[k], ist.get(k)) for k in soll if ist.get(k) != soll[k]}
    assert soll and not fehlt, fehlt


def test_h9_5_schreibgeschuetzt_wird_nichts_geschrieben(lage, tmp_path):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    with sitzung() as s:
        with DiskTool.open_physical(s.sync) as d:
            with pytest.raises(K1520DiskError):
                dz.anwenden(d, lage["sz"], tmp_path)
        assert s.sync.stats.writes_done == 0


def test_h9_6_zu_grosse_datei_aendert_nichts(lage, tmp_path):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    sz = lage["sz"]
    with sitzung(schreibbar=True) as s:
        with DiskTool.open_physical(s.sync, read_only=False) as d:
            vorher = dz.verzeichnis(d)
            with pytest.raises(K1520DiskError):
                d.insert(dz.zu_gross(d, sz, tmp_path), dz.zielname(d, sz, "RIESIG"))
            d.flush()
            assert dz.verzeichnis(d) == vorher
        assert s.sync.flush(120_000), s.sync.last_error
        assert s.sync.stats.writes_done == 0


def _cli(*args, timeout=900):
    p = laufwerk()
    befehl = [sys.executable, str(PROJECT_ROOT / "app/disktool/main.py"), "--physical",
              "--drive", p["drive"], "--rate", str(p["cell_rate_kbps"]), *args]
    env = dict(os.environ, LD_LIBRARY_PATH=str(PROJECT_ROOT / "build"))
    return subprocess.run(befehl, capture_output=True, text=True, env=env, timeout=timeout)


def test_h9_7_kommandozeile(lage, tmp_path):
    from app.core_binding.k1520disk import DiskTool
    sz = lage["sz"]
    name = {"cpa": "CLIPROBE.TXT", "udos": "CLIPROBE", "wega": "cliprobe"}[sz.name]
    quelle = tmp_path / name
    inhalt = dz.muster(2048, 11)
    quelle.write_bytes(inhalt)
    os.utime(quelle, (dz.MTIME, dz.MTIME))
    if sz.name == "udos":
        # Kopfsektorangaben wie beim Einfuegen ueber die API: Vorlage aus dem Orakel-Abbild
        kopie = lage["ordner"] / f"orakel_udos{sz.abbild.suffix}"
        with DiskTool.open(kopie) as d:
            dz.zu_gross(d, sz, tmp_path / "v")
        vorlage = next((tmp_path / "v").glob("*.fileinfo")).read_text(encoding="utf-8")
        dz._udos_beiblatt(tmp_path, name, vorlage)
    seite = "Side0/" if sz.name == "udos" else ""

    r = _cli("--write", "put", str(quelle))
    assert r.returncode == 0, r.stderr[-600:]
    r = _cli("ls")
    assert r.returncode == 0 and name in r.stdout, r.stdout[-600:] + r.stderr[-300:]
    ziel = tmp_path / "zurueck"
    ziel.mkdir()
    r = _cli("get", seite + name, "--to", str(ziel))
    assert r.returncode == 0, r.stderr[-600:]
    geholt = next(p for p in ziel.rglob("*") if p.is_file() and p.suffix != ".fileinfo")
    assert geholt.read_bytes() == inhalt
    r = _cli("--write", "rm", seite + name)
    assert r.returncode == 0, r.stderr[-600:]
    r = _cli("ls")
    assert name not in r.stdout
    r = _cli("check")
    assert r.returncode == 0, r.stdout[-600:] + r.stderr[-300:]

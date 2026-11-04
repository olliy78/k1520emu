"""Statuszeile des DiskTools: Kapazitaet statt nur freier Platz (Entwurf 47, Z1)."""

import re
from types import SimpleNamespace

import pytest

from conftest import requires_disk

from app.groessen import groesse, inhalt_text


def test_groesse_einheiten():
    assert groesse(0) == "0 KB"
    assert groesse(1023 * 1024) == "1023 KB"
    assert groesse(1024 * 1024) == "1,0 MB"
    assert groesse(1024 * 1024 * 3 // 2) == "1,5 MB"
    assert groesse(1024 * 1024 + 60 * 1024) == "1,1 MB"       # 1,0586 -> 1,1
    assert groesse(1024 * 1024 + 40 * 1024) == "1,0 MB"       # 1,039 -> 1,0
    assert groesse(1024 * 1024 - 1) == "1023 KB"              # unter 1 MB bleibt KB


def test_ohne_kapazitaet_nie_von_null():
    v = SimpleNamespace(dir="", total=0, free=64 * 1024)
    t = inhalt_text(3, [v], False)
    assert t == "3 Dateien - 64 KB frei" and "von" not in t


def test_mehrseitig_und_tar_reine_funktion():
    vs = [SimpleNamespace(dir=d, total=320 * 1024, free=64 * 1024) for d in ("A", "B")]
    t = inhalt_text(2, vs, True)
    assert "A: 64 KB von 320 KB frei" in t and "B: 64 KB von 320 KB frei" in t
    plan = {"abbilder": 3, "strom": 1258291, "kapazitaet": 720 * 1024}
    assert inhalt_text(12, [], False, plan) == "12 Dateien - 1,2 MB von 2,1 MB belegt (3 Disketten)"
    plan = {"abbilder": 1, "strom": 0, "kapazitaet": 720 * 1024}
    assert inhalt_text(0, [], False, plan) == "0 Dateien - 0 KB von 720 KB belegt"


@requires_disk
def test_einzeldiskette_cpm(temp_disk):
    from app.core_binding.k1520disk import DiskTool
    with DiskTool.open(temp_disk("cpa_cpa780_k5601_clock.img")) as t:
        vs = t.volumes()
        text = inhalt_text(len(t.list()), vs, t.volume_count > 1)
    m = re.fullmatch(r"(\d+) Dateien - (\d+) KB von (\d+) KB frei", text)
    assert m, text
    assert 0 <= int(m.group(2)) <= int(m.group(3)) and int(m.group(3)) > 0


@requires_disk
def test_udos_zweiseitig_je_seite(temp_disk):
    from app.core_binding.k1520disk import DiskTool
    with DiskTool.open(temp_disk("udos_ds77_k5601_fremdsync.hfe")) as t:
        assert t.volume_count == 2
        text = inhalt_text(len(t.list()), t.volumes(), True)
    seiten = re.findall(r"(\S+): (\d+) KB von (\d+) KB frei", text)
    assert len(seiten) == 2, text
    for _, frei, ges in seiten:
        assert int(frei) <= int(ges)


@requires_disk
def test_tar_satz_zwei_abbilder(tmp_path):
    from app.core_binding.k1520disk import DiskTool
    q = tmp_path / "gross.bin"
    q.write_bytes(bytes((i * 7) & 0xFF for i in range(300000)))
    with DiskTool.tar_satz_neu("ss40_16x256", tmp_path / "satz.hfe") as t:
        t.insert(str(q), "gross")
        plan = t.tar_satz_planen()
        assert plan["abbilder"] == 2
        text = inhalt_text(len(t.list()), t.volumes(), t.volume_count > 1, plan)
    assert re.fullmatch(r"1 Dateien - \d+(,\d)? [KM]B von 320 KB belegt \(2 Disketten\)", text), text

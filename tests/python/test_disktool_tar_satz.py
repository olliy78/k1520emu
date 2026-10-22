"""tar-Saetze und neue tar-Disketten ueber die C-ABI (`DiskTool.tar_*`, Entwurf 37, AP T3).

Die Regeln pruefen die C++-Tests `TarSatz.*`/`TarNeu.*`; hier geht es um die Verdrahtung:
neu anlegen, Dateien ablegen, Attribute, Satz speichern, wieder oeffnen, `.tar` als Datei
oder Container, Sperre fuer physische Disketten.
"""

import shutil
import subprocess

import pytest

from conftest import requires_disk

pytestmark = requires_disk

KLEIN = 163840            # Nutzbytes von k5601_ss40_16x256


def _tool():
    from app.core_binding.k1520disk import DiskTool
    return DiskTool


def _quelle(tmp_path, name, daten):
    p = tmp_path / "q" / name
    p.parent.mkdir(exist_ok=True)
    p.write_bytes(daten)
    return p


def _bytes(n, start=1):
    return bytes((start + i * 7) & 0xFF for i in range(n))


def test_format_list_names_uniform_formats_only():
    liste = _tool().tar_formate()
    namen = {f["name"]: f for f in liste}
    assert namen["k5601_9x512"]["bytes"] == 737280
    assert namen["k5601_ss40_16x256"]["bytes"] == KLEIN
    assert all(f["bytes"] >= 1024 for f in liste)
    assert any(f["img"] for f in liste) and any(f["hfe"] for f in liste)


def test_new_empty_disk_in_chosen_format_roundtrip(tmp_path):
    pfad = tmp_path / "klein.img"
    daten = _bytes(50000)
    with _tool().tar_disk_neu(pfad, "k5601_ss40_16x256") as t:
        assert t.tar_satz and not t.read_only
        assert pfad.stat().st_size == KLEIN                  # sofort da, genau dieser Name
        t.insert(str(_quelle(tmp_path, "a.bin", daten)), "a.bin")
        plan = t.tar_satz_planen()
        assert plan["einzel"] and plan["abbilder"] == 1 and plan["namen"] == [str(pfad)]
        r = t.tar_satz_speichern()
        assert r["geschrieben"] == [str(pfad)]
    with _tool().open_tar_satz_format([pfad], "k5601_ss40_16x256") as w:
        assert [e.name for e in w.list()] == ["a.bin"]
        ziel = tmp_path / "aus.bin"
        w.extract("a.bin", str(ziel))
        assert ziel.read_bytes() == daten
    if shutil.which("tar"):
        assert subprocess.run(["tar", "-tf", str(pfad)], capture_output=True).returncode == 0


def test_new_set_grows_to_numbered_images_and_attributes_survive(tmp_path):
    basis = tmp_path / "satz.hfe"
    gross = _bytes(300000, 3)
    with _tool().tar_satz_neu("k5601_ss40_16x256", basis) as t:
        assert t.tar_satz and not t.dirty
        t.insert(str(_quelle(tmp_path, "gross", gross)), "gross")
        t.set_unix_attrs("gross", mode=0o600, uid=25, gid=7)
        assert t.dirty
        plan = t.tar_satz_planen()
        assert plan["abbilder"] == 2 and not plan["einzel"]
        assert plan["namen"] == [str(tmp_path / "satz1.hfe"), str(tmp_path / "satz2.hfe")]
        assert plan["kapazitaet"] == KLEIN and plan["strom"] > 300000
        r = t.tar_satz_speichern()
        assert r["geschrieben"] == plan["namen"] and r["hinweise"] == []
        assert "physische Diskette" in t.physisch_schreiben_grund
    with _tool().open_tar_satz_format(plan["namen"]) as w:
        eintrag = [e for e in w.list() if e.name == "gross"][0]
        assert (eintrag.unix_mode & 0o777, eintrag.unix_uid, eintrag.unix_gid) == (0o600, 25, 7)


def test_old_numbers_are_kept_and_reported(tmp_path):
    basis = tmp_path / "s.img"
    with _tool().tar_satz_neu("k5601_ss40_16x256", basis) as t:
        t.insert(str(_quelle(tmp_path, "gross", _bytes(300000, 5))), "gross")
        t.tar_satz_speichern()
    # der Satz schrumpft: Abbild 2 gehoert nicht mehr dazu und bleibt liegen
    with _tool().open_tar_satz_format([tmp_path / "s1.img", tmp_path / "s2.img"], "k5601_ss40_16x256") as t:
        t.set_read_only(False)
        t.erase("gross")
        plan = t.tar_satz_planen()
        assert plan["abbilder"] == 1 and plan["altlasten"] == [str(tmp_path / "s2.img")]
        r = t.tar_satz_speichern()
        assert len(r["hinweise"]) == 1 and (tmp_path / "s2.img").exists()


def test_single_disk_becomes_set_on_demand(tmp_path):
    einzel = tmp_path / "e.img"
    _tool().create(einzel, "tar720_leer", "").close()
    with _tool().open(einzel, read_only=False) as t:
        assert not t.tar_satz
        t.tar_zu_satz(tmp_path / "ueb.img")
        assert t.tar_satz and t.tar_satz_basis == str(tmp_path / "ueb.img")
        t.insert(str(_quelle(tmp_path, "gross", _bytes(800000, 9))), "gross")
        assert t.tar_satz_planen()["abbilder"] == 2


def test_new_set_rejects_unfit_formats_and_names(tmp_path):
    from app.core_binding.k1520disk import K1520DiskError
    with pytest.raises(K1520DiskError):
        _tool().tar_satz_neu("cpa780", tmp_path / "x.hfe")           # gemischtes Format
    with pytest.raises(K1520DiskError):
        _tool().tar_disk_neu(tmp_path / "x.txt", "k5601_9x512")
    assert not (tmp_path / "x.txt").exists()


@pytest.mark.skipif(not shutil.which("tar"), reason="kein tar im System")
def test_dropped_tar_as_file_or_as_container(tmp_path):
    from app.core_binding.k1520disk import K1520DiskError
    _quelle(tmp_path, "x.bin", _bytes(3000, 14))
    paket = tmp_path / "paket.tar"
    subprocess.run(["tar", "--format=v7", "-cf", str(paket), "-C", str(tmp_path / "q"), "x.bin"], check=True)
    basis = tmp_path / "z.img"
    with _tool().tar_satz_neu("k5601_9x512", basis) as t:
        u = t.tar_datei_pruefen(paket)
        assert u["als_datei"] and u["als_container"] and u["ziel_leer"]
        assert not u["wird_satz"] and u["abbilder"] == 1
        t.tar_datei_ablegen(paket)                                  # Weg 1: als Datei
        assert [e.name for e in t.list()] == ["paket.tar"]
        u = t.tar_datei_pruefen(paket)                              # nicht mehr leer
        assert u["als_datei"] and not u["als_container"] and u["container_grund"]
        with pytest.raises(K1520DiskError):
            t.tar_datei_ablegen(paket, als_container=True)
    with _tool().tar_satz_neu("k5601_9x512", basis) as t:           # Weg 2: der Inhalt WIRD das Archiv
        t.tar_datei_ablegen(paket, als_container=True)
        assert [e.name for e in t.list()] == ["x.bin"]


@pytest.mark.skipif(not shutil.which("tar"), reason="kein tar im System")
def test_container_beyond_one_disk_makes_a_set(tmp_path):
    for i in range(3):
        _quelle(tmp_path, "d%d" % i, _bytes(300000, 20 + i))
    paket = tmp_path / "gross.tar"
    subprocess.run(["tar", "--format=v7", "-cf", str(paket), "-C", str(tmp_path / "q"), "d0", "d1", "d2"], check=True)
    einzel = tmp_path / "e.img"
    _tool().create(einzel, "tar720_leer", "").close()
    with _tool().open(einzel, read_only=False) as t:
        u = t.tar_datei_pruefen(paket)
        assert u["als_container"] and u["wird_satz"] and u["abbilder"] == 2
        t.tar_datei_ablegen(paket, als_container=True)
        assert t.tar_satz
        r = t.tar_satz_speichern()
        assert len(r["geschrieben"]) == 2
    with _tool().open_tar_satz(r["geschrieben"]) as w:
        assert [e.name for e in w.list()] == ["d0", "d1", "d2"]


def test_tar_check_without_writable_tar_volume_fails_clearly(tmp_path):
    from app.core_binding.k1520disk import K1520DiskError
    pfad = tmp_path / "s.img"
    _tool().tar_disk_neu(pfad, "k5601_ss40_16x256").close()
    paket = _quelle(tmp_path, "k.tar", bytes(1024))
    with _tool().open_tar_satz_format([pfad], "k5601_ss40_16x256") as w:   # schreibgeschuetzt
        assert w.read_only
        with pytest.raises(K1520DiskError):
            w.tar_datei_pruefen(paket)

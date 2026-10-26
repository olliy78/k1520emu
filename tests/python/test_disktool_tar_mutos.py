"""tar im MUTOS-8000-Layout ueber die C-ABI (`tar_mutos8k_26x128_mfm`/`_fm`, Entwurf 39, Merkposten tar_satz.md Nr. 17).

Die Regeln (Blockabbildung, Auffuellung, Erkennung) pruefen die C++-Tests `TarMutos.*`; hier geht es
um die Verdrahtung: die Formatliste nennt die Profile, ein Satz laesst sich damit anlegen/speichern/
wieder oeffnen, die leere tar-Diskette laesst sich per `create` anlegen, MUTOS-Fixtures werden erkannt.
"""

import pytest

from conftest import requires_disk

pytestmark = requires_disk

KAPAZITAET = 494 * 512            # Rohgeraet des MUTOS-Treibers: 494 Bloecke


def _tool():
    from app.core_binding.k1520disk import DiskTool
    return DiskTool


def _bytes(n, start=1):
    return bytes((start + i * 7) & 0xFF for i in range(n))


def _quelle(tmp_path, name, daten):
    p = tmp_path / "q" / name
    p.parent.mkdir(exist_ok=True)
    p.write_bytes(daten)
    return p


def test_format_list_names_the_mutos_profiles_with_the_raw_device_capacity():
    namen = {f["name"]: f for f in _tool().tar_formate()}
    for profil in ("tar_mutos8k_26x128_mfm", "tar_mutos8k_26x128_fm"):
        assert namen[profil]["bytes"] == KAPAZITAET
        assert namen[profil]["hfe"]


def test_a_set_in_the_mutos_layout_spreads_over_two_disks_and_reopens(tmp_path):
    a, b = _bytes(150000, 3), _bytes(160000, 5)
    with _tool().tar_satz_neu("tar_mutos8k_26x128_mfm", tmp_path / "m.hfe") as t:
        t.insert(str(_quelle(tmp_path, "a.bin", a)), "a.bin")
        t.insert(str(_quelle(tmp_path, "b.bin", b)), "b.bin")
        plan = t.tar_satz_planen()
        assert plan["abbilder"] == 2 and plan["kapazitaet"] == KAPAZITAET
        t.tar_satz_speichern()
    assert (tmp_path / "m1.hfe").exists() and (tmp_path / "m2.hfe").exists()
    with _tool().open_tar_satz([tmp_path / "m1.hfe", tmp_path / "m2.hfe"]) as w:
        assert sorted(e.name for e in w.list()) == ["a.bin", "b.bin"]
        w.extract("b.bin", str(tmp_path / "b_aus.bin"))
    assert (tmp_path / "b_aus.bin").read_bytes() == b


def test_a_new_empty_disk_is_detected_as_the_empty_mutos_profile(tmp_path):
    pfad = tmp_path / "leer.hfe"
    _tool().create(pfad, "tar_mutos8k_26x128_fm_leer").close()
    with _tool().open(pfad) as t:
        assert t.filesystem == "tar_mutos8k_26x128_fm_leer"


def test_a_disk_written_by_mutos_itself_is_recognised_as_tar(temp_disk):
    with _tool().open(temp_disk("mutos8000_tar_crt0.dmk")) as t:
        assert t.filesystem == "tar_mutos8k_26x128_mfm"
        assert [e.name for e in t.list()] == ["crt0.o"]

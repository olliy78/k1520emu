"""TeleDisk (.td0) und ImageDisk (.imd) im Emulator, Kernseite (AP K1, Entwurf 38).

Nur ueber ``K1520Emulator`` / C-ABI, keine Oberflaeche: Mounten, Container-Auskunft,
Speichern unter + Wiederbooten, Schreibschutz-Politik (kein erzwungener Schutz),
leere Diskette wird abgelehnt.  Die Dateien sind unabhaengig vom C++-Kern gebaut
(``td0_pack.py``, ``imd_bau.py``), das Boot-Abbild entsteht ueber ``save_disk_as``.
"""

import pytest

from conftest import requires_core, run_until_text
import imd_bau
import td0_pack

pytestmark = requires_core


def _placeholder(emu):
    """Selbstbeschreibende Container brauchen im Kern trotzdem einen Katalognamen."""
    formate = emu.drive_formats(0)
    return formate[0] if formate else "cpa800"


@pytest.fixture
def td0_datei(tmp_path):
    p = tmp_path / "test.td0"
    p.write_bytes(td0_pack.beispiel())
    return str(p)


@pytest.fixture
def imd_datei(tmp_path):
    daten = bytes([0xE5]) * 512
    spur = {"mode": 5, "cyl": 0, "head": 0, "n": 2,
            "sectors": [{"c": 0, "h": 0, "r": r, "typ": imd_bau.typ_aus_flags(daten, False, False, False),
                         "data": daten} for r in range(1, 10)]}
    p = tmp_path / "test.imd"
    p.write_bytes(imd_bau.baue([spur], kommentar=b"Testkommentar IMD"))
    return str(p)


def test_td0_mounts_writable_and_reports_its_header(emulator, td0_datei):
    assert emulator.mount_disk(0, td0_datei, _placeholder(emulator), False), emulator.last_error()
    assert emulator.disk_container(0) == "td0"
    assert emulator.disk_container_info(0, "type") == "td0"
    assert emulator.disk_container_info(0, "comment").startswith("Testvektor")
    assert emulator.disk_container_info(0, "version") == "1.5"
    assert emulator.disk_container_info(0, "rate") == "250"
    assert emulator.disk_container_info(0, "sectors") == "4"
    assert emulator.disk_container_info(0, "nonsens") == ""
    assert not emulator.is_disk_write_protected(0)       # kein erzwungener Schutz
    emulator.set_disk_write_protect(0, True)
    assert emulator.is_disk_write_protected(0)
    emulator.set_disk_write_protect(0, False)
    assert not emulator.is_disk_write_protected(0)
    assert emulator.disk_container_info(1, "comment") == ""


def test_imd_mounts_writable_and_reports_its_header(emulator, imd_datei):
    assert emulator.mount_disk(0, imd_datei, _placeholder(emulator), False), emulator.last_error()
    assert emulator.disk_container(0) == "imd"
    assert emulator.disk_container_info(0, "comment") == "Testkommentar IMD"
    assert emulator.disk_container_info(0, "version") == "1.17"
    assert emulator.disk_container_info(0, "sectors") == "9"
    assert emulator.disk_container_info(0, "rate") == ""        # IMD fuehrt keine Dateirate
    assert not emulator.is_disk_write_protected(0)


@pytest.mark.parametrize("endung", ["td0", "imd"])
def test_boot_disk_roundtrips_through_the_container_and_boots(emulator, temp_disk, tmp_path, endung):
    """CP/A-Bootdiskette (.img) -> save_disk_as(.td0|.imd) -> frischer Emulator bootet davon."""
    quelle = temp_disk("cpa_cpa780_k5601_clock.img")
    assert emulator.mount_disk(0, quelle, "cpa780", False), emulator.last_error()
    ziel = str(tmp_path / ("boot." + endung))
    assert emulator.save_disk_as(0, ziel, ""), emulator.last_error()
    assert emulator.disk_container(0) == endung
    assert emulator.disk_path(0) == ziel
    assert emulator.disk_container_info(0, "comment") != ""      # Standardkommentar des Kerns
    assert emulator.flush_disks()
    emulator.unmount_disk(0)

    from app.core_binding.k1520 import K1520Emulator
    frisch = K1520Emulator()
    try:
        assert frisch.mount_disk(0, ziel, "cpa780", False), frisch.last_error()
        frisch.power_on()
        assert run_until_text(frisch, "Bootloader, Version"), frisch.screen_text()
        assert run_until_text(frisch, "TPA ist OK"), frisch.screen_text()
    finally:
        del frisch


def test_blank_disk_cannot_be_created_as_td0_or_imd(emulator, tmp_path):
    for endung in ("td0", "imd"):
        ziel = tmp_path / ("leer." + endung)
        assert not emulator.create_disk(0, str(ziel), "", False)
        assert "leere Diskette" in emulator.last_error().lower() or "Diskette" in emulator.last_error()
        assert endung in emulator.last_error()
        assert not ziel.exists()


@pytest.mark.parametrize("endung", ["td0", "imd"])
def test_preformatted_disk_can_be_created_as_td0_or_imd(emulator, tmp_path, endung):
    ziel = tmp_path / ("neu." + endung)
    assert emulator.create_disk(0, str(ziel), "cpa800", False), emulator.last_error()
    assert ziel.exists()
    assert emulator.disk_container(0) == endung
    assert emulator.disk_container_info(0, "type") == endung
    assert emulator.disk_container_info(0, "sectors") != ""

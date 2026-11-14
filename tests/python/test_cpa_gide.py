"""GIDE-CP/A (Klaus Wilfling) am A5120: Boot von der GIDE-Platte mit dem Urlader-EPROM K2526UL4.

Die Eingaben stammen aus der CPA_Workbench (Variante ``gide_a5120``, K7024, 2 Floppy, 8 GIDE-Laufwerke, Basis D0H):
``disks/a5120_cpa_gide/`` -- Systemdiskette, bootfaehige Platte (``HDFORMAT`` + ``SYSWRCPA`` im Emulator ausgefuehrt)
und das Lade-ROM ``ul4`` (``doc/EPROMS/K2526/laderom_ul4_gide.bin``, bitgleich zu ``k2526ul4.bin`` der Workbench).
Gemountet werden nur Kopien (der Emulator oeffnet Abbilder lesend UND schreibend).
"""

import shutil
from pathlib import Path

from conftest import requires_core, run_until_text

pytestmark = requires_core

DISKS = Path(__file__).resolve().parents[2] / "disks" / "a5120_cpa_gide"
SYSTEM = DISKS / "a5120_cpa_gide_k7024_2fdd_8gide_system.hfe"
PLATTE = DISKS / "a5120_cpa_gide_platte_256M.img.gz"
GIDE = {"basis": 0xD0}          # die Basis des Urladers und der GIDE-Werkzeuge


def _emu(tmp_path, laderom, diskette=False):
    from app.core_binding.k1520 import K1520Emulator
    emu = K1520Emulator(machine="a5120", laderom=laderom, gide=GIDE)
    platte = tmp_path / PLATTE.name
    shutil.copyfile(PLATTE, platte)
    assert emu.gide_image_mount(platte), emu.gide_error()
    if diskette:
        ziel = tmp_path / SYSTEM.name
        shutil.copyfile(SYSTEM, ziel)
        assert emu.mount_disk(0, str(ziel), "cpa780", False), emu.last_error()
    emu.power_on()
    return emu


def _tippe(emu, text):
    for ch in text:
        code = 0x01000004 if ch == "\r" else ord(ch)
        emu.key_press(code)
        emu.run(300_000)
        emu.key_release(code)
        emu.run(300_000)
    emu.run(20_000_000)


def test_ul4_bootet_ohne_diskette_von_der_gide_platte(tmp_path):
    emu = _emu(tmp_path, "ul4")
    assert emu.laderom == "ul4"
    assert run_until_text(emu, "TPA ist OK!", 600_000_000), emu.screen_text()
    bild = emu.screen_text()
    assert "GIDE-CP/A" in bild and "GIDE-Modul auf Adresse 0D0H" in bild
    assert "GIDE-Laufwerke C: bis J:" in bild
    assert "RTC 72421 auf Adresse 0D5H" in bild
    emu.run(20_000_000)
    assert "C>" in emu.screen_text()                 # Standardlaufwerk = erstes GIDE-Laufwerk
    _tippe(emu, "dir\r")
    bild = emu.screen_text()
    for name in ("HDINFO", "SYSWRCPA", "@OS"):
        assert name in bild, bild


def test_standard_lade_rom_bootet_ohne_diskette_nicht(tmp_path):
    """Gegenprobe: V0.9_2 kennt kein GIDE, ohne Diskette kommt kein CP/A."""
    emu = _emu(tmp_path, "v09_2")
    emu.run(400_000_000)
    assert "GIDE-CP/A" not in emu.screen_text()


def test_diskette_bootet_mit_ul4_zuerst_und_bringt_den_gide_treiber_mit(tmp_path):
    emu = _emu(tmp_path, "ul4", diskette=True)
    assert run_until_text(emu, "TPA ist OK!", 600_000_000), emu.screen_text()
    bild = emu.screen_text()
    assert "GIDE-Laufwerke C: bis J:" in bild
    emu.run(20_000_000)
    assert "A>" in emu.screen_text()                 # Diskette hat Vorrang, Standardlaufwerk A:

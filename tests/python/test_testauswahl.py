"""Testauswahl vor dem Merge (``tools/test_auswahl.py``, ``tools/dev.sh test-merge``).

Laeuft ohne Kern und ohne Qt.  Der Waechter am Ende prueft gegen die ctest-
Registrierung, dass kein langsamer Fall OHNE Fachlabel ist — sonst fiele ein
neuer Test still aus jeder gefilterten Auswahl.
"""

import importlib.util
import json
import shutil
import subprocess

import pytest

from conftest import PROJECT_ROOT

_spec = importlib.util.spec_from_file_location("test_auswahl", PROJECT_ROOT / "tools" / "test_auswahl.py")
ta = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ta)


def plan(*dateien):
    return ta.plan(list(dateien))


@pytest.mark.parametrize("pfad, fach", [
    ("core/peripherals/floppy_drive/track_codec.cpp", "fach_floppy"),
    ("core/cards/k5122/k5122.cpp", "fach_floppy"),
    ("core/filesystem/cpm.cpp", "fach_floppy"),
    ("data/formats.yaml", "fach_floppy"),
    ("tests/system/drivers/format_all.py", "fach_floppy"),
    ("tools/format_driver.cpp", "fach_floppy"),
    ("core/serial/hub.cpp", "fach_serial"),
    ("core/primitives/z80_sio.cpp", "fach_serial"),
    ("kermit/protokoll.cpp", "fach_kermit"),
    ("tools/kermit80/bau.py", "fach_kermit"),
    ("core/cards/p8000/wdc.cpp", "fach_p8000"),
    ("core/machines/p8000/p8000.cpp", "fach_wega"),
    ("core/peripherals/winchester/platte.cpp", "fach_p8000"),
    ("core/peripherals/p8000_terminal/t.cpp", "fach_p8000"),
    ("core/cards/p8000/rtc.cpp", "fach_rtc"),
    ("core/machines/a5120/a5120.cpp", "fach_a5120"),
    ("core/machines/k8915/k8915.cpp", "fach_k8915"),
    ("core/machines/pc1715/pc1715.cpp", "fach_pc1715"),
    ("core/machines/prg710/prg710.cpp", "fach_prg710"),
])
def test_je_bereich_das_fachgebiet(pfad, fach):
    p = plan(pfad)
    assert p["test"] and fach in p["facher"], p


def test_ein_bereich_zieht_nicht_die_ganze_welt_mit():
    p = plan("core/cards/p8000/rtc.cpp")
    assert p["facher"] != ta.ALLE
    assert "fach_floppy" not in p["facher"] and "fach_serial" not in p["facher"]


@pytest.mark.parametrize("pfad", [
    "core/bus/k1520_bus.cpp", "core/api/k1520_api.h", "core/util/os_compat.h", "core/logger.cpp",
    "core/primitives/z80.cpp", "cmake/toolchain-mingw64.cmake", "CMakeLists.txt",
    "tests/support/tempdisk.h", "tests/cmake/K1520AddTest.cmake", "tools/dev.sh",
    "core/gibt_es_nicht/neu.cpp", "core/machines/laufwerke.cpp",
])
def test_gemeinsames_und_unbekanntes_unter_core_faellt_auf_alles_zurueck(pfad):
    p = plan(pfad)
    assert p["facher"] == ta.ALLE and p["test"] and p["win"], p


@pytest.mark.parametrize("dateien", [
    ["README.md"], ["doc/design/12_testing.md", "CLAUDE.md"], ["app/help/handbuch.md"],
])
def test_nur_doku_laeuft_gar_nichts(dateien):
    p = plan(*dateien)
    assert p["nur_doku"] and not p["test"] and not p["facher"] and not p["win"], p


def test_doku_plus_code_ist_nicht_nur_doku():
    assert plan("README.md", "core/cards/p8000/rtc.cpp")["test"]


def test_reine_oberflaeche_nur_test_ohne_langsame_spur_und_win():
    p = plan("app/ui/main_window.py")
    assert p["test"] and not p["facher"] and not p["win"], p


def test_cpp_aenderung_loest_windows_aus_python_nicht():
    assert plan("core/machines/mc80/mc80.cpp")["win"]
    assert not plan("tools/test_auswahl.py")["win"]


def test_alles_erzwingt_alles():
    p = ta.plan([], alles=True)
    assert p["test"] and p["win"] and p["facher"] == ta.ALLE


def test_keine_aenderung_nichts():
    p = ta.plan([])
    assert not p["test"] and not p["facher"]


def test_jedes_ziel_der_tabelle_ist_ein_bekanntes_fachlabel():
    for muster, facher, _g, _w in ta.REGELN:
        if facher != ta.ALLE:
            assert set(facher) <= set(ta.FACHER), (muster, facher)


def test_dev_sh_trocken_gibt_den_plan_ohne_zu_bauen():
    if shutil.which("bash") is None:
        pytest.skip("keine bash")
    r = subprocess.run(["bash", str(PROJECT_ROOT / "tools" / "dev.sh"), "test-merge", "--trocken",
                        "--dateien", "core/cards/p8000/rtc.cpp"],
                       capture_output=True, text=True, cwd=PROJECT_ROOT)
    assert r.returncode == 0, r.stderr
    text = r.stdout + r.stderr   # der Plan geht auf stderr (stdout gehoert den Zuweisungen)
    assert "fach_rtc" in text and "fach_p8000" in text, text


def test_jeder_langsame_fall_traegt_ein_fachlabel():
    """Waechter: ein neuer Fall der langsamen Spuren ohne `fach_*` fiele still aus test-merge."""
    ctest = shutil.which("ctest")
    bau = PROJECT_ROOT / "build"
    if ctest is None or not (bau / "CTestTestfile.cmake").exists():
        pytest.skip("build/ nicht konfiguriert")
    r = subprocess.run([ctest, "--test-dir", str(bau), "--show-only=json-v1"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr[-500:]
    ohne, n = [], 0
    for t in json.loads(r.stdout)["tests"]:
        labels = set()
        for prop in t.get("properties", []):
            if prop["name"] == "LABELS":
                labels = set(prop["value"])
        if labels & set(ta.LANGSAME_LABELS):
            n += 1
            fach = labels & set(ta.FACHER)
            if not fach:
                ohne.append(t["name"])
    assert n > 100, f"auffaellig wenige langsame Faelle ({n})"
    assert not ohne, "ohne fach_*-Label: " + ", ".join(ohne[:20])

#!/usr/bin/env python3
"""Waehlt die Tests vor dem Merge nach Aenderungsbereich (tools/dev.sh test-merge).

Die langsamen Spuren (Labels format_integration, format_matrix, wega_install)
tragen zusaetzlich Fachgebiets-Labels `fach_*` (Registrierung in tests/*/CMakeLists.txt).
Hier steht an EINER Stelle, welcher Pfad welche Fachgebiete beruehrt.  Aufbau:

  * `test` (schnelle Spur, inkl. Python-Ebene) laeuft immer, ausser bei reiner Doku.
  * Die langsamen Spuren laufen nur fuer die beruehrten Fachgebiete; Gemeinsames
    (core/bus, core/api, core/util, cmake, tests/support, dev.sh, unbekannte Pfade
    unter core/) beruehrt ALLE.
  * `win` (MinGW + wine) bei jeder Aenderung an C++/CMake: dort fallen Pfad- und
    Portierungsfehler (std::filesystem, POSIX-Reste) an, die kein Fachlabel kennt.

Stdlib only, kein Qt.  Aufruf:
  test_auswahl.py [--basis REF] [--alles] [--dateien P ...] [--shell]
--shell druckt auf stdout zuweisbare Zeilen fuer dev.sh, den Plan auf stderr.
"""
import argparse
import re
import subprocess
import sys

ALLE = "*"  # Sonderwert: alle Fachgebiete (= alle langsamen Faelle)

# Fachgebiete (muessen mit den Labels in tests/ uebereinstimmen; Waechter prueft es).
FACHER = ("fach_floppy", "fach_serial", "fach_kermit", "fach_p8000", "fach_wega",
          "fach_rtc", "fach_a5120", "fach_k8915", "fach_pc1715", "fach_prg710")
LANGSAME_LABELS = ("format_integration", "format_matrix", "wega_install")

_P8 = ("fach_p8000", "fach_wega")
_FLOPPY_WEIT = ("fach_floppy", "fach_kermit", "fach_p8000", "fach_wega")

# Reihenfolge = Vorrang: der ERSTE passende Eintrag entscheidet je Datei.
# (Muster, Fachgebiete oder ALLE, Begruendung, win?)  `**` = beliebig tief, `*` = ein Namensteil.
# win=None: aus dem Pfad ableiten (C++/CMake -> ja).
REGELN = [
    # ── nur Doku: nichts ────────────────────────────────────────────────
    ("**/*.md", (), "Doku", False),
    ("doc/**", (), "Doku", False),
    ("app/help/**", (), "Handbuch", False),
    ("LICENSE*", (), "Doku", False),
    (".gitignore", (), "Doku", False),
    (".gitattributes", (), "Doku", False),
    # ── Gemeinsames: alles ──────────────────────────────────────────────
    ("tools/dev.sh", ALLE, "Testtreiber", True),
    ("CMakeLists.txt", ALLE, "Bausystem", True),
    ("cmake/**", ALLE, "Bausystem", True),
    ("tests/CMakeLists.txt", ALLE, "Testsystem", True),
    ("tests/cmake/**", ALLE, "Testsystem", True),
    ("tests/support/**", ALLE, "Testhilfen", True),
    ("third_party/**", ALLE, "Fremdcode", True),
    ("core/bus/**", ALLE, "Bus", True),
    ("core/api/**", ALLE, "C-ABI", True),
    ("core/util/**", ALLE, "gemeinsame Hilfen", True),
    ("core/logger*", ALLE, "Logger", True),
    # ── core/primitives: zugeordnete, sonst alles ───────────────────────
    ("core/primitives/z80_sio*", ("fach_serial", "fach_kermit"), "SIO", True),
    ("core/primitives/z8000**", _P8, "Z8000", True),
    ("core/primitives/z8010**", _P8, "UB8010", True),
    ("core/primitives/z80_dma*", _P8 + ("fach_pc1715",), "Z80-DMA", True),
    ("core/primitives/i8275*", ("fach_pc1715",), "8275", True),
    ("core/primitives/i8279*", ("fach_prg710",), "8279", True),
    ("core/primitives/z8.*", ("fach_k8915", "fach_prg710") + _P8, "Z8 (Tastatur-/Terminalfirmware)", True),
    ("core/primitives/z8/**", ("fach_k8915", "fach_prg710") + _P8, "Z8", True),
    ("core/primitives/upd765*", ("fach_pc1715",), "uPD765", True),
    ("core/primitives/**", ALLE, "gemeinsamer Baustein", True),
    # ── Serie / Kermit ──────────────────────────────────────────────────
    ("core/serial/**", ("fach_serial", "fach_kermit"), "serielle Schnittstellen", True),
    ("kermit/**", ("fach_kermit",), "Kermit-Bibliothek", True),
    ("tools/kermit80/**", ("fach_kermit",), "Kermit-80", False),
    ("tools/k1520kermit*", ("fach_kermit",), "Kermit-CLI", True),
    ("app/kermittool/**", (), "Kermit-Oberflaeche", False),
    ("tools/sertest/**", ("fach_serial",), "SERTEST", False),
    # ── Diskette ────────────────────────────────────────────────────────
    ("core/peripherals/floppy_drive/**", _FLOPPY_WEIT, "Diskettenlaufwerk/-medium", True),
    ("core/cards/k5122/**", ("fach_floppy", "fach_kermit"), "K5122", True),
    ("core/filesystem/wega/**", ("fach_floppy",) + _P8, "WEGA-Dateisystem", True),
    ("core/filesystem/**", ("fach_floppy",), "Dateisysteme", True),
    ("data/formats.yaml", _FLOPPY_WEIT, "Formatkatalog", False),
    ("tests/system/drivers/**", ("fach_floppy",), "Formatiertreiber", False),
    ("tools/format_driver*", ("fach_floppy",), "Formatiertreiber", True),
    ("tools/disketten_beigaben.py", ("fach_floppy", "fach_kermit"), "Beigaben", False),
    ("disks/p8000_*/**", ("fach_p8000", "fach_wega", "fach_kermit"), "P8000-Disketten", False),
    ("disks/**", ("fach_floppy", "fach_kermit"), "Disketten", False),
    # ── P8000 / RTC ─────────────────────────────────────────────────────
    ("core/cards/p8000/rtc*", _P8 + ("fach_rtc",), "Uhrenkarte", True),
    ("core/cards/p8000/**", _P8, "P8000-Karten", True),
    ("core/machines/p8000/**", _P8, "P8000", True),
    ("core/peripherals/winchester/**", _P8, "Winchester", True),
    ("core/peripherals/p8000_terminal/**", _P8, "P8000-Terminal", True),
    ("tools/p8000/**", _P8 + ("fach_kermit",), "P8000-Werkzeuge", False),
    ("tools/osm_p8000/**", ("fach_kermit",), "OS/M", False),
    ("tests/fixtures/p8000/**", _P8, "P8000-Fixtures", False),
    # ── Maschinen (Formatier-Faelle der Maschine) ───────────────────────
    ("core/machines/a5120/**", ("fach_a5120",), "A5120", True),
    ("core/machines/k8915/**", ("fach_k8915",), "K8915", True),
    ("core/machines/pc1715/**", ("fach_pc1715",), "PC 1715", True),
    ("core/machines/prg710/**", ("fach_prg710",), "PRG 710", True),
    ("core/machines/mc80/**", (), "MC80 (keine langsamen Faelle)", True),
    ("core/cards/k8025/**", ("fach_a5120", "fach_serial"), "K8025", True),
    ("core/cards/k7028/**", ("fach_k8915", "fach_serial"), "K7028", True),
    ("core/cards/k2521/**", ("fach_k8915", "fach_prg710"), "K2521", True),
    ("core/cards/k3528/**", ("fach_k8915",), "K3528", True),
    ("core/cards/k3820/**", (), "MC80 (keine langsamen Faelle)", True),
    ("core/cards/zre8762/**", ("fach_k8915",), "ZRE 8762", True),
    ("core/cards/pc1715*/**", ("fach_pc1715",), "PC-1715-Karten", True),
    ("core/peripherals/p8000_terminal_hw/**", _P8, "P8000-Terminal", True),
    ("core/cards/prg710*/**", ("fach_prg710",), "PRG-710-Karten", True),
    ("core/cards/atp*/**", ("fach_prg710",), "ATP", True),
    ("core/cards/ass*/**", ("fach_prg710",), "ASS", True),
    ("core/cards/mc80/**", (), "MC80 (keine langsamen Faelle)", True),
    ("core/cards/k2526/**", ("fach_a5120",), "ZRE A5120", True),
    ("core/cards/k3525/**", ("fach_a5120",), "RAM", True),
    ("core/cards/k3526/**", ("fach_a5120",), "RAM", True),
    ("core/cards/k3521/**", ("fach_a5120",), "RAM", True),
    ("core/cards/k7024/**", ("fach_a5120",), "Bildschirm", True),
    ("core/cards/k7023/**", ("fach_a5120",), "Bildschirm", True),
    ("core/cards/em/**", ("fach_a5120",), "Erweiterungsmodul", True),
    ("core/cards/k5020/**", ("fach_a5120",), "Kassettenkarte", True),
    ("core/cards/raf/**", ("fach_a5120",), "RAM-Floppy", True),
    ("core/cards/k6022/**", ("fach_a5120",), "Lochstreifen", True),
    ("core/peripherals/kassette/**", ("fach_a5120",), "Kassette", True),
    ("core/peripherals/k7672*/**", ("fach_k8915", "fach_prg710"), "Tastatur K7672", True),
    ("core/peripherals/k7609/**", ("fach_prg710",), "Tastatur K7609", True),
    ("core/peripherals/k76[34]*/**", ("fach_a5120",), "Tastatur A5120", True),
    ("core/peripherals/tastatur1715/**", ("fach_pc1715",), "Tastatur PC 1715", True),
    ("core/peripherals/mc80*/**", (), "MC80 (keine langsamen Faelle)", True),
    ("tools/kassette/**", ("fach_a5120",), "Kassettenwerkzeuge", False),
    ("tools/em256/**", ("fach_a5120",), "EM256-Programme", False),
    ("tools/bcu_a5120/**", ("fach_a5120",), "BCU", False),
    ("tools/cpm_bau.py", ("fach_a5120", "fach_kermit", "fach_serial"), "CP/M-Bau", False),
    # ── Unbekanntes unter core/: alles ──────────────────────────────────
    ("core/**", ALLE, "unbekannter Pfad unter core/ (sicherheitshalber alles)", True),
    # ── Tests: nach Dateiname ───────────────────────────────────────────
    ("tests/system/test_p8000_*", _P8, "P8000-Systemtest", True),
    ("tests/system/test_*kermit*", ("fach_kermit",), "Kermit-Systemtest", True),
    ("tests/system/test_sertest*", ("fach_serial",), "SERTEST-Systemtest", True),
    ("tests/system/**", ALLE, "Systemtest (Zuordnung unbekannt)", True),
    ("tests/fixtures/**", ("fach_floppy", "fach_kermit"), "Fixtures", False),
    ("tests/**/*.cpp", (), "Test (schnelle Spur)", True),
    ("tests/**/*.h", (), "Testhilfe (schnelle Spur)", True),
    ("tests/**/CMakeLists.txt", ALLE, "Testregistrierung", True),
    ("tests/**", (), "Test (schnelle Spur)", False),
    # ── Rest: Oberflaeche, Pakete, Werkzeuge ────────────────────────────
    ("app/**", (), "Oberflaeche (Python-Ebene in `test`)", False),
    ("packaging/**", (), "Paketierung", False),
    ("tools/*.cpp", (), "Werkzeug (schnelle Spur)", True),
    ("tools/*.h", (), "Werkzeug (schnelle Spur)", True),
    ("tools/**", (), "Werkzeug (schnelle Spur)", False),
    ("data/**", (), "Daten (schnelle Spur)", False),
    ("*", (), "unbekannter Pfad ausserhalb core/ (nur `test`)", False),
]

_cache = {}


def _regex(muster):
    if muster in _cache:
        return _cache[muster]
    out, i = "", 0
    while i < len(muster):
        if muster.startswith("**/", i):
            out += "(?:.*/)?"; i += 3
        elif muster.startswith("**", i):
            out += ".*"; i += 2
        elif muster[i] == "*":
            out += "[^/]*"; i += 1
        else:
            out += re.escape(muster[i]) if muster[i] not in "[]" else muster[i]
            i += 1
    _cache[muster] = re.compile("^" + out + "$")
    return _cache[muster]


def regel_fuer(pfad):
    for muster, facher, grund, win in REGELN:
        if _regex(muster).match(pfad):
            if win is None:
                win = False
            return muster, facher, grund, win
    raise AssertionError(pfad)  # "*" faengt alles


def plan(dateien, alles=False):
    """-> dict(test, facher (set | ALLE), win, gruende [(pfad-gruppe, text)], nur_doku)."""
    p = {"test": False, "facher": set(), "win": False, "gruende": [], "nur_doku": False}
    if alles:
        p.update(test=True, facher=ALLE, win=True)
        p["gruende"].append(("--alles", "erzwungen"))
        return p
    if not dateien:
        p["gruende"].append(("-", "keine Aenderungen"))
        return p
    gruppen = {}
    nicht_doku = False
    for f in dateien:
        muster, facher, grund, win = regel_fuer(f)
        gruppen.setdefault((muster, grund), []).append(f)
        if grund not in ("Doku", "Handbuch"):
            nicht_doku = True
        if facher == ALLE or p["facher"] == ALLE:
            p["facher"] = ALLE
        else:
            p["facher"] |= set(facher)
        p["win"] = p["win"] or win
    p["nur_doku"] = not nicht_doku
    p["test"] = nicht_doku
    for (muster, grund), fs in gruppen.items():
        ex = fs[0] + (f" (+{len(fs) - 1})" if len(fs) > 1 else "")
        p["gruende"].append((ex, grund))
    return p


def geaenderte_dateien(basis):
    def git(*a):
        r = subprocess.run(["git", *a], capture_output=True, text=True)
        return r.returncode, r.stdout
    if basis is None:
        rc, zweig = git("rev-parse", "--abbrev-ref", "HEAD")
        basis = "origin/main" if zweig.strip() == "main" else "main"
    rc, _ = git("rev-parse", "--verify", "-q", basis)
    if rc != 0:
        sys.exit(f"Basis '{basis}' existiert nicht (git fetch? oder: test-merge <basis>)")
    namen = set()
    for a in (("diff", "--name-only", f"{basis}...HEAD"), ("diff", "--name-only", "HEAD"),
              ("ls-files", "-o", "--exclude-standard")):
        rc, out = git(*a)
        namen |= {z for z in out.splitlines() if z}
    return sorted(namen), basis


def beschreibung(p):
    z = []
    if p["nur_doku"]:
        z.append("nur Doku: nichts zu laufen")
    else:
        z.append("LAEUFT  test (schnelle Spur inkl. Python-Ebene)" if p["test"] else "UEBERSPRUNGEN test")
    if p["nur_doku"] or not p["test"]:
        pass
    elif p["facher"] == ALLE:
        z.append("LAEUFT  langsame Spuren: alle (format_integration, format_matrix, wega_install)")
    elif p["facher"]:
        z.append("LAEUFT  langsame Spuren gefiltert: " + ", ".join(sorted(p["facher"])))
    else:
        z.append("UEBERSPRUNGEN langsame Spuren: kein Fachgebiet beruehrt")
    if p["test"] and p["win"]:
        z.append("LAEUFT  win (C++/CMake geaendert: Pfad-/Portierungsfehler zeigt nur Windows)")
    elif p["test"]:
        z.append("UEBERSPRUNGEN win: kein C++/CMake geaendert")
    return z


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--basis")
    ap.add_argument("--alles", action="store_true")
    ap.add_argument("--dateien", nargs="*")
    ap.add_argument("--shell", action="store_true")
    a = ap.parse_args()
    basis = a.basis
    if a.dateien is not None:
        dateien = a.dateien
    elif a.alles:
        dateien = []
    else:
        dateien, basis = geaenderte_dateien(a.basis)
    p = plan(dateien, a.alles)
    out = sys.stderr if a.shell else sys.stdout
    print(f"Aenderungen: {len(dateien)} Datei(en)" + (f" gegen {basis}" if basis and not a.alles and a.dateien is None else ""), file=out)
    for ex, grund in p["gruende"]:
        print(f"  {ex}: {grund}", file=out)
    for z in beschreibung(p):
        print(z, file=out)
    if a.shell:
        fach = ALLE if p["facher"] == ALLE else "|".join(sorted(p["facher"]))
        print(f"PLAN_TEST={int(p['test'])}")
        print(f"PLAN_WIN={int(p['test'] and p['win'])}")
        print(f"PLAN_FACH='{fach}'")


if __name__ == "__main__":
    main()

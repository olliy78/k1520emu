#!/usr/bin/env python3
"""Quell-Archive der MUTOS-8000-Disketten erzeugen (Entwurf 39, 4.8 und 9.13).

--art wurzel (Vorgabe): Archiv der rekonstruierten Wurzel.  --art ladediskette: Archiv der UDOS-Ladediskette
(`dateien/` mit allen UDOS-Dateien und -Angaben, `systemspuren.bin`, `bauangaben.yaml`); danach baut
bau_ladediskette.py je Laufwerksvariante eine Diskette.

Wurzel:
Liest die (eingecheckte) rekonstruierte Diskette mit `python3 -m app.disktool.archive` aus:
dateien/ (+ .fileinfo, wega-dateiangaben.txt) samt Beilagen bauangaben.yaml (Text) und
zylinder0.bin (binaer, per `boot-get` von der Diskette; gleich dem von M8KCMD).
Ohne Abbild im Archiv (--ohne-abbild): ein angepasstes Archiv soll nicht neben einem veralteten
Abbild stehen; das Abbild entsteht neu mit bau_aus_archiv.py.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m8k  # noqa: E402

QUELLE = m8k.fertig("root.hfe")
ZIEL = m8k.disk("m8k_root.zip")
ANGABEN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bauangaben.yaml")
LADE_QUELLE = m8k.disk("boot_udos.dmk")
LADE_ZIEL = m8k.disk("boot_udos.zip")
LADE_ANGABEN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bauangaben_ladediskette.yaml")


def _disktool(*args):
    d = [os.path.join(m8k.REPO, "tools", "dev.sh"), "tool", "k1520disktool"] + list(args)
    r = subprocess.run(d, capture_output=True, text=True, cwd=m8k.REPO)
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit("k1520disktool %s fehlgeschlagen" % args[0])


def _leere_vermerke_ausschreiben(zip_pfad):
    """`erst=-` / `geaend=-` (leerer Datumsvermerk) in den UDOS-Angaben durch sechs Leerzeichen ersetzen.

    `get` schreibt einen leeren Vermerk als `-`, `put` liest `-` aber als "nicht angegeben" und setzt das
    heutige Datum ein: ein Bau waere dann von der Tageszeit abhaengig und wuerde sich vom Original unterscheiden.
    `______` (Unterstrich = Leerzeichen) stellt den leeren Vermerk bitgleich wieder her."""
    muster = re.compile(r"\b(erst|geaend)=-(?=\s|$)")
    tmp = zip_pfad + ".neu"
    with zipfile.ZipFile(zip_pfad) as zin, zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as zout:
        for info in zin.infolist():
            daten = zin.read(info.filename)
            if info.filename.endswith(".fileinfo") or info.filename.endswith("udos-dateiangaben.txt"):
                daten = muster.sub(lambda m: m.group(1) + "=______", daten.decode("utf-8")).encode("utf-8")
            zout.writestr(info, daten)
    os.replace(tmp, zip_pfad)


def erzeugen(abbild, ziel, force=False, art="wurzel"):
    lade = art == "ladediskette"
    tmp = tempfile.mkdtemp(prefix="m8k_archiv_")
    try:
        kopie = os.path.join(tmp, "quelle" + os.path.splitext(abbild)[1])   # nie die eingecheckte Datei anfassen
        shutil.copyfile(abbild, kopie)
        z0 = os.path.join(tmp, "systemspuren.bin" if lade else "zylinder0.bin")
        _disktool("boot-get", kopie, z0)
        angaben = os.path.join(tmp, "bauangaben.yaml")           # im Archiv heisst sie immer so
        shutil.copyfile(LADE_ANGABEN if lade else ANGABEN, angaben)
        cmd = [sys.executable, "-m", "app.disktool.archive", kopie, ziel, "--ohne-abbild",
               "--bezeichnung", "boot_udos" if lade else "m8k_root",
               "--beilage", angaben, "--beilage-binaer", z0,
               "--herkunft", "hinweise=" + (
                   "UDOS-Ladediskette fuer MUTOS 8000 (Entwurf 39, 9.13); bauen: tools/mutos8000/bau_ladediskette.py"
                   if lade else
                   "Rekonstruierte MUTOS-8000-Wurzel (Entwurf 39, 4.8); bauen: tools/mutos8000/bau_aus_archiv.py")]
        if force:
            cmd.append("--force")
        r = subprocess.run(cmd, cwd=m8k.REPO, env=dict(os.environ, PYTHONPATH=m8k.REPO))
        if r.returncode:
            raise SystemExit("archive fehlgeschlagen")
        if lade:
            _leere_vermerke_ausschreiben(ziel)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--art", choices=("wurzel", "ladediskette"), default="wurzel")
    ap.add_argument("--abbild", help="Quelldiskette (Vorgabe je --art die eingecheckte)")
    ap.add_argument("--ziel", help="Zielarchiv (Vorgabe je --art das eingecheckte)")
    ap.add_argument("--force", action="store_true", help="vorhandenes Archiv ersetzen")
    a = ap.parse_args(argv)
    lade = a.art == "ladediskette"
    erzeugen(a.abbild or (LADE_QUELLE if lade else QUELLE), os.path.abspath(a.ziel or (LADE_ZIEL if lade else ZIEL)),
             a.force, a.art)
    return 0


if __name__ == "__main__":
    sys.exit(main())

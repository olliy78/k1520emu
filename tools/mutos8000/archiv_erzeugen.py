#!/usr/bin/env python3
"""Quell-Archiv der rekonstruierten MUTOS-8000-Wurzel erzeugen (Entwurf 39, 4.8).

Liest die (eingecheckte) rekonstruierte Diskette mit `python3 -m app.disktool.archive` aus:
dateien/ (+ .fileinfo, wega-dateiangaben.txt) samt Beilagen bauangaben.yaml (Text) und
zylinder0.bin (binaer, per `boot-get` von der Diskette; gleich dem von M8KCMD).
Ohne Abbild im Archiv (--ohne-abbild): ein angepasstes Archiv soll nicht neben einem veralteten
Abbild stehen; das Abbild entsteht neu mit bau_aus_archiv.py.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m8k  # noqa: E402

QUELLE = m8k.disk("m8k_root_rekonstruiert.hfe")
ZIEL = m8k.disk("m8k_root.zip")
ANGABEN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bauangaben.yaml")


def erzeugen(abbild, ziel, force=False):
    tmp = tempfile.mkdtemp(prefix="m8k_archiv_")
    try:
        kopie = os.path.join(tmp, "m8k_root.hfe")      # nie die eingecheckte Datei anfassen
        shutil.copyfile(abbild, kopie)
        z0 = os.path.join(tmp, "zylinder0.bin")
        d = [os.path.join(m8k.REPO, "tools", "dev.sh"), "tool", "k1520disktool", "boot-get", kopie, z0]
        r = subprocess.run(d, capture_output=True, text=True, cwd=m8k.REPO)
        if r.returncode:
            sys.stderr.write(r.stdout + r.stderr)
            raise SystemExit("boot-get fehlgeschlagen")
        cmd = [sys.executable, "-m", "app.disktool.archive", kopie, ziel,
               "--bezeichnung", "m8k_root", "--ohne-abbild",
               "--beilage", ANGABEN, "--beilage-binaer", z0,
               "--herkunft", "hinweise=Rekonstruierte MUTOS-8000-Wurzel (Entwurf 39, 4.8); "
                             "bauen: tools/mutos8000/bau_aus_archiv.py"]
        if force:
            cmd.append("--force")
        r = subprocess.run(cmd, cwd=m8k.REPO, env=dict(os.environ, PYTHONPATH=m8k.REPO))
        if r.returncode:
            raise SystemExit("archive fehlgeschlagen")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--abbild", default=QUELLE)
    ap.add_argument("--ziel", default=ZIEL)
    ap.add_argument("--force", action="store_true", help="vorhandenes Archiv ersetzen")
    a = ap.parse_args(argv)
    erzeugen(a.abbild, os.path.abspath(a.ziel), a.force)
    return 0


if __name__ == "__main__":
    sys.exit(main())

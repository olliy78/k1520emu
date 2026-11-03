#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
build1715.py  -  die MicroDOS-1715-Systemdiskette bauen
=======================================================

  disks/pc1715_microdos/pc1715_microdos_k5601_system.hfe

MicroDOS 1715 (PC 1715, K5601: 80 Spuren, 2 Seiten, 5 x 1024 B).  Das Bootabbild
`data/bootsektoren/boot_microdos_1715.bin` ist der 128-B-Bootkopf (Verzeichnisplaetze 0-3) PLUS die
Systemdatei MICRODOS.SYS (14 336 B, die erste Datei der Diskette - der Urlader liest sie von den ersten
Datenbloecken).  `create --fs microdos1715 --boot` legt beides in einem Zug an; danach kommen die
Hilfsprogramme aus `inhalt1715/` (Herkunft: Originaldiskette MicroDOS1715.hfe der Sammlung Rostock/DL,
unveraendert) und das LIESMICH.TXT.

`MicroDOS1715_LW_C.hfe` der Sammlung ist NICHT enthalten: ihre Laufwerkstabelle in MICRODOS.SYS ist leer
(A: = "disk error"), siehe doc/merkposten/microdos1715.md.

Aufruf:
  python3 tools/microdos/build1715.py --tool build/k1520disktool            # bauen
  python3 tools/microdos/build1715.py --tool build/k1520disktool --check    # nur vergleichen
``--check`` ist der Waechter ``cli_microdos1715``.
"""

import argparse
import importlib.util
import os
import subprocess
import sys
import tempfile

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HIER))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import disketten_beigaben as _db  # noqa: E402  (Ordner je Diskette)

_spec = importlib.util.spec_from_file_location('cpa_a5120_build', os.path.join(REPO, 'tools', 'cpa_a5120', 'build.py'))
a5120 = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(a5120)

NAME = 'pc1715_microdos_k5601_system.hfe'
BOOT = os.path.join(REPO, 'data', 'bootsektoren', 'boot_microdos_1715.bin')
INHALT = os.path.join(HIER, 'inhalt1715')
LIESMICH = 'LIESMICH.TXT'
SYSTEMDATEI = 'MICRODOS.SYS'


def _lauf(tool, *args):
    r = subprocess.run([*tool, *args], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"Fehler: {' '.join([os.path.basename(tool[-1]), *args])}\n{r.stdout}{r.stderr}")
    return r.stdout


def _lies(*teile):
    with open(os.path.join(*teile), 'rb') as f:
        return f.read()


def soll():
    """{Diskettenname: Bytes} der Dateien AUSSER MICRODOS.SYS (die steckt im Bootabbild)."""
    inhalt = {}
    for n in sorted(os.listdir(INHALT)):
        inhalt[n] = _lies(INHALT, n)
    inhalt[LIESMICH] = a5120._cpm_text(inhalt[LIESMICH])
    return inhalt


def bauen(tool, ausgabe):
    ziel = _db.diskpfad(ausgabe, NAME)
    os.makedirs(os.path.dirname(ziel), exist_ok=True)
    if os.path.exists(ziel):
        os.remove(ziel)
    with tempfile.TemporaryDirectory() as tmp:
        quelle = os.path.join(tmp, 'q')
        os.makedirs(quelle)
        for n, d in soll().items():
            with open(os.path.join(quelle, n), 'wb') as f:
                f.write(d)
        _lauf(tool, 'create', ziel, '--fs', 'microdos1715', '--boot', BOOT)    # Kopf + MICRODOS.SYS
        _lauf(tool, 'put', ziel, quelle, '--no-backup')
    pruefung = _lauf(tool, 'check', ziel, '--full')
    if 'ohne Befund' not in pruefung:
        sys.exit(f'{NAME}: Dateisystempruefung hat Befunde:\n{pruefung}')
    print(f'{NAME}: gebaut, check --full ohne Befund')


def pruefen(tool, ausgabe):
    diskette = _db.diskpfad(ausgabe, NAME)
    if not os.path.isfile(diskette):
        print(f'{NAME}: fehlt')
        return False
    erwartet = soll()
    erwartet[SYSTEMDATEI] = _lies(BOOT)[128:]
    ok = True
    with tempfile.TemporaryDirectory() as tmp:
        ist_ordner = os.path.join(tmp, 'ist')
        os.makedirs(ist_ordner)
        _lauf(tool, 'get', diskette, '*', '--to', ist_ordner)
        ist = {n: _lies(ist_ordner, n) for n in os.listdir(ist_ordner)
               if n != 'cpm-dateiangaben.txt'}
        boot = os.path.join(tmp, 'boot.bin')
        _lauf(tool, 'boot-get', diskette, boot)
        boot_ok = _lies(boot) == _lies(BOOT)
    for n in sorted(set(erwartet) | set(ist)):
        if n not in ist:
            print(f'{NAME}: {n} fehlt auf der Diskette'); ok = False
        elif n not in erwartet:
            print(f'{NAME}: {n} steht auf der Diskette, aber nicht im Ordner'); ok = False
        elif erwartet[n] != ist[n]:
            print(f'{NAME}: {n} weicht ab'); ok = False
    if not boot_ok:
        print(f'{NAME}: Bootabbild (Kopf + MICRODOS.SYS) weicht ab'); ok = False
    if ok:
        print(f'{NAME}: ok ({len(erwartet)} Dateien)')
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--tool', required=True, help='k1520disktool')
    ap.add_argument('--emulator', action='append', default=[],
                    help='davor zu setzender Starter (Cross-Bau: wine; wiederholbar)')
    ap.add_argument('--out', default=os.path.join(REPO, 'disks'), help='Zielordner (Vorgabe: disks/)')
    ap.add_argument('--check', action='store_true', help='nur vergleichen, nichts schreiben')
    o = ap.parse_args()
    tool = [*o.emulator, os.path.abspath(o.tool)]
    if o.check:
        sys.exit(0 if pruefen(tool, o.out) else 1)
    bauen(tool, o.out)


if __name__ == '__main__':
    main()

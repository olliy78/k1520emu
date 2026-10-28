#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
build.py  -  die UDOS-5.0-Systemdiskette des A5120 aus dem Original umwandeln
=============================================================================

  disks/a5120_udos50/a5120_udos50_k5601_system.hfe

UDOS 5.0 fuer den A5120 (BIZ FmSZBw Schwerin, 03/1997, Startbild „UDOS_5.0“), K5601,
80 x 2 x 16 x 256.  Das Dateisystem ist NICHT das ZDOS des A5120 (Kontrollblock hinter der
Daten-CRC), sondern das NDOS des PC 1715 und der P8000: Zeigersektoren, Spur = ganzer Zylinder,
Verzeichnis 16H, Belegungsplan 17H (Profil `udos50` = Fuellsitte 0xFF hinter dem Plan,
doc/merkposten/disktool.md).  Deshalb ist das rohe Sektorabbild `original/a5120-udos5.img`
vollstaendig - sektorgleich mit der ebenfalls gelieferten `.dmk`; `original/a5120-udos5.txt`
ist das Verzeichnis, wie es der Absender aufgelistet hat.

Anders als bei UDOS 4.3 (tools/udos_a5120/) wird die Diskette NICHT aus Einzeldateien neu
aufgebaut, sondern 1:1 umgewandelt: so bleibt sie sektorgleich mit dem Original, und die leere
Datei `ERR1` (0 Records) bleibt erhalten, die `put` unter UDOS nicht ablegen kann.  Eine eigene
UDOS-5.0-Bootdiskette baut das DiskTool trotzdem (Katalog data/bootsektor.yaml, Abbild
data/bootsektoren/boot_udos50.bin; Bedingungen doc/udos1715_diskettenformat.md §3.0b).

Aufruf:
  python3 tools/udos50_a5120/build.py --tool build/k1520disktool            # umwandeln
  python3 tools/udos50_a5120/build.py --tool build/k1520disktool --check    # nur vergleichen

``--check`` ist der Waechter ``cli_udos50_a5120``: Exit 1, wenn die Diskette in disks/ nicht
sektorgleich mit dem Original ist oder die Dateisystempruefung etwas findet.
"""

import argparse
import os
import subprocess
import sys
import tempfile

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HIER))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import disketten_beigaben as _db  # noqa: E402  (Ordner je Diskette)

NAME = 'a5120_udos50_k5601_system.hfe'
ORIGINAL = os.path.join(HIER, 'original', 'a5120-udos5.img')


def _lauf(tool, *args):
    r = subprocess.run([*tool, *args], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"Fehler: {' '.join([os.path.basename(tool[-1]), *args])}\n{r.stdout}{r.stderr}")
    return r.stdout


def _pruefung(tool, diskette):
    """Vollpruefung; None = ohne Befund, sonst der Bericht."""
    bericht = _lauf(tool, 'check', diskette, '--full')
    return None if 'ohne Befund' in bericht else bericht


def bauen(tool, ausgabe):
    ziel = _db.diskpfad(ausgabe, NAME)
    os.makedirs(os.path.dirname(ziel), exist_ok=True)
    if os.path.exists(ziel):
        os.remove(ziel)
    _lauf(tool, 'save-as', ORIGINAL, ziel)
    befund = _pruefung(tool, ziel)
    if befund:
        sys.exit(f'{NAME}: Dateisystempruefung hat Befunde:\n{befund}')
    print(f'{NAME}: umgewandelt, check --full ohne Befund')


def pruefen(tool, ausgabe):
    diskette = _db.diskpfad(ausgabe, NAME)
    if not os.path.isfile(diskette):
        print(f'{NAME}: fehlt')
        return False
    with tempfile.TemporaryDirectory() as tmp:
        roh = os.path.join(tmp, 'ist.img')
        _lauf(tool, 'save-as', diskette, roh)
        gleich = open(roh, 'rb').read() == open(ORIGINAL, 'rb').read()
    if not gleich:
        print(f'{NAME}: weicht vom Original ab (tools/udos50_a5120/original/)')
        return False
    befund = _pruefung(tool, diskette)
    if befund:
        print(f'{NAME}: Dateisystempruefung hat Befunde:\n{befund}')
        return False
    print(f'{NAME}: ok (sektorgleich mit dem Original)')
    return True


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

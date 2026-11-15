#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
sound_karten.py  -  Gastsoftware der SOUND-Karte K1520 auf den Systemdisketten (Entwurf 51, AP S9a)
=================================================================================================

Welche Diskette welchen Satz traegt, steht HIER (eine Wahrheit).  Die Bauskripte der Disketten rufen
NICHT dieses Modul, sondern ``tools/gide/karten.py::dateien`` (dort wird :func:`dateien` eingemischt),
damit der bestehende Weg (cpa_a5120/scpx_a5120/cpa_pc1715 ``build.py`` -> ``soll()``) unveraendert bleibt.

Quellen (EINZIGE Wahrheit, bytegleich zu den unveraenderten Archiven in ``fremd/original/``; der Waechter
vergleicht das):
  fremd/a5120/   TGCPM, STCTEST1, PLAYSTC, SONG.STC aus K1520_SOUND_BC.zip; SKTEST4 im neueren Bau aus
                 rowikla_SKTEST4_BC/ (5250 B statt 5142 B im ZIP)
  fremd/pc1715/  die PC-1715-Fassungen aus K1520_SOUND_PC1715_CPA.zip (Vektor F7F0H, Karte an 38H)
Die Systemtests (``SoundSystem.*``) lesen dieselben Dateien: keine zweite Kopie unter tests/fixtures/.

Namen auf der Diskette = Dateinamen (8.3).  Waechter: ``cli_sound_beigaben`` (``--check``).
"""
import os
import sys
import zipfile

HIER = os.path.dirname(os.path.abspath(__file__))
FREMD = os.path.join(HIER, 'fremd')
ORIGINAL = os.path.join(FREMD, 'original')

DATEIEN = ('TGCPM.COM', 'SKTEST4.COM', 'STCTEST1.COM', 'PLAYSTC.COM', 'SONG.STC')

#: Schluessel (wie tools/gide/karten.py) -> (Quellordner unter fremd/, Dateien, Systemdiskette in disks/)
SAETZE = {
    'a5120_cpa':  ('a5120', DATEIEN, 'a5120_cpa_k5601_system.hfe'),
    'a5120_scpx': ('a5120', DATEIEN, 'a5120_scpx17_k5601_system.hfe'),
    'pc1715_cpa': ('pc1715', DATEIEN, 'pc1715_cpa1715_system.hfe'),
}


def _lies(*teile):
    with open(os.path.join(*teile), 'rb') as f:
        return f.read()


def _satz(daten):
    """CP/M speichert in 128-B-Saetzen; das DiskTool liefert den Rest mit 00 aufgefuellt zurueck.  Die Quellen
    bleiben unveraendert (Laenge wie im Archiv), erst die Diskettenfassung ist aufgefuellt."""
    return daten + b'\x00' * (-len(daten) % 128)


def dateien(schluessel):
    """{Diskettenname: Bytes} der SOUND-Gastsoftware fuer @p schluessel; leer, wenn die Diskette keine traegt."""
    if schluessel not in SAETZE:
        return {}
    ordner, namen, _ = SAETZE[schluessel]
    return {n: _satz(_lies(FREMD, ordner, n)) for n in namen}


def _quellen_pruefen():
    """Die entpackten Quellen sind bytegleich zu den Archiven (SKTEST4 A5120: dem rowikla-Bau)."""
    fehler = 0
    bc = zipfile.ZipFile(os.path.join(ORIGINAL, 'K1520_SOUND_BC.zip'))
    pc = zipfile.ZipFile(os.path.join(ORIGINAL, 'K1520_SOUND_PC1715_CPA.zip'))
    ordner_in_zip = {'TGCPM.COM': 'TGCPM/', 'STCTEST1.COM': 'STCTEST1/', 'PLAYSTC.COM': 'PLAYSTC/',
                     'SONG.STC': 'PLAYSTC/', 'SKTEST4.COM': 'SKTEST4/'}
    for n in DATEIEN:
        z = ordner_in_zip[n]
        erwartet_a = (_lies(ORIGINAL, 'rowikla_SKTEST4_BC', 'SKTEST4.COM') if n == 'SKTEST4.COM'
                      else bc.read(f'K1520_SOUND_BC/{z}{n}'))
        for ordner, erwartet in (('a5120', erwartet_a), ('pc1715', pc.read(f'K1520_SOUND_PC1715_CPA/{z}{n}'))):
            if _lies(FREMD, ordner, n) != erwartet:
                print(f'QUELLE  fremd/{ordner}/{n} weicht vom Archiv ab')
                fehler += 1
    if not fehler:
        print('aktuell fremd/a5120 + fremd/pc1715 (bytegleich zu den Archiven)')
    return fehler


def pruefen(tool, ordner):
    """Jede Systemdiskette traegt ihren Satz bytegleich; Rueckgabe = Zahl der Abweichungen."""
    import subprocess
    import tempfile
    sys.path.insert(0, os.path.dirname(HIER))
    import disketten_beigaben as db
    fehler = _quellen_pruefen()
    for schluessel, (_, _, name) in SAETZE.items():
        abbild = db.diskpfad(ordner, name)
        if not os.path.isfile(abbild):
            print(f'FEHLT   {name}')
            fehler += 1
            continue
        vorher = fehler
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run([*tool, 'get', abbild, '*', '--to', tmp], capture_output=True, text=True)
            for n, soll in dateien(schluessel).items():
                pfad = os.path.join(tmp, n)
                if not os.path.isfile(pfad):
                    print(f'OHNE    {name}: {n}')
                    fehler += 1
                elif _lies(pfad) != soll:
                    print(f'ALT     {name}: {n}')
                    fehler += 1
        if fehler == vorher:
            print(f'aktuell {name}')
    return fehler


def main():
    import argparse
    ap = argparse.ArgumentParser(description='SOUND-Gastsoftware auf den Systemdisketten pruefen')
    ap.add_argument('--tool', required=True, help='k1520disktool')
    ap.add_argument('--emulator', action='append', default=[])
    ap.add_argument('--ordner', default=os.path.join(os.path.dirname(os.path.dirname(HIER)), 'disks'))
    ap.add_argument('--check', action='store_true', required=True)
    a = ap.parse_args()
    fehler = pruefen([*a.emulator, a.tool], a.ordner)
    if fehler:
        print('Nachziehen: die Bauskripte der Systemdisketten (tools/cpa_a5120/build.py usw.) mit --tool aufrufen.')
    return 1 if fehler else 0


if __name__ == '__main__':
    sys.exit(main())

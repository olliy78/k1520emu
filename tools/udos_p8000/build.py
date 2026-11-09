#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
build.py  -  die UDOS-Disketten des P8000 (8-Bit-Teil) neu bauen
             (disks/p8000_udos30/, disks/p8000_udos31/; Ticket #30, 2026-11-09)

UDOS 3.0 und 3.1 (nicht 2.2!) liegen auf pofo.de (Olli Lehmann) als CopyQM-Abzuege in `original/`:

  udos_org.cqm   UDOS 3.0  Systemdiskette   ->  p8000_udos30/p8000_udos30_system.hfe      (bootet, mit Systemspuren)
  uspr_org.cqm   UDOS 3.0  Programmiersprachen (PLZ, BASIC, FORTRAN, PASCAL)  ->  p8000_udos30_sprachen.hfe
  udossys.cqm    UDOS 3.1  Systemdiskette   ->  p8000_udos31/p8000_udos31_system.hfe
  udosprog.cqm   UDOS 3.1  Programmiersprachen                                ->  p8000_udos31_sprachen.hfe

NICHT sektorgleich zum Abzug (das DiskTool legt die Dateien in anderer Reihenfolge ab), aber Datei fuer Datei gleich
(Inhalt UND Dateiangaben) und mit denselben Systemspuren: Urlader (Spur 0 Sektor 0), OSLOAD (Spur 16H Sektor 07-09)
und der Dateizugriffscode fuer GET/SAVE des Monitors (Spur 16H Sektor 12-15, Spur 17H Sektor 02-15) stammen aus
`data/bootsektoren/boot_p8000_udos3{0,1}.bin` (= `boot-get` der Systemdisketten; byteidentisch zu dem, was
`FORMAT D=1 ID='..' S` am Emulator auf eine Leerdiskette schreibt).  Das Verzeichnis (DIRECTORY) legt das DiskTool selbst an.

Der EPROM-Programmierer (UPROG) und die EPROM-Inhalte (MON8, P8T, WDC) liegen auf den Systemdisketten, nicht auf den Sprachendisketten.

  python3 tools/udos_p8000/build.py --tool build/k1520disktool            # bauen
  python3 tools/udos_p8000/build.py --tool build/k1520disktool --check    # Waechter cli_udos_p8000
"""
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import diskbau as db  # noqa: E402

FS = 'udos_p8000_ds80_16x256'
OHNE = ('DIRECTORY', 'DIRECTORY.fileinfo')


def _tool():
    """Der Pfad des DiskTools steht schon auf der Kommandozeile, bevor `main` ihn auswertet (soll() braucht ihn zum Auspacken)."""
    a = sys.argv
    return os.path.abspath(a[a.index('--tool') + 1])


def _inhalt(cqm):
    """{Name: Bytes} aller Dateien (samt `.fileinfo`) eines Abzugs, ohne das Verzeichnis."""
    with tempfile.TemporaryDirectory() as tmp:
        ziel = os.path.join(tmp, 'o')
        os.makedirs(ziel)
        r = subprocess.run([_tool(), 'get', os.path.join(db.HIER_ORIGINAL, cqm), '*', '--to', ziel],
                           capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f'{cqm}: Auspacken fehlgeschlagen\n{r.stdout}{r.stderr}')
        d = db.ordner(ziel, ausser=OHNE)
    # Dateien OHNE Erstellungsdatum (`erst=-`): `put` traegt das Aenderungsdatum ein, das ist hier die Sollangabe.
    for n in list(d):
        if n.endswith('.fileinfo') and b'\nerst=-\n' in d[n]:
            geaend = next(z for z in d[n].split(b'\n') if z.startswith(b'geaend='))[7:]
            d[n] = d[n].replace(b'\nerst=-\n', b'\nerst=' + geaend + b'\n')
    return d


def _diskette(name, ordner, boot, label):
    return db.Diskette(__file__, name, fs=FS, boot=boot, create_args=['--label', label], ignoriere=OHNE)


db.HIER_ORIGINAL = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'original')

UDOS30_SYS = _diskette('p8000_udos30_system.hfe', 'p8000_udos30', 'boot_p8000_udos30.bin', 'P8000_UDOS-SYSTEM')
UDOS30_SPR = _diskette('p8000_udos30_sprachen.hfe', 'p8000_udos30', None, 'P8000_UDOS-SPRACHEN')
UDOS31_SYS = _diskette('p8000_udos31_system.hfe', 'p8000_udos31', 'boot_p8000_udos31.bin', 'P8000_UDOS-SYSTEM')
UDOS31_SPR = _diskette('p8000_udos31_sprachen.hfe', 'p8000_udos31', None, 'P8000_UDOS-SPRACHEN')

if __name__ == '__main__':
    db.main_mehrere([
        (UDOS30_SYS, lambda: _inhalt('udos_org.cqm')),
        (UDOS30_SPR, lambda: _inhalt('uspr_org.cqm')),
        (UDOS31_SYS, lambda: _inhalt('udossys.cqm')),
        (UDOS31_SPR, lambda: _inhalt('udosprog.cqm')),
    ], __doc__)

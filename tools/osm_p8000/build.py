#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
build.py  -  die OS/M-3.0-Systemdiskette des P8000 (8-Bit-Teil) bauen   (disks/p8000_osm30/p8000_osm30_system.hfe)

OS/M 3.0 (ZFT/KEAW, 15.01.1988) ist CP/M 2.2 auf den SCP-Formaten; Auslieferungsformat 5 (80 x 2 x 16 x 256, 640K, Profil `osm640`).
`data/bootsektoren/boot_p8000_osm30.bin` = die vier Systemspuren (Urlader OS/M, Zylinder 0 und Anfang Zylinder 1; ohne diesen Urlader erkennt der Kern die
Diskette nicht als OS/M).  `inhalt/` = die 32 Dateien der Systemdiskette der pofo-Sammlung (osm30), dazu LIESMICH.TXT und KERMIT.COM.

  python3 tools/osm_p8000/build.py --tool build/k1520disktool            # bauen
  python3 tools/osm_p8000/build.py --tool build/k1520disktool --check    # Waechter cli_osm_p8000
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import diskbau as db  # noqa: E402

DISK = db.Diskette(__file__, 'p8000_osm30_system.hfe', fs='osm640', boot='boot_p8000_osm30.bin')
HIER = DISK.hier


def soll():
    d = db.ordner(os.path.join(HIER, 'inhalt'))
    d['LIESMICH.TXT'] = db.liesmich(d['LIESMICH.TXT'])
    d.update(db.kermit80('p8000'))
    return d


if __name__ == '__main__':
    DISK.main(soll, __doc__)

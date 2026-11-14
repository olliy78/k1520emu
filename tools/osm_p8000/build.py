#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
build.py  -  die OS/M-Systemdisketten des P8000 (8-Bit-Teil) bauen
             (disks/p8000_osm30/p8000_osm30_system.hfe, disks/p8000_osm20/p8000_osm20_system.hfe)

OS/M (ZFT/KEAW) ist CP/M 2.2 auf den SCP-Formaten; Auslieferungsformat 5 (80 x 2 x 16 x 256, 640K, Profil `osm_ds80_16x256_boot`).
`data/bootsektoren/boot_p8000_osm<nn>.bin` = die vier Systemspuren (Urlader OS/M, Zylinder 0 und Anfang Zylinder 1; ohne diesen Urlader
erkennt der Kern die Diskette nicht als OS/M).

  OS/M 3.0 (15.01.1988)  `inhalt/`        = die 32 Dateien der Systemdiskette der pofo-Sammlung (osm30), dazu LIESMICH.TXT und KERMIT.COM.
  OS/M 2.0 (15.04.1987)  `inhalt_osm20/`  = die 28 Dateien der Systemdiskette `osm20` (Anwender, 2026-11-08), dazu LIESMICH.TXT.
                         Dieser Urlader prueft vor dem Start die Leitungen der Konsole (/DCD an tty1); Waechter dafuer:
                         P8000Osm20.*  (tests/integration).  Kein KERMIT.COM: Kermit-80 ist dort nicht am Gast geprueft.

  python3 tools/osm_p8000/build.py --tool build/k1520disktool            # bauen
  python3 tools/osm_p8000/build.py --tool build/k1520disktool --check    # Waechter cli_osm_p8000
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import diskbau as db  # noqa: E402

OSM30 = db.Diskette(__file__, 'p8000_osm30_system.hfe', fs='osm_ds80_16x256_boot', boot='boot_p8000_osm30.bin')
OSM20 = db.Diskette(__file__, 'p8000_osm20_system.hfe', fs='osm_ds80_16x256_boot', boot='boot_p8000_osm20.bin')
HIER = OSM30.hier


def soll_osm30():
    d = db.ordner(os.path.join(HIER, 'inhalt'))
    d['LIESMICH.TXT'] = db.liesmich(d['LIESMICH.TXT'])
    d.update(db.kermit80('p8000'))
    return d


def soll_osm20():
    d = db.ordner(os.path.join(HIER, 'inhalt_osm20'))
    d['LIESMICH.TXT'] = db.liesmich(d['LIESMICH.TXT'])
    return d


if __name__ == '__main__':
    db.main_mehrere([(OSM30, soll_osm30), (OSM20, soll_osm20)], __doc__)

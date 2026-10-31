#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
build.py  -  die SIOS-1526-Beispieldisketten des A5120 aus den Originalen ableiten
===================================================================================

  disks/a5120_sios1526/a5120_sios11_3xk5601_seriell.hfe        SIOS 1.1, Vorgabe-Bestueckung (SYLs)
  disks/a5120_sios1526/a5120_sios11_3xk5601_seriell_sl1.hfe    SIOS 1.1, kuerzere Generierung (SL1_1)
  disks/a5120_sios1526/a5120_sios09_3xk5601_parallel.hfe       SIOS 0.9, 80 Zylinder einseitig (auch K5600.20)
  disks/a5120_sios1526/a5120_sios09_3xk5600_10_parallel.hfe    SIOS 0.9, 40-Spur-Fassung
  disks/a5120_sios1526/a5120_sios_programme1.hfe               DP1  (PHASELB: CATM ... CODP)
  disks/a5120_sios1526/a5120_sios_programme2.hfe               DP2  (PHASELB: LINK ... SORD)
  disks/a5120_sios1526/a5120_sios_generierung.hfe              SG1  (Generiermodule SYSORG09 + SGEN)

Namensschema der Systemdisketten: ``a5120_sios<version>_<anzahl>x<laufwerk>_<tastatur>.hfe``
(Bestueckung, fuer die die Diskette gebaut ist).  Die Daten- und Programmdisketten tragen keine
Bestueckung im Namen: SIOS doppelschrittet 40-Spur-Disketten im K5601 selbst.

Herkunft: tools/sios_a5120/original/ (README dort).  Die Disketten 1.1/DP1/DP2/SG1 sind die
Originale BYTEGLEICH; 0.9 entsteht aus SIOS1526_SYLp.hfe (80x2 ausgelesene 40-Spur-Diskette)
durch ``sylp_bereinigen.py`` (Seite 0, Zylinder 0..39, Zellenstroeme unveraendert; Kopf der DP1; die
80-Zylinder-Fassung mit den leeren Spuren 40..79 der SYLs).  Das Original selbst passt zu keinem
Katalogformat - jede gelieferte Diskette muss das DiskTool OHNE Zusatzangabe erkennen (``--check``).

``SIOS1526_SL1_1.img`` (rohes Sektorabbild, 80 x 26 x 128) wird mit ``k1520disktool save-as`` in
eine ``.hfe`` umgewandelt; dabei gehen die F8-Marken der Etikettenspur verloren (der Boot stoert das
nicht, Waechter ``SiosVorgabe.*``).

Aufruf:
  python3 tools/sios_a5120/build.py --tool build/k1520disktool            # bauen (idempotent)
  python3 tools/sios_a5120/build.py --tool build/k1520disktool --check    # nur vergleichen (cli_sios_a5120)
  --out ORDNER                                                            # statt disks/
"""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HIER))
ORIG = os.path.join(HIER, 'original')
sys.path.insert(0, HIER)
import sylp_bereinigen  # noqa: E402

ORDNER = 'a5120_sios1526'

#: Name der Diskette -> (Quelle im Ordner original/, Umwandlung)
DISKETTEN = {
    'a5120_sios11_3xk5601_seriell.hfe': ('SIOS1526_SYLs.hfe', None),
    'a5120_sios11_3xk5601_seriell_sl1.hfe': ('SIOS1526_SL1_1.img', 'img'),
    'a5120_sios09_3xk5601_parallel.hfe': ('SIOS1526_SYLp.hfe', 'sylp80'),
    'a5120_sios09_3xk5600_10_parallel.hfe': ('SIOS1526_SYLp.hfe', 'sylp40'),
    'a5120_sios_programme1.hfe': ('SIOS1526_DP1.hfe', None),
    'a5120_sios_programme2.hfe': ('SIOS1526_DP2.hfe', None),
    'a5120_sios_generierung.hfe': ('SIOS1526_SG1.hfe', None),
}


def _erzeuge(name, ziel, tool):
    quelle, wie = DISKETTEN[name]
    q = os.path.join(ORIG, quelle)
    if wie is None:
        shutil.copyfile(q, ziel)
    elif wie == 'img':
        r = subprocess.run([*tool, 'save-as', q, ziel], capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f'Fehler: save-as {quelle}\n{r.stdout}{r.stderr}')
    elif wie == 'sylp80':
        sylp_bereinigen.main80(q, os.path.join(ORIG, 'SIOS1526_SYLs.hfe'), ziel)
    else:  # 'sylp40': Kopf der DP1 als Vorlage
        sylp_bereinigen.main(q, os.path.join(ORIG, 'SIOS1526_DP1.hfe'), ziel)


def _sha(pfad):
    with open(pfad, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def bauen(out, tool):
    ordner = os.path.join(out, ORDNER)
    os.makedirs(ordner, exist_ok=True)
    for name in DISKETTEN:
        ziel = os.path.join(ordner, name)
        _erzeuge(name, ziel, tool)
        print(f'{name}: {_sha(ziel)[:12]}')


def pruefen(out, tool):
    ok = True
    ordner = os.path.join(out, ORDNER)
    with tempfile.TemporaryDirectory() as tmp:
        for name in DISKETTEN:
            ist = os.path.join(ordner, name)
            if not os.path.isfile(ist):
                print(f'{name}: fehlt')
                ok = False
                continue
            soll = os.path.join(tmp, name)
            _erzeuge(name, soll, tool)
            if _sha(ist) != _sha(soll):
                print(f'{name}: weicht vom Bau aus original/ ab')
                ok = False
                continue
            r = subprocess.run([*tool, 'info', ist], capture_output=True, text=True)
            if r.returncode != 0 or 'Dateisystem: sios_' not in r.stdout:
                print(f'{name}: vom DiskTool nicht erkannt\n{r.stdout}{r.stderr}')
                ok = False
                continue
            print(f'{name}: ok')
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
        sys.exit(0 if pruefen(o.out, tool) else 1)
    bauen(o.out, tool)


if __name__ == '__main__':
    main()

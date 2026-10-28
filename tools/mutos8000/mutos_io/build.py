#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
build.py  -  Bau von mutosio.bin (= UDOS-Datei mutos_io, U880-Seite von MUTOS 8000)
===================================================================================

Assembliert src/mutosio.mac mit M80 + LINKMT (ueber cparun aus der CPA_Workbench,
CPA_TOOLS=<pfad>) - gemeinsamer Bau in tools/cpm_bau.py.  Das Ergebnis ist die
Datei so, wie sie auf der Ladediskette liegt (vier Segmente, jedes auf 256 B
mit FFH aufgefuellt); die Angaben der .fileinfo (segs= usw.) kommen aus
`fileinfo.py`.  Vergleich mit dem Original: --check.

  python3 tools/mutos8000/mutos_io/build.py            # baut mutosio.bin
  python3 tools/mutos8000/mutos_io/build.py --out <datei>
  python3 tools/mutos8000/mutos_io/build.py --check    # 0 gleich, 1 verschieden, 77 ohne Werkzeugkette
"""
import os
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HIER)))
import cpm_bau  # noqa: E402

PROGRAMME = [cpm_bau.Programm('mutosio', HIER, endung='bin')]

if __name__ == '__main__':
    sys.exit(cpm_bau.hauptprogramm(PROGRAMME, __doc__))

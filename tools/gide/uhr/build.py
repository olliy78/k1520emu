#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
build.py  -  Build-Skript fuer uhr.com (Uhr der GIDE-Karte, RTC 72421)
=====================================================================

Assembliert src/uhr.mac mit M80 + LINKMT (ueber cparun aus der CPA_Workbench,
CPA_TOOLS=<pfad>) zu uhr.com - gemeinsamer Bau in tools/cpm_bau.py.

  python3 tools/gide/uhr/build.py            # baut, Ergebnis tools/gide/uhr/uhr.com
  python3 tools/gide/uhr/build.py --check    # bytegleich mit der eingecheckten .com?
                                             # (0 gleich, 1 verschieden, 77 ohne Werkzeugkette)

Die .com ist eingecheckt (die CI hat die CPA_Workbench nicht).  Danach die
Disketten nachziehen: tools/gide/README.md (Abschnitt Disketten).
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
import cpm_bau  # noqa: E402

PROGRAMME = [cpm_bau.Programm('uhr', os.path.dirname(os.path.abspath(__file__)))]

if __name__ == '__main__':
    sys.exit(cpm_bau.hauptprogramm(PROGRAMME, __doc__))

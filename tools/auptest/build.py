#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
build.py  -  Build-Skript fuer auptest.com (AUP 062-8531 Tastaturteil, A5120/K8924)
===================================================================================

Assembliert src/auptest.mac und src/auptsio.mac (Sonderschnittstelle, N10d) mit M80 + LINKMT (ueber cparun aus der
CPA_Workbench, CPA_TOOLS=<pfad>) zu auptest.com bzw. auptsio.com — gemeinsamer Bau in
tools/cpm_bau.py.

  python3 tools/auptest/build.py            # baut, Ergebnis tools/auptest/auptest.com
  python3 tools/auptest/build.py clean      # leert build/
  python3 tools/auptest/build.py --out <datei>   # temporaer bauen, nach <datei>
  python3 tools/auptest/build.py --check    # bytegleich mit der eingecheckten .com?
                                            # (0 gleich, 1 verschieden, 77 ohne Werkzeugkette)

Die .com ist eingecheckt (die CI hat die CPA_Workbench nicht).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import cpm_bau  # noqa: E402

_HIER = os.path.dirname(os.path.abspath(__file__))
PROGRAMME = [cpm_bau.Programm('auptest', _HIER),
             cpm_bau.Programm('auptsio', _HIER)]   # N10d: Sonderschnittstelle

if __name__ == '__main__':
    sys.exit(cpm_bau.hauptprogramm(PROGRAMME, __doc__))

#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
build.py  -  Build-Skript fuer k3521tst.com (Pruefprogramm K3521.30, A5120/K8924)
================================================================================

Assembliert src/k3521tst.mac mit M80 + LINKMT (ueber cparun aus der CPA_Workbench,
CPA_TOOLS=<pfad>) zu k3521tst.com — gemeinsamer Bau in tools/cpm_bau.py.

  python3 tools/k3521tst/build.py            # baut, Ergebnis tools/k3521tst/k3521tst.com
  python3 tools/k3521tst/build.py clean      # leert build/
  python3 tools/k3521tst/build.py --out <datei>   # temporaer bauen, nach <datei>
  python3 tools/k3521tst/build.py --check    # bytegleich mit der eingecheckten .com?
                                             # (0 gleich, 1 verschieden, 77 ohne Werkzeugkette)

Die .com ist eingecheckt (die CI hat die CPA_Workbench nicht).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import cpm_bau  # noqa: E402

_HIER = os.path.dirname(os.path.abspath(__file__))
PROGRAMME = [cpm_bau.Programm('k3521tst', _HIER)]

if __name__ == '__main__':
    sys.exit(cpm_bau.hauptprogramm(PROGRAMME, __doc__))

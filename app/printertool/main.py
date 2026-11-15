#!/usr/bin/env python3
"""
K1520 Printer
=============

Hardware-Emulator der Nadeldrucker der K1520-Familie (SD 1157/264): die Original-Firmware laeuft auf
dem Z80 des Druckers, das Blatt entsteht Punkt fuer Punkt.  Serielle Schnittstelle per Telnet oder
RFC 2217 wie bei den Rechner-Emulatoren; Ausgabe als PDF oder ueber die Druckfunktion des Systems.

Aufruf:
    bash run_k1520printer.sh
    python3 app/printertool/main.py [--instance NAME] [--config DATEI] [--paths]
"""

import os
import signal
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

_HILFE = """k1520printer - Emulator der Nadeldrucker SD 1157 (Ticket #28)

  k1520printer                    Fenster oeffnen
  k1520printer --instance NAME    eigene Konfiguration (k1520printer-NAME.yaml), mehrere Fenster nebeneinander
  k1520printer --config DATEI     Konfigurationsdatei von Hand festlegen
  k1520printer --paths            aufgeloeste Pfade zeigen (Bibliothek, Vorgaben)
  k1520printer --help             diese Hilfe
"""

# --help/--paths VOR den Qt- und Bindungs-Importen: die Auskunft kommt auch, wenn PySide6 oder die
# Kernbibliothek fehlen - wonach gefragt wird, ist dann ja gerade das Fehlende.
_rest = []
_args = iter(sys.argv[1:])
for _arg in _args:
    if _arg in ("--instance", "--config"):
        _wert = next(_args, "")
        if not _wert:
            print(f"{_arg} braucht einen Wert", file=sys.stderr)
            sys.exit(2)
        os.environ["K1520_INSTANZ" if _arg == "--instance" else "K1520_KONFIG"] = _wert
    elif _arg.startswith(("--instance=", "--config=")):
        _name, _wert = _arg.split("=", 1)
        os.environ["K1520_INSTANZ" if _name == "--instance" else "K1520_KONFIG"] = _wert
    else:
        _rest.append(_arg)
sys.argv[1:] = _rest

if "--paths" in sys.argv[1:]:
    from app import paths
    print(paths.describe())
    sys.exit(0)
if "--help" in sys.argv[1:] or "-h" in sys.argv[1:]:
    print(_HILFE)
    sys.exit(0)
for _arg in sys.argv[1:]:
    print(f"k1520printer: unbekannte Option '{_arg}' - `--help` zeigt die Bedienung", file=sys.stderr)
    sys.exit(2)

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication

from app.ui_dateidialoge import qt_dateidialoge


def main() -> int:
    qt_dateidialoge()
    app = QApplication(sys.argv)
    app.setOrganizationName("k1520emu")
    app.setApplicationName("K1520 Printer")
    from app import version as _version
    app.setApplicationVersion(_version.fassung())
    signal.signal(signal.SIGINT, lambda *_: app.quit())
    ticker = QTimer()                      # Ctrl+C im Terminal: Qt kehrt sonst nie nach Python zurueck
    ticker.start(200)
    ticker.timeout.connect(lambda: None)

    from app.printertool.fenster import PrinterWindow
    try:
        fenster = PrinterWindow()
    except Exception as e:                 # noqa: BLE001 - Startabbruch des Kerns: Meldung unveraendert
        print(f"Failed to start printer emulator: {e}", file=sys.stderr)
        return 1
    fenster.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())

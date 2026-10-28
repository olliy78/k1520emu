#!/usr/bin/env python3
"""
K1520 Kermit
============

Dateiaustausch zwischen dem PC und einer Kermit-Gegenstelle (WEGA/P8000, C-Kermit) ueber Telnet
oder RFC 2217 — als Client UND als Server.  Oberflaeche zu ``libk1520kermit``.

Aufruf:
    bash run_kermit.sh
    python3 app/kermittool/main.py [--paths]

Die Kommandozeilenfassung desselben Unterbaus ist ``k1520kermit`` (``tools/k1520kermit.md``).
"""

import signal
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

# --paths VOR den Qt- und Bindungs-Importen: die Auskunft soll auch dann kommen, wenn genau das
# fehlt, wonach gefragt wird (Bibliothek, PySide6) — wie bei den Nachbarprogrammen.
if "--paths" in sys.argv[1:]:
    from app import paths
    print(paths.describe())
    sys.exit(0)

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication
from app.ui_dateidialoge import qt_dateidialoge

from app.kermittool.ui.main_window import MainWindow


def main() -> int:
    qt_dateidialoge()               # Qts eigene Dateidialoge (app/ui_dateidialoge.py)
    app = QApplication(sys.argv)
    app.setOrganizationName("k1520emu")
    app.setApplicationName("K1520 Kermit")
    signal.signal(signal.SIGINT, lambda *_: app.quit())
    ticker = QTimer()                      # Ctrl+C im Terminal: Qt kehrt sonst nie nach Python zurueck
    ticker.start(200)
    ticker.timeout.connect(lambda: None)

    from app import paths
    dateiordner = paths.ensure_user_files_dir()
    ordner = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else (
        str(dateiordner) if dateiordner is not None else None)
    fenster = MainWindow(ordner=ordner)
    fenster.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())

"""Dateidialoge: immer Qts eigener Dialog, nie der des Desktops.

Unter GNOME zeigte Qt fuer *Neue Diskette* und *Speichern unter…* den GTK-Dateiwaehler so,
dass sich nur VORHANDENE Dateien waehlen liessen — ein neuer Name war nicht einzugeben.
Qts eigener Dialog verhaelt sich auf jedem Desktop und unter Windows gleich (und genau so,
wie ihn die Tests unter ``QT_QPA_PLATFORM=offscreen`` sehen).

Gilt fuer alle drei Programme (Emulator, DiskTool, Kermit).  Das Attribut muss VOR dem
Anlegen der ``QApplication`` gesetzt werden; danach ist es wirkungslos.
"""

from PySide6.QtCore import QCoreApplication, Qt


def qt_dateidialoge() -> None:
    """Systemeigene Dialoge abschalten — vor ``QApplication(...)`` aufrufen."""
    QCoreApplication.setAttribute(Qt.ApplicationAttribute.AA_DontUseNativeDialogs, True)

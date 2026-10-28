"""Alle drei Programme benutzen Qts eigene Dateidialoge (app/ui_dateidialoge.py).

Anlass: unter GNOME liess der systemeigene GTK-Dialog bei *Neue Diskette* und
*Speichern unter…* nur vorhandene Dateien waehlen.  Das Attribut wirkt nur, wenn es VOR
``QApplication(...)`` gesetzt ist — deshalb prueft der Waechter die Reihenfolge im Quelltext.
"""

import ast

import pytest

from conftest import PROJECT_ROOT

pytest.importorskip("PySide6")

STARTDATEIEN = ["app/main.py", "app/disktool/main.py", "app/kermittool/main.py"]


def _aufrufe_in_main(pfad):
    """Namen der Funktionsaufrufe in ``main()`` in Quelltextreihenfolge."""
    baum = ast.parse((PROJECT_ROOT / pfad).read_text(encoding="utf-8"))
    main = next(n for n in baum.body if isinstance(n, ast.FunctionDef) and n.name == "main")
    aufrufe = []
    for n in ast.walk(main):
        if isinstance(n, ast.Call):
            f = n.func
            name = f.id if isinstance(f, ast.Name) else getattr(f, "attr", "")
            aufrufe.append((n.lineno, n.col_offset, name))
    return [name for _, _, name in sorted(aufrufe)]


@pytest.mark.parametrize("pfad", STARTDATEIEN)
def test_qt_dateidialoge_vor_der_qapplication(pfad):
    namen = _aufrufe_in_main(pfad)
    assert "qt_dateidialoge" in namen, f"{pfad}: qt_dateidialoge() fehlt in main()"
    assert "QApplication" in namen, f"{pfad}: keine QApplication in main()"
    assert namen.index("qt_dateidialoge") < namen.index("QApplication"), \
        f"{pfad}: qt_dateidialoge() muss VOR QApplication(...) stehen"


def test_qt_dateidialoge_setzt_das_attribut():
    from PySide6.QtCore import QCoreApplication, Qt
    from app.ui_dateidialoge import qt_dateidialoge
    qt_dateidialoge()
    assert QCoreApplication.testAttribute(Qt.ApplicationAttribute.AA_DontUseNativeDialogs)

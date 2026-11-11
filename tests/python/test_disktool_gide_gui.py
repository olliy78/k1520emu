"""GIDE-Platte in der Oberflaeche des DiskTool (Poppe / Kingstener).

Gearbeitet wird an einer KOPIE des Golden-Fixtures (`tests/fixtures/gide/poppe_cpa.img.gz`, vom Poppe-Treiber
beschrieben) bzw. an selbst angelegten Abbildern.  Doku: doc/merkposten/disktool.md (GIDE-Platten).
"""

import shutil
from pathlib import Path

import pytest

pytest.importorskip("PySide6")

FIXTURE = Path(__file__).resolve().parents[1] / "fixtures" / "gide" / "poppe_cpa.img.gz"


@pytest.fixture(scope="module")
def qt_app():
    from PySide6.QtWidgets import QApplication
    yield QApplication.instance() or QApplication([])


@pytest.fixture
def window(qt_app):
    from app.disktool.ui.main_window import MainWindow
    w = MainWindow()
    yield w
    w._close_tool()


@pytest.fixture
def golden(tmp_path):
    if not FIXTURE.exists():
        pytest.skip("Golden-Fixture fehlt")
    ziel = tmp_path / "poppe.img.gz"
    shutil.copyfile(FIXTURE, ziel)
    return str(ziel)


@pytest.fixture
def offen(window, golden):
    assert window.open_image(golden)
    window.tool.set_backup(False)
    return window


def _item(baum, ref):
    from app.disktool.ui.disk_view import REF_ROLE
    stapel = [baum.topLevelItem(i) for i in range(baum.topLevelItemCount())]
    while stapel:
        i = stapel.pop()
        if i.data(0, REF_ROLE) == ref:
            return i
        stapel += [i.child(k) for k in range(i.childCount())]
    return None


def _waehle(fenster, ref):
    baum = fenster.disk_view.tree
    baum.clearSelection()
    i = _item(baum, ref)
    assert i is not None, ref
    i.setSelected(True)
    baum.setCurrentItem(i)


def test_oeffnen_erkennt_das_layout_ohne_frage(window, golden, monkeypatch):
    from app.core_binding.k1520disk import GideTool
    monkeypatch.setattr(type(window), "_gide_layout_fragen",
                        lambda self, grund: pytest.fail("bei eindeutigem Befund darf nicht gefragt werden"))
    assert window.open_image(golden)
    assert isinstance(window.tool, GideTool)
    assert window.tool.layout == "poppe" and window.tool.read_only
    assert "GIDE" in window.kopf.angaben.text() and "poppe" in window.kopf.angaben.text()
    assert "lw0" in window.kopf.angaben.text() and "lw1" in window.kopf.angaben.text()
    assert not window.kopf.fs_wahl.isEnabled()
    assert "schreibgeschützt" in window.info_bar.text() and "unverträglich" in window.info_bar.text()


def test_baum_laufwerke_als_oberste_ordner(offen):
    baum = offen.disk_view.tree
    from app.disktool.ui.disk_view import DIR_ROLE, REF_ROLE
    oben = [baum.topLevelItem(i).data(0, REF_ROLE) for i in range(baum.topLevelItemCount())]
    assert oben == ["lw0", "lw1"]
    lw1 = _item(baum, "lw1")
    assert lw1.data(0, DIR_ROLE) and "KB frei" in lw1.text(0)
    assert _item(baum, "lw1/HALLO.TXT").parent() is lw1
    assert [baum.headerItem().text(i) for i in range(baum.columnCount())] == ["Name", "Nutzer", "Größe", "Attribute"]
    assert "GIDE" in offen.disk_view.titel.text()


def test_gesperrte_und_eigene_aktionen(offen):
    w = offen
    assert not w.act_diskeditor.isEnabled() and "GIDE" in w.act_diskeditor.toolTip()
    assert not w.act_eigenschaften.isEnabled()
    assert not w.act_platte_ordner.isVisible()                      # CP/M kennt keine Ordner
    assert w.act_gide_pruefen.isVisible() and w.act_gide_pruefen.isEnabled()
    assert w.act_gide_format.isVisible() and not w.act_gide_format.isEnabled()   # schreibgeschuetzt
    w.set_read_only(False)
    assert w.act_gide_format.isEnabled()
    assert "beschreibbar" in w.info_bar.text()


def test_holen_bitgleich_und_ablegen(offen, tmp_path):
    w = offen
    meldungen = []
    w._fehler = lambda titel, text: meldungen.append((titel, text))
    ziel = tmp_path / "raus"
    ziel.mkdir()
    assert w._platte_holen(["lw1/HALLO.TXT"], ziel)
    assert (ziel / "lw1" / "HALLO.TXT").read_bytes().startswith(b"Hallo von Poppe, geschrieben im Emulator.\r\n")
    # ablegen: ins Laufwerk lw1 (angeklickt), Schreibschutz erst aufheben
    quelle = tmp_path / "neu.txt"
    quelle.write_bytes(b"vom DiskTool\r\n")
    assert not w._platte_einfuegen([str(quelle)], "lw1")          # schreibgeschuetzt
    assert "schreibgeschützt" in meldungen[-1][1]
    w.set_read_only(False)
    assert w._platte_einfuegen([str(quelle)], "lw1")
    assert w.tool.read("lw1/NEU.TXT").startswith(b"vom DiskTool\r\n")
    assert w.tool.dirty
    assert w.save()
    assert not w.tool.dirty
    assert w.erase_refs(["lw1/NEU.TXT"])
    assert not w.erase_refs(["lw1"])                                # ein Laufwerk laesst sich nicht loeschen
    assert "Laufwerk" in meldungen[-1][1]


def test_schreibschutz_aufheben_scheitert_bei_fremder_sperre(window, golden):
    import os
    Path(golden + ".lock").write_text(f"{os.getpid()}\n")
    assert window.open_image(golden)
    assert window.tool.fremd_gesperrt
    assert "gesperrt" in window.info_bar.text()
    erschienen = []
    window._fehler = lambda titel, text: erschienen.append((titel, text))
    window.set_read_only(False)
    assert window.tool.read_only and window.act_schreibschutz.isChecked()
    assert erschienen and "gesperrt" in erschienen[0][1]


def test_neue_platte_formatiert_alle_laufwerke_und_oeffnet(window, tmp_path):
    pfad = str(tmp_path / "neu.img.gz")
    assert window.neue_gide_platte(pfad, "kingstener")
    assert window.tool.layout == "kingstener" and window.tool.layout_ausdruecklich
    assert [v.dir for v in window.tool.volumes()] == ["lw0"]
    assert window.tool.read_only


def test_unklarer_befund_fragt_nach_dem_layout(window, tmp_path, monkeypatch):
    from app.core_binding.k1520disk import gide_anlegen
    p = tmp_path / "leer.img.gz"
    gide_anlegen(p, "poppe")
    gefragt = []
    monkeypatch.setattr(type(window), "_gide_layout_fragen", lambda self, grund: gefragt.append(grund) or "poppe")
    assert window.open_image(str(p))
    assert gefragt and "gide-layout" in gefragt[0]
    assert window.tool.layout == "poppe" and window.tool.volumes() == []
    # Abbruch der Frage: nicht geoeffnet
    monkeypatch.setattr(type(window), "_gide_layout_fragen", lambda self, grund: "")
    assert not window.open_image(str(p))
    assert window.tool is None


def test_laufwerk_formatieren(window, tmp_path):
    pfad = str(tmp_path / "f.img.gz")
    from app.core_binding.k1520disk import gide_anlegen
    gide_anlegen(pfad, "poppe")
    window._gide_layout_fragen = lambda grund: "poppe"
    assert window.open_image(pfad)
    window.tool.set_backup(False)
    window.set_read_only(False)
    assert window.gide_laufwerk_formatieren(3)
    assert [v.dir for v in window.tool.volumes()] == ["lw3"]
    assert [e.name for e in window.tool.list()] == ["lw3"]           # nur die Laufwerkszeile

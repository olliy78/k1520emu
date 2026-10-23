"""P8000-Platte in der Oberflaeche des DiskTool (Entwurf 37, AP P3).

Gearbeitet wird an einer KOPIE der ausgelieferten WEGA-3.0-Platte (`.img.gz`; md3 = /tmp ist
leer und dient als Schreibziel).  Nie die eingecheckte Platte selbst oeffnen.
"""

import shutil

import pytest

from conftest import ausgelieferte_diskette, requires_disk

pytestmark = requires_disk

pytest.importorskip("PySide6")

STAND = 400000000


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
def platte_pfad(tmp_path):
    ziel = tmp_path / "platte.k5504.img.gz"
    shutil.copyfile(ausgelieferte_diskette("p8000_wega30_platte.k5504.img.gz"), ziel)
    return str(ziel)


@pytest.fixture
def offen(window, platte_pfad):
    assert window.open_image(platte_pfad)
    window.tool.set_backup(False)          # kein ~ im Testordner noetig
    return window


def _item(baum, ref):
    """Baumzeile zu einer Referenz (rekursiv)."""
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


# ─── Oeffnen ─────────────────────────────────────────────────────────────────

def test_open_image_erkennt_eine_platte_ohne_formatdialog(window, platte_pfad, monkeypatch):
    from app.core_binding.k1520disk import PlatteTool
    # Ein Formatdialog darf nicht einmal erscheinen.
    from app.disktool.ui import main_window as mw
    monkeypatch.setattr(mw, "QInputDialog", None, raising=False)
    assert window.open_image(platte_pfad)
    assert isinstance(window.tool, PlatteTool)
    assert window.tool.read_only                       # Vorgabe: schreibgeschuetzt
    assert window.act_schreibschutz.isChecked()
    assert "Platte" in window.kopf.angaben.text() and "md3" in window.kopf.angaben.text()
    assert not window.kopf.fs_wahl.isEnabled()
    assert "Gesperrt" in window.kopf.fs_wahl.toolTip()
    assert "schreibgeschützt" in window.info_bar.text()


def test_ablegen_einer_img_gz_gilt_als_abbild(window, platte_pfad):
    from app.disktool.ui.disk_view import ABBILD_ENDUNGEN
    assert platte_pfad.lower().endswith(ABBILD_ENDUNGEN)
    window._abbild_abgelegt(platte_pfad)               # = Ziehen auf die Diskettenansicht
    assert window.tool is not None and window.tool.ist_plattenabbild
    assert "*.img.gz" in window.OEFFNEN_FILTER


def test_eine_diskette_bleibt_eine_diskette(window):
    window.open_image(str(ausgelieferte_diskette("p8000_wega30_root1.dmk")))
    assert not getattr(window.tool, "ist_plattenabbild", False)
    assert window.kopf.fs_wahl.isEnabled()
    assert window.disk_view.tree.headerItem().text(3) == "Eigensch."
    assert window.act_diskeditor.isEnabled()


def test_keine_datei_ist_keine_platte(window, tmp_path):
    assert not window.open_image(str(tmp_path / "gibt_es_nicht.img.gz"))
    assert window.tool is None


# ─── Anzeige ─────────────────────────────────────────────────────────────────

def test_baum_hat_die_partitionen_als_oberste_ordner(offen):
    from app.disktool.ui.disk_view import DIR_ROLE, REF_ROLE
    baum = offen.disk_view.tree
    oben = [baum.topLevelItem(i).data(0, REF_ROLE) for i in range(baum.topLevelItemCount())]
    assert oben[:2] == ["md0", "md2"] and "md3" in oben
    md3 = _item(baum, "md3")
    assert md3.data(0, DIR_ROLE) and "/tmp" in md3.text(0)
    assert "KB frei" in md3.text(6)
    # Ordner stehen UNTER ihrer Partition, Dateien unter ihrem Ordner.
    etc = _item(baum, "md2/etc")
    assert etc.parent() is _item(baum, "md2")
    pw = _item(baum, "md2/etc/passwd")
    assert pw.parent() is etc
    kopf = [baum.headerItem().text(i) for i in range(baum.columnCount())]
    assert kopf == ["Name", "Art", "Größe", "Rechte", "uid", "gid", "Geändert"]
    assert pw.text(0) == "passwd" and pw.text(1) == "-"
    assert pw.text(3).startswith("-") and len(pw.text(3)) == 10      # ls-Modus
    assert pw.text(4).isdigit() and pw.text(5).isdigit()
    assert pw.text(6)                                                # Zeit
    assert "WEGA" in offen.disk_view.titel.text()


def test_statuszeile_nennt_den_freien_platz_je_partition(offen):
    text = offen.st_inhalt.text()
    assert "md3:" in text and "KB frei" in text


# ─── Gesperrte Aktionen (mit Begruendung) ────────────────────────────────────

def test_sinnlose_aktionen_sind_gesperrt_und_begruendet(offen):
    from app.disktool.ui.schreibsperre import PLATTE_GESPERRT
    for name, grund in PLATTE_GESPERRT.items():
        a = getattr(offen, f"act_{name}")
        assert not a.isEnabled(), name
        assert "Gesperrt: " in a.toolTip(), name
    assert not offen.menue_fs.isEnabled()
    assert not offen.menue_schnitt.isEnabled()
    assert not offen.act_text.isEnabled()
    # Was geht, geht: Lesen, Eigenschaften (nach Auswahl), Aktualisieren, Schliessen.
    _waehle(offen, "md2/etc/passwd")
    for name in ("holen", "eigenschaften", "alles_raus", "aktualisieren", "schliessen",
                 "schreibschutz"):
        assert getattr(offen, f"act_{name}").isEnabled(), name


def test_nach_dem_schliessen_ist_alles_wie_vorher(offen):
    offen.close_disk()
    assert offen.tool is None
    assert not offen.act_platte_ordner.isVisible()
    assert "Gesperrt" not in offen.act_diskeditor.toolTip()
    assert offen.kopf.fs_wahl.isEnabled()
    # und an einer Diskette sind sie wieder da
    offen.open_image(str(ausgelieferte_diskette("p8000_wega30_root1.dmk")))
    assert offen.act_diskeditor.isEnabled()
    assert "Gesperrt" not in offen.act_diskeditor.toolTip()
    assert offen.menue_fs.isEnabled() and offen.act_text.isEnabled()


def test_die_neue_aktion_hat_kein_tastenkuerzel(window):
    assert window.act_platte_ordner.shortcut().isEmpty()


# ─── Schreibschutz ───────────────────────────────────────────────────────────

def test_schreibschutz_vorgabe_sperrt_jedes_schreiben_bis_zum_aufheben(offen, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    meldungen = []
    monkeypatch.setattr(QMessageBox, "critical",
                        staticmethod(lambda *a, **k: meldungen.append(a[2])))
    assert not offen.act_speichern.isEnabled()
    _waehle(offen, "md3")
    assert not offen.act_platte_ordner.isEnabled()
    assert "Schreibschutz" in offen.act_platte_ordner.toolTip()
    # Von Hand an der Sperre vorbei (Ziehen ignoriert die Aktionen): der Kern lehnt ab.
    assert not offen.platte_mkdir("md3/x")
    assert not offen._platte_einfuegen([__file__], "md3")
    assert len(meldungen) == 2 and all("schreibgesch" in m for m in meldungen)
    assert not offen.tool.dirty

    offen.act_schreibschutz.setChecked(False)          # = Anwender hebt auf
    assert not offen.tool.read_only
    assert offen.act_platte_ordner.isEnabled()
    offen.act_schreibschutz.setChecked(True)
    assert offen.tool.read_only


# ─── Schreiben ───────────────────────────────────────────────────────────────

def test_ordner_anlegen_einfuegen_loeschen_speichern(offen, platte_pfad, tmp_path):
    from app.core_binding.k1520disk import PlatteTool
    offen.set_read_only(False)
    _waehle(offen, "md3")
    assert offen.act_platte_ordner.isEnabled()
    assert offen.platte_mkdir("md3/neu")
    assert _item(offen.disk_view.tree, "md3/neu") is not None
    assert offen.windowTitle().startswith(offen._kurzname()) and offen.tool.dirty

    # Datei samt .fileinfo aus dem Ordner einfuegen (Ziel = der angeklickte Ordner).
    quelle = tmp_path / "q"
    quelle.mkdir()
    (quelle / "daten").write_bytes(b"hallo platte\n")
    (quelle / "daten.fileinfo").write_text(
        "fs=wega_mutos\nmode=0640\nuid=7\ngid=3\nmtime=%d\n" % STAND)
    _waehle(offen, "md3/neu")
    assert offen.act_platte_ordner.isEnabled()
    assert offen.insert_paths([str(quelle / "daten"), str(quelle / "daten.fileinfo")])
    e = next(x for x in offen.tool.list() if x.name == "md3/neu/daten")
    assert (e.unix_mode & 0o7777, e.unix_uid, e.unix_gid, e.unix_mtime) == (0o640, 7, 3, STAND)
    assert offen.tool.read("md3/neu/daten") == b"hallo platte\n"

    # Ablegen auf einen Ordner (Ziehen): der Ordner unter dem Mauszeiger gewinnt.
    (quelle / "zwei").write_bytes(b"2")
    offen.disk_view.tree.drop_ziel = "md3"
    offen._einfuegen_pfade([str(quelle / "zwei")], 0)
    assert offen.tool.read("md3/zwei") == b"2"
    # Kein Ordner unter dem Mauszeiger: es wird NICHT geraten (kein md0).
    offen.disk_view.tree.drop_ziel = ""
    gemeldet = []
    offen._fehler = lambda t, x: gemeldet.append(x)
    assert not offen._einfuegen_pfade([str(quelle / "zwei")], 0)  # liefert None -> falsy
    assert gemeldet and "Zielordner" in gemeldet[0]

    # Loeschen; eine Partition selbst nicht.
    assert not offen.erase_refs(["md3"])
    assert "Partition" in gemeldet[-1]
    assert offen.erase_refs(["md3/zwei"])
    assert all(x.name != "md3/zwei" for x in offen.tool.list())

    assert offen.save()
    assert not offen.tool.dirty
    offen._close_tool()
    with PlatteTool.open(platte_pfad) as neu:                       # frisch gelesen
        assert neu.read("md3/neu/daten") == b"hallo platte\n"
        assert all(x.name != "md3/zwei" for x in neu.list())
    with open(platte_pfad, "rb") as f:
        assert f.read(2) == b"\x1f\x8b"                             # weiter gzip


def test_loeschen_fragt_nach_und_nennt_die_platte(offen, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    offen.set_read_only(False)
    offen.platte_mkdir("md3/weg")
    fragen = []

    def frage(*a, **k):
        fragen.append(a[2])
        return QMessageBox.No
    monkeypatch.setattr(QMessageBox, "question", staticmethod(frage))
    offen._loeschen_refs(["md3/weg"])
    assert "Platte" in fragen[0] and "Diskette" not in fragen[0]
    assert any(x.name == "md3/weg" for x in offen.tool.list())      # verneint = bleibt


def test_ungespeichert_schliessen_fragt(offen, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    offen.set_read_only(False)
    offen.platte_mkdir("md3/x")
    monkeypatch.setattr(QMessageBox, "question",
                        staticmethod(lambda *a, **k: QMessageBox.Cancel))
    assert not offen.close_disk()
    assert offen.tool is not None


# ─── Holen ───────────────────────────────────────────────────────────────────

def test_holen_schreibt_fileinfo_und_alles_extrahieren_die_partitionen(offen, tmp_path):
    aus = tmp_path / "aus"
    assert offen.extract_refs(["md2/etc/passwd"], aus)
    assert (aus / "md2" / "etc" / "passwd").read_bytes() == offen.tool.read("md2/etc/passwd")
    assert (aus / "md2" / "etc" / "passwd.fileinfo").exists()

    alles = tmp_path / "alles"
    assert offen.extract_all(alles)
    assert (alles / "md2" / "etc" / "passwd").exists() and (alles / "md3").is_dir()


# ─── Eigenschaften ───────────────────────────────────────────────────────────

def test_eigenschaftendialog_aendert_modus_besitzer_zeit(offen):
    from app.disktool.ui.properties_dialog import PropertiesDialog
    offen.set_read_only(False)
    e = next(x for x in offen.tool.list() if x.name == "md2/etc/passwd")
    d = PropertiesDialog(offen.tool, e, offen)
    assert d.wega and d.f_uid.value() == e.unix_uid
    assert d.aenderungen() == {}
    d.f_oktal.setText("0600")
    d.f_uid.setValue(11)
    d.f_gid.setValue(12)
    d.f_mtime.setText("2001-02-03 04:05:06")
    assert d.uebernehmen()
    n = next(x for x in offen.tool.list() if x.name == "md2/etc/passwd")
    assert (n.unix_mode & 0o7777, n.unix_uid, n.unix_gid, n.unix_mtime) == (
        0o600, 11, 12, 981173106)
    assert n.size == e.size and n.type == "-"


def test_eigenschaftendialog_zeigt_bei_schreibschutz_den_weg_zum_aufheben(offen):
    from app.disktool.ui.properties_dialog import PropertiesDialog
    e = next(x for x in offen.tool.list() if x.name == "md2/etc/passwd")
    d = PropertiesDialog(offen.tool, e, offen)
    assert "Platte" in d.hinweis.text() and "Strg+R" in d.hinweis.text()


def test_show_properties_findet_den_eintrag_der_platte(offen, monkeypatch):
    from app.disktool.ui import main_window as mw
    gezeigt = []
    monkeypatch.setattr(mw.PropertiesDialog, "exec", lambda self: gezeigt.append(self.entry.name))
    assert offen.show_properties("md2/etc/passwd")
    assert gezeigt == ["md2/etc/passwd"]


# ─── Handbuch ────────────────────────────────────────────────────────────────

def test_handbuch_beschreibt_die_platte():
    from app.disktool.ui.help_window import lade_handbuch
    text = lade_handbuch()
    assert "P8000-Platten (WEGA)" in text and "md0" in text and ".img.gz" in text

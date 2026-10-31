"""SIOS 1526 in der Oberflaeche des DiskTool (D6, Entwurf 43): Bibliotheken als Ordner,
Eigenschaften (VOL1/HDR1/AADR), Diskeditor mit EBCDIC/DKOI-Textspalte.

Gearbeitet wird an Kopien (`temp_disk`) - eine eingecheckte Diskette nie direkt oeffnen.
"""

import pytest

from conftest import requires_disk

pytestmark = requires_disk

pytest.importorskip("PySide6")


@pytest.fixture(scope="module")
def qt_app(qapp):
    yield qapp


@pytest.fixture
def window(qt_app):
    from PySide6.QtCore import QCoreApplication, QEvent
    from app.disktool.ui.main_window import MainWindow
    w = MainWindow()
    yield w
    w._close_tool()
    w.deleteLater()
    qt_app.processEvents()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)


def _oben(view) -> dict:
    return {view.tree.topLevelItem(i).text(0): view.tree.topLevelItem(i)
            for i in range(view.tree.topLevelItemCount())}


# ─── (a) Bibliotheken als Ordner ─────────────────────────────────────────────

def test_a_library_is_a_folder_with_its_objects_below(window, temp_disk):
    assert window.open_image(temp_disk("sios1526_sg1.hfe"))
    oben = _oben(window.disk_view)
    # Oben: die physischen Dateien; die Objekte stehen NICHT daneben.
    assert set(oben) == {"SYSORG09", "PHASELB"}
    lib = oben["PHASELB"]
    kinder = [lib.child(i).text(0) for i in range(lib.childCount())]
    assert kinder == ["SGEN", "SOPH", "SYSD"]
    # Die Referenz bleibt der volle Name - Holen/Loeschen/Eigenschaften sprechen den Kern so an.
    from app.disktool.ui.disk_view import REF_ROLE, DIR_ROLE
    assert lib.child(1).data(0, REF_ROLE) == "PHASELB/SOPH"
    assert lib.data(0, DIR_ROLE) is True
    assert not oben["SYSORG09"].data(0, DIR_ROLE)


def test_selecting_an_object_extracts_it_by_its_full_name(window, temp_disk, tmp_path):
    assert window.open_image(temp_disk("sios1526_sg1.hfe"))
    lib = _oben(window.disk_view)["PHASELB"]
    lib.child(1).setSelected(True)
    refs = window.disk_view.selected_refs()
    assert refs == ["PHASELB/SOPH"]
    assert window.extract_refs(refs, tmp_path)
    assert any(p.name.endswith("SOPH") for p in tmp_path.iterdir())


def test_dropping_onto_a_library_makes_the_files_its_objects(window, temp_disk, tmp_path):
    assert window.open_image(temp_disk("sios1526_sg1.hfe"))
    window.set_read_only(False)
    quelle = tmp_path / "neu"
    quelle.write_bytes(bytes(range(200)))
    window.disk_view.tree.drop_ziel = "PHASELB"          # wie dropEvent bei einem Ablegen auf die Bibliothek
    window._einfuegen_pfade([str(quelle)], 0)
    namen = {e.name for e in window.tool.list()}
    assert "PHASELB/NEU" in namen and "NEU" not in namen
    # ohne Ordner unter dem Mauszeiger: physische Datei (Vorgabe wie bisher)
    zwei = tmp_path / "datei2"
    zwei.write_bytes(b"x" * 10)
    window._einfuegen_pfade([str(zwei)], 0)
    assert "DATEI2" in {e.name for e in window.tool.list()}


def test_selected_library_is_the_target_of_write(window, temp_disk, tmp_path):
    assert window.open_image(temp_disk("sios1526_sg1.hfe"))
    oben = _oben(window.disk_view)
    oben["PHASELB"].child(0).setSelected(True)           # ein Objekt: seine Bibliothek zaehlt
    window.disk_view.tree.setCurrentItem(oben["PHASELB"].child(0))
    assert window.disk_view.sios_ziel() == "PHASELB"
    oben["SYSORG09"].setSelected(True)
    window.disk_view.tree.clearSelection()
    window.disk_view.tree.setCurrentItem(oben["SYSORG09"])
    oben["SYSORG09"].setSelected(True)
    assert window.disk_view.sios_ziel() == ""


# ─── (b) Eigenschaften ───────────────────────────────────────────────────────

def _dialog(window, name):
    from app.disktool.ui.properties_dialog import PropertiesDialog
    eintrag = next(e for e in window.tool.list() if e.name == name)
    return PropertiesDialog(window.tool, eintrag, window)


def test_properties_of_a_file_show_hdr1_and_vol1(window, temp_disk):
    assert window.open_image(temp_disk("sios1526_sg1.hfe"))
    d = _dialog(window, "SYSORG09")
    assert d.art == "sios"
    hdr, vol = d.sios["datei"], d.sios["vol"]
    assert (hdr["satzlaenge"], hdr["boe"], hdr["eoe"], hdr["eod"]) == ("1024", "01001", "28026", "29001")
    assert (hdr["austausch"], hdr["fortsetzung"], hdr["folge"]) == ("E", "C", "01")
    assert hdr["art"] == "datei" and hdr["etikett"] == "8"
    assert vol["vol_name"] == "MWD002" and vol["vol_aufzeichnung"] == "M" and vol["vol_version"] == "W"
    assert vol["vol_sektorfolge"] == "01"
    # Mehrdisketten-Kennzeichen im Klartext
    assert "Folgediskette" in d._sios_kennzeichen(hdr)
    # Nur Anzeige: nichts zu uebernehmen, Knopf aus
    assert d.aenderungen() == {}
    from PySide6.QtWidgets import QDialogButtonBox
    assert not d.knoepfe.button(QDialogButtonBox.Save).isEnabled()


def test_properties_of_a_library_and_an_object(window, temp_disk):
    assert window.open_image(temp_disk("sios1526_sg1.hfe"))
    lib = _dialog(window, "PHASELB").sios["datei"]
    assert lib["art"] == "bibliothek" and lib["fortsetzung"] == ""
    obj = _dialog(window, "PHASELB/SOPH").sios["datei"]
    assert obj == {"art": "objekt", "bibliothek": "PHASELB", "aadr": "0049", "ml": obj["ml"],
                   "laenge": "26"}


def test_properties_of_a_non_sios_disk_are_unchanged(window, fixture_disks):
    assert window.open_image(fixture_disks / "cpa_cpa780_k5601_clock.img")
    d = _dialog(window, "PIP.COM")
    assert d.art == "cpm"


# ─── (c) Diskeditor: ASCII <-> EBCDIC/DKOI ───────────────────────────────────

def test_ebcdic_text_column_function():
    from app.disktool.ui.disk_editor import ebcdic_zeichen, hexdump
    assert ebcdic_zeichen(0xE5) == "V" and ebcdic_zeichen(0xD6) == "O"
    assert ebcdic_zeichen(0xF1) == "1" and ebcdic_zeichen(0x40) == " "
    # DKOI: die fuenf Abweichungen
    assert [ebcdic_zeichen(b) for b in (0x4A, 0x4F, 0x5A, 0x5F, 0x6A)] == ["[", "!", "]", "^", "/"]
    assert ebcdic_zeichen(0x00) == "." and ebcdic_zeichen(0xFF) == "."
    zeile = hexdump(bytes([0xE5, 0xD6, 0xD3, 0xF1]), kodierung="ebcdic")
    assert zeile.endswith("VOL1")
    assert hexdump(bytes([0x56, 0x4F]), kodierung="ascii").endswith("VO")


def test_sios_track_zero_shows_ebcdic_by_default_and_remembers_the_choice(window, temp_disk):
    assert window.open_image(temp_disk("sios1526_dp1.hfe"))
    ed = window.open_disk_editor()
    try:
        # Spur 0, Sektor 7 = VOL1: automatisch EBCDIC
        span = next(s for s in ed.surface.tracks[0][0].spans if s.is_sector and s.id == 7)
        assert ed.select_sector(0, 0, span.index)
        assert ed.kodierung() == "ebcdic"
        assert "VOL1" in ed.hex.toPlainText().split("\n")[0]
        assert "MWD051" in ed.hex.toPlainText()
        # Datenspur: wieder ASCII (ohne ausdrueckliche Wahl)
        span1 = next(s for s in ed.surface.tracks[0][1].spans if s.is_sector)
        assert ed.select_sector(0, 1, span1.index)
        assert ed.kodierung() == "ascii"
        # Auf Spur 0 ausdruecklich ASCII waehlen: gilt danach auch dort
        assert ed.select_sector(0, 0, span.index)
        ed.kodierung_wahl.setCurrentIndex(0)
        assert ed.select_sector(0, 1, span1.index)
        assert ed.select_sector(0, 0, span.index)
        assert ed.kodierung() == "ascii"
        assert "VOL1" not in ed.hex.toPlainText()
        # Umschalten zeichnet die Textspalte neu, die Hexbytes bleiben
        hexvorher = [z[:70] for z in ed.hex.toPlainText().split("\n")]
        ed.kodierung_wahl.setCurrentIndex(1)
        assert "VOL1" in ed.hex.toPlainText()
        assert [z[:70] for z in ed.hex.toPlainText().split("\n")] == hexvorher
    finally:
        ed.close()
    ed2 = window.open_disk_editor()
    try:
        assert ed2._kodierung_wahl == "ebcdic"          # zuletzt gewaehlt, je Diskette gemerkt
    finally:
        ed2.close()


def test_editing_the_hex_keeps_the_chosen_text_column(window, temp_disk):
    assert window.open_image(temp_disk("sios1526_dp1.hfe"))
    ed = window.open_disk_editor()
    try:
        span = next(s for s in ed.surface.tracks[0][0].spans if s.is_sector and s.id == 7)
        assert ed.select_sector(0, 0, span.index)
        from app.disktool.ui.disk_editor import parse_hexdump
        daten = bytearray(parse_hexdump(ed.hex.toPlainText()))
        daten[4:10] = bytes([0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6])      # ABCDEF
        from app.disktool.ui.disk_editor import hexdump
        ed.hex.setPlainText(hexdump(bytes(daten), kodierung="ebcdic"))
        assert "VOL1ABCDEF" in ed.hex.toPlainText()
    finally:
        ed.close()


def test_a_non_sios_disk_defaults_to_ascii(window, fixture_disks):
    assert window.open_image(fixture_disks / "cpa_cpa780_k5601_clock.img")
    ed = window.open_disk_editor()
    try:
        span = next(x for x in ed.surface.tracks[0][0].spans if x.is_sector)
        assert ed.select_sector(0, 0, span.index)
        assert ed.kodierung() == "ascii"
    finally:
        ed.close()

"""tar-Export, leere tar-Diskette (``tar720_leer``) und ``.tar`` ablegen:
C-ABI (``DiskTool.tar_*``) und Oberflaeche (PySide6, headless).

Seit der Vereinheitlichung (Entwurf 37) ist eine tar-Diskette immer ein Satz mit N >= 1 Abbildern:
es gibt keinen Sonderweg „genau EIN .tar auf die leere Diskette“ mehr (frueher ``tar_put`` /
„tar-Datei auf Diskette schreiben…“).  Eine ``.tar`` kommt ueber den normalen Ablegen-Weg
(``tar_datei_ablegen`` bzw. Einfuegen/Ziehen) hinein: auf einem LEEREN Archiv als Datei oder als
Container, auf einem Archiv mit Inhalt immer als Datei; die Diskette bleibt danach bearbeitbar.

Die Regeln selbst (Endmarke, Folgeteil, V7-Urteil, Rundlauf) pruefen die C++-Tests
``TarExport.*``/``TarPruefen.*``/``TarLeer.*``; hier geht es um die Verdrahtung.
"""

import pytest

from conftest import requires_disk

pytestmark = requires_disk

DISK = 737280


def _kopf(name: str, daten: bytes, ustar: bool = False) -> bytes:
    h = bytearray(512)
    h[0:len(name)] = name.encode()
    h[100:108] = b"%07o\0" % 0o644
    h[108:116] = b"%07o\0" % 25
    h[116:124] = b"%07o\0" % 1
    h[124:136] = b"%011o\0" % len(daten)
    h[136:148] = b"%011o\0" % 612345678
    h[156] = ord("0")
    if ustar:
        h[257:265] = b"ustar  \0"
    h[148:156] = b"        "
    h[148:155] = b"%06o\0" % sum(h)
    h[155] = ord(" ")
    return bytes(h)


def _tar(ustar: bool = False) -> bytes:
    s = b""
    for name, daten in (("a.txt", b"Hallo\n"), ("dir/b.bin", bytes(range(200)))):
        s += _kopf(name, daten, ustar) + daten + bytes(-len(daten) % 512)
    return s + bytes(1024)


@pytest.fixture
def leere(tmp_path):
    from app.core_binding.k1520disk import DiskTool
    p = tmp_path / "leer.cqm"
    DiskTool.create(p, "tar720_leer", "").close()
    return p


# ─── C-ABI ───────────────────────────────────────────────────────────────────

def test_catalog_knows_the_empty_tar_profile():
    from app.core_binding.k1520disk import filesystems
    fs = {f.name: f for f in filesystems()}
    assert fs["tar720_leer"].leer and fs["tar720_leer"].type == "tar"
    assert not fs["tar720"].leer


def test_empty_tar_disk_roundtrip_through_the_abi(tmp_path, leere):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    quelle = tmp_path / "x.tar"
    quelle.write_bytes(_tar())
    with DiskTool.open(leere, read_only=False) as d:
        # eine tar-Diskette ist ein Satz mit EINEM Abbild
        assert d.ist_tar and d.tar_satz and d.tar_satz_einzel and not d.read_only_forced
        assert d.filesystem == "tar720_leer"
        with pytest.raises(K1520DiskError, match="leer"):
            d.tar_export(tmp_path / "nichts.tar")
        u = d.tar_datei_pruefen(quelle)
        assert u["ziel_leer"] and u["als_datei"] and u["als_container"] and not u["wird_satz"]
        assert d.tar_datei_ablegen(quelle, als_container=True) == ""
        d.flush()                                    # geht in dasselbe Abbild (.cqm bleibt .cqm)
        # nicht schreibgeschuetzt, ganz normal bearbeitbar; ein zweiter Container: nein
        assert not d.read_only and not d.read_only_forced and not d.dirty
        assert d.filesystem == "tar720"
        assert [e.name for e in d.list()] == ["a.txt", "dir/b.bin"]
        with pytest.raises(K1520DiskError, match="nicht leer"):
            d.tar_datei_ablegen(quelle, als_container=True)
        aus = tmp_path / "aus.tar"
        d.tar_export(aus)
        assert aus.read_bytes() == quelle.read_bytes()
    with DiskTool.open(leere) as d:                  # Wiederoeffnen
        assert d.filesystem == "tar720" and d.container_info()["type"] == "cqm"
        aus2 = tmp_path / "aus2.tar"
        d.tar_export(aus2)
        assert aus2.read_bytes() == quelle.read_bytes()


def test_container_check_reports_warning_and_rejections(tmp_path, leere):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    gnu = tmp_path / "gnu.tar"
    gnu.write_bytes(_tar(ustar=True))
    gross = tmp_path / "gross.tar"
    gross.write_bytes(bytes(DISK + 512))
    krumm = tmp_path / "krumm.tar"
    krumm.write_bytes(b"x" * 700)
    with DiskTool.open(leere, read_only=False) as d:
        u = d.tar_datei_pruefen(gnu)
        assert u["als_container"] and "V7" in u["warnung"]
        # ohne Kopf / nicht durch 512 teilbar: kein Container (als Datei geht es immer)
        for datei, grund in ((gross, "Kopf"), (krumm, "512")):
            u = d.tar_datei_pruefen(datei)
            assert u["als_datei"] and not u["als_container"] and grund in u["container_grund"]
            with pytest.raises(K1520DiskError, match=grund):
                d.tar_datei_ablegen(datei, als_container=True)
        assert not d.dirty and d.list() == []            # Pruefen und Ablehnen schreiben nie


def test_container_needs_the_lock_released(tmp_path, leere):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    quelle = tmp_path / "x.tar"
    quelle.write_bytes(_tar())
    with DiskTool.open(leere) as d:                    # Vorgabe: schreibgeschuetzt
        assert d.tar_satz_einzel and d.read_only and not d.read_only_forced
        with pytest.raises(K1520DiskError, match="Schreibschutz"):
            d.tar_datei_ablegen(quelle, als_container=True)
        d.set_read_only(False)
        d.tar_datei_ablegen(quelle, als_container=True)


# ─── Oberflaeche ─────────────────────────────────────────────────────────────

pytest.importorskip("PySide6")


@pytest.fixture(scope="module")
def qt_app(qapp):
    """Die gemeinsame QApplication aus conftest.py — sie baut die Fenster vor dem Prozessende ab
    (sonst SegFault beim Interpreterende, zuerst in der CI an py_cqm_disktool_gui gesehen)."""
    yield qapp


@pytest.fixture
def window(qt_app):
    from app.disktool.ui.main_window import MainWindow
    w = MainWindow()
    yield w
    w._close_tool()
    # Gleich abbauen statt am Prozessende in beliebiger Reihenfolge (ohne close(): das
    # schriebe den Fensterzustand in die Benutzerkonfiguration).
    w.deleteLater()
    qt_app.processEvents()


def _wahl(window, monkeypatch, antwort, gesehen=None):
    """Das Popup „Als Datei / Container“ ersetzen (ein echtes modales Fenster scheitert in der Testebene)."""
    def w(name, urteil):
        if gesehen is not None:
            gesehen.append((name, urteil))
        return antwort
    monkeypatch.setattr(window, "_tar_datei_wahl", w)


def test_actions_exist_without_shortcut(window):
    assert window.act_tar_export.shortcut().isEmpty()
    assert ".tar" in window.act_tar_export.text()
    # der Sonderweg „tar-Datei auf Diskette schreiben“ ist entfallen (Einfuegen/Ziehen genuegt)
    assert not hasattr(window, "act_tar_schreiben")


def test_empty_tar_disk_in_the_window(window, leere):
    assert window.open_image(str(leere))
    t = window.tool
    assert t.tar_satz_einzel and t.list() == []
    assert "tar-Archiv (UNIX)" in window.kopf.angaben.text()
    window.set_read_only(False)
    # auch die leere tar-Diskette nimmt Dateien, Ordner und Einfuegen an wie jedes Archiv
    assert window.act_alles_rein.isEnabled() and window.act_tar_ordner.isEnabled()
    assert window.act_tar_ordner.isVisible()


def test_container_through_the_normal_way_stays_editable_and_exportable(window, leere, tmp_path,
                                                                        monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    quelle = tmp_path / "x.tar"
    quelle.write_bytes(_tar())
    assert window.open_image(str(leere))
    window.set_read_only(False)
    gesehen = []
    _wahl(window, monkeypatch, "container", gesehen)
    assert window.insert_paths([str(quelle)])
    assert gesehen and gesehen[0][0] == "x.tar" and gesehen[0][1]["als_container"]
    t = window.tool
    assert t.dirty and [e.name for e in t.list()] == ["a.txt", "dir/b.bin"]
    assert window.save()                                # ohne Rueckfrage: ins selbe Abbild
    assert not t.dirty and not t.read_only and not t.read_only_forced
    assert "tar-Archiv (UNIX)" in window.kopf.angaben.text()
    assert window.act_tar_export.isEnabled()
    # eine zweite .tar: nicht mehr leer -> OHNE Popup als Datei
    _wahl(window, monkeypatch, None)
    monkeypatch.setattr(window, "_tar_datei_wahl",
                        lambda *a: (_ for _ in ()).throw(AssertionError("Popup auf nicht leerem Archiv")))
    assert window.insert_paths([str(quelle)])
    assert sorted(e.name for e in t.list()) == ["a.txt", "dir/b.bin", "x.tar"]
    # Export der Mischung: Strom einschliesslich der abgelegten Datei
    ziel = tmp_path / "aus.tar"
    assert window.tar_export(ziel)
    assert ziel.stat().st_size >= quelle.stat().st_size


def test_non_v7_shows_the_hint_in_the_popup_and_cancel_leaves_the_disk_untouched(
        window, leere, tmp_path, monkeypatch):
    quelle = tmp_path / "gnu.tar"
    quelle.write_bytes(_tar(ustar=True))
    assert window.open_image(str(leere))
    window.set_read_only(False)
    gesehen = []
    _wahl(window, monkeypatch, None, gesehen)           # Abbrechen
    assert not window.insert_paths([str(quelle)])
    assert gesehen and "V7" in gesehen[0][1]["warnung"]
    assert window.tool.list() == [] and not window.tool.dirty    # nichts geschrieben

    _wahl(window, monkeypatch, "container", gesehen)
    assert window.insert_paths([str(quelle)])
    assert [e.name for e in window.tool.list()] == ["a.txt", "dir/b.bin"]
    assert len(gesehen) == 2


def test_export_is_offered_on_a_raw_opened_follow_up_part(window, tmp_path):
    d = bytes(1 + (i * 7) % 250 for i in range(DISK))   # kein Kopf, kein Dateisystem
    p = tmp_path / "teil.img"
    p.write_bytes(d)
    assert window.open_image(str(p))
    assert not window.tool.has_filesystem
    assert window.act_tar_export.isEnabled()
    ziel = tmp_path / "teil.tar"
    assert window.tar_export(ziel)
    assert ziel.read_bytes() == d


def test_new_disk_dialog_offers_the_empty_tar_disk_only():
    from app.core_binding.k1520disk import filesystems
    angeboten = [f.name for f in filesystems() if f.type != "tar" or f.leer]
    assert "tar720_leer" in angeboten and "tar720" not in angeboten


# ─── |← / Strg+← und Ziehen und Ablegen auf der leeren tar-Diskette ──────────

def _drop_dateien(ziel, pfade):
    from PySide6.QtCore import QMimeData, QPointF, Qt, QUrl
    from PySide6.QtGui import QDragEnterEvent, QDropEvent
    from PySide6.QtWidgets import QApplication
    mime = QMimeData()
    mime.setUrls([QUrl.fromLocalFile(str(p)) for p in pfade])
    pos = QPointF(5, 5)
    ein = QDragEnterEvent(pos.toPoint(), Qt.CopyAction, mime, Qt.LeftButton, Qt.NoModifier)
    ein.setAccepted(False)
    QApplication.sendEvent(ziel, ein)
    if not ein.isAccepted():
        return False
    ev = QDropEvent(pos, Qt.CopyAction, mime, Qt.LeftButton, Qt.NoModifier)
    QApplication.sendEvent(ziel, ev)
    return ev.isAccepted()


def test_drop_one_tar_on_the_empty_disk_asks_and_stays_editable(window, leere, tmp_path, monkeypatch):
    quelle = tmp_path / "x.tar"
    quelle.write_bytes(_tar())
    assert window.open_image(str(leere))
    window.set_read_only(False)
    gesehen = []
    _wahl(window, monkeypatch, "container", gesehen)
    assert _drop_dateien(window.disk_view.tree.viewport(), [quelle])
    assert gesehen
    assert window.tool.ist_tar and not window.tool.read_only
    assert [e.name for e in window.tool.list()] == ["a.txt", "dir/b.bin"]


def test_toolbar_way_uses_the_selected_folder_file(window, leere, tmp_path, monkeypatch):
    quelle = tmp_path / "x.tar"
    quelle.write_bytes(_tar())
    assert window.open_image(str(leere))
    window.set_read_only(False)
    _wahl(window, monkeypatch, "datei")
    window.insert_paths([str(quelle)], 0)          # der Weg von |← / Strg+←
    assert [e.name for e in window.tool.list()] == ["x.tar"]


def test_multiple_tars_folders_and_images_are_plain_files_and_the_lock_refuses(
        window, leere, tmp_path, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    t1 = tmp_path / "a.tar"
    t2 = tmp_path / "b.tar"
    t1.write_bytes(_tar())
    t2.write_bytes(_tar())
    ordner = tmp_path / "o"
    ordner.mkdir()
    (ordner / "i.txt").write_bytes(b"i")
    abbild = tmp_path / "z.img"
    abbild.write_bytes(b"x")
    assert window.open_image(str(leere))
    window.set_read_only(False)
    # Popup nur bei GENAU EINER .tar auf leerem Archiv: hier nie
    monkeypatch.setattr(window, "_tar_datei_wahl",
                        lambda *a: (_ for _ in ()).throw(AssertionError("unerwartetes Popup")))
    assert window.insert_paths([str(t1), str(t2), str(ordner), str(abbild)])
    namen = sorted(e.name for e in window.tool.list())
    assert {"a.tar", "b.tar", "z.img", "o/i.txt"} <= set(namen), namen
    # geschuetzt: abgelehnt, mit Meldung des Kerns
    window.set_read_only(True)
    meldungen = []
    monkeypatch.setattr(QMessageBox, "critical",
                        staticmethod(lambda *a, **k: meldungen.append(a[2])))
    assert not window.insert_paths([str(t1)])
    assert meldungen and "Schreibschutz" in meldungen[0]

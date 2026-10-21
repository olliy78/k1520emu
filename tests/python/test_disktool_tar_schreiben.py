"""tar-Export, leere tar-Diskette (``tar720_leer``) und ``.tar`` auf Diskette schreiben:
C-ABI (``DiskTool.tar_*``) und Oberflaeche (PySide6, headless).

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
        assert d.tar_leer and d.ist_tar and not d.read_only_forced
        assert d.filesystem == "tar720_leer"
        with pytest.raises(K1520DiskError, match="leer"):
            d.tar_export(tmp_path / "nichts.tar")
        assert d.tar_put_pruefen(quelle) == ""
        assert d.tar_put(quelle) == ""
        # abgeschlossenes Archiv (Kopf + Endmarke): schreibbar (T1), tar-put schuetzt es aber einmalig
        assert not d.tar_leer and d.read_only and not d.read_only_forced
        assert d.filesystem == "tar720"
        assert [e.name for e in d.list()] == ["a.txt", "dir/b.bin"]
        with pytest.raises(K1520DiskError, match="tar-Archiv"):
            d.tar_put(quelle)
        aus = tmp_path / "aus.tar"
        d.tar_export(aus)
        assert aus.read_bytes() == quelle.read_bytes()
    with DiskTool.open(leere) as d:                  # Wiederoeffnen
        assert d.filesystem == "tar720" and not d.tar_leer
        aus2 = tmp_path / "aus2.tar"
        d.tar_export(aus2)
        assert aus2.read_bytes() == quelle.read_bytes()


def test_put_check_reports_warning_and_rejections(tmp_path, leere):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    gnu = tmp_path / "gnu.tar"
    gnu.write_bytes(_tar(ustar=True))
    gross = tmp_path / "gross.tar"
    gross.write_bytes(bytes(DISK + 512))
    krumm = tmp_path / "krumm.tar"
    krumm.write_bytes(b"x" * 700)
    with DiskTool.open(leere, read_only=False) as d:
        assert "V7" in d.tar_put_pruefen(gnu)
        with pytest.raises(K1520DiskError, match="split -b 737280"):
            d.tar_put_pruefen(gross)
        with pytest.raises(K1520DiskError, match="512"):
            d.tar_put_pruefen(krumm)
        assert d.tar_leer                              # Pruefen schreibt nie


def test_put_needs_the_lock_released(tmp_path, leere):
    from app.core_binding.k1520disk import DiskTool, K1520DiskError
    quelle = tmp_path / "x.tar"
    quelle.write_bytes(_tar())
    with DiskTool.open(leere) as d:                    # Vorgabe: schreibgeschuetzt
        assert d.tar_leer and d.read_only and not d.read_only_forced
        with pytest.raises(K1520DiskError, match="Schreibschutz"):
            d.tar_put(quelle)
        d.set_read_only(False)
        d.tar_put(quelle)


# ─── Oberflaeche ─────────────────────────────────────────────────────────────

pytest.importorskip("PySide6")


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


def test_actions_exist_without_shortcut(window):
    for name in ("act_tar_export", "act_tar_schreiben"):
        a = getattr(window, name)
        assert a.shortcut().isEmpty()
    assert ".tar" in window.act_tar_export.text()
    assert ".tar" in window.act_tar_schreiben.text() or "tar-Datei" in window.act_tar_schreiben.text()


def test_empty_tar_disk_in_the_window(window, leere):
    assert window.open_image(str(leere))
    t = window.tool
    assert t.tar_leer
    assert "tar-Diskette, leer" in window.kopf.angaben.text()
    # geschuetzt geoeffnet: auch das Schreiben des .tar ist zu, der Tooltip nennt den Grund
    assert not window.act_tar_schreiben.isEnabled()
    assert "Schreibschutz" in window.act_tar_schreiben.toolTip()
    window.set_read_only(False)
    assert window.act_tar_schreiben.isEnabled()
    # die Datei-Wege bleiben zu, der Export auch (nichts zu exportieren)
    for a in (window.act_loeschen, window.act_alles_rein, window.act_tar_export):
        assert not a.isEnabled()
    assert "genau ein .tar" in window.act_loeschen.toolTip()


def test_write_a_v7_tar_then_locked_and_exportable(window, leere, tmp_path, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    quelle = tmp_path / "x.tar"
    quelle.write_bytes(_tar())
    assert window.open_image(str(leere))
    window.set_read_only(False)
    # V7: keine Rueckfrage (die Sperre der Tests wuerde sie sonst als Fehlschlag melden)
    assert window.tar_schreiben(quelle)
    t = window.tool
    # abgeschlossenes Archiv: im Kern schreibbar (Entwurf 37 T1); tar-put schuetzt es einmalig
    assert t.ist_tar and not t.tar_leer and t.read_only and not t.read_only_forced
    assert "tar-Archiv (UNIX)" in window.kopf.angaben.text()
    assert not window.act_tar_schreiben.isEnabled()
    assert window.act_tar_export.isEnabled()
    assert [e.name for e in t.list()] == ["a.txt", "dir/b.bin"]
    # zweites Schreiben: abgelehnt mit Meldung
    meldungen = []
    monkeypatch.setattr(QMessageBox, "critical",
                        staticmethod(lambda *a, **k: meldungen.append(a[2])))
    assert not window.tar_schreiben(quelle)
    assert meldungen and "tar-Archiv" in meldungen[0]
    # Export
    ziel = tmp_path / "aus.tar"
    assert window.tar_export(ziel)
    assert ziel.read_bytes() == quelle.read_bytes()


def test_non_v7_asks_and_no_leaves_the_disk_untouched(window, leere, tmp_path, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    quelle = tmp_path / "gnu.tar"
    quelle.write_bytes(_tar(ustar=True))
    assert window.open_image(str(leere))
    window.set_read_only(False)
    fragen = []

    def antwort(wert):
        def q(*a, **k):
            fragen.append(a[2])
            return wert
        return staticmethod(q)

    monkeypatch.setattr(QMessageBox, "question", antwort(QMessageBox.No))
    assert not window.tar_schreiben(quelle)
    assert fragen and "Trotzdem schreiben?" in fragen[0] and "V7" in fragen[0]
    assert window.tool.tar_leer                         # nichts geschrieben

    monkeypatch.setattr(QMessageBox, "question", antwort(QMessageBox.Yes))
    assert window.tar_schreiben(quelle)
    assert not window.tool.tar_leer and window.tool.ist_tar
    assert len(fragen) == 2


def test_export_is_offered_on_a_raw_opened_follow_up_part(window, tmp_path):
    d = bytes(1 + (i * 7) % 250 for i in range(DISK))   # kein Kopf, kein Dateisystem
    p = tmp_path / "teil.img"
    p.write_bytes(d)
    assert window.open_image(str(p))
    assert not window.tool.has_filesystem
    assert window.act_tar_export.isEnabled()
    assert not window.act_tar_schreiben.isEnabled()
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


def test_drop_one_tar_writes_it_and_locks(window, leere, tmp_path):
    quelle = tmp_path / "x.tar"
    quelle.write_bytes(_tar())
    assert window.open_image(str(leere))
    window.set_read_only(False)
    assert _drop_dateien(window.disk_view.tree.viewport(), [quelle])
    assert window.tool.ist_tar and not window.tool.tar_leer
    assert [e.name for e in window.tool.list()] == ["a.txt", "dir/b.bin"]


def test_drop_of_non_v7_asks_and_no_keeps_the_disk_empty(window, leere, tmp_path, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    quelle = tmp_path / "gnu.tar"
    quelle.write_bytes(_tar(ustar=True))
    assert window.open_image(str(leere))
    window.set_read_only(False)
    fragen = []
    monkeypatch.setattr(QMessageBox, "question",
                        staticmethod(lambda *a, **k: fragen.append(a[2]) or QMessageBox.No))
    window.insert_paths([str(quelle)])
    assert fragen and "Trotzdem schreiben?" in fragen[0]
    assert window.tool.tar_leer


def test_toolbar_way_uses_the_selected_folder_file(window, leere, tmp_path):
    quelle = tmp_path / "x.tar"
    quelle.write_bytes(_tar())
    assert window.open_image(str(leere))
    window.set_read_only(False)
    window.insert_paths([str(quelle)], 0)          # der Weg von |← / Strg+←
    assert window.tool.ist_tar and not window.tool.tar_leer


def test_multiple_folders_images_and_locked_are_refused_with_a_message(window, leere, tmp_path,
                                                                     monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    t1 = tmp_path / "a.tar"
    t2 = tmp_path / "b.tar"
    t1.write_bytes(_tar())
    t2.write_bytes(_tar())
    ordner = tmp_path / "o"
    ordner.mkdir()
    abbild = tmp_path / "z.img"
    abbild.write_bytes(b"x")
    assert window.open_image(str(leere))
    window.set_read_only(False)
    for pfade, wort in (([t1, t2], "EINE"), ([ordner], "Ordner"), ([abbild], "Abbild")):
        assert not window.insert_paths([str(p) for p in pfade])
        assert wort in window.statusBar().currentMessage(), (wort, window.statusBar().currentMessage())
        assert window.tool.tar_leer
    window.set_read_only(True)
    assert not window.insert_paths([str(t1)])
    assert "Schreibschutz" in window.statusBar().currentMessage()
    # belegte tar720: weiter gesperrt
    window.set_read_only(False)
    assert window.insert_paths([str(t1)])
    assert not window.act_schreiben.isEnabled()
    monkeypatch.setattr(QMessageBox, "critical", staticmethod(lambda *a, **k: None))
    assert not window.insert_paths([str(t2)])       # ein zweites .tar wird abgelehnt
    assert not window.tool.tar_leer

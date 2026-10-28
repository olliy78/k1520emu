"""Hinterlegte Bootabbilder des DiskTools (Ticket #25): Katalog, Dialog, Ablauf.

Waechter fuer ``data/bootsektor.yaml`` + ``data/bootsektoren/``:
  * jeder Eintrag taugt (Dateisystem bekannt, Datei da, passt in die Systemspuren)
  * jede .bin im Ordner ist eingetragen (kein verwaistes Abbild)
  * mit jedem Eintrag laesst sich eine Diskette anlegen, deren Systemspuren das Abbild tragen
und fuer den Dialog „Neue Diskette": Haken gesperrt ohne Eintrag, erster Treffer vorgewaehlt,
„Eigene .bin-Datei auswaehlen…" als letzter Eintrag, Dateidialog erst NACH „Diskette anlegen".
"""

from pathlib import Path

import pytest

from conftest import PROJECT_ROOT, requires_core, requires_disk, run_until_text

pytestmark = requires_disk

KATALOG = PROJECT_ROOT / "data" / "bootsektor.yaml"
ORDNER = PROJECT_ROOT / "data" / "bootsektoren"


def _kapazitaet():
    from app.core_binding.k1520disk import filesystems
    return {f.name: f.boot_capacity for f in filesystems()}


def _nur_mitgeliefert():
    return [(KATALOG, False)]


# ─── Katalog ─────────────────────────────────────────────────────────────────

def test_jeder_eintrag_des_mitgelieferten_katalogs_taugt():
    from app.disktool import bootsektoren
    alle, warn = bootsektoren.laden(_nur_mitgeliefert())
    assert not warn
    assert len(alle) >= 20
    nach_fs, warn = bootsektoren.brauchbare(_kapazitaet(), _nur_mitgeliefert())
    assert not warn, "\n".join(warn)
    assert sum(len(v) for v in nach_fs.values()) == len(alle)


def test_jedes_abbild_im_ordner_steht_im_katalog():
    from app.disktool import bootsektoren
    alle, _ = bootsektoren.laden(_nur_mitgeliefert())
    eingetragen = {e.path.resolve() for e in alle}
    for bin_ in ORDNER.glob("*.bin"):
        assert bin_.resolve() in eingetragen, f"{bin_.name} liegt in data/bootsektoren, steht aber nicht im Katalog"


def test_mit_jedem_eintrag_entsteht_eine_diskette_mit_diesen_systemspuren(tmp_path):
    from app.core_binding.k1520disk import DiskTool
    from app.disktool import bootsektoren
    nach_fs, _ = bootsektoren.brauchbare(_kapazitaet(), _nur_mitgeliefert())
    n = 0
    for fs, eintraege in nach_fs.items():
        for e in eintraege:
            n += 1
            ziel = tmp_path / f"d{n}.hfe"
            with DiskTool.create(ziel, fs, "", str(e.path)) as d:
                zurueck = tmp_path / f"d{n}.bin"
                d.read_boot_image(zurueck)
            soll = e.path.read_bytes()
            ist = zurueck.read_bytes()
            assert ist[:len(soll)] == soll, f"{e.description}: Systemspuren tragen das Abbild nicht"


def test_ergaenzung_des_anwenders_kommt_hinzu(tmp_path, monkeypatch):
    from app import paths
    from app.disktool import bootsektoren
    eigen_dir = tmp_path / "konfig"
    eigen_dir.mkdir()
    (eigen_dir / "meins.bin").write_bytes(b"\x00" * 128)
    (eigen_dir / "bootsektor.yaml").write_text(
        "version: 1\nbootsektoren:\n"
        "  - description: \"Mein CP/A\"\n    filesystem: cpa780\n    file: meins.bin\n"
        "  - description: \"Kaputt\"\n    filesystem: gibtsnicht\n    file: meins.bin\n",
        encoding="utf-8")
    monkeypatch.setattr(paths, "bootsektor_user_file", lambda: eigen_dir / "bootsektor.yaml")
    monkeypatch.setattr(paths, "bootsektor_file", lambda: KATALOG)

    nach_fs, warn = bootsektoren.brauchbare(_kapazitaet())
    texte = [e.description for e in nach_fs["cpa780"]]
    assert texte[0].startswith("CP/A — A5120")          # mitgeliefert zuerst
    assert texte[-1] == "Mein CP/A"                      # dann die eigenen
    assert nach_fs["cpa780"][-1].eigen
    assert nach_fs["cpa780"][-1].path == eigen_dir / "meins.bin"
    assert any("gibtsnicht" in w for w in warn)          # untauglich: gemeldet, nicht angeboten


def test_zu_grosses_abbild_wird_uebergangen(tmp_path):
    from app.disktool import bootsektoren
    gross = tmp_path / "gross.bin"
    gross.write_bytes(b"\x00" * (_kapazitaet()["cpa780"] + 1))
    kat = tmp_path / "bootsektor.yaml"
    kat.write_text("bootsektoren:\n  - {description: X, filesystem: cpa780, file: gross.bin}\n",
                   encoding="utf-8")
    nach_fs, warn = bootsektoren.brauchbare(_kapazitaet(), [(kat, True)])
    assert "cpa780" not in nach_fs
    assert any("Systemspuren" in w for w in warn)


# ─── Dialog ──────────────────────────────────────────────────────────────────

@pytest.fixture(scope="module")
def qt_app():
    pytest.importorskip("PySide6")
    from PySide6.QtWidgets import QApplication
    yield QApplication.instance() or QApplication([])


def _dialog(qt_app):
    from app.disktool import bootsektoren
    from app.disktool.ui.main_window import neu_eintraege
    from app.disktool.ui.neu_dialog import NeuDialog
    nach_fs, _ = bootsektoren.brauchbare(_kapazitaet(), _nur_mitgeliefert())
    return NeuDialog(neu_eintraege(), nach_fs)


def _waehle(dlg, name):
    i = dlg.wahl.findData(name)
    assert i >= 0, name
    dlg.wahl.setCurrentIndex(i)


def test_ohne_eintrag_ist_der_haken_gesperrt(qt_app):
    dlg = _dialog(qt_app)
    _waehle(dlg, "cpa800")                     # Datendiskette, keine Systemspuren
    assert not dlg.boot_haken.isEnabled() and dlg.bootabbild() is None
    _waehle(dlg, "udos_p8000")                 # NDOS P8000: (noch) kein geprueftes Abbild
    assert not dlg.boot_haken.isEnabled()
    _waehle(dlg, "udos50")                     # UDOS 5.0 dagegen schon
    assert dlg.boot_haken.isEnabled()


def test_erster_treffer_vorgewaehlt_eigene_datei_als_letzter(qt_app):
    from app.disktool.ui.neu_dialog import EIGENE_DATEI, EIGENE_DATEI_TEXT
    dlg = _dialog(qt_app)
    _waehle(dlg, "scpx640")
    assert dlg.boot_haken.isEnabled() and not dlg.boot_haken.isChecked()
    assert dlg.boot_wahl.currentIndex() == 0
    assert dlg.boot_wahl.currentText() == "SCPX 1526 V1.7 — A5120, 16×256"
    letzter = dlg.boot_wahl.count() - 1
    assert dlg.boot_wahl.itemData(letzter) == EIGENE_DATEI
    assert dlg.boot_wahl.itemText(letzter) == EIGENE_DATEI_TEXT
    assert dlg.bootabbild() is None            # ohne Haken: gewoehnliche Diskette
    assert not dlg.boot_wahl.isEnabled()
    dlg.boot_haken.setChecked(True)
    assert dlg.boot_wahl.isEnabled()
    assert dlg.bootabbild() == ORDNER / "boot_scpx640.bin"


def test_eigene_datei_wird_bei_der_wahl_erfragt(qt_app, tmp_path, monkeypatch):
    from PySide6.QtWidgets import QFileDialog
    from app.disktool.ui.neu_dialog import EIGENE_DATEI
    dlg = _dialog(qt_app)
    _waehle(dlg, "cpa780")
    dlg.boot_haken.setChecked(True)
    eigen = dlg.boot_wahl.findData(EIGENE_DATEI)

    monkeypatch.setattr(QFileDialog, "getOpenFileName", lambda *a, **k: ("", ""))
    dlg.boot_wahl.setCurrentIndex(eigen)
    dlg._boot_gewaehlt(eigen)                  # abgebrochen → zurueck auf Treffer 1
    assert dlg.boot_wahl.currentIndex() == 0

    datei = tmp_path / "eigen.bin"
    datei.write_bytes(b"\x00" * 64)
    monkeypatch.setattr(QFileDialog, "getOpenFileName", lambda *a, **k: (str(datei), ""))
    dlg.boot_wahl.setCurrentIndex(eigen)
    dlg._boot_gewaehlt(eigen)
    assert dlg.bootabbild() == datei
    assert "eigen.bin" in dlg.boot_wahl.currentText()


def test_dateisystemwechsel_nimmt_den_haken_zurueck(qt_app):
    dlg = _dialog(qt_app)
    _waehle(dlg, "cpa780")
    dlg.boot_haken.setChecked(True)
    _waehle(dlg, "scpx798")
    assert not dlg.boot_haken.isChecked() and dlg.bootabbild() is None
    assert dlg.boot_wahl.currentText() == "SCPX 1526 V1.7 — A5120, 5×1024"


# ─── Ablauf im Hauptfenster ──────────────────────────────────────────────────

def test_dateidialog_kommt_erst_nach_diskette_anlegen(qt_app, tmp_path, monkeypatch):
    """Reihenfolge: Dialog (mit Bootabbild) → Speichern-Dialog → Datentraegername → Diskette."""
    from PySide6.QtWidgets import QDialog, QInputDialog
    from app.disktool.ui import main_window as mw
    from app.disktool.ui.neu_dialog import NeuDialog

    ablauf = []
    ziel = tmp_path / "boot.hfe"

    def exec_(dlg):
        ablauf.append("dialog")
        _waehle(dlg, "cpa780")
        dlg.boot_haken.setChecked(True)
        return QDialog.Accepted

    def speichern(self, *a, **k):
        ablauf.append("speichern")
        return str(ziel)

    monkeypatch.setattr(NeuDialog, "exec", exec_)
    monkeypatch.setattr(mw.MainWindow, "_neue_datei_waehlen", speichern)
    monkeypatch.setattr(QInputDialog, "getText", lambda *a, **k: (ablauf.append("name") or "", True))
    monkeypatch.setattr(mw.MainWindow, "_darf_verwerfen", lambda self: True)

    w = mw.MainWindow()
    try:
        w._neu_dialog()
        assert ablauf == ["dialog", "speichern", "name"]
        assert w.tool is not None and ziel.is_file()
        zurueck = tmp_path / "zurueck.bin"
        w.tool.read_boot_image(zurueck)
        soll = (ORDNER / "boot_cpa780.bin").read_bytes()
        assert zurueck.read_bytes()[:len(soll)] == soll
    finally:
        w._close_tool()


# ─── UDOS 5.0: eine damit GEBAUTE Diskette startet ───────────────────────────

@requires_core
def test_udos50_bootdiskette_aus_dem_katalog_startet(tmp_path):
    """Leere `udos50`-Diskette + Katalogabbild + Dateien der Originaldiskette → Kaltstart.

    Haelt die drei Dinge fest, an denen das hing (doc/udos1715_diskettenformat.md §3.0b):
    zweiter Bootteil in Spur 16H Sektor 07–09, Verzeichnis von vorn gefuellt, Byte 179H = 01.
    Ohne einen davon endet der Start mit ERROR 45 bzw. ERROR C4.
    """
    from app.core_binding.k1520 import K1520Emulator
    from app.core_binding.k1520disk import DiskTool

    auszug = tmp_path / "auszug"
    auszug.mkdir()
    with DiskTool.open(PROJECT_ROOT / "tests" / "fixtures" / "disks" / "udos50_640k_a5120_system.img") as d:
        d.extract_all(auszug)
    # Die leere ERR1 kann UDOS nicht ablegen; DIRECTORY legt das Dateisystem selbst an.
    for n in ("ERR1", "ERR1.fileinfo", "DIRECTORY", "DIRECTORY.fileinfo"):
        (auszug / n).unlink(missing_ok=True)

    ziel = tmp_path / "udos50_neu.hfe"
    with DiskTool.create(ziel, "udos50", "SYSTEM", str(ORDNER / "boot_udos50.bin")) as d:
        d.insert_all(auszug)
        d.flush()

    emu = K1520Emulator()
    try:
        assert emu.mount_disk(0, str(ziel), "k5601_16x256", False), emu.last_error()
        emu.power_on()
        assert run_until_text(emu, "Neues Datum", max_cycles=120_000_000), (
            "UDOS 5.0 startet von der gebauten Diskette nicht:\n" + emu.screen_text())
        assert "UDOS VERSION 5.0" in emu.screen_text()
        assert "ERROR" not in emu.screen_text()
    finally:
        del emu


def test_dialog_fuer_die_neue_diskette_nimmt_einen_neuen_namen(qt_app, tmp_path, monkeypatch):
    """Ein SPEICHERN-Dialog (neuer Name oder Ueberschreiben), nicht der systemeigene, Name vorbelegt.

    Anlass: unter GNOME liess der GTK-Dialog nur vorhandene Abbilder waehlen.
    """
    from PySide6.QtCore import QTimer
    from PySide6.QtWidgets import QApplication, QFileDialog, QLineEdit
    from app.disktool.ui import main_window as mw

    monkeypatch.setattr(mw.MainWindow, "_disketten_ordner", lambda self: str(tmp_path))
    w = mw.MainWindow()
    gesehen = {}

    def antworten():
        for d in QApplication.topLevelWidgets():
            if isinstance(d, QFileDialog) and d.isVisible():
                gesehen.update(modus=d.acceptMode(), datei=d.fileMode(),
                               nativ=not d.testOption(QFileDialog.DontUseNativeDialog),
                               name=d.selectedFiles())
                d.selectNameFilter("DMK-Abbild (*.dmk)")
                d.filterSelected.emit("DMK-Abbild (*.dmk)")
                QApplication.processEvents()      # Filterwechsel ganz abarbeiten (sonst Wettlauf)
                gesehen["nach_filter"] = d.selectedFiles()
                # wie der Anwender: den Namen ins Namensfeld tippen
                d.findChild(QLineEdit, "fileNameEdit").setText("meine_diskette")
                QApplication.processEvents()
                d.accept()
                return
        QTimer.singleShot(20, antworten)

    try:
        QTimer.singleShot(20, antworten)
        pfad = w._neue_datei_waehlen("Neue Diskette anlegen", "neue_diskette.hfe",
                                     "HFE-Abbild (*.hfe);;DMK-Abbild (*.dmk)")
    finally:
        w._close_tool()
    assert gesehen["modus"] == QFileDialog.AcceptSave
    assert gesehen["datei"] == QFileDialog.AnyFile
    assert not gesehen["nativ"]
    assert Path(gesehen["name"][0]).name == "neue_diskette.hfe"
    assert Path(gesehen["nach_filter"][0]).name == "neue_diskette.dmk"
    assert Path(pfad) == tmp_path / "meine_diskette.dmk"          # Endung aus dem Filter

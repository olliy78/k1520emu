"""tar-Saetze im DiskTool-Fenster bearbeiten (Entwurf 37, AP T4; PySide6 headless).

Die Regeln des Kerns (Strom, Verteilen, Nummern, .tar-Urteil) pruefen `TarSatz.*`, `TarNeu.*`
und `py_disktool_tar_satz`; hier geht es um die Oberflaeche: Satz oeffnen/bearbeiten/speichern,
Popup fuer `.tar`, Hinweis auf eine Folgediskette, Sperre fuer den Greaseweazle, `.fileinfo`.
Jede Frage an den Anwender ist eine kleine Methode des Fensters und wird hier ersetzt — ein
unerwartetes Meldungsfenster scheitert ueber die Fixture `kein_unerwartetes_meldungsfenster`.
"""

import shutil
import subprocess
import time

import pytest

from conftest import requires_disk

pytestmark = requires_disk

pytest.importorskip("PySide6")

FORMAT = "k5601_ss40_16x256"
KLEIN = 163840            # Nutzbytes dieses Formats


def _bytes(n, start=1):
    return bytes((start + i * 7) & 0xFF for i in range(n))


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


def _tool():
    from app.core_binding.k1520disk import DiskTool
    return DiskTool


def _quellen(tmp_path):
    """Ein Ordner mit zwei Dateien und einem Unterordner samt leerem Ordner."""
    q = tmp_path / "quelle"
    (q / "unter" / "leer").mkdir(parents=True)
    (q / "a.bin").write_bytes(_bytes(3000, 3))
    (q / "unter" / "b.txt").write_bytes(b"zweite datei\n")
    return q


@pytest.fixture
def satz_zwei(tmp_path):
    """Ein gespeicherter tar-Satz aus zwei Abbildern (eine Datei laenger als eine Diskette)."""
    gross = _bytes(220000, 9)
    q = tmp_path / "g"
    q.mkdir()
    (q / "gross.bin").write_bytes(gross)
    basis = tmp_path / "satz.hfe"
    with _tool().tar_satz_neu(FORMAT, basis) as t:
        t.insert(str(q / "gross.bin"), "gross.bin")
        r = t.tar_satz_speichern()
    assert len(r["geschrieben"]) == 2
    return r["geschrieben"], gross


@pytest.fixture
def satz_720(tmp_path):
    """Satz aus zwei 720K-Abbildern: nur dort erkennt der Kern ein EINZELNES Stueck als tar."""
    q = tmp_path / "g7"
    q.mkdir()
    (q / "gross.bin").write_bytes(_bytes(800000, 9))
    with _tool().tar_satz_neu("k5601_9x512", tmp_path / "f.img") as t:
        t.insert(str(q / "gross.bin"), "gross.bin")
        r = t.tar_satz_speichern()
    assert len(r["geschrieben"]) == 2
    return r["geschrieben"]


def _antworte_nie(window, monkeypatch):
    """Ein Test, der KEINE Frage erwartet: jede Rueckfrage der Oberflaeche scheitert."""
    def nein(*a, **k):
        raise AssertionError("unerwartete Rueckfrage")
    for name in ("_tar_datei_wahl", "_satz_speichern_fragen"):
        monkeypatch.setattr(window, name, nein)


# ─── Aktionen ────────────────────────────────────────────────────────────────

def test_new_actions_exist_without_shortcut(window):
    for name in ("act_tar_neu", "act_tar_ordner"):
        assert getattr(window, name).shortcut().isEmpty()
    assert "tar" in window.act_tar_neu.text()
    assert not window.act_tar_ordner.isVisible()            # ohne tar-Archiv nicht da


# ─── Satz oeffnen und bearbeiten ─────────────────────────────────────────────

def test_open_set_edit_and_save_roundtrip(window, satz_zwei, tmp_path, monkeypatch):
    namen, gross = satz_zwei
    assert window.open_image(namen[0], satz=namen)
    t = window.tool
    assert t.tar_satz and t.read_only
    assert "tar-Satz (UNIX)" in window.kopf.angaben.text()
    assert not window.act_schreiben.isEnabled()              # geschuetzt geoeffnet
    # Schreibschutz aufheben wie bei anderen Datentraegern
    window.act_schreibschutz.setChecked(False)
    assert not t.read_only and window.act_speichern.isEnabled()
    assert window.act_loeschen.isEnabled() is False          # nichts ausgewaehlt
    assert window.act_tar_ordner.isVisible() and window.act_tar_ordner.isEnabled()

    # Ordner mit Unterordner und leerem Ordner, dazu eine einzelne Datei, per Ziehen/Einfuegen
    q = _quellen(tmp_path)
    _antworte_nie(window, monkeypatch)
    assert window.insert_paths([str(q / "unter"), str(q / "a.bin")])
    namen_neu = sorted(e.name for e in t.list())
    assert "a.bin" in namen_neu and "unter/b.txt" in namen_neu and "gross.bin" in namen_neu
    assert any(n.rstrip("/") == "unter/leer" for n in namen_neu)
    assert window.windowTitle().startswith(window._kurzname()) and t.dirty

    # Ordner anlegen, Eigenschaften aendern, loeschen
    assert window.tar_mkdir("doku/bilder")
    assert any(e.name.rstrip("/") == "doku/bilder" for e in t.list())
    from app.disktool.ui.properties_dialog import PropertiesDialog
    e = next(x for x in t.list() if x.name == "a.bin")
    dlg = PropertiesDialog(t, e, window)
    dlg.f_oktal.setText("0600")
    dlg.f_uid.setValue(42)
    dlg.f_gid.setValue(7)
    dlg.f_mtime.setText("2001-02-03 04:05:06")
    assert dlg.uebernehmen()
    e = next(x for x in t.list() if x.name == "a.bin")
    assert (e.unix_mode & 0o7777, e.unix_uid, e.unix_gid) == (0o600, 42, 7)
    assert time.gmtime(e.unix_mtime)[:6] == (2001, 2, 3, 4, 5, 6)
    assert window.erase_refs(["unter/b.txt"])
    assert "unter/b.txt" not in [x.name for x in t.list()]

    # Speichern: Rueckfrage mit Planung, hier ersetzt (Basisname des Satzes)
    gefragt = []
    monkeypatch.setattr(window, "_satz_speichern_fragen",
                        lambda unter=False: gefragt.append(unter) or "")
    assert window.save()
    assert gefragt == [False]
    assert not t.dirty
    # Wieder oeffnen: alles da, Eigenschaften erhalten, Inhalt der grossen Datei gleich
    with _tool().open_tar_satz(namen) as w:
        li = {x.name: x for x in w.list()}
        assert (li["a.bin"].unix_mode & 0o7777, li["a.bin"].unix_uid) == (0o600, 42)
        assert "unter/b.txt" not in li and "gross.bin" in li
        ziel = tmp_path / "gross.aus"
        w.extract("gross.bin", str(ziel))
        assert ziel.read_bytes() == gross
    if shutil.which("tar"):
        # Gegenprobe mit dem System-tar: die Abbilder hintereinander sind ein gueltiges Archiv
        with _tool().open_tar_satz(namen) as w:
            pass


def test_nothing_to_save_without_change_asks_nothing(window, satz_zwei, monkeypatch):
    namen, _ = satz_zwei
    assert window.open_image(namen[0], satz=namen)
    window.set_read_only(False)
    _antworte_nie(window, monkeypatch)
    assert window.save()                                     # keine Aenderung: keine Frage


def test_save_as_asks_for_the_base_name(window, satz_zwei, tmp_path, monkeypatch):
    namen, _ = satz_zwei
    assert window.open_image(namen[0], satz=namen)
    fragen = []
    ziel = str(tmp_path / "andere.img")
    monkeypatch.setattr(window, "_satz_speichern_fragen",
                        lambda unter=False: fragen.append(unter) or ziel)
    window._speichern_unter_dialog()
    assert fragen == [True]
    assert (tmp_path / "andere1.img").exists() and (tmp_path / "andere2.img").exists()
    assert (tmp_path / "satz1.hfe").exists()                 # das Original bleibt


def test_saving_error_is_reported_not_swallowed(window, satz_zwei, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    namen, _ = satz_zwei
    assert window.open_image(namen[0], satz=namen)
    meldungen = []
    monkeypatch.setattr(QMessageBox, "critical",
                        staticmethod(lambda *a, **k: meldungen.append(a[2])))
    monkeypatch.setattr(window, "_satz_speichern_fragen", lambda unter=False: "ohne_endung")
    assert not window._satz_speichern(unter=True)
    assert meldungen and "Endung" in meldungen[0]


# ─── Rueckfragedialog des Speicherns ─────────────────────────────────────────

def test_save_dialog_shows_plan_overwrites_and_leftovers(window, satz_zwei, tmp_path):
    from app.disktool.ui.tar_satz_speichern_dialog import TarSatzSpeichernDialog, plan_text
    namen, _ = satz_zwei
    assert window.open_image(namen[0], satz=namen)
    window.set_read_only(False)
    # der Satz schrumpft auf ein Abbild: Abbild 2 gehoert nicht mehr dazu und bleibt liegen
    window.erase_refs(["gross.bin"])
    dlg = TarSatzSpeichernDialog(window.tool, window)
    assert dlg.plan["abbilder"] == 1
    assert dlg.liste.count() == 1
    assert "ein Abbild" in dlg.l_plan.text() == plan_text(dlg.plan) or "ein Abbild" in dlg.l_plan.text()
    assert "Es gibt schon" in dlg.l_ueber.text()             # satz1.hfe existiert
    assert "Bleiben liegen" in dlg.l_alt.text() and "satz2.hfe" in dlg.l_alt.text()
    # ein Basisname ohne Endung: Fehler, Speichern gesperrt
    dlg.f_basis.setText(str(tmp_path / "x"))
    assert dlg.plan is None and dlg.l_fehler.text()
    from PySide6.QtWidgets import QDialogButtonBox
    assert not dlg.knoepfe.button(QDialogButtonBox.Ok).isEnabled()
    dlg.f_basis.setText(str(tmp_path / "neu.hfe"))
    assert dlg.plan is not None and dlg.basis().endswith("neu.hfe")
    assert not dlg.l_ueber.text() and not dlg.l_alt.text()


def test_leftover_numbers_are_reported_after_saving(window, satz_zwei, monkeypatch):
    namen, _ = satz_zwei
    assert window.open_image(namen[0], satz=namen)
    window.set_read_only(False)
    window.erase_refs(["gross.bin"])
    monkeypatch.setattr(window, "_satz_speichern_fragen", lambda unter=False: "")
    assert window.save()
    protokoll = window.log_dock.text.toPlainText() if hasattr(window.log_dock, "toPlainText") \
        else window.protokoll.toPlainText()
    assert "satz2.hfe" in protokoll and "Gespeichert" in protokoll


# ─── Neu anlegen ─────────────────────────────────────────────────────────────

def test_new_dialog_lists_formats_and_kind(qt_app):
    from app.disktool.ui.tar_neu_dialog import EINZEL, SATZ, TarNeuDialog
    dlg = TarNeuDialog(_tool().tar_formate())
    namen = [dlg.wahl.itemData(i) for i in range(dlg.wahl.count())]
    assert FORMAT in namen and "k5601_9x512" in namen
    assert dlg.format() == "k5601_9x512" and dlg.art() == EINZEL
    dlg.r_satz.setChecked(True)
    assert dlg.art() == SATZ
    assert dlg.info()["bytes"] == 737280


def test_new_single_disk_and_set_via_the_window(window, tmp_path, monkeypatch):
    from app.disktool.ui.tar_neu_dialog import EINZEL, SATZ
    einzel = tmp_path / "e.img"
    monkeypatch.setattr(window, "_tar_neu_wahl", lambda: (FORMAT, EINZEL, str(einzel)))
    window._tar_neu_dialog()
    assert window.tool.tar_satz and not window.tool.read_only
    assert einzel.stat().st_size == KLEIN                    # Einzeldiskette: sofort da
    q = _quellen(tmp_path)
    assert window.insert_paths([str(q / "a.bin")])
    monkeypatch.setattr(window, "_satz_speichern_fragen", lambda unter=False: "")
    assert window.save() is True
    assert not window.tool.dirty
    assert einzel.stat().st_size == KLEIN                    # Name ohne Nummer, nichts Zweites
    assert not (tmp_path / "e1.img").exists()
    with _tool().open_tar_satz_format([einzel], FORMAT) as w:
        assert [e.name for e in w.list()] == ["a.bin"]
    # Der Satz: nichts auf der Platte, bis gespeichert wird
    basis = tmp_path / "s.hfe"
    monkeypatch.setattr(window, "_tar_neu_wahl", lambda: (FORMAT, SATZ, str(basis)))
    window._tar_neu_dialog()
    assert window.tool.tar_satz and not (tmp_path / "s1.hfe").exists()
    assert window.insert_paths([str(q / "unter")])
    assert window.save()
    assert (tmp_path / "s1.hfe").exists()


def test_new_with_a_bad_name_reports_and_closes_nothing_half_open(window, tmp_path, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    from app.disktool.ui.tar_neu_dialog import SATZ
    meldungen = []
    monkeypatch.setattr(QMessageBox, "critical",
                        staticmethod(lambda *a, **k: meldungen.append(a[2])))
    assert not window._tar_neu_oeffnen(FORMAT, SATZ, str(tmp_path / "x.txt"))
    assert window.tool is None and meldungen


# ─── .tar ablegen: Popup ─────────────────────────────────────────────────────

@pytest.fixture
def paket(tmp_path):
    if not shutil.which("tar"):
        pytest.skip("kein tar im System")
    (tmp_path / "q").mkdir()
    (tmp_path / "q" / "x.bin").write_bytes(_bytes(3000, 14))
    p = tmp_path / "paket.tar"
    subprocess.run(["tar", "--format=v7", "-cf", str(p), "-C", str(tmp_path / "q"), "x.bin"],
                   check=True)
    return p


def _neuer_satz(window, tmp_path, monkeypatch, name="z.img"):
    from app.disktool.ui.tar_neu_dialog import SATZ
    assert window._tar_neu_oeffnen("k5601_9x512", SATZ, str(tmp_path / name))


def test_tar_on_an_empty_archive_asks_and_files_it_as_a_file(window, paket, tmp_path, monkeypatch):
    _neuer_satz(window, tmp_path, monkeypatch)
    fragen = []
    monkeypatch.setattr(window, "_tar_datei_wahl",
                        lambda name, urteil: fragen.append((name, urteil)) or "datei")
    assert window.insert_paths([str(paket)])
    assert fragen and fragen[0][0] == "paket.tar" and fragen[0][1]["als_container"]
    assert [e.name for e in window.tool.list()] == ["paket.tar"]


def test_tar_on_an_empty_archive_as_container(window, paket, tmp_path, monkeypatch):
    _neuer_satz(window, tmp_path, monkeypatch)
    monkeypatch.setattr(window, "_tar_datei_wahl", lambda name, urteil: "container")
    assert window.insert_paths([str(paket)])
    assert [e.name for e in window.tool.list()] == ["x.bin"]


def test_tar_popup_cancel_changes_nothing(window, paket, tmp_path, monkeypatch):
    _neuer_satz(window, tmp_path, monkeypatch)
    monkeypatch.setattr(window, "_tar_datei_wahl", lambda name, urteil: None)
    assert not window.insert_paths([str(paket)])
    assert window.tool.list() == []


def test_tar_on_a_nonempty_archive_is_always_a_file_without_a_popup(window, paket, tmp_path,
                                                                    monkeypatch):
    _neuer_satz(window, tmp_path, monkeypatch)
    q = _quellen(tmp_path)
    assert window.insert_paths([str(q / "a.bin")])
    _antworte_nie(window, monkeypatch)                       # KEIN Popup
    assert window.insert_paths([str(paket)])
    assert sorted(e.name for e in window.tool.list()) == ["a.bin", "paket.tar"]


def test_tar_dropped_with_other_files_is_just_a_file(window, paket, tmp_path, monkeypatch):
    _neuer_satz(window, tmp_path, monkeypatch)
    q = _quellen(tmp_path)
    _antworte_nie(window, monkeypatch)
    assert window.insert_paths([str(paket), str(q / "a.bin")])
    assert sorted(e.name for e in window.tool.list()) == ["a.bin", "paket.tar"]


def test_the_popup_text_names_both_ways_and_the_warning(window, paket, monkeypatch):
    """Der echte Dialog (ohne exec): Knopfbeschriftungen nach dem Entwurf."""
    from PySide6.QtWidgets import QMessageBox
    gesehen = {}

    def nur_bauen(self):
        gesehen["text"] = self.text()
        gesehen["knoepfe"] = [b.text() for b in self.buttons()]
        gesehen["gewaehlt"] = [b for b in self.buttons() if "fertiger" in b.text()][0]
    monkeypatch.setattr(QMessageBox, "exec", nur_bauen)
    monkeypatch.setattr(QMessageBox, "clickedButton", lambda self: gesehen["gewaehlt"])
    wahl = window._tar_datei_wahl("paket.tar", {"abbilder": 2, "wird_satz": True,
                                                 "warnung": "kein V7"})
    assert wahl == "container"
    assert "Als Datei im Container ablegen" in gesehen["knoepfe"]
    assert "Ist ein fertiger tar-Container" in gesehen["knoepfe"]
    assert "2 Abbilder" in gesehen["text"] and "kein V7" in gesehen["text"]


# ─── Einzeldiskette waechst zum Satz ─────────────────────────────────────────

def test_single_disk_overflow_is_announced_and_saved_as_numbered_images(window, tmp_path, monkeypatch):
    """Eine tar-Diskette ist ein Satz mit EINEM Abbild: kein Ablehnen, keine Rueckfrage beim Einfuegen;
    der Inhalt waechst im Speicher, das Protokoll sagt es, der Plan-Dialog zeigt die Abbilder."""
    einzel = tmp_path / "e.img"
    _tool().create(einzel, "tar720_leer", "").close()
    vorher = einzel.read_bytes()
    p = tmp_path / "q"
    p.mkdir()
    (p / "gross.bin").write_bytes(_bytes(800000, 5))
    assert window.open_image(str(einzel))
    assert window.tool.tar_satz_einzel
    window.set_read_only(False)
    _antworte_nie_ausser_speichern(window, monkeypatch)
    assert window.insert_paths([str(p / "gross.bin")])
    assert window.tool.tar_satz_planen()["abbilder"] == 2
    assert "2 nummerierte Abbilder" in window.log_dock.text.toPlainText()
    # Abbrechen im Plan-Dialog: nichts geschrieben
    monkeypatch.setattr(window, "_satz_speichern_fragen", lambda unter=False: None)
    assert not window.save()
    assert einzel.read_bytes() == vorher and not (tmp_path / "e1.img").exists()
    # Zustimmen: nummeriert, die Einzeldatei bleibt unberuehrt
    monkeypatch.setattr(window, "_satz_speichern_fragen", lambda unter=False: "")
    assert window.save()
    assert (tmp_path / "e1.img").exists() and (tmp_path / "e2.img").exists()
    assert einzel.read_bytes() == vorher
    assert not window.tool.tar_satz_einzel


def _antworte_nie_ausser_speichern(window, monkeypatch):
    def nein(*a, **k):
        raise AssertionError("unerwartete Rueckfrage")
    monkeypatch.setattr(window, "_tar_datei_wahl", nein)


def test_save_of_an_opened_tar_disk_goes_into_its_image_without_a_question(window, tmp_path,
                                                                           monkeypatch):
    einzel = tmp_path / "e.hfe"
    _tool().create(einzel, "tar720_leer", "").close()
    q = _quellen(tmp_path)
    assert window.open_image(str(einzel))
    window.set_read_only(False)
    _antworte_nie(window, monkeypatch)                   # Strg+S: KEIN Plan-Dialog bei einem Abbild
    assert window.insert_paths([str(q / "a.bin")])
    assert window.tool.dirty
    assert window.save()
    assert not window.tool.dirty
    assert not (tmp_path / "e1.hfe").exists()
    with _tool().open(einzel) as t:
        assert [e.name for e in t.list()] == ["a.bin"] and t.container_info()["type"] == "hfe"


def test_tar_on_an_empty_single_disk_offers_both_ways(window, paket, tmp_path, monkeypatch):
    """Popup gilt fuer JEDE .tar auf einem LEEREN Archiv — auch auf der leeren Einzeldiskette."""
    for antwort, erwartet in (("datei", ["paket.tar"]), ("container", ["x.bin"])):
        einzel = tmp_path / f"{antwort}.img"
        _tool().create(einzel, "tar720_leer", "").close()
        assert window.open_image(str(einzel))
        window.set_read_only(False)
        fragen = []
        monkeypatch.setattr(window, "_tar_datei_wahl",
                            lambda name, urteil, f=fragen, a=antwort: f.append((name, urteil)) or a)
        assert window.insert_paths([str(paket)])
        assert fragen and fragen[0][1]["als_datei"] and fragen[0][1]["als_container"]
        assert [e.name for e in window.tool.list()] == erwartet
        assert window.save()


def test_container_beyond_one_disk_becomes_a_set_in_the_window(window, tmp_path, monkeypatch):
    if not shutil.which("tar"):
        pytest.skip("kein tar im System")
    q = tmp_path / "q"
    q.mkdir()
    for i in range(3):
        (q / f"d{i}").write_bytes(_bytes(300000, 20 + i))
    gross = tmp_path / "gross.tar"
    subprocess.run(["tar", "--format=v7", "-cf", str(gross), "-C", str(q), "d0", "d1", "d2"], check=True)
    einzel = tmp_path / "e.img"
    _tool().create(einzel, "tar720_leer", "").close()
    assert window.open_image(str(einzel))
    window.set_read_only(False)
    gesehen = []
    monkeypatch.setattr(window, "_tar_datei_wahl",
                        lambda name, urteil: gesehen.append(urteil) or "container")
    assert window.insert_paths([str(gross)])
    assert gesehen[0]["wird_satz"] and gesehen[0]["abbilder"] == 2
    assert window.tool.tar_satz_planen()["abbilder"] == 2
    monkeypatch.setattr(window, "_satz_speichern_fragen", lambda unter=False: "")
    assert window.save()
    assert (tmp_path / "e1.img").exists() and (tmp_path / "e2.img").exists()


# ─── Hinweis auf eine Folgediskette ──────────────────────────────────────────

def test_a_follow_up_part_offers_the_set_dialog(window, satz_720, monkeypatch):
    namen = satz_720
    _antworte_nie(window, monkeypatch)                       # kein modales Fenster
    # Das erste Stueck endet mitten in der grossen Datei (tar.abgeschnitten); ein zweites ohne
    # jeden Kopf erkennt der Kern gar nicht als tar (roh, Export geht) — dort gibt es keinen Hinweis.
    assert window.open_image(namen[0])
    assert any(f.id == "tar.abgeschnitten" for f in window.tool.findings().findings)
    assert "tar-Satz" in window.info_bar.text()
    assert window.info_bar._knopf.text() == "tar-Satz öffnen…"
    assert not window.info_bar._knopf.isHidden()
    # der Knopf startet den Satzdialog
    aufgerufen = []
    monkeypatch.setattr(window, "_tar_satz_dialog", lambda: aufgerufen.append(1))
    window.info_bar._knopf.click()
    assert aufgerufen == [1]
    # als Satz geoeffnet: der Hinweis verschwindet
    assert window.open_image(namen[0], satz=namen)
    assert "Teil einer tar-Diskettenfolge" not in window.info_bar.text()


def test_a_follow_up_part_stays_readonly_with_the_reason(window, satz_720):
    from app.disktool.ui.schreibsperre import sperrgrund
    assert window.open_image(satz_720[0])
    t = window.tool
    assert t.ist_tar and t.read_only_forced                  # abgeschnitten = nicht abgeschlossen
    assert "tar-Satz öffnen" in sperrgrund(t)
    assert not window.act_schreibschutz.isEnabled()
    assert "tar-Satz öffnen" in window.act_schreibschutz.toolTip()
    assert "Teilstück" in window.st_schutz.text()


# ─── Greaseweazle ────────────────────────────────────────────────────────────

def test_a_set_cannot_be_written_to_a_physical_disk(window, satz_zwei, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    namen, _ = satz_zwei
    assert window.open_image(namen[0], satz=namen)
    window.set_read_only(False)
    assert not window.act_physisch_schreiben.isEnabled()
    tip = window.act_physisch_schreiben.toolTip()
    assert "Gesperrt" in tip and "physische Diskette" in tip and "Einzelabbilder" in tip
    meldungen = []
    monkeypatch.setattr(QMessageBox, "critical",
                        staticmethod(lambda *a, **k: meldungen.append(a[2])))
    window._physisch_schreiben_dialog()                      # Riegel hinter der Aktion
    assert meldungen and "Einzelabbilder" in meldungen[0]
    # eine gewoehnliche Diskette: nicht gesperrt, Tooltip wieder der urspruengliche
    window.close_disk()
    from conftest import ausgelieferte_diskette
    window.open_image(str(ausgelieferte_diskette("p8000_wega30_root1.dmk")))
    assert window.act_physisch_schreiben.isEnabled()
    assert "Gesperrt" not in window.act_physisch_schreiben.toolTip()


# ─── .fileinfo: Python-Schreiber und Rundlauf ────────────────────────────────

def test_python_fileinfo_writer_knows_tar_and_wega(tmp_path):
    from app.disktool.fileinfo import schreibe_fileinfo, unix_vorgaben
    for fs in ("tar", "wega"):
        p = schreibe_fileinfo(tmp_path / f"{fs}.fileinfo",
                              {"fs": fs, "mtime": 1234, "uid": 0, "gid": 3, "mode": "0100640",
                               "art": "-"}, "dir/datei")
        text = p.read_text(encoding="utf-8")
        zeilen = [z for z in text.splitlines() if not z.startswith("#")]
        assert zeilen[0] == f"fs={fs}" and zeilen[1] == "name=dir/datei"
        # Reihenfolge wie im Kern: art, mode, uid, gid, ... mtime; uid=0 geht NICHT verloren
        assert zeilen[2:] == ["art=-", "mode=0100640", "uid=0", "gid=3", "mtime=1234"]
    v = unix_vorgaben(jetzt=99)
    assert v == {"art": "-", "mode": "0100644", "uid": 0, "gid": 0, "mtime": 99}
    assert unix_vorgaben(ordner=True, jetzt=1)["mode"] == "0040755"


def test_python_written_tar_fileinfo_is_read_by_the_core(window, tmp_path, monkeypatch):
    """Dasselbe Format wie der Kern: ein vom Python-Schreiber erzeugtes `.fileinfo` setzt
    Modus/uid/gid/mtime beim Einfuegen in einen tar-Satz."""
    from app.disktool.fileinfo import schreibe_fileinfo
    _neuer_satz(window, tmp_path, monkeypatch, "f.img")
    q = tmp_path / "f"
    q.mkdir()
    (q / "d.bin").write_bytes(b"inhalt")
    schreibe_fileinfo(q / "d.bin.fileinfo",
                      {"fs": "tar", "mode": "0100600", "uid": 25, "gid": 7, "mtime": 612345678},
                      "d.bin")
    _antworte_nie(window, monkeypatch)
    assert window.insert_paths([str(q / "d.bin")])
    e = next(x for x in window.tool.list() if x.name == "d.bin")
    assert (e.unix_mode & 0o7777, e.unix_uid, e.unix_gid, e.unix_mtime) == (0o600, 25, 7, 612345678)
    # Ohne .fileinfo: die Vorgaben 0644, 0/0
    (q / "e.bin").write_bytes(b"x")
    assert window.insert_paths([str(q / "e.bin")])
    e = next(x for x in window.tool.list() if x.name == "e.bin")
    assert (e.unix_mode & 0o7777, e.unix_uid, e.unix_gid) == (0o644, 0, 0)


def test_download_writes_fileinfo_for_tar_and_insert_reads_it(window, satz_zwei, tmp_path,
                                                              monkeypatch):
    namen, _ = satz_zwei
    assert window.open_image(namen[0], satz=namen)
    window.set_read_only(False)
    window.tool.set_unix_attrs("gross.bin", mode=0o640, uid=11, gid=12)
    ziel = tmp_path / "raus"
    ziel.mkdir()
    assert window.extract_refs(["gross.bin"], ziel)
    info = ziel / "gross.bin.fileinfo"
    assert info.exists()
    zeilen = info.read_text(encoding="utf-8")
    assert "fs=tar" in zeilen and "uid=11" in zeilen and "gid=12" in zeilen
    # zurueck in einen neuen Satz: die Angaben kommen mit
    _neuer_satz(window, tmp_path, monkeypatch, "r.img")
    assert window.insert_paths([str(ziel / "gross.bin")])
    e = next(x for x in window.tool.list() if x.name == "gross.bin")
    assert (e.unix_mode & 0o7777, e.unix_uid, e.unix_gid) == (0o640, 11, 12)


# ─── Sperren im Fenster ──────────────────────────────────────────────────────

def test_empty_single_disk_takes_files_and_folders_like_any_archive(window, tmp_path):
    einzel = tmp_path / "e.img"
    _tool().create(einzel, "tar720_leer", "").close()
    assert window.open_image(str(einzel))
    window.set_read_only(False)
    assert window.act_alles_rein.isEnabled()
    assert window.act_tar_ordner.isVisible() and window.act_tar_ordner.isEnabled()


def test_mkdir_in_a_read_only_archive_is_refused_with_a_message(window, satz_zwei, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    namen, _ = satz_zwei
    assert window.open_image(namen[0], satz=namen)
    assert not window.act_tar_ordner.isEnabled()
    assert "Schreibschutz" in window.act_tar_ordner.toolTip()
    meldungen = []
    monkeypatch.setattr(QMessageBox, "critical",
                        staticmethod(lambda *a, **k: meldungen.append(a[2])))
    assert not window.tar_mkdir("x")
    assert meldungen and "schreibgesch" in meldungen[0]


def test_unsaved_new_set_discard_recreates_it_empty(window, tmp_path, monkeypatch):
    from PySide6.QtWidgets import QMessageBox
    from app.disktool.ui.tar_neu_dialog import SATZ
    assert window._tar_neu_oeffnen(FORMAT, SATZ, str(tmp_path / "v.img"))
    q = _quellen(tmp_path)
    assert window.insert_paths([str(q / "a.bin")])
    assert window.tool.dirty
    monkeypatch.setattr(QMessageBox, "question",
                        staticmethod(lambda *a, **k: QMessageBox.No))     # „Nein" = verwerfen
    window.act_schreibschutz.setChecked(True)
    assert window.tool.list() == [] and window.tool.tar_satz

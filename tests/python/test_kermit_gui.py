"""Die Oberflaeche von K1520 Kermit (``app/kermittool/``) — headless ueber ``QT_QPA_PLATFORM=offscreen``.

Geprueft wird die Verdrahtung, nicht die Pixel: Fenster baut, jede Bedienung ist eine
``QAction``, Kuerzel nur mit Strg+Umschalt, die Kuerzeltabelle des Handbuchs stimmt in beiden
Richtungen, und ein Rundlauf **GUI-Client <-> Bibliotheks-Server** (Python-Faden, Loopback-Telnet,
Port 0) mit Verzeichnisanzeige, Holen, Schreiben, Loeschen, Abbruch.  Dazu der Server-Modus
(Freigabe Pflicht, nur lesen, wieder lauschen), die Konfiguration (Kennwoerter nur an den erlaubten Orten)
und die Client-Anmeldung auf Abruf (K11: Popup, Wiederholung, Speichern im Klartext).

Drag&Drop wird ueber die Auswertung der Mime-Daten (``ui/ablage.py``) und die Annahme-Methoden des
Fensters geprueft — das Fallenlassen selbst erzeugt kein Test.  Kein greaseweazle noetig.
"""

import os
import re
import threading
import time
from pathlib import Path

import pytest

from conftest import PROJECT_ROOT, requires_kermit

pytestmark = requires_kermit

PySide6 = pytest.importorskip("PySide6")

from PySide6.QtCore import QMimeData, Qt, QUrl                       # noqa: E402
from PySide6.QtGui import QAction                                    # noqa: E402
from PySide6.QtWidgets import QMessageBox                            # noqa: E402

from app.kermittool import binding, konfig                           # noqa: E402
from app.kermittool.binding import Kermit, KermitError               # noqa: E402


# ─── Hilfen ──────────────────────────────────────────────────────────────────

def warte(qapp, bedingung, frist=30.0):
    """Ereignisschleife drehen, bis ``bedingung()`` wahr wird; ``False`` bei Ueberschreitung."""
    ende = time.monotonic() + frist
    while time.monotonic() < ende:
        qapp.processEvents()
        if bedingung():
            return True
        time.sleep(0.005)
    qapp.processEvents()
    return bool(bedingung())


@pytest.fixture
def lokal(tmp_path):
    d = tmp_path / "lokal"
    d.mkdir()
    return d


@pytest.fixture
def fenster(qapp, tmp_path, lokal):
    from app.kermittool.ui.main_window import MainWindow
    f = MainWindow(konfig_pfad=str(tmp_path / "kermit.yaml"), ordner=str(lokal))
    f.show()
    qapp.processEvents()
    yield f
    f.close()
    qapp.processEvents()


class Gegenstelle:
    """Ein Kermit-Server der Bibliothek in einem Faden, vor den die GUI als Client tritt."""

    def __init__(self, freigabe: Path, login=None):
        self.freigabe = freigabe
        self.lauscher = Kermit.telnet_server(0)
        if login:                                           # (name, kennwort): Server mit Anmeldezwang (K12)
            self.lauscher.set_server_login(True, login[0], login[1])
        self.port = self.lauscher.listen_port
        self.fehler = []
        self._t = threading.Thread(target=self._lauf)
        self._t.start()

    def _lauf(self):
        try:
            self.lauscher.accept(30000)
            self.lauscher.serve(str(self.freigabe))
        except KermitError as e:
            self.fehler.append(e)

    def ende(self):
        self.lauscher.abort()
        self._t.join(15)
        self.lauscher.close()


@pytest.fixture
def gegenstelle(tmp_path):
    frei = tmp_path / "freigabe"
    frei.mkdir()
    g = Gegenstelle(frei)
    yield g
    g.ende()


def client_verbinden(qapp, fenster, port):
    fenster.client_verbinden({"leitung": "telnet", "aufbau": "verbinden", "host": "127.0.0.1", "port": port})
    assert warte(qapp, lambda: fenster.zustand_art == "verbunden"
                 and fenster.fern_view.baum.topLevelItemCount() > 0), fenster._zustand_text


def namen_links(fenster):
    b = fenster.fern_view.baum
    return [b.topLevelItem(i).text(0) for i in range(b.topLevelItemCount())]


def auswaehlen(baum, name):
    baum.clearSelection()
    for i in range(baum.topLevelItemCount()):
        if baum.topLevelItem(i).text(0) == name:
            baum.topLevelItem(i).setSelected(True)
            return
    raise AssertionError(f"{name} nicht in der Liste: " +
                         str([baum.topLevelItem(i).text(0) for i in range(baum.topLevelItemCount())]))


def _zeile(baum, name):
    for i in range(baum.topLevelItemCount()):
        if baum.topLevelItem(i).text(0) == name:
            return baum.topLevelItem(i)
    raise AssertionError(name)


def ruhe(qapp, fenster):
    assert warte(qapp, lambda: not fenster.arbeiter.beschaeftigt)


# ─── Fenster, Aktionen, Kuerzel ──────────────────────────────────────────────

def test_fenster_baut_und_jede_bedienung_ist_eine_aktion(fenster):
    from app.kermittool.ui import actions
    for name in actions.namen():
        a = getattr(fenster, f"act_{name}")
        assert isinstance(a, QAction), name
        assert not a.statusTip() == "", name
    titel = [a.text() for a in fenster.menuBar().actions()]
    assert titel == ["&Datei", "&Verbindung", "&Ansicht", "&Hilfe"]
    # Zustand statt Zaehler: getrennt, kein Fortschrittsgeplapper.
    assert fenster.st_zustand.text() == "Getrennt"
    assert fenster.st_modus.text() == "binär"
    assert not fenster.act_holen.isEnabled() and not fenster.act_trennen.isEnabled()
    assert fenster.act_verbinden.isEnabled()
    assert fenster.windowTitle() == "K1520 Kermit"


def test_symbolleiste_kommt_aus_einer_namensliste_und_uebergeht_unbekanntes(fenster):
    assert "verbinden" in fenster._leisten_inhalt
    fenster._leiste_fuellen(["verbinden", "gibt_es_nicht", "", "trennen"])
    assert fenster._leisten_inhalt == ["verbinden", None, "trennen"]
    # Zweimal umbauen: QToolBar.clear() haette die Huelle des toggleViewAction() freigegeben.
    fenster._leiste_fuellen(["holen", "schreiben"])
    fenster._leiste_fuellen(["holen", "schreiben"])
    assert fenster.act_leiste_zeigen.text() == "&Symbolleiste"
    assert [a.text() for a in fenster.leiste.actions()] == [fenster.act_holen.text(), fenster.act_schreiben.text()]


def test_no_shortcut_takes_a_plain_key(fenster):
    """Jedes Kuerzel traegt Strg+Umschalt — ausser F11 (wie bei Emulator und DiskTool)."""
    fremd = []
    for a in fenster.findChildren(QAction):
        k = a.shortcut().toString()
        if k and k != "F11" and not k.startswith("Ctrl+Shift+"):
            fremd.append(f"{k} ({a.text()})")
    assert fremd == []


_TASTEN = {"Strg": "Ctrl", "Umschalt": "Shift", "Entf": "Del", "Eingabe": "Return", "→": "Right", "←": "Left"}


def _als_kuerzel(deutsch):
    from PySide6.QtGui import QKeySequence
    return QKeySequence("+".join(_TASTEN.get(t, t) for t in deutsch.split("+"))).toString()


def _handbuch_kuerzel():
    from app.ui_help import lade_handbuch
    from app.kermittool.ui.main_window import HANDBUCH
    tabelle = lade_handbuch(HANDBUCH).split("## Tastenkürzel", 1)[1].split("\n## ", 1)[0]
    out = {}
    for zeile in tabelle.splitlines():
        if not zeile.startswith("|") or "---" in zeile:
            continue
        sp = [s.strip() for s in zeile.strip("|").split("|")]
        if len(sp) == 2 and sp[0] != "Kürzel":
            out[sp[0]] = sp[1]
    return out


def test_every_shortcut_in_the_manual_really_exists(fenster):
    vorhanden = {a.shortcut().toString() for a in fenster.findChildren(QAction) if not a.shortcut().isEmpty()}
    fehlend = [k for k in _handbuch_kuerzel() if _als_kuerzel(k) not in vorhanden]
    assert fehlend == [], f"im Handbuch versprochen, aber nicht verdrahtet: {fehlend}"


def test_every_shortcut_of_the_window_is_in_the_manual(fenster):
    im_buch = {_als_kuerzel(k) for k in _handbuch_kuerzel()}
    fehlend = [f"{a.shortcut().toString()} ({a.text()})" for a in fenster.findChildren(QAction)
               if not a.shortcut().isEmpty() and a.shortcut().toString() not in im_buch]
    assert fehlend == [], f"verdrahtet, aber nicht im Handbuch: {fehlend}"


def test_handbuch_liegt_im_app_baum_und_das_fenster_springt_zu_jedem_abschnitt(fenster):
    from app.ui_help import abschnitte, lade_handbuch
    from app.kermittool.ui.main_window import HANDBUCH
    assert HANDBUCH.is_file() and "app" in HANDBUCH.parts
    text = lade_handbuch(HANDBUCH)
    assert text.startswith("# K1520 Kermit") and "{{VERSION}}" not in text
    erwartet = abschnitte(text)
    for pflicht in ("Was ist Kermit", "So arbeitet K1520 Kermit", "Der übliche Weg", "Das Fenster",
                    "Fall 1 — die WEGA ist der Server", "Fall 2 — die WEGA ist der Client",
                    "Kermit Einrichtung unter WEGA", "Fehlersuche", "Tastenkürzel", "Einstellungen",
                    "Die Kommandozeile k1520kermit"):
        assert pflicht in erwartet
    h = fenster.open_help()
    assert [h.inhalt.item(i).text() for i in range(h.inhalt.count())] == erwartet
    for name in erwartet:
        assert h.springe_zu(name), name
    assert fenster.open_help() is h
    h.close()


# ─── Drag&Drop: Dateilisten aus Mime-Daten ───────────────────────────────────

def test_ablage_hilfen_filtern_dateien(tmp_path, lokal):
    from app.kermittool.ui import ablage
    (lokal / "a.bin").write_bytes(b"1")
    (lokal / "ordner").mkdir()
    pfade = [str(lokal / "a.bin"), str(lokal / "ordner"), str(lokal / "fehlt")]

    m = ablage.lokal_mime(pfade)
    assert ablage.lokale_dateien_aus(m) == [str(lokal / "a.bin")]
    assert not ablage.fern_namen_aus(m)

    von_aussen = QMimeData()
    von_aussen.setUrls([QUrl.fromLocalFile(p) for p in pfade] + [QUrl("http://example.org/x")])
    assert ablage.lokale_dateien_aus(von_aussen) == [str(lokal / "a.bin")]

    f = ablage.fern_mime(["x.c", "y.h"])
    assert ablage.fern_namen_aus(f) == ["x.c", "y.h"]
    assert ablage.lokale_dateien_aus(f) == []
    assert ablage.fern_namen_aus(QMimeData()) == []


# ─── Rundlauf GUI-Client <-> Bibliotheks-Server ──────────────────────────────

def test_rundlauf_verzeichnis_holen_schreiben_loeschen(qapp, fenster, gegenstelle, lokal, monkeypatch):
    frei = gegenstelle.freigabe
    (frei / "vorhanden.txt").write_bytes(b"vom Server\n")
    (frei / "unter").mkdir()
    (frei / "unter" / "tief.bin").write_bytes(b"\0\1\2\xff")
    client_verbinden(qapp, fenster, gegenstelle.port)

    # Verzeichnisanzeige links: .. , Ordner vor Dateien
    assert namen_links(fenster) == ["..", "unter/", "vorhanden.txt"]
    assert fenster.st_zustand.text().startswith("Verbunden mit")
    assert fenster.act_holen.isEnabled() is False          # nichts ausgewaehlt

    # Holen ueber die Aktion (links -> rechts)
    auswaehlen(fenster.fern_view.baum, "vorhanden.txt")
    qapp.processEvents()
    assert fenster.act_holen.isEnabled()
    fenster.act_holen.trigger()
    assert warte(qapp, lambda: (lokal / "vorhanden.txt").exists() and not fenster.arbeiter.beschaeftigt)
    assert (lokal / "vorhanden.txt").read_bytes() == b"vom Server\n"
    assert not fenster.fortschritt_dialog.isVisible()

    # Schreiben ueber die Aktion (rechts -> links)
    inhalt = os.urandom(5000) + b"\r\n\r\0"
    (lokal / "neu.bin").write_bytes(inhalt)
    fenster.lokal_view.aktualisieren()
    auswaehlen(fenster.lokal_view.baum, "neu.bin")
    qapp.processEvents()
    assert fenster.act_schreiben.isEnabled()
    fenster.act_schreiben.trigger()
    assert warte(qapp, lambda: "neu.bin" in namen_links(fenster))
    assert (frei / "neu.bin").read_bytes() == inhalt

    # Doppelklick: auf einen Ordner wechselt (G C), `..` kehrt zurueck
    fenster.fern_view.baum.itemActivated.emit(fenster.fern_view.baum.topLevelItem(1), 0)
    assert warte(qapp, lambda: namen_links(fenster) == ["..", "tief.bin"])
    assert "unter" in fenster.fern_view.pfad.text()
    fenster.act_hinauf.trigger()
    assert warte(qapp, lambda: "vorhanden.txt" in namen_links(fenster))
    assert fenster.fern_view.pfad.text() == "Server: (Startverzeichnis)"

    # Doppelklick auf eine Serverdatei holt sie
    (lokal / "vorhanden.txt").unlink()
    fenster.fern_view.baum.itemActivated.emit(_zeile(fenster.fern_view.baum, "vorhanden.txt"), 0)
    assert warte(qapp, lambda: (lokal / "vorhanden.txt").exists())
    ruhe(qapp, fenster)

    # Drag&Drop-Annahme: lokale Datei -> Server, Serverdatei -> lokaler Ordner
    (lokal / "gezogen.txt").write_bytes(b"gezogen")
    from app.kermittool.ui import ablage
    fenster._senden_pfade(ablage.lokale_dateien_aus(ablage.lokal_mime([str(lokal / "gezogen.txt")])))
    assert warte(qapp, lambda: "gezogen.txt" in namen_links(fenster))
    ruhe(qapp, fenster)
    (lokal / "vorhanden.txt").unlink()
    fenster._holen_namen(ablage.fern_namen_aus(ablage.fern_mime(["vorhanden.txt"])))
    assert warte(qapp, lambda: (lokal / "vorhanden.txt").exists())
    ruhe(qapp, fenster)

    # Loeschen: Rueckfrage verneint -> nichts; bejaht -> weg
    auswaehlen(fenster.fern_view.baum, "gezogen.txt")
    qapp.processEvents()
    monkeypatch.setattr(QMessageBox, "question", staticmethod(lambda *a, **k: QMessageBox.No))
    fenster.act_loeschen.trigger()
    qapp.processEvents()
    assert (frei / "gezogen.txt").exists()
    monkeypatch.setattr(QMessageBox, "question", staticmethod(lambda *a, **k: QMessageBox.Yes))
    fenster.act_loeschen.trigger()
    assert warte(qapp, lambda: "gezogen.txt" not in namen_links(fenster) and not (frei / "gezogen.txt").exists())

    # Gegenstelle beenden (G F) -> getrennt, Server-Faden endet
    ruhe(qapp, fenster)
    fenster.act_gegenstelle_beenden.trigger()
    assert warte(qapp, lambda: fenster.zustand_art == "getrennt")
    assert gegenstelle.fehler == []


def test_ohne_verbindung_wird_nichts_gesendet(qapp, fenster, lokal):
    (lokal / "x").write_bytes(b"x")
    fenster._senden_pfade([str(lokal / "x")])
    assert not fenster.arbeiter.beschaeftigt
    assert fenster.zustand_art == "getrennt"


def test_fehler_der_gegenstelle_landet_im_protokoll_und_im_streifen(qapp, fenster, gegenstelle, lokal):
    client_verbinden(qapp, fenster, gegenstelle.port)
    fenster.einstellungen_setzen({**fenster.einst, "max_namen": 5})
    # Die Namensgrenze gilt beim EMPFANG durch die Bibliothek: zu langer Name wird abgelehnt.
    (gegenstelle.freigabe / "sehr_langer_name.txt").write_bytes(b"x")
    fenster.fern_view.baum.clearSelection()
    fenster._holen_namen(["sehr_langer_name.txt"])
    assert warte(qapp, lambda: fenster.info_bar.text().startswith("Holen fehlgeschlagen"))
    assert fenster.info_bar.stufe == "fehler"
    assert "Holen fehlgeschlagen" in fenster.log_dock.text.toPlainText()
    assert not (lokal / "sehr_langer_name.txt").exists()


def test_abbruch_einer_uebertragung_laesst_die_verbindung_benutzbar(qapp, fenster, gegenstelle, lokal):
    client_verbinden(qapp, fenster, gegenstelle.port)
    fenster.einstellungen_setzen({**fenster.einst, "paketlaenge": 40})      # viele kleine Pakete: dauert
    (lokal / "gross.bin").write_bytes(os.urandom(8_000_000))
    fenster.lokal_view.aktualisieren()
    auswaehlen(fenster.lokal_view.baum, "gross.bin")
    qapp.processEvents()
    fenster.act_schreiben.trigger()
    assert warte(qapp, lambda: fenster.fortschritt_dialog.isVisible() and fenster._uebertragung == "gross.bin"), \
        (fenster.info_bar.text(), fenster.log_dock.text.toPlainText()[-600:])
    assert fenster.act_abbrechen.isEnabled()
    fenster.fortschritt_dialog.knopf.click()               # = Abbrechen im Dialog
    assert warte(qapp, lambda: not fenster.arbeiter.beschaeftigt and not fenster.fortschritt_dialog.isVisible())
    assert fenster.info_bar.text().startswith("Abgebrochen")
    assert not (gegenstelle.freigabe / "gross.bin").exists() or \
        (gegenstelle.freigabe / "gross.bin").stat().st_size < 8_000_000
    # geordneter Abbruch: dieselbe Verbindung geht weiter
    assert fenster.zustand_art == "verbunden"
    fenster.act_aktualisieren.trigger()
    assert warte(qapp, lambda: not fenster.arbeiter.beschaeftigt)
    fenster.act_trennen.trigger()
    assert warte(qapp, lambda: fenster.zustand_art == "getrennt")


def test_trennen_beendet_auch_ein_wartendes_lauschen(qapp, fenster):
    fenster.client_verbinden({"leitung": "telnet", "aufbau": "lauschen", "host": "", "port": 0})
    assert warte(qapp, lambda: fenster.zustand_art == "lauscht")
    assert re.search(r"Lauscht auf Port \d+", fenster.st_zustand.text())
    fenster.act_trennen.trigger()
    assert warte(qapp, lambda: fenster.zustand_art == "getrennt" and not fenster.arbeiter.beschaeftigt)


def test_verbinden_ohne_gegenstelle_meldet_den_fehler(qapp, fenster):
    with Kermit.telnet_server(0) as s:
        frei = s.listen_port                                # danach geschlossen: nichts lauscht
    fenster.client_verbinden({"leitung": "telnet", "aufbau": "verbinden", "host": "127.0.0.1", "port": frei})
    assert warte(qapp, lambda: fenster.info_bar.text().startswith("Verbindung fehlgeschlagen"))
    assert fenster.zustand_art == "getrennt"
    assert fenster.act_verbinden.isEnabled()


# ─── Server-Modus ────────────────────────────────────────────────────────────

def _lausch_port(qapp, fenster):
    assert warte(qapp, lambda: fenster.zustand_art == "lauscht")
    m = re.search(r"Port (\d+)", fenster._zustand_text)
    assert m, fenster._zustand_text
    return int(m.group(1))


def _server_vorbereiten(fenster, ordner):
    fenster.server_verb = {**fenster.server_verb, "leitung": "telnet", "aufbau": "lauschen", "port": 0}
    fenster.freigabe = str(ordner)


def test_server_start_ohne_freigabe_wird_verweigert(qapp, fenster):
    assert fenster.freigabe == ""
    assert fenster.server_starten() is False
    assert not fenster._server_laeuft and fenster.zustand_art == "getrennt"
    assert fenster.info_bar.stufe == "warnung" and "Ordner" in fenster.info_bar.text()
    fenster.act_server_start.trigger()                      # auch ueber die Aktion
    assert not fenster._server_laeuft
    fenster.freigabe = "/gibt/es/nicht"
    assert fenster.server_starten() is False


def test_server_gibt_den_ordner_frei_und_zeigt_die_uebertragungen(qapp, fenster, tmp_path):
    frei = tmp_path / "frei"
    frei.mkdir()
    (frei / "aus_dem_server.txt").write_bytes(b"hallo")
    _server_vorbereiten(fenster, frei)
    assert fenster.act_server_start.isEnabled()
    assert fenster.server_starten() is True
    assert fenster._server_laeuft and fenster.links.currentIndex() == 1
    assert fenster.act_server_stopp.isEnabled() and not fenster.act_server_start.isEnabled()
    assert warte(qapp, lambda: fenster.st_zustand.text().startswith("Server: Lauscht auf Port"))
    port = _lausch_port(qapp, fenster)

    daten = os.urandom(20_000)
    ziel = tmp_path / "ziel"
    ziel.mkdir()
    (tmp_path / "quelle.bin").write_bytes(daten)
    with Kermit.telnet_client("127.0.0.1", port) as c:
        assert warte(qapp, lambda: fenster.zustand_art == "verbunden")
        c.send(str(tmp_path / "quelle.bin"), "quelle.bin")
        assert (frei / "quelle.bin").read_bytes() == daten
        assert c.get("aus_dem_server.txt", str(ziel)) == ["aus_dem_server.txt"]
        c.remote_finish()
    assert warte(qapp, lambda: not fenster._server_laeuft)
    zeilen = fenster.server_view.zeilen()
    assert any(z[1] == "Datei" and z[2] == "quelle.bin" and z[4] == "fertig" for z in zeilen), zeilen
    assert any(z[1] == "Ereignis" for z in zeilen)
    assert fenster.zustand_art == "getrennt"


def test_server_nur_lesen_weist_schreiben_ab(qapp, fenster, tmp_path):
    frei = tmp_path / "frei"
    frei.mkdir()
    _server_vorbereiten(fenster, frei)
    fenster.act_nur_lesen.setChecked(True)
    assert fenster.server_starten()
    port = _lausch_port(qapp, fenster)
    (tmp_path / "x.bin").write_bytes(b"abc")
    with Kermit.telnet_client("127.0.0.1", port) as c:
        with pytest.raises(KermitError):
            c.send(str(tmp_path / "x.bin"), "x.bin")
        c.remote_finish()
    assert warte(qapp, lambda: not fenster._server_laeuft)
    assert not (frei / "x.bin").exists()


def test_server_lauscht_nach_der_trennung_wieder_wenn_gewuenscht(qapp, fenster, tmp_path):
    frei = tmp_path / "frei"
    frei.mkdir()
    _server_vorbereiten(fenster, frei)
    fenster.act_wieder_lauschen.setChecked(True)
    zustaende = []
    from PySide6.QtCore import Qt as _Qt
    fenster.arbeiter.zustand.connect(lambda a, t: zustaende.append(a), _Qt.QueuedConnection)
    assert fenster.server_starten()
    port1 = _lausch_port(qapp, fenster)
    with Kermit.telnet_client("127.0.0.1", port1):
        assert warte(qapp, lambda: fenster.zustand_art == "verbunden")
    # Verbindung ohne Finish beendet -> der Server lauscht erneut und bleibt „laufend"
    assert warte(qapp, lambda: zustaende.count("lauscht") >= 2), zustaende
    assert fenster._server_laeuft
    port2 = _lausch_port(qapp, fenster)
    (tmp_path / "z.bin").write_bytes(b"zweite Verbindung")
    with Kermit.telnet_client("127.0.0.1", port2) as c:
        c.send(str(tmp_path / "z.bin"), "z.bin")
    assert warte(qapp, lambda: (frei / "z.bin").exists())
    assert warte(qapp, lambda: zustaende.count("lauscht") >= 3)       # und noch einmal
    fenster.act_wieder_lauschen.setChecked(False)
    fenster.act_server_stopp.trigger()
    assert warte(qapp, lambda: not fenster._server_laeuft and fenster.zustand_art == "getrennt")


def test_server_stopp_beendet_den_lauscher(qapp, fenster, tmp_path):
    _server_vorbereiten(fenster, tmp_path)
    assert fenster.server_starten()
    _lausch_port(qapp, fenster)
    fenster.act_server_stopp.trigger()
    assert warte(qapp, lambda: not fenster._server_laeuft and fenster.zustand_art == "getrennt")
    assert fenster.act_server_start.isEnabled()


# ─── Einstellungen und Konfiguration ─────────────────────────────────────────

def test_einstellungsdialog_rundlauf_und_wega_werte(qapp, fenster):
    from app.kermittool.ui.einstellungen_dialog import EinstellungenDialog
    e = {**konfig.EINSTELLUNGEN_VORGABE, "modus": "automatik", "pruefsumme": 2, "paketlaenge": 200,
         "paket_frist_s": 30, "max_namen": 14, "namen_senden": "konvertiert", "tempo": 10.0}
    d = EinstellungenDialog(fenster, e)
    assert d.werte() == konfig.einstellungen_aus(e) and d.werte()["tempo"] == 10.0
    d.setzen({**konfig.EINSTELLUNGEN_VORGABE, "paketlaenge": 1000, "max_namen": 0})
    d.max_namen.setValue(0)
    w = d.werte()
    assert w["paketlaenge"] == 1000 and w["max_namen"] == 0
    fenster.einstellungen_setzen(e)
    assert fenster.st_modus.text() == "Automatik"
    assert konfig.text_modus("automatik", "a.C") and konfig.text_modus("automatik", "x.MAC")
    assert not konfig.text_modus("automatik", "a.out") and not konfig.text_modus("binaer", "a.txt")
    assert konfig.text_modus("text", "a.out")


def test_verbindungsdialog_voreinstellung_wega(qapp, fenster):
    from app.kermittool.ui.verbindung_dialog import VerbindungDialog
    d = VerbindungDialog(fenster, "client", konfig.VERBINDUNG_VORGABE, [])
    namen = [d.voreinst.itemText(i) for i in range(d.voreinst.count())]
    assert "WEGA 3.1 (Benutzer kermit)" in namen
    d.voreinst.setCurrentIndex(namen.index("WEGA 3.1 (Benutzer kermit)"))
    d._voreinstellung(d.voreinst.currentIndex())
    v = d.verbindung()
    assert v["login"] and v["login_name"] == "kermit" and d.kennwort() == "kermit"
    assert v["kennwort"] == "" and not d.speichern()               # Laborkennwort nur Sitzung: kein Haken, nicht in v
    assert d.einstellungsaenderung["max_namen"] == 14
    d.host.setText("")
    d._annehmen()
    assert "Host" in d.meldung.text() and d.result() == 0


def _kennwort_orte(knoten, pfad=()):
    """Alle Pfade einer geladenen YAML-Struktur, an deren Ende ein Schluessel ``kennwort*`` steht
    (Listenindizes als ``*``)."""
    orte = set()
    if isinstance(knoten, dict):
        for k, v in knoten.items():
            if str(k).lower().startswith("kennwort"):
                orte.add(pfad + (k,))
            orte |= _kennwort_orte(v, pfad + (k,))
    elif isinstance(knoten, list):
        for v in knoten:
            orte |= _kennwort_orte(v, pfad + ("*",))
    return orte


def test_konfiguration_schreibt_kein_kennwort_ohne_haken_und_liest_alles_zurueck(qapp, tmp_path, lokal):
    from app.kermittool.ui.main_window import MainWindow
    pfad = tmp_path / "kermit.yaml"
    f = MainWindow(konfig_pfad=str(pfad), ordner=str(lokal))
    f.show()
    qapp.processEvents()
    f.einstellungen_setzen({**f.einst, "modus": "text", "paketlaenge": 100, "max_namen": 14})
    # ohne Haken: das Verbindungsdict traegt kein Kennwort, die Sitzung schon
    f.verbindung = {**f.verbindung, "host": "wega.example", "port": 4711, "login": True, "login_name": "kermit"}
    f._kennwort = "geheim-xyz-123"
    f.zuletzt = konfig.zuletzt_merken([], f.verbindung)
    f.freigabe = str(lokal)
    f.act_nur_lesen.setChecked(True)
    f.act_wieder_lauschen.setChecked(True)
    assert f.konfig_sichern()
    f.close()

    text = pfad.read_text(encoding="utf-8")
    assert "geheim-xyz-123" not in text                              # Gegentest: kein Haken = nicht in der Datei
    import yaml
    roh = yaml.safe_load(text)
    assert _kennwort_orte(roh) == {("server", "login", "kennwort")}  # nur der Anmeldezwang des eigenen Servers
    assert roh["server"]["login"] == {"pflicht": False, "name": "kermit", "kennwort": "kermit"}
    assert "geschrieben_von" in text and "version: 1" in text

    g = MainWindow(konfig_pfad=str(pfad))
    try:
        assert g.einst["modus"] == "text" and g.einst["paketlaenge"] == 100 and g.einst["max_namen"] == 14
        assert g.verbindung["host"] == "wega.example" and g.verbindung["port"] == 4711
        assert g.verbindung["login_name"] == "kermit" and g.verbindung["kennwort"] == "" and g._kennwort == ""
        assert g.freigabe == str(lokal) and g.act_nur_lesen.isChecked() and g.act_wieder_lauschen.isChecked()
        assert g.lokal_view.ordner == lokal
        assert len(g.zuletzt) == 1 and not g.zuletzt[0]["kennwort"]
        assert g.st_modus.text() == "Text"
    finally:
        g.close()


def test_eine_neuere_konfiguration_wird_nicht_ueberschrieben(qapp, tmp_path, lokal):
    from app.kermittool.ui.main_window import MainWindow
    pfad = tmp_path / "kermit.yaml"
    pfad.write_text("version: 99\nzukunft: true\n", encoding="utf-8")
    f = MainWindow(konfig_pfad=str(pfad), ordner=str(lokal))
    assert f.konfig_sichern() is False
    f.close()
    assert pfad.read_text(encoding="utf-8") == "version: 99\nzukunft: true\n"


def test_defekte_konfiguration_faellt_auf_vorgaben_zurueck(qapp, tmp_path, lokal):
    from app.kermittool.ui.main_window import MainWindow
    pfad = tmp_path / "kermit.yaml"
    pfad.write_text("uebertragung: {pruefsumme: 77, modus: quatsch, paketlaenge: abc}\nclient: 5\n"
                    "zuletzt: [1, {host: x}]\n", encoding="utf-8")
    f = MainWindow(konfig_pfad=str(pfad), ordner=str(lokal))
    try:
        assert f.einst["pruefsumme"] == 3 and f.einst["modus"] == "binaer" and f.einst["paketlaenge"] == 94
        assert f.verbindung["port"] == 5000
    finally:
        f.close()


def test_maximiert_wird_aus_dem_eigenen_feld_nachgeholt(qapp, tmp_path, lokal):
    """Qts restoreGeometry traegt „maximiert" nicht zurueck — das eigene Feld entscheidet."""
    from app.kermittool.ui.main_window import MainWindow
    f = MainWindow(konfig_pfad=str(tmp_path / "k.yaml"), ordner=str(lokal))
    try:
        f._anwenden_fenster({"width": 700, "height": 400, "maximized": True})
        assert f._maximiert_nachholen is True
        assert (f.width(), f.height()) == (700, 400)
        assert f._fenster_daten()["maximized"] is False        # noch nicht angezeigt
    finally:
        f.close()


def test_kennwoerter_fallen_aus_jeder_ebene_ausser_an_den_erlaubten_orten(tmp_path):
    d = {"a": {"kennwort": "x", "b": [{"Kennwort_alt": "y", "ok": 1}]}, "ok": 2}
    assert konfig.ohne_kennwort(d) == {"a": {"b": [{"ok": 1}]}, "ok": 2}
    # schreiben(): erlaubt sind exakt konfig.KENNWORT_ORTE; das Kennwort der Server-VERBINDUNG und alles andere faellt
    daten = {"client": {"host": "h", "kennwort": "c-pw"},
             "zuletzt": [{"betriebsart": "client", "host": "h", "kennwort": "z-pw"},
                         {"betriebsart": "server", "host": "s", "kennwort": "s-pw"}],
             "server": {"kennwort": "sv-pw", "login": {"pflicht": True, "name": "n", "kennwort": "l-pw"}},
             "uebertragung": {"kennwort": "u-pw"}, "window": {"kennwort_x": "w-pw"}}
    datei = tmp_path / "k.yaml"
    assert konfig.schreiben(daten, str(datei))
    import yaml
    roh = yaml.safe_load(datei.read_text(encoding="utf-8"))
    assert _kennwort_orte(roh) == {("client", "kennwort"), ("zuletzt", "*", "kennwort"),
                                   ("server", "login", "kennwort")}
    assert set(konfig.KENNWORT_ORTE) == {("client", "kennwort"), ("zuletzt", "*", "kennwort"),
                                         ("server", "login", "kennwort")}
    text = datei.read_text(encoding="utf-8")
    assert "c-pw" in text and "z-pw" in text and "l-pw" in text           # Klartext (Entscheidung des Anwenders)
    for fremd in ("s-pw", "sv-pw", "u-pw", "w-pw"):
        assert fremd not in text


def test_hilfe_menue_laedt_das_handbuch_mit_dem_wega_abschnitt(fenster):
    """Hilfe > Handbuch: das Fenster laedt `help/handbuch.md` samt dem Abschnitt zur WEGA-Einrichtung."""
    from app.ui_help import abschnitte
    fenster.act_hilfe.trigger()
    h = fenster._hilfe
    assert h is not None and h.isVisible()
    assert "Kermit Einrichtung unter WEGA" in abschnitte(h.text)
    assert h.springe_zu("Kermit Einrichtung unter WEGA")
    assert "kermit:" in h.text and ".kermrc" in h.text and "/usr/bin/kermit" in h.text
    assert "Kermit Einrichtung unter WEGA" in [h.inhalt.item(i).text() for i in range(h.inhalt.count())]
    h.suchfeld.setText(".kermrc")
    assert h.weitersuchen()
    h.close()


# ─── Server mit Anmeldezwang (K12) ───────────────────────────────────────────

def test_server_dialog_hat_anmeldung_erzwingen_mit_vorgabe_und_klartexthinweis(qapp, fenster):
    from app.kermittool.ui.verbindung_dialog import KLARTEXT_HINWEIS, VerbindungDialog
    d = VerbindungDialog(fenster, "server", fenster.server_verb, [], server_login=fenster.server_login)
    assert not d.srv_pflicht.isChecked()
    assert d.srv_name.text() == "kermit" and d.srv_kennwort.text() == "kermit"      # Vorgabe
    assert not d.srv_name.isEnabled() and not d.srv_kennwort.isEnabled()            # nur mit Haken aktiv
    d.srv_pflicht.setChecked(True)
    assert d.srv_name.isEnabled() and d.srv_kennwort.isEnabled()
    assert "Klartext" in d.srv_hinweis.text() and d.srv_hinweis.text() == KLARTEXT_HINWEIS
    assert "Klartext" in d.srv_kennwort.toolTip()
    d.srv_name.setText("olli")
    d.srv_kennwort.setText("geheim")
    assert d.server_login() == {"pflicht": True, "name": "olli", "kennwort": "geheim"}
    d.srv_kennwort.setText("")
    d._annehmen()
    assert d.result() == 0 and "Kennwort" in d.meldung.text()                        # leer wird nicht angenommen
    d.srv_kennwort.setText("geheim")
    d._annehmen()
    assert d.result() == VerbindungDialog.Accepted
    # Der Client-Dialog kennt die Felder nicht
    c = VerbindungDialog(fenster, "client", konfig.VERBINDUNG_VORGABE, [])
    assert c.srv_pflicht.parent() is None and not c.srv_pflicht.isVisibleTo(c)     # nicht im Client-Dialog


def test_server_anmeldezwang_wird_im_klartext_gespeichert_und_gelesen(qapp, tmp_path, lokal):
    import yaml
    from app.kermittool.ui.main_window import MainWindow
    pfad = tmp_path / "kermit.yaml"
    f = MainWindow(konfig_pfad=str(pfad), ordner=str(lokal))
    f.server_login = {"pflicht": True, "name": "olli", "kennwort": "geheim-xyz-123"}
    f.verbindung = {**f.verbindung, "login": True, "login_name": "kermit"}
    f._kennwort = "client-geheim-456"                                  # Sitzung, ohne Haken
    assert f.konfig_sichern()
    f.close()
    text = pfad.read_text(encoding="utf-8")
    assert "geheim-xyz-123" in text                                   # Klartext, Entscheidung des Anwenders
    assert "client-geheim-456" not in text                            # ohne Haken kein Client-Kennwort
    assert yaml.safe_load(text)["server"]["login"] == {"pflicht": True, "name": "olli", "kennwort": "geheim-xyz-123"}
    g = MainWindow(konfig_pfad=str(pfad), ordner=str(lokal))
    try:
        assert g.server_login == {"pflicht": True, "name": "olli", "kennwort": "geheim-xyz-123"}
    finally:
        g.close()


def test_server_mit_anmeldezwang_zeigt_es_in_der_statuszeile_und_verlangt_die_anmeldung(qapp, fenster, tmp_path):
    frei = tmp_path / "frei"
    frei.mkdir()
    (frei / "a.txt").write_bytes(b"hallo")
    _server_vorbereiten(fenster, frei)
    fenster.server_login = {"pflicht": True, "name": "olli", "kennwort": "geheim"}
    assert fenster.server_starten() is True
    assert warte(qapp, lambda: fenster.st_zustand.text().startswith("Server: Lauscht auf Port"))
    assert fenster.st_zustand.text().endswith("(mit Anmeldung)")
    assert "mit Anmeldung" in fenster.server_kopf.text()
    port = _lausch_port(qapp, fenster)
    ziel = tmp_path / "ziel"
    ziel.mkdir()
    with Kermit.telnet_client("127.0.0.1", port) as c:
        assert warte(qapp, lambda: fenster.zustand_art == "verbunden")
        assert "(mit Anmeldung)" in fenster.st_zustand.text()
        with pytest.raises(KermitError):
            c.login("olli", "falsch", 3000)
        c.login("olli", "geheim", 30000)
        assert c.get("a.txt", str(ziel)) == ["a.txt"]
        c.remote_finish()                               # Server bleibt (Anmeldung folgt wieder)
        assert fenster._server_laeuft
    assert warte(qapp, lambda: any("Angemeldet: olli" in z[2] for z in fenster.server_view.zeilen()))
    fenster._server_stopp()
    assert warte(qapp, lambda: not fenster._server_laeuft)


# ─── Client-Anmeldung auf Abruf (K11) ────────────────────────────────────────

class _Fragen:
    """Ersatz fuer das Popup: zeichnet die Fragen auf und antwortet der Reihe nach."""

    def __init__(self, *antworten):
        self.antworten = list(antworten)
        self.fragen = []          # (versuch, gelesen, name, kennwort, speichern)

    def __call__(self, a):
        self.fragen.append((a.versuch, a.gelesen, a.name, a.kennwort, a.speichern))
        return self.antworten.pop(0) if self.antworten else None


@pytest.fixture
def login_gegenstelle(tmp_path):
    frei = tmp_path / "freigabe_l"
    frei.mkdir()
    (frei / "a.txt").write_bytes(b"hallo")
    g = Gegenstelle(frei, login=("olli", "geheim"))
    yield g
    g.ende()


def _login_verbinden(qapp, fenster, port, **extra):
    fenster.client_verbinden({"leitung": "telnet", "aufbau": "verbinden", "host": "127.0.0.1", "port": port,
                              "login": True, **extra})


def _verbunden(qapp, fenster):
    assert warte(qapp, lambda: fenster.zustand_art == "verbunden"
                 and fenster.fern_view.baum.topLevelItemCount() > 0), fenster._zustand_text


def _yaml(fenster):
    import yaml
    return yaml.safe_load(Path(fenster._konfig_pfad).read_text(encoding="utf-8"))


def test_anmeldedialog_hat_felder_hinweise_und_knoepfe(qapp, fenster):
    from app.kermittool.ui.anmelde_dialog import AnmeldeDialog
    from PySide6.QtWidgets import QDialogButtonBox, QLineEdit
    d = AnmeldeDialog(fenster, 0, "login: ")
    assert d.windowTitle() == "Anmeldung"
    assert d.name.text() == "" and d.kennwort.text() == ""                  # Vorbelegung leer
    assert d.kennwort.echoMode() == QLineEdit.Password                      # Eingabe verdeckt
    assert d.vorschlag.text() == "Möglicherweise ist es einfach kermit / kermit."
    assert d.speichern.text() == "Zugangsdaten für diese Verbindung speichern" and not d.speichern.isChecked()
    assert "Klartext" in d.klartext.text() and "Klartext" in d.speichern.toolTip()
    assert d.knoepfe.button(QDialogButtonBox.Ok).text() == "Anmelden"
    assert d.knoepfe.button(QDialogButtonBox.Cancel).text() == "Abbrechen"
    assert d.abgelehnt.isHidden()
    d.name.setText("  olli ")
    d.kennwort.setText("geheim")
    d.speichern.setChecked(True)
    assert d.ergebnis() == ("olli", "geheim", True)
    # nach einer Ablehnung: Hinweis sichtbar, Eingabe vorbelegt; leerer Name wird nicht angenommen
    r = AnmeldeDialog(fenster, 1, "Login incorrect\r\nlogin: ", "olli", "falsch", True)
    r.show()
    assert not r.abgelehnt.isHidden() and "abgelehnt" in r.abgelehnt.text()
    assert r.name.text() == "olli" and r.kennwort.text() == "falsch" and r.speichern.isChecked()
    r.name.setText("")
    r._annehmen()
    assert r.result() == 0 and "Benutzername" in r.meldung.text()
    r.close()


def test_verbindungsdialog_hat_den_haken_mit_klartexthinweis_und_speichert_das_kennwort_nur_mit_haken(qapp, fenster):
    from app.kermittool.ui.verbindung_dialog import VerbindungDialog
    d = VerbindungDialog(fenster, "client", konfig.VERBINDUNG_VORGABE, [])
    assert d.login_speichern.text() == "Zugangsdaten für diese Verbindung speichern"
    assert "Klartext" in d.login_hinweis.text() and "Klartext" in d.login_speichern.toolTip()
    assert "nur gespeichert, wenn" in d.login_kennwort.toolTip() and "nie gespeichert" not in d.login_kennwort.toolTip()
    d.login.setChecked(True)
    d.login_name.setText("olli")
    d.login_kennwort.setText("geheim")
    assert d.verbindung()["kennwort"] == "" and not d.speichern() and d.kennwort() == "geheim"
    d.login_speichern.setChecked(True)
    assert d.verbindung()["kennwort"] == "geheim" and d.speichern()
    # leerer Name ist erlaubt (dann fragt das Popup) — ausser zum Speichern
    d.login_name.setText("")
    d._annehmen()
    assert d.result() == 0 and "Benutzername" in d.meldung.text()
    d.login_speichern.setChecked(False)
    d._annehmen()
    assert d.result() == 1
    # ein gespeichertes Kennwort stellt den Haken wieder her
    e = VerbindungDialog(fenster, "client", {**konfig.VERBINDUNG_VORGABE, "login": True, "login_name": "x",
                                             "kennwort": "k"}, [])
    assert e.login_speichern.isChecked() and e.login_kennwort.text() == "k"


def test_ohne_gespeicherte_daten_fragt_das_popup_und_ohne_haken_steht_nichts_in_der_datei(
        qapp, fenster, login_gegenstelle):
    fenster._anmeldefrage = fragen = _Fragen(("olli", "geheim", False))
    _login_verbinden(qapp, fenster, login_gegenstelle.port)
    _verbunden(qapp, fenster)
    assert [f[0] for f in fragen.fragen] == [0] and fragen.fragen[0][2:] == ("", "", False)
    assert fenster.konfig_sichern()
    text = Path(fenster._konfig_pfad).read_text(encoding="utf-8")
    assert "geheim" not in text                                         # kein Haken = nicht in der Datei
    assert _kennwort_orte(_yaml(fenster)) == {("server", "login", "kennwort")}
    assert fenster.verbindung["kennwort"] == "" and fenster.verbindung["login_name"] == ""


def test_falsche_eingabe_fragt_erneut_vorbelegt_und_mit_hinweis(qapp, fenster, login_gegenstelle):
    fenster._anmeldefrage = fragen = _Fragen(("olli", "falsch", False), ("olli", "geheim", False))
    _login_verbinden(qapp, fenster, login_gegenstelle.port)
    _verbunden(qapp, fenster)
    assert [f[0] for f in fragen.fragen] == [0, 1]
    assert "Login incorrect" in fragen.fragen[1][1]
    assert fragen.fragen[1][2:] == ("olli", "falsch", False)           # mit den eben eingegebenen Werten
    assert not fenster.info_bar.text().startswith("Verbindung fehlgeschlagen")


def test_abbrechen_im_popup_beendet_still_und_schliesst_die_verbindung(qapp, fenster, login_gegenstelle):
    fenster._anmeldefrage = fragen = _Fragen(None)
    _login_verbinden(qapp, fenster, login_gegenstelle.port)
    assert warte(qapp, lambda: len(fragen.fragen) == 1)
    assert warte(qapp, lambda: not fenster.arbeiter.beschaeftigt)
    assert fenster.zustand_art == "getrennt"
    assert "fehlgeschlagen" not in fenster.info_bar.text() and "abgebrochen" in fenster.info_bar.text()
    assert fenster.act_verbinden.isEnabled()


def test_abbrechen_von_aussen_beendet_das_warten_auf_das_popup(qapp, fenster, login_gegenstelle):
    """Die Frage haelt den Arbeitsfaden OHNE Sperre fest; ``abbrechen()`` weckt ihn."""
    from app.kermittool.arbeiter import AnmeldeAnfrage
    haengende = []
    fenster._anmeldefrage = lambda a: haengende.append(a) or None
    # Ersatz, der NICHT antwortet: das Fenster-Slot antwortet sofort None; also direkt den Arbeiter pruefen
    a = AnmeldeAnfrage(0, "login: ", "", "", False)
    erg = []
    t = threading.Thread(target=lambda: erg.append(fenster.arbeiter._frage_gui(a)))
    fenster.arbeiter.anmeldung_verlangt.disconnect()                    # keiner beantwortet
    t.start()
    assert warte(qapp, lambda: fenster.arbeiter._offene)
    fenster.arbeiter.abbrechen()
    t.join(10)
    assert not t.is_alive() and erg == [None]
    assert not fenster.arbeiter._offene


def test_gespeicherte_daten_brauchen_kein_popup_und_falsche_loesen_es_aus(qapp, fenster, login_gegenstelle):
    fenster._anmeldefrage = fragen = _Fragen()
    _login_verbinden(qapp, fenster, login_gegenstelle.port, login_name="olli", kennwort="geheim")
    _verbunden(qapp, fenster)
    assert fragen.fragen == []                                          # keine Frage

    # Gespeichertes Kennwort falsch: Popup (Wiederholungsregel), mit dem Gespeicherten vorbelegt und Haken gesetzt
    fenster._trennen()
    assert warte(qapp, lambda: fenster.zustand_art == "getrennt" and not fenster.arbeiter.beschaeftigt)
    g2 = Gegenstelle(login_gegenstelle.freigabe, login=("olli", "geheim"))
    try:
        fenster._anmeldefrage = fragen = _Fragen(("olli", "geheim", True))
        _login_verbinden(qapp, fenster, g2.port, login_name="olli", kennwort="alt-falsch")
        _verbunden(qapp, fenster)
        assert [f[0] for f in fragen.fragen] == [1]
        assert fragen.fragen[0][2:] == ("olli", "alt-falsch", True)
    finally:
        fenster._trennen()
        warte(qapp, lambda: not fenster.arbeiter.beschaeftigt)
        g2.ende()


def test_speichern_mit_haken_im_klartext_im_verbindungseintrag_und_ueberschreiben(qapp, fenster, login_gegenstelle):
    fenster._anmeldefrage = _Fragen(("olli", "geheim", True))
    _login_verbinden(qapp, fenster, login_gegenstelle.port)
    _verbunden(qapp, fenster)
    assert warte(qapp, lambda: Path(fenster._konfig_pfad).exists())         # gespeichert NACH gelungener Anmeldung
    roh = _yaml(fenster)
    assert roh["client"]["login_name"] == "olli" and roh["client"]["kennwort"] == "geheim"
    assert roh["zuletzt"][0]["kennwort"] == "geheim" and roh["zuletzt"][0]["login_name"] == "olli"
    assert _kennwort_orte(roh) == {("client", "kennwort"), ("zuletzt", "*", "kennwort"),
                                   ("server", "login", "kennwort")}
    # Roundtrip
    from app.kermittool.ui.main_window import MainWindow
    h = MainWindow(konfig_pfad=fenster._konfig_pfad)
    try:
        assert h.verbindung["kennwort"] == "geheim" and h.verbindung["login_name"] == "olli"
        assert h.zuletzt[0]["kennwort"] == "geheim"
    finally:
        h.close()

    # Zweites Speichern ueberschreibt: neuer Server mit anderen Daten, gespeicherte sind nun falsch
    fenster._trennen()
    assert warte(qapp, lambda: fenster.zustand_art == "getrennt" and not fenster.arbeiter.beschaeftigt)
    g2 = Gegenstelle(login_gegenstelle.freigabe, login=("neu", "pw2"))
    try:
        fenster._anmeldefrage = fragen = _Fragen(("neu", "pw2", True))
        fenster.client_verbinden({**fenster.verbindung, "port": g2.port})
        _verbunden(qapp, fenster)
        assert fragen.fragen[0][0] == 1                                     # gespeichertes (alt) wurde abgelehnt
        roh = _yaml(fenster)
        assert roh["client"]["login_name"] == "neu" and roh["client"]["kennwort"] == "pw2"
        assert roh["zuletzt"][0]["kennwort"] == "pw2" and roh["zuletzt"][0]["port"] == g2.port
    finally:
        fenster._trennen()
        warte(qapp, lambda: not fenster.arbeiter.beschaeftigt)
        g2.ende()


def test_nach_ablehnung_wird_nichts_gespeichert_bis_es_geklappt_hat(qapp, fenster, login_gegenstelle):
    fenster.verbindung = {**fenster.verbindung, "login_name": "gut", "kennwort": "gut-pw"}
    fenster._anmeldefrage = fragen = _Fragen(("olli", "falsch", True), None)       # Haken, falsch, dann Abbruch
    _login_verbinden(qapp, fenster, login_gegenstelle.port)
    assert warte(qapp, lambda: len(fragen.fragen) == 2)
    assert warte(qapp, lambda: not fenster.arbeiter.beschaeftigt)
    assert fenster.konfig_sichern()
    assert "falsch" not in Path(fenster._konfig_pfad).read_text(encoding="utf-8")
    assert fenster.verbindung["kennwort"] != "falsch"


def test_zuletzt_traegt_das_kennwort_nur_mit_haken():
    ohne = konfig.zuletzt_merken([], {**konfig.VERBINDUNG_VORGABE, "login_name": "n"})
    assert "kennwort" not in ohne[0]
    mit = konfig.zuletzt_merken(ohne, {**konfig.VERBINDUNG_VORGABE, "login_name": "n", "kennwort": "pw"})
    assert len(mit) == 1 and mit[0]["kennwort"] == "pw"                     # ersetzt den alten Eintrag
    wieder_ohne = konfig.zuletzt_merken(mit, {**konfig.VERBINDUNG_VORGABE, "login_name": "n"})
    assert "kennwort" not in wieder_ohne[0]                                 # Haken weg = Kennwort weg
    srv = konfig.zuletzt_merken([], {**konfig.VERBINDUNG_VORGABE, "betriebsart": "server", "kennwort": "pw"})
    assert "kennwort" not in srv[0]


class _OhneKennwortAttrappe:
    """Spielt eine getty nach, die dem Benutzer sofort ein neues Kennwort abverlangt (WEGA: Benutzer ohne Kennwort)."""

    def __init__(self):
        import socket
        self.lauscher = socket.socket()
        self.lauscher.bind(("127.0.0.1", 0))
        self.lauscher.listen(1)
        self.port = self.lauscher.getsockname()[1]
        self.namen = 0
        self._t = threading.Thread(target=self._lauf, daemon=True)
        self._t.start()

    def _zeile(self, c):
        z = b""
        while True:
            b = c.recv(1)
            if not b:
                return None
            if b == b"\xff":
                c.recv(2)
                continue
            if b == b"\r":
                return z
            if b not in (b"\n", b"\0"):
                z += b

    def _lauf(self):
        c, _ = self.lauscher.accept()
        c.settimeout(20)
        try:
            self._zeile(c)
            c.sendall(b"\r\nlogin: ")
            if self._zeile(c) is None:
                return
            self.namen += 1
            c.sendall(b"\r\nPassword: ")
            if self._zeile(c) is None:
                return
            c.sendall(b"\r\nYou don't have a password.  Choose one.\r\nNew password: ")
            while c.recv(100):
                pass
        except OSError:
            pass
        finally:
            c.close()


def test_benutzer_ohne_kennwort_zeigt_den_fehler_statt_erneut_zu_fragen(qapp, fenster):
    a = _OhneKennwortAttrappe()
    fenster._anmeldefrage = fragen = _Fragen(("kermit", "", False), ("kermit", "x", False))
    try:
        _login_verbinden(qapp, fenster, a.port)
        assert warte(qapp, lambda: fenster.info_bar.text().startswith("Verbindung fehlgeschlagen"))
        assert "Anmeldung abgelehnt" in fenster.info_bar.text()
        assert len(fragen.fragen) == 1 and a.namen == 1                    # keine zweite Frage
        assert fenster.zustand_art == "getrennt"
    finally:
        a.lauscher.close()

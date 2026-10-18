"""Die Oberflaeche von K1520 Kermit (``app/kermittool/``) — headless ueber ``QT_QPA_PLATFORM=offscreen``.

Geprueft wird die Verdrahtung, nicht die Pixel: Fenster baut, jede Bedienung ist eine
``QAction``, Kuerzel nur mit Strg+Umschalt, die Kuerzeltabelle des Handbuchs stimmt in beiden
Richtungen, und ein Rundlauf **GUI-Client <-> Bibliotheks-Server** (Python-Faden, Loopback-Telnet,
Port 0) mit Verzeichnisanzeige, Holen, Schreiben, Loeschen, Abbruch.  Dazu der Server-Modus
(Freigabe Pflicht, nur lesen, wieder lauschen) und die Konfiguration ohne Kennwort.

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

    def __init__(self, freigabe: Path):
        self.freigabe = freigabe
        self.lauscher = Kermit.telnet_server(0)
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
    assert d.einstellungsaenderung["max_namen"] == 14
    d.host.setText("")
    d._annehmen()
    assert "Host" in d.meldung.text() and d.result() == 0


def test_konfiguration_schreibt_kein_kennwort_und_liest_alles_zurueck(qapp, tmp_path, lokal):
    from app.kermittool.ui.main_window import MainWindow
    pfad = tmp_path / "kermit.yaml"
    f = MainWindow(konfig_pfad=str(pfad), ordner=str(lokal))
    f.show()
    qapp.processEvents()
    f.einstellungen_setzen({**f.einst, "modus": "text", "paketlaenge": 100, "max_namen": 14})
    f.verbindung = {**f.verbindung, "host": "wega.example", "port": 4711, "login": True, "login_name": "kermit"}
    f._kennwort = "geheim-xyz-123"
    f.zuletzt = konfig.zuletzt_merken([], {**f.verbindung, "kennwort": "geheim-xyz-123"})
    f.freigabe = str(lokal)
    f.act_nur_lesen.setChecked(True)
    f.act_wieder_lauschen.setChecked(True)
    assert f.konfig_sichern()
    f.close()

    text = pfad.read_text(encoding="utf-8")
    assert "geheim-xyz-123" not in text and "kennwort" not in text.lower()
    assert "geschrieben_von" in text and "version: 1" in text

    g = MainWindow(konfig_pfad=str(pfad))
    try:
        assert g.einst["modus"] == "text" and g.einst["paketlaenge"] == 100 and g.einst["max_namen"] == 14
        assert g.verbindung["host"] == "wega.example" and g.verbindung["port"] == 4711
        assert g.verbindung["login_name"] == "kermit" and g._kennwort == ""
        assert g.freigabe == str(lokal) and g.act_nur_lesen.isChecked() and g.act_wieder_lauschen.isChecked()
        assert g.lokal_view.ordner == lokal
        assert len(g.zuletzt) == 1 and "kennwort" not in g.zuletzt[0]
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


def test_kennwoerter_fallen_aus_jeder_ebene():
    d = {"a": {"kennwort": "x", "b": [{"Kennwort_alt": "y", "ok": 1}]}, "ok": 2}
    assert konfig.ohne_kennwort(d) == {"a": {"b": [{"ok": 1}]}, "ok": 2}


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

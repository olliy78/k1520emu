"""Hauptfenster von K1520 Kermit.

Zwei Seiten wie beim DiskTool: LINKS der Server (sein Verzeichnis im Client-Modus, die Liste
der Uebertragungen im Server-Modus), RECHTS ein lokaler Ordner.  Dazwischen die beiden
Richtungen als Knoepfe; ziehen geht in beide Richtungen.

Alle Aufrufe der Bibliothek laufen im Arbeitsfaden (:mod:`app.kermittool.arbeiter`); das
Fenster beauftragt ihn und bekommt das Ergebnis ueber Signale zurueck
(``Qt.QueuedConnection``).  Es gibt also keinen blockierenden Aufruf im GUI-Faden.

Das Kennwort einer Anmeldung wird nur in ``self._kennwort`` gehalten (Sitzung) und nie
geschrieben (:mod:`app.kermittool.konfig`).
"""

from __future__ import annotations

from pathlib import Path
from typing import List, Optional

from PySide6.QtCore import QByteArray, QSize, Qt, QTimer
from PySide6.QtGui import QAction, QGuiApplication
from PySide6.QtWidgets import (
    QFileDialog, QFrame, QLabel, QMainWindow, QMessageBox, QSplitter, QStackedWidget, QToolBar,
    QToolButton, QVBoxLayout, QWidget,
)

from app import paths, version as _version
from app.disktool.ui.info_bar import InfoBar
from app.disktool.ui.log_dock import LogDock
from app.kermittool import binding, konfig
from app.kermittool.arbeiter import Arbeiter
from app.kermittool.ui import actions
from app.kermittool.ui.einstellungen_dialog import EinstellungenDialog
from app.kermittool.ui.fern_view import FernView
from app.kermittool.ui.fortschritt_dialog import FortschrittDialog
from app.kermittool.ui.lokal_view import LokalView
from app.kermittool.ui.server_view import ServerView
from app.kermittool.ui.verbindung_dialog import VerbindungDialog
from app.ui_help import HelpWindow

#: Das Handbuch liegt unter ``app/`` — ``doc/`` ist nicht im Paket.
HANDBUCH = Path(__file__).resolve().parents[1] / "help" / "handbuch.md"


class MainWindow(QMainWindow):
    DEFAULT_WIDTH = 1100
    DEFAULT_HEIGHT = 620

    def __init__(self, konfig_pfad: Optional[str] = None, ordner: Optional[str] = None):
        super().__init__()
        self.setWindowTitle("K1520 Kermit")
        self._konfig_pfad = konfig_pfad or konfig.pfad()
        daten = konfig.laden(self._konfig_pfad)

        # ── Zustand ─────────────────────────────────────────────────────────
        self.einst = konfig.einstellungen_aus(daten.get("uebertragung"))
        self.verbindung = konfig.verbindung_aus(daten.get("client"))
        self.verbindung["betriebsart"] = "client"
        srv = daten.get("server") if isinstance(daten.get("server"), dict) else {}
        self.server_verb = konfig.verbindung_aus(srv)
        self.server_verb["betriebsart"] = "server"
        if "port" not in srv:
            self.server_verb["port"] = 5001
        if "aufbau" not in srv:
            self.server_verb["aufbau"] = "lauschen"
        self.freigabe: str = str(srv.get("ordner", "") or "")
        self.zuletzt: List[dict] = [konfig.verbindung_aus(v) for v in daten.get("zuletzt", [])
                                    if isinstance(v, dict)][:konfig.ZULETZT_MAX]
        opt = daten.get("optionen") if isinstance(daten.get("optionen"), dict) else {}
        self._kennwort: str = ""              # nur Sitzung
        self._modus = "client"
        self.zustand_art = "getrennt"
        self._zustand_text = ""
        self._server_laeuft = False
        self._uebertragung = ""
        self.fern_pfad: List[str] = []
        self._cd_ziel: Optional[str] = None
        self._client_p: Optional[dict] = None
        self._maximiert_nachholen: Optional[bool] = None
        self._hilfe: Optional[HelpWindow] = None

        # ── Arbeitsfaden ────────────────────────────────────────────────────
        self.arbeiter = Arbeiter(self)
        self.arbeiter.einstellungen_setzen(self.einst)
        q = Qt.QueuedConnection
        self.arbeiter.zustand.connect(self._zustand, q)
        self.arbeiter.ereignis.connect(self._ereignis, q)
        self.arbeiter.fortschritt.connect(self._fortschritt, q)
        self.arbeiter.auftrag_beginnt.connect(self._auftrag_beginnt, q)
        self.arbeiter.auftrag_fertig.connect(self._auftrag_fertig, q)
        self.arbeiter.gegenstelle_meldet.connect(lambda t: self.log(f"Gegenstelle: {t}"), q)

        # ── Oberflaeche ─────────────────────────────────────────────────────
        actions.erzeuge_aktionen(self)
        self._baue_zentrum()
        self._baue_menue()
        self._baue_leiste(daten.get("window", {}).get("toolbar") if isinstance(daten.get("window"), dict) else None)
        self._baue_protokoll()
        self._baue_status()
        self.fortschritt_dialog = FortschrittDialog(self)
        self.fortschritt_dialog.abbrechen.connect(self._abbrechen)

        self.act_nur_lesen.setChecked(bool(srv.get("nur_lesen", False)))
        self.act_wieder_lauschen.setChecked(bool(opt.get("wieder_lauschen", False)))
        self.arbeiter.wieder_lauschen_setzen(self.act_wieder_lauschen.isChecked())

        lokal = ordner or (daten.get("lokal", {}) or {}).get("ordner") or str(paths.default_folder_dir())
        self.lokal_view.set_ordner(lokal)
        self._anwenden_fenster(daten.get("window", {}))
        self._aktionen_pruefen()
        self._zustand("getrennt", "")

    # ── Aufbau ──────────────────────────────────────────────────────────────

    def _baue_zentrum(self) -> None:
        self.fern_view = FernView()
        self.server_view = ServerView()
        self.server_kopf = QLabel("")
        self.server_kopf.setTextInteractionFlags(Qt.TextSelectableByMouse)
        seite = QWidget()
        lay = QVBoxLayout(seite)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(2)
        lay.addWidget(self.server_kopf)
        lay.addWidget(self.server_view, 1)
        self.links = QStackedWidget()
        self.links.addWidget(self.fern_view)       # 0 = Client
        self.links.addWidget(seite)                # 1 = Server

        self.lokal_view = LokalView()

        mitte = QWidget()
        ml = QVBoxLayout(mitte)
        ml.setContentsMargins(0, 0, 0, 0)
        ml.addStretch(1)
        for a in (self.act_holen, self.act_schreiben):
            b = QToolButton()
            b.setDefaultAction(a)
            b.setToolButtonStyle(Qt.ToolButtonIconOnly)
            ml.addWidget(b)
        ml.addStretch(1)

        self.teiler = QSplitter(Qt.Horizontal)
        self.teiler.addWidget(self.links)
        self.teiler.addWidget(mitte)
        self.teiler.addWidget(self.lokal_view)
        self.teiler.setStretchFactor(0, 1)
        self.teiler.setStretchFactor(1, 0)
        self.teiler.setStretchFactor(2, 1)
        self.teiler.setChildrenCollapsible(False)
        self.teiler.setSizes([500, 40, 500])

        self.info_bar = InfoBar()
        lay = QVBoxLayout()
        lay.setContentsMargins(6, 4, 6, 4)
        lay.addWidget(self.info_bar)
        lay.addWidget(self.teiler, 1)
        zentral = QWidget()
        zentral.setLayout(lay)
        self.setCentralWidget(zentral)

        self.fern_view.wechseln.connect(self._wechseln)
        self.fern_view.holen.connect(self._holen_namen)
        self.fern_view.senden.connect(self._senden_pfade)
        self.lokal_view.senden.connect(self._senden_pfade)
        self.lokal_view.holen.connect(self._holen_namen)
        self.lokal_view.ordner_waehlen.connect(self._ordner_dialog)
        self.lokal_view.hinweis.connect(self.log)
        self.lokal_view.ordner_geaendert.connect(lambda _: self._aktionen_pruefen())
        self.fern_view.baum.itemSelectionChanged.connect(self._aktionen_pruefen)
        self.lokal_view.baum.itemSelectionChanged.connect(self._aktionen_pruefen)
        self.fern_view.setze_aktionen(self.act_holen, self.act_loeschen, None, self.act_hinauf,
                                      self.act_aktualisieren)
        self.lokal_view.setze_aktionen(self.act_schreiben, None, self.act_ordner)

    def _baue_menue(self) -> None:
        m = self.menuBar().addMenu("&Datei")
        m.addAction(self.act_ordner)
        m.addSeparator()
        m.addAction(self.act_beenden)

        m = self.menuBar().addMenu("&Verbindung")
        m.addAction(self.act_verbinden)
        m.addAction(self.act_trennen)
        m.addAction(self.act_gegenstelle_beenden)
        m.addSeparator()
        m.addAction(self.act_server_einrichten)
        m.addAction(self.act_server_ordner)
        m.addAction(self.act_server_start)
        m.addAction(self.act_server_stopp)
        m.addAction(self.act_nur_lesen)
        m.addSeparator()
        m.addAction(self.act_wieder_lauschen)
        m.addAction(self.act_abbrechen)
        m.addSeparator()
        m.addAction(self.act_holen)
        m.addAction(self.act_schreiben)
        m.addAction(self.act_loeschen)

        self.menue_ansicht = self.menuBar().addMenu("&Ansicht")
        m = self.menuBar().addMenu("&Hilfe")
        m.addAction(self.act_hilfe)
        m.addAction(self.act_ueber)

    def _baue_leiste(self, namen: Optional[list]) -> None:
        self.leiste = QToolBar("Symbolleiste", self)
        self.leiste.setObjectName("haupt")
        self.leiste.setToolButtonStyle(Qt.ToolButtonTextUnderIcon)
        self.addToolBar(Qt.TopToolBarArea, self.leiste)
        # Die Umschalter kommen von Qt selbst — EINMAL geholt (QToolBar.clear() gaebe die
        # Huelle eines toggleViewAction() frei, deshalb wird die Leiste weiter unten nur
        # per removeAction() umgebaut).
        self.act_leiste_zeigen = self.leiste.toggleViewAction()
        self.act_leiste_zeigen.setText("&Symbolleiste")
        self.menue_ansicht.addAction(self.act_leiste_zeigen)
        self._leisten_inhalt: List[Optional[str]] = []
        self._leiste_fuellen(namen if isinstance(namen, list) and namen else konfig.LEISTE_VORGABE)

    def _leiste_fuellen(self, namen: list) -> None:
        """Symbolleiste aus einer Namensliste (``""`` = Trenner); UNBEKANNTE NAMEN werden uebergangen."""
        for a in list(self.leiste.actions()):
            self.leiste.removeAction(a)        # einzeln, nicht clear() (siehe oben)
        inhalt: List[Optional[str]] = []
        for n in namen:
            if not n:
                self.leiste.addSeparator()
                inhalt.append(None)
                continue
            a = getattr(self, f"act_{n}", None)
            if isinstance(a, QAction):
                self.leiste.addAction(a)
                inhalt.append(n)
        self._leisten_inhalt = inhalt

    def _baue_protokoll(self) -> None:
        self.log_dock = LogDock(self)
        self.addDockWidget(Qt.BottomDockWidgetArea, self.log_dock)
        self.log_dock.hide()
        self.resizeDocks([self.log_dock], [170], Qt.Vertical)
        self.act_protokoll_zeigen = self.log_dock.toggleViewAction()
        self.act_protokoll_zeigen.setText("&Protokoll")
        self.act_protokoll_zeigen.setShortcut("Ctrl+Shift+L")
        self.menue_ansicht.addAction(self.act_protokoll_zeigen)
        self.menue_ansicht.addAction(self.act_aktualisieren)
        self.menue_ansicht.addAction(self.act_einstellungen)

    def _baue_status(self) -> None:
        self.st_zustand = QLabel("")
        self.st_zustand.setMargin(2)
        self.st_modus = QLabel("")
        self.st_modus.setMargin(2)
        self.st_modus.setToolTip("Übertragungsart — Einstellungen")
        leiste = self.statusBar()
        leiste.setSizeGripEnabled(True)
        leiste.addPermanentWidget(self.st_zustand)
        strich = QFrame()
        strich.setFrameShape(QFrame.VLine)
        strich.setFrameShadow(QFrame.Sunken)
        leiste.addPermanentWidget(strich)
        leiste.addPermanentWidget(self.st_modus)
        self._modus_anzeigen()

    # ── Anzeige und Zustand ─────────────────────────────────────────────────

    def log(self, text: str) -> None:
        self.log_dock.append(text)

    def meldung(self, text: str, stufe: str = "hinweis") -> None:
        """Dauerhafte Meldung im Streifen UND im Protokoll (nie ein modales Fenster)."""
        self.log(text)
        self.info_bar.zeige(text, stufe)

    def _modus_anzeigen(self) -> None:
        self.st_modus.setText({"binaer": "binär", "text": "Text", "automatik": "Automatik"}
                              [self.einst["modus"]])

    def _status_text(self) -> str:
        if self._uebertragung and self.zustand_art == "verbunden":
            return f"Übertragung: {self._uebertragung}"
        if self.zustand_art == "getrennt":
            return "Getrennt"
        text = self._zustand_text or self.zustand_art
        return ("Server: " + text) if self._server_laeuft else text

    def _zustand(self, art: str, text: str) -> None:
        self.zustand_art = art
        self._zustand_text = text
        self.st_zustand.setText(self._status_text())
        if art == "getrennt":
            self._uebertragung = ""
        self._aktionen_pruefen()

    def _ereignis(self, text: str) -> None:
        self.log(text)
        if self._modus == "server":
            self.server_view.ereignis(text)

    def _fortschritt(self, datei: str, b: int, g: int) -> None:
        # Die Bibliothek meldet den lokalen Pfad; angezeigt wird nur der Dateiname.
        datei = datei.replace("\\", "/").rsplit("/", 1)[-1]
        self._uebertragung = datei
        self.st_zustand.setText(self._status_text())
        if self.fortschritt_dialog.isVisible():
            self.fortschritt_dialog.setze(datei, b, g)
        if self._modus == "server":
            self.server_view.fortschritt(datei, b, g)

    def _aktionen_pruefen(self) -> None:
        getrennt = self.zustand_art == "getrennt"
        client = self._modus == "client" and not self._server_laeuft
        bereit = (self.zustand_art == "verbunden" and client and not self.arbeiter.beschaeftigt)
        self.act_verbinden.setEnabled(getrennt and not self._server_laeuft)
        self.act_trennen.setEnabled((not getrennt) and not self._server_laeuft)
        self.act_gegenstelle_beenden.setEnabled(bereit)
        self.act_server_einrichten.setEnabled(not self._server_laeuft and getrennt)
        self.act_server_ordner.setEnabled(not self._server_laeuft)
        self.act_server_start.setEnabled(not self._server_laeuft and getrennt)
        self.act_server_stopp.setEnabled(self._server_laeuft)
        self.act_abbrechen.setEnabled(self.arbeiter.beschaeftigt and not getrennt)
        self.act_holen.setEnabled(bereit and bool(self.fern_view.dateinamen())
                                  and self.lokal_view.ordner is not None)
        self.act_schreiben.setEnabled(bereit and bool(self.lokal_view.dateien()))
        self.act_loeschen.setEnabled(bereit and bool(self.fern_view.dateinamen()))
        self.act_hinauf.setEnabled(bereit)
        self.act_aktualisieren.setEnabled(True)

    # ── Client ──────────────────────────────────────────────────────────────

    def _verbinden_dialog(self) -> bool:
        dlg = VerbindungDialog(self, "client", self.verbindung, self.zuletzt)
        if dlg.exec() != VerbindungDialog.Accepted:
            return False
        if dlg.einstellungsaenderung:
            self.einst.update(dlg.einstellungsaenderung)
            self.arbeiter.einstellungen_setzen(self.einst)
        self.client_verbinden(dlg.verbindung(), dlg.kennwort())
        return True

    def client_verbinden(self, p: dict, kennwort: str = "") -> None:
        """Verbindung herstellen (Aufruf auch ohne Dialog, z. B. aus Tests)."""
        p = konfig.verbindung_aus(p)
        p["betriebsart"] = "client"
        self.verbindung = p
        self._kennwort = kennwort or ""
        self._client_p = p
        self.zuletzt = konfig.zuletzt_merken(self.zuletzt, p)
        self._modus = "client"
        self.links.setCurrentIndex(0)
        self.fern_pfad = []
        self.fern_view.leeren("Verbinde …")
        self.info_bar.verbergen()
        self.arbeiter.verbinden(p, self._kennwort)
        self._aktionen_pruefen()

    def _trennen(self) -> None:
        self.arbeiter.trennen()
        self.fern_view.leeren()
        self.fern_pfad = []

    def _gegenstelle_beenden(self) -> None:
        self.arbeiter.gegenstelle_beenden()

    def _pfad_text(self) -> str:
        return "Server: /" + "/".join(self.fern_pfad) if self.fern_pfad else "Server: (Startverzeichnis)"

    def _wechseln(self, name: str) -> None:
        if self.zustand_art != "verbunden" or self.arbeiter.beschaeftigt:
            return
        self._cd_ziel = name
        self.arbeiter.wechseln(name)
        self._aktionen_pruefen()

    def _hinauf(self) -> None:
        self._wechseln("..")

    def _aktualisieren(self) -> None:
        self.lokal_view.aktualisieren()
        if self._modus == "client" and self.zustand_art == "verbunden" and not self.arbeiter.beschaeftigt:
            self.arbeiter.verzeichnis()
            self._aktionen_pruefen()

    def _holen_namen(self, namen: list) -> None:
        if (self.zustand_art != "verbunden" or self.arbeiter.beschaeftigt or not namen
                or self.lokal_view.ordner is None):
            return
        self.arbeiter.holen(namen, str(self.lokal_view.ordner))
        self._aktionen_pruefen()

    def _holen_auswahl(self) -> None:
        self._holen_namen(self.fern_view.dateinamen())

    def _senden_pfade(self, pfade: list) -> None:
        if self.zustand_art != "verbunden" or self._modus != "client":
            self.log("Nicht verbunden — nichts gesendet")
            self.statusBar().showMessage("Nicht verbunden — nichts gesendet", 4000)
            return
        if self.arbeiter.beschaeftigt or not pfade:
            return
        self.arbeiter.senden(pfade)
        self._aktionen_pruefen()

    def _senden_auswahl(self) -> None:
        self._senden_pfade(self.lokal_view.dateien())

    def _loeschen_auswahl(self) -> None:
        namen = self.fern_view.dateinamen()
        if not namen or self.arbeiter.beschaeftigt or self.zustand_art != "verbunden":
            return
        vorschau = "\n".join(namen[:8]) + ("\n…" if len(namen) > 8 else "")
        antwort = QMessageBox.question(
            self, "Auf dem Server löschen",
            f"{len(namen)} Datei(en) auf dem Server unwiderruflich löschen?\n\n{vorschau}",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if antwort != QMessageBox.Yes:
            return
        self.arbeiter.loeschen(namen)
        self._aktionen_pruefen()

    def _abbrechen_aktion(self) -> None:
        self._abbrechen(False)

    def _abbrechen(self, hart: bool) -> None:
        self.log("Abbruch (hart)" if hart else "Abbruch angefordert")
        self.arbeiter.abbrechen(hart)

    # ── Server ──────────────────────────────────────────────────────────────

    def _server_einrichten(self) -> bool:
        dlg = VerbindungDialog(self, "server", self.server_verb, self.zuletzt,
                               ordner=self.freigabe, nur_lesen=self.act_nur_lesen.isChecked())
        if dlg.exec() != VerbindungDialog.Accepted:
            return False
        if dlg.einstellungsaenderung:
            self.einst.update(dlg.einstellungsaenderung)
            self.arbeiter.einstellungen_setzen(self.einst)
        self.server_verb = dlg.verbindung()
        self.freigabe = dlg.freigabe()
        self.act_nur_lesen.setChecked(dlg.nur_lesen.isChecked())
        self.zuletzt = konfig.zuletzt_merken(self.zuletzt, self.server_verb)
        return True

    def _server_ordner(self) -> bool:
        d = QFileDialog.getExistingDirectory(self, "Ordner freigeben", self.freigabe or str(Path.home()))
        if not d:
            return False
        self.freigabe = d
        self.statusBar().showMessage(f"Freigabe: {d}", 4000)
        return True

    def _nur_lesen_umgeschaltet(self, an: bool) -> None:
        self.log("Nur lesen — gilt ab dem nächsten Start" if an else "Schreiben erlaubt — gilt ab dem nächsten Start")

    def _wieder_lauschen_umgeschaltet(self, an: bool) -> None:
        self.arbeiter.wieder_lauschen_setzen(an)

    def server_starten(self) -> bool:
        """Server starten.  ``False`` (und eine Meldung), wenn kein Ordner freigegeben ist."""
        if self._server_laeuft or self.zustand_art != "getrennt":
            return False
        if not self.freigabe or not Path(self.freigabe).is_dir():
            self.meldung("Kein Ordner freigegeben — bitte zuerst „Ordner freigeben…“ wählen.", "warnung")
            return False
        p = dict(self.server_verb)
        p["betriebsart"] = "server"
        if p["aufbau"] == "verbinden" and not p["host"]:
            self.meldung("Server soll verbinden, aber kein Host eingetragen — „Als Server einrichten…“.", "warnung")
            return False
        self._modus = "server"
        self._server_laeuft = True
        self.links.setCurrentIndex(1)
        self.server_view.leeren()
        self.server_kopf.setText(f"Freigabe: {self.freigabe}" + ("  (nur lesen)" if self.act_nur_lesen.isChecked() else ""))
        self.info_bar.verbergen()
        self.arbeiter.wieder_lauschen_setzen(self.act_wieder_lauschen.isChecked())
        self.arbeiter.server(p, self.freigabe, self.act_nur_lesen.isChecked())
        self._aktionen_pruefen()
        return True

    def _server_start_aktion(self) -> None:
        self.server_starten()

    def _server_stopp(self) -> None:
        self.arbeiter.stoppen()

    # ── Rueckmeldungen des Arbeitsfadens ────────────────────────────────────

    def _auftrag_beginnt(self, name: str) -> None:
        if name == "holen":
            self.fortschritt_dialog.beginne("Vom Server holen")
        elif name == "senden":
            self.fortschritt_dialog.beginne("Zum Server senden")
        self._aktionen_pruefen()

    def _auftrag_fertig(self, name: str, ok: bool, code: int, text: str, daten) -> None:
        self.fortschritt_dialog.ende()
        self._uebertragung = ""
        self.st_zustand.setText(self._status_text())
        if not ok:
            self._fehler(name, code, text)
        elif name == "verbinden":
            self.log(f"Verbunden mit {daten}")
            self.arbeiter.verzeichnis()
        elif name in ("verzeichnis", "wechseln"):
            if name == "wechseln" and self._cd_ziel is not None:
                if self._cd_ziel == "..":
                    if self.fern_pfad:
                        self.fern_pfad.pop()
                    else:
                        self.fern_pfad = []
                else:
                    self.fern_pfad.append(self._cd_ziel)
            self._cd_ziel = None
            self.fern_view.zeige(daten or [], self._pfad_text())
        elif name == "holen":
            self.lokal_view.aktualisieren()
            self.meldung(f"{len(daten or [])} Datei(en) geholt: " + ", ".join(daten or []), "hinweis")
        elif name == "senden":
            self.meldung(f"{len(daten or [])} Datei(en) gesendet", "hinweis")
            self.arbeiter.verzeichnis()
        elif name == "loeschen":
            self.arbeiter.verzeichnis()
        elif name in ("finish", "trennen"):
            self.fern_view.leeren()
            self.fern_pfad = []
        elif name == "server":
            self._server_laeuft = False
            self.log(str(daten or "Server beendet"))
            self.statusBar().showMessage(str(daten or "Server beendet"), 6000)
        self._aktionen_pruefen()

    def _fehler(self, name: str, code: int, text: str) -> None:
        """Ein gescheiterter Auftrag: Meldung, bei toter Leitung aufraeumen, ggf. erneut lauschen."""
        self._cd_ziel = None
        if name == "server":
            self._server_laeuft = False
        if code == binding.E_ABORTED:
            self.meldung("Abgebrochen." if name != "verbinden" else "Verbinden abgebrochen.", "hinweis")
        else:
            titel = {"verbinden": "Verbindung", "holen": "Holen", "senden": "Senden", "loeschen": "Löschen",
                     "verzeichnis": "Verzeichnis", "wechseln": "Verzeichniswechsel",
                     "server": "Server", "finish": "Finish"}.get(name, name)
            self.meldung(f"{titel} fehlgeschlagen: {text or 'Fehler ' + str(code)}", "fehler")
        if code in (binding.E_DISCONNECTED, binding.E_LINE, binding.E_LOGIN):
            self.fern_view.leeren("Getrennt")
            self.fern_pfad = []
        if name in ("holen",):
            self.lokal_view.aktualisieren()
        elif name in ("senden", "loeschen") and self.zustand_art == "verbunden":
            self.arbeiter.verzeichnis()
        # Ein Lauscher nimmt nur EINE Verbindung an: nach deren Ende lauscht der Client erneut.
        if (code == binding.E_DISCONNECTED and self.act_wieder_lauschen.isChecked() and self._client_p
                and self._client_p.get("aufbau") == "lauschen" and self._modus == "client"):
            self.log("Gegenstelle getrennt — lausche erneut")
            self.client_verbinden(self._client_p, self._kennwort)

    # ── Einstellungen, Ordner, Hilfe ────────────────────────────────────────

    def _einstellungen_dialog(self) -> bool:
        dlg = EinstellungenDialog(self, self.einst)
        if dlg.exec() != EinstellungenDialog.Accepted:
            return False
        self.einstellungen_setzen(dlg.werte())
        return True

    def einstellungen_setzen(self, einst: dict) -> None:
        self.einst = konfig.einstellungen_aus(einst)
        self.arbeiter.einstellungen_setzen(self.einst)
        self._modus_anzeigen()

    def _ordner_dialog(self) -> None:
        start = str(self.lokal_view.ordner or Path.home())
        d = QFileDialog.getExistingDirectory(self, "Lokalen Ordner wählen", start)
        if d:
            self.lokal_view.set_ordner(d)

    def open_help(self) -> HelpWindow:
        if self._hilfe is not None and self._hilfe.isVisible():
            self._hilfe.raise_()
            self._hilfe.activateWindow()
            return self._hilfe
        self._hilfe = HelpWindow(self, "K1520 Kermit — Handbuch", HANDBUCH)
        self._hilfe.show()
        return self._hilfe

    def _ueber_dialog(self) -> None:
        try:
            lib = binding.version()
        except Exception as e:                  # noqa: BLE001 — Bibliothek fehlt: trotzdem Ueber zeigen
            lib = f"nicht geladen ({e})"
        QMessageBox.about(
            self, "Über K1520 Kermit",
            f"<h3>K1520 Kermit</h3><p>Dateiaustausch mit der WEGA (P8000) und anderen Kermit-Gegenstellen "
            f"über Telnet oder RFC 2217 — als Client und als Server.</p>"
            f"<p>Version: {_version.fassung()}<br>Bibliothek: {lib}</p>")

    # ── Konfiguration und Fensterlage ───────────────────────────────────────

    def konfig_daten(self) -> dict:
        """Der zu schreibende Zustand (OHNE Kennwoerter)."""
        return {
            "uebertragung": dict(self.einst),
            "client": dict(self.verbindung),
            "server": {**self.server_verb, "ordner": self.freigabe,
                       "nur_lesen": self.act_nur_lesen.isChecked()},
            "optionen": {"wieder_lauschen": self.act_wieder_lauschen.isChecked()},
            "zuletzt": list(self.zuletzt),
            "lokal": {"ordner": str(self.lokal_view.ordner) if self.lokal_view.ordner else ""},
            "window": self._fenster_daten(),
        }

    def konfig_sichern(self) -> bool:
        return konfig.schreiben(self.konfig_daten(), self._konfig_pfad)

    def _fenster_daten(self) -> dict:
        groesse = self.normalGeometry().size() if self.isMaximized() else self.size()
        return {
            "geometry": bytes(self.saveGeometry().toBase64()).decode("ascii"),
            "width": int(groesse.width()),
            "height": int(groesse.height()),
            "maximized": bool(self.isMaximized()),
            "dock_state": bytes(self.saveState().toBase64()).decode("ascii"),
            "toolbar": ["" if n is None else n for n in self._leisten_inhalt],
            "toolbar_style": int(self.leiste.toolButtonStyle().value),
        }

    def _anwenden_fenster(self, win) -> None:
        """Groesse, Lage, Maximierung, Leiste und Dockzustand herstellen.

        Wie beim Emulator (``app/ui/main_window.py::_geometrie_herstellen``): ``restoreGeometry``
        traegt „maximiert" NICHT zurueck, und vor dem Anzeigen verfaellt der Zustand.  Darum
        entscheidet das eigene Feld ``maximized``, nachgeholt wird im ``showEvent``.
        """
        if not isinstance(win, dict):
            win = {}
        ok = False
        geo = win.get("geometry")
        if geo:
            try:
                ok = bool(self.restoreGeometry(QByteArray.fromBase64(str(geo).encode("ascii"))))
            except Exception:                   # noqa: BLE001
                ok = False
        if not ok:
            w = int(win.get("width", self.DEFAULT_WIDTH) or self.DEFAULT_WIDTH)
            h = int(win.get("height", self.DEFAULT_HEIGHT) or self.DEFAULT_HEIGHT)
            screen = self.screen() or QGuiApplication.primaryScreen()
            if screen is not None:
                av = screen.availableGeometry()
                if w > av.width() or h > av.height():
                    w, h = self.DEFAULT_WIDTH, self.DEFAULT_HEIGHT
            self.resize(w, h)
        self.setWindowState(self.windowState() & ~Qt.WindowMaximized)
        self._maximiert_nachholen = bool(win.get("maximized", False))
        stil = win.get("toolbar_style")
        if stil is not None:
            try:
                self.leiste.setToolButtonStyle(Qt.ToolButtonStyle(int(stil)))
            except (TypeError, ValueError):
                pass
        state = win.get("dock_state")
        if state:
            try:
                self.restoreState(QByteArray.fromBase64(str(state).encode("ascii")))
            except Exception:                   # noqa: BLE001
                pass
        self.statusBar().show()

    def _maximiert_setzen(self, an: bool) -> None:
        if an == self.isMaximized():
            return
        if an:
            self.setWindowState(self.windowState() | Qt.WindowMaximized)
        else:
            self.setWindowState(self.windowState() & ~Qt.WindowMaximized)

    def showEvent(self, event):                 # noqa: N802
        super().showEvent(event)
        wunsch, self._maximiert_nachholen = self._maximiert_nachholen, None
        if wunsch is not None:
            QTimer.singleShot(0, lambda: self._maximiert_setzen(wunsch))

    def closeEvent(self, event):                # noqa: N802
        self.konfig_sichern()
        self.arbeiter.beenden()
        if self._hilfe is not None:
            self._hilfe.close()
        super().closeEvent(event)

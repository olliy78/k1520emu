"""Hauptfenster von K1520 Printer (Entwurf 49 §4.5, §5d).

Aufbau wie die Emulatoren: in der Mitte statt des Bildschirms das **Blatt** (`blatt_widget.py`),
rechts als Docks das **Bedienfeld** (aus der Typbeschreibung gebaut, `bedienfeld.py`) und die
**Einstellungen** mit den Reitern *Drucker* und *Schnittstellen* (der vorhandene Baustein
`app/ui/serial_widget.py`, nicht nachgebaut).  Unten die Statuszeile.

Eigenes Fenster statt `main_window.py` (3 200 Zeilen, gebaut fuer Rechner mit Bildschirm,
Laufwerken und Tastatur): das Druckerfenster importiert nur Bausteine - Emulationstakt,
Seriell-Reiter, Statuszeilen-Lampe, Handbuch -, und die sechs anderen Programme bleiben
unberuehrt.

**Der Kern rechnet im EIGENEN Faden** (`app/emulationstakt.py`); alle Kernaufrufe laufen ueber
die Kernsperre der Bindung.  Die Oberflaeche zieht Blatt, Kopf und Lampen im Bildtakt nach.

**Neuanlage.**  Druckertyp, ROM-Fassung und Schnittstellenwahl sind Eigenschaften der Maschine, die
beim Anlegen feststehen (`k1520_create_printer`-Konfigurationstext): ihr Wechsel legt den
Drucker neu an, nach Rueckfrage.  Das Blatt liegt im Kern; es geht dabei verloren (die Rueckfrage
sagt es und bietet vorher PDF an).  Ein "Blatt bleibt" waere nur mit einer C-ABI-Funktion zum
Einspielen von Zeilen moeglich - Abweichung von Entwurf 49 §4.5, dort nachgezogen.
"""

from __future__ import annotations

import base64
from pathlib import Path
from typing import Optional

from PySide6.QtCore import QByteArray, QSize, Qt, QTimer
from PySide6.QtGui import QStandardItem, QStandardItemModel
from PySide6.QtPrintSupport import QPrintDialog, QPrinter
from PySide6.QtWidgets import (
    QComboBox, QDockWidget, QFileDialog, QFormLayout, QFrame, QGridLayout, QHBoxLayout, QLabel, QMainWindow, QMessageBox,
    QPushButton, QScrollArea, QScrollBar, QSizePolicy, QSpinBox, QTabWidget, QToolBar, QVBoxLayout, QWidget,
)

from app import version as _version
from app.core_binding import k1520 as K
from app.emulationstakt import Emulationstakt
from app.printertool import aktionen, ausgabe, konfig, rom_katalog, typen
from app.printertool.bedienfeld import Bedienfeld
from app.printertool.blatt_widget import BlattWidget
from app.ui.serial_widget import SerialWidget
from app.ui.status_bar import SeriellFeld
from app.ui_help import HelpWindow

#: Das Handbuch liegt unter ``app/`` - ``doc/`` ist nicht im Paket.
HANDBUCH = Path(__file__).resolve().parent / "help" / "handbuch.md"

#: Wie lange ein "getippter" Tastendruck (PV1/PV2/TEST aus dem Menue) gehalten wird.
TIPP_MS = {K.DRUCKER_TASTE_PV1: 120, K.DRUCKER_TASTE_PV2: 120, K.DRUCKER_TASTE_TEST: 600}


class BlattAnsicht(QWidget):
    """Blatt mit Rollbalken (senkrecht und - bei Zoom ueber der Fensterbreite - waagerecht)."""

    def __init__(self, parent=None, bahn: int = 0):
        super().__init__(parent)
        self.blatt = BlattWidget(bahn=bahn)
        self.vbar = QScrollBar(Qt.Vertical)
        self.hbar = QScrollBar(Qt.Horizontal)
        lay = QGridLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(0)
        lay.addWidget(self.blatt, 0, 0)
        lay.addWidget(self.vbar, 0, 1)
        lay.addWidget(self.hbar, 1, 0)
        self._sync = False
        self.vbar.valueChanged.connect(self._gerollt)
        self.hbar.valueChanged.connect(self._gerollt)
        self.blatt.ansicht_geaendert.connect(self._nachziehen)
        self._nachziehen()

    def _gerollt(self, _=None):
        if self._sync:
            return
        ox, oy = self.hbar.value(), self.vbar.value()      # vor dem Signal merken: es zieht die Balken nach
        if self.blatt.folgen():
            self.blatt.folgen_beendet.emit()
        self.blatt.rollen_nach(ox, oy)

    def _nachziehen(self):
        b = self.blatt
        sb, sh = b.ansicht_masse()
        ox, oy = b.ansicht_ursprung()
        self._sync = True
        self.vbar.setRange(0, max(0, int(b.blatt_hoehe() + 20 - sh * 0.2)))
        self.vbar.setPageStep(max(1, int(sh)))
        self.vbar.setValue(int(oy))
        self.hbar.setRange(0, max(0, int(b.blatt_breite() - sb)))
        self.hbar.setPageStep(max(1, int(sb)))
        self.hbar.setValue(int(ox))
        self.hbar.setVisible(b.blatt_breite() - sb > 1)
        self._sync = False


class BelegKasten(QWidget):
    """K6316: Vorsteckschacht.  Masse waehlen, Beleg einlegen/entnehmen, das Belegblatt (Bahn 1) mit Kopf.

    Einziehen, Positionieren und Auswerfen macht die FIRMWARE (BEZ/BAU vom Rechner, S4 offline) - der Kasten
    steckt den Beleg nur vor die Lichtschranke FSO bzw. nimmt ihn heraus (hardware.md §11)."""

    def __init__(self, fenster: "PrinterWindow"):
        super().__init__()
        self._fenster = fenster
        self._abgelehnt = False
        lay = QVBoxLayout(self)
        form = QFormLayout()
        self.laenge = QSpinBox()
        self.laenge.setRange(74, 297)              # Handbuch: 74...297 x 74...210 mm, Mindestlaenge 72 mm
        self.laenge.setValue(148)
        self.laenge.setSuffix(" mm")
        self.breite = QSpinBox()
        self.breite.setRange(74, 210)
        self.breite.setValue(105)
        self.breite.setSuffix(" mm")
        form.addRow("Länge:", self.laenge)
        form.addRow("Breite:", self.breite)
        lay.addLayout(form)
        knoepfe = QHBoxLayout()
        self.einlegen = QPushButton("Beleg einlegen")
        self.entnehmen = QPushButton("Beleg entnehmen")
        for k in (self.einlegen, self.entnehmen):
            k.setFocusPolicy(Qt.NoFocus)
            knoepfe.addWidget(k)
        lay.addLayout(knoepfe)
        self.zustand = QLabel()
        self.zustand.setWordWrap(True)
        lay.addWidget(self.zustand)
        self.ansicht = BlattAnsicht(bahn=1)
        self.ansicht.setMinimumHeight(180)
        lay.addWidget(self.ansicht, 1)
        self.einlegen.clicked.connect(self._einlegen)
        self.entnehmen.clicked.connect(self._entnehmen)

    def _einlegen(self) -> None:
        emu = self._fenster.emu
        self._abgelehnt = emu is not None and not emu.printer_beleg_insert(self.laenge.value(), self.breite.value())
        self._fenster.aktualisieren()

    def _entnehmen(self) -> None:
        emu = self._fenster.emu
        if emu is not None:
            emu.printer_beleg_remove()
        self._abgelehnt = False
        self._fenster.aktualisieren()

    def aktualisieren(self, emu) -> None:
        st = emu.printer_beleg_state()
        if st is None:
            return
        if not st["eingelegt"]:
            text = "Schacht leer."
        elif st["druck_y"] >= 0:
            text = "Beleg eingezogen, liegt unter der Druckzeile."
        elif st["u"] <= 0:
            text = "Beleg steckt vor der Lichtschranke."
        else:
            text = "Beleg im Schacht."
        if self._abgelehnt:
            text = "Einlegen nicht möglich: es liegt schon ein Beleg im Schacht."
        if text != self.zustand.text():
            self.zustand.setText(text)
        self.einlegen.setEnabled(not st["eingelegt"])
        self.entnehmen.setEnabled(bool(st["eingelegt"]))
        self.ansicht.blatt.aktualisieren(emu)


class DruckerEinstellungen(QWidget):
    """Reiter *Drucker*: Typ, ROM, Schnittstelle (Neuanlage), Tempo, Seitenlaenge."""

    def __init__(self, fenster: "PrinterWindow"):
        super().__init__(fenster)
        self.f = fenster
        lay = QVBoxLayout(self)
        form = QFormLayout()
        self.form = form
        lay.addLayout(form)

        self.typ = QComboBox()
        modell = QStandardItemModel(self.typ)
        for t in typen.TYPEN:
            modell.appendRow(self._eintrag(t.titel, t.schluessel, None))
        for schluessel, titel, grund in typen.GESPERRT:
            modell.appendRow(self._eintrag(f"{titel} - nicht waehlbar", schluessel, grund))
        self.typ.setModel(modell)
        self.typ.setToolTip("Druckertyp.  Der Wechsel legt den Drucker neu an (das Blatt wird dabei geleert).")
        form.addRow("Druckertyp:", self.typ)

        # EPROM-Auswahl: nur bei Typen mit mehr als einem Katalogeintrag sichtbar (rom_anbieten).
        self.rom = QComboBox()
        self._rom_eintraege: dict = {}
        form.addRow("EPROM:", self.rom)
        self.rom_text = QLabel()
        self.rom_text.setWordWrap(True)
        form.addRow(self.rom_text)

        self.schnittstelle = QComboBox()
        for schluessel, anzeige in typen.SCHNITTSTELLEN:
            self.schnittstelle.addItem(anzeige, schluessel)
        self.schnittstelle.setToolTip("Schalterblock des SD 1157 (PIO 3 A.0): welche der beiden Schnittstellen die "
                                      "Firmware benutzt.  Der Wechsel startet den Drucker neu.")
        form.addRow("Schnittstelle:", self.schnittstelle)
        self.schnittstelle_text = QLabel("Schalter am Drucker: IFSS (Stromschleife/20 mA) oder V.24.  "
                                         "Der Wechsel startet den Drucker neu und leert das Blatt.")
        self.schnittstelle_text.setWordWrap(True)
        form.addRow(self.schnittstelle_text)

        self.tempo = QComboBox()
        for f in konfig.TEMPO_STUFEN:
            self.tempo.addItem("unbegrenzt" if f <= 0 else ("Echtzeit" if f == 1 else f"{f:g} x"), f)
        self.tempo.setToolTip("Rechengeschwindigkeit gegenüber dem Drucker (1 x = Nenntakt 2,4576 MHz).")
        form.addRow("Tempo:", self.tempo)

        self.seite = QComboBox()
        for z in konfig.FORMULARLAENGEN_ZOLL:
            self.seite.addItem(f"{z} Zoll", z)
        self.seite.setToolTip("Seitenlänge für PDF und Druck (und die Perforation im Bild).")
        form.addRow("Seitenlänge:", self.seite)
        self.seite_text = QLabel("Wie das Blatt für PDF und Druck in Seiten geteilt wird.  Der Formularvorschub "
                                 "(FF) der Firmware fährt im Kern 12 Zoll (Vorgabe, nicht einstellbar).")
        self.seite_text.setWordWrap(True)
        form.addRow(self.seite_text)

        self.info = QLabel()
        self.info.setWordWrap(True)
        form.addRow("Blatt:", self.info)
        lay.addStretch()

        for text in (self.rom_text, self.schnittstelle_text, self.seite_text, self.info):
            text.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.MinimumExpanding)
            text.setStyleSheet("color: gray;")
        self.rom.currentIndexChanged.connect(self._rom_text)
        self._rom_text()

    @staticmethod
    def _eintrag(text: str, schluessel: str, sperrgrund: Optional[str]) -> QStandardItem:
        item = QStandardItem(text)
        item.setData(schluessel, Qt.UserRole)
        if sperrgrund:
            item.setEnabled(False)
            item.setToolTip(sperrgrund)
        return item

    def _rom_text(self, _=None):
        e = self._rom_eintraege.get(self.rom.currentData())
        if e is None:
            self.rom_text.setText("")
            return
        zeilen = [e.hinweis] if e.hinweis else []
        zeilen.append(f"Güte: {e.guete}" + (f" · {e.pgm}" if e.pgm else "")
                      + (f" · Zeichensatz: {e.zeichensatz}" if e.zeichensatz else ""))
        self.rom_text.setText("\n".join(zeilen))

    def rom_anbieten(self, eintraege, gewaehlt: str) -> None:
        """Die Eintraege des Katalogs zum Typ ins Auswahlfeld; bei weniger als zwei bleibt das Feld verborgen."""
        self.rom.blockSignals(True)
        self.rom.clear()
        self._rom_eintraege = {e.schluessel: e for e in eintraege}
        for e in eintraege:
            self.rom.addItem(e.titel + (" (Vorgabe)" if e.vorgabe else ""), e.schluessel)
        self.rom.setCurrentIndex(max(0, self.rom.findData(gewaehlt)))
        self.rom.blockSignals(False)
        sichtbar = len(eintraege) > 1
        for w in (self.rom, self.form.labelForField(self.rom), self.rom_text):
            if w is not None:
                w.setVisible(sichtbar)
        self._rom_text()

    def sperrgrund(self, schluessel: str) -> str:
        return next((g for s, _t, g in typen.GESPERRT if s == schluessel), "")


class PrinterWindow(QMainWindow):
    DEFAULT_WIDTH = 1280
    DEFAULT_HEIGHT = 760

    def __init__(self, konfig_daten: Optional[dict] = None, konfig_pfad: Optional[str] = None,
                 autostart: bool = True):
        super().__init__()
        self.setWindowTitle("K1520 Printer")
        self._konfig_pfad = konfig_pfad
        daten = konfig.laden(konfig_pfad) if konfig_daten is None else konfig_daten
        allg, dr = daten.get("general", {}) or {}, daten.get("printer", {}) or {}

        self.beschreibung = typen.typ(str(allg.get("model", "")))
        # EPROM-Wahl je Typ (printer.rom.<typ>, Schluessel oder Alias des Katalogs).  Alt: `rom: repariert` (ein Text) = SD 1157.
        self._roms = rom_katalog.laden()
        wahl = dr.get("rom")
        self._rom_je_typ = {str(k): str(v) for k, v in wahl.items()} if isinstance(wahl, dict) else (
            {"sd1157": str(wahl)} if isinstance(wahl, str) else {})
        self._schnittstelle = dr.get("schnittstelle") if dr.get("schnittstelle") in ("ifss", "v24") else "ifss"
        self._betriebsart = dr.get("betriebsart") if dr.get("betriebsart") in ("aufwickeln", "schneiden", "endlos") else "endlos"
        try:
            self._dil = int(dr.get("dil", 0)) & 0xFFFF
        except (TypeError, ValueError):
            self._dil = 0
        self._tempo = konfig.tempo_waehlbar(allg.get("speed", 1.0))
        self._formular_zoll = konfig.zoll_waehlbar(dr.get("formular_zoll", 12))
        self._netz = True
        self._hilfe: Optional[HelpWindow] = None
        self._laden = True
        self._druckdialog = QPrintDialog          # Tests setzen einen Ersatz
        self.emu = self._emulator_anlegen()
        self.emu.printer_switch(K.DRUCKER_SW_ONLINE, bool(dr.get("online", True)))
        self._formular_an_kern()

        aktionen.erzeuge_aktionen(self)
        self.act_power.blockSignals(True)           # das Netz ist an: Haken setzen, ohne Neustart auszuloesen
        self.act_power.setChecked(True)
        self.act_power.setText("&Netz aus")
        self.act_power.blockSignals(False)
        self._menues_bauen()
        self._leiste_bauen()

        # ── Mitte: das Blatt ────────────────────────────────────────────────
        self.ansicht = BlattAnsicht()
        self.blatt = self.ansicht.blatt
        self.setCentralWidget(self.ansicht)
        self.blatt.folgen_beendet.connect(lambda: self.act_folgen.setChecked(False))
        self.blatt.kopf_geaendert.connect(self._kopf_anzeigen)
        self.blatt.set_formular_zeilen(self._formular_zeilen())

        # ── Rechts: Bedienfeld und Einstellungen ────────────────────────────
        self.bedienfeld = Bedienfeld(self.beschreibung)
        self.bedienfeld.taste.connect(self._taste)
        self.bedienfeld.schalter.connect(self._schalter)
        self.bedienfeld.netz.connect(self._netz_aus_bedienfeld)
        self.bedienfeld.neustart.connect(self._neustart)
        self.dock_bedienfeld = self._dock("Bedienfeld", self.bedienfeld, "dock_bedienfeld")

        self.einst_drucker = DruckerEinstellungen(self)
        self.seriell = SerialWidget(self.emu, ports=(("IFSS", 5000), ("V.24", 5001)),
                                    hinweis="Der Drucker benutzt die Schnittstelle, die im Reiter <i>Drucker</i> "
                                            "gewählt ist (IFSS oder V.24).  Ein Rechner-Emulator verbindet sich "
                                            "als Telnet-/RFC-2217-Client mit dem Port dieses Servers.")
        self.seriell.statuszeile.connect(self._seriell_status)
        self.reiter = QTabWidget()
        self.reiter.addTab(self.einst_drucker, "Drucker")
        self.reiter.addTab(self.seriell, "Schnittstellen")
        self.dock_einstellungen = self._dock("Einstellungen", self.reiter, "dock_einstellungen")
        self.splitDockWidget(self.dock_bedienfeld, self.dock_einstellungen, Qt.Vertical)
        self.beleg = BelegKasten(self)
        self.dock_beleg = self._dock("Vorsteckbeleg", self.beleg, "dock_beleg")
        self.splitDockWidget(self.dock_bedienfeld, self.dock_beleg, Qt.Vertical)

        # ── Statuszeile ─────────────────────────────────────────────────────
        sb = self.statusBar()
        self.st_typ = QLabel()
        self.st_zustand = QLabel()
        self.st_schnittstelle = QLabel()
        self.st_server = SeriellFeld()
        self.st_verbindung = SeriellFeld()
        self.st_kopf = QLabel()
        for w in (self.st_typ, self.st_zustand, self.st_schnittstelle, self.st_kopf):
            w.setMargin(2)
        sb.addWidget(self.st_typ)
        sb.addWidget(self.st_zustand)
        sb.addWidget(self.st_schnittstelle)
        sb.addWidget(self.st_server)
        sb.addWidget(self.st_verbindung)
        sb.addPermanentWidget(self.st_kopf)

        # ── Zustand aus der Konfiguration ───────────────────────────────────
        self._einstellungen_anzeigen()
        w = daten.get("window", {}) or {}
        self.resize(int(w.get("width", self.DEFAULT_WIDTH)), int(w.get("height", self.DEFAULT_HEIGHT)))
        if isinstance(w.get("dock_state"), str):
            try:
                self.restoreState(QByteArray(base64.b64decode(w["dock_state"])))
            except Exception:                                     # noqa: BLE001 - kaputter Zustand: Vorgabe
                pass
        self.act_einpassen.setChecked(bool(dr.get("einpassen", True)))
        self.act_folgen.setChecked(bool(dr.get("folgen", True)))
        self.dock_beleg.setVisible(self.beschreibung.hat_vorsteck)   # restoreState kennt den Typ nicht
        if not dr.get("einpassen", True) and dr.get("zoom"):
            self.blatt.set_zoom(float(dr["zoom"]))
        self.seriell.zustand_anwenden(daten.get("schnittstellen", {}) or {})
        self._laden = False

        # ── Takt ────────────────────────────────────────────────────────────
        self.takt = Emulationstakt(lambda: self.emu, self.beschreibung.nenntakt_hz, bild_ms=30)
        self.takt.set_tempo(self.beschreibung.nenntakt_hz, self._tempo)
        self.takt.bild.connect(self._bild)
        self.takt.fehler.connect(self._lauffehler)
        self.aktualisieren()
        if autostart:
            self.takt.start()

    # ── Aufbau ───────────────────────────────────────────────────────────────

    def _dock(self, titel: str, widget: QWidget, name: str) -> QDockWidget:
        d = QDockWidget(titel, self)
        d.setObjectName(name)
        rolle = QScrollArea()                       # zu wenig Hoehe: rollen statt Knoepfe zusammenquetschen
        rolle.setWidgetResizable(True)
        rolle.setFrameShape(QFrame.NoFrame)
        rolle.setWidget(widget)
        d.setWidget(rolle)
        d.setFeatures(QDockWidget.DockWidgetMovable | QDockWidget.DockWidgetFloatable)
        d.setFocusPolicy(Qt.NoFocus)
        self.addDockWidget(Qt.RightDockWidgetArea, d)
        return d

    def _menues_bauen(self) -> None:
        mb = self.menuBar()
        m = mb.addMenu("&Datei")
        for n in ("pdf", "drucken", None, "beenden"):
            m.addSeparator() if n is None else m.addAction(getattr(self, f"act_{n}"))
        m = mb.addMenu("D&rucker")
        for n in ("power", "reset", None, "online", "pv1", "pv2", "test", None, "blatt_leeren"):
            m.addSeparator() if n is None else m.addAction(getattr(self, f"act_{n}"))
        self.menu_ansicht = mb.addMenu("&Ansicht")
        for n in ("einpassen", "zoom_ein", "zoom_aus", "folgen"):
            self.menu_ansicht.addAction(getattr(self, f"act_{n}"))
        m = mb.addMenu("&Hilfe")
        m.addAction(self.act_hilfe)
        m.addAction(self.act_ueber)

    def _leiste_bauen(self) -> None:
        self.leiste = QToolBar("Werkzeuge", self)
        self.leiste.setObjectName("leiste")
        self.leiste.setIconSize(QSize(20, 20))
        self.leiste.setToolButtonStyle(Qt.ToolButtonTextUnderIcon)
        for n in aktionen.LEISTE:
            if n:
                self.leiste.addAction(getattr(self, f"act_{n}"))
            else:
                self.leiste.addSeparator()
        self.addToolBar(self.leiste)

    @property
    def _rom(self) -> str:
        """Schluessel des gewaehlten Katalogeintrags zum aktuellen Typ (ohne Katalog: der gemerkte Text oder die alte Vorgabe)."""
        typ = self.beschreibung.schluessel
        wahl = self._rom_je_typ.get(typ)
        eintrag = rom_katalog.aufloesen(wahl, typ, self._roms)
        if eintrag is not None:
            return eintrag.schluessel
        return wahl if wahl in ("repariert", "original") else "repariert"

    def _rom_eintrag(self):
        return rom_katalog.aufloesen(self._rom_je_typ.get(self.beschreibung.schluessel), self.beschreibung.schluessel, self._roms)

    def _rom_datei(self) -> Optional[str]:
        e = self._rom_eintrag()
        p = e.pfad() if e is not None else None
        return str(p) if p is not None else None

    def _einstellungen_anzeigen(self) -> None:
        e = self.einst_drucker
        e.rom_anbieten(rom_katalog.fuer_typ(self.beschreibung.schluessel, self._roms), self._rom)
        for combo, wert in ((e.typ, self.beschreibung.schluessel),
                            (e.schnittstelle, self._schnittstelle), (e.tempo, self._tempo),
                            (e.seite, self._formular_zoll)):
            combo.blockSignals(True)
            i = combo.findData(wert)
            combo.setCurrentIndex(max(0, i))
            combo.blockSignals(False)
        e.typ.currentIndexChanged.connect(self._typ_gewaehlt)
        e.rom.currentIndexChanged.connect(self._rom_gewaehlt)
        e.schnittstelle.currentIndexChanged.connect(self._schnittstelle_gewaehlt)
        e.tempo.currentIndexChanged.connect(self._tempo_gewaehlt)
        e.seite.currentIndexChanged.connect(self._seite_gewaehlt)
        self.st_typ.setText(self.beschreibung.titel)
        self.st_schnittstelle.setText(f"Schnittstelle: {dict(typen.SCHNITTSTELLEN)[self._schnittstelle_aktiv()]}")
        self._bedienfeld_neu()

    # ── Maschine ─────────────────────────────────────────────────────────────

    def _emulator_anlegen(self) -> K.K1520Emulator:
        return K.K1520Emulator(machine=self.beschreibung.kern_maschine,
                               drucker=typen.konfig_text(self._schnittstelle, self._rom, self.beschreibung,
                                                         self._betriebsart, self._dil, self._rom_datei()))

    def _schnittstelle_aktiv(self) -> str:
        """Die Schnittstelle des Geraets: beim K6316 legt der Typ das Modul fest, am SD 1157 der Schalter."""
        return self.beschreibung.schnittstelle_fest or self._schnittstelle

    def _formular_an_kern(self) -> None:
        """Formularlaenge an den Schalter des SD 1157 (BCH.5..4): die Firmware liest ihn beim Formularvorschub."""
        code = {4: 0, 6: 1, 8: 2, 12: 3}.get(self._formular_zoll, 3)
        self.emu.printer_switch_value(K.DRUCKER_SW_FORMULAR1, code)

    def _neu_anlegen(self, beschreibung=None, rom=None, schnittstelle=None) -> None:
        """Drucker mit anderen Anlageparametern neu bauen; Takt, Schnittstellen und Blatt ziehen mit."""
        online = self.emu.printer_switch_get(K.DRUCKER_SW_ONLINE) == 1
        zustand = self.seriell.zustand_lesen()
        self.takt.stop()
        self.seriell.alles_beenden()
        self.beschreibung = beschreibung or self.beschreibung
        if rom:
            self._rom_je_typ[self.beschreibung.schluessel] = rom
        self._schnittstelle = schnittstelle or self._schnittstelle
        alt, self.emu = self.emu, self._emulator_anlegen()
        del alt
        self.emu.printer_switch(K.DRUCKER_SW_ONLINE, online)
        self.blatt.leeren()
        self.beleg.ansicht.blatt.leeren()
        self.seriell.set_emulator(self.emu)
        self.seriell.zustand_anwenden(zustand)
        self.st_typ.setText(self.beschreibung.titel)
        self.st_schnittstelle.setText(f"Schnittstelle: {dict(typen.SCHNITTSTELLEN)[self._schnittstelle_aktiv()]}")
        self._formular_an_kern()
        self._bedienfeld_neu()
        self._netz_setzen(True, neu_starten=False)
        self.aktualisieren()
        self.takt.start()

    def _bedienfeld_neu(self) -> None:
        """Bedienfeld und Einstellungen zum (neuen) Typ: ein K6316 hat Drehschalter und DIL-Schalter, keine ROM-Wahl."""
        if self.bedienfeld.beschreibung is not self.beschreibung:
            alt = self.bedienfeld
            self.bedienfeld = Bedienfeld(self.beschreibung)
            self.bedienfeld.taste.connect(self._taste)
            self.bedienfeld.schalter.connect(self._schalter)
            self.bedienfeld.netz.connect(self._netz_aus_bedienfeld)
            self.bedienfeld.neustart.connect(self._neustart)
            self.dock_bedienfeld.setWidget(self.bedienfeld)
            alt.deleteLater()
        e = self.einst_drucker
        e.rom_anbieten(rom_katalog.fuer_typ(self.beschreibung.schluessel, self._roms), self._rom)
        for w in (e.schnittstelle, e.schnittstelle_text):
            w.setEnabled(self.beschreibung.hat_rom_wahl)
        if hasattr(self, "dock_beleg"):
            self.dock_beleg.setVisible(self.beschreibung.hat_vorsteck)
        for n in ("online", "pv1", "pv2", "test"):
            a = getattr(self, f"act_{n}", None)
            if a is not None:
                a.setEnabled(self.beschreibung.hat_online_schalter)

    def _frage(self, titel: str, text: str) -> bool:
        return QMessageBox.question(self, titel, text, QMessageBox.Yes | QMessageBox.No,
                                    QMessageBox.No) == QMessageBox.Yes

    _NEUANLAGE = ("Der Drucker wird dafür neu angelegt.  Das Blatt wird dabei geleert; "
                  "speichern Sie es vorher ggf. als PDF.\n\nFortfahren?")

    def _typ_gewaehlt(self, index: int) -> None:
        e = self.einst_drucker
        schluessel = e.typ.itemData(index)
        grund = e.sperrgrund(schluessel)
        if grund or schluessel == self.beschreibung.schluessel:
            e.typ.blockSignals(True)
            e.typ.setCurrentIndex(max(0, e.typ.findData(self.beschreibung.schluessel)))
            e.typ.blockSignals(False)
            return
        if self._frage("Druckertyp wechseln", self._NEUANLAGE):
            self._neu_anlegen(beschreibung=typen.typ(schluessel))
        else:
            e.typ.blockSignals(True)
            e.typ.setCurrentIndex(max(0, e.typ.findData(self.beschreibung.schluessel)))
            e.typ.blockSignals(False)

    def _rom_gewaehlt(self, _=None) -> None:
        e = self.einst_drucker
        neu = e.rom.currentData()
        if self._laden or neu == self._rom:
            return
        if self._frage("ROM-Fassung wechseln", self._NEUANLAGE):
            self._neu_anlegen(rom=neu)
        else:
            e.rom.blockSignals(True)
            e.rom.setCurrentIndex(max(0, e.rom.findData(self._rom)))
            e.rom.blockSignals(False)

    def _schnittstelle_gewaehlt(self, _=None) -> None:
        e = self.einst_drucker
        neu = e.schnittstelle.currentData()
        if self._laden or neu == self._schnittstelle:
            return
        if self._frage("Schnittstelle wechseln", self._NEUANLAGE):
            self._neu_anlegen(schnittstelle=neu)
        else:
            e.schnittstelle.blockSignals(True)
            e.schnittstelle.setCurrentIndex(max(0, e.schnittstelle.findData(self._schnittstelle)))
            e.schnittstelle.blockSignals(False)

    def _tempo_gewaehlt(self, _=None) -> None:
        self._tempo = float(self.einst_drucker.tempo.currentData())
        self.takt.set_tempo(self.beschreibung.nenntakt_hz, self._tempo)

    def _seite_gewaehlt(self, _=None) -> None:
        self._formular_zoll = int(self.einst_drucker.seite.currentData())
        self.blatt.set_formular_zeilen(self._formular_zeilen())
        self._formular_an_kern()

    def _formular_zeilen(self) -> int:
        ydpi = (self.emu.printer_geometry() or (200, 144, 1))[1]
        return int(round(self._formular_zoll * ydpi))

    # ── Netz, Neustart, Bedienung ────────────────────────────────────────────

    def _netz_setzen(self, an: bool, neu_starten: bool = True) -> None:
        """Netz ein/aus.  Aus: Takt steht, Lampen dunkel, Blatt bleibt.  Ein: /RESET und weiter."""
        self._netz = bool(an)
        for a, w in ((self.act_power, an),):
            a.blockSignals(True)
            a.setChecked(w)
            a.blockSignals(False)
        self.act_power.setText("&Netz aus" if an else "&Netz ein")
        self.bedienfeld.set_netz(an)
        if an:
            if neu_starten:
                self.takt.stop()
                self.emu.reset()
            self.takt.start()
        else:
            self.takt.stop()
        self.aktualisieren()

    def _netz_aktion(self, an: bool) -> None:
        self._netz_setzen(an)

    def _netz_aus_bedienfeld(self, an: bool) -> None:
        self._netz_setzen(an)

    def _neustart(self) -> None:
        self._netz_setzen(True)

    def _taste(self, kern_id: int, gedrueckt: bool) -> None:
        if self._netz:
            self.emu.printer_key(kern_id, gedrueckt)

    def _schalter(self, kern_id: int, an) -> None:
        if not self._netz:
            return
        if kern_id == K.DRUCKER_SW_BETRIEBSART:
            self.emu.printer_switch_value(kern_id, int(an))
            self._betriebsart = ("aufwickeln", "schneiden", "endlos")[int(an)]
            return
        an = bool(an)
        self.emu.printer_switch(kern_id, an)
        if K.DRUCKER_SW_DIL_BASIS <= kern_id < K.DRUCKER_SW_DIL_BASIS + 16:
            bit = 1 << (kern_id - K.DRUCKER_SW_DIL_BASIS)
            self._dil = (self._dil | bit) if an else (self._dil & ~bit)
        if kern_id == K.DRUCKER_SW_ONLINE:
            self._online_spiegeln(an)

    def _online_spiegeln(self, an: bool) -> None:
        self.act_online.blockSignals(True)
        self.act_online.setChecked(an)
        self.act_online.blockSignals(False)

    def _online_aktion(self, an: bool) -> None:
        if not self._netz:
            return
        self.emu.printer_switch(K.DRUCKER_SW_ONLINE, an)
        self.bedienfeld.aktualisieren(self.emu)

    def _tippen(self, taste: int) -> None:
        if not self._netz:
            return
        self.emu.printer_key(taste, True)

        def los():
            if self.emu is not None:
                self.emu.printer_key(taste, False)
        QTimer.singleShot(TIPP_MS[taste], los)

    def _pv1_tippen(self) -> None:
        self._tippen(K.DRUCKER_TASTE_PV1)

    def _pv2_tippen(self) -> None:
        self._tippen(K.DRUCKER_TASTE_PV2)

    def _test_tippen(self) -> None:
        self._tippen(K.DRUCKER_TASTE_TEST)

    def _blatt_leeren(self) -> None:
        self.emu.printer_sheet_clear()
        self.blatt.leeren()
        self.aktualisieren()

    # ── Ansicht ──────────────────────────────────────────────────────────────

    def _einpassen_aktion(self, an: bool) -> None:
        self.blatt.set_einpassen(an)

    def _zoom_plus(self) -> None:
        self.blatt.set_zoom(self.blatt.zoom() * 1.25)
        self.act_einpassen.blockSignals(True)
        self.act_einpassen.setChecked(False)
        self.act_einpassen.blockSignals(False)

    def _zoom_minus(self) -> None:
        self.blatt.set_zoom(self.blatt.zoom() / 1.25)
        self.act_einpassen.blockSignals(True)
        self.act_einpassen.setChecked(False)
        self.act_einpassen.blockSignals(False)

    def _folgen_aktion(self, an: bool) -> None:
        self.blatt.set_folgen(an)

    # ── Takt: Oberflaeche nachziehen ─────────────────────────────────────────

    def _bild(self, _n: int = 0) -> None:
        self.aktualisieren()

    def aktualisieren(self) -> None:
        """Blatt, Lampen und Statuszeile aus dem Kern nachziehen (auch von Tests gerufen)."""
        if self.emu is None:
            return
        self.blatt.aktualisieren(self.emu)
        self.bedienfeld.aktualisieren(self.emu)
        if self.beschreibung.hat_vorsteck:
            self.beleg.aktualisieren(self.emu)
        self._online_spiegeln(self.emu.printer_switch_get(K.DRUCKER_SW_ONLINE) == 1)
        self.act_online.setEnabled(self._netz and self.beschreibung.hat_online_schalter)
        if not self._netz:
            zustand = "Netz aus"
        else:
            zustand = "online" if self.emu.printer_lamp(K.DRUCKER_LAMPE_ONLINE) == 1 else "offline"
            if self.emu.printer_lamp(K.DRUCKER_LAMPE_FEHLER) == 1:
                zustand += " - FEHLER"
        self.st_zustand.setText(zustand)
        groesse = self.emu.printer_sheet_size()
        if groesse is not None:
            x, y = self.blatt.raster()
            info = (f"{groesse[0]} × {groesse[1]} Punkte ({groesse[0] / x:.1f} × {groesse[1] / y:.1f} Zoll), "
                    f"Raster {x} × {y} je Zoll")
            if info != self.einst_drucker.info.text():
                self.einst_drucker.info.setText(info)

    def _kopf_anzeigen(self, x: int, y: int, lauf: int) -> None:
        xdpi = self.blatt.raster()[0]
        zoll = x / float(xdpi)
        self.st_kopf.setText(f"Kopf x = {zoll:.2f} Zoll (Spalte {int(zoll * 10) + 1} bei 10 cpi)"
                             f"{'  - fährt' if lauf else ''}")

    def _seriell_status(self, server: str, server_tipp: str, verbindung: str, verbindung_tipp: str) -> None:
        self.st_server.zeige(server, server_tipp)
        self.st_verbindung.zeige(verbindung, verbindung_tipp)

    def _lauffehler(self, text: str) -> None:
        self.statusBar().showMessage(f"Lauf angehalten: {text}", 10000)

    # ── Ausgabe ──────────────────────────────────────────────────────────────

    def _pdf_speichern(self, pfad: Optional[str] = None) -> bool:
        if pfad is None:
            pfad, _ = QFileDialog.getSaveFileName(self, "Blatt als PDF speichern", "blatt.pdf", "PDF (*.pdf)")
        if not pfad:
            return False
        if not pfad.lower().endswith(".pdf"):
            pfad += ".pdf"
        n = ausgabe.pdf_speichern(pfad, self.blatt.kopie(), self._formular_zoll)
        self.statusBar().showMessage(f"{n} Punkte nach {pfad} geschrieben", 8000)
        return True

    def _drucken(self) -> bool:
        printer = QPrinter(QPrinter.HighResolution)
        dlg = self._druckdialog(printer, self)
        if dlg.exec() != QPrintDialog.Accepted:
            return False
        n = ausgabe.drucken(printer, self.blatt.kopie(), self._formular_zoll)
        self.statusBar().showMessage(f"{n} Punkte gedruckt", 8000)
        return True

    # ── Hilfe ────────────────────────────────────────────────────────────────

    def open_help(self) -> HelpWindow:
        if self._hilfe is not None and self._hilfe.isVisible():
            self._hilfe.raise_()
            self._hilfe.activateWindow()
            return self._hilfe
        self._hilfe = HelpWindow(self, "K1520 Printer - Handbuch", HANDBUCH)
        self._hilfe.show()
        return self._hilfe

    def _ueber_dialog(self) -> None:
        try:
            lib = K.K1520Emulator.version()
        except Exception as e:                                   # noqa: BLE001
            lib = f"nicht geladen ({e})"
        QMessageBox.about(self, "Über K1520 Printer",
                          f"<h3>K1520 Printer</h3><p>Hardware-Emulator der Nadeldrucker der K1520-Familie: "
                          f"die Original-Firmware läuft, das Blatt entsteht Punkt für Punkt.</p>"
                          f"<p>Version: {_version.fassung()}<br>Kern: {lib}</p>")

    # ── Konfiguration, Schliessen ────────────────────────────────────────────

    def konfig_lesen(self) -> dict:
        return {
            "general": {"model": self.beschreibung.schluessel, "speed": self._tempo},
            "printer": {"rom": {**self._rom_je_typ, self.beschreibung.schluessel: self._rom},
                        "schnittstelle": self._schnittstelle,
                        "betriebsart": self._betriebsart, "dil": self._dil,
                        "online": self.emu.printer_switch_get(K.DRUCKER_SW_ONLINE) == 1,
                        "formular_zoll": self._formular_zoll, "zoom": round(self.blatt.zoom(), 4),
                        "einpassen": self.blatt.einpassen(), "folgen": self.blatt.folgen()},
            "schnittstellen": self.seriell.zustand_lesen(),
            "window": {"width": self.width(), "height": self.height(),
                       "dock_state": base64.b64encode(bytes(self.saveState())).decode("ascii")},
        }

    def closeEvent(self, ev) -> None:
        try:
            konfig.speichern(self.konfig_lesen(), self._konfig_pfad)
        except Exception as e:                                   # noqa: BLE001 - Schliessen darf nicht scheitern
            print(f"[config] nicht gespeichert: {e}")
        self.takt.beenden()
        self.seriell.beenden()
        super().closeEvent(ev)

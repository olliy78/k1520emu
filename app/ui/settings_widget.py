"""
K1520 Emulator - Settings Widget
================================

Dockable settings panel with tabbed categories:

* **Allgemein** — general emulator settings (model, emulation speed dropdown,
  RAM-Disk RAF + Stand-by, doc/design/22_raf512.md §7.1; Lochstreifen K6022,
  doc/design/23_lochstreifen.md §7).
* **Schnittstellen** — die seriellen Schnittstellen nach außen (AP-S10; das
  Widget kommt vom Hauptfenster, damit sein Takt auch ohne sichtbaren Reiter läuft).
* **CRT** — every :class:`~app.ui.screen_widget.CRTParams` field as a live
  control (slider + spin box, or colour picker), so the picture-tube look can be
  dialled in interactively.

Every change is applied to the live emulator/``ScreenWidget.params`` and emits a
signal (:attr:`crtChanged` / :attr:`speedChanged`) so the main window can persist
the configuration automatically.
"""

from typing import Callable, List

from PySide6.QtWidgets import (
    QWidget, QTabWidget, QVBoxLayout, QHBoxLayout, QFormLayout, QScrollArea,
    QSlider, QDoubleSpinBox, QComboBox, QCheckBox, QPushButton, QLabel, QColorDialog, QFrame,
    QLineEdit, QToolButton, QFileDialog, QGroupBox,
)
from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QColor

from app.ui.screen_widget import CRTParams
from app.ui.bildschirm_schalter import BildschirmSchalter
from app import drive_types as dt
from app import profil as profile
from app import takt
from app import raf
from app import k3521
from app import p8000_ram
from app import rtc
from app.ui_icons import icon


class SettingsWidget(QWidget):
    """Tabbed settings panel: general (speed) + CRT (picture-tube) parameters."""

    # Emitted after any CRT parameter change (colour/slider/reset).
    crtChanged = Signal()
    # Emitted when the speed dropdown selection changes; carries the factor
    # (1.0 = real time, >1.0 = fast-forward).
    speedChanged = Signal(float)
    # Emitted when a drive-type dropdown changes; carries the list of 4 core
    # DriveProfile names (one per K5122 slot, "none" = empty slot).
    driveTypesChanged = Signal(list)
    #: Voreinstellung P8000 / P8000 Compact gewaehlt (Schluessel; auch beim erneuten Waehlen).
    bauartChanged = Signal(str)
    #: Zahl der Winchester-Laufwerke (1–3).
    winchesterChanged = Signal(int)
    # Emitted when the model dropdown changes; carries the model key
    # (app.modell.A5120 / app.modell.A5120_16).
    modelChanged = Signal(str)
    # Hardwarevariante (Profil.hardware) geaendert: Schluessel, Wert.
    hardwareChanged = Signal(str, str)
    # Schiebeschalter der Bildschirmkarten umgelegt (``"k7024"``/``"k7023"``), wirkt sofort.
    bildschirmSchalterChanged = Signal(str)
    # RAM-Disk-Auswahl geaendert (``"none"``/``"raf128"``/``"raf512"``/``"raf2m"``,
    # `app/raf.py`) — wie der Modellwechsel ein Neuaufbau der Maschine.
    rafChanged = Signal(str)
    # Stand-by-Kaestchen der RAF umgeschaltet (kein Neuaufbau).
    rafStandbyChanged = Signal(bool)
    # Kaestchen „Lochstreifen (SIF1000, K6022)" umgeschaltet — Neuaufbau der Maschine
    # wie bei der RAF (doc/design/23_lochstreifen.md §7).
    ptapeChanged = Signal(bool)
    # Kaestchen „Kassette (K5221, AKB K5020)" umgeschaltet — Neuaufbau wie beim Lochstreifen
    # (doc/design/29_kassette.md).
    tapeChanged = Signal(bool)
    # Option „AUP (Sparkasse)" geaendert: (an, mit Sonderschnittstelle, Alpha-Annahme
    # ``led``/``ohne``/``mit``) — Neuaufbau der Maschine (Entwurf 33 N10e).
    aupChanged = Signal(bool, bool, str)
    # Option „K3521 (batteriegepuffertes RAM)" geaendert: (Anzahl 0..4, Startadresse) —
    # Neuaufbau der Maschine (Entwurf 33 N10h).  Nur gueltige Werte werden gemeldet.
    k3521Changed = Signal(int, int)
    # Mitschrift des Terminals: Dateiname geaendert (leer = aus).  Nur Profile mit Terminal.
    mitschriftChanged = Signal(str)
    # Schaltflaeche „Uhr nach Wirtszeit stellen“ der Gruppe RTC (nur P8000).
    rtcStellenGewuenscht = Signal()

    #: (Beschriftung, Faktor).  Die Stufen
    #: stehen in :mod:`app.takt`, damit Auswahlfeld und Statuszeile dasselbe
    #: sagen: der Nenntakt des A5120 (2,45 MHz) und seine Vielfachen.  Das
    #: Profil des K8915 bekommt seine eigene Liste (2,4576 MHz), siehe __init__.
    SPEED_OPTIONS = takt.auswahl()

    def __init__(self, screen_widget, parent=None, profil=None, schnittstellen=None):
        super().__init__(parent)
        self.screen = screen_widget
        self.profil = profil or profile.VORGABE
        self.SPEED_OPTIONS = takt.auswahl(self.profil.nenntakt_text)

        # Refreshers re-read control values from params (used by "Reset").
        self._refreshers: List[Callable[[], None]] = []
        # Guards the speed combo against emitting while set programmatically.
        self._speed_guard = False
        # Guards the drive-type combos against emitting while set programmatically.
        self._drive_guard = False
        self._drive_combos: List[QComboBox] = []
        # Guards the model combo against emitting while set programmatically.
        self._model_guard = False
        # Dito fuer RAM-Disk-Auswahl und Stand-by-Kaestchen.
        self._raf_guard = False
        self._ptape_guard = False
        self._tape_guard = False
        self._aup_guard = False
        self._k3521_guard = False
        self._k3521_stand = (0, k3521.START_VORGABE)   # zuletzt uebernommener, gueltiger Wert
        self._mitschrift_guard = False

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        self.tabs = QTabWidget()
        self.tabs.addTab(self._build_general_tab(), "Allgemein")
        self._drives_tab = self._build_drives_tab()
        self.tabs.addTab(self._drives_tab, "Laufwerke")
        self.schnittstellen = schnittstellen
        if schnittstellen is not None:
            rolle = QScrollArea()
            rolle.setWidgetResizable(True)
            rolle.setFrameShape(QFrame.NoFrame)
            rolle.setWidget(schnittstellen)
            self.tabs.addTab(rolle, "Schnittstellen")
        self.tabs.addTab(self._build_crt_tab(), "CRT")
        layout.addWidget(self.tabs)
        self._modus_anpassen()

    def zeige_schnittstellen(self):
        """Den Reiter „Schnittstellen“ nach vorn holen (für Tests und Aufrufer)."""
        if self.schnittstellen is not None:
            self.tabs.setCurrentIndex(self.tabs.indexOf(self.schnittstellen.parentWidget()
                                                        .parentWidget()))

    # ── helpers ──────────────────────────────────────────────────────────────

    def _apply(self):
        """Push params to the screen, force a repaint, and notify listeners."""
        self.screen.parameter_geaendert()
        self._crt_freigeben()
        self.crtChanged.emit()

    def _add_float(self, form: QFormLayout, label: str, minv: float, maxv: float,
                   step: float, getter: Callable[[], float],
                   setter: Callable[[float], None]):
        """A labelled slider + spin box bound to one float parameter."""
        decimals = max(0, len(str(step).split(".")[-1])) if "." in str(step) else 0

        slider = QSlider(Qt.Horizontal)
        slider.setRange(0, 1000)
        spin = QDoubleSpinBox()
        spin.setRange(minv, maxv)
        spin.setSingleStep(step)
        spin.setDecimals(decimals if decimals else 2)

        guard = {"busy": False}

        def to_slider(v):
            if maxv == minv:
                return 0
            return int(round((v - minv) / (maxv - minv) * 1000))

        def from_slider(i):
            return minv + (i / 1000.0) * (maxv - minv)

        def on_spin(v):
            if guard["busy"]:
                return
            guard["busy"] = True
            slider.setValue(to_slider(v))
            guard["busy"] = False
            setter(float(v))
            self._apply()

        def on_slider(i):
            if guard["busy"]:
                return
            guard["busy"] = True
            spin.setValue(from_slider(i))
            guard["busy"] = False
            setter(float(spin.value()))
            self._apply()

        def refresh():
            guard["busy"] = True
            v = getter()
            spin.setValue(v)
            slider.setValue(to_slider(v))
            guard["busy"] = False

        spin.valueChanged.connect(on_spin)
        slider.valueChanged.connect(on_slider)
        refresh()
        self._refreshers.append(refresh)

        row = QWidget()
        rl = QHBoxLayout(row)
        rl.setContentsMargins(0, 0, 0, 0)
        rl.addWidget(slider, 1)
        rl.addWidget(spin)
        form.addRow(label, row)

    def _add_stufe(self, form: QFormLayout, label: str, attr: str):
        """Checkbox, die eine Filterstufe (``CRTParams.<attr>``) ein-/ausschaltet."""
        box = QCheckBox(label)

        def refresh():
            box.blockSignals(True)
            box.setChecked(bool(getattr(self.screen.params, attr)))
            box.blockSignals(False)

        def on_toggled(on):
            setattr(self.screen.params, attr, bool(on))
            self._apply()

        box.toggled.connect(on_toggled)
        refresh()
        self._refreshers.append(refresh)
        form.addRow(box)

    def _add_color(self, form: QFormLayout, label: str,
                   getter: Callable[[], tuple],
                   setter: Callable[[tuple], None]):
        """A colour-swatch button + hex value bound to one RGB (0..1) parameter."""
        btn = QPushButton()
        btn.setFixedWidth(80)
        hexlbl = QLabel()

        def swatch():
            r, g, b = getter()
            ri, gi, bi = int(r * 255), int(g * 255), int(b * 255)
            btn.setStyleSheet(f"background-color: rgb({ri},{gi},{bi});")
            hexlbl.setText(f"#{ri:02X}{gi:02X}{bi:02X}")

        def pick():
            r, g, b = getter()
            initial = QColor(int(r * 255), int(g * 255), int(b * 255))
            c = QColorDialog.getColor(initial, self, label)
            if c.isValid():
                setter((c.redF(), c.greenF(), c.blueF()))
                swatch()
                self._apply()

        btn.clicked.connect(pick)
        swatch()
        self._refreshers.append(swatch)

        row = QWidget()
        rl = QHBoxLayout(row)
        rl.setContentsMargins(0, 0, 0, 0)
        rl.addWidget(btn)
        rl.addWidget(hexlbl)
        rl.addStretch(1)
        form.addRow(label, row)

    # ── Allgemein tab ────────────────────────────────────────────────────────

    def _build_general_tab(self) -> QWidget:
        inner = QWidget()
        form = QFormLayout(inner)

        self.model_combo = QComboBox(inner)
        # Mit Betriebsart (P8000): das Feld zeigt nur die Modelle MIT Rechner („Rechnerausstattung“),
        # das Modell „nur Terminal“ kommt ueber das zweite Feld.
        zeilen = (self.profil.ausstattungen() if self.profil.betriebsart_wahl
                  else self.profil.modelle)
        for schluessel, _maschine, _em, beschriftung, _tastatur in zeilen:
            self.model_combo.addItem(beschriftung, schluessel)
        # Modelle, die der Kern noch nicht faehrt: sichtbar, aber nicht waehlbar.
        for schluessel, beschriftung, grund in self.profil.gesperrte_modelle:
            self.model_combo.addItem(beschriftung, schluessel)
            i = self.model_combo.count() - 1
            self.model_combo.model().item(i).setEnabled(False)
            self.model_combo.setItemData(i, grund, Qt.ToolTipRole)
        if self.profil.modell_vorgaben:
            # Modell = Vorgaben (Entwurf 33 §3a): auch das ERNEUTE Waehlen desselben Eintrags muss
            # sie uebernehmen, und dort schweigt currentIndexChanged — daher `activated`
            # (kommt bei jeder Bedienung, nicht bei set_model_value()).
            self.model_combo.currentIndexChanged.connect(self._on_model_index)
            self.model_combo.activated.connect(self._on_model_combo)
        else:
            self.model_combo.currentIndexChanged.connect(self._on_model_combo)
        self.model_combo.setToolTip(self.profil.modell_tipp)
        # Betriebsart (P8000): Computer mit Terminal | nur Terminal
        self.betriebsart_combo = QComboBox(inner)
        self.betriebsart_combo.addItem("Computer mit Terminal", "computer")
        self.betriebsart_combo.addItem("nur Terminal", "terminal")
        self.betriebsart_combo.setToolTip(
            "Computer mit Terminal: der Rechner samt seinem Terminal in einem Fenster.  "
            "Nur Terminal: der Arbeitsplatz ohne Rechner — er wird über die serielle Leitung "
            "(Maschine ▸ Verbindung zum Rechner…) mit dem Rechner eines anderen P8000-Emulators "
            "verbunden.  Ein Wechsel schaltet die Maschine aus und neu ein.")
        self.betriebsart_combo.currentIndexChanged.connect(self._on_model_combo)
        # Voreinstellung (P8000 | P8000 Compact): ein Vorgabe-Satz wie die Modellwahl des A5120.
        # `activated` kommt nur bei Bedienung, auch beim erneuten Waehlen desselben Eintrags.
        self.bauart_combo = QComboBox(inner)
        for schluessel, beschriftung, _titel, _vorgaben in self.profil.bauarten:
            self.bauart_combo.addItem(beschriftung, schluessel)
        self.bauart_combo.setToolTip(
            "Voreinstellung: übernimmt die Bestückung des Geräts — MON8/MON16-Fassung, Hauptspeicher, "
            "RTC, Zahl der Diskettenlaufwerke (Einstellungen ▸ Laufwerke) und der Winchester.  "
            "P8000: MON 3.0, 4 × 256 KB, 2 Disketten.  P8000 Compact: MON 3.1, 4 × 1 MB, 3 Disketten.  "
            "Danach ist jede Option einzeln änderbar.  Die Wahl baut die Maschine neu auf (Kaltstart).")
        self.bauart_combo.activated.connect(
            lambda _i: self.bauartChanged.emit(self.bauart_combo.currentData()))
        if self.profil.bauarten and self.profil.modellwahl:
            form.addRow("Modell:", self.bauart_combo)
        else:
            self.bauart_combo.setVisible(False)
        # Nur im Programm mit Modellwahl; model_value() liefert sonst die Vorgabe.
        if self.profil.modellwahl:
            form.addRow("Rechnerausstattung:" if self.profil.betriebsart_wahl else "Modell:",
                        self.model_combo)
            if self.profil.betriebsart_wahl:
                form.addRow("Betriebsart:", self.betriebsart_combo)
        else:
            self.model_combo.setVisible(False)
        if not self.profil.betriebsart_wahl:
            self.betriebsart_combo.setVisible(False)

        # Hardwarevarianten des Programms (Profil.hardware, z. B. PC 1715: Zeichensatz, Tastatur)
        self.hardware_combos = {}
        self.bildschirm_schalter = None        # nur mit dem Hardwarefeld „bildschirm“
        for schluessel, beschriftung, tipp, werte, _modelle in self.profil.hardware:
            box = QComboBox(inner)
            for wert, anzeige in werte:
                box.addItem(anzeige, wert)
            box.setToolTip(tipp)
            box.currentIndexChanged.connect(
                lambda _i, s=schluessel: self._on_hardware_combo(s))
            self.hardware_combos[schluessel] = box
            if self.profil.rtc_wahl and schluessel in (rtc.SCHLUESSEL_TYP, rtc.SCHLUESSEL_VERSATZ):
                continue                       # steht in der Gruppe „RTC“ (unten)
            form.addRow(beschriftung, box)
            if schluessel == "bildschirm":
                # Entwurf 33 §3.3: der Schiebeschalter sitzt unter der Kartenwahl.
                self.bildschirm_schalter = BildschirmSchalter(inner)
                self.bildschirm_schalter.stellungGeaendert.connect(
                    self.bildschirmSchalterChanged)
                form.addRow("", self.bildschirm_schalter)
        self._hardware_guard = False
        # Gruppe „RTC“ (Entwurf 38 R3): Typ und Zeitversatz nebeneinander, darunter die Uhrzeit des
        # Gastsystems und die Schaltflaeche zum Stellen.  Die Felder sind die Hardwarefelder
        # „rtc“/„rtc_versatz“ (Speichern, Vorgabe und Neubau der Maschine laufen wie bei den anderen).
        self.rtc_gruppe = None
        if self.profil.rtc_wahl:
            self.rtc_gruppe = QGroupBox("RTC", inner)
            gv = QVBoxLayout(self.rtc_gruppe)
            zeile = QHBoxLayout()
            zeile.addWidget(self.hardware_combos[rtc.SCHLUESSEL_TYP], 1)
            zeile.addWidget(self.hardware_combos[rtc.SCHLUESSEL_VERSATZ], 1)
            gv.addLayout(zeile)
            self.rtc_zeit_label = QLabel("unbekannt", self.rtc_gruppe)
            self.rtc_zeit_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
            fz = QFormLayout()
            fz.addRow("Uhrzeit des Gastsystems:", self.rtc_zeit_label)
            gv.addLayout(fz)
            self.rtc_stellen_button = QPushButton("Uhr nach Wirtszeit stellen", self.rtc_gruppe)
            self.rtc_stellen_button.setToolTip(
                "Stellt die Uhr der Karte auf die jetzige Zeit des Wirtsrechners (der gewählte "
                "Zeitversatz wirkt wie beim Lesen).  Liegt die neue Zeit vor der bisherigen, "
                "wird vorher gefragt.")
            self.rtc_stellen_button.clicked.connect(lambda _c=False: self.rtcStellenGewuenscht.emit())
            gv.addWidget(self.rtc_stellen_button, 0, Qt.AlignLeft)
            form.addRow(self.rtc_gruppe)
        self._hardware_gesperrt_nach_modell()

        self.speed_combo = QComboBox()
        for label, factor in self.SPEED_OPTIONS:
            self.speed_combo.addItem(label, float(factor))
        self.speed_combo.currentIndexChanged.connect(self._on_speed_combo)
        self.set_nenntakt(self.profil.nenntakt_text)
        form.addRow("Takt:", self.speed_combo)

        # RAM-Floppy RAF (doc/design/22_raf512.md §7.1) — in jedem Programm, dessen
        # Profil sie anbietet.  Ein Wechsel erzeugt die Maschine neu (rafChanged).
        self.raf_combo = QComboBox(inner)
        for schluessel, beschriftung in raf.TYPEN:
            self.raf_combo.addItem(beschriftung, schluessel)
        self.raf_combo.setToolTip(raf.TIPP)
        self.raf_combo.currentIndexChanged.connect(self._on_raf_combo)
        self.raf_standby_box = QCheckBox("Inhalt beim Beenden behalten (Stand-by 5PG)", inner)
        self.raf_standby_box.setToolTip(raf.STANDBY_TIPP)
        self.raf_standby_box.toggled.connect(self._on_raf_standby_box)
        self.raf_standby_box.setEnabled(False)          # erst mit gewaehlter RAF
        if self.profil.raf_wahl:
            form.addRow("RAM-Disk:", self.raf_combo)
            form.addRow("", self.raf_standby_box)
        else:
            self.raf_combo.setVisible(False)
            self.raf_standby_box.setVisible(False)

        # Lochstreifen: ADA K6022 mit Leser daro 1210 und Stanzer daro 1215 (SIF1000) —
        # in allen Programmen eine Option, Vorgabe aus (Anwenderentscheid E3).
        self.ptape_box = QCheckBox("Lochstreifen (SIF1000, K6022)", inner)
        self.ptape_box.setToolTip(
            "Steckt die ADA K6022 mit Lochstreifenleser und -stanzer (E/A E0H–E7H).  "
            "Bedient werden beide im Kasten „Lochstreifen“.  Ein Wechsel schaltet die "
            "Maschine aus und neu ein.")
        self.ptape_box.toggled.connect(self._on_ptape_box)
        if self.profil.ptape_wahl:
            form.addRow("Peripherie:", self.ptape_box)
        else:
            self.ptape_box.setVisible(False)       # P8000: gibt es die Karte nicht

        # Kassette: AKB K5020 mit zwei Laufwerken K5200 — wie der Lochstreifen eine
        # Option, Vorgabe aus (doc/design/29_kassette.md).
        self.tape_box = QCheckBox("Kassette (K5221, AKB K5020)", inner)
        self.tape_box.setToolTip(
            "Steckt den Anschlussbaustein K5020 mit zwei Kassettenlaufwerken K5200 "
            "(A5120 und PRG 710: E/A 30H–37H, K8915 und PC 1715: D0H–D7H).  Bedient "
            "werden sie im Kasten „Kassette“.  Ein Wechsel schaltet die Maschine aus "
            "und neu ein.")
        self.tape_box.toggled.connect(self._on_tape_box)
        if self.profil.tape_wahl:
            form.addRow("", self.tape_box)
        else:
            self.tape_box.setVisible(False)        # P8000: gibt es die Karte nicht

        # AUP 062-8531 (Sparkasse): Tastaturteil mit K7633 und Kundentastatur, optional mit
        # Sonderschnittstelle.  Option, Vorgabe aus bei allen Modellen (Entwurf 33 N10e).
        self.aup_box = QCheckBox("AUP (Sparkasse)", inner)
        self.aup_box.setToolTip(
            "Steckt den Adapter Universelle Ports AUP 062-8531 (E/A 40H–43H) mit der "
            "Sparkassentastatur K7633 und der Kundentastatur.  Bedient wird sie im Kasten "
            "„AUP“.  Ein Wechsel schaltet die Maschine aus und neu ein.")
        self.aup_sonder_box = QCheckBox("mit Sonderschnittstelle (CTC B0H, SIO B4H)", inner)
        self.aup_sonder_box.setToolTip(
            "Steckt die Sonderschnittstelle der AUP mit (CTC B0H–B3H, SIO B4H–B7H); SIO-Kanal B "
            "erscheint im Reiter „Schnittstellen“ als vierter Anschluss.")
        self.aup_alpha_combo = QComboBox(inner)
        for wert, text in (("led", "folgt der ALPHA-Lampe"), ("ohne", "immer ohne Alpha"),
                           ("mit", "immer mit Alpha")):
            self.aup_alpha_combo.addItem(text, wert)
        self.aup_alpha_combo.setToolTip(
            "Welche Codetabelle der K7633 gilt (Annahme: der Beleg sagt nicht, wer zwischen "
            "„ohne“ und „mit Alpha“ umschaltet).")
        self.aup_box.toggled.connect(self._on_aup_box)
        self.aup_sonder_box.toggled.connect(self._on_aup_box)
        self.aup_alpha_combo.currentIndexChanged.connect(self._on_aup_box)
        if self.profil.aup_wahl:
            form.addRow("", self.aup_box)
            form.addRow("", self.aup_sonder_box)
            form.addRow("Alpha-Tabelle:", self.aup_alpha_combo)
        else:
            for w in (self.aup_box, self.aup_sonder_box, self.aup_alpha_combo):
                w.setVisible(False)
        self._aup_abhaengig()

        # K3521.30: bis zu vier Karten (je 4 KB, batteriegepuffert) lueckenlos ab einer Startadresse
        # (Entwurf 33 N10h).  Option, Vorgabe „keine" bei allen Modellen.
        self.k3521_combo = QComboBox(inner)
        for text, wert in (("keine", 0), ("1", 1), ("2", 2), ("3", 3), ("4", 4)):
            self.k3521_combo.addItem(text, wert)
        self.k3521_combo.setToolTip(
            "Anzahl der Karten K3521.30 (je 4 KB CMOS-RAM mit Akkupufferung).  Der Inhalt bleibt "
            "über Reset, Aus- und Einschalten und – in einer Datei je Karte – über das Ende des "
            "Programms.  Ein Wechsel schaltet die Maschine aus und neu ein.")
        self.k3521_start_edit = QLineEdit(inner)
        self.k3521_start_edit.setText(f"{k3521.START_VORGABE:04X}")
        self.k3521_start_edit.setMaximumWidth(70)
        self.k3521_start_edit.setToolTip(
            "Startadresse der ersten Karte (Hex, im 4-KB-Raster 0000–F000); weitere Karten folgen "
            "lückenlos im Abstand 1000H.  Vorgabe 8000: der CP/A-Kaltstart lässt 7000H–AFFFH in Ruhe.")
        self.k3521_hinweis = QLabel("", inner)
        self.k3521_hinweis.setStyleSheet("color: #c00;")
        self.k3521_hinweis.setWordWrap(True)
        self.k3521_hinweis.setVisible(False)
        self.k3521_combo.currentIndexChanged.connect(self._on_k3521)
        self.k3521_start_edit.editingFinished.connect(self._on_k3521)
        k3521_zeile = QWidget(inner)
        zl = QHBoxLayout(k3521_zeile)
        zl.setContentsMargins(0, 0, 0, 0)
        zl.addWidget(self.k3521_combo)
        zl.addWidget(QLabel("Startadresse:", inner))
        zl.addWidget(self.k3521_start_edit)
        zl.addStretch(1)
        self._k3521_widgets = (self.k3521_combo, self.k3521_start_edit, self.k3521_hinweis, k3521_zeile)
        if self.profil.k3521_wahl:
            form.addRow("K3521 (batteriegepuffertes RAM):", k3521_zeile)
            form.addRow("", self.k3521_hinweis)
        else:
            for w in self._k3521_widgets:
                w.setVisible(False)

        # Mitschrift des Terminals (nur Profil mit Terminal): Dateiname + Ordnerknopf.
        self.mitschrift_edit = QLineEdit(inner)
        self.mitschrift_edit.setPlaceholderText("leer = keine Mitschrift")
        self.mitschrift_edit.setClearButtonEnabled(True)
        self.mitschrift_edit.setToolTip(
            "Textdatei, in die alles geschrieben wird, was das Terminal vom Rechner empfängt "
            "(ohne Steuer- und Escape-Folgen).  Eine vorhandene Datei wird fortgesetzt, nicht "
            "überschrieben.  Weil der Rechner Eingaben zurückspiegelt (Echo), steht darin auch "
            "die Eingabe — Passwörter nicht.")
        self.mitschrift_edit.editingFinished.connect(self._on_mitschrift_edit)
        self.mitschrift_knopf = QToolButton(inner)
        self.mitschrift_knopf.setIcon(icon("folder"))
        self.mitschrift_knopf.setToolTip("Textdatei für die Mitschrift wählen oder anlegen …")
        self.mitschrift_knopf.clicked.connect(self._on_mitschrift_waehlen)
        self._mitschrift_zeile = QWidget(inner)
        zl = QHBoxLayout(self._mitschrift_zeile)
        zl.setContentsMargins(0, 0, 0, 0)
        zl.addWidget(self.mitschrift_edit, 1)
        zl.addWidget(self.mitschrift_knopf)
        if self.profil.terminal:
            form.addRow("Mitschrift:", self._mitschrift_zeile)
        else:
            self._mitschrift_zeile.setVisible(False)

        return inner

    # ── Mitschrift ───────────────────────────────────────────────────────────

    def _on_mitschrift_edit(self):
        if not self._mitschrift_guard:
            self.mitschriftChanged.emit(self.mitschrift_edit.text().strip())

    def _on_mitschrift_waehlen(self):
        """Ordnerknopf: .txt wählen oder neu benennen — eine vorhandene wird angehängt, deshalb
        keine Überschreibfrage."""
        start = self.mitschrift_edit.text().strip()
        pfad, _ = QFileDialog.getSaveFileName(
            self, "Mitschrift in Textdatei", start, "Textdateien (*.txt);;Alle Dateien (*)",
            options=QFileDialog.DontConfirmOverwrite)
        if not pfad:
            return
        if "." not in pfad.replace("\\", "/").rsplit("/", 1)[-1]:
            pfad += ".txt"
        self.mitschrift_edit.setText(pfad)
        self.mitschriftChanged.emit(pfad)

    def mitschrift_value(self) -> str:
        return self.mitschrift_edit.text().strip()

    def set_mitschrift_value(self, pfad: str):
        """Dateinamen setzen, ohne ``mitschriftChanged`` auszulösen."""
        self._mitschrift_guard = True
        self.mitschrift_edit.setText(pfad or "")
        self._mitschrift_guard = False

    def _on_hardware_combo(self, schluessel: str):
        self._hardware_gesperrt_nach_modell()      # Erweiterungsmodul haengt am RAM
        if self._hardware_guard:
            return
        self.hardwareChanged.emit(schluessel, self.hardware_combos[schluessel].currentData())

    def bildschirm_schalter_value(self) -> str:
        """Stellung des Schiebeschalters (``"k7024"``/``"k7023"``; ``""`` ohne Schalter)."""
        return self.bildschirm_schalter.stellung() if self.bildschirm_schalter else ""

    def set_bildschirm_schalter_value(self, karte: str):
        """Stellung setzen, ohne Signal."""
        if self.bildschirm_schalter is not None:
            self.bildschirm_schalter.set_stellung(karte)

    def hardware_value(self) -> dict:
        """Gewählte Hardwarevarianten ``{Schlüssel: Wert}`` (leer ohne Profil.hardware)."""
        return {k: b.currentData() for k, b in self.hardware_combos.items()}

    def set_hardware_value(self, daten: dict):
        """Hardwarevarianten setzen (ohne Signal)."""
        self._hardware_guard = True
        try:
            for k, box in self.hardware_combos.items():
                wert = (daten or {}).get(k)
                i = box.findData(wert)
                if i < 0 and k == "dram" and wert and p8000_ram.gueltig(wert):
                    # eigene Bestueckung aus der Konfiguration (P23b): als Eintrag zeigen
                    box.addItem(p8000_ram.anzeige(wert), wert)
                    i = box.count() - 1
                box.setCurrentIndex(i if i >= 0 else 0)
        finally:
            self._hardware_guard = False
        self._hardware_gesperrt_nach_modell()

    def _hardware_gesperrt_nach_modell(self):
        """Felder ausgrauen, die am gewählten Modell nichts bewirken (PC 1715W: Zeichensatz)."""
        modell = self.model_value()
        for k, box in self.hardware_combos.items():
            box.setEnabled(self.profil.hardware_wirkt(k, modell, self.hardware_value()))
        # Der Schiebeschalter ist nur mit BEIDEN Bildschirmkarten aktiv.
        if self.bildschirm_schalter is not None:
            box = self.hardware_combos["bildschirm"]
            self.bildschirm_schalter.setEnabled(
                box.isEnabled() and box.currentData() == "beide")
        if getattr(self, "rtc_gruppe", None) is not None:
            typ = self.hardware_combos[rtc.SCHLUESSEL_TYP]
            self.rtc_stellen_button.setEnabled(
                typ.isEnabled() and typ.currentData() != rtc.KEINE)

    # ── K3521 (batteriegepuffertes RAM) ──────────────────────────────────────

    def _on_k3521(self, *_):
        """Anzahl oder Startadresse geaendert: pruefen; ungueltig = sichtbar abweisen und den
        letzten gueltigen Wert zurueckholen, gueltig = melden (Neuaufbau)."""
        if self._k3521_guard:
            return
        anzahl = int(self.k3521_combo.currentData())
        start, grund = k3521.pruefe(anzahl, self.k3521_start_edit.text())
        if start is None:
            alt_anzahl, alt_start = self._k3521_stand
            self.k3521_hinweis.setText(
                f"Eingabe abgewiesen: {grund}.  Bleibt bei {alt_anzahl or 'keine'} / {alt_start:04X}H.")
            self.k3521_hinweis.setVisible(True)
            self.set_k3521_value(alt_anzahl, alt_start)
            return
        self.k3521_hinweis.setVisible(False)
        self.k3521_start_edit.setText(f"{start:04X}")
        if (anzahl, start) == self._k3521_stand:
            return
        self._k3521_stand = (anzahl, start)
        self.k3521Changed.emit(anzahl, start)

    def k3521_value(self):
        """``(Anzahl, Startadresse)`` des zuletzt uebernommenen Stands."""
        return self._k3521_stand

    def set_k3521_value(self, anzahl: int, start: int = k3521.START_VORGABE):
        """Felder setzen, ohne ``k3521Changed`` auszuloesen."""
        self._k3521_guard = True
        i = self.k3521_combo.findData(int(anzahl))
        self.k3521_combo.setCurrentIndex(i if i >= 0 else 0)
        self.k3521_start_edit.setText(f"{int(start):04X}")
        self._k3521_guard = False
        self._k3521_stand = (self.k3521_combo.currentData(), int(start))

    # ── AUP (Sparkasse) ──────────────────────────────────────────────────────

    def _aup_abhaengig(self):
        """Unteroptionen nur mit gesteckter AUP bedienbar."""
        an = self.aup_box.isChecked()
        self.aup_sonder_box.setEnabled(an)
        self.aup_alpha_combo.setEnabled(an)

    def _on_aup_box(self, *_):
        self._aup_abhaengig()
        if self._aup_guard:
            return
        self.aupChanged.emit(*self.aup_value())

    def aup_value(self):
        """``(an, sonder, alpha)``; ohne AUP sind Sonder und Alpha die Vorgabe (aus, ``led``)."""
        an = self.aup_box.isChecked()
        if not an:
            return False, False, "led"
        return True, self.aup_sonder_box.isChecked(), str(self.aup_alpha_combo.currentData())

    def set_aup_value(self, an: bool, sonder: bool = False, alpha: str = "led"):
        """Felder setzen, ohne ``aupChanged`` auszuloesen."""
        self._aup_guard = True
        self.aup_box.setChecked(bool(an))
        self.aup_sonder_box.setChecked(bool(an) and bool(sonder))
        i = self.aup_alpha_combo.findData(alpha)
        self.aup_alpha_combo.setCurrentIndex(i if i >= 0 else 0)
        self._aup_guard = False
        self._aup_abhaengig()

    # ── Lochstreifen (K6022) ─────────────────────────────────────────────────

    def _on_ptape_box(self, an: bool):
        if self._ptape_guard:
            return
        self.ptapeChanged.emit(bool(an))

    def _on_tape_box(self, an: bool):
        if self._tape_guard:
            return
        self.tapeChanged.emit(bool(an))

    def tape_value(self) -> bool:
        return self.tape_box.isChecked()

    def set_tape_value(self, an: bool):
        """Kästchen setzen, ohne ``tapeChanged`` auszulösen."""
        self._tape_guard = True
        self.tape_box.setChecked(bool(an))
        self._tape_guard = False

    def ptape_value(self) -> bool:
        return self.ptape_box.isChecked()

    def set_ptape_value(self, an: bool):
        """Kästchen setzen, ohne ``ptapeChanged`` auszulösen."""
        self._ptape_guard = True
        self.ptape_box.setChecked(bool(an))
        self._ptape_guard = False

    # ── RAM-Disk (RAF) ───────────────────────────────────────────────────────

    def _on_raf_combo(self, _idx: int):
        self.raf_standby_box.setEnabled(self.raf_value() != raf.KEINE)
        if self._raf_guard:
            return
        self.rafChanged.emit(self.raf_value())

    def _on_raf_standby_box(self, checked: bool):
        if self._raf_guard:
            return
        self.rafStandbyChanged.emit(bool(checked))

    def raf_value(self) -> str:
        """Der gewählte RAF-Schlüssel (``"none"``/``"raf128"``/``"raf512"``/``"raf2m"``)."""
        return raf.normalize(self.raf_combo.currentData())

    def set_raf_value(self, typ: str):
        """Den Eintrag für *typ* wählen (ohne Signal)."""
        self._raf_guard = True
        idx = self.raf_combo.findData(raf.normalize(typ))
        self.raf_combo.setCurrentIndex(idx if idx >= 0 else 0)
        self._raf_guard = False

    def raf_standby_value(self) -> bool:
        return self.raf_standby_box.isChecked()

    def set_raf_standby_value(self, an: bool):
        """Das Stand-by-Kästchen setzen (ohne Signal)."""
        self._raf_guard = True
        self.raf_standby_box.setChecked(bool(an))
        self._raf_guard = False

    def _on_model_index(self, _idx: int):
        """Nur Anzeige nachziehen (Modelle als Vorgaben melden ueber ``activated``)."""
        self._modus_anpassen()

    def _on_model_combo(self, _idx: int):
        self._modus_anpassen()
        self._hardware_gesperrt_nach_modell()
        if self._model_guard:
            return
        self.modelChanged.emit(self.model_value())

    def bauart_value(self) -> str:
        """Die gewählte Bauart (``""`` ohne Bauarten)."""
        return self.profil.bauart_normalisieren(self.bauart_combo.currentData())

    def set_bauart_value(self, schluessel: str) -> None:
        """Die Bauart setzen (ohne Signal)."""
        i = self.bauart_combo.findData(self.profil.bauart_normalisieren(schluessel))
        if i >= 0:
            self.bauart_combo.setCurrentIndex(i)

    def winchester_value(self) -> int:
        return int(self.winchester_combo.currentData() or 1) if self.winchester_combo else 1

    def set_winchester_value(self, n: int) -> None:
        """Zahl der Winchester setzen (ohne Signal)."""
        if self.winchester_combo is None:
            return
        self._winchester_guard = True
        try:
            i = self.winchester_combo.findData(max(1, min(3, int(n))))
            self.winchester_combo.setCurrentIndex(i if i >= 0 else 0)
        finally:
            self._winchester_guard = False

    def _terminal_betrieb(self) -> bool:
        return (self.profil.betriebsart_wahl
                and self.betriebsart_combo.currentData() == "terminal")

    def ausstattung_value(self) -> str:
        """Die gewählte Rechnerausstattung (auch im Betrieb „nur Terminal“ — sie wird gemerkt)."""
        return self.profil.ausstattung_normalisieren(self.model_combo.currentData())

    def set_ausstattung_value(self, schluessel: str):
        """Die Ausstattung setzen (ohne Signal)."""
        self._model_guard = True
        idx = self.model_combo.findData(self.profil.ausstattung_normalisieren(schluessel))
        self.model_combo.setCurrentIndex(idx if idx >= 0 else 0)
        self._model_guard = False

    def model_value(self) -> str:
        """Der aktuell gewählte Modellschlüssel (``app.modell.A5120``/``A5120_16``; P8000: bei
        „nur Terminal“ der Schlüssel des Terminalmodells)."""
        if self._terminal_betrieb():
            return self.profil.einheit_modell()
        data = self.model_combo.currentData()
        return self.profil.modell_normalisieren(data)

    def set_model_value(self, model: str):
        """Den Eintrag für *model* wählen (ohne Signal)."""
        self._model_guard = True
        schluessel = self.profil.modell_normalisieren(model)
        if self.profil.betriebsart_wahl:
            terminal = self.profil.modell_art(schluessel) == "einheit"
            self.betriebsart_combo.setCurrentIndex(1 if terminal else 0)
            if not terminal:                  # im Terminalbetrieb bleibt die gemerkte Ausstattung
                idx = self.model_combo.findData(schluessel)
                self.model_combo.setCurrentIndex(idx if idx >= 0 else 0)
        else:
            idx = self.model_combo.findData(schluessel)
            self.model_combo.setCurrentIndex(idx if idx >= 0 else 0)
        self._model_guard = False
        self._modus_anpassen()

    def _modus_anpassen(self):
        """Felder und Reiter dem Modell anpassen: unwirksame Hardwarewahl ausgrauen, bei „nur
        Terminal“ die Ausstattung ausgrauen und den Reiter „Laufwerke“ ausblenden."""
        self._hardware_gesperrt_nach_modell()
        if self.profil.betriebsart_wahl:
            self.model_combo.setEnabled(not self._terminal_betrieb())
        hat = self.profil.modell_hat_rechner(self.model_value())
        if getattr(self, "winchester_combo", None) is not None:
            self.winchester_combo.setEnabled(self.profil.modell_hat_wdc(self.model_value()))
        if getattr(self, "bauart_combo", None) is not None:
            self.bauart_combo.setEnabled(hat)        # „nur Terminal“: die Bauart gehoert zum Rechner
        if getattr(self, "rtc_gruppe", None) is not None:
            self.rtc_gruppe.setVisible(hat)          # „nur Terminal“: wie die anderen Rechnerfelder weg
        i = self.tabs.indexOf(self._drives_tab) if hasattr(self, "_drives_tab") else -1
        if i >= 0:
            self.tabs.setTabVisible(i, hat and self.profil.laufwerke)   # MC80: keine Laufwerke

    def set_rtc_anzeige(self, text: str, tipp: str = "") -> None:
        """Text „Uhrzeit des Gastsystems“ setzen (nur bei Aenderung; ohne Signal)."""
        if getattr(self, "rtc_gruppe", None) is None:
            return
        if self.rtc_zeit_label.text() != text:
            self.rtc_zeit_label.setText(text)
        if self.rtc_zeit_label.toolTip() != tipp:
            self.rtc_zeit_label.setToolTip(tipp)

    def set_nenntakt(self, text: str) -> None:
        """Beschriftung der Taktstufen nach dem Nenntakt des Modells (ohne Signal)."""
        self.SPEED_OPTIONS = takt.auswahl(text)
        self._speed_guard = True
        try:
            for i, (label, _f) in enumerate(self.SPEED_OPTIONS):
                self.speed_combo.setItemText(i, label)
        finally:
            self._speed_guard = False
        self.speed_combo.setToolTip(
            f"Der {self.profil.rechner} läuft mit {text}.  "
            "Ein Vielfaches davon "
            "kürzt einen Kaltstart ab — die Uhr des Gastsystems zählt aber "
            "Taktzyklen und geht dann entsprechend falsch.")

    def _on_speed_combo(self, _idx: int):
        if self._speed_guard:
            return
        self.speedChanged.emit(self.speed_value())

    def speed_value(self) -> float:
        """Current speed factor from the dropdown."""
        data = self.speed_combo.currentData()
        return float(data) if data is not None else 1.0

    def set_speed_value(self, factor: float):
        """Select the dropdown entry matching *factor* (no signal emitted)."""
        self._speed_guard = True
        idx = 0
        for i in range(self.speed_combo.count()):
            if abs(float(self.speed_combo.itemData(i)) - float(factor)) < 1e-9:
                idx = i
                break
        self.speed_combo.setCurrentIndex(idx)
        self._speed_guard = False

    # ── Laufwerke tab ────────────────────────────────────────────────────────

    def _build_drives_tab(self) -> QWidget:
        """One dropdown per K5122 slot to pick the drive type (or 'kein Laufwerk')."""
        inner = QWidget()
        form = QFormLayout(inner)

        for slot in range(dt.NUM_SLOTS):
            combo = QComboBox()
            for _short, core, _desc in self.profil.waehlbare_laufwerke():
                combo.addItem(dt.combo_label(core), core)
            combo.addItem(dt.NO_DRIVE_LABEL, dt.NO_DRIVE)
            combo.currentIndexChanged.connect(self._on_drive_combo)
            self._drive_combos.append(combo)
            form.addRow(f"Laufwerk {slot}:", combo)

        # Winchester-Laufwerke (P8000, Ticket #15): 1–3 Kaesten im Laufwerksreiter; wirkt ohne Neubau.
        self.winchester_combo = None
        self._winchester_guard = False
        if self.profil.platte:
            self.winchester_combo = QComboBox()
            for n in (1, 2, 3):
                # Der gespeicherte Wert bleibt die Zahl; nur die Anzeige traegt den Zusatz (unter WEGA ist LW 2 nicht nutzbar).
                self.winchester_combo.addItem(
                    str(n) if n < 3 else "3 (wird aktuell nicht von WEGA unterstützt)", n)
            self.winchester_combo.setToolTip(
                "Zahl der Winchester-Laufwerke am WDC (1–3, Vorgabe 1).  Jedes bekommt im Reiter "
                "Laufwerke einen Kasten für sein Plattenabbild.  Der WDC erkennt die Platten beim "
                "Hochlauf (nach Rückstellen oder Rechner ein).  Ein weggenommenes Laufwerk wird "
                "abgetrennt.  Ohne Winchesterkontroller (Ausstattung ohne Winchester / nur 8-Bit) "
                "gesperrt.")
            self.winchester_combo.currentIndexChanged.connect(self._on_winchester_combo)
            form.addRow("Winchester-Laufwerke:", self.winchester_combo)

        # Start from the standard configuration until a config restore sets it.
        self.set_drive_types(self.profil.standard_laufwerke())
        return inner

    def _on_winchester_combo(self, _i: int):
        if not self._winchester_guard:
            self.winchesterChanged.emit(self.winchester_value())

    def _on_drive_combo(self, _idx: int):
        if self._drive_guard:
            return
        self.driveTypesChanged.emit(self.drive_types())

    def drive_types(self) -> list:
        """Current per-slot core DriveProfile names (length :data:`dt.NUM_SLOTS`)."""
        return [c.currentData() for c in self._drive_combos]

    def set_drive_types(self, types: list):
        """Select the dropdown entries for *types* (no signal emitted)."""
        types = dt.normalize_list(types)
        self._drive_guard = True
        for slot, combo in enumerate(self._drive_combos):
            core = types[slot]
            idx = combo.findData(core)
            combo.setCurrentIndex(idx if idx >= 0 else 0)
        self._drive_guard = False

    # ── CRT tab ──────────────────────────────────────────────────────────────

    def _build_crt_tab(self) -> QWidget:
        p = self.screen.params

        inner = QWidget()
        form = QFormLayout(inner)

        # Gesamtschalter + Bildrate der Anzeige
        self._add_stufe(form, "Röhren-Nachbildung (aus = schlichte Textkonsole, spart Rechenleistung)",
                        "crt_on")
        rate = QComboBox()
        for hz in (15, 30, 60):
            rate.addItem(f"{hz} Bilder/s", float(hz))

        def rate_refresh():
            rate.blockSignals(True)
            i = rate.findData(float(self.screen.params.bildrate))
            rate.setCurrentIndex(i if i >= 0 else 1)
            rate.blockSignals(False)

        def rate_changed(_i):
            self.screen.params.bildrate = float(rate.currentData())
            self._apply()

        rate.currentIndexChanged.connect(rate_changed)
        rate_refresh()
        self._refreshers.append(rate_refresh)
        form.addRow("Bildrate der Anzeige", rate)

        # Filterstufen (einzeln abschaltbar)
        for text, attr in (("Vignette", "vignette_on"),):
            self._add_stufe(form, text, attr)

        # Colours
        self._add_color(form, "Farbe aktiv",
                        lambda: self.screen.params.phosphor_on,
                        lambda v: setattr(self.screen.params, "phosphor_on", v))
        self._add_color(form, "Farbe inaktiv",
                        lambda: self.screen.params.phosphor_off,
                        lambda v: setattr(self.screen.params, "phosphor_off", v))

        # Image
        self._add_float(form, "Helligkeit", 0.0, 4.0, 0.01,
                        lambda: self.screen.params.brightness,
                        lambda v: setattr(self.screen.params, "brightness", v))
        self._add_float(form, "Kontrast", 0.0, 2.0, 0.01,
                        lambda: self.screen.params.contrast,
                        lambda v: setattr(self.screen.params, "contrast", v))

        # Geometry
        self._add_float(form, "Krümmung X", 0.0, 0.3, 0.005,
                        lambda: self.screen.params.curvature[0],
                        lambda v: setattr(self.screen.params, "curvature",
                                          (v, self.screen.params.curvature[1])))
        self._add_float(form, "Krümmung Y", 0.0, 0.3, 0.005,
                        lambda: self.screen.params.curvature[1],
                        lambda v: setattr(self.screen.params, "curvature",
                                          (self.screen.params.curvature[0], v)))
        self._add_float(form, "Ecken-Rundung", 0.0, 0.25, 0.005,
                        lambda: self.screen.params.corner_radius,
                        lambda v: setattr(self.screen.params, "corner_radius", v))

        # Raster geometry (H/V size + position)
        self._add_float(form, "Bildbreite", 0.3, 1.5, 0.005,
                        lambda: self.screen.params.scale_x,
                        lambda v: setattr(self.screen.params, "scale_x", v))
        self._add_float(form, "Bildhöhe", 0.3, 1.5, 0.005,
                        lambda: self.screen.params.scale_y,
                        lambda v: setattr(self.screen.params, "scale_y", v))
        self._add_float(form, "Verschiebung X", -0.5, 0.5, 0.005,
                        lambda: self.screen.params.offset_x,
                        lambda v: setattr(self.screen.params, "offset_x", v))
        self._add_float(form, "Verschiebung Y", -0.5, 0.5, 0.005,
                        lambda: self.screen.params.offset_y,
                        lambda v: setattr(self.screen.params, "offset_y", v))

        # Reset
        reset_btn = QPushButton("Auf Standard zurücksetzen")
        reset_btn.clicked.connect(self._reset_crt)
        form.addRow(reset_btn)

        # Regler, die nur der CRT-Shader auswertet (die schlichte Qt-Flaeche kennt sie nicht).
        self._nur_crt = []
        nur_crt = {"Kontrast", "Krümmung X", "Krümmung Y", "Ecken-Rundung"}
        for i in range(form.rowCount()):
            lab = form.itemAt(i, QFormLayout.LabelRole)
            feld = form.itemAt(i, QFormLayout.FieldRole)
            span = form.itemAt(i, QFormLayout.SpanningRole)
            if lab is not None and lab.widget() is not None and lab.widget().text() in nur_crt:
                self._nur_crt += [lab.widget(), feld.widget()]
            elif span is not None and isinstance(span.widget(), QCheckBox) \
                    and span.widget().text() == "Vignette":
                self._nur_crt.append(span.widget())
        self._crt_freigeben()

        # Wrap in a scroll area so the panel stays usable when narrow.
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(inner)
        return scroll

    def _crt_freigeben(self):
        """Regler, die ohne Roehren-Nachbildung nichts bewirken, ausgrauen."""
        an = bool(self.screen.params.crt_on)
        for w in getattr(self, "_nur_crt", ()):
            w.setEnabled(an)

    def refresh_all(self):
        """Re-read every control from the live params (e.g. after a config load)."""
        for refresh in self._refreshers:
            refresh()
        self._crt_freigeben()

    def _reset_crt(self):
        from dataclasses import fields
        defaults = CRTParams()
        for f in fields(CRTParams):
            setattr(self.screen.params, f.name, getattr(defaults, f.name))
        self.refresh_all()
        self._apply()

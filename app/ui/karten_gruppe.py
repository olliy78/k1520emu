"""
K1520 Emulator - Zeilen „USB-Stick" und „Netzwerk" unter *Einstellungen ▸ Allgemein*
====================================================================================

Zwei weitere Gruppen der Reihe „Erweiterungskarten" (doc/design/50_gun_gide.md, AP U3/N3) nach dem
Muster der :class:`app.ui.gide_gruppe.GideGruppe`: sie sammeln NUR die Eingaben und melden sie als
Zustand (:mod:`app.usb`, :mod:`app.netz`); das Hauptfenster steckt die Karte (Neuaufbau der Maschine),
wirkt auf den Kern und zeigt Kernfehler an.  Die Gruppen kennen den Kern nicht.
"""

from __future__ import annotations

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QFileDialog, QFormLayout, QHBoxLayout, QLabel, QLineEdit,
    QPushButton, QSpinBox, QWidget,
)

from app import kartenwahl, netz, sound, usb


class KartenGruppe(QWidget):
    """Gemeinsamer Teil: Zeile mit Kaestchen und Basis-Auswahl, roter Fehlertext -- flach, ohne Rahmen."""

    def __init__(self, titel: str, karte_text: str, art: kartenwahl.KartenArt, basis_vorgabe: int,
                 tipp_karte: str, tipp_basis: str, parent=None):
        super().__init__(parent)
        self._guard = False
        self._basis_vorgabe = int(basis_vorgabe)
        self._gesteckt = False
        self.form = QFormLayout(self)
        self.form.setContentsMargins(0, 0, 0, 0)

        self.karte_box = QCheckBox(karte_text, self)
        self.karte_box.setToolTip(tipp_karte)

        self.basis_combo = QComboBox(self)
        for b in art.basen():
            text = kartenwahl.basis_text(b)
            if b in art.hervorgehoben:
                text += f" ({art.hervorgehoben[b]})"
            self.basis_combo.addItem(text, b)
        self.basis_combo.setToolTip(tipp_basis)
        zeile = QHBoxLayout()
        zeile.addWidget(self.karte_box)
        zeile.addWidget(QLabel("E/A-Basis:", self))
        zeile.addWidget(self.basis_combo, 1)
        self.form.addRow(titel + ":", zeile)

        self.fehler_label = QLabel("", self)
        self.fehler_label.setStyleSheet("color: #c00;")
        self.fehler_label.setWordWrap(True)
        self.fehler_label.setVisible(False)

    def _fehlerzeile(self) -> None:
        """Die Fehlerzeile als LETZTE Zeile (nach den Feldern der Unterklasse)."""
        self.form.addRow(self.fehler_label)

    def _basis_setzen(self, z: dict) -> None:
        idx = self.basis_combo.findData(int(z.get("basis", self._basis_vorgabe)))
        self.basis_combo.setCurrentIndex(idx if idx >= 0 else self.basis_combo.findData(self._basis_vorgabe))

    def set_basen(self, zeilen) -> None:
        """Auswahlfeld beschriften (:func:`app.kartenwahl.eintraege`): belegte Eintraege nennen ihren
        Besitzer und sind nicht waehlbar.  Die Auswahl bleibt, wie sie ist."""
        modell = self.basis_combo.model()
        for z in zeilen:
            i = self.basis_combo.findData(z.basis)
            if i < 0:
                continue
            self.basis_combo.setItemText(i, z.text)
            item = modell.item(i)
            if item is not None:
                item.setEnabled(not z.belegt)

    def set_fehler(self, text: str) -> None:
        self.fehler_label.setText(text or "")
        self.fehler_label.setVisible(bool(text))

    def gesteckt(self) -> bool:
        return self._gesteckt


class UsbGruppe(KartenGruppe):
    """Kaestchen, Basis, Stick-Ordner, Schreibschutz und Stick-Knopf der GUN-USB."""

    #: Zustand (:func:`app.usb.normalize`) nach einer Eingabe des Anwenders.
    usbChanged = Signal(dict)
    #: „Stick entfernen/stecken": True = einstecken gewuenscht.
    stickGewuenscht = Signal(bool)

    def __init__(self, basis_vorgabe: int, parent=None):
        super().__init__("USB-Stick (GUN)", "USB-Karte stecken", kartenwahl.USB, basis_vorgabe,
                         usb.TIPP_KARTE, usb.TIPP_BASIS, parent)
        self._stick = False

        self.ordner_edit = QLineEdit(self)
        self.ordner_edit.setPlaceholderText(usb.ordner_vorgabe())
        self.ordner_edit.setToolTip(usb.TIPP_ORDNER)
        self.waehlen_knopf = QPushButton("Wählen …", self)
        zeile = QWidget(self)
        zl = QHBoxLayout(zeile)
        zl.setContentsMargins(0, 0, 0, 0)
        zl.addWidget(self.ordner_edit, 1)
        zl.addWidget(self.waehlen_knopf, 0)
        self.form.addRow("Stick-Ordner:", zeile)

        self.schutz_box = QCheckBox("Stick schreibgeschützt", self)
        self.schutz_box.setToolTip(usb.TIPP_SCHUTZ)
        self.stick_knopf = QPushButton("Stick entfernen", self)
        self.stick_knopf.setToolTip(usb.TIPP_STICK)
        z2 = QHBoxLayout()
        z2.addWidget(self.schutz_box)
        z2.addStretch(1)
        z2.addWidget(self.stick_knopf)
        self.form.addRow("", z2)
        self._fehlerzeile()

        self.karte_box.toggled.connect(self._aenderung)
        self.basis_combo.currentIndexChanged.connect(self._aenderung)
        self.ordner_edit.editingFinished.connect(self._aenderung)
        self.schutz_box.toggled.connect(self._aenderung)
        self.waehlen_knopf.clicked.connect(lambda _c=False: self._on_waehlen())
        self.stick_knopf.clicked.connect(lambda _c=False: self.stickGewuenscht.emit(not self._stick))
        self.set_zustand(usb.vorgabe(self._basis_vorgabe))

    def zustand(self) -> dict:
        return {"enabled": self.karte_box.isChecked(),
                "basis": int(self.basis_combo.currentData()),
                "ordner": self.ordner_edit.text().strip(),
                "schreibgeschuetzt": self.schutz_box.isChecked()}

    def set_zustand(self, z: dict, fehler: str = "", gesteckt=None, stick=None) -> None:
        """Eingaben setzen (ohne Signal).  *gesteckt*: die Karte steckt WIRKLICH; *stick*: ein Stick steckt."""
        self._guard = True
        self.karte_box.setChecked(bool(z.get("enabled")))
        self._basis_setzen(z)
        self.ordner_edit.setText(str(z.get("ordner") or ""))
        self.schutz_box.setChecked(z.get("schreibgeschuetzt") is True)
        self._guard = False
        if gesteckt is not None:
            self._gesteckt = bool(gesteckt)
        if stick is not None:
            self._stick = bool(stick)
        self.set_fehler(fehler)
        self._knoepfe()

    def _knoepfe(self) -> None:
        """Stick-Bedienung nur mit gesteckter Karte."""
        g = self._gesteckt
        self.stick_knopf.setEnabled(g)
        self.stick_knopf.setText("Stick entfernen" if self._stick else "Stick stecken")

    def _aenderung(self, *_args) -> None:
        self._knoepfe()
        if not self._guard:
            self.usbChanged.emit(self.zustand())

    def _ordner_waehlen(self) -> str:
        return QFileDialog.getExistingDirectory(self, "Stick-Ordner wählen", usb.ordner_vorgabe())

    def _on_waehlen(self) -> None:
        pfad = self._ordner_waehlen()
        if pfad:
            self.ordner_edit.setText(pfad)
            self._aenderung()


class NetzGruppe(KartenGruppe):
    """Kaestchen, Basis, Loopback, Kabel und Zaehler der KCNet-Karte."""

    netChanged = Signal(dict)

    def __init__(self, basis_vorgabe: int, parent=None):
        super().__init__("Netzwerk (KCNet)", "Netzwerkkarte stecken", kartenwahl.NETZ, basis_vorgabe,
                         netz.TIPP_KARTE, netz.TIPP_BASIS, parent)

        self.loopback_box = QCheckBox("nur lokal (Loopback)", self)
        self.loopback_box.setToolTip(netz.TIPP_LOOPBACK)
        self.link_box = QCheckBox("Netzwerkkabel verbunden", self)
        self.link_box.setToolTip(netz.TIPP_LINK)
        z2 = QHBoxLayout()
        z2.addWidget(self.loopback_box)
        z2.addWidget(self.link_box)
        z2.addStretch(1)
        self.form.addRow("", z2)
        self.zaehler_label = QLabel("", self)
        self.form.addRow("Verkehr:", self.zaehler_label)
        self._fehlerzeile()

        self.karte_box.toggled.connect(self._aenderung)
        self.basis_combo.currentIndexChanged.connect(self._aenderung)
        self.loopback_box.toggled.connect(self._aenderung)
        self.link_box.toggled.connect(self._aenderung)
        self.set_zustand(netz.vorgabe(self._basis_vorgabe))

    def zustand(self) -> dict:
        return {"enabled": self.karte_box.isChecked(),
                "basis": int(self.basis_combo.currentData()),
                "nur_loopback": self.loopback_box.isChecked(),
                "link": self.link_box.isChecked()}

    def set_zustand(self, z: dict, fehler: str = "", gesteckt=None) -> None:
        self._guard = True
        self.karte_box.setChecked(bool(z.get("enabled")))
        self._basis_setzen(z)
        self.loopback_box.setChecked(z.get("nur_loopback") is True)
        self.link_box.setChecked(z.get("link") is not False)
        self._guard = False
        if gesteckt is not None:
            self._gesteckt = bool(gesteckt)
        self.set_fehler(fehler)
        self._knoepfe()

    def _knoepfe(self) -> None:
        self.zaehler_label.setEnabled(self._gesteckt)

    def set_zaehler(self, text: str) -> None:
        self.zaehler_label.setText(text or "—")

    def _aenderung(self, *_args) -> None:
        self._knoepfe()
        if not self._guard:
            self.netChanged.emit(self.zustand())


class SoundGruppe(KartenGruppe):
    """SOUND-Karte K1520: Kaestchen, Chip, Basis, AY-Takt, Quarz, Kanalzuordnung, Tiefpass, CTC-Kaskade."""

    #: Zustand (:func:`app.sound.normalize`) nach einer Eingabe des Anwenders.
    soundChanged = Signal(dict)

    def __init__(self, basis_vorgabe: int, parent=None):
        super().__init__("Soundkarte", "Soundkarte stecken", kartenwahl.SOUND, basis_vorgabe,
                         sound.TIPP_KARTE, sound.TIPP_BASIS, parent)
        # Bestueckung: aus / AY-3-8910 / AY-3-8912 -- das Kaestchen "stecken" bleibt (wie bei den
        # Schwesterkarten), der Chip steht in der Zeile darunter.
        self.chip_combo = QComboBox(self)
        for wert, text in sound.CHIPS:
            self.chip_combo.addItem(text, wert)
        self.chip_combo.setToolTip(sound.TIPP_CHIP)
        self.kanal_combo = QComboBox(self)
        for wert, text in sound.KANAELE:
            self.kanal_combo.addItem(text, wert)
        self.kanal_combo.setToolTip(sound.TIPP_KANAL)
        z1 = QHBoxLayout()
        z1.addWidget(QLabel("Chip:", self))
        z1.addWidget(self.chip_combo)
        z1.addWidget(QLabel("Kanäle:", self))
        z1.addWidget(self.kanal_combo, 1)
        self.form.addRow("", z1)

        self.takt_combo = QComboBox(self)
        for wert, text in sound.TAKTE:
            self.takt_combo.addItem(text, wert)
        self.takt_combo.setToolTip(sound.TIPP_TAKT)
        self.quarz_spin = QSpinBox(self)
        self.quarz_spin.setRange(100_000, 20_000_000)
        self.quarz_spin.setSingleStep(1000)
        self.quarz_spin.setGroupSeparatorShown(True)
        self.quarz_spin.setSuffix(" Hz")
        self.quarz_spin.setToolTip(sound.TIPP_QUARZ)
        z2 = QHBoxLayout()
        z2.addWidget(QLabel("AY-Takt:", self))
        z2.addWidget(self.takt_combo)
        z2.addWidget(QLabel("Quarz:", self))
        z2.addWidget(self.quarz_spin, 1)
        self.form.addRow("", z2)

        self.tiefpass_box = QCheckBox("Tiefpass C16/C17", self)
        self.tiefpass_box.setToolTip(sound.TIPP_TIEFPASS)
        self.kaskade_boxen = []
        z3 = QHBoxLayout()
        z3.addWidget(self.tiefpass_box)
        z3.addWidget(QLabel("CTC-Kaskade:", self))
        for text in sound.KASKADEN:
            b = QCheckBox(text, self)
            b.setToolTip(sound.TIPP_KASKADE)
            self.kaskade_boxen.append(b)
            z3.addWidget(b)
        z3.addStretch(1)
        self.form.addRow("", z3)
        self._fehlerzeile()

        for w in (self.karte_box, self.tiefpass_box, *self.kaskade_boxen):
            w.toggled.connect(self._aenderung)
        for w in (self.basis_combo, self.chip_combo, self.kanal_combo, self.takt_combo):
            w.currentIndexChanged.connect(self._aenderung)
        self.quarz_spin.editingFinished.connect(self._aenderung)
        self.set_zustand(sound.vorgabe(self._basis_vorgabe))

    def zustand(self) -> dict:
        return {"enabled": self.karte_box.isChecked(),
                "basis": int(self.basis_combo.currentData()),
                "chip": self.chip_combo.currentData(),
                "takt": self.takt_combo.currentData(),
                "quarz_hz": int(self.quarz_spin.value()),
                "kanal": self.kanal_combo.currentData(),
                "tiefpass": self.tiefpass_box.isChecked(),
                "kaskade": sum(1 << i for i, b in enumerate(self.kaskade_boxen) if b.isChecked())}

    def set_zustand(self, z: dict, fehler: str = "", gesteckt=None) -> None:
        self._guard = True
        self.karte_box.setChecked(bool(z.get("enabled")))
        self._basis_setzen(z)
        for combo, schl in ((self.chip_combo, "chip"), (self.takt_combo, "takt"), (self.kanal_combo, "kanal")):
            i = combo.findData(z.get(schl))
            combo.setCurrentIndex(i if i >= 0 else 0)
        self.quarz_spin.setValue(int(z.get("quarz_hz", sound.QUARZ_VORGABE)))
        self.tiefpass_box.setChecked(z.get("tiefpass") is True)
        maske = int(z.get("kaskade", 0))
        for i, b in enumerate(self.kaskade_boxen):
            b.setChecked(bool(maske >> i & 1))
        self._guard = False
        if gesteckt is not None:
            self._gesteckt = bool(gesteckt)
        self.set_fehler(fehler)

    def _aenderung(self, *_args) -> None:
        if not self._guard:
            self.soundChanged.emit(self.zustand())

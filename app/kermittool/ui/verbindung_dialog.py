"""Verbindungsdialog — Client verbinden bzw. Server einrichten.

Eine Klasse fuer beide Betriebsarten: ``betriebsart`` blendet weg, was nicht gilt
(Anmeldung nur beim Client, Freigabe nur beim Server).  Die Rolle der LEITUNG
(verbinden/lauschen) ist davon unabhaengig — auch ein Server darf zum Emulator
verbinden, und ein Client darf lauschen.

Das **Kennwort** steht nur im Eingabefeld (verdeckt) und im Rueckgabewert von
:meth:`kennwort`; es wird nie in die Konfiguration uebernommen.
"""

from __future__ import annotations

from typing import List, Optional

from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QDialog, QDialogButtonBox, QFileDialog, QFormLayout, QHBoxLayout, QLabel,
    QLineEdit, QPushButton, QSpinBox, QVBoxLayout, QWidget,
)
from PySide6.QtWidgets import QLineEdit as _QLE

from app.kermittool import konfig


class VerbindungDialog(QDialog):
    def __init__(self, parent, betriebsart: str, vorgabe: dict, zuletzt: List[dict],
                 ordner: str = "", nur_lesen: bool = False):
        super().__init__(parent)
        self.betriebsart = betriebsart
        server = betriebsart == "server"
        self.setWindowTitle("Server einrichten" if server else "Als Client verbinden")
        self.resize(460, 10)
        self._liste: List[tuple] = []        # (Verbindung, Kennwort, Einstellungsaenderungen)
        self.einstellungsaenderung: dict = {}

        self.voreinst = QComboBox()
        self.voreinst.addItem("— Voreinstellung wählen —", None)
        for name, v, kw, aend in konfig.VOREINSTELLUNGEN:
            if v["betriebsart"] == betriebsart:
                self.voreinst.addItem(name, ("vor", v, kw, aend))
        for v in zuletzt:
            if v.get("betriebsart") == betriebsart:
                self.voreinst.addItem("Zuletzt: " + konfig.anzeigename(v), ("zuletzt", v, "", {}))
        self.voreinst.activated.connect(self._voreinstellung)

        self.leitung = QComboBox()
        self.leitung.addItem("Telnet", "telnet")
        self.leitung.addItem("RFC 2217 (serielle Leitung über Telnet)", "rfc2217")
        self.aufbau = QComboBox()
        self.aufbau.addItem("Verbinden (zum Emulator/Server)", "verbinden")
        self.aufbau.addItem("Lauschen (der Emulator verbindet hierher)", "lauschen")
        self.aufbau.currentIndexChanged.connect(self._sichtbarkeit)
        self.host = QLineEdit()
        self.port = QSpinBox()
        self.port.setRange(0, 65535)
        self.port.setToolTip("0 beim Lauschen = freien Port nehmen")
        self.baud = QComboBox()
        self.baud.setEditable(True)
        for b in (1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200):
            self.baud.addItem(str(b), b)
        self.leitung.currentIndexChanged.connect(self._sichtbarkeit)

        self.login = QCheckBox("Anmelden (Login-Skript für eine getty)")
        self.login.toggled.connect(self._sichtbarkeit)
        self.login_name = QLineEdit()
        self.login_kennwort = QLineEdit()
        self.login_kennwort.setEchoMode(_QLE.Password)
        self.login_kennwort.setToolTip("Wird nur für diese Sitzung gehalten und nie gespeichert.")

        self.ordner = QLineEdit(ordner)
        self.ordner_knopf = QPushButton("Wählen …")
        self.ordner_knopf.clicked.connect(self._ordner_waehlen)
        zeile = QHBoxLayout()
        zeile.setContentsMargins(0, 0, 0, 0)
        zeile.addWidget(self.ordner, 1)
        zeile.addWidget(self.ordner_knopf)
        self._ordner_zeile = QWidget()
        self._ordner_zeile.setLayout(zeile)
        self.nur_lesen = QCheckBox("Nur lesen (Gegenstelle darf nichts schreiben oder löschen)")
        self.nur_lesen.setChecked(nur_lesen)

        self.form = QFormLayout()
        self.form.addRow("Voreinstellung", self.voreinst)
        self.form.addRow("Leitung", self.leitung)
        self.form.addRow("Aufbau", self.aufbau)
        self.form.addRow("Host", self.host)
        self.form.addRow("Port", self.port)
        self.form.addRow("Baud-Vorschlag", self.baud)
        if server:
            self.form.addRow("Freigegebener Ordner", self._ordner_zeile)
            self.form.addRow("", self.nur_lesen)
        else:
            self.form.addRow("", self.login)
            self.form.addRow("Benutzername", self.login_name)
            self.form.addRow("Kennwort", self.login_kennwort)

        self.knoepfe = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        self.knoepfe.button(QDialogButtonBox.Ok).setText("Server einrichten" if server else "Verbinden")
        self.knoepfe.accepted.connect(self._annehmen)
        self.knoepfe.rejected.connect(self.reject)
        self.meldung = QLabel("")
        self.meldung.setStyleSheet("color: #a33;")

        lay = QVBoxLayout(self)
        lay.addLayout(self.form)
        lay.addWidget(self.meldung)
        lay.addWidget(self.knoepfe)
        self.setzen(vorgabe)

    # ── Werte ───────────────────────────────────────────────────────────────

    def setzen(self, v: dict) -> None:
        v = konfig.verbindung_aus(v)
        self.leitung.setCurrentIndex(max(0, self.leitung.findData(v["leitung"])))
        self.aufbau.setCurrentIndex(max(0, self.aufbau.findData(v["aufbau"])))
        self.host.setText(v["host"])
        self.port.setValue(v["port"])
        self.baud.setCurrentText(str(v["baud"]))
        self.login.setChecked(v["login"])
        self.login_name.setText(v["login_name"])
        self._sichtbarkeit()

    def verbindung(self) -> dict:
        try:
            baud = int(self.baud.currentText())
        except ValueError:
            baud = 9600
        return {"betriebsart": self.betriebsart,
                "leitung": self.leitung.currentData(),
                "aufbau": self.aufbau.currentData(),
                "host": self.host.text().strip(),
                "port": self.port.value(),
                "baud": baud,
                "login": self.login.isChecked() and self.betriebsart == "client",
                "login_name": self.login_name.text().strip()}

    def kennwort(self) -> str:
        return self.login_kennwort.text()

    def freigabe(self) -> str:
        return self.ordner.text().strip()

    def _voreinstellung(self, index: int) -> None:
        d = self.voreinst.itemData(index)
        if not d:
            return
        _art, v, kw, aend = d
        self.setzen(v)
        self.login_kennwort.setText(kw)
        self.einstellungsaenderung = dict(aend)

    def _sichtbarkeit(self) -> None:
        lauschen = self.aufbau.currentData() == "lauschen"
        self.host.setEnabled(not lauschen)
        self.baud.setEnabled(self.leitung.currentData() == "rfc2217")
        an = self.login.isChecked()
        self.login_name.setEnabled(an)
        self.login_kennwort.setEnabled(an)

    def _ordner_waehlen(self) -> None:
        d = QFileDialog.getExistingDirectory(self, "Ordner freigeben", self.ordner.text() or "")
        if d:
            self.ordner.setText(d)

    def _annehmen(self) -> None:
        v = self.verbindung()
        if v["aufbau"] == "verbinden" and not v["host"]:
            self.meldung.setText("Bitte einen Host eintragen.")
            return
        if v["aufbau"] == "verbinden" and v["port"] <= 0:
            self.meldung.setText("Bitte einen Port eintragen.")
            return
        if v["login"] and not v["login_name"]:
            self.meldung.setText("Bitte einen Benutzernamen eintragen.")
            return
        self.accept()

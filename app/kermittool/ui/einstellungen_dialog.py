"""Einstellungen der Uebertragung — Text/Binaer, Namen, Pruefsumme, Paketlaenge, Fristen."""

from __future__ import annotations

from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QDialog, QDialogButtonBox, QFormLayout, QPushButton, QSpinBox, QVBoxLayout,
)

from app.kermittool import konfig


class EinstellungenDialog(QDialog):
    def __init__(self, parent, einst: dict):
        super().__init__(parent)
        self.setWindowTitle("Einstellungen der Übertragung")
        self.modus = QComboBox()
        self.modus.addItem("Binär (Byte für Byte)", "binaer")
        self.modus.addItem("Text (Zeilenende LF ↔ CR LF)", "text")
        self.modus.addItem("Automatik nach Endung (.txt .c .h .mac .asm .doc = Text)", "automatik")
        self.namen_senden = QComboBox()
        self.namen_empfang = QComboBox()
        for c in (self.namen_senden, self.namen_empfang):
            c.addItem("Literal (Name bleibt, wie er ist)", "literal")
            c.addItem("Konvertiert (Kleinschrift/Punkt wie C-Kermit)", "konvertiert")
        self.pruefsumme = QComboBox()
        for n, t in ((1, "1 — 6 Bit"), (2, "2 — 12 Bit"), (3, "3 — CRC-16 (empfohlen)")):
            self.pruefsumme.addItem(t, n)
        self.paketlaenge = QSpinBox()
        self.paketlaenge.setRange(20, 9024)
        self.paketlaenge.setToolTip("Bis 94 normale Pakete; darüber Langpakete (C-Kermit 4.2 kann keine)")
        self.paket_frist = QSpinBox()
        self.paket_frist.setRange(1, 600)
        self.paket_frist.setSuffix(" s")
        self.wiederholungen = QSpinBox()
        self.wiederholungen.setRange(1, 100)
        self.login_frist = QSpinBox()
        self.login_frist.setRange(1, 600)
        self.login_frist.setSuffix(" s")
        self.tempo = QComboBox()
        self.tempo.addItem("Automatisch (aus der Antwortzeit geschätzt)", 0.0)
        for t in (0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0):
            self.tempo.addItem(f"{t:g}x", t)
        self.tempo.setToolTip("Läuft der Emulator schneller oder langsamer als Echtzeit (z. B. P8000 mit 10-fachem "
                              "Takt), laufen die Fristen der Gegenstelle in Maschinenzeit — 'Automatisch' misst das.")
        self.max_namen = QSpinBox()
        self.max_namen.setRange(0, 255)
        self.max_namen.setSpecialValueText("keine Grenze")
        self.max_namen.setToolTip("Längste Zielnamen: WEGA 14 — längere werden abgelehnt, nicht gekürzt")
        self.ueberschreiben = QCheckBox("Vorhandene Dateien beim Empfang ersetzen")

        wega = QPushButton("WEGA-Werte")
        wega.setToolTip("Paketlänge 94, Namen höchstens 14 Zeichen, Prüfsumme 3 (ausgehandelt)")
        wega.clicked.connect(lambda: self.setzen({**self.werte(), **konfig.WEGA_EINSTELLUNGEN}))

        f = QFormLayout()
        f.addRow("Übertragungsart", self.modus)
        f.addRow("Namen beim Senden", self.namen_senden)
        f.addRow("Namen beim Empfangen", self.namen_empfang)
        f.addRow("Prüfsumme", self.pruefsumme)
        f.addRow("Paketlänge", self.paketlaenge)
        f.addRow("Frist je Paket", self.paket_frist)
        f.addRow("Wiederholungen je Paket", self.wiederholungen)
        f.addRow("Frist je Anmeldeschritt", self.login_frist)
        f.addRow("Geschwindigkeit der Gegenstelle", self.tempo)
        f.addRow("Längster Zielname", self.max_namen)
        f.addRow("", self.ueberschreiben)
        f.addRow("", wega)

        knoepfe = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        knoepfe.accepted.connect(self.accept)
        knoepfe.rejected.connect(self.reject)
        lay = QVBoxLayout(self)
        lay.addLayout(f)
        lay.addWidget(knoepfe)
        self.setzen(einst)

    def setzen(self, e: dict) -> None:
        e = konfig.einstellungen_aus(e)
        for box, schl in ((self.modus, "modus"), (self.namen_senden, "namen_senden"),
                          (self.namen_empfang, "namen_empfang"), (self.pruefsumme, "pruefsumme")):
            box.setCurrentIndex(max(0, box.findData(e[schl])))
        self.paketlaenge.setValue(e["paketlaenge"])
        self.paket_frist.setValue(e["paket_frist_s"])
        self.wiederholungen.setValue(e["wiederholungen"])
        self.login_frist.setValue(e["login_frist_s"])
        i = self.tempo.findData(e["tempo"])
        if i < 0:                           # ein Wert aus der Konfiguration, den die Liste nicht kennt
            self.tempo.addItem(f"{e['tempo']:g}x", e["tempo"])
            i = self.tempo.count() - 1
        self.tempo.setCurrentIndex(i)
        self.max_namen.setValue(e["max_namen"])
        self.ueberschreiben.setChecked(e["ueberschreiben"])

    def werte(self) -> dict:
        return {"modus": self.modus.currentData(),
                "namen_senden": self.namen_senden.currentData(),
                "namen_empfang": self.namen_empfang.currentData(),
                "pruefsumme": self.pruefsumme.currentData(),
                "paketlaenge": self.paketlaenge.value(),
                "paket_frist_s": self.paket_frist.value(),
                "wiederholungen": self.wiederholungen.value(),
                "login_frist_s": self.login_frist.value(),
                "tempo": float(self.tempo.currentData()),
                "max_namen": self.max_namen.value(),
                "ueberschreiben": self.ueberschreiben.isChecked()}

"""Schiebeschalter der zwei Bildschirmkarten (Entwurf 33 §3.3, N6).

Links K7024 (24x80), rechts K7023 (16x64).  Er ist nur aktiv, wenn beide Karten
gesteckt sind (``Bildschirmkarte: K7024 und K7023 mit Umschalter``); sonst ausgegraut,
seine Stellung bleibt gemerkt und hat keine Wirkung.  Das Widget kennt den Kern nicht:
es meldet nur ``stellungGeaendert``; das Umlegen im Kern (unter der Kernsperre) macht
das Hauptfenster.
"""

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import QHBoxLayout, QLabel, QSlider, QWidget

#: Stellungen in Schieberichtung: links = 0, rechts = 1.
STELLUNGEN = ("k7024", "k7023")
TEXT_LINKS = "K7024\n24x80"
TEXT_RECHTS = "K7023\n16x64"


class BildschirmSchalter(QWidget):
    #: Der Anwender hat umgelegt: ``"k7024"`` oder ``"k7023"``.
    stellungGeaendert = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setToolTip("Schiebeschalter der Bildschirmkarten: links die K7024 (24x80), "
                        "rechts die K7023 (16x64).  Wirkt sofort, ohne Neustart; nur aktiv, "
                        "wenn beide Karten gesteckt sind.")
        self.links = QLabel(TEXT_LINKS, self)
        self.rechts = QLabel(TEXT_RECHTS, self)
        for l in (self.links, self.rechts):
            l.setAlignment(Qt.AlignCenter)
        self.regler = QSlider(Qt.Horizontal, self)
        self.regler.setRange(0, 1)
        self.regler.setPageStep(1)
        self.regler.setSingleStep(1)
        self.regler.setFixedWidth(64)
        self.regler.valueChanged.connect(self._geaendert)
        zeile = QHBoxLayout(self)
        zeile.setContentsMargins(0, 0, 0, 0)
        zeile.addWidget(self.links)
        zeile.addWidget(self.regler)
        zeile.addWidget(self.rechts)
        zeile.addStretch(1)
        self._still = False

    def _geaendert(self, wert: int):
        if not self._still:
            self.stellungGeaendert.emit(STELLUNGEN[wert])

    def stellung(self) -> str:
        return STELLUNGEN[self.regler.value()]

    def set_stellung(self, karte: str):
        """Stellung setzen, ohne Signal (unbekannt = K7024)."""
        self._still = True
        try:
            self.regler.setValue(1 if str(karte).lower() == "k7023" else 0)
        finally:
            self._still = False

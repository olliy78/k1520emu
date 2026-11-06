"""Alle Aktionen des Druckerfensters an EINER Stelle (Muster ``app/kermittool/ui/actions.py``).

Menue, Symbolleiste und Bedienfeld-Menue zeigen dieselbe ``QAction``.  ``erzeuge_aktionen(fenster)``
haengt sie als ``fenster.act_<name>`` an und verdrahtet jede mit der gleichnamigen Methode.

**Kuerzel nur mit ``Strg+Umschalt``** (Ausnahme ``F11``): wie bei Emulator, DiskTool und Kermit; die
Kuerzeltabelle des Handbuchs ist ein Vertrag, den Waechter in beiden Richtungen pruefen.
"""

from __future__ import annotations

from typing import List, Tuple

from PySide6.QtGui import QAction, QKeySequence

from app.ui_icons import icon

#: (Name, Beschriftung, Symbol, Kuerzel, Statustext, Methode des Fensters, rastend)
_SPEC: List[Tuple] = [
    # ── Datei ───────────────────────────────────────────────────────────────
    ("pdf", "Blatt als &PDF speichern…", "save-as", "Ctrl+Shift+D",
     "Das Blatt als PDF-Datei speichern (eine Seite je Formular, Punkte als Kreise)", "_pdf_speichern", False),
    ("drucken", "Blatt &drucken…", "save", "Ctrl+Shift+P",
     "Das Blatt über die Druckfunktion des Betriebssystems ausdrucken", "_drucken", False),
    ("beenden", "&Beenden", None, "Ctrl+Shift+Q", "K1520 Printer beenden", "close", False),

    # ── Drucker ─────────────────────────────────────────────────────────────
    ("power", "&Netz ein", "power", "Ctrl+Shift+N",
     "Netzschalter: Aus hält den Drucker an (das Blatt bleibt), Ein startet ihn neu", "_netz_aktion", True),
    ("reset", "Neu&start (Netz aus und ein)", "reset", "Ctrl+Shift+R",
     "Drucker neu starten (/RESET); die Firmware liest ONLINE, 6/8 lpi und Papier dabei neu", "_neustart", False),
    ("online", "&ONLINE", None, "Ctrl+Shift+O",
     "Taste ONLINE (rastend).  TEST wirkt nur, wenn der Drucker offline eingeschaltet wurde", "_online_aktion", True),
    ("pv1", "Papiervorschub &PV1 (eine Zeile)", None, None,
     "PV1 kurz tippen: eine Zeile vor (nur offline)", "_pv1_tippen", False),
    ("pv2", "Papiervorschub P&V2 (Bahn 2)", None, None,
     "PV2 kurz tippen (der SD 1157/264 hat nur eine Bahn)", "_pv2_tippen", False),
    ("test", "&TEST (Selbsttest)", None, None,
     "TEST kurz tippen - wirkt nur nach einem Offline-Einschalten (ONLINE aus, dann Neustart)", "_test_tippen", False),
    ("blatt_leeren", "Blatt &leeren", "delete", "Ctrl+Shift+L",
     "Das Blatt leeren (Papierlage bleibt); vorher ggf. als PDF speichern", "_blatt_leeren", False),

    # ── Ansicht ─────────────────────────────────────────────────────────────
    ("einpassen", "Blatt &einpassen", "reset-view", "Ctrl+Shift+B",
     "Das Blatt auf die Fensterbreite einpassen", "_einpassen_aktion", True),
    ("zoom_ein", "Z&oom +", None, None, "Vergrößern (auch Strg + Mausrad)", "_zoom_plus", False),
    ("zoom_aus", "Zoo&m -", None, None, "Verkleinern (auch Strg + Mausrad)", "_zoom_minus", False),
    ("folgen", "Blatt &folgen", "refresh", "Ctrl+Shift+F",
     "Das Blatt schiebt sich beim Druck nach oben: die Kopfzeile bleibt im Bild", "_folgen_aktion", True),

    # ── Hilfe ───────────────────────────────────────────────────────────────
    ("hilfe", "&Handbuch…", "help", "Ctrl+Shift+H", "Bedienung, Begriffe und Tastenkürzel", "open_help", False),
    ("ueber", "Ü&ber K1520 Printer…", None, None, "Fassung und Herkunft", "_ueber_dialog", False),
]

#: Beschriftung in der Symbolleiste.
KURZ = {"pdf": "PDF", "drucken": "Drucken", "power": "Netz", "reset": "Neustart", "online": "ONLINE",
        "blatt_leeren": "Leeren", "einpassen": "Einpassen", "folgen": "Folgen", "hilfe": "Handbuch"}

#: Inhalt der Symbolleiste ("" = Trennstrich).
LEISTE = ["pdf", "drucken", "", "power", "reset", "online", "", "blatt_leeren", "", "einpassen", "folgen", "", "hilfe"]


def erzeuge_aktionen(fenster) -> None:
    for name, text, bild, kuerzel, tipp, methode, rastend in _SPEC:
        a = QAction(text, fenster)
        if bild:
            a.setIcon(icon(bild))
        if kuerzel:
            a.setShortcut(QKeySequence(kuerzel))
        a.setStatusTip(tipp)
        a.setToolTip(tipp)
        if name in KURZ:
            a.setIconText(KURZ[name])
        a.setCheckable(rastend)
        ziel = getattr(fenster, methode)
        if rastend:
            a.toggled.connect(ziel)
        else:
            a.triggered.connect(lambda *_, f=ziel: f())
        setattr(fenster, f"act_{name}", a)


def namen() -> List[str]:
    return [s[0] for s in _SPEC]

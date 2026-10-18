"""Alle Aktionen des Hauptfensters an EINER Stelle (Muster ``app/disktool/ui/actions.py``).

Menue, Symbolleiste und Kontextmenues zeigen dieselbe ``QAction``.  ``erzeuge_aktionen(fenster)``
haengt sie als ``fenster.act_<name>`` an und verdrahtet jede mit der gleichnamig eingetragenen
Methode des Fensters.

**Kuerzel nur mit ``Strg+Umschalt``** (kein Rechner, dem eine Taste gehoerte — aber dieselbe Regel
wie bei den Nachbarprogrammen, und die Kuerzeltabelle des Handbuchs ist ein Vertrag, den zwei
Waechter in beiden Richtungen pruefen).
"""

from __future__ import annotations

from typing import List, Tuple

from PySide6.QtGui import QAction, QKeySequence

from app.ui_icons import icon

#: (Name, Beschriftung, Symbol, Kuerzel, Statustext, Methode des Fensters, rastend)
_SPEC: List[Tuple] = [
    # ── Datei ───────────────────────────────────────────────────────────────
    ("ordner", "&Lokalen Ordner wählen…", "folder", None,
     "Den lokalen Ordner der rechten Seite wählen", "_ordner_dialog", False),
    ("beenden", "&Beenden", None, "Ctrl+Shift+Q", "K1520 Kermit beenden", "close", False),

    # ── Verbindung ──────────────────────────────────────────────────────────
    ("verbinden", "Als &Client verbinden…", "serial", "Ctrl+Shift+K",
     "Eine Verbindung zu einem Kermit-Server herstellen (Telnet/RFC 2217, verbinden oder lauschen)",
     "_verbinden_dialog", False),
    ("trennen", "&Trennen", None, "Ctrl+Shift+T",
     "Die Leitung schließen bzw. das Lauschen beenden", "_trennen", False),
    ("gegenstelle_beenden", "Server der &Gegenstelle beenden (Finish)", None, None,
     "Den Kermit-Server der Gegenstelle beenden (G F); auf der WEGA kehrt die Anmeldung zurück",
     "_gegenstelle_beenden", False),
    ("server_einrichten", "Als S&erver einrichten…", None, None,
     "Leitung und freigegebenen Ordner für den Server-Betrieb festlegen", "_server_einrichten", False),
    ("server_ordner", "&Ordner freigeben…", "folder", None,
     "Den Ordner wählen, den die Gegenstelle sehen darf (Pflicht vor dem Start)", "_server_ordner", False),
    ("server_start", "Server &starten", "in-all", "Ctrl+Shift+S",
     "Den Server starten: der freigegebene Ordner steht der Gegenstelle zur Verfügung",
     "_server_start_aktion", False),
    ("server_stopp", "Server s&toppen", "delete", None,
     "Den Server beenden und die Leitung schließen", "_server_stopp", False),
    ("nur_lesen", "&Nur lesen", "lock", None,
     "Die Gegenstelle darf nichts schreiben oder löschen (gilt ab dem nächsten Start)",
     "_nur_lesen_umgeschaltet", True),
    ("wieder_lauschen", "Nach der Trennung &wieder lauschen", None, None,
     "Ein Lauscher nimmt nur EINE Verbindung an: mit diesem Haken lauscht er nach deren Ende erneut",
     "_wieder_lauschen_umgeschaltet", True),
    ("abbrechen", "Übertragung &abbrechen", None, None,
     "Die laufende Übertragung geordnet beenden (die Verbindung bleibt benutzbar)", "_abbrechen_aktion", False),

    # ── Bearbeiten ──────────────────────────────────────────────────────────
    ("holen", "In den Ordner &holen", "out", "Ctrl+Shift+Right",
     "Die ausgewählten Dateien vom Server in den lokalen Ordner holen", "_holen_auswahl", False),
    ("schreiben", "Zum Server &schreiben", "in", "Ctrl+Shift+Left",
     "Die ausgewählten Dateien des lokalen Ordners zum Server senden", "_senden_auswahl", False),
    ("loeschen", "Auf dem Server &löschen", "delete", "Ctrl+Shift+Del",
     "Die ausgewählten Dateien auf dem Server löschen (mit Rückfrage)", "_loeschen_auswahl", False),
    ("hinauf", "Server: eine Ebene &höher", None, None,
     "Im Verzeichnis des Servers eine Ebene hinaufgehen (G C ..)", "_hinauf", False),

    # ── Ansicht ─────────────────────────────────────────────────────────────
    ("aktualisieren", "&Aktualisieren", "refresh", "Ctrl+Shift+R",
     "Verzeichnis des Servers und lokalen Ordner neu einlesen", "_aktualisieren", False),
    ("einstellungen", "&Einstellungen…", "settings", "Ctrl+Shift+E",
     "Text/Binär, Namen, Prüfsumme, Paketlänge, Fristen", "_einstellungen_dialog", False),

    # ── Hilfe ───────────────────────────────────────────────────────────────
    ("hilfe", "&Handbuch…", "help", "Ctrl+Shift+H", "Bedienung, Begriffe und Tastenkürzel",
     "open_help", False),
    ("ueber", "Ü&ber K1520 Kermit…", None, None, "Fassung und Herkunft", "_ueber_dialog", False),
]

#: Beschriftung in der Symbolleiste.
KURZ = {
    "ordner": "Ordner", "verbinden": "Verbinden", "trennen": "Trennen", "server_einrichten": "Server",
    "server_ordner": "Freigabe", "server_start": "Start", "server_stopp": "Stopp",
    "nur_lesen": "Nur lesen", "wieder_lauschen": "Neu lauschen", "abbrechen": "Abbruch",
    "holen": "Holen", "schreiben": "Schreiben", "loeschen": "Löschen", "hinauf": "Hinauf",
    "aktualisieren": "Neu lesen", "einstellungen": "Einstellungen", "hilfe": "Handbuch",
    "gegenstelle_beenden": "Finish", "beenden": "Beenden", "ueber": "Über",
}


def erzeuge_aktionen(fenster) -> None:
    """Die Aktionen anlegen, verdrahten und als ``fenster.act_<name>`` ablegen."""
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
    """Alle Aktionsnamen (fuer Tests und die Pruefung der Symbolleistenliste)."""
    return [s[0] for s in _SPEC]

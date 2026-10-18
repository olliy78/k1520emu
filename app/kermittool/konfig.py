"""Konfiguration von K1520 Kermit — ``kermit.yaml`` im Konfigurationsordner.

Gleicher Aufbau und gleiche Schutzregeln wie bei den Nachbarprogrammen
(:mod:`app.config_io`): oberste Ebene ist eine Abbildung, ``version`` ist die
FORMATversion, ``geschrieben_von`` nur Auskunft; eine Datei mit hoeherer
Formatversion wird nicht ueberschrieben.

**Kennwoerter stehen NIE in der Datei.**  Ein Kennwort lebt nur im Verbindungsdialog
und in der laufenden Sitzung; :func:`ohne_kennwort` entfernt jeden Schluessel
dieses Namens, bevor etwas geschrieben wird — auch aus der Liste der zuletzt
benutzten Verbindungen.
"""

from __future__ import annotations

import copy
import os
from typing import List, Optional

from app import config_io, paths

#: Dateiname im Konfigurationsordner (:func:`app.paths.config_dir`).
KONFIG_DATEI = "kermit.yaml"

#: So viele zuletzt benutzte Verbindungen werden gemerkt.
ZULETZT_MAX = 10

#: Dateiendungen, die im Modus „Automatik" als Text gelten.
TEXT_ENDUNGEN = (".txt", ".c", ".h", ".mac", ".asm", ".doc")

#: Uebertragungsvorgaben (Einstellungsdialog).  Schluessel = Schluessel in ``kermit.yaml``.
EINSTELLUNGEN_VORGABE = {
    "modus": "binaer",              # binaer | text | automatik
    "namen_senden": "literal",      # literal | konvertiert
    "namen_empfang": "konvertiert",
    "pruefsumme": 3,                # 1..3, ausgehandelt (die Gegenstelle darf herunterstufen)
    "paketlaenge": 94,              # <= 94 normale Pakete, darueber Langpakete
    "paket_frist_s": 10,
    "wiederholungen": 10,
    "login_frist_s": 60,
    "max_namen": 0,                 # 0 = keine Grenze; WEGA: 14
    "ueberschreiben": True,
    "tempo": 0.0,                   # Geschwindigkeit der Gegenstelle: 0 = Automatik, sonst Faktor (10 = Emulator mit 10x Takt)
}

#: Was die WEGA-Voreinstellungen an den Uebertragungsvorgaben aendern (C-Kermit 4.2:
#: keine Langpakete, Namen hoechstens 14 Zeichen, siehe doc/p8000/kermit_wega.md).
WEGA_EINSTELLUNGEN = {"paketlaenge": 94, "max_namen": 14, "pruefsumme": 3,
                      "namen_senden": "literal", "namen_empfang": "konvertiert"}

#: Verbindungsvorgabe (Felder des Verbindungsdialogs).
VERBINDUNG_VORGABE = {
    "betriebsart": "client",        # client | server
    "leitung": "telnet",            # telnet | rfc2217
    "aufbau": "verbinden",          # verbinden | lauschen
    "host": "127.0.0.1",
    "port": 5000,
    "baud": 9600,
    "login": False,
    "login_name": "",
}

#: Eingebaute Voreinstellungen: (Anzeigename, Verbindung, Kennwort der Sitzung, Uebertragungsaenderungen).
#: Das Kennwort der WEGA-Voreinstellung ist das dokumentierte des Laborrechners
#: (doc/p8000/kermit_wega.md §1) — es steht nur hier, nie in der Konfigurationsdatei.
VOREINSTELLUNGEN = [
    ("WEGA 3.1 (Benutzer kermit)",
     {"betriebsart": "client", "leitung": "telnet", "aufbau": "verbinden",
      "host": "127.0.0.1", "port": 5000, "baud": 9600,
      "login": True, "login_name": "kermit"},
     "kermit", WEGA_EINSTELLUNGEN),
    ("WEGA als Client (hier lauschen, Kermit auf tty3)",
     {"betriebsart": "server", "leitung": "telnet", "aufbau": "lauschen",
      "host": "", "port": 5001, "baud": 9600,
      "login": False, "login_name": ""},
     "", WEGA_EINSTELLUNGEN),
]

#: Vorgabe der Symbolleiste (Aktionsnamen, ``""`` = Trennstrich).
LEISTE_VORGABE = ["verbinden", "trennen", "", "holen", "schreiben", "loeschen", "", "aktualisieren",
                  "", "server_start", "server_stopp"]


def pfad() -> str:
    """Pfad der Konfigurationsdatei (``K1520_KERMIT_CONFIG`` hat Vorrang, fuer Tests und Mehrfachbetrieb)."""
    von_hand = os.environ.get("K1520_KERMIT_CONFIG")
    if von_hand:
        return von_hand
    return os.path.join(str(paths.config_dir()), KONFIG_DATEI)


def ohne_kennwort(daten):
    """Tiefe Kopie von *daten* ohne jeden Schluessel ``kennwort*`` (rekursiv)."""
    if isinstance(daten, dict):
        return {k: ohne_kennwort(v) for k, v in daten.items()
                if not str(k).lower().startswith("kennwort")}
    if isinstance(daten, list):
        return [ohne_kennwort(v) for v in daten]
    return copy.deepcopy(daten)


def einstellungen_aus(daten: Optional[dict]) -> dict:
    """Uebertragungsvorgaben aus einem Abschnitt, mit Vorgabe fuer Fehlendes und Unbrauchbares."""
    e = dict(EINSTELLUNGEN_VORGABE)
    if not isinstance(daten, dict):
        return e
    for k, vorgabe in EINSTELLUNGEN_VORGABE.items():
        if k not in daten:
            continue
        w = daten[k]
        if isinstance(vorgabe, bool):
            e[k] = bool(w)
        elif isinstance(vorgabe, float):
            try:
                e[k] = float(w)
            except (TypeError, ValueError):
                pass
        elif isinstance(vorgabe, int):
            try:
                e[k] = int(w)
            except (TypeError, ValueError):
                pass
        elif isinstance(w, str):
            e[k] = w
    if e["modus"] not in ("binaer", "text", "automatik"):
        e["modus"] = "binaer"
    for k in ("namen_senden", "namen_empfang"):
        if e[k] not in ("literal", "konvertiert"):
            e[k] = EINSTELLUNGEN_VORGABE[k]
    e["pruefsumme"] = min(3, max(1, e["pruefsumme"]))
    e["paketlaenge"] = min(9024, max(20, e["paketlaenge"]))
    e["paket_frist_s"] = max(1, e["paket_frist_s"])
    e["wiederholungen"] = max(1, e["wiederholungen"])
    e["login_frist_s"] = max(1, e["login_frist_s"])
    e["max_namen"] = max(0, e["max_namen"])
    e["tempo"] = e["tempo"] if 0.0 <= e["tempo"] <= 1000.0 else 0.0
    return e


def verbindung_aus(daten: Optional[dict]) -> dict:
    """Verbindungsfelder aus einem Abschnitt, mit Vorgabe fuer Fehlendes und Unbrauchbares."""
    v = dict(VERBINDUNG_VORGABE)
    if not isinstance(daten, dict):
        return v
    for k, vorgabe in VERBINDUNG_VORGABE.items():
        if k not in daten:
            continue
        w = daten[k]
        try:
            if isinstance(vorgabe, bool):
                v[k] = bool(w)
            elif isinstance(vorgabe, int):
                v[k] = int(w)
            else:
                v[k] = str(w)
        except (TypeError, ValueError):
            pass
    if v["betriebsart"] not in ("client", "server"):
        v["betriebsart"] = "client"
    if v["leitung"] not in ("telnet", "rfc2217"):
        v["leitung"] = "telnet"
    if v["aufbau"] not in ("verbinden", "lauschen"):
        v["aufbau"] = "verbinden"
    v["port"] = min(65535, max(0, v["port"]))
    return v


def laden(datei: Optional[str] = None) -> dict:
    """Die Konfiguration lesen; fehlt oder bricht die Datei, kommt ``{}`` (eingebaute Vorgaben)."""
    datei = datei or pfad()
    if not os.path.isfile(datei):
        return {}
    try:
        return config_io.load_config(datei)
    except Exception as e:                      # defektes YAML, Leserechte ...
        print(f"[kermit] Konfiguration {datei} nicht lesbar: {e}")
        return {}


def schreiben(daten: dict, datei: Optional[str] = None) -> bool:
    """Schreibt *daten* (Kennwoerter entfernt) mit ``version``/``geschrieben_von``; ``False`` = nichts geschrieben."""
    from app import version as _version
    datei = datei or pfad()
    ganz = {"version": config_io.CONFIG_VERSION,
            "geschrieben_von": _version.fassung()}
    ganz.update(ohne_kennwort(daten))
    try:
        return config_io.save_config(datei, ganz)
    except OSError as e:
        print(f"[kermit] Konfiguration {datei} nicht geschrieben: {e}")
        return False


def zuletzt_merken(liste: List[dict], verbindung: dict) -> List[dict]:
    """*verbindung* (ohne Kennwort) nach vorn in die Liste; gleiche Gegenstelle ersetzt den alten Eintrag."""
    neu = ohne_kennwort(verbindung)

    def schluessel(v):
        return (v.get("betriebsart"), v.get("leitung"), v.get("aufbau"), v.get("host"), v.get("port"))

    rest = [v for v in liste if schluessel(v) != schluessel(neu)]
    return ([neu] + rest)[:ZULETZT_MAX]


def anzeigename(v: dict) -> str:
    """Kurzer Text fuer die Verbindungsliste."""
    art = "Telnet" if v.get("leitung") == "telnet" else "RFC 2217"
    rolle = "Server" if v.get("betriebsart") == "server" else "Client"
    if v.get("aufbau") == "lauschen":
        return f"{rolle}, {art}, lauscht auf Port {v.get('port')}"
    return f"{rolle}, {art}, {v.get('host')}:{v.get('port')}"


def text_modus(modus: str, dateiname: str) -> bool:
    """Gilt *dateiname* beim Modus *modus* als Text?  (``automatik`` = nach der Endung.)"""
    if modus == "text":
        return True
    if modus == "automatik":
        return dateiname.lower().endswith(TEXT_ENDUNGEN)
    return False

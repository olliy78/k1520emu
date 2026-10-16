"""
K1520 Emulator - Modellwahl des A5120 Emulators (A5120 / A5130 / K8924)
=======================================================================

Single source of truth fuer die wahlbaren Modelle, geteilt vom Programmprofil
(`app/profil.py`), der Einstellungen-Auswahl und der Konfiguration (Abschnitt
``general.model``).  Entwurf: doc/design/33_a5130_k8924.md §3a.

Ein Modell ist NUR eine Sammlung von Vorgaben: die Wahl uebernimmt Werte fuer
Bildschirmkarte, RAM, Tastatur, Lade-ROM, Erweiterungsmodul, Kassette und
Laufwerke; danach ist jede Option einzeln aenderbar.  Die frueheren Schluessel
(``a5120.16``, ``a5120-alt``, ``a5120-kmbg``, ``a5120-bcu``) gibt es nicht mehr; ein
gespeicherter unbekannter Wert wird zur Vorgabe, nicht umgesetzt.
"""

#: Interne Modellschluessel — stehen so in der Konfiguration (``general.model``).
A5120 = "a5120"
A5130 = "a5130"
K8924 = "k8924"

#: Vorgabe: der A5120.
DEFAULT_MODEL = A5120

#: (Schluessel, Anzeigename, Fenstertitel)
MODELS = [
    (A5120, "A5120", "A5120 Emulator"),
    (A5130, "A5130", "A5130 Emulator"),
    (K8924, "K8924", "K8924 Emulator"),
]

#: Bestueckung der Vorgaben, ein Eintrag je Schluessel der Optionen (Werte wie im
#: Konfigurationstext des Kerns, `A5120_KONFIG_WERTE` in `app/core_binding/k1520.py`):
#: ``ram``, ``bildschirm``, ``tastatur``, ``laderom``, ``erweiterungsmodul`` (``none``/
#: ``em064``/``em256``), ``kassette`` (``0``/``1``) und ``laufwerke`` (K5122-Steckplaetze).
_K5601 = "K5601"
_LEER = "none"
VORGABEN = {
    A5120: {"ram": "k3526", "bildschirm": "k7024", "tastatur": "k7637", "laderom": "v09_2",
            "erweiterungsmodul": "none", "kassette": "0",
            "laufwerke": (_K5601, _K5601, _K5601, _LEER)},
    A5130: {"ram": "k3526", "bildschirm": "k7024", "tastatur": "k7636", "laderom": "v09_2",
            "erweiterungsmodul": "none", "kassette": "0",
            "laufwerke": (_K5601, _K5601, _K5601, _K5601)},
    K8924: {"ram": "k3526", "bildschirm": "k7024", "tastatur": "k7637", "laderom": "v09_2",
            "erweiterungsmodul": "none", "kassette": "0",
            "laufwerke": (_K5601, _K5601, _K5601, _LEER)},
}

_LABEL_FOR = {schluessel: label for schluessel, label, _t in MODELS}
_TITEL_FOR = {schluessel: titel for schluessel, _l, titel in MODELS}


def normalize(model) -> str:
    """Ein bekannter Modellschluessel — alles Unbekannte/Fehlende wird die Vorgabe.

    Auch die frueheren Schluessel (``a5120.16`` …) sind unbekannt: keine Umsetzung.
    """
    model = str(model).strip().lower() if model else ""
    return model if model in VORGABEN else DEFAULT_MODEL


def label(model) -> str:
    """Anzeigename fuer die Auswahl."""
    return _LABEL_FOR[normalize(model)]


def titel(model) -> str:
    """Fenstertitel des Modells („K8924 Emulator“)."""
    return _TITEL_FOR[normalize(model)]


def vorgabe(model) -> dict:
    """Kopie der Vorgaben des Modells."""
    return dict(VORGABEN[normalize(model)])


# ── PRG 710 / PRG 710-1 (prg710emu) ─────────────────────────────────────────
# Dort ist die Modellwahl keine Erweiterung, sondern die Maschine selbst; die
# Zuordnung Schluessel → Kern steht im Programmprofil (`app/profil.py`), hier nur
# die Schluessel, damit Konfiguration und Tests dieselben Namen benutzen.
PRG710 = "prg710"
PRG710_1 = "prg710-1"

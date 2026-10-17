"""`app/modell.py` — Modelle des A5120 Emulators (A5120/A5130/K8924).

Reine Python-Logik ohne Kern und ohne Qt: die eine Stelle, die einen Modellschluessel
(``general.model`` der Konfiguration) auf Anzeigename, Fenstertitel und Vorgaben abbildet
(Entwurf 33 §3a).
"""

import app.modell as modell


def test_default_model_is_a5120():
    assert modell.DEFAULT_MODEL == modell.A5120


def test_normalize_falls_back_to_the_default():
    """Aeltere/fremde Konfigurationen und die frueheren Schluessel: Vorgabe, keine Umsetzung."""
    for wert in (None, "", "   ", "unbekannt", "EM256", 42,
                 "a5120.16", "a5120-alt", "a5120-kmbg", "a5120-bcu"):
        assert modell.normalize(wert) == modell.DEFAULT_MODEL


def test_normalize_is_case_insensitive_for_known_keys():
    assert modell.normalize("A5130") == modell.A5130
    assert modell.normalize(" k8924 ") == modell.K8924


def test_every_model_has_label_title_and_complete_defaults():
    optionen = {"ram", "bildschirm", "tastatur", "laderom", "erweiterungsmodul",
                "kassette", "aup", "laufwerke"}
    for schluessel, beschriftung, titel in modell.MODELS:
        assert modell.label(schluessel) == beschriftung
        assert modell.titel(schluessel) == titel == f"{beschriftung} Emulator"
        assert set(modell.vorgabe(schluessel)) == optionen
        assert len(modell.vorgabe(schluessel)["laufwerke"]) == 4


def test_vorgabe_ist_eine_kopie():
    modell.vorgabe(modell.A5120)["ram"] = "x"
    assert modell.vorgabe(modell.A5120)["ram"] == "k3526"

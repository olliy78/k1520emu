"""Konfiguration von K1520 Printer - ``k1520printer.yaml`` im Konfigurationsordner.

Gleiche Schutzregeln wie bei den Nachbarprogrammen (:mod:`app.config_io`): oberste Ebene ist
eine Abbildung, ``version`` ist die FORMATversion, eine Datei mit hoeherer Formatversion wird
nicht ueberschrieben.  Mehrinstanz (``--instance``/``--config``) wie die Emulatoren.

Abschnitte::

    general:   {model, speed}                   Drucker und Tempo
    printer:   {rom, schnittstelle, online, formular_zoll, zoom, folgen, einpassen}
    schnittstellen: {...}                      Zustand der SerialWidget-Bloecke
    window:    {width, height, dock_state}
"""

from __future__ import annotations

import os
from typing import Optional

from app import config_io, instanz, paths, version as _version

#: Dateiname im Konfigurationsordner (:func:`app.paths.config_dir`).
KONFIG_DATEI = "k1520printer.yaml"
#: Auslieferungsvorgabe (``data/`` bzw. ``share/k1520emu/``).
VORGABE_DATEI = "default_config_printer.yaml"

#: Formularlaengen, die der SD 1157 kennt (Zoll, BCH.5..4); die Vorgabe des Kerns ist 12".
FORMULARLAENGEN_ZOLL = (4, 6, 8, 12)

#: Tempo-Stufen (Faktor, 1.0 = Echtzeit); 0.0 = unbegrenzt.
TEMPO_STUFEN = (1.0, 2.0, 5.0, 10.0, 0.0)


def pfad() -> str:
    von_hand = instanz.konfig_pfad_vorgabe()
    if von_hand:
        return von_hand
    return os.path.join(str(paths.config_dir()), instanz.konfig_datei(KONFIG_DATEI))


def standard() -> dict:
    """Die mitgelieferte Vorgabe (``{}``, wenn sie fehlt - sie ist Beigabe, keine Voraussetzung)."""
    p = paths.default_config_file(VORGABE_DATEI)
    if p is None:
        return {}
    try:
        return config_io.load_config(str(p))
    except Exception as e:                      # noqa: BLE001 - defekte Beigabe darf den Start nicht stoppen
        print(f"[config] Vorgabe {p} nicht lesbar: {e}")
        return {}


def laden(p: Optional[str] = None) -> dict:
    """Die Konfiguration des Anwenders; ohne Datei die Auslieferungsvorgabe."""
    p = p or pfad()
    if os.path.isfile(p):
        try:
            return config_io.load_config(p)
        except Exception as e:                  # noqa: BLE001
            print(f"[config] {p} nicht lesbar: {e}")
    return standard()


def speichern(daten: dict, p: Optional[str] = None) -> bool:
    daten = dict(daten)
    daten["version"] = config_io.CONFIG_VERSION
    daten["geschrieben_von"] = _version.fassung()
    return config_io.save_config(p or pfad(), daten)


def zoll_waehlbar(wert) -> int:
    try:
        wert = int(wert)
    except (TypeError, ValueError):
        return 12
    return wert if wert in FORMULARLAENGEN_ZOLL else 12


def tempo_waehlbar(wert) -> float:
    try:
        wert = float(wert)
    except (TypeError, ValueError):
        return 1.0
    return wert if wert in TEMPO_STUFEN else 1.0

"""Programmprofile: A5120 Emulator und K8915 Emulator — ein Programm, zwei Gesichter.

Die Oberfläche (`app/ui/main_window.py`) ist für beide Rechner dieselbe; was sie
unterscheidet, steht **hier** und nirgends sonst (doc/design/18_k8915emu_oberflaeche.md
§2): Maschinentyp des Kerns, Fenstertitel, Name der Konfigurationsdatei und ihrer
Auslieferungsvorgabe, Laufwerksbestückung, Nenntakt, Bildschirmtastatur, Lampen der
Frontplatte und die zusätzlichen Aktionen.  Wer eine Eigenheit einer Maschine
braucht, fragt das Profil — kein ``if machine == …`` quer durch die Oberfläche.

Gewählt wird das Profil beim Start (``app/main.py --machine k8915``, fest
übergeben vom Starter ``run_k8915emu.sh`` bzw. ``bin/k8915emu``); ohne Angabe ist
es der A5120 — so verhalten sich ältere Starter und Tests unverändert.

Das Modul importiert kein Qt: `app/paths.py` und `app/config_io.py` brauchen es
schon, bevor PySide6 geladen ist (``--paths``, ``--help``).
"""

from __future__ import annotations

import dataclasses
from dataclasses import dataclass, field
from typing import Optional, Tuple

from app import modell as _modell
from app import p8000_ram as _p8000_ram
from app import rtc as _rtc


@dataclass(frozen=True)
class Programmprofil:
    """Alles, worin sich die beiden Emulatoren unterscheiden."""

    #: Maschinenname der Bindung (``K1520Emulator(machine=…)``).
    maschine: str
    #: Programmname — Starter, ``.desktop``, Meldungen auf der Konsole.
    programm: str
    #: Fenstertitel, Startmenue-Eintrag, ``.desktop``-Name, Kopf der Startskripte.
    titel: str
    #: Rechner, wie er im Text genannt wird („Der A5120 laeuft mit …").
    rechner: str
    #: Einzeiler fuer „Ueber“ und ``--help``.
    beschreibung: str
    #: Konfiguration des Anwenders in :func:`app.paths.config_dir`.
    konfig_datei: str
    #: Auslieferungsvorgabe (``data/`` bzw. ``share/k1520emu/``).
    vorgabe_datei: str
    #: Nenntakt in Hertz und so, wie ihn der Anwender liest.
    nenntakt_hz: int
    nenntakt_text: str
    #: Bildschirmtastatur: ``"k7637"`` oder ``"k7672"``.
    tastatur: str
    #: Frontplatte mit Lampen in der Statuszeile (K8915: Run … Power).
    frontplatte: bool = False
    #: Kasten „EPROMmer“ mit virtuellem Sockel (PRG 710, doc/design/20_prg710.md AP-P7c).
    eprommer: bool = False
    #: Modellwahl unter *Einstellungen ▸ Allgemein*: A5120 / A5120.16 (Erweiterungs-
    #: modul, `app/modell.py`), K8915 V3 / V2, PRG 710 / PRG 710-1, PC 1715 / 1715W.
    modellwahl: bool = False
    #: Die waehlbaren Modelle: ``(Schluessel, Kern-Maschine, Kern-``em``, Anzeigename,
    #: Tastatur)``.  Leer = ein Modell, die Maschine des Profils.  Der Schluessel steht
    #: in der Konfiguration (``general.model``); ein unbekannter wird zum ersten.
    modelle: Tuple[Tuple[str, str, Optional[str], str, str], ...] = ()
    #: Hinweistext am Auswahlfeld des Modells.
    modell_tipp: str = ""
    #: Abweichender Nenntakt einzelner Modelle: ``(Schluessel, Hertz, Text)``.  Alle
    #: anderen Modelle laufen mit :attr:`nenntakt_hz` (PC 1715W: 3,9936 MHz).
    modell_takte: Tuple[Tuple[str, int, str], ...] = ()
    #: Modelle, die das Profil kennt, der Kern aber noch nicht faehrt:
    #: ``(Schluessel, Anzeigename, Begruendung)``.  Sie stehen ausgegraut im
    #: Auswahlfeld (mit der Begruendung als Hinweis), sind nicht waehlbar und gelten
    #: in einer Konfiguration als unbekannt.  Wer das Modell im Kern freischaltet,
    #: verschiebt die Zeile nach :attr:`modelle`.
    gesperrte_modelle: Tuple[Tuple[str, str, str], ...] = ()
    #: Waehlbare Hardwarevarianten unter *Einstellungen ▸ Allgemein* (Kaltstart noetig, wie die
    #: Modellwahl): ``(Schluessel, Beschriftung, Hinweis, ((Wert, Anzeige), …), Modelle)``.
    #: Der erste Wert ist die Vorgabe (fehlender Schluessel in der Konfiguration = Vorgabe).
    #: ``Modelle`` = die Modellschluessel, an denen die Wahl wirkt; sonst ist das Feld
    #: ausgegraut (PC 1715W: der Zeichensatz kommt von Diskette).  ``()`` = alle.
    hardware: Tuple[Tuple[str, str, str, Tuple[Tuple[str, str], ...], Tuple[str, ...]], ...] = ()
    #: Modelle als VORGABEN (A5120/A5130/K8924, Entwurf 33 §3a): ``(Schluessel, ((Option, Wert), …))``
    #: mit den Optionen ``ram``, ``bildschirm``, ``tastatur``, ``laderom``, ``erweiterungsmodul``,
    #: ``kassette`` und ``laufwerke`` (Tupel der Steckplaetze).  Die Modellwahl UEBERNIMMT diese
    #: Werte; danach ist jede Option einzeln aenderbar.  Leer = Modelle sind Maschinen (alle anderen
    #: Programme).  Optionen mit Auswahlfeld stehen zusaetzlich in :attr:`hardware`.
    modell_vorgaben: Tuple[Tuple[str, Tuple[Tuple[str, object], ...]], ...] = ()
    #: Terminal-Maschine (P8000): statt der Bildroehre ein Textbildschirm je Kern-Terminal
    #: (`app/ui/p8000_terminal.py`), Funktionstastenleiste statt Bildschirmtastatur.
    terminal: bool = False
    #: Kasten „Winchester" mit Plattenabbild (P8000, `app/ui/platten_widget.py`).
    platte: bool = False
    #: Port-Vorschlaege der seriellen Schnittstellen: ``(Schnittstellenname, Port)`` (P8000: tty0 …
    #: tty7 als Server fuer Arbeitsplatz-Terminals, XB5 der Terminaleinheit als Client).
    schnittstellen_ports: Tuple[Tuple[str, int], ...] = ()
    #: Terminalart je Modell (P8000, Entwurf 25 §11): ``(Schluessel, Art)`` mit Art ``"kern"`` (Rechner
    #: mit dem Kern-Terminal), ``"original"`` (Rechner mit dem Originalterminal Typ 2 an tty1) oder
    #: ``"einheit"`` (nur das Terminal, ohne Rechner, Leitung ueber den Hub).  Fehlt ein Modell: ``"kern"``.
    modell_arten: Tuple[Tuple[str, str], ...] = ()
    #: Zwei Auswahlfelder statt einem: *Rechnerausstattung* (die Modelle mit Rechner) und
    #: *Betriebsart* („Computer mit Terminal“ | „nur Terminal“ = das Modell der Art ``"einheit"``).
    #: Der Modellschluessel in der Konfiguration bleibt EINER (bei „nur Terminal“ der der Einheit);
    #: die gemerkte Ausstattung steht zusaetzlich als ``general.ausstattung`` (P23a).
    betriebsart_wahl: bool = False
    #: Fruehere Modellschluessel → heutige (alte Konfigurationen des P8000: die Kern-Terminal-Modelle
    #: ``p8000``/``p8000-16``/``p8000-8`` gibt es in der Oberflaeche nicht mehr).
    modell_alt: Tuple[Tuple[str, str], ...] = ()
    #: Gruppe „RTC" (Uhrenkarte, Entwurf 37 R3) unter *Allgemein*: nur am P8000.  Die beiden
    #: Felder stehen in :attr:`hardware` (Schluessel ``rtc``/``rtc_versatz``), werden aber als
    #: eigene Gruppe mit Anzeige der Gastzeit und Schaltflaeche gebaut.
    rtc_wahl: bool = False
    #: Vorgaben einzelner Hardwarefelder, die NICHT der erste Wert sind: ``(Schluessel, Wert)``.
    hardware_vorgaben: Tuple[Tuple[str, str], ...] = ()
    #: Kaestchen „Lochstreifen" unter *Allgemein* (K6022): am P8000 gibt es die Karte nicht.
    ptape_wahl: bool = True
    #: Kaestchen „Kassette (K5221, AKB K5020)" unter *Allgemein*: wie der Lochstreifen
    #: nicht am P8000 (kein K1520-Bus).
    tape_wahl: bool = True
    #: Option „AUP (Sparkasse)" unter *Allgemein* (AUP 062-8531, Entwurf 33 N10e): nur A5120-Profil.
    aup_wahl: bool = False
    #: Option „K3521 (batteriegepuffertes RAM)" unter *Allgemein* (N10h): nur A5120-Profil.
    k3521_wahl: bool = False
    #: Modelle mit fest eingebauter Kassette: das Kaestchen „Kassette“ ist dort ohne Wirkung
    #: (ausgegraut, der Kasten steht immer da) — MC80.21/.22 (MBI).
    tape_fest: Tuple[str, ...] = ()
    #: Lampen der Frontplatte: ``"k8915"`` (Latch 61H, aktiv low) oder ``"p8000"``
    #: (Run/Unit16/Platte, aktiv high) — nur mit :attr:`frontplatte`.
    frontplatte_art: str = "k8915"
    #: Kernparameter je Modell: ``(Schluessel, ((Name, Wert), …))`` — Eintraege des
    #: Konfigurationstextes (P8000: ``karte16``, ``wdc``).  Leer = keine.
    modell_kern: Tuple[Tuple[str, Tuple[Tuple[str, str], ...]], ...] = ()
    #: Feste Kernparameter einzelner Modelle der Profile ohne :attr:`modell_kern`:
    #: ``(Schluessel, ((Name, Wert), …))`` — zusaetzliche Argumente fuer ``K1520Emulator(…)``
    #: (A5120 „BCU 880“: ``laderom="v06"``, ``tastatur="k7636"``).  Sie gehen der Hardwarewahl vor.
    modell_kernwahl: Tuple[Tuple[str, Tuple[Tuple[str, str], ...]], ...] = ()
    #: Auswahl „RAM-Disk" (RAF 128/512/2M, `app/raf.py`) samt Stand-by-Kaestchen
    #: unter *Einstellungen ▸ Allgemein* (doc/design/22_raf512.md §7).  Alle drei
    #: Programme bieten sie an; ohne sie laeuft die Maschine stets ohne RAF.
    raf_wahl: bool = True
    #: Hinweise, die der Laufwerkskasten beim Einlegen eines ``.cqm`` zeigt (Hinweiszeile,
    #: kein Meldungsfenster): was CopyQM an DIESEM Rechner unbrauchbar macht.  Leer = keine.
    cqm_hinweise: Tuple[str, ...] = ()
    #: Diskettenlaufwerke (K5122 o. ae.): ohne sie (MC80) fallen Kasten „Laufwerke“, die
    #: Diskettenmenues und die Laufwerksfelder der Statuszeile weg.
    laufwerke: bool = True
    #: Satzteil fuer „Ueber …“ (HTML): „des Buerocomputers <b>robotron A5120</b>“.
    ueber_rechner: str = ""
    #: Weitere Emulatoren neben :attr:`andere` (Menue *Werkzeuge*) — Maschinenname
    #: des Profils.
    weitere: Tuple[str, ...] = ()
    #: Aktionen, die nur dieses Programm hat (Namen aus `app/ui/actions.py`).
    eigene_aktionen: Tuple[str, ...] = field(default_factory=tuple)
    #: Die jeweils ANDERE Maschine (fuer *Werkzeuge ▸ … starten*).
    andere: str = ""
    #: Fruehere Namen serieller Schnittstellen → heutiger Name (Kern).  Die
    #: Konfiguration ist ueber den NAMEN verschluesselt; ohne diese Abbildung waeren
    #: Einstellungen unter einem alten Namen herrenlos (K8915: AP-S12).
    alte_schnittstellen: Tuple[Tuple[str, str], ...] = field(default_factory=tuple)

    # ── Modellwahl: Schluessel → Kern-Maschine / EM / Tastatur ───────────────
    # Ein Profil ohne `modelle` hat genau ein Modell: seine Maschine.

    def standard_modell(self) -> str:
        """Schlüssel des Modells ab Werk (das erste; ohne Modellwahl ``a5120``,
        der Schlüssel „kein Erweiterungsmodul“ aus `app/modell.py`)."""
        return self.modelle[0][0] if self.modelle else _modell.DEFAULT_MODEL

    def modell_normalisieren(self, modell) -> str:
        """Ein bekannter Modellschlüssel — Unbekanntes/Fehlendes wird die Vorgabe."""
        modell = str(modell).strip().lower() if modell else ""
        modell = dict(self.modell_alt).get(modell, modell)
        return modell if any(m[0] == modell for m in self.modelle) \
            else self.standard_modell()

    def _modell_zeile(self, modell):
        schluessel = self.modell_normalisieren(modell)
        for m in self.modelle:
            if m[0] == schluessel:
                return m
        return None

    def modell_maschine(self, modell) -> str:
        """Maschinenname für ``K1520Emulator(machine=…)``."""
        z = self._modell_zeile(modell)
        return z[1] if z else self.maschine

    def modell_vorgabe(self, modell) -> dict:
        """Vorgaben des Modells (leer ohne :attr:`modell_vorgaben`): Optionen -> Wert,
        ``laufwerke`` als Liste der Kernprofile."""
        schluessel = self.modell_normalisieren(modell)
        for k, paare in self.modell_vorgaben:
            if k == schluessel:
                aus = dict(paare)
                if "laufwerke" in aus:
                    aus["laufwerke"] = list(aus["laufwerke"])
                return aus
        return {}

    def modell_em(self, modell, hardware=None) -> Optional[str]:
        """``em=``-Parameter des Kerns (``None`` = ohne Erweiterungsmodul).

        Mit :attr:`modell_vorgaben` ist das Erweiterungsmodul eine OPTION: wirksam nur am
        A5120 und nur mit der K3526 (64 KB) — sonst ``None``, auch wenn das Feld etwas anderes
        zeigt (es ist dann ausgegraut, :meth:`hardware_wirkt`)."""
        if self.modell_vorgaben:
            hw = self.hardware_normalisieren(hardware or {}, modell)
            em = hw.get("erweiterungsmodul", "none")
            if em == "none" or not self.hardware_wirkt("erweiterungsmodul", modell, hw):
                return None
            return em
        z = self._modell_zeile(modell)
        return z[2] if z else None

    def modell_nenntakt(self, modell) -> Tuple[int, str]:
        """``(Hertz, Text)`` des Nenntakts des Modells."""
        schluessel = self.modell_normalisieren(modell)
        for k, hz, text in self.modell_takte:
            if k == schluessel:
                return hz, text
        return self.nenntakt_hz, self.nenntakt_text

    def hardware_standard(self, modell=None) -> dict:
        """Vorgabe jeder Hardwarevariante (``{}`` ohne :attr:`hardware`).  Mit
        :attr:`modell_vorgaben` die des Modells *modell* (ohne Angabe: des Standardmodells),
        sonst der erste Wert jeder Auswahl."""
        vorgabe = self.modell_vorgabe(modell or self.standard_modell())
        eigene = dict(self.hardware_vorgaben)
        return {k: str(vorgabe.get(k, eigene.get(k, werte[0][0])))
                for k, _b, _t, werte, _m in self.hardware}

    def hardware_normalisieren(self, daten, modell=None) -> dict:
        """Hardwarewahl aus einem Konfigurationsabschnitt: fehlende oder unbekannte
        Werte werden zur Vorgabe (ältere Konfigurationen) — mit :attr:`modell_vorgaben`
        zur Vorgabe des Modells *modell*.  Jede Option wird einzeln geprüft."""
        daten = daten if isinstance(daten, dict) else {}
        aus = self.hardware_standard(modell)
        for k, _b, _t, werte, _m in self.hardware:
            v = str(daten.get(k, "")).strip().lower()
            for w, _a in werte:        # Gross-/Kleinschreibung egal ("1M@0"), Wert wie im Profil
                if v == w.lower():
                    aus[k] = w
                    break
            if k == "dram":            # P8000: Umzug alter Werte, eigene Bestueckung bleibt
                aus[k] = _p8000_ram.normalisieren(daten.get(k))
        return aus

    def hardware_wirkt(self, schluessel: str, modell, hardware=None) -> bool:
        """Wirkt die Wahl *schluessel* am Modell?  (Sonst im Feld ausgegraut.)

        Das Erweiterungsmodul (A5120.16, Entwurf 33 §3a) wirkt nur am A5120 UND nur mit der
        K3526 als RAM; *hardware* ist die aktuelle Wahl (fehlt sie: Vorgabe des Modells)."""
        for k, _b, _t, _w, modelle in self.hardware:
            if k == schluessel:
                if modelle and self.modell_normalisieren(modell) not in modelle:
                    return False
                if k == "rtc_versatz":
                    # Der Versatz wirkt nur mit gesteckter Karte; der gemerkte Wert bleibt.
                    hw = hardware if hardware is not None else self.hardware_standard(modell)
                    return hw.get("rtc", "keine") != "keine"
                if k == "erweiterungsmodul" and self.modell_vorgaben:
                    hw = hardware if hardware is not None else self.hardware_standard(modell)
                    return hw.get("ram") == "k3526"
                return True
        return False

    def tastatur_bauart(self, modell, hardware=None) -> str:
        """Bauart der Bildschirmtastatur: :meth:`modell_tastatur`, beim PC 1715 mit dem
        QWERTZ-ROM ``"pc1715-tast618"``."""
        if self.modell_vorgaben:
            # A5120/A5130/K8924 (Entwurf 33 N7): die Bauart folgt der OPTION ``tastatur``
            # (k7636 | k7637), nicht dem Modell; die Modellzeile traegt nur die Vorgabe.
            hw = self.hardware_normalisieren(hardware or {}, modell)
            return hw["tastatur"]
        art = self.modell_tastatur(modell)
        if art == "pc1715" and (hardware or {}).get("tastatur") == "tast618":
            return "pc1715-tast618"
        return art

    def kern_parameter(self, modell, hardware=None, schalter=None) -> dict:
        """Zusatzargumente für ``K1520Emulator(…)`` aus Modell und Hardwarewahl.

        Ohne :attr:`modell_kern` (alle Maschinen außer dem P8000) sind es die
        Hardwarevarianten selbst (PC 1715: ``zeichensatz``, ``tastatur``).  Der P8000
        übersetzt beides in EIN Wörterbuch ``p8000={…}`` mit den Schlüsseln von
        ``k1520_create_p8000`` (Entwurf 25 §10.9): das Modell setzt ``karte16``/``wdc``,
        die Hardwarewahl ROM-Fassungen, Platinenindex und DRAM.
        """
        hardware = self.hardware_normalisieren(hardware or {}, modell)
        if self.modell_vorgaben:
            # A5120/A5130/K8924: alles als Einzeloptionen in EIN Woerterbuch ``konfig`` (Entwurf 33
            # N4: nicht mit ausfuehrung/tastatur/laderom/em/tape kombinierbar).  ``kassette``
            # fuegt die Maschinenerzeugung aus dem Kaestchen dazu; ``bildschirm`` ist ein Auswahlfeld (N6).
            konfig = {
                "ram": hardware["ram"],
                "bildschirm": hardware["bildschirm"],
                "tastatur": hardware["tastatur"],
                "laderom": hardware["laderom"],
                "em": self.modell_em(modell, hardware) or "none",
            }
            # Anfangsstellung des Schiebeschalters: nur mit zwei Karten (sonst Fehler im Kern).
            if hardware["bildschirm"] == "beide":
                konfig["schalter"] = schalter if schalter in ("k7024", "k7023") else "k7024"
            return {"konfig": konfig}
        if not self.modell_kern:
            aus = dict(hardware)
            schluessel = self.modell_normalisieren(modell)
            for k, paare in self.modell_kernwahl:
                if k == schluessel:
                    aus.update(dict(paare))
            return aus
        schluessel = self.modell_normalisieren(modell)
        if self.modell_art(schluessel) == "einheit":
            # Terminal ohne Rechner: ROM-Fassungen/Platinenindex gibt es nicht; die Leitung
            # (Hub der Einheit) stellt der Reiter „Schnittstellen“ / der Verbindungsdialog ein.
            return {"p8000": {}}
        kern = {}
        for k, paare in self.modell_kern:
            if k == schluessel:
                kern.update(dict(paare))
        mit16 = kern.get("karte16") == "1"
        mit_wdc = kern.get("wdc", "aus") != "aus"
        # Platinenindex: ein Schalter fuer beide Karten (nur 1/1 oder 3/4 sind zulaessig).
        index = hardware.get("index", "34")
        kern["index8"] = index[0]
        if mit16:
            kern["index16"] = index[1]
            kern["mon16"] = hardware.get("mon16", "3.1")
            kern["dram"] = hardware.get("dram", _p8000_ram.STANDARD)
            if self.rtc_wahl and hardware.get("rtc", "keine") != "keine":
                # Uhrenkarte nur am 16-Bit-Teil (sonst Fehler im Kern); der gemerkte Wert bleibt in
                # der Konfiguration, wirkt aber erst wieder mit 16-Bit-Teil.
                kern["rtc"] = hardware["rtc"]
                kern["rtc_jahresversatz"] = _rtc.versatz_jahre(hardware.get("rtc_versatz"))
                kern["rtc_zustand"] = _rtc.zustand_pfad(self.programm)
        kern["mon8"] = hardware.get("mon8", "3.1")
        if mit_wdc:
            kern["wdc"] = hardware.get("wdc", "4.2")
            kern["plattepar"] = hardware.get("platte_par", "aus")   # P24: Programmvorgabe AUS
        return {"p8000": kern}

    def modell_hat_wdc(self, modell) -> bool:
        """Hat das Modell einen Winchesterkontroller (nur Profile mit :attr:`platte`)?"""
        schluessel = self.modell_normalisieren(modell)
        for k, paare in self.modell_kern:
            if k == schluessel:
                return dict(paare).get("wdc", "aus") != "aus"
        return False

    def schnittstellen_hinweis(self, modell) -> str:
        """Hinweis über den Blöcken des Reiters „Schnittstellen“ (leer = keiner).  Nur P8000."""
        art = self.modell_art(modell)
        if art == "einheit":
            return ("Diese Leitung (XB5) führt zum <b>Rechner</b>: Betriebsart und Rechner-Adresse "
                    "einstellen, dann <i>Verbinden</i> (oder Menü <i>Maschine ▸ Verbindung zum "
                    "Rechner…</i>).  Der Rechner bietet die Leitung als Server an, z. B. tty4 auf "
                    "Port 5004.")
        if self.modell_arten and art in ("original", "kern") and self.modell_normalisieren(modell) != "p8000-8-ot":
            return ("<b>Mehrplatzbetrieb:</b> jede Leitung (tty0, tty2, tty3, tty4–7) kann als "
                    "<i>Server</i> für ein Arbeitsplatz-Terminal laufen — <i>Starten</i> drücken, "
                    "in einem zweiten <b>p8000emu</b> (<i>Betriebsart: nur Terminal</i>, "
                    "Menü <i>Maschine ▸ Verbindung zum Rechner…</i>) dieselbe Adresse und denselben "
                    "Port eintragen.  WEGA belegt "
                    "tty1 (Konsole), 6, 7, 0, 2, 4, 5.  Vorschläge: tty4 → 5004 … tty7 → 5007, "
                    "tty0 → 5000, tty2 → 5002, tty3 → 5003.")
        return ""

    def modell_art(self, modell) -> str:
        """Terminalart des Modells: ``"kern"`` | ``"original"`` | ``"einheit"`` (nur P8000)."""
        schluessel = self.modell_normalisieren(modell)
        for k, art in self.modell_arten:
            if k == schluessel:
                return art
        return "kern"

    def modell_original_terminal(self, modell) -> bool:
        """Zeigt das Modell das Originalterminal (mit Rechner oder allein)?"""
        return self.modell_art(modell) in ("original", "einheit")

    def modell_hat_rechner(self, modell) -> bool:
        """Gehört zum Modell ein Rechner (Laufwerke, Platte, Frontplatte)?  Nein nur an der
        eigenständigen Terminaleinheit."""
        return self.modell_art(modell) != "einheit"

    def ausstattungen(self) -> tuple:
        """Die Modelle MIT Rechner (Auswahl „Rechnerausstattung“), als Zeilen von :attr:`modelle`."""
        return tuple(m for m in self.modelle if self.modell_art(m[0]) != "einheit")

    def einheit_modell(self) -> Optional[str]:
        """Schlüssel des Modells „nur Terminal“ (``None`` ohne ein solches)."""
        for k, art in self.modell_arten:
            if art == "einheit":
                return k
        return None

    def ausstattung_normalisieren(self, wert) -> str:
        """Eine bekannte Rechnerausstattung — Unbekanntes, Fehlendes und das Terminalmodell
        werden zur Vorgabe (der ersten)."""
        k = self.modell_normalisieren(wert) if wert else ""
        z = self.ausstattungen()
        return k if any(m[0] == k for m in z) else (z[0][0] if z else self.standard_modell())

    def modell_titel(self, modell) -> str:
        """Fenstertitel des Modells: die eigenständige Terminaleinheit heißt „P8000 Terminal“."""
        if self.modell_vorgaben:
            return _modell.titel(self.modell_normalisieren(modell))
        if self.modell_art(modell) == "einheit":
            return "P8000 Terminal"
        return "P8000 Emulator" if self.modell_arten else self.titel

    def modell_tastatur(self, modell) -> str:
        """Bildschirmtastatur des Modells (``"k7637"``/``"k7672"``/``"k7609"``)."""
        z = self._modell_zeile(modell)
        return z[4] if z else self.tastatur

    def schnittstellen_umbenennen(self, daten: dict) -> dict:
        """Abschnitt ``schnittstellen`` mit alten Namen auf die heutigen abbilden.

        Ein Eintrag unter dem heutigen Namen geht vor (er ist der jüngere); alles
        andere bleibt, wie es ist.
        """
        if not isinstance(daten, dict) or not self.alte_schnittstellen:
            return daten
        neu = dict(daten)
        for alt, heute in self.alte_schnittstellen:
            if alt in neu:
                wert = neu.pop(alt)
                neu.setdefault(heute, wert)
        return neu

    # Laufwerke stehen in `app/drive_types.py` (je Maschine) — dort ist der
    # Laufwerkskatalog; das Profil reicht nur durch.  Spaet importiert: die
    # Bindung (und damit die Kernbibliothek) soll hier nicht mitkommen.
    def standard_laufwerke(self) -> list:
        """Laufwerksbestückung ab Werk, je K5122-Steckplatz ein Kernprofil."""
        from app import drive_types as dt
        return dt.default_drive_types(self.maschine)

    def waehlbare_laufwerke(self) -> list:
        """Wählbare Laufwerkstypen im Einstellungskasten (Einträge aus DRIVE_TYPES)."""
        from app import drive_types as dt
        return dt.drive_types_for(self.maschine)


#: Hinweise zu CopyQM (.cqm) beim Einlegen; je Profil in `cqm_hinweise` gewaehlt.
CQM_HINWEIS_UDOS = ("CopyQM-Abbilder sind mit UDOS nicht verwendbar: das Format kann die Sektorkontrollblöcke nicht speichern.")
CQM_HINWEIS_BOOT = ("CopyQM ist für Bootdisketten ungeeignet: diese enthalten ein gemischtes Format (128-Byte-Systemspuren), das CopyQM nicht darstellen kann.")

A5120 = Programmprofil(
    maschine="a5120",
    programm="a5120emu",
    titel="A5120 Emulator",
    rechner="A5120",
    cqm_hinweise=(CQM_HINWEIS_UDOS, CQM_HINWEIS_BOOT),
    beschreibung="Emulator des Bürocomputers robotron A5120 (K1520-Bus)",
    konfig_datei="a5120emu.yaml",
    vorgabe_datei="default_config_a5120.yaml",
    nenntakt_hz=2_450_000,
    nenntakt_text="2,45 MHz",
    tastatur="k7637",
    modellwahl=True,
    # Drei Rechner (Entwurf 33 §3a): A5120, A5130, K8924.  Ein Modell ist nur eine Sammlung von
    # Vorgaben (`modell_vorgaben`); die Spalte „Tastatur“ ist die Bauart der Bildschirmtastatur
    # (folgt der Tastaturoption erst mit N7).  Alte Schluessel (a5120.16, a5120-alt …) sind unbekannt.
    modelle=tuple((k, "a5120", None, lbl, "k7637") for k, lbl, _t in _modell.MODELS),
    modell_vorgaben=tuple((k, tuple(v.items())) for k, v in _modell.VORGABEN.items()),
    aup_wahl=True,
    k3521_wahl=True,
    # Optionen unter *Einstellungen ▸ Allgemein*.  Kaltstart noetig wie bei der Modellwahl.
    # Die Kassette ist das vorhandene Kaestchen (`tape_wahl`).  Das Lade-ROM „K8924 V0.9a
    # (handgepatcht)“ fehlt, bis der Kern es kennt (kein `v09a_k8924` in `A5120_KONFIG_WERTE`).
    hardware=(
        ("ram", "RAM:",
         "Speicherkarten: K3525 (je 16 KB) ab 0000H im 4000H-Raster oder die K3526 mit 64 KB.  "
         "Software, die mehr braucht (CP/A will den oberen Bereich), läuft mit weniger nicht.",
         (("k3525_1", "1x K3525 16KB"), ("k3525_2", "2x K3525 32KB"),
          ("k3525_4", "4x K3525 64KB"), ("k3526", "K3526 64KB")), ()),
        ("bildschirm", "Bildschirmkarte:",
         "K7024 (80x24, Zeichen 8x12) und/oder K7023 (64x16).  Mit beiden Karten wählt der "
         "Schiebeschalter darunter, welche das Bild liefert; er wirkt sofort.",
         (("k7024", "K7024 Auflösung 24x80"), ("k7023", "K7023 Auflösung 16x64"),
          ("beide", "K7024 und K7023 mit Umschalter")), ()),
        ("tastatur", "Tastatur:",
         "K7636 parallel am U-Bus der K2526, K7637 seriell am Kanal der K8025.",
         (("k7636", "K7636 (parallel)"), ("k7637", "K7637 (seriell)")), ()),
        ("laderom", "Lade-ROM:",
         "Lade-ROM der K2526.  V0.6 liest die BCU-Disketten „SYL auf Spur 1“.",
         (("v09_2", "V0.9_2"), ("v09_1", "V0.9_1"), ("v06", "V0.6")), ()),
        ("erweiterungsmodul", "Erweiterungsmodul:",
         "EM064/EM256 mit dem U8001 (A5120.16): nur am A5120 und nur mit der K3526 (64 KB) als RAM.",
         (("none", "keines"), ("em064", "EM064"), ("em256", "EM256 + U8001")), ("a5120",)),
    ),
    modell_tipp=("Das Modell setzt Vorgaben für Bildschirm, RAM, Tastatur, Lade-ROM, Erweiterungsmodul, "
                 "Kassette und Laufwerke; danach lässt sich jede Einzelheit ändern.  Wählt man das "
                 "Modell erneut, gelten wieder seine Vorgaben.  Ein Wechsel erzeugt die Maschine "
                 "neu (wie ein Kaltstart)."),
    ueber_rechner="des Bürocomputers <b>robotron A5120</b>",
    andere="k8915",
    weitere=("prg710", "pc1715", "p8000", "mc80"),
)

K8915 = Programmprofil(
    maschine="k8915",
    programm="k8915emu",
    titel="K8915 Emulator",
    rechner="K8915",
    beschreibung="Emulator des Arbeitsplatzcomputers robotron K8915 (K1520-Bus)",
    konfig_datei="k8915emu.yaml",
    vorgabe_datei="default_config_k8915.yaml",
    # 2,4576 MHz (K8915Machine::CPU_HZ; Quarz 9,8304 MHz / 4).
    nenntakt_hz=2_457_600,
    nenntakt_text="2,4576 MHz",
    tastatur="k7672",
    frontplatte=True,
    eigene_aktionen=("nmi",),
    modellwahl=True,
    # doc/design/24_k8915_varianten.md R5: die Schluessel sind technisch und bleiben,
    # nur die Anzeigetexte tragen die Geraetenamen V3/V2 (F1, 2026-10-05: „Gen 2“ = V2).  Fehlt ``general.model`` (Vorgabe-
    # datei, aeltere Konfigurationen), ist es der V3.  Ein EM gibt es hier nicht
    # (``em`` = None in jeder Zeile) — ``a5120.16`` ist am K8915 unbekannt → V3.
    modelle=(("k8915", "k8915", None, "K8915 V3 (ZRE 045-8762, 128 KB)", "k7672"),
             ("k8915-g2", "k8915-g2", None, "K8915 V2 (ZRE K2521, 64 KB)", "k7672")),
    # V1 (Tastatur K7634) faehrt der Kern nicht: kein Urlader-Baustein fuer 0400H
    # (F11, AP-V9 zurueckgestellt).  Die Bildschirmtastatur K7634 liegt bereit
    # (`app/ui/keyboard_k7634.py`), ist aber bewusst nicht eingehaengt.
    gesperrte_modelle=(("k8915-g1", "K8915 V1 (Tastatur K7634)",
                        "Nicht wählbar: kein Urlader-Baustein für 0400H (F11)."),),
    modell_tipp=("K8915 V3 (ZRE 045-8762, 128 KB) oder V2 (ZRE K2521 mit RAM-Karte "
                 "K3528, 64 KB).  Ein Wechsel erzeugt die Maschine neu (wie ein Kaltstart)."),
    ueber_rechner="des Arbeitsplatzcomputers <b>robotron K8915</b>",
    andere="a5120",
    weitere=("prg710", "pc1715", "p8000", "mc80"),
    # Bis AP-S12 hiessen SIO1-B und SIO2-A nach dem Entwurf „IFS 1"/„IFS 2"; seitdem
    # nach der Beschriftung am Geraet.  „V.24" (SIO1-A) blieb.
    alte_schnittstellen=(("IFS 1", "Drucker/IFSS1"), ("IFS 2", "DFÜ/IFSS2")),
)

PRG710 = Programmprofil(
    maschine="prg710",
    programm="prg710emu",
    titel="PRG710 Emulator",
    rechner="PRG 710",
    cqm_hinweise=(CQM_HINWEIS_UDOS,),
    beschreibung="Emulator der Programmiergeräte robotron PRG 710 und PRG 710-1 (K1520-Bus)",
    konfig_datei="prg710emu.yaml",
    vorgabe_datei="default_config_prg710.yaml",
    # 2,4576 MHz (Prg710Machine::CPU_HZ, wie der K8915).
    nenntakt_hz=2_457_600,
    nenntakt_text="2,4576 MHz",
    tastatur="k7609",
    eprommer=True,
    modellwahl=True,
    # Beide Geraete laufen auf derselben ZRE/ABS/AFS; das 710 hat die Tastatur
    # K7609 an einem 8279 (ATP), das 710-1 die K7672 an der K8025 (§3.9/§3.10).
    modelle=(("prg710", "prg710", None, "PRG 710 (Tastatur K7609)", "k7609"),
             ("prg710-1", "prg710-1", None, "PRG 710-1 (Tastatur K7672)", "k7672")),
    modell_tipp=("PRG 710 oder PRG 710-1: andere ZRE-Fassung, andere Tastatur und "
                 "andere Schnittstellen.  Ein Wechsel erzeugt die Maschine neu "
                 "(wie ein Kaltstart)."),
    ueber_rechner="der Programmiergeräte <b>robotron PRG 710 und PRG 710-1</b>",
    andere="a5120",
    weitere=("k8915", "pc1715", "p8000", "mc80"),
)

PC1715 = Programmprofil(
    maschine="pc1715",
    programm="pc1715emu",
    titel="PC1715 Emulator",
    rechner="PC 1715",
    beschreibung="Emulator der Bürocomputer robotron PC 1715 und PC 1715W",
    konfig_datei="pc1715emu.yaml",
    vorgabe_datei="default_config_pc1715.yaml",
    # 2,458 MHz (Pc1715Zre::CPU_HZ); der 1715W laeuft mit 3,9936 MHz (`modell_takte`).
    nenntakt_hz=2_458_000,
    nenntakt_text="2,458 MHz",
    tastatur="pc1715",
    modellwahl=True,
    # Der Bildschirm (K7222 80 × 24 / K7221 64 × 16) ist eine Hardwarevariante der
    # ZRE-Bestueckung, im Kern ein Konstruktorparameter (`bild`) → ein Wechsel ist ein
    # Kaltstart wie der Modellwechsel; er steht deshalb als Modell in der Auswahl.
    modelle=(("pc1715", "pc1715", None, "PC 1715 (Bildschirm K7222, 80 × 24)", "pc1715"),
             ("pc1715-k7221", "pc1715-k7221", None,
              "PC 1715 (Bildschirm K7221, 64 × 16)", "pc1715"),
             # Der 1715W hat nur den 8275-Bildschirm mit ladbarem Zeichensatz (kein
             # K7221) — als eigenes Modell ist die Kombination gar nicht waehlbar.
             ("pc1715w", "pc1715w", None, "PC 1715W (SCP 3.0, 256 KB, 80 × 24)",
              "pc1715")),
    modell_takte=(("pc1715w", 3_993_600, "3,9936 MHz"),),
    # ROM-Varianten, die technisch etwas aendern (AP-6): die ZG-EPROMs der ZRE (nur PC 1715;
    # der 1715W laedt seinen Satz von Diskette) und das Tastatur-ROM (beide).
    hardware=(
        ("zeichensatz", "Zeichensatz:",
         "Bestückung der Zeichengenerator-EPROMs A25.2/A25.1 der ZRE.  Wirkt nur am PC 1715 — "
         "der PC 1715W bekommt seinen Satz von Diskette.  Ein Wechsel startet die Maschine kalt.",
         (("deutsch", "Deutsch (S619 + S602)"),
          ("polnisch", "Polnisch (S641 + S619)"),
          ("kyrillisch", "Kyrillisch (S643 + S605)")),
         ("pc1715", "pc1715-k7221")),
        ("tastatur", "Tastatur:",
         "ROM der Tastatur-CPU: S600 (QWERTY) oder TAST_618 (QWERTZ, Y und Z vertauscht, "
         "andere Zeichensetzung).  Ein Wechsel startet die Maschine kalt.",
         (("s600", "S600 (QWERTY)"), ("tast618", "TAST_618 (QWERTZ)")),
         ()),
    ),
    modell_tipp=("PC 1715 mit dem Bildschirm K7222 (80 × 24) oder K7221 (64 × 16), oder "
                 "PC 1715W (4 MHz, 256 KB, U8272).  "
                 "Ein Wechsel erzeugt die Maschine neu (wie ein Kaltstart)."),
    ueber_rechner="der Bürocomputer <b>robotron PC 1715</b>",
    andere="a5120",
    weitere=("k8915", "prg710", "p8000", "mc80"),
)

#: Modelle des P8000 mit einem Rechner / mit der 16-Bit-Karte (Wirkungsbereich der Hardwarewahl).
#: Die Kern-Terminal-Modelle (``p8000``, ``p8000-16``, ``p8000-8``) gibt es in der Oberflaeche nicht
#: mehr (P23a); im Kern bleiben sie als Testgegenstelle.
_RECHNER = ("p8000-ot", "p8000-16-ot", "p8000-8-ot")
_MIT16 = ("p8000-ot", "p8000-16-ot")

P8000 = Programmprofil(
    maschine="p8000",
    programm="p8000emu",
    titel="P8000 Emulator",
    rechner="P8000",
    beschreibung="Emulator des 16-Bit-Arbeitsplatzcomputers EAW P8000 (U880 + U8001, UDOS und WEGA)",
    konfig_datei="p8000emu.yaml",
    vorgabe_datei="default_config_p8000.yaml",
    # 4 MHz (P8000Machine::Config::takt8_hz); 16-Bit-Karte und WDC laufen ebenfalls mit 4 MHz.
    nenntakt_hz=4_000_000,
    nenntakt_text="4 MHz",
    tastatur="k7673",
    frontplatte=True,
    frontplatte_art="p8000",
    eigene_aktionen=("nmi", "stand_speichern", "stand_laden"),
    terminal=True,
    platte=True,
    ptape_wahl=False,
    tape_wahl=False,
    raf_wahl=False,                 # die RAF ist eine K1520-Karte; der P8000 hat keinen K1520-Bus
    rtc_wahl=True,
    hardware_vorgaben=(("rtc", _rtc.VORGABE_TYP), ("rtc_versatz", _rtc.VORGABE_VERSATZ)),
    modellwahl=True,
    betriebsart_wahl=True,
    # Das Vollgeraet (16-Bit-Teil + Winchester) steht vorn und ist damit die Vorgabe.  Alle sind
    # dieselbe Kernmaschine mit dem ORIGINALTERMINAL (Typ 2, Z8 + Firmware P8T 5.0 + Flachtastatur
    # K7673.09) an tty1 (Entwurf 25 §11, 28); sie unterscheiden sich in `karte16`/`wdc`.
    modelle=(("p8000-ot", "p8000", None, "Vollgerät (16-Bit-Teil und Winchester)", "k7673"),
             ("p8000-16-ot", "p8000", None, "ohne Winchester (16-Bit-Teil, nur UDOS)", "k7673"),
             ("p8000-8-ot", "p8000", None, "nur 8-Bit-Teil (UDOS, erste Baustufe)", "k7673"),
             # Nur das Terminal: ein Arbeitsplatz, der sich ueber seine serielle Leitung mit dem
             # Rechner eines anderen Programms verbindet (Mehrplatzbetrieb).
             ("p8000-term", "p8000-terminal", None,
              "nur Terminal (Arbeitsplatz, ohne Rechner)", "k7673")),
    modell_kern=(("p8000-ot", (("karte16", "1"), ("wdc", "4.2"), ("terminal", "original"))),
                 ("p8000-16-ot", (("karte16", "1"), ("terminal", "original"))),
                 ("p8000-8-ot", (("karte16", "0"), ("terminal", "original")))),
    modell_arten=(("p8000-ot", "original"), ("p8000-16-ot", "original"),
                  ("p8000-8-ot", "original"), ("p8000-term", "einheit")),
    modell_alt=(("p8000", "p8000-ot"), ("p8000-16", "p8000-16-ot"), ("p8000-8", "p8000-8-ot")),
    modell_tipp=("Rechnerausstattung: Vollgerät = 8-Bit-Karte, 16-Bit-Karte mit U8001 und "
                 "Winchesterkontroller (WEGA); ohne Winchester = nur UDOS und Monitor; nur 8-Bit-Teil "
                 "= wie die erste Baustufe des Geräts (die Koppelsoftware meldet „Hardware Error in "
                 "Connection“).  Betriebsart: „Computer mit Terminal“ bringt den Rechner samt "
                 "Originalterminal (Z8, Firmware P8T 5.0, Flachtastatur K7673.09) mit; „nur Terminal“ "
                 "ist der Arbeitsplatz ohne Rechner, verbunden über die serielle Leitung mit einem "
                 "anderen P8000-Emulator.  Ein Wechsel erzeugt die Maschine neu (wie ein Kaltstart)."),
    # ROM-Fassungen und Platinenindex (Entwurf 25 §10.4); jeder Wechsel ist ein Kaltstart.
    # Wo eine Wahl am Modell nichts bewirkt (16-Bit-Karte, WDC), steht das Feld ausgegraut.
    hardware=(
        ("index", "Platinenindex:",
         "Ausgabestand der Karten: 8-Bit-Karte Index 3 mit 16-Bit-Karte Index 4 (Vorgabe) oder "
         "beide Index 1.  Wirkt auf Rücksetzweg, Kopplung und Laufwerksstecker.",
         (("34", "8-Bit 3 / 16-Bit 4 (Vorgabe)"), ("11", "8-Bit 1 / 16-Bit 1")),
         _RECHNER),
        ("mon8", "MON8 (U880-Monitor):",
         "ROM-Fassung des Monitors der 8-Bit-Karte.",
         (("3.1", "MON8 3.1"), ("3.0", "MON8 3.0")),
         _RECHNER),
        ("mon16", "MON16 (U8000-Monitor):",
         "ROM-Fassung des Monitors der 16-Bit-Karte.",
         (("3.1", "MON16 3.1"), ("3.0", "MON16 3.0"), ("3.3", "MON16 3.3")),
         _MIT16),
        ("dram", "Hauptspeicher (16-Bit):", _p8000_ram.TIPP, _p8000_ram.AUSBAUTEN, _MIT16),
        ("wdc", "WDC-Firmware:",
         "Firmware des Winchesterkontrollers (Z80 mit eigener ROM).  4.2 ist laufwerksunabhängig "
         "(Parametersatz auf der Platte) und die geprüfte Fassung.  4.0.05 und 3.4.05 sind "
         "EXPERIMENTELL: ihr EPROM legt das Laufwerk fest (K5504.50, 1024/5/18), eine Platte "
         "anderer Größe wird abgewiesen, und das Spurformat der 3.x ist noch nicht belegt "
         "(doc/p8000/wdc_firmware.md §12 Nr. 11).",
         (("4.2", "WDC 4.2"), ("4.0.05", "WDC 4.0.05 (experimentell)"),
          ("3.4.05", "WDC 3.4.05 (experimentell)")),
         ("p8000-ot",)),
        # Uhrenkarte (Entwurf 37): als Gruppe „RTC“ gebaut (settings_widget), Vorgabe siehe hardware_vorgaben.
        (_rtc.SCHLUESSEL_TYP, "RTC:", _rtc.TIPP_TYP, _rtc.TYPEN, _MIT16),
        (_rtc.SCHLUESSEL_VERSATZ, "Zeitversatz:", _rtc.TIPP_VERSATZ, _rtc.VERSAETZE, _MIT16),
        ("platte_par", "Parametersatz der Platte:",
         "Was der WDC auf Zylinder 0 / Kopf 0 / Sektor 1 sieht.  aus (Vorgabe): nur das, was in der "
         "Datei steht — eine unformatierte Platte bleibt unformatiert.  ergänzen: fehlt der "
         "Parametersatz, liefert die Platte einen erzeugten (nur für das WEGA-3.1-Abbild des "
         "AVR-Emulators, 1380/10/18).",
         (("aus", "aus (Vorgabe)"), ("ergaenzen", "ergänzen (WEGA-3.1-AVR-Abbild)")),
         ("p8000-ot",)),
    ),
    schnittstellen_ports=(("tty0", 5000), ("tty2", 5002), ("tty3", 5003), ("tty4", 5004),
                          ("tty5", 5005), ("tty6", 5006), ("tty7", 5007),
                          ("Terminal (XB5)", 5004)),
    ueber_rechner="des 16-Bit-Arbeitsplatzcomputers <b>EAW P8000</b>",
    andere="a5120",
    weitere=("k8915", "prg710", "pc1715", "mc80"),
)

MC80 = Programmprofil(
    maschine="mc80",
    programm="mc80emu",
    titel="MC80 Emulator",
    rechner="MC80",
    beschreibung="Emulator der Mikrorechner MC80.30, .31, .21 und .22 (VEB Elektronik Gera)",
    konfig_datei="mc80emu.yaml",
    vorgabe_datei="default_config_mc80.yaml",
    # 2,4576 MHz (Mc80Machine::CPU_HZ = Mc80Zve::CPU_HZ).
    nenntakt_hz=2_457_600,
    nenntakt_text="2,4576 MHz",
    tastatur="mc80",
    modellwahl=True,
    # .30 und .31 laufen im Kern bis M7 gleich (die .31 hat zusaetzlich die EPROM-Segmente
    # 10H/20H, M4); sie unterscheiden sich erst am Kassettenanschluss (M7).
    modelle=(("mc80", "mc80", None, "MC80.30", "mc80"),
             ("mc80-31", "mc80-31", None, "MC80.31 (Programmsegmente 10H/20H)", "mc80"),
             # MC80.2x (AP M11/M12): eigene Klasse, Bild 528 × 256 (Oszilloskop-Darstellung),
             # Matrixtastatur an der DPL, Kassette (MBI) fest eingebaut.  .21 und .22 sind im
             # Kern gleich (Unterschied nicht belegt, hardware_2x.md) [?].
             ("mc80-21", "mc80-21", None, "MC80.21 (Oszilloskop-Bild, BSYS)", "mc80-2x"),
             ("mc80-22", "mc80-22", None, "MC80.22 (Oszilloskop-Bild, BSYS)", "mc80-2x")),
    # Der Kern lehnt diese Variante ab (Mc80Machine → std::invalid_argument).
    gesperrte_modelle=(
        ("mc80-33", "MC80.33 (AKM + PK 3)",
         "Nicht wählbar: vom Treiber-ROM der AKM liegt nur die Hälfte vor (doc/mc80/egos.md §3)."),
    ),
    # Beim MC80.2x ist die Kassette (MBI) fest eingebaut: das Kaestchen ist ohne Wirkung.
    tape_fest=("mc80-21", "mc80-22"),
    hardware=(
        ("spe", "Speichererweiterung:",
         "Zahl der Speicherkarten SPE 2: die erste trägt ROM und RAM-Segment 01H, eine zweite "
         "nur RAM (Segment 02H).  Ein Wechsel startet die Maschine kalt.",
         (("1", "eine SPE 2"), ("2", "zwei SPE 2")),
         ("mc80", "mc80-31")),
    ),
    modell_tipp=("MC80.30 oder MC80.31 (mit den EPROM-Programmsegmenten 10H/20H), MC80.21/.22 "
                 "(Mikrorechner mit Oszilloskop-Bild und Betriebssystem BSYS).  "
                 "Ein Wechsel erzeugt die Maschine neu (wie ein Kaltstart)."),
    eigene_aktionen=("speicherabzug_laden",),
    laufwerke=False,
    # Lochstreifen K6022 (E0H–E7H) kollidiert mit dem EPROM-Programmierer E0H–EFH; die RAF
    # (88H/89H) liegt im E/A-Bereich der ZVE (80H–8FH) [?].  Kassette seit AP M7: K5020 an
    # 30H (.30 LW 1200 mit Takt aus der ASP, .31 K5200), EGOS bedient nur Laufwerk 1.
    ptape_wahl=False,
    tape_wahl=True,
    raf_wahl=False,
    ueber_rechner="der Mikrorechner <b>MC80.30/31/21/22</b> des VEB Elektronik Gera",
    andere="a5120",
    weitere=("k8915", "prg710", "pc1715", "p8000"),
)

#: Alle Profile nach Name (Schluessel = Maschinenname).
PROFILE = {p.maschine: p for p in (A5120, K8915, PRG710, PC1715, P8000, MC80)}

#: Das Profil ohne Angabe — aeltere Starter, Tests, ``app/main.py`` ohne Schalter.
VORGABE = A5120


def profil(name: str = "") -> Programmprofil:
    """Profil zu einem Maschinennamen (``""`` = Vorgabe A5120).

    Raises:
        ValueError: bei einem unbekannten Namen — mit den bekannten im Text.
    """
    if not name:
        return VORGABE
    try:
        return PROFILE[name.lower()]
    except KeyError:
        raise ValueError(f"unbekannte Maschine {name!r} "
                         f"(bekannt: {', '.join(PROFILE)})") from None

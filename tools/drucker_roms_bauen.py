#!/usr/bin/env python3
"""Erzeugt data/drucker_roms.yaml (Entwurf 49 §12, WP9) - EINMALWERKZEUG, nicht Laufzeit.

Die YAML traegt die ROM-Daten selbst als Intel-HEX (16 Byte je Zeile, Adresse ab 0000H, Endsatz :00000001FF);
Eintraege mit denselben Daten (IFSS-Varianten) verweisen per `daten_von` auf den ersten.  Quellen der Daten:
die Abzuege unter doc/EPROMS (die .bin bleiben Herkunftsbeleg).  K6316-Saetze = fuenf Einzelchips I1-I5 / V1-V5
lueckenlos hintereinander.

Aufruf (Metadaten aus einer bestehenden Katalogfassung, z. B. dem Stand vor WP9):
    git show 30f5fafb:data/drucker_roms.yaml > alt.yaml          # Eintraege mit `datei:` und `md5:`
    python3 tools/drucker_roms_bauen.py --meta alt.yaml [--ausgabe data/drucker_roms.yaml]
Ohne --meta liest das Skript die vorhandene data/drucker_roms.yaml NICHT (sie traegt keine `datei` mehr);
Aenderungen am Katalog macht man danach direkt in der YAML (Texteditor) - das Skript ist nur der Erstbau.
Die Herkunftspruefung (YAML byteweise gegen die .bin) macht tests/python/test_drucker_rom_katalog.py.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
EPROMS = ROOT / "doc" / "EPROMS"
ZEILE = 16

#: zusammengesetzte K6316-Saetze: Katalogdatei (alt) -> Chips in Reihenfolge
K6316_SAETZE = {
    "K6316/k6316_ifss.rom": [f"K6316/ifss/I{i}.BIN" for i in range(1, 6)],
    "K6316/k6316_v24.rom": [f"K6316/v24/V{i}.BIN" for i in range(1, 6)],
}


def quelle_bytes(datei: str) -> bytes:
    """Die Bytes eines alten `datei:`-Eintrags aus den Abzuegen (K6316: fuenf Einzelchips)."""
    if datei in K6316_SAETZE:
        return b"".join((EPROMS / c).read_bytes() for c in K6316_SAETZE[datei])
    return (EPROMS / datei).read_bytes()


def intel_hex(daten: bytes) -> str:
    """Intel-HEX-Text: Datensaetze zu je 16 Byte, Adresse ab 0000H (max. 64 KiB), Endsatz :00000001FF."""
    if len(daten) > 0x10000:
        raise ValueError("mehr als 64 KiB")
    zeilen = []
    for a in range(0, len(daten), ZEILE):
        teil = daten[a:a + ZEILE]
        rec = bytes([len(teil), a >> 8, a & 0xFF, 0]) + teil
        zeilen.append(":" + (rec + bytes([(-sum(rec)) & 0xFF])).hex().upper())
    zeilen.append(":00000001FF")
    return "\n".join(zeilen)


def vorspann_kommentare(text: str) -> dict:
    """Kommentarzeilen unmittelbar vor `- schluessel: X` der alten Fassung -> {X: [Zeilen]}."""
    res, puffer = {}, []
    for z in text.splitlines():
        s = z.strip()
        if s.startswith("#"):
            puffer.append(s)
            continue
        m = re.match(r"- schluessel:\s*(\S+)", s)
        if m and z.startswith("  - "):
            res[m.group(1)] = puffer
        if s:
            puffer = [] if not m else puffer
            if m:
                puffer = []
    return res


def q(s: str) -> str:
    return json.dumps(s, ensure_ascii=False)


KOPF = """\
# Katalog der EPROM-Saetze der Nadeldrucker (Entwurf 49 §12, WP9, Merkposten drucker.md Nr. 38).
# EINE Datei: Metadaten UND ROM-Daten (Intel-HEX).  Gelesen vom Kern (core/machines/printer/drucker_rom_katalog.*);
# Suchpfad: Umgebung K1520_PRINTER_ROMS -> <Modul>/../share/k1520emu/ -> data/.  Erzeugt mit tools/drucker_roms_bauen.py
# (Einmalwerkzeug); danach darf man sie mit einem Texteditor aendern - die Zeilenpruefsumme jeder HEX-Zeile ist die Pruefung.
#
# schluessel      eindeutig im ganzen Katalog (technischer Name, a-z 0-9 _ . -)
# typ             sd1157 | k6316-ifss | k6316-v24 | k6313 | k6313-ifss | k6311 | k6311-ifss (= Typschluessel der Oberflaeche)
# titel, pgm      Anzeige / Etikett-Programmstand
# schnittstellen  Liste aus ifss, v24
# guete           ok | unvollstaendig | defekt | unbekannt
# vorgabe         genau ein Eintrag je typ ist die Vorgabe
# aliases         alte Schluessel (Konfigurationen, rom=repariert); nie entfernen
# groesse         Bytes der Daten (der Leser vergleicht; SD 1157: 10624 = 0000H-297FH; K6316/K6313/K6311 kurz: 10240; K6313 lang: 16384; K6311: auch 8192)
# daten           Intel-HEX, 16 Byte je Zeile, Adresse ab 0000H, Endsatz :00000001FF
# herkunft        Abzuege unter doc/EPROMS (relativ; bei mehreren lueckenlos hintereinander) - nur Beleg, KEINE Laufzeitquelle;
#                 ein Waechter vergleicht die Daten byteweise damit (nur im Quellbaum)
# daten_von       statt `daten`: Schluessel des Eintrags, dessen Daten dieser mitbenutzt (IFSS-Varianten)
version: 1
eintraege:
"""


def bauen(meta_text: str) -> str:
    meta = yaml.safe_load(meta_text)
    komm = vorspann_kommentare(meta_text)
    out = [KOPF.rstrip("\n")]
    besitzer: dict[str, str] = {}   # Quelle -> Schluessel mit Daten
    for e in meta["eintraege"]:
        k = e["schluessel"]
        for z in komm.get(k, []):
            out.append("  " + z)
        roh = quelle_bytes(e["datei"])
        if len(roh) != e["groesse"]:
            raise SystemExit(f"{k}: {len(roh)} Bytes, Metadaten {e['groesse']}")
        out.append(f"  - schluessel: {k}")
        out.append(f"    typ: {e['typ']}")
        out.append(f"    titel: {q(e['titel'])}")
        out.append(f"    pgm: {q(e.get('pgm', ''))}")
        out.append(f"    schnittstellen: [{', '.join(e.get('schnittstellen', []))}]")
        out.append(f"    zeichensatz: {q(e.get('zeichensatz', ''))}")
        out.append(f"    guete: {e['guete']}")
        out.append(f"    vorgabe: {'true' if e['vorgabe'] else 'false'}")
        out.append(f"    aliases: [{', '.join(e.get('aliases', []))}]")
        out.append(f"    groesse: {e['groesse']}")
        out.append(f"    hinweis: {q(e.get('hinweis', ''))}")
        if e["datei"] in besitzer:
            out.append(f"    daten_von: {besitzer[e['datei']]}")
        else:
            besitzer[e["datei"]] = k
            quelle = K6316_SAETZE.get(e["datei"], [e["datei"]])
            out.append(f"    herkunft: [{', '.join(quelle)}]")
            out.append("    daten: |")
            out.extend("      " + z for z in intel_hex(roh).split("\n"))
    return "\n".join(out) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--meta", help="alte Katalogfassung mit `datei:`")
    ap.add_argument("--ausgabe", default=str(ROOT / "data" / "drucker_roms.yaml"))
    a = ap.parse_args()
    if not a.meta:
        ap.error("--meta fehlt")
    text = Path(a.meta).read_text(encoding="utf-8")
    Path(a.ausgabe).write_text(bauen(text), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""UDOS-Ladediskette fuer MUTOS 8000 (Entwurf 39, 9.13) aus einem Archiv oder ausgepackten Ordner bauen.

Aufbau des Archivs (siehe README.md): bauangaben.yaml, systemspuren.bin und dateien/ (alle UDOS-Dateien mit
.fileinfo bzw. udos-dateiangaben.txt).  Dateien und Meta-Angaben kommen aus dem Archiv - es wird kein Sektor
einer anderen Diskette kopiert.  Wer eine Datei unter dateien/ tauscht, bekommt mit diesem Skript eine neue Diskette.

--laufwerke waehlt die Variante (Aufzeichnung + Laufwerkstyp-Konstante, die `mutos_io`, FORM.*, COPY.* aus
[0FCAH] lesen; 2 Bit je Laufwerk):
  mf3200         FM,  2AH  alle drei Laufwerke 8 Zoll (Auslieferung)
  k5601          MFM, 3FH  alle drei Laufwerke 5,25 Zoll
  mf3200_k5601   FM,  3EH  A: MF3200 (FM), B:/C: K5601 (MFM)  - die Diskette liegt in A:

Ablauf: create im Profil der Aufzeichnung, boot-put (Systemspuren mit gepatchter Konstante; das Muster muss genau
einmal vorkommen und den erwarteten Wert tragen, sonst Abbruch VOR dem Patch), put dateien/, check --full ohne
Befund, Gegenprobe (Konstante zurueckgelesen, jede Datei byteweise gleich dem Archiv).  Das DiskTool wird nur
ueber tools/dev.sh (oder K1520_DISKTOOL) gerufen.  Container nur .hfe: der DMK-Schreiber macht bei FM falsche CRCs.
Ein vorhandenes Ziel (auch eine eingecheckte Diskette) wird nur mit --ueberschreiben ersetzt.
"""
import argparse
import os
import re
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bau_aus_archiv as basis  # noqa: E402

PFLICHT = ("fs_fm", "fs_mfm", "systemspuren", "typ_muster", "typ_erwartet")
VARIANTEN = ("mf3200", "k5601", "mf3200_k5601")
# Aufzeichnung -> (Schluessel des Profils, erkanntes Katalogformat)
AUFZEICHNUNG = {"fm": ("fs_fm", "mf3200_26x128"), "mfm": ("fs_mfm", "udos_ss77")}
BEIBLATT = "udos-dateiangaben.txt"


def variante_lesen(a, name):
    """`variante_<name>: fm|mfm HEX` -> (Aufzeichnung, Konstante)."""
    teile = a.get("variante_" + name, "").split()
    if len(teile) != 2 or teile[0] not in AUFZEICHNUNG or not re.fullmatch(r"[0-9A-Fa-f]{2}", teile[1]):
        raise SystemExit("bauangaben.yaml: variante_%s fehlt oder ist nicht '<fm|mfm> <HEX>'" % name)
    return teile[0], int(teile[1], 16)


def muster_stelle(daten, muster_hex, erwartet):
    """Offset des Konstantenbytes (die `??` im Muster) - genau eine Fundstelle, Erwartungswert geprueft."""
    if not re.fullmatch(r"([0-9A-Fa-f]{2}|\?\?)+", muster_hex) or muster_hex.count("??") != 1:
        raise SystemExit("typ_muster: Hexbytes mit genau einem '??' erwartet")
    muster = [None if muster_hex[i:i + 2] == "??" else int(muster_hex[i:i + 2], 16)
              for i in range(0, len(muster_hex), 2)]
    pos = muster.index(None)
    treffer = [i for i in range(len(daten) - len(muster) + 1)
               if all(b is None or daten[i + k] == b for k, b in enumerate(muster))]
    if len(treffer) != 1:
        raise SystemExit("Laufwerkstyp-Konstante: Muster %s kommt %d-mal in den Systemspuren vor (erwartet: 1)"
                         % (muster_hex, len(treffer)))
    stelle = treffer[0] + pos
    if daten[stelle] != erwartet:
        raise SystemExit("Laufwerkstyp-Konstante an Offset %d ist %02XH, erwartet %02XH - nicht gepatcht"
                         % (stelle, daten[stelle], erwartet))
    return stelle


def dateien_des_archivs(ordner):
    """Relative Pfade der Nutzdateien (ohne .fileinfo und Beiblatt)."""
    for wurzel, _, namen in os.walk(ordner):
        for n in sorted(namen):
            if n.endswith(".fileinfo") or n == BEIBLATT:
                continue
            yield os.path.relpath(os.path.join(wurzel, n), ordner)


def bauen(quelle, ziel, laufwerke, ueberschreiben=False, ausgabe=print):
    if laufwerke not in VARIANTEN:
        raise SystemExit("--laufwerke: " + " | ".join(VARIANTEN))
    ziel = os.path.abspath(ziel)
    if not ziel.lower().endswith(".hfe"):
        raise SystemExit("Ziel muss .hfe sein (DMK-Schreiber: falsche CRCs bei FM)")
    if os.path.exists(ziel) and not ueberschreiben:
        raise SystemExit("%s existiert; --ueberschreiben oder anderes Ziel" % ziel)
    tmp = tempfile.mkdtemp(prefix="m8k_lade_")
    try:
        wurzel = basis.auspacken(quelle, tmp)
        a = basis.bauangaben_lesen(os.path.join(wurzel, "bauangaben.yaml"), PFLICHT, ())
        aufz, konstante = variante_lesen(a, laufwerke)
        fs_schluessel, format_ = AUFZEICHNUNG[aufz]
        dateien = os.path.join(wurzel, "dateien")
        sys_pfad = os.path.join(wurzel, a["systemspuren"])
        if os.path.dirname(a["systemspuren"]) or not os.path.isfile(sys_pfad):
            raise SystemExit("systemspuren '%s' fehlt im Archiv (nur Name an der Wurzel)" % a["systemspuren"])
        if not os.path.isdir(dateien):
            raise SystemExit("Ordner dateien/ fehlt")

        daten = bytearray(open(sys_pfad, "rb").read())
        stelle = muster_stelle(daten, a["typ_muster"], int(a["typ_erwartet"], 16))
        daten[stelle] = konstante                      # erst NACH der Pruefung
        gepatcht = os.path.join(tmp, "systemspuren_gepatcht.bin")
        open(gepatcht, "wb").write(daten)

        bau = os.path.join(tmp, "bau.hfe")
        ausgabe("anlegen: %s (%s), Laufwerkstyp %02XH" % (a[fs_schluessel], aufz.upper(), konstante))
        basis.disktool("create", bau, "--fs", a[fs_schluessel])
        basis.disktool("boot-put", bau, gepatcht)
        basis.disktool("put", bau, dateien)
        pruef = basis.disktool("check", bau, "--full")
        if "ohne Befund" not in pruef:
            raise SystemExit("check --full meldet Befunde, Ziel nicht geschrieben:\n" + pruef)
        ausgabe("check --full: ohne Befund")

        # Gegenprobe 1: Aufzeichnung (Katalogformat) und Systemspuren samt Konstante auf der fertigen Diskette
        info = basis.disktool("info", bau)
        if not re.search(r"^Format:\s+" + format_ + r"\s*$", info, re.M):
            raise SystemExit("Aufzeichnung nicht wie verlangt (%s):\n%s" % (format_, info))
        zurueck = os.path.join(tmp, "zurueck.bin")
        basis.disktool("boot-get", bau, zurueck)
        if open(zurueck, "rb").read()[:len(daten)] != bytes(daten):
            raise SystemExit("Systemspuren der Diskette weichen von den eingespielten ab")
        # Gegenprobe 2: jede Datei des Archivs byteweise
        namen = list(dateien_des_archivs(dateien))
        # Das Verzeichnis (UDOS `DIRECTORY`, Typ D) entsteht mit der Diskette und steht nicht im Archiv.
        ls = basis.disktool("ls", bau, "-l")
        belegt = {z.split()[0] for z in ls.splitlines() if re.match(r"^\S+\s+(A|A2|P|P1|B|D)\s", z)}
        if belegt - set(namen) != {"DIRECTORY"} or set(namen) - belegt:
            raise SystemExit("Dateien der Diskette und des Archivs weichen ab: nur Diskette %s, nur Archiv %s"
                             % (sorted(belegt - set(namen)), sorted(set(namen) - belegt)))
        holen = os.path.join(tmp, "geholt")
        os.makedirs(holen)
        basis.disktool("get", bau, "--to", holen)
        for n in namen:
            p = os.path.join(holen, n)
            if not os.path.isfile(p) or open(p, "rb").read() != open(os.path.join(dateien, n), "rb").read():
                raise SystemExit("Datei %s weicht vom Archiv ab, Ziel nicht geschrieben" % n)
        ausgabe("%d Dateien gleich dem Archiv" % len(namen))

        os.makedirs(os.path.dirname(ziel), exist_ok=True)
        shutil.copyfile(bau, ziel)
        ausgabe("gebaut: " + ziel)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return ziel


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("quelle", help="Archiv (.zip) oder ausgepackter Ordner")
    ap.add_argument("ziel", help="Zieldiskette (.hfe)")
    ap.add_argument("--laufwerke", required=True, choices=VARIANTEN,
                    help="Variante: mf3200 (FM, 2AH) | k5601 (MFM, 3FH) | mf3200_k5601 (FM, 3EH)")
    ap.add_argument("--ueberschreiben", action="store_true", help="vorhandenes Ziel ersetzen")
    a = ap.parse_args(argv)
    bauen(a.quelle, a.ziel, a.laufwerke, a.ueberschreiben)
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Wurzeldiskette (MUTOS 8000, Entwurf 39, 4.8) aus einem Archiv oder ausgepackten Ordner bauen.

Aufbau des Archivs (siehe README.md): bauangaben.yaml (fs, inodes, bloecke, zylinder0),
zylinder0.bin und dateien/ (Dateien mit .fileinfo, dazu wega-dateiangaben.txt fuer Geraete/FIFOs/
Wurzel).  Wer eine Datei unter dateien/ austauscht, bekommt mit diesem Skript eine neue Diskette.

Ablauf: auspacken (Temp), create --fs/--inodes/--blocks, boot-put, put dateien/, check --full,
unabhaengige Pruefung (wurzel_pruefen.py; Fehler bei Block >= s_fsize).
`--laufwerke mf3200` (Vorgabe) baut FM-Spuren (Profil `fs`), `--laufwerke k5601` dieselbe Wurzel in MFM
(Profil `fs_mfm`, Entwurf 39 §9.13): Blockabbildung und Verschraenkung haengen nur an den Sektornummern.
Die UDOS-Ladedisketten baut bau_ladediskette.py.
Das DiskTool wird nur ueber tools/dev.sh gerufen.  Die eingecheckte Diskette ist nur mit
--ueberschreiben als Ziel erlaubt; ein vorhandenes Ziel ebenso.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m8k  # noqa: E402
import wurzel_pruefen  # noqa: E402

EINGECHECKT = m8k.fertig("root_fm.hfe")
PFLICHT = ("fs", "inodes", "bloecke", "zylinder0")
# --laufwerke -> Schluessel der Bauangaben, der das Profil nennt (Spuraufzeichnung steckt im Profil)
LAUFWERKE = {"mf3200": "fs", "k5601": "fs_mfm"}


def disktool(*args):
    # K1520_DISKTOOL: fertiges Binary (Tests, die schon von dev.sh gebaut und gerufen werden);
    # sonst immer ueber dev.sh, damit kein veraltetes Binary laeuft.
    roh = os.environ.get("K1520_DISKTOOL")
    cmd = ([roh] if roh else [os.path.join(m8k.REPO, "tools", "dev.sh"), "tool", "k1520disktool"]) \
        + [str(x) for x in args]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=m8k.REPO)
    if r.returncode != 0:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit("k1520disktool %s: Rueckgabe %d" % (args[0], r.returncode))
    return r.stdout


def bauangaben_lesen(pfad, pflicht=PFLICHT, zahlen=("inodes", "bloecke")):
    """Flaches `schluessel: wert` mit #-Kommentaren; fehlt etwas, bricht der Bau ab."""
    if not os.path.isfile(pfad):
        raise SystemExit("bauangaben.yaml fehlt (Archiv ohne Bauangaben?)")
    a = {}
    for z in open(pfad, encoding="utf-8").read().splitlines():
        z = z.split("#", 1)[0].strip()
        if not z:
            continue
        k, sep, v = z.partition(":")
        if not sep:
            raise SystemExit("bauangaben.yaml: Zeile ohne ':' -> %r" % z)
        a[k.strip()] = v.strip().strip("\"'")
    fehlt = [k for k in pflicht if not a.get(k)]
    if fehlt:
        raise SystemExit("bauangaben.yaml: fehlt " + ", ".join(fehlt))
    for k in zahlen:
        if not a[k].isdigit():
            raise SystemExit("bauangaben.yaml: %s muss eine Zahl sein" % k)
    return a


def auspacken(quelle, tmp):
    """Archiv -> Ordner (mit Pfadpruefung); ein Ordner wird unveraendert benutzt."""
    if os.path.isdir(quelle):
        return os.path.abspath(quelle)
    ziel = os.path.join(tmp, "archiv")
    with zipfile.ZipFile(quelle) as z:
        for n in z.namelist():
            teile = n.split("/")
            if n.startswith("/") or ".." in teile or ":" in n or "\\" in n:
                raise SystemExit("Archiv enthaelt unzulaessigen Pfad: %r" % n)
        z.extractall(ziel)
    return ziel


def bauen(quelle, ziel, ueberschreiben=False, ausgabe=print, laufwerke="mf3200"):
    if laufwerke not in LAUFWERKE:
        raise SystemExit("--laufwerke: %s" % " | ".join(LAUFWERKE))
    ziel = os.path.abspath(ziel)
    if os.path.realpath(ziel) == os.path.realpath(EINGECHECKT) and not ueberschreiben:
        raise SystemExit("Ziel ist die eingecheckte Diskette; nur mit --ueberschreiben")
    if os.path.exists(ziel) and not ueberschreiben:
        raise SystemExit("%s existiert; --ueberschreiben oder anderes Ziel" % ziel)
    tmp = tempfile.mkdtemp(prefix="m8k_aus_archiv_")
    try:
        wurzel = auspacken(quelle, tmp)
        a = bauangaben_lesen(os.path.join(wurzel, "bauangaben.yaml"))
        dateien = os.path.join(wurzel, "dateien")
        boot = os.path.join(wurzel, a["zylinder0"])
        if os.path.dirname(a["zylinder0"]) or not os.path.isfile(boot):
            raise SystemExit("zylinder0 '%s' fehlt im Archiv (nur Name an der Wurzel)" % a["zylinder0"])
        if not os.path.isdir(dateien):
            raise SystemExit("Ordner dateien/ fehlt")
        bau = os.path.join(tmp, "bau.hfe")
        fs = a.get(LAUFWERKE[laufwerke])
        if not fs:
            raise SystemExit("bauangaben.yaml: %s fehlt (--laufwerke %s)" % (LAUFWERKE[laufwerke], laufwerke))
        ausgabe("anlegen: %s, %s Inodes, %s Bloecke" % (fs, a["inodes"], a["bloecke"]))
        disktool("create", bau, "--fs", fs, "--inodes", a["inodes"], "--blocks", a["bloecke"])
        disktool("boot-put", bau, boot)
        disktool("put", bau, dateien)
        ausgabe(disktool("check", bau, "--full").strip().splitlines()[-1])
        td0 = os.path.join(tmp, "bau.td0")
        disktool("save-as", bau, td0)
        if wurzel_pruefen.pruefen(td0, dateien, ausgabe=lambda *_: None):
            raise SystemExit("Bau weicht von dateien/ ab (oder Block >= s_fsize), Ziel nicht geschrieben")
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
    ap.add_argument("--ueberschreiben", action="store_true",
                    help="vorhandenes Ziel (auch die eingecheckte Diskette) ersetzen")
    ap.add_argument("--laufwerke", choices=sorted(LAUFWERKE), default="mf3200",
                    help="mf3200: FM (8 Zoll, Vorgabe) | k5601: MFM (5,25 Zoll)")
    a = ap.parse_args(argv)
    bauen(a.quelle, a.ziel, a.ueberschreiben, laufwerke=a.laufwerke)
    return 0


if __name__ == "__main__":
    sys.exit(main())

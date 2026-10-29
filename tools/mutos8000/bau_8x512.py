#!/usr/bin/env python3
"""Wurzel- und Befehlsdiskette fuer den DISK-Satz (8 x 512 B) bauen (Entwurf 39 A5c).

Warum ueberhaupt: die TeleDisk-Originale des DISK-Satzes (`a5120_mutos8000_{root,cmd}.td0`) tragen zwei Fallen,
die sich beim Arbeiten mit `tar` zeigen:
  * DISK-ROOT hat die `pwd` von 1987 in /bin.  Sie gibt `/` aus und kehrt nie zur Shell zurueck (Gastprogramm,
    Entwurf 39 10.1) - `tar` ruft `pwd` bei jedem Lauf (`/bin/pwd` zuerst) und haengt damit ebenso.
    Die Wurzel hier traegt die `pwd` von 1986 (aus dem Archiv `best_cmd.zip`), sonst ist sie DISK-ROOT.
  * DISK-CMD hat 83 Spuren (6 Spuren Altbestand hinter dem Format) und ein K5601 erreicht nur 80: sie laesst sich
    nicht einlegen.  Die neue Befehlsdiskette traegt dieselben 19 Dateien auf einem sauberen 77-Zylinder-Medium,
    mit Platz (480 Bloecke statt 320, die Originaldiskette ist bis auf 512 B voll).
DISK-CC1/DISK-CC2 lassen sich unveraendert einlegen und werden nicht nachgebaut.

    python3 -I tools/mutos8000/bau_8x512.py [--ziel-ordner ORDNER] [--ueberschreiben]

Ziel (Vorgabe disks/a5120_16_MUTOS/): a5120_mutos8000_root_8x512.hfe, a5120_mutos8000_cmd_8x512.hfe.
Das DiskTool wird nur ueber tools/dev.sh (oder K1520_DISKTOOL) gerufen.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bau_aus_archiv  # noqa: E402
import m8k  # noqa: E402

disktool = bau_aus_archiv.disktool


def pruefen(abbild, ausgabe):
    """`check --full`: Rueckgabe 1 (nur Warnungen) ist erlaubt - DISK-ROOT traegt zwei alte (verwaiste Inode 26,
    s_tinode), Rueckgabe 2 (Fehler) bricht ab."""
    roh = os.environ.get("K1520_DISKTOOL")
    cmd = ([roh] if roh else [os.path.join(m8k.REPO, "tools", "dev.sh"), "tool", "k1520disktool"]) \
        + ["check", abbild, "--full"]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=m8k.REPO)
    if r.returncode not in (0, 1):
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit("check --full: Rueckgabe %d" % r.returncode)
    ausgabe(r.stdout.strip().splitlines()[-1] if r.stdout.strip() else "")

ROOT_TD0 = m8k.disk("root.td0")
CMD_TD0 = m8k.disk("cmd.td0")
PWD_ARCHIV = m8k.disk("best_cmd.zip")           # dateien/bin/pwd = die lauffaehige von 1986
ZIELE = {"root": "root_8x512.hfe", "cmd": "cmd_8x512.hfe"}
CMD_BLOECKE = 480
CMD_INODES = 224


def _pwd_holen(tmp):
    """Die pwd von 1986 samt .fileinfo aus dem Archiv nach tmp/pwd/bin/."""
    ziel = os.path.join(tmp, "pwd")
    with zipfile.ZipFile(PWD_ARCHIV) as z:
        for n in ("dateien/bin/pwd", "dateien/bin/pwd.fileinfo"):
            z.extract(n, ziel)
    return os.path.join(ziel, "dateien", "bin", "pwd")


def wurzel_bauen(ziel, tmp, ausgabe=print):
    bau = os.path.join(tmp, "root.hfe")
    disktool("save-as", ROOT_TD0, bau)
    pwd = _pwd_holen(tmp)
    disktool("rm", bau, "bin/pwd")
    disktool("put", bau, pwd, "--as", "bin/pwd")
    pruefen(bau, ausgabe)
    shutil.copyfile(bau, ziel)


def befehle_bauen(ziel, tmp, ausgabe=print):
    bau = os.path.join(tmp, "cmd.hfe")
    dateien = os.path.join(tmp, "cmd_dateien")
    zyl0 = os.path.join(tmp, "cmd_zyl0.bin")
    disktool("get", CMD_TD0, "*", "--to", dateien)
    disktool("boot-get", CMD_TD0, zyl0)
    disktool("create", bau, "--fs", "mutos8k_8x512_mfm", "--blocks", CMD_BLOECKE, "--inodes", CMD_INODES, "--boot", zyl0)
    disktool("put", bau, dateien)
    pruefen(bau, ausgabe)
    shutil.copyfile(bau, ziel)


def bauen(ordner, ueberschreiben=False, ausgabe=print):
    ziele = {k: os.path.join(ordner, "a5120_mutos8000_" + v) for k, v in ZIELE.items()}
    vorhanden = [z for z in ziele.values() if os.path.exists(z)]
    if vorhanden and not ueberschreiben:
        raise SystemExit("vorhanden (--ueberschreiben): " + ", ".join(os.path.basename(z) for z in vorhanden))
    os.makedirs(ordner, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="m8k_8x512_")
    try:
        ausgabe("Wurzel: DISK-ROOT mit der pwd von 1986")
        wurzel_bauen(ziele["root"], tmp, ausgabe)
        ausgabe("Befehle: DISK-CMD auf sauberem 77-Zylinder-Medium")
        befehle_bauen(ziele["cmd"], tmp, ausgabe)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return ziele


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--ziel-ordner", default=m8k.FERTIG, help="Vorgabe: disks/a5120_16_MUTOS/")
    ap.add_argument("--ueberschreiben", action="store_true", help="vorhandene Disketten ersetzen")
    a = ap.parse_args(argv)
    for z in bauen(os.path.abspath(a.ziel_ordner), a.ueberschreiben).values():
        print("gebaut:", z)
    return 0


if __name__ == "__main__":
    sys.exit(main())

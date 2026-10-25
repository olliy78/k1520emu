#!/usr/bin/env python3
"""Nachbau der rekonstruierten MUTOS-8000-Wurzeldiskette (Entwurf 39, 4.8, 7).

Fahrplan (alles in einem Temp-Verzeichnis, an Kopien der eingecheckten Abbilder):
  1. DISK-ROOT mit `k1520disktool get` in einen Ordner auszug holen,
  2. Quellbaum bauen (wurzel_quellbaum.py: M8KROOT zurueckgeschoben + DISK-ROOT-Programme),
  3. `create --fs mutos8k_fm --inodes 104 --blocks 250`,
  4. Zylinder 0 von M8KCMD mit `boot-get` holen und mit `boot-put` einspielen,
  5. `put` des Quellbaums (Modi, Eigentuemer, Zeiten, Geraete aus .fileinfo/wega-dateiangaben.txt),
  6. `check --full`,
  7. unabhaengige Pruefung (wurzel_pruefen.py) des Nachbaus, und - mit --vergleich - der
     eingecheckten rekonstruierten Diskette; dazu Vergleich der `ls -l`-Ausgaben beider.

Das DiskTool wird nur ueber tools/dev.sh aufgerufen.  Vorgabe-Ziel ist out/mutos8000/…hfe im
Repo (gitignored); die eingecheckte Diskette wird nur mit --ueberschreiben ersetzt.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m8k  # noqa: E402
import wurzel_pruefen  # noqa: E402
import wurzel_quellbaum  # noqa: E402

EINGECHECKT = m8k.fertig("root_fm.hfe")
INODES = "104"      # s_isize 15 wie M8KROOT
# s_fsize 250 wie M8KROOT.  Dahinter liegt auf demselben Geraet (rk2, /dev/swap = 4,2) der
# AUSLAGERUNGSBEREICH des Kernels.  Mit s_fsize 494 (ganze Diskette) legte das Dateisystem Daten
# (u. a. /bin/sh) in den Swap-Bereich; nach Benutzung stand dort ein Prozessabbild, fsck sah nichts
# (Entwurf 39 4.8).
BLOECKE = "250"


def disktool(*args, ok=(0,)):
    cmd = [os.path.join(m8k.REPO, "tools", "dev.sh"), "tool", "k1520disktool"] + [str(x) for x in args]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=m8k.REPO)
    if r.returncode not in ok:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit("k1520disktool %s: Rueckgabe %d" % (args[0], r.returncode))
    return r.stdout


def ls_l(abbild):
    """`ls -l` ohne Kopfzeilen und ohne die Spalten, die vom Abbild (Zeitstempel der Anlage) abhaengen."""
    return [z for z in disktool("ls", abbild, "-l").splitlines() if z.strip() and not z.startswith((">>", "\x1b"))]


def als_td0(abbild, tmp, name):
    ziel = os.path.join(tmp, name)
    disktool("save-as", abbild, ziel)
    return ziel


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--ziel", default=os.path.join(m8k.REPO, "out", "mutos8000", "m8k_root_nachbau.hfe"),
                    help="Zieldatei des Nachbaus (Vorgabe: out/mutos8000/m8k_root_nachbau.hfe)")
    ap.add_argument("--ueberschreiben", action="store_true",
                    help="auch die eingecheckte rekonstruierte Diskette als Ziel zulassen / vorhandenes Ziel ersetzen")
    ap.add_argument("--vergleich", action="store_true", help="Nachbau gegen die eingecheckte Diskette vergleichen")
    ap.add_argument("--referenz", default=EINGECHECKT,
                    help="Diskette fuer --vergleich (Vorgabe: die eingecheckte rekonstruierte)")
    ap.add_argument("--behalten", metavar="ORDNER", help="Arbeitsordner behalten (Quellbaum, Auszug, Kopien)")
    a = ap.parse_args(argv)

    ziel = os.path.abspath(a.ziel)
    if os.path.realpath(ziel) == os.path.realpath(EINGECHECKT) and not a.ueberschreiben:
        raise SystemExit("Ziel ist die eingecheckte Diskette; nur mit --ueberschreiben")
    if os.path.exists(ziel) and not a.ueberschreiben:
        raise SystemExit("%s existiert; --ueberschreiben oder anderes --ziel" % ziel)
    os.makedirs(os.path.dirname(ziel), exist_ok=True)

    tmp = a.behalten or tempfile.mkdtemp(prefix="m8k_bau_")
    os.makedirs(tmp, exist_ok=True)
    try:
        # Kopien: der Emulator-/DiskTool-Zugriff soll eingecheckte Abbilder nie beruehren
        kopie = {}
        for n in ("root.td0", "m8k_cmd.td0", "m8k_root.td0"):
            kopie[n] = os.path.join(tmp, n)
            shutil.copyfile(m8k.disk(n), kopie[n])

        print("1. DISK-ROOT holen")
        auszug = os.path.join(tmp, "diskroot")
        shutil.rmtree(auszug, ignore_errors=True)
        disktool("get", kopie["root.td0"], "--to", auszug)

        print("2. Quellbaum bauen")
        src = os.path.join(tmp, "src")
        abw = wurzel_quellbaum.bauen(kopie["m8k_root.td0"], auszug, src)
        if abw:
            raise SystemExit("Programme aus DISK-ROOT weichen von lesbaren M8KROOT-Bytes ab: %d" % abw)

        print("3.-5. anlegen, Zylinder 0, einspielen")
        bau = os.path.join(tmp, "bau.hfe")
        if os.path.exists(bau):
            os.remove(bau)
        disktool("create", bau, "--fs", "mutos8k_fm", "--inodes", INODES, "--blocks", BLOECKE)
        boot = os.path.join(tmp, "cmdboot.bin")
        disktool("boot-get", kopie["m8k_cmd.td0"], boot)
        disktool("boot-put", bau, boot)
        disktool("put", bau, src)

        print("6. check --full")
        print(disktool("check", bau, "--full").strip().splitlines()[-1])

        print("7. unabhaengige Pruefung")
        if wurzel_pruefen.pruefen(als_td0(bau, tmp, "bau.td0"), src):
            raise SystemExit("Nachbau weicht vom Quellbaum ab")

        if os.path.exists(ziel):
            os.remove(ziel)
        shutil.copyfile(bau, ziel)
        print("Nachbau:", ziel)

        if a.vergleich:
            print("8. Vergleich mit", a.referenz)
            ref = os.path.join(tmp, "eingecheckt.hfe")
            shutil.copyfile(a.referenz, ref)
            if wurzel_pruefen.pruefen(als_td0(ref, tmp, "eingecheckt.td0"), src):
                raise SystemExit("eingecheckte Diskette weicht vom Quellbaum ab")
            la, lb = ls_l(bau), ls_l(ref)
            diff = [(x, y) for x, y in zip(la, lb) if x != y]
            print("ls -l: %d/%d Zeilen, %d verschieden" % (len(la), len(lb), len(diff)))
            for x, y in diff:
                print("  neu:  " + x + "\n  alt:  " + y)
            if len(la) != len(lb) or diff:
                return 1
    finally:
        if not a.behalten:
            shutil.rmtree(tmp, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

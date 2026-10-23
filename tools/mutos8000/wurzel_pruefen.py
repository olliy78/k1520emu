#!/usr/bin/env python3
"""Unabhaengige Pruefung einer Wurzeldiskette (Entwurf 39, 4.8): Baum eines heilen mutos8k_fm-Abbilds
(.td0/.imd; .hfe vorher mit `k1520disktool save-as x.hfe x.td0` umwandeln) byteweise gegen einen
Quellbaum.  Eigener Leser (Formel 4.6.3), kein DiskTool-Code.  Exit 1 bei Abweichung."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m8k  # noqa: E402


def pruefen(abbild, quelle, ausgabe=print):
    fs = m8k.Dateisystem(abbild)
    ausgabe("s_isize %d s_fsize %d" % (fs.isize, fs.fsize))
    stat = {"bad": 0, "seen": 0, "hoechster": 0}

    def walk(n, pfad):
        for ino, nm in fs.eintraege(n):
            p = pfad + "/" + nm
            mode = fs.inode(ino)[0]
            stat["seen"] += 1
            if mode & 0xF000 in (0x4000, 0x8000):
                stat["hoechster"] = max([stat["hoechster"]] + fs.belegt(ino))
            if mode & 0xF000 == 0x4000:
                walk(ino, p)
            elif mode & 0xF000 == 0x8000:
                ref = os.path.join(quelle, p[1:])
                if not (os.path.exists(ref) and open(ref, "rb").read() == fs.inhalt(ino)):
                    stat["bad"] += 1
                    ausgabe("ABWEICHUNG " + p)
            else:
                ad0 = fs.inode(ino)[5][0]
                ausgabe("  node %s %o %d,%d" % (p, mode, ad0 >> 8, ad0 & 255))

    ausgabe("root mode %o" % fs.inode(2)[0])
    walk(2, "")
    ausgabe("Eintraege %d Abweichungen %d, hoechster belegter Block %d" % (
        stat["seen"], stat["bad"], stat["hoechster"]))
    # Hinter s_fsize liegt der Auslagerungsbereich des Kernels (Entwurf 39 4.8): kein Block darf dorthin zeigen.
    if stat["hoechster"] >= fs.fsize:
        ausgabe("FEHLER: Block %d liegt hinter s_fsize %d" % (stat["hoechster"], fs.fsize))
        stat["bad"] += 1
    return stat["bad"]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("abbild", help="Wurzeldiskette (.td0 oder .imd)")
    ap.add_argument("quellbaum", help="Quellordner (wurzel_quellbaum.py)")
    a = ap.parse_args(argv)
    return 1 if pruefen(a.abbild, a.quellbaum) else 0


if __name__ == "__main__":
    sys.exit(main())

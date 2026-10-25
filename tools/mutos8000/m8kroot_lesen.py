#!/usr/bin/env python3
"""Befund der beschaedigten Wurzeldiskette M8KROOT (Entwurf 39, 4.7.6).

Liest M8KROOT mit der Blockabbildung von 4.6.3, schiebt jeden 128-B-Sektor um 1 Bit zurueck
(80 von 128 Byte brauchbar) und listet Superblock, Inodes, Verzeichnisbaum und die lesbaren Byte.
Mit --vergleich ORDNER wird jede Datei gleicher Groesse in den Ordnern auf Uebereinstimmung der
lesbaren Byte geprueft (Byte 1 jedes Sektors ohne oberstes Bit).
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m8k  # noqa: E402


def kandidaten(ordner):
    cand = {}
    for basis in ordner:
        for r, _d, fs in os.walk(basis):
            for f in fs:
                if f.endswith(".fileinfo"):
                    continue
                p = os.path.join(r, f)
                b = open(p, "rb").read()
                cand.setdefault(len(b), []).append((p, b))
    return cand


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("abbild", nargs="?", default=m8k.disk("m8k_root.td0"),
                    help="M8KROOT (.td0); Vorgabe: die eingecheckte Diskette")
    ap.add_argument("--inodes", action="store_true", help="auch Inode-Tabelle und Verzeichnis-Slots ausgeben")
    ap.add_argument("--vergleich", action="append", default=[], metavar="ORDNER",
                    help="Ordner mit herausgeholten Dateien (mehrfach); vergleicht nach Groesse")
    a = ap.parse_args(argv)

    w = m8k.BeschaedigteWurzel(a.abbild)
    print("isize", w.isize, "fsize", w.fsize)
    print("inodes belegt", len(w.inodes))
    baum = w.baum()
    print("root mode %o" % w.inodes[2].mode, "eintraege", len(baum))

    if a.inodes:
        print("ino off  typ  mode   nl uid gid size  ad0..")
        for n, i in sorted(w.inodes.items()):
            t = i.art()
            rd = "%d,%d" % (i.ad[0] >> 8, i.ad[0] & 255) if t in "cb" and i.ad else ""
            print("%3d %2d %s %06o %2d %3d %3d %6d  %s %s" % (n, i.off % 128, t, i.mode, i.nl, i.uid, i.gid,
                                                              i.size, rd, i.ad[:2]))
        for dn in range(2, 9):
            if dn not in w.inodes:
                continue
            dat, m = w.daten(dn)
            print("DIR", dn, len(dat))
            for k in range(0, len(dat), 16):
                if all(m[k:k + 16]) and len(m[k:k + 16]) == 16:
                    import struct
                    ino = struct.unpack(">H", dat[k:k + 2])[0]
                    print("  slot", k // 16, ino, dat[k + 2:k + 16].split(b"\0")[0])
                else:
                    print("  slot", k // 16, "?")

    for p, ino, mode, size, x in baum:
        if x is None:
            print("%-28s ino %3d mode %s size %s" % (p, ino, "%o" % mode if mode else "?", size))
            continue
        g, ln, dd = x
        voll = (g == size and ln == size)
        print("%-28s ino %3d %06o %6d  lesbar %5d B %s" % (p, ino, mode, size, g, "VOLL" if voll else ""))
        if voll and size <= 80:
            print("      ", dd)

    if a.vergleich:
        cand = kandidaten(a.vergleich)
        print("\nVergleich (nur lesbare Byte; Byte 1 jedes Sektors ohne Bit 7):")
        for p, ino, mode, size, x in baum:
            if not x or not size:
                continue
            dat, mask = w.daten(ino)
            hits = []
            for cp, cb in cand.get(size, []):
                ok = all(cb[k] == dat[k] or (k % 128 == 1 and (cb[k] & 0x7F) == dat[k])
                         for k in range(len(dat)) if mask[k])
                if ok:
                    hits.append(cp)
            print("%-14s %6d geprueft %5d B  -> %s" % (p, size, sum(mask), ", ".join(hits) or "-"))
    return 0


if __name__ == "__main__":
    sys.exit(main())

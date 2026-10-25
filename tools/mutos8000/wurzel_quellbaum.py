#!/usr/bin/env python3
"""Quellbaum der rekonstruierten Wurzel bauen (Entwurf 39, 4.8).

Aus M8KROOT (zurueckgeschoben, 4.7.6) und den Programmen der Diskette DISK-ROOT (Ordner, wie ihn
`k1520disktool get <DISK-ROOT> --to ORDNER` liefert).  Ergebnis: ein Ordner mit allen Dateien,
je Datei einem `.fileinfo` und dem Beiblatt `wega-dateiangaben.txt` (Geraete, Wurzel), geeignet
fuer `k1520disktool put`.  Die Programme aus DISK-ROOT werden NICHT eingecheckt.

Gemessen (aus M8KROOT): Baum, Modi, uid/gid, Geraetenummern, voll lesbare Dateien, Zeiten der
Inodes am Sektoranfang.  Abgeleitet: die Namen unten (NAMEN, DEV), Zeiten der Inodes am
Sektorende, Bit 0200 im Modus, die letzten 28 Byte von .profile, etc/mount/umount (Ersatz).
"""
import argparse
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m8k  # noqa: E402

# Programme aus DISK-ROOT (alle lesbaren Byte von M8KROOT stimmen ueberein).  Zielpfad -> Pfad im Auszug.
AUS_DISKROOT = {
    "bin/cat": "bin/cat", "bin/date": "bin/date", "bin/login": "bin/login", "bin/ls": "bin/ls",
    "bin/sh": "bin/sh", "bin/stty": "bin/stty", "bin/sync": "bin/sync",
    "etc/getty": "etc/getty", "etc/init": "etc/init",
    # mount/umount: Ersatz (DISK-ROOT-Fassungen sind kleiner), auf M8KROOT liegen sie in /etc
    "etc/mount": "bin/mount", "etc/umount": "bin/umount",
}

# Voll lesbare Dateien aus M8KROOT: Zielpfad -> Inode
AUS_M8KROOT = {"etc/group": 92, "etc/passwd": 87, "etc/ttys": 86, "etc/utmp": 96, "etc/motd": 90}
MTAB_INODE = 88    # 160 von 256 Byte lesbar, Rest Null ergaenzt

# .profile: 80 Byte lesbar, Rest (stty/date) aus dem DISK-ROOT-Text abgeleitet; Laenge 108 B geht auf
PROFILE = (b"PATH=:/mnt/bin:/usr/bin:/bin:/etc:/mnt/etc:/tmp/bin:/mnt:/usr:/tmp\n"
           b"export PATH\nstty length 23 width 79\ndate\n")

# Inode je Pfad (Namenszuordnung abgeleitet, Entwurf 39 4.7.6)
NAMEN = {
    "bin/cat": 102, "bin/date": 101, "bin/login": 98, "bin/ls": 97, "bin/sh": 95, "bin/stty": 94, "bin/sync": 93,
    "etc/getty": 100, "etc/init": 91, "etc/group": 92, "etc/passwd": 87, "etc/ttys": 86, "etc/utmp": 96,
    "etc/motd": 90, "etc/mount": 89, "etc/umount": 85, "etc/mtab": 88, ".profile": 104,
    "bin": 3, "etc": 4, "dev": 5, "tmp": 6, "usr": 7, "mnt": 8, ".": 2,
}
# Geraeteknoten in /dev (Major/Minor stehen in M8KROOT, nur die Namen sind abgeleitet)
DEV = {"console": 84, "kmem": 83, "lp": 82, "lp0": 81, "mem": 80, "null": 79, "rk0": 78, "rk1": 77, "rk2": 76,
       "rk3": 75, "rlp": 74, "rlp0": 73, "rrk0": 72, "rrk1": 71, "rrk2": 70, "rrk3": 69, "swap": 68, "tty": 67,
       "tty1": 66}
LEERE_VERZEICHNISSE = ("usr", "mnt", "tmp", "dev")


def bauen(m8kroot, diskroot, ziel):
    w = m8k.BeschaedigteWurzel(m8kroot)
    shutil.rmtree(ziel, ignore_errors=True)
    os.makedirs(ziel)

    def put(pfad, daten):
        p = os.path.join(ziel, pfad)
        os.makedirs(os.path.dirname(p) or ziel, exist_ok=True)
        open(p, "wb").write(daten)

    def voll(n):
        dd, mm = w.daten(n)
        assert all(mm) and len(dd) == w.inodes[n].size, n
        return dd

    dateien = {z: open(os.path.join(diskroot, q), "rb").read() for z, q in AUS_DISKROOT.items()}
    for z, n in AUS_M8KROOT.items():
        dateien[z] = voll(n)
    dateien["etc/mtab"] = w.daten(MTAB_INODE)[0]
    dateien[".profile"] = PROFILE
    for p, d in dateien.items():
        put(p, d)
    for d in LEERE_VERZEICHNISSE:
        os.makedirs(os.path.join(ziel, d), exist_ok=True)

    ino = dict(NAMEN)
    for k, v in DEV.items():
        ino["dev/" + k] = v

    lines = []
    order = [".", "bin", "dev", "etc", "mnt", "tmp", "usr"] + sorted(k for k in ino if "/" in k or k == ".profile")
    for p in order:
        n = ino[p]
        i = w.inodes[n]
        mode = i.mode
        if i.off % 128 == 0:
            mode |= 0o200       # Bit 0200 liegt im verlorenen obersten Bit von Byte 1
        t = w.zeit(n)
        art = i.art()
        e = "rdev=%d,%d" % (i.ad[0] >> 8, i.ad[0] & 255) if art in "cb" else ""
        lines.append("%s art=%s mode=%07o uid=%d gid=%d nlink=1 inode=0 mtime=%d atime=%d ctime=%d %s"
                     % (p, art, mode, i.uid, i.gid, t, t, t, e))
        if art in "cb":
            print(p, art, e, "%o" % mode)
        if art in "d-" and p != ".":
            put(p + ".fileinfo",
                ("fs=wega_mutos\nname=%s\nart=%s\nmode=%07o\nuid=%d\ngid=%d\nnlink=1\ninode=%d\n"
                 "mtime=%d\natime=%d\nctime=%d\n" % (p, art, mode, i.uid, i.gid, n, t, t, t)).encode())
    put("wega-dateiangaben.txt", ("\n".join(lines) + "\n").encode())

    # Gegenprobe: eingesetzte Programme gegen die lesbaren Byte von M8KROOT
    abw = 0
    for p, n in ino.items():
        if p in dateien and w.inodes[n].size == len(dateien[p]):
            dd, mm = w.daten(n)
            b = dateien[p]
            bad = [k for k in range(len(dd)) if mm[k] and dd[k] != b[k] and not (k % 128 == 1 and (b[k] & 0x7f) == dd[k])]
            abw += len(bad)
            print("%-12s %6d lesbar %5d abweichend %d" % (p, len(b), sum(mm), len(bad)))
        elif p in dateien:
            print("%-12s Groesse M8K %d / Ersatz %d" % (p, w.inodes[n].size, len(dateien[p])))
    return abw


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("diskroot", help="Auszug von DISK-ROOT (k1520disktool get ... --to ORDNER)")
    ap.add_argument("ziel", help="Zielordner (wird neu angelegt)")
    ap.add_argument("--m8kroot", default=m8k.disk("m8k_root.td0"), help="M8KROOT (.td0)")
    a = ap.parse_args(argv)
    abw = bauen(a.m8kroot, a.diskroot, a.ziel)
    print("Abweichungen gegen lesbare M8KROOT-Byte:", abw)
    return 1 if abw else 0


if __name__ == "__main__":
    sys.exit(main())

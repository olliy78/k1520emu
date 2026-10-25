#!/usr/bin/env python3
"""MUTOS 8000: "Best-of"-Datendisketten aus den beiden Originalsaetzen zusammenstellen (Entwurf 39, Abschnitt 10).

Quellen (disks/a5120_mutos8000/):
  DISK-Satz (17.03.1987, 8x512, gesund):  cmd, cc1, cc2, root     -> Kuerzel dcmd, cc1, cc2, droot
  M8K-Satz  (Januar 1986, teils defekt):   m8k_cmd, m8k_usr, m8k_bin -> Kuerzel mcmd, musr, mbin
Der Lader-Treiber `mutos_io` kann nur 26x128-Disketten (Verschraenkung 2, Spurversatz 6) lesen; die 8x512-Disketten
des DISK-Satzes sind im Emulator nicht einbindbar (er=0,304).  Deshalb wird der INHALT beider Saetze auf
26x128-Disketten (Profil mutos8k_fm/_mfm) neu verteilt.

Rollen (Mountpunkte der Wurzel, PATH der .profile = :/mnt/bin:/usr/bin:/bin:/etc:/mnt/etc:/tmp/bin:/mnt:/usr:/tmp):
  cmd  -> /mnt   bin/ + etc/ der Befehle (Gegenstueck M8KCMD)
  usr  -> /usr   bin/ include/ lib/ des C-Uebersetzers (Gegenstueck M8KUSR)
  bin  -> /tmp   Zusatzbefehle im Wurzelverzeichnis (Gegenstueck M8KBIN; /tmp steht im PATH)
  cc1, cc2 -> /usr  Inhalt von DISK-CC1 bzw. DISK-CC2 (1987) unveraendert auf 26x128; beide tragen lib/ und bin/, es kann nur
                    eine zugleich auf /usr liegen - die vollstaendige Kette steht auf usr

Je Datei gilt: die gesunde Fassung des DISK-Satzes (neuer), sonst die gesunde des M8K-Satzes; eine neuere Fassung, die
auf dem Kernel der Ladediskette (1986) nachweislich nicht laeuft (pwd), wird durch die aeltere ersetzt.  "Gesund" heisst: die
gelesene Groesse stimmt mit dem Inode, bei a.out-Programmen auch 16 + text + data + syms == Dateigroesse, bei
Textdateien kein NUL-Byte.  Die Entscheidungen stehen unten in AUSWAHL und als entscheidungen.txt im Archiv.

Das Skript erzeugt einen Zwischenstand (FM-Abbild + Beilagen) fuer archiv_erzeugen.py; die fertigen Disketten entstehen
erst aus dem Archiv (bau_aus_archiv.py).
"""
import argparse
import calendar
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import m8k  # noqa: E402

QUELLEN = {"dcmd": "cmd", "cc1": "cc1", "cc2": "cc2", "droot": "root",
           "mcmd": "m8k_cmd", "musr": "m8k_usr", "mbin": "m8k_bin"}

# (Ziel, Quelle, Pfad in der Quelle, Grund)
D, M = "DISK-Satz 1987 (neuer, heil)", "nur im M8K-Satz heil"
AUSWAHL = {
    "cmd": [
        ("bin/ar", "dcmd", "bin/ar", D + " (in allen Saetzen gleich)"),
        ("bin/cat", "droot", "bin/cat", D + " (gleich M8KCMD)"),
        ("bin/chmod", "dcmd", "bin/chmod", D + " (gleich)"),
        ("bin/chown", "dcmd", "bin/chown", D + " (gleich)"),
        ("bin/cmp", "droot", "bin/cmp", D + " (gleich M8KCMD)"),
        ("bin/convert", "dcmd", "bin/convert", D + " (gleich)"),
        ("bin/cp", "droot", "bin/cp", D + " (gleich M8KCMD)"),
        ("bin/dd", "droot", "bin/dd", D + " (gleich M8KCMD)"),
        ("bin/df", "droot", "bin/df", D + " (gleich M8KCMD)"),
        ("bin/echo", "droot", "bin/echo", D + " (gleich M8KCMD)"),
        ("bin/ed", "dcmd", "bin/ed", D + "; M8KUSR-Fassung (11410 B) ist unlesbar"),
        ("bin/hex", "dcmd", "bin/hex", D + " (gleich)"),
        ("bin/kill", "droot", "bin/kill", D + " (gleich M8KCMD)"),
        ("bin/ln", "dcmd", "bin/ln", D + " (gleich)"),
        ("bin/ls", "droot", "bin/ls", D + " (gleich M8KCMD)"),
        ("bin/mkdir", "droot", "bin/mkdir", D + " (gleich M8KCMD)"),
        ("bin/mv", "droot", "bin/mv", D + " (gleich M8KCMD)"),
        ("bin/nm", "cc2", "bin/nm", D + " (gleich M8KCMD)"),
        ("bin/od", "dcmd", "bin/od", D + " (gleich)"),
        ("bin/passwd", "mcmd", "bin/passwd", M + ": DISK-Satz hat kein passwd, M8KBIN-Kopie auf 7680 von 9140 B gekuerzt"),
        ("bin/pr", "dcmd", "bin/pr", D + "; M8KCMD-Fassung hat einen unlesbaren Block (CRC-Fehler)"),
        ("bin/ps", "dcmd", "bin/ps", D + " (7846 B; M8KCMD 7726 B ist aelter)"),
        ("bin/pstat", "dcmd", "bin/pstat", D + " (6866 B; M8KCMD 7286 B ist aelter)"),
        ("bin/pwd", "mcmd", "bin/pwd", M + ": die 1987er Fassung (DISK-ROOT, gleiche Groesse) gibt \"/\" aus und kehrt dann nie "
                                      "zur Shell zurueck (Emulator, Kernel der Ladediskette von 1986)"),
        ("bin/rm", "droot", "bin/rm", D + " (gleich M8KCMD)"),
        ("bin/rmdir", "dcmd", "bin/rmdir", D + " (gleich)"),
        ("bin/size", "dcmd", "bin/size", D + "; M8KBIN-Kopie besteht aus Fuellbytes"),
        ("bin/strip", "dcmd", "bin/strip", D + " (gleich)"),
        ("bin/wc", "dcmd", "bin/wc", D + "; M8KBIN-Kopie besteht aus Fuellbytes"),
        ("bin/who", "mcmd", "bin/who", M + ": im DISK-Satz nicht vorhanden"),
        ("etc/clri", "dcmd", "bin/clri", D + " (gleich M8KCMD); Lage wie auf M8KCMD (etc/, PATH enthaelt /mnt/etc)"),
        ("etc/dcheck", "droot", "bin/dcheck", D + " (gleich M8KCMD)"),
        ("etc/icheck", "droot", "bin/icheck", D + " (gleich M8KCMD)"),
        ("etc/mkfs", "droot", "etc/mkfs", D + " (gleiche Groesse wie M8KCMD, 1987 neu geschrieben)"),
        ("etc/mknod", "droot", "etc/mknod", D + " (gleich M8KCMD)"),
        ("etc/ncheck", "droot", "bin/ncheck", D + " (gleich M8KCMD)"),
    ],
    "usr": [
        ("bin/as", "cc1", "bin/as", D + " (35814 B; M8KUSR 32484 B ist aelter)"),
        ("bin/cc", "cc1", "bin/cc", D + " (6536 B; M8KUSR 7274 B ist aelter)"),
        ("bin/ld", "cc1", "bin/ld", D + " (10726 B)"),
        ("include/a.out.h", "cc1", "include/a.out.h", D + "; M8KUSR-Fassung ist Programmdaten statt Text"),
        ("include/ar.h", "cc1", "include/ar.h", D + " (gleich)"),
        ("include/ctype.h", "cc1", "include/ctype.h", D + "; M8KUSR-Fassung ist Programmdaten statt Text"),
        ("include/execargs.h", "cc1", "include/execargs.h", D + "; M8KUSR-Fassung ist Programmdaten statt Text"),
        ("include/math.h", "cc1", "include/math.h", D + "; M8KUSR-Fassung ist Programmdaten statt Text"),
        ("include/pwd.h", "cc1", "include/pwd.h", D + "; M8KUSR-Fassung ist Programmdaten statt Text"),
        ("include/sgtty.h", "cc1", "include/sgtty.h",
         "GEKUERZT auf die lesbaren ersten 512 B (struct sgttyb/tchars); die restlichen 1729 B sind in BEIDEN Saetzen "
         "Programmdaten statt Text - weder DISK-CC1 noch M8KUSR hat eine heile Fassung"),
        ("include/signal.h", "cc1", "include/signal.h", D + "; M8KUSR-Fassung ist Programmdaten statt Text"),
        ("include/stdio.h", "cc1", "include/stdio.h", D + "; M8KUSR-Fassung ist Programmdaten statt Text"),
        ("include/symbol.h", "cc1", "include/symbol.h", D + "; M8KUSR-Fassung ist Programmdaten statt Text"),
        ("include/time.h", "cc1", "include/time.h", D + "; M8KUSR-Fassung ist Programmdaten statt Text"),
        ("include/sys/stat.h", "musr", "include/sys/stat.h",
         M + ": DISK-CC1 fuehrt include/sys als 64-Byte-Datei mit falschen Typbits, stat.h/types.h fehlen dort"),
        ("include/sys/types.h", "musr", "include/sys/types.h", M + " (s. stat.h)"),
        ("lib/c0", "cc1", "lib/c0", D + "; M8KUSR-Fassung beginnt mit Programmdaten"),
        ("lib/c1", "cc1", "lib/c1", D + "; M8KUSR-Fassung hat einen unlesbaren Block"),
        ("lib/c2", "cc2", "lib/c2", D + " (18976 B)"),
        ("lib/cpp", "cc2", "lib/cpp", D + "; M8KUSR-Fassung hat einen wilden Indirektblock"),
        ("lib/crt0.o", "cc2", "lib/crt0.o", D + " (gleich)"),
        ("lib/libc.a", "cc2", "lib/libc.a", D + " (71914 B, 140 Glieder); M8KUSR-Fassung ab Offset 29810 Programmdaten"),
        ("lib/mcrt0.o", "cc2", "lib/mcrt0.o", D + " (gleich)"),
    ],
    # cc1/cc2: der Inhalt von DISK-CC1 bzw. DISK-CC2 (1987) unveraendert, nur auf 26x128 umgesetzt.  Weggelassen wie bei
    # usr: bin/c_beispiel.c (ab Byte 1155 Programmdaten) und die 64-Byte-"Datei" include/sys (Verzeichnis mit falschen Typbits).
    "cc1": [
        ("bin/as", "cc1", "bin/as", D),
        ("bin/cc", "cc1", "bin/cc", D),
        ("bin/ed", "cc1", "bin/ed", D + " (gleich DISK-CMD)"),
        ("bin/ld", "cc1", "bin/ld", D),
    ] + [("include/" + h, "cc1", "include/" + h, D) for h in
         ("a.out.h", "ar.h", "ctype.h", "execargs.h", "math.h", "pwd.h", "signal.h", "stdio.h", "symbol.h", "time.h")] + [
        ("include/sgtty.h", "cc1", "include/sgtty.h",
         "GEKUERZT auf die lesbaren ersten 512 B; der Rest (1729 B) ist in beiden Saetzen Programmdaten statt Text"),
        ("lib/c0", "cc1", "lib/c0", D),
        ("lib/c1", "cc1", "lib/c1", D),
    ],
    "cc2": [
        ("bin/nm", "cc2", "bin/nm", D),
        ("lib/c2", "cc2", "lib/c2", D),
        ("lib/cpp", "cc2", "lib/cpp", D),
        ("lib/crt0.o", "cc2", "lib/crt0.o", D),
        ("lib/libc.a", "cc2", "lib/libc.a", D + " (140 Glieder)"),
        ("lib/mcrt0.o", "cc2", "lib/mcrt0.o", D),
    ],
    "bin": [
        ("adb", "dcmd", "bin/adb", D + "; M8KCMD 27846 B ist die aeltere Fassung, M8KBIN ab 7680 B beschaedigt; "
                                  "wegen der Platzgrenze von cmd (2 Bloecke zu viel) hier statt auf cmd"),
        ("mt", "dcmd", "bin/mt", D + " (gleich M8KBIN); Zusatzbefehl, auf cmd ist kein Platz"),
        ("tar", "dcmd", "bin/tar", D + "; M8KBIN-Kopie ab 16896 B Fuellbytes"),
    ],
}
# Verzeichnisse: (Rolle, Pfad) -> Zeit "JJJJ-MM-TT HH:MM" (GMT), Eigentuemer 0/1, Modus 0775 wie auf M8KCMD/M8KUSR
VERZEICHNISSE = {
    "cmd": {"": "1986-01-30 10:18", "bin": "1986-01-30 10:11", "etc": "1986-01-30 10:18"},
    "usr": {"": "1986-01-09 22:37", "bin": "1986-01-09 22:14", "include": "1986-01-09 22:25",
            "include/sys": "1986-01-09 22:34", "lib": "1986-01-09 22:31"},
    "bin": {"": "1986-01-30 10:18"},
    "cc1": {"": "1987-03-17 21:56", "bin": "1987-03-17 21:56", "include": "1987-03-17 21:56", "lib": "1987-03-17 21:56"},
    "cc2": {"": "1987-03-17 22:28", "bin": "1987-03-17 22:28", "lib": "1987-03-17 22:28"},
}
INODES = {"cmd": 56, "usr": 48, "bin": 16, "cc1": 48, "cc2": 16}      # wie M8KCMD/M8KUSR; die Zusatzdiskette braucht 3 Dateien
BLOECKE = 494                                    # ganze 8-Zoll-Diskette (76 Spuren x 26 / 4); kein Auslagerungsbereich
ROLLEN = ("cmd", "usr", "bin", "cc1", "cc2")
# Dateien, von denen nur der lesbare Anfang existiert: Ziel -> Laenge
KUERZEN = {"include/sgtty.h": 512}
# Bewusst WEGGELASSEN (in keinem Satz heil): bin/c_beispiel.c (DISK-CC1, ab Byte 1155 Programmdaten),
# include/grp.h (nur M8KUSR, Programmdaten), bin/mkdir.tot (DISK-ROOT, Duplikat von mkdir)


def epoche(text):
    return calendar.timegm(time.strptime(text, "%Y-%m-%d %H:%M"))


def disktool(*args):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import bau_aus_archiv
    return bau_aus_archiv.disktool(*args)


def aout_heil(daten):
    """a.out (Magic E807): 16 + text + data + syms == Groesse.  Kein a.out -> True (nicht beurteilbar)."""
    if len(daten) < 16 or struct.unpack(">H", daten[:2])[0] != 0xE807:
        return True
    _, text, data, _, syms = struct.unpack(">5H", daten[:10])
    return len(daten) == 16 + text + data + syms


def fileinfo_umschreiben(text, name):
    return "".join(("name=%s\n" % name) if z.startswith("name=") else z + "\n" for z in text.splitlines())


def verzeichnis_info(name, zeit):
    t = epoche(zeit)
    return ("fs=wega_mutos\nname=%s\nart=d\nmode=0040775\nuid=0\ngid=1\nnlink=2\nmtime=%d\natime=%d\nctime=%d\n"
            % (name or ".", t, t, t))


def holen(tmp):
    """Alle benoetigten Dateien je Quelle mit EINEM `get --fileinfo` holen: tmp/<kuerzel>/<pfad>."""
    ordner = {}
    for kz, abbild in QUELLEN.items():
        muster = sorted({p for rolle in AUSWAHL.values() for (_, q, p, _) in rolle if q == kz})
        if not muster:
            continue
        ziel = os.path.join(tmp, "quelle_" + kz)
        os.makedirs(ziel)
        disktool("get", m8k.disk(abbild + ".td0"), *muster, "--to", ziel, "--fileinfo")
        ordner[kz] = ziel
    return ordner


def zusammenstellen(rolle, stand_ordner, ausgabe=print):
    """Stagingordner einer Rolle bauen und Pruefungen fahren; Rueckgabe: Entscheidungstext."""
    if rolle not in AUSWAHL:
        raise SystemExit("Rolle: " + " | ".join(ROLLEN))
    tmp = tempfile.mkdtemp(prefix="m8k_best_")
    zeilen = []
    try:
        quellen = holen(tmp)
        for ziel, kz, pfad, grund in AUSWAHL[rolle]:
            quelle = os.path.join(quellen[kz], pfad)
            daten = open(quelle, "rb").read()
            if not ziel.endswith(".o") and not aout_heil(daten):     # .o tragen Relokationen hinter den Symbolen
                raise SystemExit("%s (%s): a.out-Groesse passt nicht zum Kopf" % (pfad, kz))
            if ziel in KUERZEN:
                daten = daten[:KUERZEN[ziel]]
                open(quelle, "wb").write(daten)
            if ziel.endswith((".h", ".c")) and b"\x00" in daten:
                raise SystemExit("%s (%s): Textdatei mit NUL-Byte (beschaedigt)" % (pfad, kz))
            z = os.path.join(stand_ordner, ziel)
            os.makedirs(os.path.dirname(z), exist_ok=True)
            shutil.copyfile(quelle, z)
            info = open(quelle + ".fileinfo", encoding="utf-8").read()
            open(z + ".fileinfo", "w", encoding="utf-8").write(fileinfo_umschreiben(info, ziel))
            zeilen.append("%-22s %7d B  %-5s %-22s %s" % (ziel, len(daten), kz, QUELLEN[kz] + ":" + pfad, grund))
        for pfad, zeit in VERZEICHNISSE[rolle].items():
            if pfad:
                os.makedirs(os.path.join(stand_ordner, pfad), exist_ok=True)
                open(os.path.join(stand_ordner, pfad + ".fileinfo"), "w", encoding="utf-8").write(
                    verzeichnis_info(pfad, zeit))
        # Wurzelverzeichnis: nur ueber das Beiblatt
        t = epoche(VERZEICHNISSE[rolle][""])
        open(os.path.join(stand_ordner, "wega-dateiangaben.txt"), "w", encoding="utf-8").write(
            ". art=d mode=0040775 uid=0 gid=1 nlink=2 inode=2 mtime=%d atime=%d ctime=%d\n" % (t, t, t))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    kopf = ("MUTOS 8000, Datendiskette '%s' - Zusammenstellung aus dem DISK-Satz (1987) und dem M8K-Satz (1986)\n"
            "Ziel | Groesse | Quelle | Herkunft | Grund\n\n" % rolle)
    return kopf + "\n".join(zeilen) + "\n"


def zwischenstand(rolle, ziel_hfe, ausgabe=print):
    """Rolle -> FM-Abbild `ziel_hfe` (mutos8k_fm) und Entscheidungstext (Rueckgabe)."""
    tmp = tempfile.mkdtemp(prefix="m8k_best_stand_")
    try:
        stand = os.path.join(tmp, "stand")
        os.makedirs(stand)
        text = zusammenstellen(rolle, stand, ausgabe)
        z0 = os.path.join(tmp, "zylinder0.bin")
        # Zylinder 0 mit den Etiketten ERMAP/VOL1MUTOS: gleich dem der Wurzel (gleich M8KCMD)
        disktool("boot-get", m8k.fertig("root_fm.hfe"), z0)
        disktool("create", ziel_hfe, "--fs", "mutos8k_fm", "--inodes", INODES[rolle], "--blocks", BLOECKE)
        disktool("boot-put", ziel_hfe, z0)
        disktool("put", ziel_hfe, stand)
        ausgabe(disktool("check", ziel_hfe, "--full").strip().splitlines()[-1])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return text


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("rolle", choices=ROLLEN)
    ap.add_argument("ziel", help="Zwischenstand .hfe (FM)")
    a = ap.parse_args(argv)
    print(zwischenstand(a.rolle, a.ziel))
    return 0


if __name__ == "__main__":
    sys.exit(main())

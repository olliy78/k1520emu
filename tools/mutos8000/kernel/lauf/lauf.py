#!/usr/bin/env python3
"""K6a: Befehlsabdeckung des Kernels mutos_1216 im Emulator sammeln (Entwurf 41, doc/mutos_kernel/kernel_struktur.md).

Aufruf (Repo-Wurzel):  python3 -I tools/mutos8000/kernel/lauf/lauf.py [boot|shell|cc|alle]
Je Lauf entsteht ein k1520dbg-Skript (im Arbeitsordner), der Lauf selbst (tools/dev.sh tool k1520dbg --em em256 ...)
und `abdeckung_<name>.txt` hier: eine Befehlsanfangsadresse des Kernels (Systembetrieb, Segment 0) je Zeile,
hex vierstellig, sortiert.  `abdeckung_gesamt.txt` ist die Vereinigung aller vorhandenen Laeufe.
Rohbericht von `cover report` (U8001-Kontext): Zeilen "SS:OOOO S|N Zaehler".
"""
import os, subprocess, sys, tempfile, glob

HIER = os.path.dirname(os.path.abspath(__file__))
WURZEL = os.path.abspath(os.path.join(HIER, "..", "..", "..", ".."))
DISKS = os.path.join(WURZEL, "disks", "a5120_16_MUTOS")
BOOT = os.path.join(DISKS, "a5120_mutos8000_boot_udos_k5601.hfe")
KERNEL_ENDE = 0x8E00          # Abbildgroesse 36352 B; darueber liegt BSS/Heap -- nur Abbild zaehlt als Karte

def kopf(laufwerke):
    """Anfang jedes Laufs: Kaltstart, Ladediskette, Wurzel, Anmeldung als root.  laufwerke = [(Buchstabe, Datei)]."""
    z = ["cpu u8000", "cover on", "g 80000000", "keys BOOT\\sMUTOS\\r", "g 100000000", "eject A"]
    for b, d in laufwerke:
        z.append("mount %s %s udos_ss77" % (b, d))
    z += ["keys r", "g 100000000", "keys root\\r", "g 100000000"]
    return z

def befehl(z, text, nr, grenze=400000000):
    """Befehl tippen und auf eine Marke warten, die nur als AUSGABE (nicht als Eingabe) auf dem Bild erscheint."""
    z.append("keys %s;echo\\sEN\\x5CDE%d\\r" % (text.replace(" ", "\\s"), nr))
    z.append('gscreen "ENDE%d" %d' % (nr, grenze))

def skript_boot():
    z = kopf([("C", os.path.join(DISKS, "a5120_mutos8000_root_mfm.hfe"))])
    z += ["keys ls\\s-l\\s/\\r", "g 60000000", "keys cat\\s/etc/passwd\\r", "g 60000000", "screen"]
    return z

def skript_shell():
    z = kopf([("B", os.path.join(DISKS, "a5120_mutos8000_usr_mfm.hfe")),
              ("C", os.path.join(DISKS, "a5120_mutos8000_root_mfm.hfe")),
              ("D", os.path.join(DISKS, "a5120_mutos8000_cmd_mfm.hfe"))])
    z += ["keys /etc/mount\\s/dev/rk3\\s/mnt\\r", "g 40000000", "keys /etc/mount\\s/dev/rk1\\s/usr\\r", "g 40000000",
          "keys PATH=/bin:/mnt/bin:.\\r", "g 20000000"]
    cmds = [
        "ls -l /", "ls -l /dev", "cat /etc/passwd /etc/ttys", "pwd", "date", "df", "who", "ps",
        # Fehlerfaelle: fehlende Datei, Verzeichnis lesen, Nicht-Verzeichnis, Ausfuehren ohne x-Recht
        "cat /gibtsnicht", "cat /etc", "rm /gibtsnicht", "cd /gibtsnicht", "mkdir /etc/passwd/x", "ln /gibtsnicht /tmp/l",
        # Dateien schreiben, kopieren, verschieben, loeschen, Rechte
        "mkdir /tmp/t", "cp /etc/passwd /tmp/t/p", "ls -l /tmp/t", "/tmp/t/p", "chmod 755 /tmp/t/p", "chmod 000 /tmp/t/p",
        "cp /bin/sh /tmp/t/sh2", "cp /tmp/t/sh2 /tmp/t/sh3", "ls -l /tmp/t", "mv /tmp/t/sh3 /tmp/t/sh4", "ln /tmp/t/sh4 /tmp/t/sh5",
        "cmp /tmp/t/sh2 /tmp/t/sh4", "od /tmp/t/p", "wc /tmp/t/sh2", "rm /tmp/t/sh4", "rm /tmp/t/sh5", "rm /tmp/t/sh2 /tmp/t/p", "rmdir /tmp/t",
        "chown 1 /etc/ttys", "ls -l /etc/ttys", "chown 0 /etc/ttys",
        # Pipes und Hintergrund
        "ls /bin | cat", "ls -l /bin | wc", "cat /etc/passwd | wc | cat", "ls /bin /mnt/bin | pr | wc",
        "cat /etc/passwd > /tmp/o; wc /tmp/o; rm /tmp/o", "ls -l / > /tmp/x &", "wait", "cat &", "kill $!", "kill -9 99", "ps",
        "pstat", "stty", "sync", "df", "/etc/umount /dev/rk3", "ls /mnt", "/etc/mount /dev/rk3 /mnt", "ls /mnt",
        "echo $PATH `pwd`", "sh -c 'ls /'", "ed /etc/ttys </dev/null",
    ]
    for i, c in enumerate(cmds, 1):
        befehl(z, c, i)
    z.append("screen")
    return z

def skript_cc(quelle):
    z = kopf([("B", os.path.join(DISKS, "a5120_mutos8000_usr_mfm.hfe")),
              ("C", os.path.join(DISKS, "a5120_mutos8000_root_mfm.hfe")),
              ("D", quelle)])
    z += ["keys /etc/mount\\s/dev/rk1\\s/usr\\r", "g 30000000", "keys /etc/mount\\s/dev/rk3\\s/mnt\\r", "g 30000000",
          "keys cd\\s/mnt\\r", "g 20000000", "keys PATH=/bin:/usr/bin:.\\r", "g 20000000"]
    folge = [("cc -c t2.c", 900000000), ("cc -c t3.c", 1500000000), ("cc t2.o t3.o", 1500000000), ("./a.out", 300000000),
             ("cc -c fehler.c", 900000000), ("cc -S t2.c", 900000000), ("cc -c nichtda.c", 300000000), ("ls -l", 60000000)]
    for i, (c, g) in enumerate(folge, 1):
        z.append("keys %s;cat\\sfertig%d\\r" % (c.replace(" ", "\\s"), i))
        z.append('gscreen "ENDEN" %d' % g if False else 'gscreen "ENDE%d" %d' % (i, g))
    z.append("screen")
    return z

def quelldiskette(arbeit):
    """Quelldiskette (D:) mit den Probeprogrammen; fertigN enthaelt ENDEn als Marke."""
    pfad = os.path.join(arbeit, "quelle.hfe")
    if os.path.exists(pfad):
        os.remove(pfad)
    probe = os.path.join(HIER, "..", "probe")
    fehler = os.path.join(arbeit, "fehler.c")
    open(fehler, "w").write("main()\n{\n\tint x\n\tx = = 1;\n\treturn y;\n}\n")
    dateien = [os.path.join(probe, "t2.c"), os.path.join(probe, "t3.c"), fehler]
    for i in range(1, 9):
        f = os.path.join(arbeit, "fertig%d" % i)
        open(f, "w").write("ENDE%d\n" % i)
        dateien.append(f)
    dt = [os.path.join(WURZEL, "tools", "dev.sh"), "tool", "k1520disktool"]
    subprocess.run(dt + ["create", pfad, "--fs", "mutos8k_26x128_mfm", "--inodes", "24"], check=True, cwd=WURZEL)
    subprocess.run(dt + ["put", pfad] + dateien, check=True, cwd=WURZEL)
    return pfad

def auswerten(roh, ziel):
    """Rohbericht -> Befehlsanfaenge des Kernels (Segment 0, Systembetrieb, unterhalb KERNEL_ENDE), sortiert, hex."""
    adr = set()
    for zeile in open(roh):
        try:
            sa, bm, _ = zeile.split()
            seg, off = (int(x, 16) for x in sa.split(":"))
        except ValueError:
            continue
        if seg == 0 and bm == "S" and off < KERNEL_ENDE:
            adr.add(off)
    with open(ziel, "w") as f:
        f.write("".join("%04X\n" % a for a in sorted(adr)))
    return len(adr)

def lauf(name, skript, arbeit):
    roh = os.path.join(arbeit, "roh_%s.txt" % name)
    skript = skript + ["cover off", "cover report " + roh, "q"]
    dbg = os.path.join(arbeit, "lauf_%s.dbg" % name)
    open(dbg, "w").write("\n".join(skript) + "\n")
    mit = subprocess.run([os.path.join(WURZEL, "tools", "dev.sh"), "tool", "k1520dbg", "--em", "em256", "--laufwerke",
                          "K5601,K5601,K5601,K5601", BOOT, "-x", dbg], cwd=WURZEL, capture_output=True, text=True)
    open(os.path.join(arbeit, "lauf_%s.log" % name), "w").write(mit.stdout + mit.stderr)
    n = auswerten(roh, os.path.join(HIER, "abdeckung_%s.txt" % name))
    print("%s: %d Befehlsanfaenge" % (name, n))

def main():
    wahl = sys.argv[1] if len(sys.argv) > 1 else "alle"
    arbeit = os.environ.get("K6A_ARBEIT") or tempfile.mkdtemp(prefix="k6a_")
    os.makedirs(arbeit, exist_ok=True)
    if wahl in ("boot", "alle"):
        lauf("boot", skript_boot(), arbeit)
    if wahl in ("shell", "alle"):
        lauf("shell", skript_shell(), arbeit)
    if wahl in ("cc", "alle"):
        lauf("cc", skript_cc(quelldiskette(arbeit)), arbeit)
    gesamt = set()
    for f in glob.glob(os.path.join(HIER, "abdeckung_*.txt")):
        if not f.endswith("_gesamt.txt"):
            gesamt.update(int(l, 16) for l in open(f) if l.strip())
    with open(os.path.join(HIER, "abdeckung_gesamt.txt"), "w") as f:
        f.write("".join("%04X\n" % a for a in sorted(gesamt)))
    print("gesamt: %d Befehlsanfaenge (Arbeitsordner %s)" % (len(gesamt), arbeit))

if __name__ == "__main__":
    main()

"""Gast-`as` aus dem Host heraus fahren (AP K4, Entwurf 41).

Baut aus Quelltexten eine Quelldiskette (mutos8k_26x128_mfm), startet MUTOS im Emulator (k1520dbg, Stapelskript),
laesst im Gast `as <teil>.s` laufen und holt je Teil den Bildschirmtext des `as` (<teil>.err) und das Objekt
(<teil>.o, vom `as` als `asm.out` geschrieben) zurueck.

Aufruf als Modul (`python3 -I`), siehe rundlauf_gast.py.  Alles Erzeugte liegt im Arbeitsordner; die Plattenabbilder
sind KOPIEN (der Lauf schreibt: --rw).
"""
import os
import shutil
import subprocess
import sys
import time

WURZEL = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
DISKS = os.path.join(WURZEL, "disks", "a5120_16_MUTOS")
DISKTOOL = os.path.join(WURZEL, "build", "k1520disktool")
DEV = os.path.join(WURZEL, "tools", "dev.sh")

PLATTEN = {
    "boot.hfe": "a5120_mutos8000_boot_udos_k5601.hfe",
    "usr.hfe": "a5120_mutos8000_usr_mfm.hfe",
    "root.hfe": "a5120_mutos8000_root_mfm.hfe",
}


class Fehlt(Exception):
    """Ein Hilfsmittel ist nicht da (Aufrufer meldet Exit 77)."""


def pruefe_hilfsmittel():
    for p in [DISKTOOL, DEV] + [os.path.join(DISKS, n) for n in PLATTEN.values()]:
        if not os.path.exists(p):
            raise Fehlt(p)


def _lauf(args, **kw):
    r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kw)
    return r.returncode, r.stdout.decode("latin-1")


def dbg_skript(arbeit, teile, kommando=">asm.out;as %s.s >/tmp/%s.err 2>&1;cat asm.out >/tmp/%s.o", frist=3_000_000_000):
    """Skript fuer k1520dbg.  Je Teil ein Befehl, danach `cat fertigN` (eigene Marke ENDE<N>, eindeutig)."""
    z = []
    z.append("# erzeugt von gast_as.py -- nicht von Hand aendern")
    z.append("g 80000000")
    z.append("keys BOOT\\sMUTOS\\r")
    z.append("g 100000000")
    z.append("eject A")
    z.append("mount A %s/tmp.hfe udos_ss77" % arbeit)
    z.append("mount B %s/usr.hfe udos_ss77" % arbeit)
    z.append("mount C %s/root.hfe udos_ss77" % arbeit)
    z.append("mount D %s/quelle.hfe udos_ss77" % arbeit)
    z.append("keys r")
    z.append("g 100000000")
    z.append("keys root\\r")
    z.append("g 100000000")
    z.append("keys /etc/mount\\s/dev/rk1\\s/usr\\r")
    z.append("g 30000000")
    z.append("keys /etc/mount\\s/dev/rk3\\s/mnt\\r")
    z.append("g 30000000")
    z.append("keys /etc/mount\\s/dev/rk0\\s/tmp\\r")
    z.append("g 30000000")
    z.append("keys cd\\s/mnt\\r")
    z.append("g 26000000")
    z.append("keys PATH=/bin:/usr/bin:.\\r")
    z.append("g 26000000")
    for i, name in enumerate(teile, 1):
        k = kommando.replace("%s", name)
        z.append("keys %s;cat\\sfertig%d\\r" % (k.replace(" ", "\\s"), i))
        z.append('gscreen "ENDE%d" %d' % (i, frist))
        z.append("g 4000000")          # erst nach dem Prompt weitertippen (gscreen schlaegt schon bei der Marke an)
    z.append("keys sync;cat\\sfertigs\\r")
    z.append('gscreen "ENDES" 300000000')
    z.append("g 60000000")
    z.append("q")
    return "\n".join(z) + "\n"


def gast_assemblieren(arbeit, teile, ausgabe=None, kommando=">asm.out;as %s.s >/tmp/%s.err 2>&1;cat asm.out >/tmp/%s.o",
                    frist=3_000_000_000):
    """teile: {name: quelltext}.  Gibt {name: (fehlertext, objektbytes oder None)} zurueck, dazu die Laufzeit."""
    pruefe_hilfsmittel()
    arbeit = os.path.abspath(arbeit)
    os.makedirs(arbeit, exist_ok=True)
    for ziel, quelle in PLATTEN.items():
        shutil.copyfile(os.path.join(DISKS, quelle), os.path.join(arbeit, ziel))
    q = os.path.join(arbeit, "q")
    shutil.rmtree(q, ignore_errors=True)
    os.makedirs(q)
    namen = list(teile)
    for n, t in teile.items():
        with open(os.path.join(q, n + ".s"), "w", newline="\n") as f:
            f.write(t)
    for i in range(1, len(namen) + 1):
        with open(os.path.join(q, "fertig%d" % i), "w") as f:
            f.write("ENDE%d\n" % i)
    with open(os.path.join(q, "fertigs"), "w") as f:
        f.write("ENDES\n")
    gesamt = sum(len(t) for t in teile.values())
    if gesamt > 260_000:
        raise ValueError("Quellen zu gross fuer eine Diskette (%d B > 260000); in Stapeln aufteilen" % gesamt)
    for img in ("tmp.hfe", "quelle.hfe"):
        p = os.path.join(arbeit, img)
        if os.path.exists(p):
            os.remove(p)
    rc, o = _lauf([DISKTOOL, "create", os.path.join(arbeit, "tmp.hfe"), "--fs", "mutos8k_26x128_mfm", "--inodes", str(2 * len(namen) + 24)])
    if rc:
        raise RuntimeError("create tmp: " + o)
    rc, o = _lauf([DISKTOOL, "create", os.path.join(arbeit, "quelle.hfe"), "--fs", "mutos8k_26x128_mfm", "--inodes",
                   str(4 * len(namen) + 24)])
    if rc:
        raise RuntimeError("create quelle: " + o)
    dateien = [os.path.join(q, f) for f in sorted(os.listdir(q))]
    for i in range(0, len(dateien), 20):
        rc, o = _lauf([DISKTOOL, "put", os.path.join(arbeit, "quelle.hfe")] + dateien[i:i + 20])
        if rc:
            raise RuntimeError("put: " + o)
    skript = os.path.join(arbeit, "lauf.dbg")
    with open(skript, "w") as f:
        f.write(dbg_skript(arbeit, namen, kommando, frist))
    log = os.path.join(arbeit, "lauf.log")
    t0 = time.time()
    with open(log, "w") as lf:
        r = subprocess.run([DEV, "tool", "k1520dbg", "--rw", "--em", "em256", "--laufwerke", "K5601,K5601,K5601,K5601",
                            os.path.join(arbeit, "boot.hfe"), "-x", skript], stdout=lf, stderr=subprocess.STDOUT,
                           cwd=WURZEL)
    dauer = time.time() - t0
    if ausgabe is not None:
        ausgabe["dauer"] = dauer
        ausgabe["log"] = log
    holen = os.path.join(arbeit, "zurueck")
    shutil.rmtree(holen, ignore_errors=True)
    os.makedirs(holen)
    erg = {}
    for n in namen:
        for ext in (".err", ".o"):
            _lauf([DISKTOOL, "get", os.path.join(arbeit, "tmp.hfe"), n + ext, "--to", holen])
    for n in namen:
        fe = os.path.join(holen, n + ".err")
        ob = os.path.join(holen, n + ".o")
        err = open(fe, "rb").read().decode("latin-1") if os.path.exists(fe) else None
        obj = open(ob, "rb").read() if os.path.exists(ob) else None
        erg[n] = (err, obj)
    return erg


def gast_befehle(arbeit, quellen, befehle, rueck, frist=3_000_000_000, extra_dateien=None, ausgabe=None):
    """Allgemeiner Gastlauf (AP K5): `quellen` {name: bytes|str} kommen auf die Quelldiskette D (im Gast /mnt = Arbeitsordner),
    `befehle` laufen nacheinander in der Shell (Rueckmeldung je Befehl ueber eine Marke), danach werden die Dateien
    `rueck` aus /tmp (eigene leere Diskette A) geholt.  Rueckgabe {name: bytes oder None}.
    Befehle schreiben ihre Meldungen selbst nach /tmp/<name> (`>/tmp/x.err 2>&1`)."""
    pruefe_hilfsmittel()
    arbeit = os.path.abspath(arbeit)
    os.makedirs(arbeit, exist_ok=True)
    for ziel, quelle in PLATTEN.items():
        shutil.copyfile(os.path.join(DISKS, quelle), os.path.join(arbeit, ziel))
    q = os.path.join(arbeit, "q")
    shutil.rmtree(q, ignore_errors=True)
    os.makedirs(q)
    for n, t in quellen.items():
        with open(os.path.join(q, n), "wb") as f:
            f.write(t if isinstance(t, bytes) else t.encode("latin-1"))
    for i in range(1, len(befehle) + 1):
        with open(os.path.join(q, "fertig%d" % i), "w") as f:
            f.write("ENDE%d\n" % i)
    with open(os.path.join(q, "fertigs"), "w") as f:
        f.write("ENDES\n")
    for img in ("tmp.hfe", "quelle.hfe"):
        p = os.path.join(arbeit, img)
        if os.path.exists(p):
            os.remove(p)
    rc, o = _lauf([DISKTOOL, "create", os.path.join(arbeit, "tmp.hfe"), "--fs", "mutos8k_26x128_mfm", "--inodes", "200"])
    if rc:
        raise RuntimeError("create tmp: " + o)
    rc, o = _lauf([DISKTOOL, "create", os.path.join(arbeit, "quelle.hfe"), "--fs", "mutos8k_26x128_mfm", "--inodes",
                   str(len(quellen) + len(befehle) + 40)])
    if rc:
        raise RuntimeError("create quelle: " + o)
    dateien = [os.path.join(q, f) for f in sorted(os.listdir(q))]
    for i in range(0, len(dateien), 20):
        rc, o = _lauf([DISKTOOL, "put", os.path.join(arbeit, "quelle.hfe")] + dateien[i:i + 20])
        if rc:
            raise RuntimeError("put: " + o)
    skript = os.path.join(arbeit, "lauf.dbg")
    with open(skript, "w") as f:
        f.write(dbg_skript(arbeit, befehle, "%s", frist))
    log = os.path.join(arbeit, "lauf.log")
    t0 = time.time()
    with open(log, "w") as lf:
        subprocess.run([DEV, "tool", "k1520dbg", "--rw", "--em", "em256", "--laufwerke", "K5601,K5601,K5601,K5601",
                        os.path.join(arbeit, "boot.hfe"), "-x", skript], stdout=lf, stderr=subprocess.STDOUT, cwd=WURZEL)
    if ausgabe is not None:
        ausgabe["dauer"] = time.time() - t0
        ausgabe["log"] = log
    holen = os.path.join(arbeit, "zurueck")
    shutil.rmtree(holen, ignore_errors=True)
    os.makedirs(holen)
    erg = {}
    for n in rueck:
        _lauf([DISKTOOL, "get", os.path.join(arbeit, "tmp.hfe"), n, "--to", holen])
        pf = os.path.join(holen, n)
        erg[n] = open(pf, "rb").read() if os.path.exists(pf) else None
    return erg

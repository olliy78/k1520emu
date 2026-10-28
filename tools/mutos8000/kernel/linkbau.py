"""Kernel aus MODULEN neu binden (AP K5, Entwurf 41 §4.2): z8kdis (Linkkarte) -> Gast-`as -u` je Modul -> Gast-`ld` -> Vergleich.

Ablauf (Linux-Seite):
  1. mutos_1216 aus dem Ladedisketten-Archiv holen,
  2. je Modul der Linkkarte (`linkkarte.txt`) `z8kdis --format as --modul NAME` -> `s/NAME.s` (mit --schreibe-s eingecheckt),
     dazu das Modul `bss` (Gemeinsame Variablen),
  3. im Gast: `as -u -o /tmp/NAME.o NAME.s` je Modul (-u: unbekannte Namen sind extern), danach
     `ld -s -o /tmp/lk <Module in Linkreihenfolge> bss`  (-s: ohne Symbole; Ausgabe = Kopf 16 B + Text + Daten),
  4. Text+Daten zurueckholen (die BSS-Variablen liegen als nullgefuellte Daten am Ende und werden abgeschnitten), auf die
     512-B-Rundung des Originals (36 352 B) bringen, vergleichen.
  Verschiebe-Test (--verschiebe MODUL): dasselbe mit einem `nop` am Ende des Moduls; schreibt das Ergebnis nach
  ARBEIT/mutos_verschoben (Boot: siehe doc/mutos_kernel/linkbau.md).

Aufruf:   python3 -I tools/mutos8000/kernel/linkbau.py [--arbeit DIR] [--nur-quelle] [--schreibe-s] [--verschiebe prf]
Exit:     0 = bytegleich, 1 = Abweichung/Fehler im Gast, 77 = Hilfsmittel fehlen.   Rechenzeit: Minuten (Hintergrund).
"""
import argparse
import os
import struct
import subprocess
import sys
import tempfile

HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HIER)
import gast_as          # noqa: E402
import rundlauf_gast    # noqa: E402

WURZEL = gast_as.WURZEL
KARTE = os.path.join(HIER, "karte.txt")
LINKKARTE = os.path.join(HIER, "linkkarte.txt")
S_ORDNER = os.path.join(HIER, "s")
Z8KDIS = rundlauf_gast.Z8KDIS
KERNEL_GROESSE = rundlauf_gast.KERNEL_GROESSE


def modulnamen():
    """Module in Linkreihenfolge aus der Linkkarte."""
    r = []
    for z in open(LINKKARTE):
        p = z.split()
        if len(p) >= 2 and p[0] == "modul":
            r.append(p[1])
    return r


def quellen_erzeugen(abbild_pfad):
    q = {}
    for n in modulnamen() + ["bss"]:
        p = subprocess.run([Z8KDIS, "--karte", KARTE, "--karte", LINKKARTE, "--format", "as", "--modul", n, abbild_pfad],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if p.returncode:
            raise RuntimeError("z8kdis %s: %s" % (n, p.stderr.decode("latin-1")))
        q[n] = p.stdout.decode("latin-1")
    return q


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--arbeit")
    ap.add_argument("--nur-quelle", action="store_true")
    ap.add_argument("--schreibe-s", action="store_true", help="Quellen nach tools/mutos8000/kernel/s/ schreiben")
    ap.add_argument("--verschiebe", metavar="MODUL", help="ein nop ans Ende des Moduls (Verschiebe-Test)")
    ap.add_argument("--frist", type=int, default=20_000_000_000)
    a = ap.parse_args(argv)
    for p in (rundlauf_gast.ARCHIV, KARTE, LINKKARTE, Z8KDIS):
        if not os.path.exists(p):
            print("Hilfsmittel fehlt:", p, file=sys.stderr)
            return 77
    try:
        gast_as.pruefe_hilfsmittel()
    except gast_as.Fehlt as e:
        print("Hilfsmittel fehlt:", e, file=sys.stderr)
        return 77
    arbeit = os.path.abspath(a.arbeit) if a.arbeit else tempfile.mkdtemp(prefix="linkbau_")
    os.makedirs(arbeit, exist_ok=True)
    abbild = rundlauf_gast.kernel_holen()
    assert len(abbild) == KERNEL_GROESSE
    apfad = os.path.join(arbeit, "mutos_1216")
    open(apfad, "wb").write(abbild)
    q = quellen_erzeugen(apfad)
    if a.schreibe_s:
        os.makedirs(S_ORDNER, exist_ok=True)
        for n, t in q.items():
            open(os.path.join(S_ORDNER, n + ".s"), "w", newline="\n").write(t)
    print("%d Module, Quelle %d Byte" % (len(q), sum(len(t) for t in q.values())))
    if a.nur_quelle:
        return 0
    namen = list(q)
    quellen = {n + ".s": t for n, t in q.items()}
    befehle = ["as -u -o /tmp/%s.o %s.s >/tmp/%s.err 2>&1" % (n, n, n) for n in namen]
    # Linkreihenfolge: Module der Karte, dann bss (Gemeinsames wird erst beim Binden zugeteilt)
    rueck = ["ld.err"] + [n + ".err" for n in namen]
    if not a.verschiebe:        # (die /tmp-Diskette fasst nur ein Bindeergebnis; Verschiebe-Test = eigener Lauf)
        quellen["ldlauf"] = "ld -s -o /tmp/lk " + " ".join("/tmp/%s.o" % n for n in namen) + "\n"
        befehle.append("sh ldlauf >/tmp/ld.err 2>&1")
        rueck.append("lk")
    else:
        v = a.verschiebe
        t = q[v]
        i = t.rindex("end _p_")
        quellen["v_" + v + ".s"] = t[:i] + "\tnop\n" + t[i:]
        befehle.append("as -u -o /tmp/v_%s.o v_%s.s >/tmp/v.err 2>&1" % (v, v))
        quellen["ldlauf2"] = "ld -s -o /tmp/lk2 " + " ".join("/tmp/%s.o" % ("v_" + n if n == v else n) for n in namen) + "\n"
        befehle.append("sh ldlauf2 >/tmp/ld2.err 2>&1")
        rueck += ["v.err", "ld2.err", "lk2"]
    befehle.append("rm /tmp/*.o")
    aus = {}
    erg = gast_as.gast_befehle(os.path.join(arbeit, "gast"), quellen, befehle, rueck, frist=a.frist, ausgabe=aus)
    print("Gast-Laufzeit (Wanduhr): %.0f s, Protokoll %s" % (aus["dauer"], aus["log"]))
    fehler = 0
    for n in ([] if a.verschiebe else ["ld.err"]) + [x + ".err" for x in namen]:
        t = erg.get(n)
        if t is None:
            print("%s fehlt" % n); fehler += 1
        elif t.strip():
            print("%s:\n%s" % (n, t.decode("latin-1"))); fehler += 1

    def abzug(b, name):
        if not b:
            print("%s: nicht erzeugt" % name)
            return None
        magic, text, data, bss = struct.unpack(">4H", b[:8])
        print("%s: Text %d Daten %d bss %d" % (name, text, data, bss))
        return b[16:16 + text + data]
    ist = None if a.verschiebe else abzug(erg.get("lk"), "lk")
    rc = 1
    if a.verschiebe:
        rc = 0
    if ist is not None:
        ist = ist[:KERNEL_GROESSE].ljust(KERNEL_GROESSE, b"\0")
        d = [i for i in range(min(len(ist), KERNEL_GROESSE)) if ist[i] != abbild[i]]
        gleich = len(ist) == KERNEL_GROESSE and not d
        print("Gesamt: %s (%d Byte ungleich; erste bei %s)" % ("BYTEGLEICH" if gleich else "ABWEICHUNG", len(d),
                                                                  ", ".join("%04X" % x for x in d[:8])))
        open(os.path.join(arbeit, "mutos_1216.neu"), "wb").write(ist)
        rc = 0 if gleich and not fehler else 1
    if a.verschiebe:
        i2 = abzug(erg.get("lk2"), "lk2")
        if i2 is not None:
            open(os.path.join(arbeit, "mutos_verschoben"), "wb").write(i2[:KERNEL_GROESSE + 2].ljust(KERNEL_GROESSE + 2, b"\0"))
    return rc


if __name__ == "__main__":
    sys.exit(main())

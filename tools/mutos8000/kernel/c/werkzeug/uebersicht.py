#!/usr/bin/env python3
"""Gesamtuebersicht des C-Rueckbaus: alle Kernelfunktionen nach Modul mit Status.

Aufruf: python3 -I uebersicht.py KERNEL OBJEKTVERZEICHNIS... [--md]
  Universum = kernel/lauf/funktionen.txt (Anfang, Laenge, csv-Spalte), Modulgrenzen = kernel/linkkarte.txt.
  Zu jeder c/<modul>.c wird unter den Verzeichnissen (jeweils <dir>/<modul>O.o, auch <dir>/out/) das Objekt mit den meisten
  gleichen Funktionen genommen und per vergleich.py geprueft.  Status je Funktion:
    gleich      alle Woerter (ohne Relokationen) wie im Kernel
    nahe        hoechstens NAHE_MAX abweichende Woerter (Eigenheit des aelteren Kernel-c2, siehe c_rueckbau.md)
    offen       in C vorhanden, aber weiter abweichend
    kein C      Marke fehlt (noch nicht zurueckgebaut)
    Assembler   Funktion ohne `call csv` (Handassembler, nicht C-erreichbar)
"""
import glob
import os
import re
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
C = os.path.join(HIER, "..")
K = os.path.join(C, "..")
sys.path.insert(0, C)
sys.path.insert(0, K)
import vergleich  # noqa: E402

NAHE_MAX = 30


def lese_funktionen():
    f = []
    for l in open(os.path.join(K, "lauf", "funktionen.txt")):
        if l.startswith("#") or not l.strip():
            continue
        p = l.split()
        if not re.match(r"^[0-9A-F]{4}$", p[0]):
            continue
        f.append((int(p[0], 16), int(p[1]), p[2] != "---"))
    return f


def lese_module():
    m = []
    for l in open(os.path.join(K, "linkkarte.txt")):
        p = l.split()
        if len(p) >= 4 and p[0] == "modul":
            m.append((p[1], int(p[2], 16), int(p[3], 16)))
    return m


def main(argv):
    md = "--md" in argv
    argv = [a for a in argv if a != "--md"]
    kernel = open(argv[0], "rb").read()
    dirs = argv[1:]
    funk = lese_funktionen()
    module = lese_module()
    status = {}  # adr -> (status, quelle, info)
    for q in sorted(glob.glob(os.path.join(C, "*.c"))):
        name = os.path.basename(q)[:-2]
        if not list(vergleich.funktionen_aus_quelle(q)):
            continue
        best = None
        for d in dirs:
            for p in (os.path.join(d, name + "O.o"), os.path.join(d, "out", name + "O.o")):
                if not os.path.exists(p):
                    continue
                try:
                    res = vergleich.vergleiche(kernel, p, q)
                except Exception:
                    continue
                g = sum(1 for r in res if r[2] == "GLEICH")
                if best is None or g > best[0]:
                    best = (g, res)
        if best is None:
            continue
        for adr, fn, st, info in best[1]:
            if st == "GLEICH":
                s = "gleich"
            elif st == "ABWEICHEND":
                m = re.search(r"(\d+) von", info)
                n = int(m.group(1)) if m else 99
                s = "nahe" if n <= NAHE_MAX else "offen"
                info = "%d W" % n
            else:
                continue
            # nur die beste Quelle je Adresse (gleich > nahe > offen)
            rang = {"gleich": 0, "nahe": 1, "offen": 2}
            if adr not in status or rang[s] < rang[status[adr][0]]:
                status[adr] = (s, "%s.c" % name, fn, info if s != "gleich" else "")
    zeilen = []
    summe = {}
    for adr, ln, csv in funk:
        mod = next((n for n, v, b in module if v <= adr < b), "?")
        if adr in status:
            s, qn, fn, info = status[adr]
        elif not csv:
            s, qn, fn, info = "Assembler", "-", "", ""
        else:
            s, qn, fn, info = "kein C", "-", "", ""
        zeilen.append((mod, adr, ln, fn, s, qn, info))
        summe.setdefault(mod, {}).setdefault(s, 0)
        summe[mod][s] += 1
    order = ["gleich", "nahe", "offen", "kein C", "Assembler"]
    if md:
        print("| Modul | gleich | nahe | offen | kein C | Assembler | Summe |")
        print("|-------|-------:|-----:|------:|-------:|----------:|------:|")
    tot = dict.fromkeys(order, 0)
    for n, v, b in module:
        if n not in summe:
            continue
        row = [summe[n].get(s, 0) for s in order]
        for s, x in zip(order, row):
            tot[s] += x
        print("| %s | %s | %d |" % (n, " | ".join(str(x) for x in row), sum(row)))
    print("| **Summe** | %s | %d |" % (" | ".join(str(tot[s]) for s in order), sum(tot.values())))
    if "--liste" in sys.argv:
        for z in zeilen:
            print("%-6s %04X %4d %-8s %-10s %-9s %s" % z)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

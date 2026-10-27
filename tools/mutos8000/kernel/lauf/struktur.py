#!/usr/bin/env python3
"""K6a: Strukturdaten des Kernels aus Abbild + Karte: Textpool und Funktionsliste (Eingabe fuer doc/mutos_kernel/kernel_struktur.md).

Aufruf (Repo-Wurzel):  python3 -I tools/mutos8000/kernel/lauf/struktur.py ABBILD [ausgabeordner]
Schreibt textpool.txt und funktionen.txt.  Voraussetzung: build/z8kdis (vorher tools/dev.sh tool z8kdis --help) und karte.txt.
Alles daraus ist Messung am Abbild (belegt, soweit Code durch den Lauf bestaetigt, sonst statisch erreicht).
"""
import os, re, subprocess, sys, collections

HIER = os.path.dirname(os.path.abspath(__file__))
WURZEL = os.path.abspath(os.path.join(HIER, "..", "..", "..", ".."))
ZEILE = re.compile(r"^([0-9A-F]{4})  ((?:[0-9A-F]{2,4} ?)+) +(\S+)\s*(.*)$")

def main():
    bild = sys.argv[1]
    aus = sys.argv[2] if len(sys.argv) > 2 else HIER
    d = open(bild, "rb").read()
    lst = subprocess.run([os.path.join(WURZEL, "build", "z8kdis"), "--karte", os.path.join(HIER, "..", "karte.txt"), bild],
                         capture_output=True, text=True, check=True).stdout
    zeilen = []
    for z in lst.splitlines():
        m = ZEILE.match(z)
        if m:
            zeilen.append((int(m.group(1), 16), len(m.group(2).replace(" ", "")) // 2, m.group(3), m.group(4)))
    kodiert = bytearray(len(d))
    for a, n, mn, op in zeilen:
        if mn not in ("DB", "DW"):
            for k in range(n):
                kodiert[a + k] = 1
    treffer = {int(z, 16) for z in open(os.path.join(HIER, "abdeckung_gesamt.txt")) if z.strip()}
    # --- Textpool: druckbare Laeufe >= 4 Zeichen, mit NUL beendet, nicht in Code
    texte = []
    i = 0
    while i < len(d):
        j = i
        while j < len(d) and (32 <= d[j] < 127 or d[j] in (9, 10)):
            j += 1
        t = d[i:j].decode("latin1")
        # Text, nicht Zufallsbytes: mindestens 4 Buchstaben und 80 % Buchstaben/Ziffern/Leerzeichen/Formatzeichen
        echt = sum(c.isalpha() for c in t) >= 4 and sum(c.isalnum() or c in " %=/:,.+?<>\n()^-_" for c in t) >= 0.8 * len(t)
        if j - i >= 4 and j < len(d) and d[j] == 0 and not any(kodiert[i:j]) and echt:
            texte.append((i, d[i:j].decode("latin1")))
            i = j + 1
        else:
            i = max(j, i + 1)
    # Verweise auf Texte: Direktoperanden (#%ADR) in Befehlen
    imm = collections.defaultdict(list)
    for a, n, mn, op in zeilen:
        if mn in ("DB", "DW"):
            continue
        for w in re.findall(r"#(?:%|L)([0-9A-F]{4})\b", op):
            imm[int(w, 16)].append(a)
    with open(os.path.join(aus, "textpool.txt"), "w") as f:
        f.write("# Adresse  Verweise(Befehle mit #ADR)  Text\n")
        for a, t in texte:
            v = imm.get(a, [])
            f.write("%04X  %-4d %s\n" % (a, len(v), t.replace("\n", "\\n").replace("\t", "\\t")))
    # --- Funktionen
    ruf = collections.defaultdict(set)      # Ziel -> Aufrufer-Befehle
    for a, n, mn, op in zeilen:
        if mn in ("CALL", "CALR"):
            m = re.match(r"L([0-9A-F]{4})", op)
            if m:
                ruf[int(m.group(1), 16)].add(a)
    csv = 0x05F8
    start = set(ruf)
    for a, n, mn, op in zeilen:
        if mn == "CALL" and op.startswith("L05F8"):
            start.add(a)
    start |= {0x027E, 0x030E}
    start = sorted(s for s in start if kodiert[s])
    imm_all = set(imm)
    with open(os.path.join(aus, "funktionen.txt"), "w") as f:
        f.write("# Anfang  Laenge  csv  Aufrufer  gelaufen  Ziele(Aufrufe)   Texte\n")
        info = {a: (n, mn, op) for a, n, mn, op in zeilen}
        for k, s in enumerate(start):
            e = start[k + 1] if k + 1 < len(start) else len(d)
            ende = e
            # Ende = letztes Code-Byte vor dem naechsten Anfang
            while ende > s and not kodiert[ende - 1]:
                ende -= 1
            inn = [(a, n, mn, op) for a, n, mn, op in zeilen if s <= a < ende and mn not in ("DB", "DW")]
            ziele = collections.Counter()
            for a, n, mn, op in inn:
                if mn in ("CALL", "CALR"):
                    m = re.match(r"L([0-9A-F]{4})", op)
                    ziele[m.group(1) if m else op.split()[0]] += 1
            txt = [t for a, t in texte if any(s <= b < ende for b in imm.get(a, []))]
            hat_csv = bool(inn) and inn[0][2] == "CALL" and inn[0][3].startswith("L05F8")
            gel = sum(1 for a, n, mn, op in inn if a in treffer)
            f.write("%04X  %5d  %s  %3d  %3d/%-3d  %s   %s\n" % (
                s, ende - s, "csv" if hat_csv else "---", len(ruf.get(s, ())), gel, len(inn),
                " ".join("%s(%d)" % kv for kv in sorted(ziele.items())),
                "|".join(x[:40] for x in txt)))
    print("Texte:", len(texte), " Funktionen:", len(start))

if __name__ == "__main__":
    main()

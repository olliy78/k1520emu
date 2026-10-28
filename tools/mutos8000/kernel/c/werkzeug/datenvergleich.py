#!/usr/bin/env python3
"""Vergleicht den DATEN-Teil eines .o des MUTOS-cc mit den Bytes des Kernels.

Aufruf: python3 -I datenvergleich.py KERNEL OBJEKT ADRESSE [-v]
  ADRESSE (hex) = Kernelanschrift des ersten Datenbytes des Objekts.  Relokierte Woerter (Adressen von Funktionen/Variablen) sind
  Wildcards.  Ausgabe: Laenge, Zahl abweichender Woerter, Liste der ersten Abweichungen.  Dient conf.c (Tabellen in initialisierten Daten).
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
import objformat  # noqa: E402


def main(argv):
    v = "-v" in argv
    argv = [a for a in argv if a != "-v"]
    if len(argv) != 3:
        print(__doc__)
        return 2
    kernel = open(argv[0], "rb").read()
    o = objformat.Objekt.lesen(open(argv[1], "rb").read())
    adr = int(argv[2], 16)
    wild = set(off for off, _ in o.relocs("data"))
    abw = []
    for i in range(0, len(o.data), 2):
        x, y = o.data[i:i + 2], kernel[adr + i:adr + i + 2]
        if i not in wild and x != y:
            abw.append((adr + i, x.hex(), y.hex()))
        if v:
            print("%04X %s %s %s" % (adr + i, x.hex(), y.hex(), "~" if i in wild else ("=" if x == y else "!")))
    print("Daten ab %04X: %d B, %d Wildcard-Woerter, %d abweichende Woerter" % (adr, len(o.data), len(wild), len(abw)))
    for a, x, y in abw[:20]:
        print("  %04X obj %s kern %s" % (a, x, y))
    return 0 if not abw else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3
"""Vergleicht uebersetzte C-Funktionen (.o des MUTOS-cc) mit den Bytes im Kernel mutos_1216.

Aufruf: python3 -I vergleich.py KERNEL OBJEKT QUELL.c [-v] [-m]
  -m   Modulvergleich: der gesamte Text des .o (Funktionen in Quellreihenfolge = Kernelreihenfolge) wird gegen den
       Kernelbereich ab der Adresse der ersten Marke verglichen; Ausgabe: Laenge, Zahl abweichender Woerter, erste Abweichung.
Die Quelle traegt je Funktion einen Kommentar  /* KERNEL xxxx */  vor der Funktion; der naechste
Funktionsname dahinter ("name(") ist das Symbol _name im Objekt.  Der Objekttext der Funktion
reicht vom Symbol bis zum naechsten Textsymbol der Datei.  Relokierte Woerter (Aufrufziele,
Globale) sind Wildcards, im Kernel wie im Objekt.  Ausgabe je Funktion: GLEICH / ABWEICHEND
(Laenge, Zahl abweichender Woerter) / FEHLT.  -v zeigt die Woerter nebeneinander.
"""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import objformat  # noqa: E402


def funktionen_aus_quelle(pfad):
    q = open(pfad, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r"/\*\s*KERNEL\s+([0-9A-Fa-f]{4})\b[^\n]*\*/\s*\n(?:[^\n(;{}]*\n)?\s*(\w+)\s*\(", q):
        yield int(m.group(1), 16), m.group(2)


def vergleiche(kernel, obj, quelle, ausfuehrlich=False):
    o = objformat.Objekt.lesen(open(obj, "rb").read())
    text, maske = o.maskierter_text()
    tsym = sorted((s.wert, s.name) for s in o.syms if (s.typ & 0x3F) == objformat.N_TEXT | objformat.N_EXT)
    wert = {n: w for w, n in tsym}
    ende_liste = sorted(set(w for w, _ in tsym) | {len(o.text)})
    ergebnis = []
    for adr, name in funktionen_aus_quelle(quelle):
        sym = ("_" + name).encode()[:8]
        if sym not in wert:
            ergebnis.append((adr, name, "FEHLT", ""))
            continue
        a = wert[sym]
        e = min(x for x in ende_liste if x > a)
        mine = text[a:e]
        mm = maske[a:e]
        k = kernel[adr:adr + len(mine)]
        wörter = [(mine[i:i + 2], k[i:i + 2], mm[i]) for i in range(0, len(mine), 2)]
        abw = [i for i, (x, y, mk) in enumerate(wörter) if not mk and x != y]
        info = "%d B (Kernel-Funktion hier %d B)" % (len(mine), len(k))
        if not abw:
            ergebnis.append((adr, name, "GLEICH", info))
        else:
            ergebnis.append((adr, name, "ABWEICHEND", info + ", %d von %d Woertern" % (len(abw), len(wörter))))
        if ausfuehrlich:
            for i, (x, y, mk) in enumerate(wörter):
                flag = "~" if mk else ("=" if x == y else "!")
                ergebnis.append((adr, name, "  %04X" % (adr + 2 * i), "%s obj %s  kern %s" % (flag, x.hex(), y.hex())))
    return ergebnis


def modulvergleich(kernel, obj, quelle):
    """Ganzes Objekt gegen den Kernelbereich: (Adresse, Laenge, Zahl abweichender Woerter, erste Abweichung oder None)."""
    o = objformat.Objekt.lesen(open(obj, "rb").read())
    text, maske = o.maskierter_text()
    marken = list(funktionen_aus_quelle(quelle))
    if not marken:
        return None
    adr = min(a for a, _ in marken)
    k = kernel[adr:adr + len(text)]
    abw = [i for i in range(0, len(text), 2) if not maske[i] and text[i:i + 2] != k[i:i + 2]]
    return adr, len(text), len(abw), (adr + abw[0]) if abw else None


def main(argv):
    v = "-v" in argv
    m = "-m" in argv
    argv = [a for a in argv if a not in ("-v", "-m")]
    if len(argv) != 3:
        print(__doc__)
        return 2
    kernel = open(argv[0], "rb").read()
    if m:
        r = modulvergleich(kernel, argv[1], argv[2])
        if r is None:
            print("keine Marken in der Quelle")
            return 1
        adr, laenge, nabw, erste = r
        print("Modul ab %04X: %d B, %d abweichende Woerter%s" % (adr, laenge, nabw,
              "" if erste is None else ", erste bei %04X" % erste))
        return 0 if nabw == 0 else 1
    res = vergleiche(kernel, argv[1], argv[2], v)
    gleich = 0
    for adr, name, st, info in res:
        print("%04X %-10s %-10s %s" % (adr, name, st, info))
        gleich += st == "GLEICH"
    n = sum(1 for r in res if r[2] in ("GLEICH", "ABWEICHEND", "FEHLT"))
    print("gleich: %d von %d" % (gleich, n))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

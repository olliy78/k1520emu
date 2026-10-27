#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
dis.py  -  Z80-Disassembler, der wieder ASSEMBLIERBAREN M80-Quelltext erzeugt
=============================================================================

Arbeitswerkzeug fuer mutos_io (Entwurf 40).  Anders als tools/z80_disasm2.py
gibt es kein Listing, sondern eine .MAC-Quelle: Marken statt Adressen, DB/DW/
DEFM fuer Daten.  Richtig ist, was beim Neubau BYTEGLEICH herauskommt
(build.py --check) - ob eine Stelle als Code oder als Daten gefuehrt wird,
darf also falsch sein, ohne dass die Bytes falsch werden.

Die Einteilung (Code/Daten/Text/Wort) steht in der Kartendatei (karte.txt):

    seg  <adresse> <datei-offset> <laenge>        ein Ladesegment der Datei
    code <adresse>                                Einstieg (Wurzel der Verfolgung)
    db   <von> <bis>                              Daten, Bytes
    dw   <von> <bis>                              Daten, Woerter (Adressen -> Marken)
    txt  <von> <bis>                              Text (Zeichenkette, Schluss siehe unten)
    name <adresse> <marke>                        eigene Marke statt Lxxxx
    kom  <adresse> <text>                         Kommentar hinter/vor der Stelle
    eq   <marke> <adresse>                        Marke ausserhalb der Segmente (EQU)
"""

import os
import re
import sys

ADR = bool(os.environ.get('DIS_ADR'))

R8 = ['B', 'C', 'D', 'E', 'H', 'L', '(HL)', 'A']
RP = ['BC', 'DE', 'HL', 'SP']
RP2 = ['BC', 'DE', 'HL', 'AF']
CC = ['NZ', 'Z', 'NC', 'C', 'PO', 'PE', 'P', 'M']
ALU = ['ADD A,', 'ADC A,', 'SUB ', 'SBC A,', 'AND ', 'XOR ', 'OR ', 'CP ']
ROT = ['RLC', 'RRC', 'RL', 'RR', 'SLA', 'SRA', 'SLL', 'SRL']


def hx(v, n=2):
    s = f"{v:0{n}X}"
    return ('0' + s if s[0] in 'ABCDEF' else s) + 'H'


class Speicher:
    """Die Ladesegmente im Adressraum; ausserhalb: unbekannt."""

    def __init__(self):
        self.b = {}

    def laden(self, adr, daten):
        for i, x in enumerate(daten):
            self.b[adr + i] = x

    def __contains__(self, a):
        return a in self.b

    def __getitem__(self, a):
        return self.b[a]


def entschluesseln(m, a):
    """Eine Anweisung bei @a.  Rueckgabe (vorlage, laenge, ziele, ende, abs_ziel).

    @vorlage ist Text mit {a} fuer ein Adresswort (nn) bzw. {j} fuer ein
    Sprungziel; die Marken werden erst bei der Ausgabe eingesetzt.
    """
    def b(n):
        if a + n not in m:
            raise IndexError
        return m[a + n]

    def w(n):
        return b(n) | (b(n + 1) << 8)

    def d(n):
        v = b(n)
        return f"+{v}" if v < 128 else f"-{256 - v}"

    op = b(0)
    if op == 0xCB:
        x = b(1)
        r = R8[x & 7]
        if x < 0x40:
            return f"{ROT[x >> 3]} {r}", 2, [], False, None
        return f"{['BIT', 'RES', 'SET'][(x >> 6) - 1]} {(x >> 3) & 7},{r}", 2, [], False, None
    if op in (0xDD, 0xFD):
        ix = 'IX' if op == 0xDD else 'IY'
        x = b(1)
        if x == 0xCB:
            y = b(3)
            if (y & 7) != 6:
                raise ValueError("undokumentiert")
            t = f"({ix}{d(2)})"
            if y < 0x40:
                return f"{ROT[y >> 3]} {t}", 4, [], False, None
            return f"{['BIT', 'RES', 'SET'][(y >> 6) - 1]} {(y >> 3) & 7},{t}", 4, [], False, None
        rr = lambda i: ix if i == 2 else RP[i]
        if x == 0x21:
            return f"LD {ix},{{a}}", 4, [], False, w(2)
        if x == 0x22:
            return f"LD ({{a}}),{ix}", 4, [], False, w(2)
        if x == 0x2A:
            return f"LD {ix},({{a}})", 4, [], False, w(2)
        if x == 0x23:
            return f"INC {ix}", 2, [], False, None
        if x == 0x2B:
            return f"DEC {ix}", 2, [], False, None
        if x in (0x09, 0x19, 0x29, 0x39):
            return f"ADD {ix},{rr((x >> 4) & 3)}", 2, [], False, None
        if x == 0xE1:
            return f"POP {ix}", 2, [], False, None
        if x == 0xE5:
            return f"PUSH {ix}", 2, [], False, None
        if x == 0xE3:
            return f"EX (SP),{ix}", 2, [], False, None
        if x == 0xE9:
            return f"JP ({ix})", 2, [], True, None
        if x == 0xF9:
            return f"LD SP,{ix}", 2, [], False, None
        if x == 0x36:
            return f"LD ({ix}{d(2)}),{hx(b(3))}", 4, [], False, None
        if x in (0x34, 0x35):
            return f"{'INC' if x == 0x34 else 'DEC'} ({ix}{d(2)})", 3, [], False, None
        if (x & 0xC7) == 0x46:
            return f"LD {R8[(x >> 3) & 7]},({ix}{d(2)})", 3, [], False, None
        if (x & 0xF8) == 0x70 and x != 0x76:
            return f"LD ({ix}{d(2)}),{R8[x & 7]}", 3, [], False, None
        if (x & 0xC7) == 0x86:
            return f"{ALU[(x >> 3) & 7]}({ix}{d(2)})", 3, [], False, None
        raise ValueError("undokumentiert")
    if op == 0xED:
        x = b(1)
        if (x & 0xC7) == 0x40 and x != 0x70:
            return f"IN {R8[(x >> 3) & 7]},(C)", 2, [], False, None
        if (x & 0xC7) == 0x41 and x != 0x71:
            return f"OUT (C),{R8[(x >> 3) & 7]}", 2, [], False, None
        if (x & 0xCF) == 0x42:
            return f"SBC HL,{RP[(x >> 4) & 3]}", 2, [], False, None
        if (x & 0xCF) == 0x4A:
            return f"ADC HL,{RP[(x >> 4) & 3]}", 2, [], False, None
        if (x & 0xCF) == 0x43:
            return f"LD ({{a}}),{RP[(x >> 4) & 3]}", 4, [], False, w(2)
        if (x & 0xCF) == 0x4B:
            return f"LD {RP[(x >> 4) & 3]},({{a}})", 4, [], False, w(2)
        t = {0x44: 'NEG', 0x45: 'RETN', 0x4D: 'RETI', 0x46: 'IM 0', 0x56: 'IM 1',
             0x5E: 'IM 2', 0x47: 'LD I,A', 0x4F: 'LD R,A', 0x57: 'LD A,I',
             0x5F: 'LD A,R', 0x67: 'RRD', 0x6F: 'RLD', 0xA0: 'LDI', 0xA1: 'CPI',
             0xA2: 'INI', 0xA3: 'OUTI', 0xA8: 'LDD', 0xA9: 'CPD', 0xAA: 'IND',
             0xAB: 'OUTD', 0xB0: 'LDIR', 0xB1: 'CPIR', 0xB2: 'INIR', 0xB3: 'OTIR',
             0xB8: 'LDDR', 0xB9: 'CPDR', 0xBA: 'INDR', 0xBB: 'OTDR'}.get(x)
        if t is None:
            raise ValueError("undokumentiert")
        return t, 2, [], x in (0x45, 0x4D), None
    # unprefixed
    x = op >> 6
    y = (op >> 3) & 7
    z = op & 7
    if x == 1:
        if op == 0x76:
            return "HALT", 1, [], False, None
        return f"LD {R8[y]},{R8[z]}", 1, [], False, None
    if x == 2:
        return f"{ALU[y]}{R8[z]}", 1, [], False, None
    if x == 0:
        if z == 0:
            if y == 0:
                return "NOP", 1, [], False, None
            if y == 1:
                return "EX AF,AF'", 1, [], False, None
            e = b(1)
            t = a + 2 + (e - 256 if e >= 128 else e)
            if y == 2:
                return "DJNZ {j}", 2, [t], False, ('j', t)
            if y == 3:
                return "JR {j}", 2, [t], True, ('j', t)
            return f"JR {CC[y - 4]},{{j}}", 2, [t], False, ('j', t)
        if z == 1:
            if y & 1:
                return f"ADD HL,{RP[y >> 1]}", 1, [], False, None
            return f"LD {RP[y >> 1]},{{a}}", 3, [], False, w(1)
        if z == 2:
            t = ['LD (BC),A', 'LD A,(BC)', 'LD (DE),A', 'LD A,(DE)'][y] if y < 4 else None
            if t:
                return t, 1, [], False, None
            return ['LD ({a}),HL', 'LD HL,({a})', 'LD ({a}),A', 'LD A,({a})'][y - 4], 3, [], False, w(1)
        if z == 3:
            return f"{'DEC' if y & 1 else 'INC'} {RP[y >> 1]}", 1, [], False, None
        if z == 4:
            return f"INC {R8[y]}", 1, [], False, None
        if z == 5:
            return f"DEC {R8[y]}", 1, [], False, None
        if z == 6:
            return f"LD {R8[y]},{hx(b(1))}", 2, [], False, None
        return ['RLCA', 'RRCA', 'RLA', 'RRA', 'DAA', 'CPL', 'SCF', 'CCF'][y], 1, [], False, None
    # x == 3
    if z == 0:
        return f"RET {CC[y]}", 1, [], False, None
    if z == 1:
        if y & 1:
            return ['RET', 'EXX', 'JP (HL)', 'LD SP,HL'][y >> 1], 1, [], y in (1, 5), None
        return f"POP {RP2[y >> 1]}", 1, [], False, None
    if z == 2:
        return f"JP {CC[y]},{{j}}", 3, [w(1)], False, ('j', w(1))
    if z == 3:
        if y == 0:
            return "JP {j}", 3, [w(1)], True, ('j', w(1))
        if y == 2:
            return f"OUT ({hx(b(1))}),A", 2, [], False, None
        if y == 3:
            return f"IN A,({hx(b(1))})", 2, [], False, None
        if y == 4:
            return "EX (SP),HL", 1, [], False, None
        if y == 5:
            return "EX DE,HL", 1, [], False, None
        if y == 6:
            return "DI", 1, [], False, None
        if y == 7:
            return "EI", 1, [], False, None
        raise ValueError("CB")
    if z == 4:
        return f"CALL {CC[y]},{{j}}", 3, [w(1)], False, ('c', w(1))
    if z == 5:
        if y & 1:
            if y == 1:
                return "CALL {j}", 3, [w(1)], False, ('c', w(1))
            raise ValueError("prefix")
        return f"PUSH {RP2[y >> 1]}", 1, [], False, None
    if z == 6:
        return f"{ALU[y]}{hx(b(1))}", 2, [], False, None
    return f"RST {hx(y * 8)}", 1, [], False, None


def lies_karte(pfad):
    k = {'seg': [], 'code': [], 'db': [], 'dw': [], 'dwc': [], 'txt': [], 'str': [], 'name': {}, 'kom': {}, 'eq': {}}
    for z in open(pfad, encoding='utf-8'):
        z = z.rstrip('\n')
        if not z.strip() or z.lstrip().startswith('#'):
            continue
        t = z.split(None, 2)
        zahl = lambda s: int(s, 16)
        if t[0] == 'seg':
            _, a, o, l = z.split()
            k['seg'].append((zahl(a), zahl(o), zahl(l)))
        elif t[0] == 'code':
            k['code'] += [zahl(x) for x in z.split()[1:]]
        elif t[0] in ('db', 'dw', 'dwc', 'txt', 'str'):
            _, v, bis = z.split()[:3]
            k[t[0]].append((zahl(v), zahl(bis)))
        elif t[0] == 'name':
            k['name'][zahl(t[1])] = t[2].split()[0]
        elif t[0] == 'kom':
            k['kom'][zahl(t[1])] = t[2]
        elif t[0] == 'eq':
            k['eq'][t[1]] = zahl(t[2].split()[0])
    k['dw'] += k['dwc']
    return k


def disassembliere(datei, kartenpfad, nur_roh=True):
    daten = open(datei, 'rb').read()
    k = lies_karte(kartenpfad)
    m = Speicher()
    seg_ende = {}
    for a, o, l in k['seg']:
        m.laden(a, daten[o:o + l])
        seg_ende[a] = a + l

    def in_ro(lst, a):
        return any(v <= a <= bis for v, bis in lst)

    kind = {}          # adresse -> ('i', laenge) | ('db'|'dw'|'txt')
    ziele_marke = set()
    # Daten laut Karte zuerst festnageln
    gesperrt = set()
    for art in ('db', 'dw', 'txt', 'str'):
        for v, bis in k[art]:
            for a in range(v, bis + 1):
                gesperrt.add(a)
    work = list(k['code'])
    for v, bis in k['dwc']:
        for a in range(v, bis, 2):
            t = m[a] | (m[a + 1] << 8)
            if t in m and t not in gesperrt:
                work.append(t)
    seen = set()
    while work:
        a = work.pop()
        while a in m and a not in seen and a not in gesperrt:
            try:
                vorlage, n, ziele, ende, absz = entschluesseln(m, a)
            except (ValueError, IndexError):
                break
            if any((a + i) in gesperrt or (a + i) not in m for i in range(n)):
                break
            if all(m.b.get(a + i) == 0xFF for i in range(min(n + 3, 4))) and n == 1:
                break
            kind[a] = ('i', n, vorlage, absz)
            for i in range(n):
                seen.add(a + i)
            for t in ziele:
                ziele_marke.add(t)
                if t in m:
                    work.append(t)
            # Zeiger auf Code: LD rr,nn / LD (nn),..  mit Ziel im Segment, das
            # (noch) nicht als Daten gesperrt ist, wird als Einstieg probiert.
            if isinstance(absz, int) and vorlage.startswith('LD ') and \
                    vorlage.endswith(',{a}') and absz in m and absz not in gesperrt:
                work.append(absz)
            if ende:
                # Der Befehl hinter einem unbedingten Ende ist meist der naechste
                # Einstieg (Treiberroutinen liegen dicht an dicht).
                nxt = a + n
                if nxt in m and nxt not in gesperrt and m[nxt] != 0xFF:
                    work.append(nxt)
                break
            a += n
    return m, k, kind, ziele_marke, seen, gesperrt, seg_ende, daten


def ausgabe(m, k, kind, ziele_marke, seen, gesperrt, seg_ende, name_prefix='L'):
    marke = dict(k['name'])
    # Adressen, auf die irgendein Befehl/Wort zeigt, werden Marken
    refs = {t for t in ziele_marke if t in m}
    for a, e in kind.items():
        absz = e[3]
        if isinstance(absz, int) and absz in m:
            refs.add(absz)
    for v, bis in k['dw']:
        for a in range(v, bis + 1, 2):
            if a + 1 in m:
                t = m[a] | (m[a + 1] << 8)
                if t in m:
                    refs.add(t)
    for a in refs:
        marke.setdefault(a, f"{name_prefix}{a:04X}")

    def sym(a):
        if a in marke:
            return marke[a]
        for n, v in k['eq'].items():
            if v == a:
                return n
        return None

    def ausdruck(a):
        s = sym(a)
        return s if s else hx(a, 4)

    strstart = set()
    for v, bis in k['str']:
        a = v
        while a <= bis:
            strstart.add(a)
            a += 1 + m[a]
    out = ["\t.Z80", "\tCSEG", "",
           "; N Fuellbytes FFH (UDOS-Segmente haben unbenutzte Luecken)",
           "FILLFF\tMACRO\tN", "\tREPT\tN", "\tDB\t0FFH", "\tENDM", "\tENDM", ""]
    for n, v in sorted(k['eq'].items(), key=lambda x: x[1]):
        out.append(f"{n}\tEQU\t{hx(v, 4)}")
    segs = sorted(seg_ende.items())
    for sa, se in segs:
        out.append(f"\n; ---- Segment {sa:04X}H .. {se - 1:04X}H ----")
        out.append(f"\t.PHASE\t{hx(sa, 4)}")
        out.append(f"SEG{sa:04X}:")
        a = sa
        while a < se:
            if a in k['kom']:
                out.append(f"; {k['kom'][a]}")
            if a in marke:
                out.append(f"{marke[a]}:")
            e = kind.get(a)
            if e and e[0] == 'i':
                _, n, vorlage, absz = e
                if absz is None:
                    text = vorlage
                elif isinstance(absz, tuple):
                    text = vorlage.format(j=ausdruck(absz[1]))
                else:
                    text = vorlage.format(a=ausdruck(absz))
                out.append(f"\t{text}" + (f"\t; {a:04X}" if ADR else ""))
                a += n
                continue
            # Daten
            art = 'db'
            for nm in ('dw', 'txt', 'str'):
                for v, bis in k[nm]:
                    if v <= a <= bis:
                        art = 'txt' if nm == 'str' and a not in strstart else nm
            # Ende des Datenlaufs: naechste Marke, naechster Befehl, Segmentende, Kartenwechsel
            ende = a + 1
            if art == 'dw':
                ende = a + 2
            elif art == 'str':
                ende = a + 1
            else:
                while ende < se and ende not in marke and ende not in kind and \
                        ende not in k['kom'] and not _wechsel(k, ende - 1, ende):
                    ende += 1
                    if ende - a >= 16 and art == 'db':
                        break
            if art == 'dw' and a + 1 < se:
                t = m[a] | (m[a + 1] << 8)
                out.append(f"\tDW\t{ausdruck(t) if t in m or sym(t) else hx(t, 4)}")
                a += 2
                continue
            bs = [m[x] for x in range(a, min(ende, se))]
            if art == 'db' and bs[0] == 0xFF:
                n = 0
                while a + n < se and a + n not in marke and a + n not in kind and \
                        a + n not in k['kom'] and m[a + n] == 0xFF and not _wechsel(k, a, a + n):
                    n += 1
                if n >= 8:
                    out.append(f"\tFILLFF\t{n}")
                    a += n
                    continue
            if art == 'str':
                out.append(f"\tDB\t{hx(bs[0])}")
            elif art == 'txt':
                out.extend(_text(bs))
            else:
                out.append("\tDB\t" + ",".join(hx(v) for v in bs))
            a += len(bs)
        # Segment endet auf der Sektorgrenze der UDOS-Datei (256 B), Fuellbyte FFH
        out.append(f"SEG{sa:04X}E:")
        out.append(f"\tREPT\t(100H-(($-SEG{sa:04X}) AND 0FFH)) AND 0FFH")
        out.append("\tDB\t0FFH")
        out.append("\tENDM")
        out.append("\t.DEPHASE")
    out.append("\tEND")
    return "\n".join(out) + "\n"


def _wechsel(k, a, b):
    def art(x):
        for nm in ('dw', 'txt', 'str', 'db'):
            for v, bis in k[nm]:
                if v <= x <= bis:
                    return nm
        return None
    return art(a) != art(b)


def _text(bs):
    """Zeichenkette als DB mit Anfuehrungszeichen; Steuerzeichen als Zahlen."""
    teile, cur = [], ''
    for v in bs:
        if 0x20 <= v < 0x7F and v not in (0x27, 0x22):
            cur += chr(v)
        else:
            if cur:
                teile.append(f"'{cur}'")
                cur = ''
            teile.append(hx(v))
    if cur:
        teile.append(f"'{cur}'")
    return ["\tDB\t" + ",".join(teile)]


def main():
    if len(sys.argv) != 3:
        sys.exit("Aufruf: dis.py <mutos_io-datei> <karte.txt>")
    erg = disassembliere(sys.argv[1], sys.argv[2])
    sys.stdout.write(ausgabe(*erg[:7]))


if __name__ == '__main__':
    main()

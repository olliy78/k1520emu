"""Gesamtbau des MUTOS-Kernels aus C und Assembler (AP K8, Entwurf 41).

Idee (Mischbau je FUNKTION): Der Kernel ist die Folge seiner Funktionen in Adressreihenfolge.  Jede Funktion kommt entweder
aus dem Gast-`cc` (c/*.c -> *.o) oder aus der Assemblerquelle, die `z8kdis --modul` fuer genau diesen Adressbereich schreibt
(wie linkbau.py, K5).  Die Objekte des `cc` werden auf dem Wirt in Stuecke je Funktion geschnitten und "kernelfertig" gemacht:

  * Das Objekt des `cc` fuehrt Namen (`_proc`, `_bread`) und eigene Daten (Zeichenketten `L7`), der Kernel kennt nur Adressen.
    Jede Fremdbezugsstelle (Relokation) wird deshalb auf den Namen abgebildet, den z8kdis fuer diese ADRESSE schreibt
    (Kartenname oder `Lxxxx`); Adressen ab D256 (Postfach, `u` bei FC00) werden zur Konstante.
  * Die Adressen der C-Namen sind nicht dokumentiert, sondern aus den bytegleichen Funktionen GEMESSEN: das Kernelwort an einer
    Relokationsstelle minus dem Zusatzwert im Objekt ist die Adresse des Namens.  Widerspruch zwischen zwei Messungen bricht ab.
  * Die Daten (Zeichenketten, Tabellen) bleiben im Assemblermodul `daten`; die Datenabschnitte der C-Objekte werden NICHT
    mitgebunden.  Zeigt eine Stelle auf Daten des Objekts, wird die Adresse im Kernel gesucht (gemessen oder per Inhalt).

Stufe 1: nur Funktionen, die der Kernel-Compiler bytegleich erzeugt (--stufe 1) -> der gebundene Kernel muss BYTEGLEICH sein.
Stufe 2: alle Funktionen mit C-Quelle (--stufe 2) -> funktionsgleich, nicht bytegleich; Boot-Test.

Aufruf:   python3 -I tools/mutos8000/kernel/gesamtbau.py --arbeit DIR [--stufe 1|2] [--ausser f1,f2] [--nur-plan]
                                                              [--uebersetzen] [--objekte DIR] [--boot]
Exit:     0 = Ergebnis erwartungsgemaess, 1 = Fehler, 77 = Hilfsmittel fehlen.   Laufzeit: Minuten (Hintergrund).
"""
import argparse
import os
import re
import struct
import subprocess
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HIER)
sys.path.insert(0, os.path.join(HIER, "c"))
import objformat as O          # noqa: E402
import vergleich               # noqa: E402

WURZEL = os.path.abspath(os.path.join(HIER, "..", "..", ".."))
KARTE = os.path.join(HIER, "karte.txt")
LINKKARTE = os.path.join(HIER, "linkkarte.txt")
FUNKTIONEN = os.path.join(HIER, "lauf", "funktionen.txt")
CQUELLEN = os.path.join(HIER, "c")
TEXT_ENDE = 0x875A          # ab hier Daten
BSS_VON, BSS_BIS = 0x8DC0, 0xD256
KONST_AB = 0xD256           # Postfach, u bei FC00: feste Adressen (mutos_io kennt sie)

# C-Dateien, die Kernelfunktionen enthalten (Reihenfolge egal; Versuchsdateien var*, k7b*, djnz*, pilot, varianten fehlen mit Absicht).
# Name -> Objektdatei im Uebersetzungsordner (dev0 ist im Kernel OHNE -O uebersetzt)
CDATEIEN = ["alloc", "bio", "clock", "dev0", "fio", "iget", "lp", "machdep", "main", "malloc", "nami", "pipe", "prf", "rdwri",
            "rk", "sig", "slp", "subr", "sys1", "sys2", "sys3", "sys4", "text", "trap", "tty", "tty2", "ureg"]
OHNE_O = ("dev0",)          # nur diese ohne -O
# Zusatz-C-Dateien, die nur Daten liefern (kein Funktionstext)
CDATEN = ["conf", "sysent"]


def objname(c):
    return c if c in OHNE_O else c + "O"


class Fehler(Exception):
    pass


# ---------------------------------------------------------------- Kartenwissen
def karte_namen():
    n = {}
    for z in open(KARTE):
        p = z.split()
        if len(p) >= 3 and p[0] == "name":
            n[int(p[1], 16)] = p[2]
    return n


def funktionsanfaenge():
    s = {}
    for z in open(FUNKTIONEN):
        if z.startswith("#") or not z.strip():
            continue
        p = z.split()
        if len(p) >= 2 and re.fullmatch(r"[0-9A-F]{4}", p[0]) and p[1].isdigit():
            s[int(p[0], 16)] = int(p[1])
    return s


class Welt:
    """Alles, was Kernel, Karte und C-Objekte gemeinsam wissen."""

    def __init__(self, kernel, objdir):
        self.k = kernel
        self.kn = karte_namen()
        self.objdir = objdir
        self.funcs = []                 # CFunc in Adressreihenfolge
        self.symadr = {}                # C-Name -> Adresse (gemessen oder aus Marken)
        self.symquelle = {}             # C-Name -> Beleg (fuer Meldungen)
        self.dmap = {}                  # (cdatei, Objektwort) -> Kernel-Adresse einer Datenstelle
        self.warn = []
        self.alias = {}
        self.mitgebunden = []           # (cdatei, von, bis, Funktion, Bytes) der mitgebundenen Datenstuecke
        self._laden()

    # ---- Marken und Objekte
    def _laden(self):
        starts = funktionsanfaenge()
        self.objekte = {}
        cf = []
        for c in CDATEN:
            self.objekte[c] = O.Objekt.lesen(open(os.path.join(self.objdir, objname(c) + ".o"), "rb").read())
        for c in CDATEIEN:
            pfad = os.path.join(self.objdir, objname(c) + ".o")
            if not os.path.exists(pfad):
                raise Fehler("Objekt fehlt: %s (erst --uebersetzen)" % pfad)
            o = O.Objekt.lesen(open(pfad, "rb").read())
            self.objekte[c] = o
            text, maske = o.maskierter_text()
            tsym = sorted((s.wert, s.name) for s in o.syms if s.basistyp == O.N_TEXT and s.ext)
            wert = {n: w for w, n in tsym}
            ende = sorted(set(w for w, _ in tsym) | {len(o.text)})
            for adr, name in vergleich.funktionen_aus_quelle(os.path.join(CQUELLEN, c + ".c")):
                sym = ("_" + name).encode()[:8]
                if sym not in wert:
                    self.warn.append("%s.c: %s (%04X) fehlt im Objekt" % (c, name, adr))
                    continue
                a = wert[sym]
                e = min(x for x in ende if x > a)
                cf.append(CFunc(c, name, adr, a, e))
        for f in cf:
            starts.setdefault(f.addr, 0)
        self.starts = sorted(starts)
        ends = {a: (self.starts[i + 1] if i + 1 < len(self.starts) else TEXT_ENDE) for i, a in enumerate(self.starts)}
        seen = {}
        for f in sorted(cf, key=lambda x: x.addr):
            if f.addr in seen:
                self.warn.append("Adresse %04X doppelt: %s.c %s und %s.c %s" % (f.addr, seen[f.addr].cdatei, seen[f.addr].name,
                                                                                f.cdatei, f.name))
                continue
            seen[f.addr] = f
            f.orig_ende = ends[f.addr]
            o = self.objekte[f.cdatei]
            f.text = o.text[f.lo:f.hi]
            f.relocs = [(off - f.lo, r) for off, r in o.relocs("text") if f.lo <= off < f.hi]
            self.funcs.append(f)
            self.symadr[("_" + f.name)[:8]] = f.addr
            self.symquelle[("_" + f.name)[:8]] = "Marke"
        self.byaddr = seen
        for f in self.funcs:
            f.status = self._status(f)
        # asm-Namen aus der Karte (nur wo ein C-Name gleich heisst)
        for a, n in self.kn.items():
            self.symadr.setdefault(n[:8], a)
            self.symquelle.setdefault(n[:8], "Karte")
        self._messen()
        for z in open(os.path.join(HIER, "adressen.txt")):
            q = z.split()
            if len(q) == 2 and not z.startswith("#"):
                self.symadr.setdefault(q[0], int(q[1], 16))
                self.symquelle.setdefault(q[0], "adressen.txt")

    def _status(self, f):
        """GLEICH = alle Woerter ausser den Relokationsstellen wie im Kernel (Laenge gleich)."""
        n = f.orig_ende - f.addr
        if len(f.text) != n:
            return "LAENGE"
        wild = {off for off, _ in f.relocs}
        for i in range(0, len(f.text), 2):
            if i in wild:
                continue
            if f.text[i:i + 2] != self.k[f.addr + i:f.addr + i + 2]:
                return "ABW"
        return "GLEICH"

    # ---- Adressen der Namen aus bytegleichen Funktionen
    def _messen(self):
        """Adresse eines Namens = Kernelwort minus Zusatzwert an jeder Relokationsstelle einer (bis auf Relokation) gleichen
        Funktion.  Abgestimmt wird nach Mehrheit; eine Funktion, deren Stelle der Mehrheit widerspricht (z. B. vertauschte
        Argumentreihenfolge: der Maskenvergleich sieht das nicht), ist NICHT gleich (Status RELOC)."""
        from collections import Counter, defaultdict
        stimmen = defaultdict(Counter)
        dstimmen = defaultdict(Counter)
        for f in self.funcs:
            if f.status != "GLEICH":
                continue
            for off, r in f.relocs:
                w = struct.unpack(">H", f.text[off:off + 2])[0]
                kw = struct.unpack(">H", self.k[f.addr + off:f.addr + off + 2])[0]
                if r.art == O.R_EXT:
                    name = self.objekte[f.cdatei].syms[r.sym].name.decode("latin-1")
                    stimmen[name][(kw - w) & 0xFFFF] += 1
                elif r.art == O.R_DATA:
                    dstimmen[(f.cdatei, w)][kw] += 1
        for name, c in stimmen.items():
            if name in self.symadr and self.symquelle[name] in ("Marke", "Karte"):
                a = self.symadr[name]
            else:
                a = c.most_common(1)[0][0]
                self.symadr[name] = a
                self.symquelle[name] = "gemessen"
            if len(c) > 1:
                self.warn.append("Mehrheit %s: %s" % (name, ", ".join("%04X x%d" % x for x in c.most_common())))
        for k, c in dstimmen.items():
            self.dmap[k] = c.most_common(1)[0][0]
        for f in self.funcs:
            if f.status != "GLEICH":
                continue
            for off, r in f.relocs:
                w = struct.unpack(">H", f.text[off:off + 2])[0]
                kw = struct.unpack(">H", self.k[f.addr + off:f.addr + off + 2])[0]
                if r.art == O.R_EXT:
                    name = self.objekte[f.cdatei].syms[r.sym].name.decode("latin-1")
                    ok = self.symadr[name] == (kw - w) & 0xFFFF
                elif r.art == O.R_DATA:
                    ok = self.dmap[(f.cdatei, w)] == kw
                else:
                    continue
                if not ok:
                    f.status = "RELOC"
                    break

    def datenstueck(self, c, w):
        """(Anfang, Ende, hat_Relokation) des Datenstuecks im Objekt c, das das Objektwort w enthaelt (Stuecke = Datensymbole)."""
        o = self.objekte[c]
        tl = len(o.text)
        starts = sorted(set(s.wert for s in o.syms if s.basistyp == O.N_DATA and s.wert >= tl) | {tl, tl + len(o.data)})
        a = max(x for x in starts if x <= w)
        e = min(x for x in starts if x > a)
        rd = dict(o.relocs("data"))
        return a, e, any((x - tl) in rd for x in range(a, e, 2))

    def inhaltstreffer(self, c, w):
        """Adresse im Kernel, die dasselbe Datenstueck (ohne Zeiger) schon enthaelt; sonst None.  Zeichenketten: bei Mehrdeutigkeit
        der erste Treffer (unveraenderlich), alles andere nur bei genau einem Treffer."""
        a, e, rel = self.datenstueck(c, w)
        if rel:
            return None
        o = self.objekte[c]
        tl = len(o.text)
        b = o.data[a - tl:e - tl]
        if not b or not any(b):
            return None
        tr = []
        i = self.k.find(b, TEXT_ENDE, BSS_VON)
        while i >= 0:
            tr.append(i)
            i = self.k.find(b, i + 1, BSS_VON)
        if not tr or (len(tr) > 1 and not istext(b, 0)):
            return None
        return tr[0] + (w - a)

    # ---- Namen im Assembler
    def adrname(self, a):
        n = self.kn.get(a)
        if n and len(n) <= 8:
            return n
        return "L%04X" % a


class CFunc:
    def __init__(self, cdatei, name, addr, lo, hi):
        self.cdatei, self.name, self.addr, self.lo, self.hi = cdatei, name, addr, lo, hi
        self.status = None
        self.orig_ende = None
        self.text = b""
        self.relocs = []
        self.nahe = None


def istext(daten, d):
    """Zeichenkette (druckbare Zeichen bis NUL) ab Datenoffset d?"""
    e = daten.find(b"\0", d)
    if e <= d:
        return False
    return all(32 <= c < 127 or c in (9, 10, 13) for c in daten[d:e])


def datenstelle_finden(w, f, objektdaten, textlaenge, kernel):
    """Adresse einer Datenstelle des C-Objekts im Kernel per Inhalt (Zeichenkette ab Datenoffset)."""
    d = w - textlaenge
    if not 0 <= d < len(objektdaten):
        return None
    e = objektdaten.find(b"\0", d)
    if e < 0:
        return None
    s = objektdaten[d:e + 1]
    lo, hi = TEXT_ENDE, BSS_VON
    treffer = []
    i = kernel.find(s, lo, hi)
    while i >= 0:
        treffer.append(i)
        i = kernel.find(s, i + 1, hi)
    return treffer


# ---------------------------------------------------------------- Umwandlung eines C-Stuecks
def kernelfertig(welt, f, cache_dateien=None):
    """Gibt (text, relokationsworte, externe Namen) einer C-Funktion zurueck, bereit zum Binden im Kernel.

    Relokationsworte: Liste (offset, art, symbolname|None) mit art 1 = Text (Zusatzwert = Offset im Stueck), 4 = extern."""
    o = welt.objekte[f.cdatei]
    t = bytearray(f.text)
    out = []
    for off, r in f.relocs:
        w = struct.unpack(">H", t[off:off + 2])[0]
        if r.art == O.R_TEXT:
            # Sprung/Zeiger innerhalb der Objektdatei: im Stueck -> relativ zum Stueck, sonst auf eine andere Funktion
            if f.lo <= w < f.hi:
                out.append((off, "t", None, w - f.lo))
                t[off:off + 2] = struct.pack(">H", w - f.lo)
                continue
            ziel = [g for g in welt.funcs if g.cdatei == f.cdatei and g.lo <= w < g.hi]
            if not ziel:
                raise Fehler("%s.c %s: Textverweis %04X ohne Funktion" % (f.cdatei, f.name, w))
            g = ziel[0]
            out.append((off, "x", welt.adrname(g.addr), w - g.lo))
            t[off:off + 2] = struct.pack(">H", w - g.lo)
        elif r.art == O.R_EXT:
            name = o.syms[r.sym].name.decode("latin-1")
            if name not in welt.symadr:
                raise Fehler("%s.c %s: Adresse von %s unbekannt" % (f.cdatei, f.name, name))
            a = welt.symadr[name]
            if a >= KONST_AB:
                t[off:off + 2] = struct.pack(">H", (a + w) & 0xFFFF)
                continue
            out.append((off, "x", welt.adrname(a), w))
        elif r.art == O.R_DATA:
            kw = welt.dmap.get((f.cdatei, w))
            if kw is None:
                a0, _, _ = welt.datenstueck(f.cdatei, w)
                for s in o.syms:
                    if s.ext and s.basistyp == O.N_DATA and s.wert == a0 and s.name.decode("latin-1") in welt.symadr:
                        kw = welt.symadr[s.name.decode("latin-1")] + (w - a0)
            if kw is None:
                kw = welt.inhaltstreffer(f.cdatei, w)
                if kw is not None:
                    welt.dmap[(f.cdatei, w)] = kw
            if kw is None:
                # Tabelle oder Zeichenkette ohne Gegenstueck im Kernel (Sprungtabelle des neuen Codes): wird mitgebunden
                out.append((off, "d", None, w))
                continue
            if kw >= KONST_AB:
                t[off:off + 2] = struct.pack(">H", kw)
                continue
            out.append((off, "x", welt.adrname(kw), 0))
            t[off:off + 2] = b"\0\0"
        else:
            raise Fehler("%s.c %s: Relokationsart %d" % (f.cdatei, f.name, r.art))
    return bytes(t), out


# ---------------------------------------------------------------- Plan, Zusammenbau
def plan(welt, erlaubt, ausser=(), nur=None, adressen_aus=()):
    """Segmente in Adressreihenfolge: ('a', von, bis) Assembler, ('c', von, bis, [Funktionen]) C."""
    nutz = {}
    for f in welt.funcs:
        if f.status not in erlaubt or f.name in ausser or f.addr in adressen_aus:
            continue
        if nur is not None and f.cdatei not in nur:
            continue
        try:
            kernelfertig(welt, f)
        except Fehler as e:
            welt.warn.append("nicht verwendbar: %s" % e)
            continue
        nutz[f.addr] = f
    runs = []
    for a in sorted(nutz):
        f = nutz[a]
        if runs and runs[-1][1] == a:
            runs[-1] = (runs[-1][0], f.orig_ende, runs[-1][2] + [f])
        else:
            runs.append((a, f.orig_ende, [f]))
    seg = []
    pos = 0
    for von, bis, fs in runs:
        if von > pos:
            seg.append(("a", pos, von))
        seg.append(("c", von, bis, fs))
        pos = bis
    if pos < TEXT_ENDE:
        seg.append(("a", pos, TEXT_ENDE))
    return seg


def benenne(segs):
    """Modulnamen: sg00.. (Assembler) und cg00.. (C)."""
    r = []
    for i, s in enumerate(segs):
        r.append(("%s%s%02d" % ("s" if s[0] == "a" else "c", "g", i),) + s)
    return r


def komposit(welt, funcs):
    """Ein Objekt (a.out) aus den kernelfertigen Funktionsstuecken.  Text + Relokation; Daten nur fuer mitgebundene
    Tabellen (Sprungtabellen des neuen Codes)."""
    text = bytearray()
    syms = []
    idx = {}
    piece = {}                      # Funktion -> Offset im Text

    def sym(name, typ, wert):
        if name in idx:
            s = syms[idx[name]]
            if typ != 0x20:
                s.typ, s.wert = typ, wert
            return idx[name]
        idx[name] = len(syms)
        syms.append(O.Symbol(name.encode()[:8], typ, 0, wert))
        return idx[name]
    rel = {}
    daten_fix = []                  # (Textoffset, cdatei, Objektwort)
    fertig = [kernelfertig(welt, f) for f in funcs]
    eigen = {}                      # Name -> Offset der im Objekt selbst definierten Funktionen
    p = 0
    for f, (t, out) in zip(funcs, fertig):
        eigen[welt.adrname(f.addr)] = p
        p += len(t)
    for f, (t, out) in zip(funcs, fertig):
        p = len(text)
        piece[f] = p
        sym(welt.adrname(f.addr), 0x22, p)
        for off, kind, name, add in out:
            if kind == "x" and name in eigen:
                # Bezug auf eine im selben Objekt definierte Funktion: Textrelokation (der ld kennt nur Fremdbezuege als extern)
                w = struct.unpack(">H", t[off:off + 2])[0] + eigen[name]
                t = t[:off] + struct.pack(">H", w) + t[off + 2:]
                rel[p + off] = (1 << 1)
            elif kind == "t":
                w = struct.unpack(">H", t[off:off + 2])[0] + p
                t = t[:off] + struct.pack(">H", w) + t[off + 2:]
                rel[p + off] = (1 << 1)
            elif kind == "d":
                daten_fix.append((p + off, f.cdatei, add, f))
            else:
                rel[p + off] = (sym(name, 0x20, 0) << 4) | (O.R_EXT << 1)
        text += t
    # mitgebundene Datenstuecke
    data = bytearray()
    drel = {}
    ditem = {}                      # (cdatei, Anfang im Objekt) -> Offset in data
    carried = []                    # (cdatei, von, bis, Funktion): was mitgebunden wurde (Aufrufer verwerfen das Gegenstueck im Kernel)
    owner = [None]

    def stueck(c, w):
        o = welt.objekte[c]
        tl = len(o.text)
        starts = sorted(set(s.wert for s in o.syms if s.basistyp == O.N_DATA and s.wert >= tl) | {tl, tl + len(o.data)})
        a = max(x for x in starts if x <= w)
        e = min(x for x in starts if x > a)
        if (c, a) not in ditem:
            carried.append((c, a, e, owner[0]))
            ditem[(c, a)] = len(data)
            base = len(data)
            data.extend(o.data[a - tl:e - tl])
            rd = dict(o.relocs("data"))
            for off in range(a - tl, e - tl, 2):
                r = rd.get(off)
                if r is None:
                    continue
                v = struct.unpack(">H", o.data[off:off + 2])[0]
                pos = base + off - (a - tl)
                if r.art == O.R_TEXT:
                    ziel = [g for g in welt.funcs if g.cdatei == c and g.lo <= v < g.hi]
                    if not ziel:
                        raise Fehler("%s.c: Datenzeiger auf Text %04X ohne Funktion" % (c, v))
                    g = ziel[0]
                    if welt.adrname(g.addr) in eigen:
                        data[pos:pos + 2] = struct.pack(">H", eigen[welt.adrname(g.addr)] + v - g.lo)
                        drel[pos] = (1 << 1)
                    else:
                        data[pos:pos + 2] = struct.pack(">H", v - g.lo)
                        drel[pos] = (sym(welt.adrname(g.addr), 0x20, 0) << 4) | (O.R_EXT << 1)
                elif r.art == O.R_EXT:
                    nm = o.syms[r.sym].name.decode("latin-1")
                    if nm not in welt.symadr:
                        raise Fehler("%s.c: Adresse von %s (in Daten) unbekannt" % (c, nm))
                    ad = welt.symadr[nm]
                    if ad >= KONST_AB:
                        data[pos:pos + 2] = struct.pack(">H", (ad + v) & 0xFFFF)
                    elif welt.adrname(ad) in eigen:
                        data[pos:pos + 2] = struct.pack(">H", eigen[welt.adrname(ad)] + v)
                        drel[pos] = (1 << 1)
                    else:
                        drel[pos] = (sym(welt.adrname(ad), 0x20, 0) << 4) | (O.R_EXT << 1)
                else:
                    raise Fehler("%s.c: Relokationsart %d in mitgebundenen Daten" % (c, r.art))
            if len(data) & 1:
                data.append(0)
        return ditem[(c, a)] + (w - a)
    drel_text = {}
    for toff, c, w, ofunc in daten_fix:
        owner[0] = ofunc
        d = stueck(c, w)
        v = len(text) + d
        text[toff:toff + 2] = struct.pack(">H", v)
        rel[toff] = (O.R_DATA << 1)
    o = O.Objekt()
    o.text = bytes(text)
    o.data = bytes(data)
    o.rtext = [rel.get(i, 0) for i in range(0, len(text), 2)]
    o.rdata = [drel.get(i, 0) for i in range(0, len(data), 2)]
    # Textrelokationen in den Daten (art 1) liegen als Textoffsets; Datenrelokation art 2 zeigt in den Datenabschnitt
    o.syms = syms
    welt.mitgebunden.extend((c, a, e, g, bytes(welt.objekte[c].data[a - len(welt.objekte[c].text):e - len(welt.objekte[c].text)]))
                            for c, a, e, g in carried)
    return o.schreiben()


def waisen(welt):
    """Datenbereiche des Kernels, die durch mitgebundene Tabellen (Sprungtabellen des neuen Codes) ersetzt sind: [(von, bis)].

    Gesucht wird in 8766..8DC0 ein Lauf aus Woertern, der an den Zeigerstellen der Tabelle Adressen im Bereich der ALTEN Funktion
    traegt und sonst gleiche Bytes hat."""
    r = []
    belegt = set()
    for c, a, e, g, b in welt.mitgebunden:
        o = welt.objekte[c]
        tl = len(o.text)
        rd = dict(o.relocs("data"))
        zeiger = [x - (a - tl) for x in range(a - tl, e - tl, 2) if x in rd and rd[x].art == O.R_TEXT]
        if not zeiger:
            continue
        n = e - a
        gef = None
        for pos in range(0x8766, BSS_VON - n + 1, 2):
            if any(x in belegt for x in range(pos, pos + n, 2)):
                continue
            ok = True
            for i in range(0, n, 2):
                v = struct.unpack(">H", welt.k[pos + i:pos + i + 2])[0]
                if i in zeiger:
                    if not (g.addr <= v < g.orig_ende):
                        ok = False
                        break
                elif welt.k[pos + i:pos + i + 2] != b[i:i + 2]:
                    ok = False
                    break
            if ok:
                gef = pos
                break
        if gef is None:
            welt.warn.append("Waise zu %s.c %04X (%s) nicht gefunden" % (c, a, g.name))
            continue
        belegt.update(range(gef, gef + n, 2))
        r.append((gef, gef + n))
    return sorted(r)


def datenstuecke(waise):
    """Datenbereich 8766..8DC0 ohne die Waisen als Liste (von, bis)."""
    r = []
    pos = 0x8766
    for v, b in waise:
        if v > pos:
            r.append((pos, v))
        pos = b
    if pos < BSS_VON:
        r.append((pos, BSS_VON))
    return r


def linkkarte_neu(segs, dstuecke):
    """Linkkarte fuer z8kdis: die Zeilen der Original-Linkkarte ausser `modul`, dann die Module des Plans."""
    z = [l.rstrip("\n") for l in open(LINKKARTE) if not l.startswith("modul ") and not l.startswith("bss ")]
    z += [l.rstrip("\n") for l in open(LINKKARTE) if l.startswith("bss ")]
    m = []
    for i, (name, kind, von, bis, *rest) in enumerate(segs):
        dat = "875A 8766" if i == 0 else "- -"
        m.append("modul %s %04X %04X %s" % (name, von, bis, dat))
    for i, (v, b) in enumerate(dstuecke):
        m.append("modul da%02d 875A 875A %04X %04X" % (i, v, b))
    return "\n".join(z + m) + "\n"


def tote_marken(text, segs, definiert):
    """In Datenmodulen zeigen Sprungtabellen des ALTEN Codes auf Marken, die es im neuen nicht mehr gibt: null setzen."""
    cb = [(s[2], s[3]) for s in segs if s[1] == "c"]

    def ers(m):
        a = int(m.group(1), 16)
        if m.group(0) in definiert or not any(v <= a < b for v, b in cb):
            return m.group(0)
        return "%0000"
    return re.sub(r"(?<![A-Za-z0-9_])L([0-9A-F]{4})(?![A-Za-z0-9_:])", ers, text)


# ---------------------------------------------------------------- Uebersetzen der C-Quellen im Gast
def uebersetzen(arbeit):
    """C-Quellen im MUTOS-Gast mit `cc -O -c` uebersetzen (c/gast_lauf.sh, sechs Laeufe gleichzeitig); Objekte nach ARBEIT/obj."""
    stapel = [["alloc", "bio", "clock", "conf", "fio", "iget"],
              ["machdep", "main", "malloc", "nami", "pipe", "prf", "rdwri", "rk"],
              ["sig", "slp", "subr", "sys1", "ureg"],
              ["sys2", "sys3", "sys4", "text", "trap", "tty", "sysent"],
              ["tty2", "lp"],
              ["dev0"]]
    os.makedirs(os.path.join(arbeit, "obj"), exist_ok=True)
    ps = []
    for i, s in enumerate(stapel, 1):
        env = dict(os.environ, KEIN_S="1")
        if s != ["dev0"]:
            env["NUR_O"] = "1"
        ps.append((i, subprocess.Popen(["bash", os.path.join(CQUELLEN, "gast_lauf.sh"), os.path.join(arbeit, "g%d" % i)] +
                                       [x + ".c" for x in s], env=env, stdout=open(os.path.join(arbeit, "g%d.log" % i), "w"),
                                       stderr=subprocess.STDOUT)))
        import time
        time.sleep(15)          # dev.sh baut zuerst: nicht gleichzeitig anstossen
    for i, p in ps:
        p.wait()
    import glob
    import shutil
    for g in glob.glob(os.path.join(arbeit, "g*", "out", "*.o")):
        shutil.copy(g, os.path.join(arbeit, "obj"))


def ladediskette(arbeit, kernel_bytes):
    """Kopie der K5601-Ladediskette mit dem neuen Kernel als `mutos_1216` (Dateiangaben des Originals bleiben)."""
    import shutil
    import gast_as
    k = gast_as.DISKTOOL
    os.makedirs(arbeit, exist_ok=True)
    ziel = os.path.join(arbeit, "lade.hfe")
    shutil.copyfile(os.path.join(gast_as.DISKS, "a5120_mutos8000_boot_udos_k5601.hfe"), ziel)
    d = os.path.join(arbeit, "kern")
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    for args in (["get", ziel, "mutos_1216", "--to", d, "--fileinfo"], ["rm", ziel, "mutos_1216"]):
        r = subprocess.run([k] + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if r.returncode:
            raise Fehler("disktool %s: %s" % (args[0], r.stdout.decode("latin-1")))
    open(os.path.join(d, "mutos_1216"), "wb").write(kernel_bytes)
    r = subprocess.run([k, "put", ziel, os.path.join(d, "mutos_1216")], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode:
        raise Fehler("disktool put: " + r.stdout.decode("latin-1"))
    return ziel


BOOTTEST = [
    ("b1", "ls -l / >/tmp/b1 2>&1"),
    ("b2", "(ls /etc | cat) >/tmp/b2 2>&1"),
    ("b3", "(cat t.c | cat | cat) >/tmp/b3 2>&1"),
    ("b4", "cc -c t.c >/tmp/b4 2>&1"),
    ("b5", "ls -l t.o >/tmp/b5 2>&1"),
    ("b6", "date >/tmp/b6 2>&1"),
    ("b7", "(cat t.c >/tmp/e1; cat /tmp/e1; ls -l /tmp/e1) >/tmp/b7 2>&1"),
    ("b8", "(cc t2.c; a.out; a.out | cat) >/tmp/b8 2>&1"),
    ("b9", "(cc -O -c prf.c; ls -l prf.o) >/tmp/b9 2>&1"),
]


def boot_pruefen(arbeit, kernel_bytes, frist=20_000_000_000):
    """Bootet den Kernel (BOOT MUTOS), meldet sich als root an und fuehrt kleine Arbeitstests aus; gibt {name: text} zurueck."""
    import gast_as
    lade = ladediskette(arbeit, kernel_bytes)
    quellen = {"t.c": "int f(a)\nint a;\n{\n\treturn a + 1;\n}\n",
               "t2.c": "main()\n{\n\tint i, s;\n\ts = 0;\n\tfor (i = 1; i <= 100; i++)\n\t\ts += i;\n"
                       "\tprintf(\"summe = %d\\n\", s);\n}\n",
               "prf.c": open(os.path.join(CQUELLEN, "prf.c"), "rb").read()}
    for h in sorted(os.listdir(os.path.join(CQUELLEN, "h"))):
        if h.endswith(".h"):
            quellen[h] = open(os.path.join(CQUELLEN, "h", h), "rb").read()
    aus = {}
    erg = gast_as.gast_befehle(os.path.join(arbeit, "boot"), quellen, [b for _, b in BOOTTEST], [n for n, _ in BOOTTEST],
                               frist=frist, ausgabe=aus, boot_hfe=lade, schirm=True)
    print("Boot-Test Laufzeit (Wanduhr): %.0f s, Protokoll %s" % (aus["dauer"], aus["log"]))
    ok = True
    for n, b in BOOTTEST:
        v = erg.get(n)
        print("--- %s  (%s)" % (b, "fehlt" if v is None else "ok"))
        if v is None:
            ok = False
        else:
            print(v.decode("latin-1").rstrip())
    return ok


def natuerlich(welt, arbeit, a):
    import gast_as
    q, absnamen = nat_quellen(welt, arbeit)
    print("natuerlicher Bau: %d Dateien, %d Byte; absolute Symbole: %s" % (len(q), sum(len(v) for v in q.values()),
                                                                          " ".join("%s=%04X" % x for x in absnamen)))
    mk = os.path.join(HIER, "make")
    q["make"] = open(os.path.join(mk, "make.bin"), "rb").read()
    q["make.fileinfo"] = open(os.path.join(mk, "make.bin.fileinfo"), "r").read()
    if a.nur_plan:
        return 0
    befehle = ["(cd /tmp; /mnt/make -f /mnt/Makefile) >/tmp/make.log 2>&1", "ls -l /tmp >/tmp/ls.log 2>&1"]
    aus = {}
    erg = gast_as.gast_befehle(os.path.join(arbeit, "gast_nat"), q, befehle, ["make.log", "ls.log", "lk"], frist=a.frist, ausgabe=aus)
    print("Gast-Laufzeit (Wanduhr): %.0f s, Protokoll %s" % (aus["dauer"], aus["log"]))
    for n in ("make.log", "ls.log"):
        print("---", n)
        print((erg.get(n) or b"(fehlt)").decode("latin-1")[-3000:])
    b = erg.get("lk")
    if not b:
        print("lk: nicht erzeugt")
        return 1
    open(os.path.join(arbeit, "lk"), "wb").write(b)
    magic, text, data, bss = struct.unpack(">4H", b[:8])
    print("lk: Text %d Daten %d bss %d" % (text, data, bss))
    voll = b[16:16 + text + data]
    bsslen = BSS_BIS - BSS_VON
    ist = voll[:len(voll) - bsslen]
    ende = len(ist)
    print("Kernel: Text+Daten %d Byte (Original %d); BSS ab %04X bis %04X (Grenze %04X)" % (ende, BSS_VON, ende, ende + bsslen, BSS_BIS))
    if ende + bsslen > BSS_BIS:
        print("FEHLER: BSS reicht ueber das Postfach")
        return 1
    ist = ist.ljust((ende + 511) // 512 * 512, b"\0")
    open(os.path.join(arbeit, "mutos_1216.nat"), "wb").write(ist)
    if bss != 0:
        return 1
    if a.boot and not boot_pruefen(os.path.join(arbeit, "nat_boot"), ist):
        return 1
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--arbeit", required=True)
    ap.add_argument("--objekte")
    ap.add_argument("--uebersetzen", action="store_true")
    ap.add_argument("--stufe", type=int, default=1)
    ap.add_argument("--ausser", default="", help="Funktionsnamen, die trotzdem aus dem Assembler kommen")
    ap.add_argument("--nur", default="", help="nur diese C-Dateien verwenden (Komma)")
    ap.add_argument("--nur-plan", action="store_true")
    ap.add_argument("--frist", type=int, default=20_000_000_000)
    ap.add_argument("--natuerlich", action="store_true", help="make im Gast: alle 29 C-Dateien mit cc, dazu low/bss/shim/abs")
    ap.add_argument("--boot", action="store_true", help="den gebundenen Kernel auf eine Ladediskette und booten + Arbeitstests")
    a = ap.parse_args(argv)
    sys.path.insert(0, HIER)
    import gast_as
    import rundlauf_gast
    arbeit = os.path.abspath(a.arbeit)
    os.makedirs(arbeit, exist_ok=True)
    if a.uebersetzen:
        uebersetzen(arbeit)
    objdir = a.objekte or os.path.join(arbeit, "obj")
    kernel = rundlauf_gast.kernel_holen()
    welt = Welt(kernel, objdir)
    if a.natuerlich:
        return natuerlich(welt, arbeit, a)
    erlaubt = {"GLEICH"} if a.stufe == 1 else {"GLEICH", "RELOC", "ABW", "LAENGE"}
    segs = benenne(plan(welt, erlaubt, set(x for x in a.ausser.split(",") if x), set(x for x in a.nur.split(",") if x) or None))
    nc = sum(len(s[4]) for s in segs if s[1] == "c")
    nall = len(welt.funcs)
    print("Stufe %d: %d von %d C-Funktionen verwendet, %d Segmente (%d C)" % (a.stufe, nc, nall, len(segs),
                                                                         sum(1 for s in segs if s[1] == "c")))
    for w in welt.warn:
        print("  Hinweis:", w)
    if a.nur_plan:
        for s in segs:
            print(s[0], s[1], "%04X-%04X" % (s[2], s[3]), ",".join(f.name for f in s[4]) if s[1] == "c" else "")
        return 0
    # Quellen: zuerst die C-Objekte (sie bestimmen, welche Tabellen mitgebunden und welche Altdaten Waisen sind)
    komp = {}
    for name, kind, von, bis, *rest in segs:
        if kind == "c":
            komp[name] = komposit(welt, rest[0])
    waise = waisen(welt)
    dst = datenstuecke(waise)
    if waise:
        print("Waisen im Datenbereich (ersetzt durch mitgebundene Tabellen): %s, zusammen %d Byte" %
              (", ".join("%04X-%04X" % w for w in waise), sum(b - v for v, b in waise)))
    print("Mitgebundene Datenstuecke: %d, %d Byte" % (len(welt.mitgebunden), sum(len(x[4]) for x in welt.mitgebunden)))
    lk = os.path.join(arbeit, "linkkarte_neu.txt")
    open(lk, "w").write(linkkarte_neu(segs, dst))
    abbild = os.path.join(arbeit, "mutos_1216")
    open(abbild, "wb").write(kernel)
    definiert = set(welt.adrname(f.addr) for s in segs if s[1] == "c" for f in s[4])
    quellen = {}
    namen = []

    def zdis(name):
        p = subprocess.run([rundlauf_gast.Z8KDIS, "--karte", KARTE, "--karte", lk, "--format", "as", "--modul", name, abbild],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if p.returncode:
            raise Fehler("z8kdis %s: %s" % (name, p.stderr.decode("latin-1")))
        return p.stdout.decode("latin-1")
    for name, kind, von, bis, *rest in segs:
        if kind == "a":
            quellen[name + ".s"] = zdis(name)
            namen.append((name, "a"))
        else:
            quellen[name + ".o"] = komp[name]
            namen.append((name, "c"))
    for i in range(len(dst)):
        n = "da%02d" % i
        quellen[n + ".s"] = tote_marken(zdis(n), segs, definiert)
        namen.append((n, "a"))
    # Auffuellen, damit die BSS-Variablen (8DC0..D256) an ihren Original-Adressen bleiben (Postfach ab D256 ist fest)
    tt = sum(bis - von for n, k, von, bis, *r in segs if k == "a")
    dd = 12 + sum(b - v for v, b in dst)
    for n, k, von, bis, *r in segs:
        if k == "c":
            oc = O.Objekt.lesen(komp[n])
            tt += len(oc.text)
            dd += len(oc.data)
    fuell = BSS_VON - tt - dd
    print("Text %d Byte (Original %d), Daten %d Byte (Original %d), Auffuellung bis 8DC0: %d Byte" % (tt, TEXT_ENDE, dd, BSS_VON - TEXT_ENDE, fuell))
    if fuell < 0:
        raise Fehler("Kernel %d Byte zu gross: die BSS-Variablen rutschten ueber das Postfach ab D256" % -fuell)
    if fuell:
        quellen["pad.s"] = ("pad module\n$NONSEGMENTED\nconstant\n\tG\t:= global\n\tA\t:= array\n\tB\t:= byte\nG\tLPAD\tA[%%%X B]\nend pad\n" % fuell)
        namen.append(("pad", "a"))
    quellen["bss.s"] = zdis("bss")
    namen.append(("bss", "a"))
    # Adressliste neu gegen alt
    with open(os.path.join(arbeit, "adressen_neu.txt"), "w") as af:
        pos = 0
        for n, k, von, bis, *r in segs:
            if k == "a":
                pos += bis - von
            else:
                for f in r[0]:
                    t, _ = kernelfertig(welt, f)
                    af.write("%s %04X %04X %+d\n" % (f.name, f.addr, pos, pos - f.addr))
                    pos += len(t)
    gross = sum(len(v) for v in quellen.values())
    print("Quellen/Objekte fuer den Gast: %d Dateien, %d Byte" % (len(quellen), gross))
    befehle = []
    rueck = ["ld.err"]
    for n, k in namen:
        if k == "a":
            befehle.append("as -u -o /tmp/%s.o %s.s >/tmp/%s.err 2>&1" % (n, n, n))
            rueck.append(n + ".err")
    quellen["ldlauf"] = "ld -s -o /tmp/lk " + " ".join(("/tmp/%s.o" if k == "a" else "%s.o") % n for n, k in namen) + "\n"
    befehle.append("sh ldlauf >/tmp/ld.err 2>&1")
    rueck.append("lk")
    befehle.append("rm /tmp/*.o")
    aus = {}
    erg = gast_as.gast_befehle(os.path.join(arbeit, "gast"), quellen, befehle, rueck, frist=a.frist, ausgabe=aus)
    print("Gast-Laufzeit (Wanduhr): %.0f s, Protokoll %s" % (aus["dauer"], aus["log"]))
    fehler = 0
    for n in rueck:
        if n.endswith(".err"):
            t = erg.get(n)
            if t is None:
                print("%s fehlt" % n)
                fehler += 1
            elif t.strip():
                print("%s:\n%s" % (n, t.decode("latin-1")))
                fehler += 1
    b = erg.get("lk")
    if not b:
        print("lk: nicht erzeugt")
        return 1
    magic, text, data, bss = struct.unpack(">4H", b[:8])
    print("lk: Text %d Daten %d bss %d" % (text, data, bss))
    voll = b[16:16 + text + data]
    bsslen = BSS_BIS - BSS_VON          # die BSS-Variablen liegen als Nullen am Ende (siehe linkbau.md), der Kernel schneidet sie ab
    ist = voll[:len(voll) - bsslen]
    assert not any(voll[len(voll) - bsslen:]), "BSS nicht null"
    ende = (len(ist) + 511) // 512 * 512
    ist = ist.ljust(ende, b"\0")
    open(os.path.join(arbeit, "mutos_1216.neu"), "wb").write(ist)
    print("Kernel: Text+Daten %d Byte (Original %d), Datei %d Byte, Ende der Daten %04X, BSS bis %04X (Grenze %04X)" %
          (len(voll) - bsslen, BSS_VON, len(ist), len(voll) - bsslen, len(voll), BSS_BIS))
    d = [i for i in range(min(len(kernel), len(ist))) if ist[i] != kernel[i]]
    gleich = not d and len(ist) == len(kernel)
    print("Gesamt: %s (%d Byte ungleich; erste bei %s)" % ("BYTEGLEICH" if gleich else "ABWEICHUNG", len(d),
                                                         ", ".join("%04X" % x for x in d[:8])))
    if a.stufe == 1 and not gleich:
        fehler += 1
    if a.boot:
        if not boot_pruefen(arbeit, ist, a.frist):
            fehler += 1
    return 0 if not fehler else 1



# ---------------------------------------------------------------- "Natuerlicher" Bau: make im Gast aus den C-Quellen
# Alle 29 C-Dateien kommen unveraendert vom Gast-`cc` (cc -O -c, dev0.c ohne -O); nur `low` (Handassembler mch.s, 26 Funktionen)
# und die BSS-Zuteilung kommen aus z8kdis (Assembler), dazu zwei Zeilen Klebstoff (shim.s) und die absoluten Adressen (abs.o).
NAT_CDATEIEN = CDATEIEN + CDATEN
# Anfangswerte, die der Kernel in keiner C-Datei definiert (extern in main.c, lp.c, slp.c ...): als Daten aus dem Abbild
NAT_DATEN = [(0x88CC, 0x88D4), (0x8A8C, 0x8A8E), (0x8B32, 0x8B4E), (0x8C94, 0x8C9A)]
SHIM = """shim module
$NONSEGMENTED
constant
\tFP\t:= r14
\tSP\t:= r15
\tX\t:= external
\tG\t:= global
\tI\t:= internal
\tA\t:= array
\tPR\t:= procedure
\tE\t:= entry
\tB\t:= byte
\tW\t:= word
\tBV\t:= bval
\tWV\t:= wval
\tLV\t:= lval
\tL\t:= label
G
_save\tL
G
_mchinit\tL
G\t_p_shim\tPR
E
_save:
\tjp\t_savu
_mchinit:
\tjp\t_ttyinit
end _p_shim
end shim
"""


def nat_namen(welt):
    """Kartenzeilen `name ADR _x`: C-Namen fuer Funktionen und BSS-Variablen, damit Assembler und C-Objekte dieselben Namen tragen."""
    z = []
    gesehen = {}
    for f in welt.funcs:
        n = ("_" + f.name)[:8]
        gesehen[f.addr] = n
        z.append("name %04X %s" % (f.addr, n))
    for o in welt.objekte.values():
        for s in o.syms:
            nm = s.name.decode("latin-1")
            m = re.fullmatch(r"_cf_([0-9a-f]{4})", nm)
            if m and nm not in welt.symadr and s.basistyp == O.N_UNDF:
                welt.symadr[nm] = int(m.group(1), 16)
    for n, a in (("_cf_8c94", 0x8C94),):
        welt.symadr.setdefault(n, a)
    welt.alias.setdefault("_backup", "_sigret")
    for n, a in sorted(welt.symadr.items(), key=lambda x: x[1]):
        if n in ("csv", "cret", "_save", "_mchinit", "_backup") or n.startswith("L"):
            continue
        if 0x875A <= a < BSS_VON and n not in ("_proctop", "_nlp", "_szicode", "_icode", "_regloc", "_conptr", "_cf_8c94"):
            continue
        if a < 0x0674 or 0x875A <= a < BSS_BIS or BSS_VON <= a < BSS_BIS:
            if a in gesehen and gesehen[a] != n:
                if a < 0x0674:
                    continue            # Alias im Handassembler (z. B. _savu/_save): shim.s
                welt.alias[n] = gesehen[a]      # zwei C-Namen fuer eine Variable: die Dateien bekommen -Dalt=neu
                continue
            gesehen[a] = n
            z.append("name %04X %s" % (a, n))
    return "\n".join(z) + "\n"


def nat_abs(welt):
    """Objekt mit den absoluten Symbolen (u bei FC00, Postfach und Puffer ab D256)."""
    o = O.Objekt()
    for n, a in sorted(welt.symadr.items(), key=lambda x: x[1]):
        if a >= KONST_AB and not n.startswith("L") and n not in ("csv", "cret"):
            o.syms.append(O.Symbol(n.encode()[:8], O.N_ABS | O.N_EXT, 0, a))
    o.text = b""
    o.data = b""
    return o.schreiben(), [(s.name.decode(), s.wert) for s in o.syms]


MAKEFILE = """# Makefile fuer den MUTOS-Kernel (AP K8, Entwurf 41).  Aufruf im Gast:  cd /tmp; /mnt/make -f /mnt/Makefile
# Quellen und Header liegen auf /mnt (Datentraeger D), die Objekte und das Ergebnis auf /tmp (Datentraeger A).
SRC = /mnt
CFLAGS = -O -I$(SRC)
OBJS = low.o shim.o abs.o %(objs)s %(dn)s bss.o

lk: $(OBJS)
\tld -s -o lk $(OBJS)

low.o: $(SRC)/low.s
\tas -u -o low.o $(SRC)/low.s
shim.o: $(SRC)/shim.s
\tas -u -o shim.o $(SRC)/shim.s
bss.o: $(SRC)/bss.s
\tas -u -o bss.o $(SRC)/bss.s
abs.o: $(SRC)/abs.o
\tcat $(SRC)/abs.o >abs.o
%(regeln)s
"""


def nat_quellen(welt, arbeit):
    import rundlauf_gast
    q = {}
    abb = os.path.join(arbeit, "mutos_1216")
    open(abb, "wb").write(welt.k)
    ov = os.path.join(arbeit, "namen_c.txt")
    open(ov, "w").write(nat_namen(welt))
    lk = os.path.join(arbeit, "linkkarte_nat.txt")
    z = [l.rstrip("\n") for l in open(LINKKARTE) if not l.startswith("modul ")]
    z.append("modul low 0000 0674 875A 8766")
    z.append("modul rest 0674 875A - -")       # nur damit z8kdis weiss, welche Marken von low aus fremd benutzt werden
    for i, (v, b) in enumerate(NAT_DATEN):
        z.append("modul dn%d 875A 875A %04X %04X" % (i, v, b))
    open(lk, "w").write("\n".join(z) + "\n")
    for n in ["low", "bss"] + ["dn%d" % i for i in range(len(NAT_DATEN))]:
        p = subprocess.run([rundlauf_gast.Z8KDIS, "--karte", KARTE, "--karte", ov, "--karte", lk, "--format", "as", "--modul", n, abb],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if p.returncode:
            raise Fehler("z8kdis %s: %s" % (n, p.stderr.decode("latin-1")))
        txt = p.stdout.decode("latin-1")
        if n.startswith("dn"):      # in keinem anderen Modul benutzte Marken waeren sonst nicht GLOBAL
            for lab in re.findall(r"^(_\w+):", txt, re.M):
                if "G\n%s\tL\n" % lab not in txt:
                    txt = txt.replace("G\t_p_", "G\n%s\tL\nG\t_p_" % lab, 1)
        q[n + ".s"] = txt
    q["shim.s"] = SHIM
    ab, namen = nat_abs(welt)
    q["abs.o"] = ab
    for c in NAT_CDATEIEN:
        q[c + ".c"] = open(os.path.join(CQUELLEN, c + ".c"), "rb").read()
    for h in sorted(os.listdir(os.path.join(CQUELLEN, "h"))):
        if h.endswith(".h"):
            q[h] = open(os.path.join(CQUELLEN, "h", h), "rb").read()
    hs = " ".join("$(SRC)/%s" % h for h in sorted(os.listdir(os.path.join(CQUELLEN, "h"))) if h.endswith(".h"))
    regeln = []
    for c in NAT_CDATEIEN:
        # Dateien, die eine Variable unter einem zweiten Namen benutzen, bekommen ein -D auf den gemeinsamen Namen
        o = welt.objekte.get(c)
        ds = ""
        if o is not None:
            for s in o.syms:
                nm = s.name.decode("latin-1")
                if nm in welt.alias and s.basistyp == O.N_UNDF:
                    ds += "-D%s=%s " % (nm[1:], welt.alias[nm][1:])
        regeln.append("%s.o: $(SRC)/%s.c $(SRC)/param.h\n\tcc %s%s-I$(SRC) -c $(SRC)/%s.c" % (c, c, "" if c in OHNE_O else "-O ", ds, c))
    q["Makefile"] = MAKEFILE % {"objs": " ".join(c + ".o" for c in NAT_CDATEIEN), "regeln": "\n".join(regeln + ["dn%d.o: $(SRC)/dn%d.s\n\tas -u -o dn%d.o $(SRC)/dn%d.s" % (i, i, i, i) for i in range(len(NAT_DATEN))]),
                                  "dn": " ".join("dn%d.o" % i for i in range(len(NAT_DATEN)))}
    return q, namen


if __name__ == "__main__":
    sys.exit(main())

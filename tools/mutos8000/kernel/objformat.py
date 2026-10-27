"""a.out-Objekt- und Archivformat der MUTOS-Werkzeuge (as, ld, nm, cc) -- AP K2.

Belegt an crt0.o, mcrt0.o und libc.a der MUTOS-Disketten (doc/mutos_kernel/objformat.md).
Alles big-endian (Z8000), Standardbibliothek, ausfuehren mit `python3 -I`.

Objekt:   Kopf 16 B (8 Worte magic text data bss syms entry unused flag)
          text | data | Relokation text (1 Wort je Textwort) | Relokation data | Symbole (12 B)
Relokwort: (symnr << 4) | (art << 1) | pcrel ; art 0=abs 1=text 2=data 3=bss 4=extern
Symbol:   8 B Name (NUL aufgefuellt, ohne Terminator bei 8 Zeichen), Typ 1 B, unused 1 B, Wert 16 Bit
Archiv:   Magic FF65 (0177545), je Mitglied 26-B-Kopf (V7-artig, aber mit LONG size) + Daten,
          auf gerade Laenge aufgefuellt (vermutet, s. Doku).
"""
import struct
import sys

A_MAGICS = (0xE807, 0xE811, 0xE805)
ARMAG = 0xFF65
AR_HDR = 26

N_UNDF, N_ABS, N_TEXT, N_DATA, N_BSS = 0, 1, 2, 3, 4
N_REG, N_SN, N_FN = 0x14, 0x1E, 0x1F
N_EXT = 0x20
R_ABS, R_TEXT, R_DATA, R_BSS, R_EXT = 0, 1, 2, 3, 4
R_NAMEN = ("abs", "text", "data", "bss", "ext")
TYPNAMEN = {0: "U", 1: "A", 2: "T", 3: "D", 4: "B", 0x14: "r", 0x1E: "s", 0x1F: "f"}


class Fehler(Exception):
    pass


class Symbol:
    def __init__(self, name, typ, unused, wert):
        self.name = name        # bytes, bis 8, ohne Auffuell-NUL
        self.typ = typ
        self.unused = unused
        self.wert = wert

    def roh(self):
        return self.name.ljust(8, b"\0") + struct.pack(">BBH", self.typ, self.unused, self.wert)

    @property
    def ext(self):
        return bool(self.typ & N_EXT)

    @property
    def basistyp(self):
        return self.typ & 0x1F

    def text(self):
        t = TYPNAMEN.get(self.basistyp, "?")
        if self.ext:
            t = t.upper() if t.isalpha() else t
        else:
            t = t.lower()
        return "%04x %s %s" % (self.wert, t, self.name.decode("latin-1"))


class Reloc:
    def __init__(self, wort):
        self.wort = wort

    @property
    def pcrel(self):
        return self.wort & 1

    @property
    def art(self):
        return (self.wort >> 1) & 7

    @property
    def sym(self):
        return self.wort >> 4

    def text(self):
        s = R_NAMEN[self.art] if self.art < len(R_NAMEN) else "art%d" % self.art
        if self.art == R_EXT:
            s += " #%d" % self.sym
        return s + (" pc" if self.pcrel else "")


class Objekt:
    def __init__(self):
        self.magic = 0xE807
        self.entry = 0
        self.unused = 0
        self.flag = 0
        self.text = b""
        self.data = b""
        self.bss = 0
        self.rtext = []     # Liste von Relokwoerten (int)
        self.rdata = []
        self.syms = []
        self.rest = b""     # Bytes hinter der Symboltabelle (bei Archivmitgliedern erwartet leer)

    @classmethod
    def lesen(cls, b):
        if len(b) < 16:
            raise Fehler("zu kurz fuer einen Kopf")
        m, t, d, bss, sy, en, un, fl = struct.unpack(">8H", b[:16])
        if m not in A_MAGICS:
            raise Fehler("kein a.out-Magic: %04x" % m)
        o = cls()
        o.magic, o.bss, o.entry, o.unused, o.flag = m, bss, en, un, fl
        p = 16
        o.text = b[p:p + t]; p += t
        o.data = b[p:p + d]; p += d
        if not (fl & 1):    # AF_STRIP: keine Relokation, keine Symbole
            n = t // 2
            o.rtext = list(struct.unpack(">%dH" % n, b[p:p + 2 * n])); p += 2 * n
            n = d // 2
            o.rdata = list(struct.unpack(">%dH" % n, b[p:p + 2 * n])); p += 2 * n
        for i in range(sy // 12):
            q = p + 12 * i
            nm = b[q:q + 8].rstrip(b"\0")
            ty, un2, wert = struct.unpack(">BBH", b[q + 8:q + 12])
            o.syms.append(Symbol(nm, ty, un2, wert))
        p += sy
        o.rest = b[p:]
        if len(o.text) != t or len(o.data) != d:
            raise Fehler("Datei abgeschnitten")
        return o

    def schreiben(self):
        # Namen mit exakt 8 Zeichen haben keine NUL; rstrip beim Lesen ist verlustfrei,
        # solange der Name selbst nicht auf NUL endet (nie belegt).
        sy = b"".join(s.roh() for s in self.syms)
        out = struct.pack(">8H", self.magic, len(self.text), len(self.data), self.bss,
                          len(sy), self.entry, self.unused, self.flag)
        out += self.text + self.data
        if not (self.flag & 1):
            out += struct.pack(">%dH" % len(self.rtext), *self.rtext)
            out += struct.pack(">%dH" % len(self.rdata), *self.rdata)
        return out + sy + self.rest

    def relocs(self, teil="text"):
        r = self.rtext if teil == "text" else self.rdata
        return [(2 * i, Reloc(w)) for i, w in enumerate(r) if w]

    def maskierter_text(self):
        """Text mit 0 an allen relokierten Woertern; zweiter Wert: Maske (1 = Wildcard) je Byte."""
        t = bytearray(self.text)
        mask = bytearray(len(t))
        for off, r in self.relocs("text"):
            t[off:off + 2] = b"\0\0"
            mask[off:off + 2] = b"\1\1"
        return bytes(t), bytes(mask)


class Mitglied:
    def __init__(self, name, datum, uid, gid, mode, daten, pad=b""):
        self.name = name    # bytes ohne Auffuell-NUL
        self.datum = datum
        self.uid = uid
        self.gid = gid
        self.mode = mode
        self.daten = daten
        self.pad = pad      # Fuellbyte(s) hinter ungerader Laenge

    def kopf(self):
        return (self.name.ljust(14, b"\0") +
                struct.pack(">IBBHI", self.datum, self.uid, self.gid, self.mode, len(self.daten)))

    def objekt(self):
        return Objekt.lesen(self.daten)


class Archiv:
    def __init__(self, mitglieder=None):
        self.mitglieder = mitglieder or []

    @classmethod
    def lesen(cls, b):
        if len(b) < 2 or struct.unpack(">H", b[:2])[0] != ARMAG:
            raise Fehler("kein Archiv (Magic FF65)")
        p, ms = 2, []
        while p < len(b):
            if p + AR_HDR > len(b):
                raise Fehler("abgeschnittener Mitgliedskopf bei %d" % p)
            name = b[p:p + 14].rstrip(b"\0")
            datum, uid, gid, mode, size = struct.unpack(">IBBHI", b[p + 14:p + AR_HDR])
            p += AR_HDR
            daten = b[p:p + size]
            if len(daten) != size:
                raise Fehler("Mitglied %r abgeschnitten" % name)
            p += size
            pad = b""
            if size & 1 and p < len(b):
                pad = b[p:p + 1]
                p += 1
            ms.append(Mitglied(name, datum, uid, gid, mode, daten, pad))
        return cls(ms)

    def schreiben(self):
        out = struct.pack(">H", ARMAG)
        for m in self.mitglieder:
            out += m.kopf() + m.daten + m.pad
        return out


def nm_zeilen(o):
    """Wie nm: nur benannte Symbole ohne Debug-Typen (r/s/f werden mit ausgegeben, markiert)."""
    return [s.text() for s in o.syms]


def dump(o, name=""):
    z = ["%s magic %04x text %d data %d bss %d syms %d entry %04x unused %04x flag %04x" %
         (name, o.magic, len(o.text), len(o.data), o.bss, 12 * len(o.syms), o.entry, o.unused, o.flag)]
    for teil, b in (("text", o.text), ("data", o.data)):
        rl = dict(o.relocs(teil))
        for i in range(0, len(b), 2):
            w = b[i:i + 2]
            r = rl.get(i)
            z.append("  %s %04x: %s%s" % (teil, i, w.hex(), ("  " + r.text()) if r else ""))
    for i, s in enumerate(o.syms):
        z.append("  sym %2d: %s" % (i, s.text()))
    return "\n".join(z)


def _lies(pfad):
    with open(pfad, "rb") as f:
        return f.read()


def main(argv):
    if len(argv) < 3 or argv[1] not in ("dump", "nm", "ar-list", "ar-extract"):
        print("usage: objformat.py dump|nm|ar-list|ar-extract <datei> [verzeichnis]")
        return 0 if "--help" in argv else 2
    b = _lies(argv[2])
    cmd = argv[1]
    if cmd in ("dump", "nm"):
        objs = ([(m.name.decode("latin-1"), m.objekt()) for m in Archiv.lesen(b).mitglieder]
                if b[:2] == b"\xff\x65" else [("", Objekt.lesen(b))])
        for n, o in objs:
            if n:
                print("%s:" % n)
            print(dump(o) if cmd == "dump" else "\n".join(nm_zeilen(o)))
    elif cmd == "ar-list":
        for m in Archiv.lesen(b).mitglieder:
            print("%-14s %6d  %08x %d/%d %o" % (m.name.decode("latin-1"), len(m.daten),
                                                m.datum, m.uid, m.gid, m.mode))
    else:
        import os
        ziel = argv[3] if len(argv) > 3 else "."
        os.makedirs(ziel, exist_ok=True)
        for m in Archiv.lesen(b).mitglieder:
            n = m.name.decode("latin-1")
            if "/" in n or n in ("", ".", ".."):
                raise Fehler("unzulaessiger Mitgliedsname %r" % n)
            with open(os.path.join(ziel, n), "wb") as f:
                f.write(m.daten)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

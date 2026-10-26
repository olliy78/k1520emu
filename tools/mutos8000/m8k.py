"""Gemeinsame Bausteine der MUTOS-8000-Auswertung (Entwurf 39, Abschnitte 4.6.3, 4.7.6, 4.8).

Nur Standardbibliothek.  Enthalten sind
  * Leser fuer TeleDisk (.td0, RLE-Methoden 0-2) und ImageDisk (.imd),
  * die Blockabbildung `Mutos8kDev` der 128-B-Disketten (Verschraenkung 2, Spurversatz 6),
  * die Rueckschiebung der beschaedigten Wurzel M8KROOT (1 Bit, 80 von 128 Byte brauchbar),
  * Inode-, Verzeichnis- und Dateilesen (lueckenhaft fuer M8KROOT, vollstaendig fuer heile Abbilder).

Die Module liegen neben den Skripten; jedes Skript fuegt sein Verzeichnis selbst in sys.path ein,
damit es auch mit `python3 -I` laeuft.
"""
import os
import struct
import sys

# Repo-Wurzel (tools/mutos8000/m8k.py -> ../..)
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DISKS = os.path.join(REPO, "disks", "a5120_mutos8000")


FERTIG = os.path.join(REPO, "disks", "a5120_16_MUTOS")


def fertig(name):
    """Pfad einer gebauten (fertigen) MUTOS-8000-Diskette in disks/a5120_16_MUTOS/."""
    return os.path.join(FERTIG, "a5120_mutos8000_" + name)


def disk(name):
    """Pfad einer eingecheckten Quelle (Original oder Archiv) in disks/a5120_mutos8000/."""
    return os.path.join(DISKS, "a5120_mutos8000_" + name)


# --------------------------------------------------------------------------- TD0 / IMD

def _td0_sektordaten(d, i):
    """Sektordatenblock: Laenge(2), Methode(1), Daten.  Gibt (Daten, naechste Position)."""
    ln = struct.unpack_from("<H", d, i)[0]
    meth = d[i + 2]
    body = d[i + 3:i + 2 + ln]
    i += 2 + ln
    if meth == 0:                       # roh
        return bytes(body), i
    if meth == 1:                       # Wiederholung eines 2-Byte-Musters
        cnt, pat = struct.unpack_from("<H2s", body)
        return pat * cnt, i
    out = bytearray()                   # Methode 2: gemischt roh / Muster
    j = 0
    while j < len(body):
        t = body[j]
        j += 1
        if t == 0:                      # roh: n Byte
            n = body[j]
            j += 1
            out += body[j:j + n]
            j += n
        else:                           # Muster der Laenge 2**t, c-mal
            plen = 1 << t
            cnt = body[j]
            j += 1
            out += body[j:j + plen] * cnt
            j += plen
    return bytes(out), i


def td0_laden(pfad):
    """TD0 (unkomprimiert, 'TD') lesen.  Ergebnis: [(zyl, kopf, [Sektor...])]; Sektor = dict
    c,h,r,n,flags,data (data None bei Fehler-/Pseudosektor ohne Daten)."""
    d = open(pfad, "rb").read()
    if d[:2] != b"TD":
        raise ValueError("%s: keine unkomprimierte TD0 (Kennung TD)" % pfad)
    i = 12
    if d[7] & 0x80:                     # Kommentarblock
        n = struct.unpack_from("<H", d, 14)[0]
        i = 12 + 10 + n
    spuren = []
    while True:
        ns, cyl, head, _crc = d[i:i + 4]
        i += 4
        if ns == 255:
            break
        secs = []
        for _ in range(ns):
            c, h, r, n, flags, _crc8 = d[i:i + 6]
            i += 6
            data = None
            if not (flags & 0x30):
                data, i = _td0_sektordaten(d, i)
            secs.append(dict(c=c, h=h, r=r, n=n, flags=flags, data=data))
        spuren.append((cyl, head, secs))
    return spuren


def imd_laden(pfad):
    """IMD lesen (z. B. die gw-Umwandlungen der SCP-Fluesse).  Ergebnis: [(zyl, kopf, [Sektor...], modus)]."""
    d = open(pfad, "rb").read()
    i = d.index(b"\x1a") + 1
    spuren = []
    while i < len(d):
        modus, cyl, head, ns, sz = d[i:i + 5]
        i += 5
        smap = list(d[i:i + ns])
        i += ns
        if head & 0x80:
            i += ns
        if head & 0x40:
            i += ns
        size = 128 << sz
        secs = []
        for r in smap:
            t = d[i]
            i += 1
            if t == 0:
                data = None
            elif t in (2, 4, 6, 8):     # komprimiert: ein Fuellbyte
                data = bytes([d[i]]) * size
                i += 1
            else:
                data = d[i:i + size]
                i += size
            secs.append(dict(r=r, data=data, t=t))
        spuren.append((cyl, head & 1, secs, modus))
    return spuren


def spuren_laden(pfad):
    """.td0 oder .imd nach Endung; Ergebnis wie td0_laden (Kopf-/Zusatzfelder je nach Quelle)."""
    if pfad.lower().endswith(".imd"):
        return [(c, h, s) for c, h, s, _m in imd_laden(pfad)]
    return td0_laden(pfad)


# --------------------------------------------------------------------------- Blockabbildung

class Mutos8kDev:
    """Blockgeraet der 128-B-Disketten (Entwurf 39, 4.6.3): 4 Sektoren = 1 Block (512 B),
    Blockraum ab Zylinder 1, 26 Sektoren je Spur, Verschraenkung 2, Spurversatz 6 je Spur."""

    SEKTOREN_JE_SPUR = 26

    def __init__(self, spuren):
        # nach Zylinder; je Zylinder nach Sektornummer (wie das Original: Kopf wird nicht unterschieden)
        self.zyl = {c: {x["r"]: x for x in s} for c, _h, s, *_ in spuren}

    def sektor(self, b, k):
        """Sektor k (0..3) von Block b als dict oder None."""
        lin = 4 * b + k
        rel, pos = divmod(lin, self.SEKTOREN_JE_SPUR)
        platz = 2 * pos if pos < 13 else 2 * (pos - 13) + 1
        return self.zyl.get(1 + rel, {}).get(((platz + 6 * rel) % 26) + 1)

    def block(self, b):
        """Voller Block (512 B); fehlende/leere Sektoren sind ein Fehler (heile Abbilder)."""
        out = b""
        for k in range(4):
            x = self.sektor(b, k)
            if not x or x["data"] is None:
                raise KeyError("Block %d Sektor %d fehlt" % (b, k))
            out += x["data"]
        return out


# --------------------------------------------------------------------------- Rueckschiebung (M8KROOT)

# Echte Inodes von M8KROOT: ab 105 sind es Datenblock-Reste (Entwurf 39, 4.7.6)
INODES_ECHT = 104
GUT = 80    # brauchbare Byte je 128-B-Sektor der beschaedigten Wurzel


def rueckschieben(s):
    """Byte 0 roh, Byte 1 ohne oberstes Bit (unbekannt), Byte i>=2: s[i] >> 1 | (s[i-1] & 1) << 7.
    Bytes 80..127 sind verloren und werden nicht geliefert (Aufrufer fuellt mit Null)."""
    o = bytearray(s[:GUT])
    for i in range(1, GUT):
        o[i] = (s[i] >> 1) | ((s[i - 1] & 1) << 7 if i >= 2 else 0)
    return bytes(o)


def gut(off):
    """Ist das Byte an diesem Offset im Block brauchbar?"""
    return off % 128 < GUT


class Inode:
    def __init__(self, n, mode, nl, uid, gid, size, ad, nad, off):
        self.n, self.mode, self.nl, self.uid, self.gid = n, mode, nl, uid, gid
        self.size, self.ad, self.nad, self.off = size, ad, nad, off

    def art(self):
        return {0x4000: "d", 0x8000: "-", 0x2000: "c", 0x6000: "b"}.get(self.mode & 0xF000, "?")


class BeschaedigteWurzel:
    """M8KROOT: zurueckgeschobene, lueckenhafte Sicht (Entwurf 39, 4.7.6)."""

    def __init__(self, pfad):
        self.dev = Mutos8kDev(td0_laden(pfad))
        sb = self.block(1)
        self.isize, self.fsize = struct.unpack(">HI", sb[:6])
        self.inodes = {}
        for n in range(1, min(self.isize * 8, INODES_ECHT) + 1):
            ino = self.inode_roh(n)
            known = GUT - (ino[1] % 128)         # bekannte Byte dieses Inodes
            x = ino[0]
            mode, nl, uid, gid, size = struct.unpack(">HHHHI", x[:12])
            nad = max(0, min(13, (known - 12) // 3))
            ad = [int.from_bytes(x[12 + 3 * i:15 + 3 * i], "big") for i in range(nad)]
            if mode:
                self.inodes[n] = Inode(n, mode, nl, uid, gid, size, ad, nad, ino[1])

    def block(self, b):
        """512 B; je Sektor zurueckgeschoben + 48 Null, fehlender Sektor = 128 Null."""
        out = []
        for k in range(4):
            x = self.dev.sektor(b, k)
            if x and x["data"]:
                out.append(rueckschieben(x["data"]) + b"\0" * 48)
            else:
                out.append(b"\0" * 128)
        return b"".join(out)

    def inode_roh(self, n):
        o = ((n - 1) % 8) * 64
        return self.block(2 + (n - 1) // 8)[o:o + 64], o

    def zeit(self, n):
        """Zeit des Inodes n; liegt er am Sektorende (Offset 64), die des Nachbarn im selben Sektor
        (abgeleitet, Entwurf 39 4.8); Rueckfall 505000000."""
        for m in (n, n - 1, n + 1, n - 3):
            if m in self.inodes and m <= INODES_ECHT and ((m - 1) % 8) * 64 % 128 == 0:
                x, _o = self.inode_roh(m)
                return struct.unpack(">I", x[56:60])[0]
        return 505000000

    def daten(self, n):
        """(Bytes, Maske brauchbar) aus den direkten Adressen (mehr reichen die Inode-Reste nicht)."""
        i = self.inodes[n]
        got = bytearray()
        mask = []
        for k in range((i.size + 511) // 512):
            if k >= 10 or k >= i.nad or not i.ad[k]:
                break
            got += self.block(i.ad[k])
            mask += [gut(x) for x in range(512)]
        return bytes(got[:i.size]), mask[:i.size]

    def baum(self):
        """Verzeichnisbaum ab Inode 2.  Eintraege: (pfad, ino, mode|None, size|None, (gut, laenge, bytes)|None)."""
        seen = set()
        res = []

        def walk(n, pfad):
            if n in seen or n not in self.inodes:
                return
            seen.add(n)
            dat, m = self.daten(n)
            for e in range(0, len(dat), 16):
                if not all(m[e:e + 16]) or len(m[e:e + 16]) < 16:
                    continue                      # Eintrag nicht ganz lesbar
                ino = struct.unpack(">H", dat[e:e + 2])[0]
                nm = dat[e + 2:e + 16].split(b"\0")[0].decode("latin1")
                if not ino or nm in (".", ".."):
                    continue
                p = pfad + "/" + nm
                i = self.inodes.get(ino)
                if not i:
                    res.append((p, ino, None, None, None))
                    continue
                if i.mode & 0xF000 == 0x4000:
                    res.append((p + "/", ino, i.mode, i.size, None))
                    walk(ino, p)
                else:
                    dd, mm = self.daten(ino) if i.mode & 0xF000 == 0x8000 else (b"", [])
                    res.append((p, ino, i.mode, i.size, (sum(mm), len(mm), dd)))

        walk(2, "")
        return res


# --------------------------------------------------------------------------- heile Abbilder

class Dateisystem:
    """Unabhaengiger Leser fuer heile mutos8k_26x128_fm-Abbilder (Formel 4.6.3; kein DiskTool-Code)."""

    def __init__(self, pfad):
        self.dev = Mutos8kDev(spuren_laden(pfad))
        self.isize, self.fsize = struct.unpack(">HI", self.dev.block(1)[:6])

    def inode(self, n):
        ib = self.dev.block(2 + (n - 1) // 8)
        o = ((n - 1) % 8) * 64
        x = ib[o:o + 64]
        mode, nl, uid, gid, size = struct.unpack(">HHHHI", x[:12])
        ad = [int.from_bytes(x[12 + 3 * i:15 + 3 * i], "big") for i in range(13)]
        return mode, nl, uid, gid, size, ad

    def inhalt(self, n):
        mode, nl, uid, gid, size, ad = self.inode(n)

        def ind(b, lvl):
            r = []
            bb = self.dev.block(b)
            for i in range(128):
                a = struct.unpack(">I", bb[4 * i:4 * i + 4])[0]
                if a:
                    r += ind(a, lvl - 1) if lvl > 1 else [a]
            return r

        bl = [a for a in ad[:10] if a]
        if ad[10]:
            bl += ind(ad[10], 1)
        if ad[11]:
            bl += ind(ad[11], 2)
        return b"".join(self.dev.block(b) for b in bl)[:size]

    def belegt(self, n):
        """Alle Bloecke der Inode n: Daten UND Indirektbloecke (fuer die Grenze s_fsize)."""
        ad = self.inode(n)[5]
        res = [a for a in ad if a]

        def ind(b, lvl):
            for i in range(128):
                a = struct.unpack(">I", self.dev.block(b)[4 * i:4 * i + 4])[0]
                if a:
                    res.append(a)
                    if lvl > 1:
                        ind(a, lvl - 1)
        if ad[10]:
            ind(ad[10], 1)
        if ad[11]:
            ind(ad[11], 2)
        return res

    def eintraege(self, n):
        """Verzeichnis n: [(ino, name)] ohne '.', '..' und leere Slots."""
        dat = self.inhalt(n)
        res = []
        for e in range(0, len(dat), 16):
            ino = struct.unpack(">H", dat[e:e + 2])[0]
            nm = dat[e + 2:e + 16].split(b"\0")[0].decode("latin1")
            if ino and nm not in (".", ".."):
                res.append((ino, nm))
        return res

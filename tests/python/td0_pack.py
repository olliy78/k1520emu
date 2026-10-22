"""Kleiner TeleDisk-Baukasten fuer Testvektoren: ``.td0`` (``TD``) bauen und mit LZHUF zu ``td`` packen.

Nur nach der Clean-Room-Beschreibung ``doc/design/38a_td0_imd_formatbeschreibung.md`` Teil 2 und dem
oeffentlichen Okumura-Algorithmus geschrieben, NICHT nach dem C++-Kern.  Der Packer ist bewusst
einfach (gieriger Treffer, quadratisch) -- er muss nur korrekte Stroeme liefern.  Gegenprobe: das
C-Entpackmodul von Greaseweazle (``greaseweazle.optimised.td0_unpack``).  ``python3 td0_pack.py``
gibt den Testvektor des Kern-Tests (tests/unit/peripherals/test_td0_codec.cpp) als Hexfeld aus.
"""

import sys

N, F, THRESH = 4096, 60, 2
N_CHAR = 256 - THRESH + F
T = N_CHAR * 2 - 1
R = T - 1
MAX_FREQ = 0x8000


def crc16(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0xA097) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


# ─── TD0 bauen ───────────────────────────────────────────────────────────────

def kopf(sig=b"TD", rate=0, fm=False, laufwerk=2, schritt=0, kommentar=False, dos=0, seiten=2, version=0x15):
    h = bytearray(sig) + bytes([0, 0x4B, version, rate | (0x80 if fm else 0), laufwerk,
                                schritt | (0x80 if kommentar else 0), dos, seiten])
    h += crc16(bytes(h)).to_bytes(2, "little")
    return bytes(h)


def kommentar_block(text: bytes, stamp=(107, 9, 11, 2, 42, 51)):
    koerper = len(text).to_bytes(2, "little") + bytes(stamp) + text
    return crc16(koerper).to_bytes(2, "little") + koerper


def spur(cyl, head, sektoren, fm=False):
    """sektoren: Liste von (c, h, r, n, flags, nutzdaten_oder_None, methode); nutzdaten = entpackte Daten."""
    k = bytes([len(sektoren), cyl, head | (0x80 if fm else 0)])
    out = bytearray(k + bytes([crc16(k) & 0xFF]))
    for c, h, r, n, flags, daten, methode in sektoren:
        if daten is None:
            out += bytes([c, h, r, n, flags, 0])
            continue
        out += bytes([c, h, r, n, flags, crc16(daten) & 0xFF])
        if methode == 0:
            nutz = daten
        elif methode == 1:
            muster = daten[:2]
            assert muster * (len(daten) // 2) == daten
            nutz = (len(daten) // 2).to_bytes(2, "little") + muster
        else:   # 2: Wiederholung c=1 aus Paaren, Rest als Literal
            nutz = bytearray()
            i = 0
            while i < len(daten):
                m = daten[i:i + 2]
                z = 1
                while daten[i + 2 * z:i + 2 * z + 2] == m and z < 255 and len(m) == 2:
                    z += 1
                if z >= 3 and len(m) == 2:
                    nutz += bytes([1, z]) + m
                    i += 2 * z
                else:
                    j = i + 1
                    nutz += bytes([0, 1]) + daten[i:j]
                    i = j
            nutz = bytes(nutz)
        out += (len(nutz) + 1).to_bytes(2, "little") + bytes([methode]) + nutz
    return bytes(out)


def ende():
    return bytes([0xFF, 0x00, 0x00, 0x00])


# ─── LZHUF packen (Okumura) ──────────────────────────────────────────────────

class Baum:
    def __init__(self):
        self.freq = [0] * (T + 1)
        self.prnt = [0] * (T + N_CHAR)
        self.son = [0] * T
        for i in range(N_CHAR):
            self.freq[i] = 1
            self.son[i] = i + T
            self.prnt[i + T] = i
        i, j = 0, N_CHAR
        while j <= R:
            self.freq[j] = self.freq[i] + self.freq[i + 1]
            self.son[j] = i
            self.prnt[i] = self.prnt[i + 1] = j
            i += 2
            j += 1
        self.freq[T] = 0xFFFF
        self.prnt[R] = 0

    def neuaufbau(self):
        f, son = self.freq, self.son
        j = 0
        for i in range(T):
            if son[i] >= T:
                f[j] = (f[i] + 1) // 2
                son[j] = son[i]
                j += 1
        i, j = 0, N_CHAR
        while j < T:
            k = i + 1
            fs = f[j] = f[i] + f[k]
            k = j - 1
            while fs < f[k]:
                k -= 1
            k += 1
            l = j - k
            f[k + 1:k + 1 + l] = f[k:k + l]
            f[k] = fs
            son[k + 1:k + 1 + l] = son[k:k + l]
            son[k] = i
            i += 2
            j += 1
        for i in range(T):
            k = son[i]
            if k >= T:
                self.prnt[k] = i
            else:
                self.prnt[k] = self.prnt[k + 1] = i

    def update(self, c):
        f, son, prnt = self.freq, self.son, self.prnt
        if f[R] == MAX_FREQ:
            self.neuaufbau()
        c = prnt[c + T]
        while True:
            f[c] += 1
            k = f[c]
            l = c + 1
            if k > f[l]:
                l += 1
                while k > f[l]:
                    l += 1
                l -= 1
                f[c] = f[l]
                f[l] = k
                i = son[c]
                prnt[i] = l
                if i < T:
                    prnt[i + 1] = l
                j = son[l]
                son[l] = i
                prnt[j] = c
                if j < T:
                    prnt[j + 1] = c
                son[c] = j
                c = l
            c = prnt[c]
            if c == 0:
                break


def _pos_codes():
    stufen = [(0, 0, 3), (1, 3, 4), (4, 11, 5), (12, 23, 6), (24, 47, 7), (48, 63, 8)]
    tab, code = {}, 0
    for ab, bis, ln in stufen:
        for v in range(ab, bis + 1):
            tab[v] = (code, ln)
            code += 1
        code <<= 1   # naechste Laenge: ein Bit mehr
    return tab


POS = _pos_codes()


def packe(daten: bytes) -> bytes:
    baum = Baum()
    bits = []

    def schreibe(wert, n):
        for i in range(n - 1, -1, -1):
            bits.append((wert >> i) & 1)

    ring = bytearray(b" " * N)
    r = N - F
    pos = 0
    while pos < len(daten):
        beste_len, beste_d = 0, 0
        for d in range(N):
            s = (r - d - 1) % N
            tmp = {}
            ln = 0
            while ln < F and pos + ln < len(daten):
                idx = (s + ln) % N
                b = tmp.get(idx, ring[idx])
                if b != daten[pos + ln]:
                    break
                tmp[(r + ln) % N] = b
                ln += 1
            if ln > beste_len:
                beste_len, beste_d = ln, d
        if beste_len > THRESH:
            sym = 255 - THRESH + beste_len
            _kodiere(baum, bits, sym)
            code, ln = POS[beste_d >> 6]
            schreibe(code, ln)
            schreibe(beste_d & 0x3F, 6)
            for k in range(beste_len):
                ring[r] = daten[pos + k]
                r = (r + 1) % N
            pos += beste_len
        else:
            _kodiere(baum, bits, daten[pos])
            ring[r] = daten[pos]
            r = (r + 1) % N
            pos += 1
    while len(bits) % 8:
        bits.append(0)
    return bytes(int("".join(map(str, bits[i:i + 8])), 2) for i in range(0, len(bits), 8))


def _kodiere(baum, bits, c):
    # prnt[c + T] ist der PLATZ des Blatts im Baum; ungerader Platz = rechtes Kind
    pfad = []
    k = baum.prnt[c + T]
    while True:
        pfad.append(k & 1)
        k = baum.prnt[k]
        if k == R:
            break
    bits.extend(reversed(pfad))
    baum.update(c)


def td_packen(plain: bytes) -> bytes:
    """Aus einer ``TD``-Datei eine ``td``-Datei machen (Kopf neu mit CRC, Rest gepackt)."""
    h = bytearray(plain[:10])
    h[0:2] = b"td"
    h += crc16(bytes(h)).to_bytes(2, "little")
    # 4 Nullbytes Nachlauf: das Stromende ist nicht markiert, und auch der C-Entpacker von gw
    # liefert das letzte Symbol erst, wenn noch Bits folgen (echte Dateien tragen ebenfalls Padding)
    return bytes(h) + packe(plain[12:] + bytes(4))


def beispiel():
    """Der Vektor des Kern-Tests: Kommentar, 2 Spuren (MFM + FM), Methoden 0/1/2."""
    text0 = (b"TeleDisk Testvektor 0123456789 " * 5)[:128]
    gemischt = b"\x00" * 40 + bytes(range(1, 9)) + b"\xAB\xCD" * 20 + b"\xE5" * 40
    gemischt = gemischt[:128]
    spur0 = spur(0, 0, [
        (0, 0, 1, 0, 0, text0, 0),
        (0, 0, 2, 0, 0, b"\xE5\xE5" * 64, 1),
        (0, 0, 3, 0, 0x02, gemischt, 2),
    ])
    spur1 = spur(1, 0, [(1, 0, 1, 1, 0x04, b"\x00\x11" * 128, 1)], fm=True)
    return (kopf(kommentar=True, rate=0) + kommentar_block(b"Testvektor\x00zweite Zeile\x00\x00")
            + spur0 + spur1 + ende())


if __name__ == "__main__":
    plain = beispiel()
    gepackt = td_packen(plain)
    try:
        from greaseweazle import optimised
        assert optimised.td0_unpack(gepackt[12:])[:len(plain) - 12] == plain[12:], "Gegenprobe gw fehlgeschlagen"
        print("// Gegenprobe gegen greaseweazle.optimised.td0_unpack: ok", file=sys.stderr)
    except ImportError:
        print("// greaseweazle nicht vorhanden: keine Gegenprobe", file=sys.stderr)
    for name, d in (("plain TD", plain), ("gepackt td", gepackt)):
        print("// %s: %d Byte" % (name, len(d)))
        print(",".join("0x%02X" % b for b in d))

#!/usr/bin/env python3
"""SYLp bereinigen: 40-Spur-Arbeitsabbild aus dem 80x2-Auslesen (S4, doc/design/43_sios1526.md).

SIOS1526_SYLp.hfe ist eine 40-Spur-Diskette, die im 80er Laufwerk zweiseitig ausgelesen wurde:
Inhalt nur auf Seite 0, Zylinder 0..10; ab Zylinder 40 Rauschen/unformatiert, Seite 1 leer bis auf
den Streusektor c15h1 (CRC-Fehler).  Hier werden Seite 0, Zylinder 0..39, UNVERAENDERT (gleiche
Zellenstroeme, also gleiche Umdrehungszeit) in eine einseitige 40-Zylinder-HFE v1 uebernommen.
Der Kopf stammt von einer einseitigen 40-Spur-Diskette (Vorlage sios1526_dp1.hfe).

  python3 tools/sios_a5120/sylp_bereinigen.py ORIGINAL.hfe VORLAGE_40SS.hfe AUS.hfe
"""
import struct
import sys

ZYL = 40


def spuren(d):
    off, = struct.unpack_from('<H', d, 18)
    for t in range(ZYL):
        o, n = struct.unpack_from('<HH', d, off * 512 + 4 * t)
        yield d[o * 512:o * 512 + n]


def seite0(roh):
    """HFE v1: je 512-Byte-Block 256 Byte Seite 0, dann 256 Byte Seite 1."""
    return b''.join(roh[i:i + 256] for i in range(0, len(roh), 512))


def main(orig, vorlage, aus):
    o = open(orig, 'rb').read()
    v = open(vorlage, 'rb').read()
    assert o[:8] == v[:8] == b'HXCPICFE'
    kopf = bytearray(v[:512])
    struct.pack_into('<BB', kopf, 9, ZYL, 1)       # Spuren, Seiten
    daten = []
    for roh in spuren(o):
        s = seite0(roh)
        # einseitig: Seite 0 in 256er-Bloecken, Seite 1 mit Nullen (Form der Vorlage)
        blk = b''.join(s[i:i + 256].ljust(256, b'\0') + b'\0' * 256 for i in range(0, len(s), 256))
        daten.append(blk)
    lut = bytearray(512)
    pos = 2
    for t, blk in enumerate(daten):
        struct.pack_into('<HH', lut, 4 * t, pos, len(blk))
        pos += (len(blk) + 511) // 512
    out = bytes(kopf) + bytes(lut)
    for blk in daten:
        out += blk.ljust((len(blk) + 511) // 512 * 512, b'\0')
    open(aus, 'wb').write(out)


if __name__ == '__main__':
    main(*sys.argv[1:4])

#!/usr/bin/env python3
"""SYLp bereinigen: 40-Spur-Arbeitsabbild aus dem 80x2-Auslesen (S4, doc/design/43_sios1526.md).

SIOS1526_SYLp.hfe ist eine 40-Spur-Diskette, die im 80er Laufwerk zweiseitig ausgelesen wurde:
Inhalt nur auf Seite 0, Zylinder 0..10; ab Zylinder 40 Rauschen/unformatiert, Seite 1 leer bis auf
den Streusektor c15h1 (CRC-Fehler).  Hier werden Seite 0, Zylinder 0..39, UNVERAENDERT (gleiche
Zellenstroeme, also gleiche Umdrehungszeit) in eine einseitige 40-Zylinder-HFE v1 uebernommen.
Der Kopf stammt von einer einseitigen 40-Spur-Diskette (Vorlage sios1526_dp1.hfe).

  python3 tools/sios_a5120/sylp_bereinigen.py ORIGINAL.hfe VORLAGE_40SS.hfe AUS.hfe

main80(): dasselbe als einseitige 80-Zylinder-HFE fuer K5601/K5600.20 (SIOS 0.9 schrittet einfach):
Zylinder 0..39 wie oben, Zylinder 40..79 sind die leer formatierten Spuren der Diskette SYLs
(26 x 128 B MFM, gleiche Form, IDs passen).  Damit passt das Abbild zum Katalogformat
k5601_ss80_26x128 und das DiskTool erkennt es; die SIOS-Spuren bleiben bitgleich.
"""
import struct
import sys

ZYL = 40


def spur(d, t):
    off, = struct.unpack_from('<H', d, 18)
    o, n = struct.unpack_from('<HH', d, off * 512 + 4 * t)
    return d[o * 512:o * 512 + n]


def spuren(d):
    for t in range(ZYL):
        yield spur(d, t)


def seite0(roh):
    """HFE v1: je 512-Byte-Block 256 Byte Seite 0, dann 256 Byte Seite 1."""
    return b''.join(roh[i:i + 256] for i in range(0, len(roh), 512))


def einseitig(roh):
    """Seite 0 in 256er-Bloecken, Seite 1 mit Nullen (Form der einseitigen Vorlagen)."""
    s = seite0(roh)
    return b''.join(s[i:i + 256].ljust(256, b'\0') + b'\0' * 256 for i in range(0, len(s), 256))


def schreibe(kopf, daten, aus):
    struct.pack_into('<BB', kopf, 9, len(daten), 1)       # Spuren, Seiten
    lut = bytearray(512)
    pos = 2
    for t, blk in enumerate(daten):
        struct.pack_into('<HH', lut, 4 * t, pos, len(blk))
        pos += (len(blk) + 511) // 512
    out = bytes(kopf) + bytes(lut)
    for blk in daten:
        out += blk.ljust((len(blk) + 511) // 512 * 512, b'\0')
    open(aus, 'wb').write(out)


def main(orig, vorlage, aus):
    o = open(orig, 'rb').read()
    v = open(vorlage, 'rb').read()
    assert o[:8] == v[:8] == b'HXCPICFE'
    schreibe(bytearray(v[:512]), [einseitig(roh) for roh in spuren(o)], aus)


def main80(orig, leer80, aus):
    o = open(orig, 'rb').read()
    v = open(leer80, 'rb').read()
    assert o[:8] == v[:8] == b'HXCPICFE'
    daten = [einseitig(roh) for roh in spuren(o)]
    daten += [spur(v, t) for t in range(ZYL, 2 * ZYL)]
    schreibe(bytearray(v[:512]), daten, aus)


if __name__ == '__main__':
    main(*sys.argv[1:4])

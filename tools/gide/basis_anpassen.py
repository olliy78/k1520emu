#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
basis_anpassen.py  -  E/A-Basisadresse der Fremdtreiber fuer die Karten GIDE, USB und Netz umstellen
===================================================================================================

Die Gast-Software der Zusatzkarten (tools/gide/README.md) hat die Portadresse fest einkompiliert.
Die Karten selbst haben eine frei einstellbare Basis (Jumper); dieses Werkzeug schreibt die
Port-Operanden der .COM-Dateien um, damit die Software zur gesteckten Karte passt.

  usb  BASIS   UTools (USB, UDIR, UGET, UPUT)      Original DCh..DFh; BASIS = Vielfaches von 4
  net  BASIS   KCNet (CPMN1587, TFTP1287)          Original C0h..C3h; BASIS = Vielfaches von 4
  gidn BASIS   Kingstener-Treiber (GIDN)           Original 50h;      BASIS = Vielfaches von 10h

  python3 tools/gide/basis_anpassen.py usb 8C --out ordner
  python3 tools/gide/basis_anpassen.py net 28 --out ordner
  python3 tools/gide/basis_anpassen.py gidn 90 --out ordner

Poppe-Treiber (GIDEHPxx.COM) braucht keine Umstellung: fuer jede Basis 00h..F0h liegt eine eigene
Fassung bei (tools/gide/fremd/poppe/).

Die Stellen sind aus den Binaerdateien belegt, nicht geraten:
  * gidn: die Fassungen @50h/@D0h/@F0h des Autors unterscheiden sich in genau diesen 16 Operanden
    (Hochnibble); mit GIDE EQU 90h uebersetzt entsteht byteweise dasselbe wie das Umschreiben hier.
  * net: Treiberblock 0109..028E, in CPMN1587 und TFTP1287 byteidentisch (doc/design/50a_kcnet_protokoll.md §0);
    ausser den Operanden unten kommt in der ganzen Datei kein Zugriff auf C0h..C3h vor (disassembliert).
  * usb: die E/A-Zugriffe stehen im gemeinsamen Teil (USBINC.Z80), in allen vier Programmen an denselben
    16 Stellen.
Eine Datei, die an diesen Stellen nicht das erwartete Original traegt, wird abgelehnt.
"""
import argparse
import os
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
FREMD = os.path.join(HIER, 'fremd')

# Stellen = Dateiversatz des OPERANDEN; Zahl = Abstand zur Originalbasis
USB_STELLEN = {137: 3, 141: 3, 145: 3, 149: 1, 153: 2, 157: 2, 159: 0, 300: 1, 320: 1, 363: 1, 365: 0, 370: 1,
               376: 0, 379: 1, 387: 1, 391: 1}
USB_DATEIEN = ('usb.com', 'udir.com', 'uget.com', 'uput.com')
USB_ORIGINAL = 0xDC
# Opcode vor dem Operanden: IN A,(n) = DBh, OUT (n),A = D3h
USB_OPCODES = {137: 0xD3, 141: 0xD3, 145: 0xD3, 149: 0xD3, 153: 0xD3, 157: 0xD3, 159: 0xDB, 300: 0xDB, 320: 0xDB,
               363: 0xD3, 365: 0xDB, 370: 0xD3, 376: 0xD3, 379: 0xDB, 387: 0xD3, 391: 0xD3}

# Ladeadresse 0100H -> Dateiversatz = Adresse - 100H.  Operandadressen: 10B 11A 11C 14D 14F 155 15A 1F0 1F7 202
NET_STELLEN = {0x10B - 0x100 + 0: 2, 0x11A - 0x100: 0, 0x11C - 0x100: 1, 0x14D - 0x100: 0, 0x14F - 0x100: 1,
               0x155 - 0x100: 1, 0x15A - 0x100: 0, 0x1F0 - 0x100: 0, 0x1F7 - 0x100: 1, 0x202 - 0x100: 1}
NET_OPCODES = {0x10B - 0x100: 0x0E, 0x11A - 0x100: 0xDB, 0x11C - 0x100: 0xDB, 0x14D - 0x100: 0xD3,
               0x14F - 0x100: 0xDB, 0x155 - 0x100: 0xDB, 0x15A - 0x100: 0xDB, 0x1F0 - 0x100: 0x0E,
               0x1F7 - 0x100: 0xDB, 0x202 - 0x100: 0xDB}
NET_DATEIEN = ('CPMN1587.COM', 'TFTP1287.COM')
NET_ORIGINAL = 0xC0

# Kingstener GIDN: Operanden (Hochnibble = Basis), Originalfassung @50h
GIDN_STELLEN = (719, 723, 727, 1248, 1255, 1270, 1283, 1293, 1308, 1316, 1322, 1368, 1373, 1380, 1385, 1390)
GIDN_ORIGINAL = 0x50


class Fehler(Exception):
    pass


def _basis(text):
    try:
        return int(text, 16)
    except ValueError:
        raise Fehler(f'Basis "{text}" ist keine Hexzahl')


def _umschreiben(daten, stellen, opcodes, original, basis, modulo, name):
    if basis % modulo or not 0 <= basis <= 0xFF:
        raise Fehler(f'Basis {basis:02X}h muss ein Vielfaches von {modulo:X}h unter 100h sein')
    out = bytearray(daten)
    for off, abstand in stellen.items():
        if off >= len(out) or out[off - 1] != opcodes[off] or out[off] != original + abstand:
            raise Fehler(f'{name}: an Versatz {off:#x} steht nicht das erwartete Original '
                         f'(Opcode {opcodes[off]:02X}h, Operand {original + abstand:02X}h) - falsche Datei?')
        neu = basis + abstand
        if neu > 0xFF:
            raise Fehler(f'{name}: Basis {basis:02X}h laeuft ueber den E/A-Raum')
        out[off] = neu
    return bytes(out)


def usb(daten, basis, name='USB'):
    """UTools-Programm @p daten (Original DCh) auf @p basis umschreiben."""
    return _umschreiben(daten, USB_STELLEN, USB_OPCODES, USB_ORIGINAL, basis, 4, name)


def net(daten, basis, name='KCNet'):
    """CPMNET/TFTP (Original C0h) auf @p basis umschreiben."""
    return _umschreiben(daten, NET_STELLEN, NET_OPCODES, NET_ORIGINAL, basis, 4, name)


def gidn(daten, basis, name='GIDN'):
    """Kingstener-Treiber (Original @50h) auf @p basis (Hochnibble) umschreiben."""
    if basis % 0x10 or not 0 <= basis <= 0xF0:
        raise Fehler(f'Basis {basis:02X}h muss ein Vielfaches von 10h sein')
    out = bytearray(daten)
    for off in GIDN_STELLEN:
        if off >= len(out) or (out[off] & 0xF0) != GIDN_ORIGINAL:
            raise Fehler(f'{name}: an Versatz {off:#x} steht nicht das erwartete Original (Hochnibble 5) - falsche Datei?')
        out[off] = basis | (out[off] & 0x0F)
    return bytes(out)


def _lies(*teile):
    with open(os.path.join(*teile), 'rb') as f:
        return f.read()


def usb_satz(basis):
    """{DateiName: Bytes} der vier UTools fuer @p basis (Namen GROSS, wie auf der Diskette)."""
    return {n.upper(): usb(_lies(FREMD, 'utools', n), basis, n) for n in USB_DATEIEN}


def net_satz(basis):
    """{DateiName: Bytes} von CPMN1587/TFTP1287 fuer @p basis."""
    return {n: net(_lies(FREMD, 'kcnet', n), basis, n) for n in NET_DATEIEN}


def gidn_datei(basis):
    """Bytes des Kingstener-Treibers fuer @p basis."""
    return gidn(_lies(FREMD, 'kingstener', 'gidn_50h.com'), basis)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('art', choices=('usb', 'net', 'gidn'))
    ap.add_argument('basis', help='neue Basis in Hex (z. B. 8C)')
    ap.add_argument('--out', required=True, help='Zielordner (wird angelegt)')
    ap.add_argument('--von', help='Ordner mit den Originalen (Vorgabe: tools/gide/fremd/<art>)')
    a = ap.parse_args(argv)
    try:
        basis = _basis(a.basis)
        if a.art == 'gidn':
            dateien = {f'GIDN{basis:02X}.COM': gidn_datei(basis)}
        else:
            if a.von:
                quelle = (usb if a.art == 'usb' else net)
                namen = USB_DATEIEN if a.art == 'usb' else NET_DATEIEN
                dateien = {n.upper(): quelle(_lies(a.von, n), basis, n) for n in namen}
            else:
                dateien = usb_satz(basis) if a.art == 'usb' else net_satz(basis)
    except (Fehler, OSError) as e:
        print(f'FEHLER: {e}', file=sys.stderr)
        return 1
    os.makedirs(a.out, exist_ok=True)
    for n, d in dateien.items():
        with open(os.path.join(a.out, n), 'wb') as f:
            f.write(d)
        print(f'{os.path.join(a.out, n)}  ({len(d)} Bytes)')
    return 0


if __name__ == '__main__':
    sys.exit(main())

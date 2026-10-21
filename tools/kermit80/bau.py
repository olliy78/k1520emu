#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
bau.py - Kermit-80 4.11 fuer die K1520-Rechner bauen (Ueberlagerung mit E/A-Anschluss)
======================================================================================

Kermit-80 (Columbia University, 1991) besteht aus einem rechnerunabhaengigen Teil
(CPSKER.HEX, ab 0100H) und einer rechnerabhaengigen Ueberlagerung ab 7000H (CPXTYP.ASM
und Includes).  Die fertigen Ueberlagerungen (CPV*.HEX) kennen keinen unserer Rechner; die
GENERISCHE (CPVGEN) geht ueber IOBYTE/BIOS-RDR:/PUN:, die CP/A, SCPX usw. so nicht
bereitstellen.  Dieses Skript uebersetzt deshalb die Ueberlagerung NEU, mit der
Systemklasse "inout" (Z80-SIO ueber IN/OUT, wie fuer andere Rechner im Quelltext vorgesehen)
und der Anschlusswahl des Zielrechners; eine Anfangsfolge programmiert Zeitgeber und SIO
auf 9600 8N1.

  bau.py <rechner> [--out DATEI]   rechner: a5120 | k8915 | pc1715 | prg710
  bau.py --selbsttest              Werkzeugkette pruefen: die Uebersetzung der GENERISCHEN
                                   Ueberlagerung muss CPVGEN.HEX bytegleich nachbilden
Voraussetzung: tools/kermit80/holen.sh gelaufen (quelle/), CP/M-Werkzeugkette wie
tools/cpm_bau.py (CPA_TOOLS=<pfad> mit cparun und m80.com).

Warum nicht LINKMT/L80 fuer die Ueberlagerung: CPXTYP.ASM setzt ASEG/ORG 7000H; LINKMT
meldet dafuer "Incompatible relocatable file".  Die Bytes werden deshalb aus dem
M80-Listing (.PRN) gelesen; der Selbsttest belegt, dass das bytegleich zum Original ist.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

HIER = os.path.dirname(os.path.abspath(__file__))
QUELLE = os.environ.get('KERMIT80_QUELLE', os.path.join(HIER, 'quelle'))
CPA_TOOLS = os.environ.get('CPA_TOOLS', os.path.expanduser('~/projects/CPA_Workbench/tools'))
OVLADR = 0x7000

# Rechner: (Datenport, Steuerport, Anfangsfolge als [(Port, Wert)], Name).
# Quelle der Werte: tools/sertest/src/sertest.mac (Tabellen IF_A/IF_K/IF_P, SIOPRG/SP_TAB).
# CTC: Steuerwort (Zeitgeber, Vorteiler 16), danach Zeitkonstante 1 -> 16 x 9600 Bd bei 2,4576 MHz.
# SIO: Kanalreset, WR4 44H (x16, 1 Stopp, ohne Paritaet), WR1 0 (keine Interrupts),
#      WR3 C1H (8 Bit, Empfaenger ein), WR5 EAH (8 Bit, Sender ein, DTR+RTS).
def _sio(ctl):
    return [(ctl, 0x18), (ctl, 4), (ctl, 0x44), (ctl, 1), (ctl, 0), (ctl, 3), (ctl, 0xC1),
            (ctl, 5), (ctl, 0xEA)]

RECHNER = {
    # A5120, ASS K8025 A33 Kanal A (DFUE/V.24), Takt ZRE-CTC K0 (0CH), CP/A-Wert 17H
    'a5120': (0x50, 0x51, [(0x0C, 0x17), (0x0C, 1)] + _sio(0x51), 'A5120 (K8025 A33-A)'),
    # K8915, K7028 SIO1 Kanal A (V.24 X4), CTC1 K0 (48H) 07H
    'k8915': (0x40, 0x41, [(0x48, 0x07), (0x48, 1)] + _sio(0x41), 'K8915 (K7028 SIO1-A)'),
    # PC 1715, ZRE SIO0 Kanal B (V.24 X5), CTC0 K1 (09H) 17H
    'pc1715': (0x0D, 0x0F, [(0x09, 0x17), (0x09, 1)] + _sio(0x0F), 'PC 1715 (SIO0-B)'),
    # PRG 710 / 710-1, K8025 A33 Kanal A, Takt CTC A34 K2 (5AH)
    'prg710': (0x50, 0x51, [(0x5A, 0x07), (0x5A, 1)] + _sio(0x51), 'PRG 710 (K8025 A33-A)'),
}


def _crlf(t):
    return t.replace('\r\n', '\n').replace('\r', '\n').replace('\x1a', '')


def _ersetze(text, alt, neu, name):
    if text.count(alt) != 1:
        raise RuntimeError(f"Patchstelle in {name} nicht eindeutig ({text.count(alt)}x): {alt!r}")
    return text.replace(alt, neu)


def _lies(name):
    pfad = os.path.join(QUELLE, name.lower())
    if not os.path.isfile(pfad):
        raise RuntimeError(f"{pfad} fehlt - zuerst tools/kermit80/holen.sh ausfuehren")
    with open(pfad, 'rb') as f:
        return _crlf(f.read().decode('latin1'))


def quelltext(rechner):
    """Die zusammengefuegte Ueberlagerungsquelle (INCLUDEs aufgeloest, END nur am Schluss).

    rechner None = GENERISCHE Ueberlagerung (Selbsttest), sonst ein Schluessel aus RECHNER."""
    dateien = {n: _lies(n) for n in ('CPSDEF.ASM', 'CPXLNK.ASM', 'CPXCOM.ASM', 'CPXSWT.ASM',
                                     'CPXSYS.ASM', 'CPXSY2.ASM', 'CPXTYP.ASM', 'CPXVDU.ASM')}
    typ = dateien['CPXTYP.ASM']
    typ = _ersetze(typ, 'm80\tEQU\tFALSE', 'm80\tEQU\tTRUE', 'CPXTYP')
    typ = _ersetze(typ, 'lasm\tEQU\tTRUE', 'lasm\tEQU\tFALSE', 'CPXTYP')
    typ = _ersetze(typ, 'crt\tEQU\tFALSE', 'crt\tEQU\tTRUE', 'CPXTYP')
    if rechner is None:
        typ = _ersetze(typ, 'gener\tEQU\tFALSE', 'gener\tEQU\tTRUE', 'CPXTYP')
        typ = _ersetze(typ, 'cpsker\tEQU\tFALSE', 'cpsker\tEQU\tFALSE\nk1520\tEQU\tFALSE', 'CPXTYP')
    else:
        p = RECHNER[rechner]
        typ = _ersetze(typ, 'cpsker\tEQU\tFALSE', 'cpsker\tEQU\tFALSE\nk1520\tEQU\tTRUE', 'CPXTYP')
        # Systemklasse "inout" (Platz in der Aufzaehlung der IN/OUT-Systeme)
        typ = _ersetze(typ, 'IF genie OR trsm4 OR ampro\ninout\tSET\tTRUE',
                       'IF genie OR trsm4 OR ampro OR k1520\ninout\tSET\tTRUE', 'CPXTYP')
        # Der Gesamtmerker "sysfam" wird im Quelltext aus der Familienliste gebildet; die
        # Anschlusswerte stehen in CPXSYS (unten).
    sys1 = dateien['CPXSYS.ASM']
    sys2 = dateien['CPXSY2.ASM']
    if rechner is not None:
        dp, cp, folge, name = RECHNER[rechner]
        init = ''.join(f"\tmvi\ta,0{w:02X}H\n\tout\t0{pt:02X}H\n" for pt, w in folge)
        sys1 = _ersetze(sys1, 'sysxin:\n',
                        'sysxin:\nIF k1520\n' + init + 'ENDIF ;k1520\n', 'CPXSYS')
        sys1 = _ersetze(sys1, '\n;\tsysxin\t- system dependent initialisation code',
                        f'\nIF k1520\nmnport\tEQU\t0{dp:02X}H\nmnprts\tEQU\t0{cp:02X}H\n'
                        'output\tEQU\t04H\t;Sendepuffer leer (SIO RR0 D2)\n'
                        'input\tEQU\t01H\t;Zeichen empfangen (SIO RR0 D0)\nENDIF ;k1520\n'
                        '\n;\tsysxin\t- system dependent initialisation code', 'CPXSYS')
        sys2 = _ersetze(sys2, 'sysspd:\n', 'sysspd:\nIF k1520\n\tret\t\t;feste 9600 Bd (Zeitgeber der Anfangsfolge)\nENDIF\n',
                        'CPXSY2')
        sys2 = _ersetze(sys2, 'IF access ;Similar to bbI with different values [29]',
                        'IF k1520\nspdtbl:\tdb\t1\n\tdb\t04h,\'9600$\',\t1,1\n'
                        'sphtbl:\tdb\tcr,lf,\'  9600$\'\nENDIF ;k1520\n'
                        'IF access ;Similar to bbI with different values [29]', 'CPXSY2')
        sys2 = _ersetze(sys2, 'IF access\nsysver:',
                        f"IF k1520\nsysver:\tdb\t'{name}$'\nENDIF\nIF access\nsysver:", 'CPXSY2')
    dateien['CPXTYP.ASM'] = typ
    dateien['CPXSYS.ASM'] = sys1
    dateien['CPXSY2.ASM'] = sys2

    def flach(n, oben=False):
        aus = []
        for z in dateien.get(n, '').split('\n') if n in dateien else []:
            m = re.match(r'^\s*INCLUDE\s+(\S+)', z, re.I)
            if m:
                aus.append(flach(m.group(1).upper()))
            elif not oben and re.match(r'^\s*END\s*(;.*)?$', z, re.I):
                aus.append('; ' + z)       # END nur am Ende der Gesamtquelle
            else:
                aus.append(z)
        return '\n'.join(aus)
    text = flach('CPXTYP.ASM', True).split('\n')
    # "ovlend" steht in CPXSYS/CPXSY2/CPXVDU je am Ende; M80 verlangt EINE Definition (die letzte)
    stellen = [i for i, z in enumerate(text) if z.lower().startswith('ovlend')]
    for i in stellen[:-1]:
        text[i] = '; ' + text[i]
    return '\r\n'.join(text)


def prn_bytes(prn):
    """Bytes aus dem M80-Listing: Adresse in Spalte 3-6, Daten ab Spalte 9."""
    mem = {}
    for z in prn.split('\n'):
        z = z.rstrip('\r')
        m = re.match(r'^ {2}([0-9A-F]{4})[ \'"]{1,4}[0-9A-F]{2}', z)
        if not m:
            continue
        a = int(m.group(1), 16)
        bs = []
        for t in re.findall(r'[0-9A-F]{2,4}', z[8:31]):
            if len(t) == 4:
                bs += [int(t[2:], 16), int(t[:2], 16)]     # DW: niederwertiges Byte zuerst
            elif len(t) == 2:
                bs.append(int(t, 16))
        for i, b in enumerate(bs):
            mem[a + i] = b
    return mem


def hex_bytes(pfad):
    mem = {}
    with open(pfad) as f:
        for z in f:
            z = z.strip()
            if z.startswith(':') and int(z[7:9], 16) == 0:
                n, a = int(z[1:3], 16), int(z[3:7], 16)
                for i in range(n):
                    mem[a + i] = int(z[9 + 2 * i:11 + 2 * i], 16)
    return mem


def uebersetze(rechner):
    """Ueberlagerungsbytes {Adresse: Byte} (ab 7000H)."""
    if not os.path.isfile(os.path.join(CPA_TOOLS, 'm80.com')):
        raise RuntimeError(f"m80.com nicht in {CPA_TOOLS} (CPA_TOOLS=<pfad> setzen)")
    cparun = os.path.join(CPA_TOOLS, 'cparun')
    with tempfile.TemporaryDirectory(prefix='kermit80_') as tmp:
        with open(os.path.join(tmp, 'OVL.MAC'), 'wb') as f:
            f.write(quelltext(rechner).encode('latin1', errors='replace'))
        shutil.copy(os.path.join(CPA_TOOLS, 'm80.com'), tmp)
        r = subprocess.run([cparun, 'm80', 'OVL,OVL=OVL'], cwd=tmp, capture_output=True,
                           text=True, errors='replace', timeout=120, stdin=subprocess.DEVNULL)
        if 'No Fatal error' not in r.stdout:
            raise RuntimeError('M80 meldet Fehler:\n' + r.stdout[-2000:])
        with open(os.path.join(tmp, 'ovl.prn'), 'rb') as f:
            return {a: b for a, b in prn_bytes(f.read().decode('latin1')).items() if a >= OVLADR}


def zusammenfuegen(ueberlagerung):
    """KERMIT.COM = CPSKER.HEX (ab 0100H) + Ueberlagerung (ab 7000H), Luecken 00."""
    kern = hex_bytes(os.path.join(QUELLE, 'cpsker.hex'))
    ende = max(max(kern), max(ueberlagerung))
    # CPSKER.HEX reicht bis 707AH (Platzhalter der Ueberlagerung, Sprungtafel mit JMP 0) - die
    # Ueberlagerung ueberschreibt ihn.
    if min(kern) != 0x100:
        raise RuntimeError('CPSKER.HEX beginnt nicht bei 0100H')
    if min(ueberlagerung) != OVLADR:
        raise RuntimeError('Ueberlagerung beginnt nicht bei 7000H')
    img = bytearray(ende + 1 - 0x100)
    for a, b in kern.items():
        img[a - 0x100] = b
    for a, b in ueberlagerung.items():
        img[a - 0x100] = b
    # auf 128-Byte-Records auffuellen (CP/M kennt keine Bytelaenge)
    img += bytes((-len(img)) % 128)
    return bytes(img)


def selbsttest():
    ov = uebersetze(None)
    ref = {a: b for a, b in hex_bytes(os.path.join(QUELLE, 'cpvgen.hex')).items() if a >= OVLADR}
    if ov != ref:
        diff = [hex(a) for a in sorted(set(ov) | set(ref)) if ov.get(a) != ref.get(a)]
        raise RuntimeError(f"Uebersetzung weicht von CPVGEN.HEX ab ({len(diff)} Bytes, z. B. {diff[:8]})")
    print(f"OK: generische Ueberlagerung bytegleich zu CPVGEN.HEX ({len(ref)} Bytes)")


def main(argv):
    try:
        if argv[:1] == ['--selbsttest']:
            selbsttest()
            return 0
        if not argv or argv[0] not in RECHNER:
            print(__doc__)
            return 2
        out = argv[argv.index('--out') + 1] if '--out' in argv else f'KERMIT_{argv[0].upper()}.COM'
        com = zusammenfuegen(uebersetze(argv[0]))
        with open(out, 'wb') as f:
            f.write(com)
        print(f"{out}: {len(com)} Bytes ({argv[0]})")
        return 0
    except RuntimeError as e:
        print(f"FEHLER: {e}", file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
ablauf1715.py  -  MicroDOS 1715: Leerdiskette formatieren -> MSYSG15A/B -> bootfaehige Systemdiskette
===================================================================================================

Treibt den PC-1715-Emulator (Python-Bindung, libk1520core) so, wie ein Anwender es an der Tastatur taete
(Merkposten: doc/merkposten/microdos1715.md; Gegenstueck zu tools/microdos/ablauf.py fuer den A5120):

  1. Systemdiskette (Original, Temp-Kopie) als A:, eine ECHTE LEERDISKETTE (unformatiert) als B:.
  2. FORMAT15: Laufwerk, Format, alle Spuren.
  3. MSYSG15A (MFS 1.6 = K5601 u. a.) bzw. MSYSG15B (MFS 1.2 = K5600.00): Ziel B: -> MICRODOS.SYS.
  4. Probe: die neue Diskette als A: kalt starten, "A>" abwarten, "D" muss MICRODOS.SYS zeigen.

Aufruf:
  python3 -I tools/microdos/ablauf1715.py ZIELVERZEICHNIS [--quelle ABBILD.hfe] [--ohne-probe] [--still]

Voraussetzung: gebaute libk1520core (`tools/dev.sh build`).
"""

import argparse
import os
import re
import shutil
import sys
import tempfile

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HIER))
sys.path.insert(0, REPO)

#: Ordner der Original-Systemdisketten
ORIGINALE = os.path.join(REPO, 'disks', 'pc1715_microdos')

# Taste "Return" des Emulators (Qt::Key_Return); alles andere geht als ASCII
QT_RETURN = 0x01000004


class Sitzung:
    """Ein PC 1715 mit MicroDOS an der Tastatur: tippen, auf Bildschirmtext warten, Bild lesen."""

    def __init__(self, a_pfad, b_pfad=None, still=True, modell='pc1715', laufwerke=None):
        from app.core_binding.k1520 import K1520Emulator
        self.still = still
        self.emu = K1520Emulator(drive_types=laufwerke, machine=modell)
        # "cpa800" ist nur der Katalogname fuers Einlegen; HFE traegt die Geometrie selbst
        if not self.emu.mount_disk(0, a_pfad, 'cpa800', False):
            raise RuntimeError('A: einlegen: ' + str(self.emu.last_error()))
        if b_pfad and os.path.exists(b_pfad):
            if not self.emu.mount_disk(1, b_pfad, 'cpa800', False):
                raise RuntimeError('B: einlegen: ' + str(self.emu.last_error()))
        elif b_pfad:
            if not self.emu.create_disk(1, b_pfad, ''):
                raise RuntimeError('B: anlegen: ' + str(self.emu.last_error()))
        self.emu.power_on()

    def bild(self):
        zeilen = (z.rstrip(' \x00') for z in self.emu.screen_text(25).split('\n'))
        return '\n'.join(z for z in zeilen if z.strip())

    def letzte_zeile(self):
        b = self.bild().split('\n')
        return b[-1] if b else ''

    def sage(self, text):
        if not self.still:
            print(text, flush=True)

    def lauf(self, takte):
        self.emu.run(takte)

    def bis(self, text, frist=600_000_000, schritt=1_000_000):
        """Rechnen, bis `text` (Teilzeichenkette oder re.Pattern) im Bild steht."""
        t = 0
        while t < frist:
            b = self.bild()
            if (text.search(b) if hasattr(text, 'search') else text in b):
                return True
            t += self.emu.run(schritt)
        return False

    def bis_prompt(self, frist=200_000_000):
        """Warten bis die letzte Bildzeile ein nackter Prompt `X>` ist (Kommando fertig)."""
        t = 0
        while t < frist:
            if re.fullmatch(r'[A-P]>', self.letzte_zeile()):
                self.emu.run(2_000_000)
                return True
            t += self.emu.run(1_000_000)
        return False

    def taste(self, zeichen):
        code = QT_RETURN if zeichen == '\r' else ord(zeichen)
        # 150 000 Takte gedrueckt, 100 000 Pause (Entprellung der S600-Tastatur, pc1715_input.h)
        self.emu.key_press(code)
        self.emu.run(150_000)
        self.emu.key_release(code)
        self.emu.run(300_000)

    def strg(self, buchstabe):
        """Strg+Buchstabe (CTRL geht der Taste einzeln voraus, das macht der Kern)."""
        code = ord(buchstabe)
        self.emu.key_press(code, False, True)
        self.emu.run(150_000)
        self.emu.key_release(code)
        self.emu.run(300_000)

    def tippe(self, text):
        for z in text:
            self.taste(z)

    # ── Betriebssystem ──────────────────────────────────────────────────────
    def neustart(self):
        """RESET-Taste: die Disketten bleiben liegen (vorher geschrieben), A: bootet neu."""
        self.emu.flush_disks()
        self.emu.reset()
        self.kaltstart_bis_prompt()

    def kaltstart_bis_prompt(self):
        """Booten.  INITIAL.SUB der Originaldiskette ist eine leere Zeile -> zwei Prompts hintereinander."""
        if not self.bis('MicroDOS Vers', frist=400_000_000):
            raise RuntimeError('kein MicroDOS-Banner:\n' + self.bild())
        self.sage('Banner: ' + [z for z in self.bild().split('\n') if 'MicroDOS Vers' in z][0].strip())
        if not self.bis_prompt():
            raise RuntimeError('kein Prompt "A>":\n' + self.bild())
        self.lauf(30_000_000)          # INITIAL.SUB zu Ende laufen lassen

    def kommando(self, text, frist=200_000_000):
        self.tippe(text + '\r')
        if not self.bis_prompt(frist):
            raise RuntimeError('Kommando %r endete nicht am Prompt:\n%s' % (text, self.bild()))
        return self.bild()


#: Formate von FORMAT15 und MSYSG15A (Handbuch msysgen.txt/mdosb15.txt, am Bild gemessen)
FORMAT15_5x1024_DS = '5'        # "5 = DD - DS 5 * 1024 * 80" (MFS 1.6 = K5601)
MSYSG_FORMAT_5x1024_DS = '4'    # "4 - 5*1024*80*2"


def formatiere(s, laufwerk='B', format_nr=FORMAT15_5x1024_DS):
    """FORMAT15: Laufwerk (EINE Taste), Format (EINE Taste), Warnung `Y`; Ende mit Neustart.

    Laufwerk und Format brauchen KEIN Return.  Nach `FORMATTING COMPLETE` fragt FORMAT15 wieder nach dem
    Laufwerk und hat keinen Ausgang.  Strg+C (Warmstart) fuehrt zu `Bdos error on A:$$$.SUB disk error`:
    FORMAT15 laesst das Motorregister (21H) auf "nur Motor B", und der MicroDOS-BIOS schreibt 21H nie
    (Gastverhalten) -- erst der RESET stellt es zurueck.  Deshalb hier ein Neustart (Kaltstart von A:).
    """
    s.tippe('FORMAT15\r')
    if not s.bis('PLEASE ENTER DRIVE'):
        raise RuntimeError('FORMAT15 meldet sich nicht:\n' + s.bild())
    s.tippe(laufwerk)
    if not s.bis('PLEASE SELECT FORMAT'):
        raise RuntimeError('Formatfrage fehlt:\n' + s.bild())
    s.tippe(format_nr)
    if not s.bis('ALL FILES WILL BE SCRATCHED'):
        raise RuntimeError('Warnung fehlt:\n' + s.bild())
    s.lauf(5_000_000)      # das BIOS schaltet die Laufwerke nach ~2,5 s Ruhe ab; nicht zu lange warten
    s.tippe('Y')
    if not s.bis('FORMATTING COMPLETE', frist=1_500_000_000):
        raise RuntimeError('FORMAT15 wurde nicht fertig:\n' + s.bild())
    if 'BAD' in s.bild().upper():
        raise RuntimeError('FORMAT15 meldet Fehler:\n' + s.bild())
    s.lauf(30_000_000)
    s.neustart()


def seldsk15(s, laufwerk, einheit, typ, format_nr):
    """SELDSK15 bedienen (jede Antwort EINE Taste ohne Return); Typ 3 = MFS 1.6, Format 7 = 5 x 1024 x 80 x 2."""
    s.tippe('SELDSK15\r')
    for erwartet, antwort in (('logische Laufwerk', laufwerk), ('physische', einheit), ('Laufwerkstyp', typ),
                              ('Formate sind', format_nr)):
        if not s.bis(erwartet):
            raise RuntimeError('SELDSK15: %r fehlt:\n%s' % (erwartet, s.bild()))
        s.tippe(antwort)
        s.lauf(3_000_000)
    if not s.bis('Wiederholung'):
        raise RuntimeError('SELDSK15: Wiederholung fehlt:\n' + s.bild())
    s.tippe('N')
    if not s.bis_prompt():
        raise RuntimeError('SELDSK15 kehrte nicht zum Prompt zurueck:\n' + s.bild())


#: Vorgabe der Generierung: Bildschirm 2 = 2K (K7222, 80 x 24), Drucker 2 = IFSS (1 = V.24),
#: Laufwerke A und B je Einheit 1/2 im Format 4 (= 5 x 1024, 80 Spuren, 2 Seiten).
VORGABE = dict(bildschirm='2', drucker='2', laufwerke=[('A', '1', '4'), ('B', '2', '4')], ziel='B')


def generiere(s, programm='MSYSG15A', **kw):
    """MSYSG15A bedienen (Menuefolge am Bild gemessen; jede Antwort EINE Taste ohne Return)."""
    p = dict(VORGABE)
    p.update(kw)

    def frage(erwartet, antwort):
        if not s.bis(erwartet):
            raise RuntimeError('%s: %r fehlt:\n%s' % (programm, erwartet, s.bild()))
        s.tippe(antwort)
        s.lauf(3_000_000)

    s.tippe(programm + '\r')
    frage('Bildschirmansteuerung', p['bildschirm'])
    frage('Interface benutzen Sie fuer Ihren Drucker', p['drucker'])
    for i, (lw, einheit, fmt) in enumerate(p['laufwerke']):
        if i:
            frage('Wiederholung (J/N)', 'J')
        frage('Drive:', lw)
        frage('Unit:', einheit)
        frage('Format:', fmt)
    frage('Wiederholung (J/N)', 'N')
    frage('zur Generierung benutzt werden', p['ziel'])
    # Das Generierungsprotokoll erscheint, sobald MICRODOS.SYS geschrieben ist (kein "Richtig?" wie am 1526)
    if not s.bis('Laufwerk D:', frist=400_000_000):
        raise RuntimeError('%s hat kein Generierungsprotokoll geliefert:\n%s' % (programm, s.bild()))
    s.lauf(20_000_000)
    s.tippe('N')
    if not s.bis_prompt():
        raise RuntimeError('%s kehrte nicht zum Prompt zurueck:\n%s' % (programm, s.bild()))


def bedienprobe(s):
    """D, Laufwerkswechsel, Ausgabeumlenkung in eine Datei auf B:, Loeschen -- auf der NEUEN Diskette in B:."""
    if 'MICRODOS.SYS' not in s.kommando('D B:'):
        raise RuntimeError('MICRODOS.SYS fehlt auf B::\n' + s.bild())
    s.kommando('>B:DIR.TXT')            # ab jetzt geht die Konsolenausgabe in die Datei
    s.kommando('D')
    s.kommando('>')                     # Umlenkung beenden
    if 'DIR     .TXT' not in s.kommando('D B:'):
        raise RuntimeError('DIR.TXT nicht angelegt:\n' + s.bild())


def probe_kaltstart(pfad, still=True, modell='pc1715'):
    """Die erzeugte Diskette als A: kalt starten; liefert das Ergebnis von "D"."""
    with tempfile.TemporaryDirectory() as t:
        kopie = os.path.join(t, 'a.hfe')
        shutil.copy(pfad, kopie)
        s = Sitzung(kopie, still=still, modell=modell)
        s.kaltstart_bis_prompt()
        return s.kommando('D')


def bauen(zielordner, quelle=None, probe=True, still=True, mit_bedienprobe=False, modell='pc1715'):

    """Gesamtablauf; liefert den Pfad der fertigen Bootdiskette."""
    quelle = quelle or os.path.join(ORIGINALE, 'pc1715_microdos_k5601_system.hfe')
    if not os.path.isfile(quelle):
        raise FileNotFoundError(quelle)
    os.makedirs(zielordner, exist_ok=True)
    ziel = os.path.join(zielordner, 'microdos1715_boot_k5601.hfe')
    with tempfile.TemporaryDirectory() as t:
        a = os.path.join(t, 'a.hfe')
        b = os.path.join(t, 'b.hfe')
        shutil.copy(quelle, a)             # das Original wird nie beschrieben
        s = Sitzung(a, b, still=still, modell=modell)
        s.kaltstart_bis_prompt()
        s.sage('1. Kaltstart der Systemdiskette: ok')
        formatiere(s)
        s.sage('2. FORMAT15 B: ok')
        if 'no files' not in s.kommando('D B:'):
            raise RuntimeError('B: nach FORMAT15 nicht leer:\n' + s.bild())
        generiere(s)
        s.sage('3. MSYSG15A -> B: ok')
        if mit_bedienprobe:
            bedienprobe(s)
        elif 'MICRODOS.SYS' not in s.kommando('D B:'):
            raise RuntimeError('MICRODOS.SYS fehlt auf B::\n' + s.bild())
        s.lauf(10_000_000)
        s.emu.flush_disks()
        shutil.copy(b, ziel)
    if probe:
        d = probe_kaltstart(ziel, still, modell)
        if 'MICRODOS.SYS' not in d:
            raise RuntimeError('neue Diskette bootet nicht richtig:\n' + d)
        if not still:
            print('4. Kaltstart von der neuen Diskette: ok')
    return ziel


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('zielordner')
    ap.add_argument('--quelle', help='Systemdiskette (Vorgabe: Original aus disks/pc1715_microdos/)')
    ap.add_argument('--ohne-probe', action='store_true', help='Kaltstart von der neuen Diskette weglassen')
    ap.add_argument('--bedienprobe', action='store_true', help='auf der neuen Diskette D, > und Datei anlegen')
    ap.add_argument('--still', action='store_true')
    a = ap.parse_args(argv)
    try:
        pfad = bauen(a.zielordner, a.quelle, not a.ohne_probe, a.still, a.bedienprobe)
    except (RuntimeError, FileNotFoundError) as e:
        print('Fehler: %s' % e, file=sys.stderr)
        return 1
    print(pfad)
    return 0


if __name__ == '__main__':
    sys.exit(main())

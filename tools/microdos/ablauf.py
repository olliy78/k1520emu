#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
ablauf.py  -  MicroDOS 1526: Leerdiskette formatieren -> MSYSG20 -> bootfaehige Systemdiskette
================================================================================================

Treibt den A5120-Emulator (Python-Bindung, libk1520core) genau so, wie ein Anwender es an der
Tastatur taete (Merkposten: doc/merkposten/microdos.md):

  1. Systemdiskette (Original, Temp-Kopie) als A:, eine ECHTE LEERDISKETTE (unformatiert, Geometrie des
     Laufwerks K5601) als B:.
  2. FORMAT20: Laufwerk B, Format 3 (5 x 1024 B, 80 Zylinder, 2 Seiten), alle Spuren.
  3. MSYSG20: Tastatur, Bildschirm, Drucker, Zusatzkanal, Laufwerke A-D, Ziel B: -> MicroDOS.SYS
     (samt SYL-Urlader in Spur 0/Sektor 1) auf B:.
  4. Probe: die neue Diskette als A: kalt starten, "A>" abwarten, "D" muss MicroDOS.SYS zeigen.

Aufruf:
  python3 -I tools/microdos/ablauf.py ZIELVERZEICHNIS [--quelle ABBILD.hfe] [--tastatur k7637|k7636]
          [--ohne-probe] [--still]

Ergebnis: ZIELVERZEICHNIS/microdos26_boot_<tastatur>.hfe.  Voraussetzung: gebaute libk1520core
(`tools/dev.sh build`); ohne --quelle dient das Original aus disks/a5120_microdos1526/ (zur Tastatur passend).

Die Tastatur der QUELLE und die der ZIELDISKETTE sind getrennt waehlbar (--tastatur gilt fuer die Ziel-
diskette; die Quelle wird passend zu --tastatur gewaehlt: K7637 = "seriell", K7636 = "parallel").
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

#: Ordner der Original-Systemdisketten (byteglich zu den Abbildern aus der Sammlung)
ORIGINALE = os.path.join(REPO, 'disks', 'a5120_microdos1526')
QUELLEN = {
    'k7637': 'a5120_microdos26_k5601_seriell.hfe',
    'k7636': 'a5120_microdos26_k5601_parallel.hfe',
}

# Taste "Return" des Emulators (Qt::Key_Return); alles andere geht als ASCII
QT_RETURN = 0x01000004

# Ziffern der MSYSG20-Menues (Reihenfolge der Fragen = Reihenfolge der Eintraege)
TASTATUR_ZIFFER = {'k7634_54': '1', 'k7634_51': '2', 'k7636': '3', 'k7637': '4'}


class Sitzung:
    """Ein A5120 mit MicroDOS an der Tastatur: tippen, auf Bildschirmtext warten, Bild lesen."""

    def __init__(self, tastatur, a_pfad, b_pfad=None, still=True, laufwerke=None):
        from app.core_binding.k1520 import K1520Emulator
        self.still = still
        self.emu = K1520Emulator(drive_types=laufwerke, tastatur=tastatur)
        # "cpa800" ist nur der Katalogname fuer das Einlegen; HFE traegt die Geometrie selbst
        if not self.emu.mount_disk(0, a_pfad, 'cpa800', False):
            raise RuntimeError('A: einlegen: ' + str(self.emu.last_error()))
        if b_pfad:
            if not self.emu.create_disk(1, b_pfad, ''):
                raise RuntimeError('B: anlegen: ' + str(self.emu.last_error()))
        self.emu.power_on()

    # ── Bild ────────────────────────────────────────────────────────────────
    def bild(self):
        zeilen = (z.rstrip(' \x00') for z in self.emu.screen_text().split('\n'))
        return '\n'.join(z for z in zeilen if z.strip())

    def letzte_zeile(self):
        b = self.bild().split('\n')
        return b[-1] if b else ''

    def sage(self, text):
        if not self.still:
            print(text, flush=True)

    # ── Lauf ────────────────────────────────────────────────────────────────
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
                self.emu.run(2_000_000)          # Nachlauf: Motor/Schreibpause
                return True
            t += self.emu.run(1_000_000)
        return False

    # ── Tastatur ────────────────────────────────────────────────────────────
    def taste(self, zeichen):
        code = QT_RETURN if zeichen == '\r' else ord(zeichen)
        # 150 000 Takte gedrueckt, 600 000 Pause: die Tastatur sendet mit 9600 Baud (K7637)
        self.emu.key_press(code)
        self.emu.run(150_000)
        self.emu.key_release(code)
        self.emu.run(600_000)

    def tippe(self, text):
        for z in text:
            self.taste(z)

    # ── Betriebssystem ──────────────────────────────────────────────────────
    def kaltstart_bis_prompt(self):
        """Booten. INITIAL.SUB der Originaldisketten laeuft MKEY20 an und wartet auf ein Return."""
        if not self.bis('MicroDOS Vers', frist=400_000_000):
            raise RuntimeError('kein MicroDOS-Banner:\n' + self.bild())
        self.sage('Banner: ' + [z for z in self.bild().split('\n') if 'MicroDOS Vers' in z][0].strip())
        # Die Quelle hat INITIAL.SUB (MKEY-Hinweis); die selbst erzeugte Diskette nicht -> sofort "A>"
        self.lauf(30_000_000)
        if self.letzte_zeile().startswith('A>MKEY20'):
            self.taste('\r')
        if not self.bis_prompt():
            raise RuntimeError('kein Prompt "A>":\n' + self.bild())

    def kommando(self, text, frist=200_000_000):
        self.tippe(text + '\r')
        if not self.bis_prompt(frist):
            raise RuntimeError('Kommando %r endete nicht am Prompt:\n%s' % (text, self.bild()))
        return self.bild()


def formatiere(s, laufwerk='B', format_nr='3'):
    """FORMAT20 (transient): Laufwerk, Format, Bestaetigung J, alle Spuren; endet mit "N" (Wiederholung)."""
    s.tippe('FORMAT20\r')
    if not s.bis('Auf welchem Laufwerk'):
        raise RuntimeError('FORMAT20 meldet sich nicht:\n' + s.bild())
    s.tippe(laufwerk + '\r')
    # Format 3 wird mit Return bestaetigt; danach fragt FORMAT20 "Richtig (J/N)"
    if not s.bis('Wie soll die'):
        raise RuntimeError('Formatfrage fehlt:\n' + s.bild())
    s.tippe(format_nr + '\r')
    if not s.bis('Richtig (J/N)'):
        raise RuntimeError('Formatbestaetigung fehlt:\n' + s.bild())
    s.tippe('J')
    if not s.bis('Formatierung beendet', frist=900_000_000):
        raise RuntimeError('FORMAT20 wurde nicht fertig:\n' + s.bild())
    if 'Bad' in s.bild():
        raise RuntimeError('FORMAT20 meldet Fehler:\n' + s.bild())
    s.tippe('N')                       # keine Wiederholung
    if not s.bis_prompt():
        raise RuntimeError('FORMAT20 kehrte nicht zum Prompt zurueck:\n' + s.bild())


#: Vorgabe der Generierung (Bestueckung des Emulators: K7024, K7637, 4 x K5601; Drucker/Zusatzkanal:
#: die zwei Kanaele der K8025 -- Drucker IFSS, Zusatzkanal V.24).  Laufwerkstyp 3 = "MFS 1.6" (K5601),
#: Format 8 = 5 x 1024 B, 80 Zylinder, 2 Seiten.
VORGABE = dict(
    tastatur='k7637', bildschirm='2', drucker='1', typenrad='1', zusatzkanal='3',
    laufwerke=[('A', '1', '3', '8'), ('B', '2', '3', '8')], ziel='B',
)


def generiere(s, **kw):
    """MSYSG20 bedienen (Menuefolge nach doc/merkposten/microdos.md, Handbuch msysgen.txt)."""
    p = dict(VORGABE)
    p.update(kw)

    def frage(erwartet, antwort):
        if not s.bis(erwartet):
            raise RuntimeError('MSYSG20: %r fehlt:\n%s' % (erwartet, s.bild()))
        s.tippe(antwort)
        s.lauf(3_000_000)

    s.tippe('MSYSG20\r')
    frage('Welche Tastatur', TASTATUR_ZIFFER[p['tastatur']])
    frage('Bildschirmansteuerung', p['bildschirm'])
    frage('Interface benutzen Sie fuer Ihren Drucker', p['drucker'])
    frage('Welches Typenrad', p['typenrad'])
    frage('Zusatzkanal', p['zusatzkanal'])
    for i, (lw, phys, typ, fmt) in enumerate(p['laufwerke']):
        if i:
            frage('Wiederholung (J/N)', 'J')       # noch ein Laufwerk
        frage('logische Laufwerk', lw)
        frage('physische  Einheit', phys)
        frage('Laufwerkstyp', typ)
        frage('Formate sind auf diesem Laufwerk', fmt)
    frage('Wiederholung (J/N)', 'N')               # keine weiteren Laufwerke
    frage('zur Generierung benutzt werden', p['ziel'])
    frage('Richtig (J/N)', 'J')                    # Systemdatei schreiben
    # Danach folgen Generierungsprotokoll (Drucker) und die Frage nach seiner Wiederholung
    if not s.bis('Laufwerk D:', frist=400_000_000):
        raise RuntimeError('MSYSG20 hat kein Generierungsprotokoll geliefert:\n' + s.bild())
    s.lauf(20_000_000)
    s.tippe('N')
    if not s.bis_prompt():
        raise RuntimeError('MSYSG20 kehrte nicht zum Prompt zurueck:\n' + s.bild())


def probe_kaltstart(pfad, tastatur, still=True):
    """Die erzeugte Diskette als A: kalt starten; liefert das Ergebnis von "D"."""
    with tempfile.TemporaryDirectory() as t:
        kopie = os.path.join(t, 'a.hfe')
        shutil.copy(pfad, kopie)
        s = Sitzung(tastatur, kopie, still=still)
        s.kaltstart_bis_prompt()
        return s.kommando('D')


def bauen(zielordner, quelle=None, tastatur='k7637', probe=True, still=True):
    """Gesamtablauf; liefert den Pfad der fertigen Bootdiskette."""
    quelle = quelle or os.path.join(ORIGINALE, QUELLEN[tastatur])
    if not os.path.isfile(quelle):
        raise FileNotFoundError(quelle)
    os.makedirs(zielordner, exist_ok=True)
    ziel = os.path.join(zielordner, 'microdos26_boot_%s.hfe' % tastatur)
    with tempfile.TemporaryDirectory() as t:
        a = os.path.join(t, 'a.hfe')
        b = os.path.join(t, 'b.hfe')
        shutil.copy(quelle, a)             # das Original wird nie beschrieben
        s = Sitzung(tastatur, a, b, still=still)
        s.kaltstart_bis_prompt()
        s.sage('1. Kaltstart der Systemdiskette: ok')
        formatiere(s)
        s.sage('2. FORMAT20 B: ok')
        # Eine formatierte, leere Diskette: D meldet "no files"
        if 'no files' not in s.kommando('D B:'):
            raise RuntimeError('B: nach FORMAT20 nicht leer:\n' + s.bild())
        generiere(s, tastatur=tastatur)
        s.sage('3. MSYSG20 -> B: ok')
        if 'MicroDOS.SYS' not in s.kommando('D B:'):
            raise RuntimeError('MicroDOS.SYS fehlt auf B::\n' + s.bild())
        s.lauf(10_000_000)
        s.emu.flush_disks()
        shutil.copy(b, ziel)
    if probe:
        d = probe_kaltstart(ziel, tastatur, still)
        if 'MicroDOS.SYS' not in d:
            raise RuntimeError('neue Diskette bootet nicht richtig:\n' + d)
        if not still:
            print('4. Kaltstart von der neuen Diskette: ok')
    return ziel


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('zielordner')
    ap.add_argument('--quelle', help='Systemdiskette (Vorgabe: Original aus disks/a5120_microdos1526/)')
    ap.add_argument('--tastatur', choices=sorted(QUELLEN), default='k7637')
    ap.add_argument('--ohne-probe', action='store_true', help='Kaltstart von der neuen Diskette weglassen')
    ap.add_argument('--still', action='store_true')
    a = ap.parse_args(argv)
    try:
        pfad = bauen(a.zielordner, a.quelle, a.tastatur, not a.ohne_probe, a.still)
    except (RuntimeError, FileNotFoundError) as e:
        print('Fehler: %s' % e, file=sys.stderr)
        return 1
    print(pfad)
    return 0


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
ladediskette.py  -  mutos_io aus der Quelle bauen und auf eine Ladediskette bringen
===================================================================================

  ladediskette.py datei  <ziel-ordner>             mutos_io + mutos_io.fileinfo schreiben
  ladediskette.py pruefen                          Erstbau == Archivdatei und -fileinfo (77 ohne Werkzeugkette)
  ladediskette.py diskette ARCHIV ZIEL.hfe --laufwerke VARIANTE [--ueberschreiben]
                                                   Archiv auspacken, mutos_io (+ .fileinfo)
                                                   durch den Bau aus src/mutosio.mac ersetzen,
                                                   dann tools/mutos8000/bau_ladediskette.py

Die Angaben der .fileinfo, die vom Inhalt abhaengen (segs=, segment=, zusatz=,
start=, Pruefung von mem=), werden aus den Segmentgrenzen der Quelle berechnet:
die Marken SEGnnnn / SEGnnnnE stehen in src/mutosio.mac, ihre Werte liefert die
Symboltabelle des M80-Listings.  Alles Uebrige (Datum, Typ, Eigenschaften, mem=)
kommt aus der Vorlage (der .fileinfo im Archiv) - geaendert wird nur, was ein
anderer Inhalt zwingend aendert.  Erstbau ohne Aenderung == Archivdatei
(Waechter tests/cli/ ... `cli_mutos_io_bau`).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HIER)))
import cpm_bau  # noqa: E402

QUELLE = os.path.join(HIER, 'src', 'mutosio.mac')
ARCHIV = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(HIER))),
                      'disks', 'a5120_mutos8000', 'a5120_mutos8000_boot_udos.zip')
NAME = 'mutos_io'


def segmente(listing):
    """Symboltabelle des M80-Listings -> [(anfang, laenge)] der Ladesegmente."""
    sym = {}
    for wert, name in re.findall(r'([0-9A-F]{4})\s+\t?([A-Z0-9_]+)', listing):
        sym[name] = int(wert, 16)
    segs = []
    for name, a in sorted(sym.items(), key=lambda x: x[1]):
        m = re.fullmatch(r'SEG([0-9A-F]{4})', name)
        if m and f'{name}E' in sym:
            segs.append((a, sym[f'{name}E'] - a))
    if not segs:
        raise RuntimeError("keine SEGnnnn/SEGnnnnE-Marken in der Symboltabelle")
    return segs


def baue(quelle=QUELLE, k8x512=False):
    """Quelle bauen; k8x512=True setzt den Variantenschalter (K8X512 EQU 1, Entwurf 40 M5)."""
    """-> (bytes der Datei mutos_io, [(anfang, laenge)])."""
    if not cpm_bau.werkzeugkette_da():
        raise RuntimeError(f"CP/M-Werkzeugkette nicht gefunden ({cpm_bau.CPA_TOOLS}; "
                           f"CPA_TOOLS=<pfad> setzen)")
    with tempfile.TemporaryDirectory(prefix='mutosio_') as tmp:
        # Variante: Kopie der Quelle mit gesetztem Schalter in einem eigenen Ordner bauen
        text_q = open(quelle, 'r', encoding='utf-8').read()
        if k8x512:
            neu, n = re.subn(r'^K8X512\tEQU\t0\b', 'K8X512\tEQU\t1', text_q, flags=re.M)
            if n != 1:
                raise RuntimeError("Schalter 'K8X512 EQU 0' in der Quelle nicht gefunden")
            text_q = neu
        qordner = os.path.join(tmp, 'q')
        os.makedirs(os.path.join(qordner, 'src'))
        quelle_neu = os.path.join(qordner, 'src', 'mutosio.mac')
        open(quelle_neu, 'w', encoding='utf-8', newline='').write(text_q)
        p = cpm_bau.Programm('mutosio', qordner, endung='bin')
        bin_pfad = os.path.join(tmp, 'mutosio.bin')
        build = os.path.join(tmp, 'build')
        cpm_bau.baue(p, build, bin_pfad)
        daten = open(bin_pfad, 'rb').read()
        # Listing mit Symboltabelle in einem eigenen Lauf
        os.makedirs(os.path.join(tmp, 'lst'))
        text = text_q.replace('\r\n', '\n').replace('\n', '\r\n')
        with open(os.path.join(tmp, 'lst', 'MUTOSIO.MAC'), 'wb') as f:
            f.write(text.encode('ascii', errors='replace'))
        shutil.copy2(os.path.join(cpm_bau.CPA_TOOLS, 'm80.com'), os.path.join(tmp, 'lst'))
        subprocess.run([cpm_bau._cparun(), 'm80', 'MUTOSIO,MUTOSIO=MUTOSIO'],
                       cwd=os.path.join(tmp, 'lst'), capture_output=True, timeout=60)
        prn = [f for f in os.listdir(os.path.join(tmp, 'lst')) if f.lower() == 'mutosio.prn']
        if not prn:
            raise RuntimeError("M80 hat kein Listing erzeugt")
        listing = open(os.path.join(tmp, 'lst', prn[0]), 'r', errors='replace').read()
    return daten, segmente(listing)


def fileinfo(vorlage_text, daten, segs):
    """Neue .fileinfo aus der Vorlage; nur die inhaltsabhaengigen Felder werden ersetzt."""
    felder = {}
    for z in vorlage_text.splitlines():
        if z and not z.startswith('#') and '=' in z:
            k, _, v = z.partition('=')
            felder[k] = v
    low, high, stack = felder['mem'].split(':')
    ende = max(a + n for a, n in segs)
    if int(low, 16) > segs[0][0] or int(high, 16) < ende:
        raise RuntimeError(f"mem={felder['mem']} reicht nicht fuer die Segmente (Ende {ende:04X}H); "
                           f"HIGH in der Vorlage anheben")
    if len(daten) % 256:
        raise RuntimeError("Dateilaenge ist kein Vielfaches von 256")
    felder['start'] = f'{segs[0][0]:04X}'
    felder['segment'] = f'{segs[0][0]:04X}:{segs[0][1]}'
    a2, n2 = segs[1] if len(segs) > 1 else (0, 0)
    felder['zusatz'] = f'{(n2 << 16) | a2:X}'
    felder['segs'] = ','.join(f'{a:04X}+{n:04X}' for a, n in segs)
    felder['rest'] = '256'
    felder['mem'] = f'{segs[0][0]:04X}:{high}:{stack}'
    kopf = [z for z in vorlage_text.splitlines() if z.startswith('#')]
    reihenfolge = ['fs', 'name', 'typ', 'eig', 'start', 'satz', 'block', 'rest', 'segment',
                   'mem', 'zusatz', 'segs', 'erst', 'geaend']
    return '\n'.join(kopf + [f'{k}={felder[k]}' for k in reihenfolge if k in felder]) + '\n'


def vorlage(archiv):
    if os.path.isdir(archiv):
        return open(os.path.join(archiv, 'dateien', f'{NAME}.fileinfo'), encoding='utf-8').read()
    with zipfile.ZipFile(archiv) as z:
        return z.read(f'dateien/{NAME}.fileinfo').decode('utf-8')


def schreibe(ordner, archiv=ARCHIV, k8x512=False, quelle=QUELLE):
    daten, segs = baue(quelle, k8x512=k8x512)
    os.makedirs(ordner, exist_ok=True)
    open(os.path.join(ordner, NAME), 'wb').write(daten)
    open(os.path.join(ordner, f'{NAME}.fileinfo'), 'w', encoding='utf-8').write(
        fileinfo(vorlage(archiv), daten, segs))
    return daten, segs


def pruefen(archiv=ARCHIV):
    """Der Bau aus der Quelle muss die Datei UND die .fileinfo des Archivs reproduzieren
    (Erstbau ohne Aenderung).  Exit 0 gleich, 1 verschieden, 77 ohne Werkzeugkette."""
    if not cpm_bau.werkzeugkette_da():
        print(f"UEBERSPRUNGEN: CP/M-Werkzeugkette nicht gefunden ({cpm_bau.CPA_TOOLS}).")
        return cpm_bau.SKIP
    daten, segs = baue()
    with zipfile.ZipFile(archiv) as z:
        orig = z.read(f'dateien/{NAME}')
        info_orig = z.read(f'dateien/{NAME}.fileinfo').decode('utf-8')
    neu = fileinfo(info_orig, daten, segs)
    inhalt = lambda t: [z for z in t.splitlines() if not z.startswith('#')]
    fehler = []
    if daten != orig:
        fehler.append(f"Datei {NAME} weicht vom Archiv ab ({len(daten)} B gegen {len(orig)} B)")
    if inhalt(neu) != inhalt(info_orig):
        fehler.append(f"{NAME}.fileinfo weicht vom Archiv ab:\n  neu: {inhalt(neu)}\n  alt: {inhalt(info_orig)}")
    # Variante 8 x 512: Quelle mit K8X512 = 1 == Archivdatei + mutos_patch io-8x512 (bytegleich)
    sys.path.insert(0, os.path.dirname(HIER))
    import mutos_patch  # noqa: E402
    gepatcht, _ = mutos_patch.anwenden('io-8x512', orig)
    daten8, segs8 = baue(k8x512=True)
    if daten8 != gepatcht:
        diff = [i for i in range(min(len(daten8), len(gepatcht))) if daten8[i] != gepatcht[i]]
        fehler.append(f"Variante K8X512 weicht von mutos_patch io-8x512 ab (Laenge {len(daten8)}/{len(gepatcht)}, "
                      f"erste Abweichungen Dateioffset {[hex(i) for i in diff[:6]]})")
    if segs8 != segs:
        fehler.append(f"Variante K8X512 hat andere Segmente: {segs8} gegen {segs}")
    for f in fehler:
        print("FEHLER:", f, file=sys.stderr)
    if not fehler:
        print(f"OK: {NAME} und .fileinfo aus der Quelle == Archiv ({len(daten)} Bytes); "
              f"Variante K8X512 == mutos_patch io-8x512")
    return 1 if fehler else 0


def diskette(archiv, ziel, laufwerke, ueberschreiben, quelle=QUELLE):
    with tempfile.TemporaryDirectory(prefix='mutosio_lade_') as tmp:
        if os.path.isdir(archiv):
            shutil.copytree(archiv, os.path.join(tmp, 'a'))
        else:
            with zipfile.ZipFile(archiv) as z:
                z.extractall(os.path.join(tmp, 'a'))
        v8 = laufwerke == 'k5601_8x512'
        schreibe(os.path.join(tmp, 'a', 'dateien'), archiv, k8x512=v8, quelle=quelle)
        if v8:
            # der Quellbau liefert mutos_io schon fertig fuer 8 x 512: den Byte-Patch weglassen
            y = os.path.join(tmp, 'a', 'bauangaben.yaml')
            t = open(y, encoding='utf-8').read()
            t2 = re.sub(r'^patch_k5601_8x512_\d+:\s*mutos_io\s+io-8x512.*\n', '', t, flags=re.M)
            if t2 == t:
                raise RuntimeError("bauangaben.yaml: Patch 'mutos_io io-8x512' nicht gefunden")
            open(y, 'w', encoding='utf-8').write(t2)
        cmd = [sys.executable, '-I', os.path.join(os.path.dirname(HIER), 'bau_ladediskette.py'),
               os.path.join(tmp, 'a'), ziel, '--laufwerke', laufwerke]
        if ueberschreiben:
            cmd.append('--ueberschreiben')
        return subprocess.call(cmd)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[1])
    sp = ap.add_subparsers(dest='cmd', required=True)
    d = sp.add_parser('datei')
    d.add_argument('ordner')
    d.add_argument('--quelle', default=QUELLE, help='andere .mac statt src/mutosio.mac (Versuche)')
    sp.add_parser('pruefen')
    k = sp.add_parser('diskette')
    k.add_argument('archiv', nargs='?', default=ARCHIV)
    k.add_argument('ziel')
    k.add_argument('--laufwerke', required=True)
    k.add_argument('--ueberschreiben', action='store_true')
    k.add_argument('--quelle', default=QUELLE, help='andere .mac statt src/mutosio.mac (Versuche)')
    a = ap.parse_args(argv)
    try:
        if a.cmd == 'pruefen':
            return pruefen()
        if a.cmd == 'datei':
            daten, segs = schreibe(a.ordner, quelle=a.quelle)
            print(f"{NAME}: {len(daten)} Bytes, Segmente "
                  + ', '.join(f'{x:04X}+{n:04X}' for x, n in segs))
            return 0
        return diskette(a.archiv, a.ziel, a.laufwerke, a.ueberschreiben, a.quelle)
    except RuntimeError as e:
        print(f"FEHLER: {e}", file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())

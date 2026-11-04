#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
build.py  -  die MicroDOS-1526-Bootdisketten des A5120 selbst bauen
===================================================================

  disks/a5120_microdos1526/a5120_microdos26_k5601_seriell.hfe       K5601 (80 Zyl. DS), Tastatur K7637
  disks/a5120_microdos1526/a5120_microdos26_k5601_parallel.hfe      K5601, Tastatur K7636
  disks/a5120_microdos1526/a5120_microdos26_k5600_10_seriell.hfe    K5600.10 (40 Spuren), K7637
  disks/a5120_microdos1526/a5120_microdos26_k5600_10_parallel.hfe   K5600.10, K7636

Jede Diskette entsteht so, wie ein Anwender sie braucht (Merkposten doc/merkposten/microdos.md):
  1. ablauf.py: Original (tools/microdos/original/) booten, Leerdiskette mit FORMAT20 formatieren, MSYSG20
     erzeugt MicroDOS.SYS samt SYL-Urlader fuer die gewaehlte Tastatur/Laufwerke -> bootfaehige Diskette;
  2. DiskTool ``get``/``put``: die Beigaben der Originaldiskette (FORMAT20, DIP, MSYSG20, SELDSK20/SELPRT20/
     SELAUX20, MKINST20, KTL/KTP, POWER, MKEY20 und die Handbuchtexte) werden nachkopiert.
     NICHT mitgenommen: INITIAL.SUB (haelt beim Start in MKEY20 an und wartet auf ein Return), PIP.COM (abgeschnitten, s. u.), MicroDOS.SYS
     des Originals (die Generierung bringt ihre eigene), WordStar/MailMerge (Fremdprodukt, ohne Zweck hier) und
     alles im Nutzerbereich 1 (DataStar, Woerterbuecher).

Die vier Originalabzuege (byteglich zu Disketten_Images_RT/MicroDOS1526{s,p}[_5600].hfe) liegen als
Referenz und Bauquelle in tools/microdos/original/ und kommen NICHT mehr in disks/.

Aufruf (Voraussetzung: ``tools/dev.sh build``; der Emulator laeuft ueber die Python-Bindung):
  LD_LIBRARY_PATH=build venv/bin/python3 -I tools/microdos/build.py --tool build/k1520disktool            # bauen
  LD_LIBRARY_PATH=build venv/bin/python3 -I tools/microdos/build.py --tool build/k1520disktool --check    # vergleichen
  --out ORDNER                                                                                          # statt disks/
"""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HIER))
ORIG = os.path.join(HIER, 'original')
sys.path.insert(0, HIER)
sys.path.insert(0, REPO)
import ablauf  # noqa: E402

ORDNER = 'a5120_microdos1526'

#: Nicht von der Originaldiskette uebernommen (siehe Kopf)
#: PIP.COM der Sammlung ist abgeschnitten (Verzeichnis: 32 Saetze = 4096 B; der Code ruft aber 1A40H/1D84H, das Abbild
#: eines vollstaendigen PIP 1.5 haette rund 7,9 KB) und laeuft ins Leere -- DIP.COM ersetzt es.
AUSGENOMMEN = {'MICRODOS.SYS', 'PIP.COM', 'INITIAL.SUB', 'WS.COM', 'WSINSTAL.COM', 'WSM.DOC', 'WSMSGS.OVR', 'WSOVLY1.OVR',
               'MAILMRGE.OVR'}

#: Laufwerke des Emulators fuer das 40-Spur-Laufwerk K5600.10 (A:, B:; C:/D: unbelegt)
K5600 = ['K5600.10'] * 2 + [None, None]

#: Name -> Bauvorgabe.  ``tastatur``/``quelle`` = die Sitzung (immer die serielle Quelle, siehe ablauf.bauen),
#: ``ziel_tastatur`` = Tastatur der erzeugten Diskette.  Generierung: Typ 3 = MFS 1.6 (K5601), Format 8 = 5 x 1024 B, 80 Zyl., 2 Seiten;
#: Typ 1 = MFS 1.2 (K5600.10), Format 4 = 5 x 1024 B, 40 Spuren, 1 Seite.
DISKETTEN = {
    'a5120_microdos26_k5601_seriell.hfe': dict(
        tastatur='k7637', quelle='MicroDOS1526s.hfe', laufwerke=None, profil='microdos_ds80_5x1024_boot',
        generierung=dict(laufwerke=[('A', '1', '3', '8'), ('B', '2', '3', '8')])),
    'a5120_microdos26_k5601_parallel.hfe': dict(
        tastatur='k7637', ziel_tastatur='k7636', quelle='MicroDOS1526s.hfe', laufwerke=None, profil='microdos_ds80_5x1024_boot',
        generierung=dict(laufwerke=[('A', '1', '3', '8'), ('B', '2', '3', '8')])),
    'a5120_microdos26_k5600_10_seriell.hfe': dict(
        tastatur='k7637', quelle='MicroDOS1526s_5600.hfe', laufwerke=K5600, profil='microdos_ss40_5x1024_boot',
        generierung=dict(laufwerke=[('A', '1', '1', '4'), ('B', '2', '1', '4')])),
    'a5120_microdos26_k5600_10_parallel.hfe': dict(
        tastatur='k7637', ziel_tastatur='k7636', quelle='MicroDOS1526s_5600.hfe', laufwerke=K5600, profil='microdos_ss40_5x1024_boot',
        generierung=dict(laufwerke=[('A', '1', '1', '4'), ('B', '2', '1', '4')])),
}


def _tool(tool, *args):
    return subprocess.run([*tool, *args], capture_output=True, text=True, errors='replace')


def _beigaben(tool, quelle, tmp):
    """Dateien der Originaldiskette (Nutzerbereich 0) ohne die ausgenommenen -> Ordner."""
    r = _tool(tool, 'ls', quelle)
    # Dateinamen enthalten kein Leerzeichen, die Kopf- und Summenzeile ("N Dateien - X KB frei") schon;
    # "1:NAME" = Nutzerbereich 1 (DataStar usw.) bleibt weg
    namen = [z.strip() for z in r.stdout.splitlines() + r.stderr.splitlines() if z.strip()]
    namen = [n for n in namen if ':' not in n and ' ' not in n and n.upper() not in AUSGENOMMEN]
    ordner = os.path.join(tmp, 'beigaben')
    shutil.rmtree(ordner, ignore_errors=True)
    os.makedirs(ordner)
    for n in namen:
        g = _tool(tool, 'get', quelle, n, '--to', ordner)
        if g.returncode != 0:
            sys.exit('Fehler: get %s %s\n%s%s' % (quelle, n, g.stdout, g.stderr))
    return ordner, sorted(namen)


def _erzeuge(name, ziel, tool, still=True):
    v = DISKETTEN[name]
    quelle = os.path.join(ORIG, v['quelle'])
    with tempfile.TemporaryDirectory() as tmp:
        roh = ablauf.bauen(tmp, quelle=quelle, tastatur=v['tastatur'], probe=False, still=still,
                           laufwerke=v['laufwerke'], generierung=v['generierung'], dateiname='roh.hfe',
                           ziel_tastatur=v.get('ziel_tastatur'))
        ordner, namen = _beigaben(tool, quelle, tmp)
        dateien = [os.path.join(dp, f) for dp, _, fs in os.walk(ordner) for f in sorted(fs)]
        dateien.sort(key=lambda p: os.path.basename(p).upper())
        r = _tool(tool, 'put', '--no-backup', '--binary', roh, *dateien)
        if r.returncode != 0:
            sys.exit('Fehler: put in %s\n%s%s' % (name, r.stdout, r.stderr))
        shutil.copyfile(roh, ziel)
    return namen


def _sha(pfad):
    with open(pfad, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def _urteil(tool, pfad, profil):
    """Erkennung = erwartetes Profil und Dateisystempruefung ohne Befund; sonst Meldungstext."""
    r = _tool(tool, 'info', pfad)
    if r.returncode != 0 or 'Dateisystem: %s' % profil not in r.stdout:
        return 'vom DiskTool nicht als %s erkannt\n%s%s' % (profil, r.stdout, r.stderr)
    # Hinweise (Stufe Info) sind erlaubt -- auch das Original traegt "cpm.dir.geloescht"; Warnung/Fehler/Gefahr nicht
    c = _tool(tool, 'check', pfad)
    if any(z.startswith(('Warnung', 'Fehler', 'Gefahr')) for z in c.stdout.splitlines()):
        return 'Dateisystempruefung meldet etwas\n%s%s' % (c.stdout, c.stderr)
    return None


def bauen(out, tool, still=True):
    ordner = os.path.join(out, ORDNER)
    os.makedirs(ordner, exist_ok=True)
    ok = True
    for name, v in DISKETTEN.items():
        ziel = os.path.join(ordner, name)
        _erzeuge(name, ziel, tool, still)
        u = _urteil(tool, ziel, v['profil'])
        print('%s: %s%s' % (name, _sha(ziel)[:12], '' if not u else '  FEHLER ' + u))
        ok = ok and not u
    return ok


def pruefen(out, tool):
    ok = True
    ordner = os.path.join(out, ORDNER)
    with tempfile.TemporaryDirectory() as tmp:
        for name, v in DISKETTEN.items():
            ist = os.path.join(ordner, name)
            if not os.path.isfile(ist):
                print('%s: fehlt' % name)
                ok = False
                continue
            soll = os.path.join(tmp, name)
            _erzeuge(name, soll, tool)
            if _sha(ist) != _sha(soll):
                print('%s: weicht vom Bau aus original/ ab' % name)
                ok = False
                continue
            u = _urteil(tool, ist, v['profil'])
            if u:
                print('%s: %s' % (name, u))
                ok = False
                continue
            print('%s: ok' % name)
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--tool', required=True, help='k1520disktool')
    ap.add_argument('--emulator', action='append', default=[],
                    help='davor zu setzender Starter fuer das DiskTool (wiederholbar)')
    ap.add_argument('--out', default=os.path.join(REPO, 'disks'), help='Zielordner (Vorgabe: disks/)')
    ap.add_argument('--check', action='store_true', help='nur vergleichen, nichts schreiben')
    o = ap.parse_args()
    tool = [*o.emulator, os.path.abspath(o.tool)]
    if o.check:
        sys.exit(0 if pruefen(o.out, tool) else 1)
    sys.exit(0 if bauen(o.out, tool) else 1)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""K6a: aus der Laufabdeckung (abdeckung_gesamt.txt) die Karte tools/mutos8000/kernel/karte.txt erzeugen.

Aufruf (Repo-Wurzel):  python3 -I tools/mutos8000/kernel/lauf/abdeckung_zu_karte.py [abbild]
  abbild: mutos_1216 (Vorgabe: aus dem Ladedisketten-Archiv in den Arbeitsordner entpackt)

Die handgepflegte Basis (karte_basis.txt: Segment, PSA, Vektoren, Datenbereiche, Namen, Kommentare) bleibt unveraendert;
dahinter kommen `code`-Zeilen mit so wenigen Einstiegen, dass die Ablaufverfolgung von z8kdis JEDEN im Lauf ausgefuehrten
Befehlsanfang als Befehl erreicht.  Ein Einstieg ist Beweis "Code" (der Befehl lief); was nur durch die Verfolgung
hinzukommt, ist Code, weil ein ausgefuehrter Befehl dorthin springt/aufruft.  Wiederholbar: gleiche Eingabe, gleiche Karte.
"""
import os, re, subprocess, sys, tempfile, zipfile

HIER = os.path.dirname(os.path.abspath(__file__))
WURZEL = os.path.abspath(os.path.join(HIER, "..", "..", "..", ".."))
BASIS = os.path.join(HIER, "karte_basis.txt")
ZIEL = os.path.join(HIER, "..", "karte.txt")
ARCHIV = os.path.join(WURZEL, "disks", "a5120_mutos8000", "a5120_mutos8000_boot_udos.zip")
Z8KDIS = os.path.join(WURZEL, "build", "z8kdis")   # vorher: tools/dev.sh tool z8kdis --help (baut aktuell)

def abbild():
    if len(sys.argv) > 1:
        return sys.argv[1]
    d = tempfile.mkdtemp(prefix="mutos_1216_")
    with zipfile.ZipFile(ARCHIV) as z:
        z.extract("dateien/mutos_1216", d)
    return os.path.join(d, "dateien", "mutos_1216")

ZEILE = re.compile(r"^([0-9A-F]{4})  (?:[0-9A-F]{2,4} ?)+ +(\S+)")

def befehlsanfaenge(karte, bild):
    """Befehlsanfaenge laut Listing: Zeilen, deren Mnemonik weder DB noch DW ist."""
    aus = subprocess.run([Z8KDIS, "--karte", karte, bild], capture_output=True, text=True, check=True).stdout
    s = set()
    for z in aus.splitlines():
        m = ZEILE.match(z)
        if m and m.group(2) not in ("DB", "DW"):
            s.add(int(m.group(1), 16))
    return s

def schreibe(pfad, eintritte):
    basis = open(BASIS).read().rstrip("\n")
    zeilen = [basis, "# ---- erzeugt von tools/mutos8000/kernel/lauf/abdeckung_zu_karte.py aus abdeckung_gesamt.txt (%d Einstiege) ----" % len(eintritte)]
    e = sorted(eintritte)
    for i in range(0, len(e), 12):
        zeilen.append("code " + " ".join("%04X" % a for a in e[i:i + 12]))
    open(pfad, "w").write("\n".join(zeilen) + "\n")

def main():
    bild = abbild()
    treffer = {int(z, 16) for z in open(os.path.join(HIER, "abdeckung_gesamt.txt")) if z.strip()}
    eintritte = set()
    tmp = os.path.join(tempfile.mkdtemp(prefix="karte_"), "k.txt")
    schreibe(tmp, eintritte)
    runde = 0
    while True:
        gelaufen = befehlsanfaenge(tmp, bild)
        fehlt = sorted(treffer - gelaufen)
        if not fehlt:
            break
        neu = []
        letzt = -99
        for a in fehlt:               # nur den ersten jeder Folge nehmen; der Rest folgt meist der Verfolgung
            if a - letzt > 8:
                neu.append(a)
            letzt = a
        if runde > 60 or all(a in eintritte for a in neu):
            # Verfolgung erreicht den Befehl nicht (Ueberlappung): jeden fehlenden einzeln eintragen
            neu = fehlt
            if all(a in eintritte for a in neu):
                print("nicht erreichbar:", " ".join("%04X" % a for a in fehlt))
                break
        eintritte.update(neu)
        schreibe(tmp, eintritte)
        runde += 1
    # Rueckwaerts verkleinern: Einstiege, die ohnehin erreicht werden, weglassen
    for a in sorted(eintritte, reverse=True):
        probe = eintritte - {a}
        schreibe(tmp, probe)
        if treffer <= befehlsanfaenge(tmp, bild):
            eintritte = probe
    schreibe(ZIEL, eintritte)
    print("Einstiege:", len(eintritte), " ausgefuehrte Befehlsanfaenge:", len(treffer), " Runden:", runde)

if __name__ == "__main__":
    main()

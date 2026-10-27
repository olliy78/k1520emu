"""Sucht die Funktionen der MUTOS-libc.a (maskierte Text-Bytes) in einem Abbild, z.B. dem Kernel
`mutos_1216` oder einem gelinkten Programm (bin/ld als Gegenprobe).  AP K2.

    python3 -I finde_signaturen.py liste  <libc.a>                 # Signaturliste (Name Laenge Wildcards Hash)
    python3 -I finde_signaturen.py suche  <libc.a> <abbild> [--min 24] [--basis 0]

Relokationsfelder (Wortadressen von Text/Data/BSS/Extern) sind Wildcards.  Treffer unter --min Byte
Text sind als schwach markiert (z.B. exit.o, fakcu.o).  Adresse = Dateiversatz + --basis (Kernel: Basis 0
vermutet, denn die Vektortabelle liegt am Dateianfang).
"""
import hashlib
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import objformat as O  # noqa: E402


def signaturen(archiv_bytes):
    """Liefert [(name, text_bytes, maske, wildcardwoerter, hash)] je Mitglied mit nichtleerem Text."""
    erg = []
    for m in O.Archiv.lesen(archiv_bytes).mitglieder:
        o = m.objekt()
        if not o.text:
            continue
        t, mask = o.maskierter_text()
        h = hashlib.sha1(t + mask).hexdigest()[:16]
        erg.append((m.name.decode("latin-1"), t, mask, sum(mask) // 2, h))
    return erg


def liste(archiv_bytes):
    z = ["# name text_bytes wildcard_worte sha1(maskiert+maske)[:16]"]
    for n, t, _m, w, h in signaturen(archiv_bytes):
        z.append("%-12s %5d %4d %s" % (n, len(t), w, h))
    return "\n".join(z) + "\n"


def muster(t, mask):
    teile, i = [], 0
    while i < len(t):
        if mask[i]:
            teile.append(b"..")
            i += 2
        else:
            j = i
            while j < len(t) and not mask[j]:
                j += 1
            teile.append(re.escape(t[i:j]))
            i = j
    return re.compile(b"".join(teile), re.S)


def suche(archiv_bytes, abbild, minlen=24):
    """Liefert [(name, laenge, [versaetze], schwach)]; nur wortausgerichtete Treffer zaehlen."""
    erg = []
    for n, t, mask, _w, _h in signaturen(archiv_bytes):
        p = muster(t, mask)
        treffer, pos = [], 0
        while True:
            m = p.search(abbild, pos)
            if not m:
                break
            if m.start() % 2 == 0:
                treffer.append(m.start())
            pos = m.start() + 1
        # ein Textstueck, das nur aus Wildcards/Nullen besteht, ist kein Beleg
        erg.append((n, len(t), treffer, len(t) < minlen or not any(t)))
    return erg


def laeufe(t, mask, minlaenge=6):
    """Maximale feste Laeufe (Versatz, Bytes) von mindestens minlaenge Bytes, wortausgerichtet."""
    erg, i = [], 0
    while i < len(t):
        if mask[i]:
            i += 2
            continue
        j = i
        while j < len(t) and not mask[j]:
            j += 2
        if j - i >= minlaenge:
            erg.append((i, t[i:j]))
        i = j
    return sorted(erg, key=lambda e: -len(e[1]))


def anker(t, mask, breite=6, anzahl=8):
    """Bis zu `anzahl` gleichmaessig verteilte feste Fenster von `breite` Bytes (Wortraster)."""
    fenster = [(i, t[i:i + breite]) for i in range(0, len(t) - breite + 1, 2)
               if not any(mask[i:i + breite]) and any(t[i:i + breite])]
    if len(fenster) <= anzahl:
        return fenster
    return [fenster[k * (len(fenster) - 1) // (anzahl - 1)] for k in range(anzahl)]


def aehnlich(archiv_bytes, abbild, schwelle=0.85, minlen=24):
    """Unscharfe Suche: Anker = verteilte feste 6-Byte-Fenster, danach Wortvergleich des ganzen Fensters.
    Findet gleiche Quelle mit leicht anderem Code (andere Register, andere Fassung).
    Liefert [(name, laenge, anteil, versatz)] fuer den besten Treffer je Mitglied >= schwelle."""
    erg = []
    for n, t, mask, _w, _h in signaturen(archiv_bytes):
        if len(t) < minlen:
            continue
        beste = None
        kandidaten = set()
        for off, b in anker(t, mask):
            pos = 0
            while True:
                q = abbild.find(b, pos)
                if q < 0:
                    break
                if (q - off) % 2 == 0 and q - off >= 0:
                    kandidaten.add(q - off)
                pos = q + 1
        for st in kandidaten:
            fenster = abbild[st:st + len(t)]
            if len(fenster) < len(t):
                continue
            fest = gleich = 0
            for i in range(0, len(t), 2):
                if not mask[i]:
                    fest += 1
                    gleich += fenster[i:i + 2] == t[i:i + 2]
            if fest and (beste is None or gleich / fest > beste[0]):
                beste = (gleich / fest, st)
        if beste and beste[0] >= schwelle:
            erg.append((n, len(t), beste[0], beste[1]))
    return erg


def main(argv):
    if len(argv) < 3 or argv[1] not in ("liste", "suche") or "--help" in argv:
        print("usage: finde_signaturen.py liste <libc.a> | suche <libc.a> <abbild> [--min N] [--basis N] [--aehnlich 0.85]")
        return 0 if "--help" in argv else 2
    ar = open(argv[2], "rb").read()
    if argv[1] == "liste":
        sys.stdout.write(liste(ar))
        return 0
    img = open(argv[3], "rb").read()
    mn = int(argv[argv.index("--min") + 1]) if "--min" in argv else 24
    basis = int(argv[argv.index("--basis") + 1], 0) if "--basis" in argv else 0
    if "--aehnlich" in argv:
        sw = float(argv[argv.index("--aehnlich") + 1])
        for n, ln, a, st in sorted(aehnlich(ar, img, sw, mn), key=lambda e: e[3]):
            print("%-12s %5d B  %04x  %3d%%" % (n, ln, basis + st, round(100 * a)))
        return 0
    for n, ln, tr, schwach in suche(ar, img, mn):
        if tr:
            print("%-12s %5d B  %s%s" % (n, ln, " ".join("%04x" % (basis + x) for x in tr[:8]),
                                         "  (schwach)" if schwach else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

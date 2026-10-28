"""Rundlauf Kernel -> z8kdis (as-Quelle) -> Gast-`as` -> Bytevergleich (AP K4, Entwurf 41).

Beweis, dass die Quellform von `z8kdis --format as` mit dem ORIGINAL-`as` des MUTOS (PLZ/ASM) zu den Originalbytes
zurueckfuehrt.  Ablauf (Linux-Seite):
  1. mutos_1216 aus dem Ladedisketten-Archiv holen (zip, kein Entpacken in den Baum),
  2. `build/z8kdis --karte karte.txt --teile` / `--format as --teil` -> Stuecke ohne Marken ueber Stueckgrenzen
     (Marken ausserhalb = absolute Konstanten, relative Spruenge liegen nie quer),
  3. Quelldiskette (mutos8k_26x128_mfm) bauen, Gast fahren (gast_as.py, k1520dbg-Stapel), Objekte zurueckholen,
  4. Textteil jedes Objekts (objformat.py) zusammensetzen und gegen das Original vergleichen.

Aufruf:   python3 -I tools/mutos8000/kernel/rundlauf_gast.py [--arbeit DIR] [--teile N] [--nur-quelle]
Exit:     0 = bytegleich, 1 = Abweichung/as-Fehler, 77 = Hilfsmittel fehlen (kein Emulator/Diskette/Archiv).
Rechenzeit: Minuten bis Stunden (Gast-`as` laeuft im Emulator); deshalb Hintergrund.  Zwischenergebnisse liegen im
Arbeitsordner (lauf.log, zurueck/*.err), karte.txt wird nur GELESEN.
"""
import argparse
import os
import struct
import subprocess
import sys
import tempfile
import zipfile

HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HIER)
import gast_as          # noqa: E402
import objformat        # noqa: E402

WURZEL = gast_as.WURZEL
ARCHIV = os.path.join(WURZEL, "disks", "a5120_mutos8000", "a5120_mutos8000_boot_udos.zip")
KARTE = os.path.join(HIER, "karte.txt")
Z8KDIS = os.path.join(WURZEL, "build", "z8kdis")
KERNEL_GROESSE = 36352


def kernel_holen():
    with zipfile.ZipFile(ARCHIV) as z:
        return z.read("dateien/mutos_1216")


def z8kdis(args):
    r = subprocess.run([Z8KDIS, "--karte", KARTE] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if r.returncode:
        raise RuntimeError("z8kdis: " + r.stderr.decode("latin-1"))
    return r.stdout.decode("latin-1")


def teile_erzeugen(abbild, arbeit, max_teil):
    """Quellen je Stueck: {name: (von, bis, quelltext)}.  Schnitte setzt z8kdis (nie quer ueber JR/DJNZ/CALR)."""
    abb = os.path.join(arbeit, "mutos_1216")
    with open(abb, "wb") as f:
        f.write(abbild)
    teile = {}
    for i, z in enumerate(z8kdis(["--teile", "%X" % max_teil, abb]).split("\n")):
        if not z.strip():
            continue
        von, bis = [int(x, 16) for x in z.split()]
        name = "kteil%02d" % i
        q = z8kdis(["--format", "as", "--modul", name, "--teil", "%X:%X" % (von, bis), abb])
        teile[name] = (von, bis, q)
    return teile


def objekt_text(obj, basis):
    """Textbytes eines as-Objekts, auf die Ladeadresse `basis` gesetzt.

    Der as legt jedes Stueck ab 0 an; Marken im Stueck tragen ein Textrelokationswort (Art 1) -- so wie `ld` sie
    mit seiner Textbasis verschieben wuerde, addieren wir hier `basis`.  Alles andere (Konstanten = Art 0) bleibt."""
    kopf = struct.unpack(">8H", obj[:16])
    magic, text, data = kopf[0], kopf[1], kopf[2]
    if magic not in objformat.A_MAGICS:
        raise ValueError("kein a.out: magic %04x" % magic)
    if data:
        raise ValueError("Datenabschnitt %d B (unerwartet)" % data)
    t = bytearray(obj[16:16 + text])
    rel = obj[16 + text + data:16 + text + data + text]
    fremd = 0
    for i in range(text // 2):
        r = struct.unpack(">H", rel[2 * i:2 * i + 2])[0]
        art = (r >> 1) & 7
        if art == objformat.R_TEXT:
            w = (struct.unpack(">H", t[2 * i:2 * i + 2])[0] + basis) & 0xFFFF
            t[2 * i:2 * i + 2] = struct.pack(">H", w)
        elif art != objformat.R_ABS:
            fremd += 1
    if fremd:
        raise ValueError("%d Relokationen anderer Art (extern/Daten/BSS)" % fremd)
    return bytes(t)


def vergleiche(soll, ist):
    if soll == ist:
        return []
    d = [i for i in range(min(len(soll), len(ist))) if soll[i] != ist[i]]
    return d


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--arbeit", help="Arbeitsordner (Vorgabe: temporaer)")
    ap.add_argument("--nur-quelle", action="store_true", help="nur die Quelle erzeugen und den Gast nicht fahren")
    ap.add_argument("--max-teil", type=lambda x: int(x, 0), default=0x1000, help="Zielgroesse je Stueck in Byte")
    ap.add_argument("--stapel-bytes", type=int, default=150_000, help="Quelltext je Gastlauf (Quelldiskette)")
    ap.add_argument("--frist", type=int, default=20_000_000_000, help="Maschinentakte je Teil bis zur Marke")
    a = ap.parse_args(argv)
    for p in (ARCHIV, KARTE, Z8KDIS):
        if not os.path.exists(p):
            print("Hilfsmittel fehlt:", p, file=sys.stderr)
            return 77
    try:
        gast_as.pruefe_hilfsmittel()
    except gast_as.Fehlt as e:
        print("Hilfsmittel fehlt:", e, file=sys.stderr)
        return 77
    arbeit = os.path.abspath(a.arbeit) if a.arbeit else tempfile.mkdtemp(prefix="rundlauf_")
    os.makedirs(arbeit, exist_ok=True)
    abbild = kernel_holen()
    assert len(abbild) == KERNEL_GROESSE, len(abbild)
    teile = teile_erzeugen(abbild, arbeit, a.max_teil)
    for n, (von, bis, q) in teile.items():
        with open(os.path.join(arbeit, n + ".s"), "w", newline="\n") as f:
            f.write(q)
    print("%d Stuecke, Quelle %d Byte" % (len(teile), sum(len(q) for _, _, q in teile.values())))
    if a.nur_quelle:
        return 0
    # Stapel: so viele Stuecke je Gastlauf, wie auf die Quelldiskette passen
    stapel, akt, groesse = [], [], 0
    for n, (_, _, q) in teile.items():
        if akt and groesse + len(q) > a.stapel_bytes:
            stapel.append(akt); akt, groesse = [], 0
        akt.append(n); groesse += len(q)
    if akt:
        stapel.append(akt)
    erg, dauer = {}, 0.0
    for i, namen in enumerate(stapel):
        aus = {}
        e = gast_as.gast_assemblieren(os.path.join(arbeit, "gast%d" % i), {n: teile[n][2] for n in namen}, aus,
                                      frist=a.frist)
        dauer += aus["dauer"]
        erg.update(e)
        print("Stapel %d: %s  (%.0f s Wanduhr)" % (i, " ".join(namen), aus["dauer"]))
    print("Gast-Laufzeit gesamt (Wanduhr): %.0f s" % dauer)
    ist, fehler = bytearray(KERNEL_GROESSE), 0
    for n, (von, bis, _) in teile.items():
        err, obj = erg[n]
        if err:
            print("%s: as-Meldungen:\n%s" % (n, err))
        if not obj:
            print("%s: KEIN Objekt" % n)
            fehler += 1
            continue
        try:
            t = objekt_text(obj, von)
        except ValueError as ex:
            print("%s: %s" % (n, ex))
            fehler += 1
            continue
        if len(t) != bis - von:
            print("%s: Laenge %d statt %d" % (n, len(t), bis - von))
            fehler += 1
        ist[von:von + len(t)] = t[:bis - von]
        d = vergleiche(abbild[von:bis], t)
        print("%s %04X-%04X: %s" % (n, von, bis, "gleich" if not d and len(t) == bis - von else
                                    "%d Bytes ungleich, erste bei %s" % (len(d), ", ".join("%04X" % (von + x) for x in d[:6]))))
        if d:
            fehler += 1
    gleich = sum(1 for x, y in zip(abbild, ist) if x == y)
    print("Gesamt: %d von %d Byte gleich" % (gleich, KERNEL_GROESSE))
    with open(os.path.join(arbeit, "mutos_1216.neu"), "wb") as f:
        f.write(ist)
    return 0 if not fehler and gleich == KERNEL_GROESSE else 1


if __name__ == "__main__":
    sys.exit(main())

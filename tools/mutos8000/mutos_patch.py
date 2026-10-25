#!/usr/bin/env python3
"""Patchliste fuer die Dateien `mutos_io` (U880-Treiber) und `mutos_1216` (Kernel) der MUTOS-Ladediskette.

Entwurf 39 Abschnitt 11.  Alle Patches sind in Abschnitt 11 mit Adresse, alt und neu beschrieben; dieses Skript
wendet sie auf eine DATEI an (Archiv `dateien/`), prueft vorher jedes Altbyte und bricht ab, wenn es nicht passt
(dann ist es eine andere Fassung oder schon gepatcht).  Es veraendert nie das Original, sondern schreibt `-o`.

    python3 -I tools/mutos8000/mutos_patch.py io-8x512 dateien/mutos_io -o mutos_io_8x512
    python3 -I tools/mutos8000/mutos_patch.py zdos-einheit0 dateien/ZDOS -o ZDOS_neu
    python3 -I tools/mutos8000/mutos_patch.py kern --root 0 --pipe 0 dateien/mutos_1216 -o mutos_1216_a
    python3 -I tools/mutos8000/mutos_patch.py kern --swap 1 --swplo 0 --nswap 608 dateien/mutos_1216 -o k

Nur Python-Standardbibliothek.
"""
import argparse
import sys

# --- mutos_io: Adresse im U880-Speicher -> Dateioffset (Segmente 9000+022D | 9400+0EB0 | A400+01A8 | C8B6+00B2,
#     jedes Segment auf 256 B aufgerundet abgelegt: 0x000, 0x300, 0x1200, 0x1400) ---------------------------------


def io_off(adr):
    if 0x9000 <= adr < 0x922D:
        return adr - 0x9000
    if 0x9400 <= adr < 0xA2B0:
        return 0x300 + adr - 0x9400
    raise ValueError("Adresse %04X nicht in den Segmenten 9000/9400" % adr)


# Neuer Block ab 9850 (49 Byte): Block -> (Zylinder, Sektor) fuer 8 Sektoren zu 512 B, linear:
#   pos = bn & 7 (996D), rel = bn >> 3 (996C), Versatz (996B) = 0 (unbenutzt)
_NEU_9850 = bytes.fromhex(
    "7D E6 07 32 6D 99"            # LD A,L / AND 07H / LD (996DH),A
    "CB 3C CB 1D CB 3C CB 1D CB 3C CB 1D"  # HL >>= 3
    "7D 32 6C 99"                  # LD A,L / LD (996CH),A
    "AF 32 6B 99"                  # XOR A / LD (996BH),A
) + bytes(49 - 26)
_ALT_9850 = bytes.fromhex(
    "29 29 06 08 11 00 0D AF ED 52 30 02 19 37 3F CB 15 CB 14 10 F2 7D 32 6C 99 7C 32 6D 99 5D AF 67 57 19 19 29 "
    "11 1A 00 AF ED 52 30 FB 19 7D 32 6B 99")

# (Adresse, alt, neu, Begruendung)
IO_8X512 = [
    (0x919A, bytes.fromhex("3A 03 00 F6 01"), bytes.fromhex("3E 04 00 00 00"),
     "Init: [0003H] (Sektorlaenge/128, global im Nukleus) = 4 statt 'OR 01'"),
    (0x9828, bytes.fromhex("80 00"), bytes.fromhex("00 02"), "Auftragslaenge je Nukleus-Aufruf 0200H statt 0080H"),
    (0x98B4, bytes.fromhex("80 00"), bytes.fromhex("00 02"), "Fortschritt je Aufruf 0200H statt 0080H"),
    (0x9850, _ALT_9850, _NEU_9850, "Blocknummer -> Zylinder = 1 + bn/8, Sektor = bn mod 8 (statt /26, Verschr. 2, Versatz 6)"),
    (0x9920, bytes.fromhex("1A"), bytes.fromhex("08"), "Weiterschalten: 8 statt 26 Sektoren je Spur"),
    (0x9924, bytes.fromhex("1A"), bytes.fromhex("08"), "Ueberlauf: 8 statt 26 abziehen"),
    (0x993F, bytes.fromhex("3A 6B 99 57 3A 6D"), bytes.fromhex("3A 6D 99 C3 55 99"),
     "Sektor = pos (ohne Verschraenkung/Versatz), weiter bei 9955H"),
]
IO_TEXT_RK0 = [(0xA0C9, b"2", b"0", "Meldetext 'insert ... root - disk on rk2' -> rk0")]

# --- ZDOS (UDOS-Kommandoprozessor, Datei `ZDOS`, Ladeadresse 2600H): Dateioffset = Adresse - 2600H ---------------
# Anmelden der Einheiten beim Start (2849..2888) und Dateisuche "DO OS.INIT" ueber alle Einheiten (329A..32B8)
ZDOS_NUR_EINHEIT0 = [
    (0x287D - 0x2600, bytes.fromhex("C6 20"), bytes.fromhex("37 00"),
     "287D: ADD A,20H -> SCF / NOP: die Anmelde-Schleife (Einheiten 0..7, liest Zyl. 23) endet nach Einheit 0"),
    (0x329B - 0x2600, bytes.fromhex("01"), bytes.fromhex("00"),
     "329B: LD A,01H -> LD A,00H: die Dateisuche ohne Laufwerksangabe beginnt bei 0 und endet vor 1"),
]

# --- mutos_1216: Offset = Adresse (flaches Abbild ab 0) ---------------------------------------------------
KERN = {
    "rootdev":  (0x887D, 1, 0x02),   # Minor-Byte; Major (0x887C) = 4
    "swapdev":  (0x887F, 1, 0x02),
    "pipedev":  (0x8881, 1, 0x02),
    "minormax": (0x6AF1, 1, 0x04),   # rkopen: Minor >= Wert -> ENXIO
    "nblocks":  (0x6B16, 2, 0x0260),  # rkstrategy: Blocknummer >= Wert -> Fehler (608 = 76 * 8)
    "swplo":    (0x8884, 4, 0xFA),
    "nswap":    (0x8888, 2, 0xF4),
}


def lies(d, off, n):
    return int.from_bytes(d[off:off + n], "big")


def setze(d, off, n, wert, alt, name, log):
    ist = lies(d, off, n)
    if ist != alt:
        sys.exit("%s: bei %04X steht %0*X, erwartet %0*X (andere Fassung oder schon gepatcht)" % (name, off, 2 * n, ist, 2 * n, alt))
    d[off:off + n] = wert.to_bytes(n, "big")
    log.append("%-9s %04X  %0*X -> %0*X" % (name, off, 2 * n, alt, 2 * n, wert))


def patch_io(d, liste, log):
    for adr, alt, neu, grund in liste:
        o = io_off(adr)
        if bytes(d[o:o + len(alt)]) != alt:
            sys.exit("mutos_io %04X (Offset %04X): Altbytes %s statt %s" % (adr, o, bytes(d[o:o + len(alt)]).hex(" "), alt.hex(" ")))
        d[o:o + len(neu)] = neu
        log.append("io %04X (Offset %04X) %s -> %s  %s" % (adr, o, alt.hex(" ")[:23], neu.hex(" ")[:23], grund))


def patch_zdos(d, log):
    for off, alt, neu, grund in ZDOS_NUR_EINHEIT0:
        if bytes(d[off:off + len(alt)]) != alt:
            sys.exit("ZDOS Offset %04X: Altbytes %s statt %s" % (off, bytes(d[off:off + len(alt)]).hex(" "), alt.hex(" ")))
        d[off:off + len(neu)] = neu
        log.append("zdos %04X %s -> %s  %s" % (off, alt.hex(" "), neu.hex(" "), grund))


def patch_kern(d, werte, log):
    """werte: dict aus root/swap/pipe (Minor 0..3), swplo, nswap, nblocks (None = nicht anfassen)."""
    for name, key in (("root", "rootdev"), ("swap", "swapdev"), ("pipe", "pipedev")):
        v = werte.get(name)
        if v is not None:
            if not 0 <= v <= 3:
                sys.exit("--%s: Minor 0..3" % name)
            off, n, alt = KERN[key]
            setze(d, off, n, v, alt, key, log)
    for key in ("swplo", "nswap", "nblocks"):
        v = werte.get(key)
        if v is not None:
            off, n, alt = KERN[key]
            setze(d, off, n, v, alt, key, log)
    if not log:
        sys.exit("nichts zu tun: mindestens eine Option angeben")


def anwenden(art, daten, optionen=None):
    """Bibliotheksaufruf (bau_ladediskette.py): Patch `art` (io-8x512 | zdos-einheit0 | kern) auf `daten` (bytes).

    Ergebnis (neue bytes, Protokollzeilen).  Gleiche Pruefungen wie auf der Kommandozeile: jedes Altbyte muss
    passen, sonst Abbruch (SystemExit) und es entsteht nichts."""
    d = bytearray(daten)
    log = []
    optionen = optionen or {}
    if art == "io-8x512":
        patch_io(d, IO_8X512 + (IO_TEXT_RK0 if optionen.get("text_rk0") else []), log)
    elif art == "zdos-einheit0":
        patch_zdos(d, log)
    elif art == "kern":
        patch_kern(d, optionen, log)
    else:
        sys.exit("unbekannter Patch: %s" % art)
    return bytes(d), log


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="art", required=True)
    p = sub.add_parser("io-8x512", help="mutos_io fuer 8 x 512 B (DISK-Satz) statt 26 x 128 B")
    p.add_argument("datei")
    p.add_argument("-o", "--aus", required=True)
    p.add_argument("--text-rk0", action="store_true", help="Meldetext 'on rk2' -> 'on rk0' (nur mit Kernel --root 0)")
    z = sub.add_parser("zdos-einheit0", help="ZDOS: UDOS meldet beim Start nur Laufwerk A: an (fremde Disketten in B:-D: stoeren nicht)")
    z.add_argument("datei")
    z.add_argument("-o", "--aus", required=True)
    k = sub.add_parser("kern", help="mutos_1216: Wurzel-/Swap-/Pipe-Geraet (Minor), Swap-Lage, Grenzen")
    k.add_argument("datei")
    k.add_argument("-o", "--aus", required=True)
    for n in ("root", "swap", "pipe"):
        k.add_argument("--" + n, type=int, help="Minor 0..3 von %sdev (Major bleibt 4)" % n)
    k.add_argument("--swplo", type=int, help="erster Swap-Block (daddr_t)")
    k.add_argument("--nswap", type=int, help="Zahl der Swap-Bloecke")
    k.add_argument("--nblocks", type=int, help="Blockgrenze des rk-Treibers (608 = 8 x 512)")
    a = ap.parse_args(argv)
    opt = {}
    if a.art == "io-8x512":
        opt["text_rk0"] = a.text_rk0
    elif a.art == "kern":
        opt = {n: getattr(a, n) for n in ("root", "swap", "pipe", "swplo", "nswap", "nblocks")}
    d, log = anwenden(a.art, open(a.datei, "rb").read(), opt)
    open(a.aus, "wb").write(d)
    print("\n".join(log))
    print("geschrieben:", a.aus)


if __name__ == "__main__":
    main()

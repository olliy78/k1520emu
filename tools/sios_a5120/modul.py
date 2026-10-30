#!/usr/bin/env python3
"""
Generiermodule der SIOS-1526-Datei SYSORG09 (Diskette SIOS1526_SG1.hfe) herausloesen und auflisten.
Format: doc/sios/module_k7633_kut.md (Abschnitt 1).  Nur lesend am Abbild.

  modul.py liste  [abbild]                       Verzeichnis (Satz 0): Name, Saetze, Position, Kopfbytes
  modul.py get    NAME out.bin [abbild]          Modulsaetze als Rohbytes (n x 1024 B, mit Kopf und CRC)
  modul.py pruefen NAME [abbild]                 CRC-CCITT (FFFFH) je Satz gegen die letzten 2 Byte
  modul.py prn    NAME out.prn --org 2000H [--abbild X]   kommentiertes Listing (Namen/Kommentare unten)

Modulsatz (1024 B): Byte 0 = Gruppe<<4 | Laufnummer, Byte 1 = 80H | 10H (weiterer Satz folgt) | Variante,
Byte 2..1021 Code/Daten, Byte 1022/1023 = CRC-CCITT (Start FFFFH, big-endian) ueber Byte 0..1021.
Der Code ist positionsunabhaengig (Seitenbasis in D beim Aufruf von Block+2, relative CALL/JP ueber
002BH/003BH); die Saetze der Module liegen im System (SYSARE11/09) bitgleich als 1-KB-Bloecke, nur
von SGEN gepatchte Stellen (Tabellenzeiger) und die CRC unterscheiden sich.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, HERE)
sys.setrecursionlimit(100000)
import dump  # noqa: E402

ABBILD = os.path.join(HERE, "original", "SIOS1526_SG1.hfe")
SATZ = 1024


def crc_ccitt(b):
    c = 0xFFFF
    for x in b:
        c ^= x << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def sysorg(abbild=ABBILD):
    ab = dump.Abbild(abbild)
    e = [x for x in dump.etiketten(ab) if x["name"] == "SYSORG09"][0]
    return dump.datei_bytes(ab, e)


def verzeichnis(d):
    """Liste (Name, Anzahl, Position) aus Satz 0 (12-Byte-Eintraege bis FF)."""
    res = []
    for i in range(0, SATZ, 12):
        n = d[i:i + 12]
        if n[0] == 0xFF:
            break
        res.append((n[:8].decode("ascii").rstrip(), int(n[8:10]), n[10] | n[11] << 8))
    return res


def modul(d, name):
    for n, c, p in verzeichnis(d):
        if n == name:
            return d[(p - 1) * SATZ:(p - 1 + c) * SATZ]
    raise SystemExit(f"Modul {name!r} nicht im Verzeichnis")


# --- Namen / Kommentare der ausgewerteten Module -------------------------------------------
import modul_text  # noqa: E402  (Namen und Kommentare, getrennt gehalten)

from gen_prn import SiosDis  # noqa: E402


def ende(m):
    return m in ("RET", "RETI", "RETN", "JP (HL)", "JP (IX)", "JP (IY)") or \
        (m.startswith(("JP ", "JR ")) and "," not in m)


def abdecken(dis, mem, org, seeds):
    for a in seeds:
        dis.recursive_decode(a)
    tent = set()
    for _ in range(6):
        neu = 0
        a = org + 2
        z = org + len(mem)
        while a < z - 2:
            if a in dis.instructions:
                a += dis.instructions[a][1]
                continue
            b = mem[a - org]
            vor = [x for x in range(a - 4, a) if x in dis.instructions
                   and x + dis.instructions[x][1] == a]
            if b not in (0x00, 0xFF, 0xE5) and vor and ende(dis.instructions[vor[-1]][0]):
                probe = SiosDis(mem, org=org)
                x, n, ok = a, 0, False
                while x < z - 2 and n < 400:
                    m, ln, t, e = probe.decode_one(x)
                    if ln == 0 or m.startswith("DB ") or x in dis.code_addrs:
                        break
                    n += 1
                    x += ln
                    if e or ende(m):
                        ok = n >= 3
                        break
                if ok:
                    dis.recursive_decode(a)
                    tent.add(a)
                    neu += 1
            a += 1
        if not neu:
            break
    return tent


def prn(d, name, org, out):
    info = modul_text.MODULE[name]
    mem = modul(d, name)
    dis = SiosDis(mem, org=org)
    dis.auto_kom = {}
    seeds = [org + o for o in info["einstiege"]]
    tent = abdecken(dis, mem, org, seeds)
    namen = {org + o: n for o, n in info["namen"].items()}
    kom = {org + o: t for o, t in info["kommentar"].items()}
    ende_ = org + len(mem)
    z = [info["kopf"].replace("{ORG}", f"{org:04X}H"), "", f"\tORG\t{org:04X}H", ""]
    a = org
    while a < ende_:
        if a in namen:
            z.append(f"{namen[a]}:")
        if (a - org) % SATZ == 0:
            z.append(f"{a:04X}  {mem[a - org]:02X} {mem[a - org + 1]:02X}\t\tDB\t{mem[a - org]:02X}H,{mem[a - org + 1]:02X}H\t\t;Satzkopf: Gruppe {mem[a - org] >> 4}, "
                     f"Lauf {mem[a - org] & 15}, Flags/Variante {mem[a - org + 1]:02X}H")
            a += 2
            continue
        if (a - org) % SATZ == SATZ - 2:
            z.append(f"{a:04X}  {mem[a - org]:02X} {mem[a - org + 1]:02X}\t\tDB\t{mem[a - org]:02X}H,{mem[a - org + 1]:02X}H\t\t;CRC-CCITT")
            a += 2
            continue
        if a in dis.instructions:
            m, ln, raw = dis.instructions[a]
            hexb = " ".join(f"{b:02X}" for b in raw)
            k = kom.get(a, "") or dis.auto_kom.get(a, "")
            if a in tent:
                k = (k + " " if k else "") + "[vermutet]"
            z.append(f"{a:04X}  {hexb:<14}\t{m}" + (f"\t\t;{k}" if k else ""))
            a += ln
            continue
        e = a + 1
        while (e < ende_ and e not in dis.instructions and e - a < 16 and e not in namen and e not in kom
               and (e - org) % SATZ not in (0, SATZ - 2)):
            e += 1
        stk = mem[a - org:e - org]
        db = ",".join(f"{b:02X}H" for b in stk)
        z.append(f"{a:04X}  {'':<14}\tDB\t{db}" + (f"\t\t;{kom[a]}" if a in kom else ""))
        a = e
    z += ["", "\tEND"]
    open(out, "w", encoding="utf-8").write("\n".join(z) + "\n")
    print(f"geschrieben: {out} ({len(z)} Zeilen, {len(tent)} vermutete Einstiege)")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    sub = ap.add_subparsers(dest="cmd", required=True)
    for n in ("liste", "get", "pruefen", "prn"):
        p = sub.add_parser(n)
        if n != "liste":
            p.add_argument("name")
        if n in ("get", "prn"):
            p.add_argument("out")
        if n == "prn":
            p.add_argument("--org", type=lambda x: int(x.rstrip("Hh"), 16), required=True)
        p.add_argument("--abbild", default=ABBILD)
    a = ap.parse_args()
    d = sysorg(a.abbild)
    if a.cmd == "liste":
        for n, c, p in verzeichnis(d):
            k = d[(p - 1) * SATZ:(p - 1) * SATZ + 2]
            print(f"{n:<8} Saetze {c}  Position {p:3d}  Kopf {k[0]:02X} {k[1]:02X}")
    elif a.cmd == "get":
        open(a.out, "wb").write(modul(d, a.name))
    elif a.cmd == "pruefen":
        m = modul(d, a.name)
        for i in range(len(m) // SATZ):
            s = m[i * SATZ:(i + 1) * SATZ]
            soll = s[1022] << 8 | s[1023]
            print(f"Satz {i}: Kopf {s[0]:02X} {s[1]:02X} CRC {crc_ccitt(s[:1022]):04X} soll {soll:04X} "
                  f"{'ok' if crc_ccitt(s[:1022]) == soll else 'FALSCH'}")
    else:
        prn(d, a.name, a.org, a.out)


if __name__ == "__main__":
    main()

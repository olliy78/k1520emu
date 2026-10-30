#!/usr/bin/env python3
"""
Sektordump und Etikettenleser fuer SIOS-1526-Disketten (IBM-3740-artig, 26 x 128 B MFM,
Spur 0 = Etikettenspur, Text in DKOI ~ EBCDIC cp500).  Siehe doc/design/43_sios1526.md Abschnitt 1.1.

Aufrufe (alle nur lesend am Abbild):
  dump.py bild   <abbild> <out.bin> [--kopf 0]   lineares Abbild, 26*128 B je Spur
  dump.py labels <abbild>                        Etikettenspur: VOL1 / HDR1 / DDR1
  dump.py get    <abbild> <NAME> <out.bin>       HDR1-Datei als Binaerdatei (BOE..EOE)

Etiketten (Spur 0, Sektor 7 = VOL1, Sektor 8..26 = HDR1/DDR1):
  Name 5-12, Satzlaenge 22-26, BOE 28-32 (TTSSS), EOE 34-38, Kennzeichen 40-49, EOD 74-78
  (Positionen 0-basiert, Endstelle eingeschlossen; Position 4 ist ein Leerzeichen).
BOE/EOE sind Spur (2 Stellen) + Sektor (3 Stellen); die Datei liegt unfragmentiert dazwischen.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "app"))
from core_binding.k1520disk import DiskTool  # noqa: E402

SPT = 26      # Sektoren je Spur
SEC = 128     # Bytes je Sektor


def ebcdic(b):
    """DKOI wie cp500 lesen; nicht darstellbares als '.'."""
    s = bytes(b).decode("cp500", errors="replace")
    return "".join(ch if ch.isprintable() else "." for ch in s)


class Abbild:
    def __init__(self, pfad, kopf=0):
        self.disk = DiskTool.open_raw(pfad)
        self.kopf = kopf
        self.zylinder = self.disk.cylinders if hasattr(self.disk, "cylinders") else 80
        self._spuren = {}

    def spur(self, z):
        """{Sektor-ID: (Daten, CRC ok, geloescht)} einer Spur; erste Fassung je ID gilt."""
        if z not in self._spuren:
            tr = self.disk.track(z, self.kopf)
            d = {}
            for sp in tr.spans:
                if sp.is_sector and sp.id not in d and sp.cyl == z and sp.head == self.kopf:
                    d[sp.id] = (self.disk.sector_data(z, self.kopf, sp.index), sp.data_crc_ok, sp.deleted)
            self._spuren[z] = d
        return self._spuren[z]

    def sektor(self, z, s):
        e = self.spur(z).get(s)
        return e[0][:SEC].ljust(SEC, b"\0") if e else b"\0" * SEC

    def linear(self, z0=0, z1=None):
        z1 = self.zylinder if z1 is None else z1
        return b"".join(self.sektor(z, s) for z in range(z0, z1) for s in range(1, SPT + 1))


def etiketten(ab):
    """Liste der Etiketten aus Spur 0, Sektor 7..26 als dicts."""
    res = []
    for s in range(7, SPT + 1):
        roh = ab.sektor(0, s)
        t = ebcdic(roh)
        kennung = t[:4]
        if kennung not in ("VOL1", "HDR1", "DDR1"):
            continue
        e = {"sektor": s, "kennung": kennung, "text": t, "roh": roh,
             "geloescht": ab.spur(0).get(s, (0, 0, False))[2]}
        if kennung == "VOL1":
            e["name"] = t[4:10].strip()
        else:
            e["name"] = t[5:13].strip()
            e["satzlaenge"] = t[22:27]
            e["boe"] = t[28:33]
            e["eoe"] = t[34:39]
            e["kennz"] = t[40:50]
            e["eod"] = t[74:79]
        res.append(e)
    return res


def ttsss(txt):
    """'TTSSS' -> (Spur, Sektor) oder None."""
    if len(txt) == 5 and txt.isdigit():
        return int(txt[:2]), int(txt[2:])
    return None


def datei_bytes(ab, e):
    """Bytes der Datei von BOE bis EOE (einschliesslich) in Sektorreihenfolge."""
    a, z = ttsss(e["boe"]), ttsss(e["eoe"])
    if not a or not z:
        raise ValueError(f"{e['name']}: BOE/EOE nicht lesbar ({e['boe']!r}/{e['eoe']!r})")
    out = bytearray()
    t, s = a
    while (t, s) <= z:
        out += ab.sektor(t, s)
        s += 1
        if s > SPT:
            t, s = t + 1, 1
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("bild")
    p.add_argument("abbild")
    p.add_argument("out")
    p.add_argument("--kopf", type=int, default=0)
    p = sub.add_parser("labels")
    p.add_argument("abbild")
    p = sub.add_parser("get")
    p.add_argument("abbild")
    p.add_argument("name")
    p.add_argument("out")
    a = ap.parse_args()

    ab = Abbild(a.abbild, getattr(a, "kopf", 0))
    if a.cmd == "bild":
        open(a.out, "wb").write(ab.linear())
    elif a.cmd == "labels":
        for e in etiketten(ab):
            if e["kennung"] == "VOL1":
                print(f"S{e['sektor']:2d} VOL1 {e['name']}")
            else:
                print(f"S{e['sektor']:2d} {e['kennung']} {e['name']:<8} satz={e['satzlaenge']} "
                      f"BOE={e['boe']} EOE={e['eoe']} kennz={e['kennz']!r} EOD={e['eod']}"
                      f"{'  (Marke F8)' if e['geloescht'] else ''}")
    else:
        for e in etiketten(ab):
            if e["kennung"] == "HDR1" and e["name"] == a.name:
                d = datei_bytes(ab, e)
                open(a.out, "wb").write(d)
                print(f"{a.name}: {len(d)} Byte (BOE {e['boe']} .. EOE {e['eoe']}) -> {a.out}")
                return
        sys.exit(f"keine HDR1-Datei {a.name!r}")


if __name__ == "__main__":
    main()

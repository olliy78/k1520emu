"""Testhilfe: liest und baut ImageDisk-Abbilder (.imd) in Python.

UNABHAENGIG vom C++-Kern geschrieben, allein nach der Clean-Room-Beschreibung
``doc/design/38a_td0_imd_formatbeschreibung.md`` Teil 1 -- Gegenstueck zu
``tests/unit/peripherals/test_imd_codec.cpp``.  Wer hier etwas "angleicht", weil der
Kern es anders macht, zerstoert die Gegenprobe: bei Abweichung zuerst die Beschreibung
und den Kern pruefen.

Ein Abbild ist ein Dict ``{"kopf": bytes, "tracks": [Track, ...]}``; eine Spur:
``{"mode", "cyl", "head", "n", "sectors": [Sektor, ...]}`` mit Sektoren
``{"c", "h", "r", "typ", "data"}`` (``typ`` 0..8, ``data`` = volle Sektorgroesse oder
``b""`` bei Typ 0).
"""

TYP_KOMPRIMIERT = 1   # (typ - 1) & 1
TYP_GELOESCHT = 2     # (typ - 1) & 2
TYP_FEHLER = 4        # (typ - 1) & 4

# Modus -> (Verfahren, Kanalrate kbit/s)
MODI = {0: ("fm", 500), 1: ("fm", 300), 2: ("fm", 250),
        3: ("mfm", 500), 4: ("mfm", 300), 5: ("mfm", 250)}


class ImdFehler(ValueError):
    pass


def lese(dat: bytes) -> dict:
    if dat[:4] != b"IMD ":
        raise ImdFehler("Kennung")
    ende = dat.find(b"\x1a")
    if ende < 0:
        raise ImdFehler("kein 0x1A")
    kopf = dat[:ende]
    i = ende + 1
    tracks = {}
    reihenfolge = []
    while len(dat) - i >= 5:            # ein Rest unter 5 Byte wird ignoriert
        mode, cyl, hb, nsec, sc = dat[i:i + 5]
        i += 5
        if mode not in MODI:
            raise ImdFehler("Modus %d" % mode)
        if sc > 6:
            raise ImdFehler("Sektorcode %d" % sc)
        head = hb & 0x3F
        if head > 1:
            raise ImdFehler("Kopf %d" % head)
        groesse = 128 << sc
        rmap = dat[i:i + nsec]
        i += nsec
        cmap = hmap = None
        if hb & 0x80:
            cmap = dat[i:i + nsec]
            i += nsec
        if hb & 0x40:
            hmap = dat[i:i + nsec]
            i += nsec
        if i > len(dat) or len(rmap) < nsec:
            raise ImdFehler("abgeschnitten")
        sektoren = []
        for k in range(nsec):
            if i >= len(dat):
                raise ImdFehler("abgeschnitten")
            typ = dat[i]
            i += 1
            if typ > 8:
                raise ImdFehler("Typ %d" % typ)
            if typ == 0:
                daten = b""
            elif (typ - 1) & TYP_KOMPRIMIERT:
                if i >= len(dat):
                    raise ImdFehler("abgeschnitten")
                daten = bytes([dat[i]]) * groesse
                i += 1
            else:
                daten = dat[i:i + groesse]
                if len(daten) < groesse:
                    raise ImdFehler("abgeschnitten")
                i += groesse
            sektoren.append({"c": cmap[k] if cmap is not None else cyl,
                             "h": hmap[k] if hmap is not None else head,
                             "r": rmap[k], "typ": typ, "data": daten})
        spur = {"mode": mode, "cyl": cyl, "head": head, "n": sc, "sectors": sektoren}
        if (cyl, head) not in tracks:
            reihenfolge.append((cyl, head))
        tracks[(cyl, head)] = spur           # der letzte Satz gilt
    return {"kopf": kopf, "tracks": [tracks[k] for k in reihenfolge]}


def baue(tracks, kommentar=b"Image file created by imd_bau",
         kopfzeile=b"IMD 1.17: 22/10/2026 12:00:00") -> bytes:
    """Schreibt Spuren im Format von :func:`lese`; Karten (Maps) nur wenn noetig."""
    out = bytearray(kopfzeile + b"\r\n" + kommentar + b"\r\n\x1a")
    for t in tracks:
        sek = t["sectors"]
        hb = t["head"]
        if any(s["c"] != t["cyl"] for s in sek):
            hb |= 0x80
        if any(s["h"] != t["head"] for s in sek):
            hb |= 0x40
        out += bytes([t["mode"], t["cyl"], hb, len(sek), t["n"] if sek else 0])
        out += bytes(s["r"] for s in sek)
        if hb & 0x80:
            out += bytes(s["c"] for s in sek)
        if hb & 0x40:
            out += bytes(s["h"] for s in sek)
        for s in sek:
            typ = s["typ"]
            out.append(typ)
            if typ == 0:
                continue
            if (typ - 1) & TYP_KOMPRIMIERT:
                out.append(s["data"][0])
            else:
                out += s["data"]
    return bytes(out)


def typ_aus_flags(daten: bytes, geloescht: bool, fehler: bool, ohne: bool) -> int:
    """Sektortyp nach 38a 1.7 (komprimiert, wenn alle Bytes dem ersten gleichen)."""
    if ohne:
        return 0
    komp = len(daten) > 0 and daten.count(daten[0]) == len(daten)
    return 1 + (1 if komp else 0) + (2 if geloescht else 0) + (4 if fehler else 0)

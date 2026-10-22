"""Referenzleser fuer ``.td0`` auf Basis von Greaseweazle (``greaseweazle.image.td0``, Unlicense).

``python3 td0_referenz.py datei.td0`` gibt auf stdout JSON aus::

    {"sektoren": [[cyl, head, c, h, r, groesse, fnv1a64_der_daten], ...]}   # Dateireihenfolge je Spur

oder ``{"fehler": "..."}``, wenn gw die Datei ablehnt.  Das Skript ist die Gegenprobe zum Kern
(``Td0Codec``), nicht Teil davon; es braucht ``greaseweazle`` und wird von
``test_td0_gegenprobe.py`` benutzt (uebersprungen, wenn das Paket fehlt).  Sektoren ohne
Datenblock liefert gw als Nullen (``flags & 0x30``); der Vergleich beruecksichtigt das.
"""

import contextlib
import io
import json
import sys


def fnv1a64(daten: bytes) -> str:
    h = 1469598103934665603
    for b in daten:
        h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return "%016x" % h


def lese(pfad: str) -> dict:
    # Reihenfolge der Importe: gw hat einen Importzyklus, der nur von codec.codec aus aufgeht
    import greaseweazle.codec.codec  # noqa: F401
    from greaseweazle.image.td0 import TD0
    from greaseweazle import error

    abb = TD0(pfad, None)
    with open(pfad, "rb") as f:
        dat = f.read()
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            abb.from_bytes(dat)
    except (error.Fatal, AssertionError, IndexError, Exception) as e:   # noqa: BLE001
        return {"fehler": "%s: %s" % (type(e).__name__, e)}
    sektoren = []
    for (cyl, head) in sorted(abb.to_track):
        t = abb.to_track[cyl, head]
        for s in t.sectors:
            sektoren.append([cyl, head, s.idam.c, s.idam.h, s.idam.r, len(s.dam.data),
                             fnv1a64(bytes(s.dam.data))])
    return {"sektoren": sektoren}


if __name__ == "__main__":
    json.dump(lese(sys.argv[1]), sys.stdout)

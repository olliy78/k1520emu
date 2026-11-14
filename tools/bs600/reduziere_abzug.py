# BS600-Disketten, die am echten Geraet als 80 Zylinder / 2 Koepfe gelesen wurden (Doppelschritt, gw read ohne step=2), auf das
# Fixture-Format reduzieren: gerade Zylinder 2n, Kopf 0 -> Zylinder n (40 Zylinder, einseitig), Spur 0 = 26 x 128 B, sonst 15 x 256 B.
# Die Sektordaten werden unveraendert uebernommen (CRCs neu gerechnet, Luecken normiert); ungerade Zylinder und Kopf 1 (Rauschen) entfallen.
# Aufruf (Repo-Wurzel):  python3 -I tools/bs600/reduziere_abzug.py <abzug80.hfe> <aus40.hfe> [--lib build/libk1520disk.so]
import ctypes, sys

def main(argv):
    lib = 'build/libk1520disk.so'
    if '--lib' in argv:
        i = argv.index('--lib'); lib = argv[i + 1]; del argv[i:i + 2]
    if len(argv) != 2: sys.exit(__doc__)
    quelle, ziel = argv
    L = ctypes.CDLL(lib)
    L.k1520d_open_raw.restype = ctypes.c_void_p
    L.k1520d_open_raw.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_bool]
    L.k1520d_create.restype = ctypes.c_void_p
    L.k1520d_create.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
    L.k1520d_sector_read.argtypes = [ctypes.c_void_p] + [ctypes.c_int] * 3 + [ctypes.c_char_p, ctypes.c_int]
    L.k1520d_sector_write.argtypes = [ctypes.c_void_p] + [ctypes.c_int] * 3 + [ctypes.c_char_p] + [ctypes.c_int] * 2
    L.k1520d_sector_write.restype = ctypes.c_bool
    L.k1520d_save_as.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    L.k1520d_save_as.restype = ctypes.c_bool
    q = L.k1520d_open_raw(quelle.encode(), None, True)
    if not q: sys.exit('kann %s nicht oeffnen' % quelle)
    z = L.k1520d_create(ziel.encode(), b'bs600_ss40_15x256_boot', b'X')
    if not z: sys.exit('create fehlgeschlagen')
    for c in range(40):
        for i in range(26 if c == 0 else 15):
            b = ctypes.create_string_buffer(512)
            r = L.k1520d_sector_read(q, 2 * c, 0, i, b, 512)
            if r != (128 if c == 0 else 256): sys.exit('Sektor %d/%d (Quelle Zyl %d): %d' % (c, i + 1, 2 * c, r))
            if not L.k1520d_sector_write(z, c, 0, i, b.raw[:r], r, -1): sys.exit('schreiben %d/%d' % (c, i + 1))
    if not L.k1520d_save_as(z, ziel.encode()): sys.exit('speichern')
    print('OK', ziel)

main(sys.argv[1:])

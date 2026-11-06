# BS600/BS620 am PRG 710/710-1 (Entwurf 48, AP-B2d/B6).  Reproduzierbar aus den eingecheckten Fixtures; nur Stdlib + libk1520disk.
#
# In disks/ liegen (AP-B6, Auslieferungsentscheidung):
#   PRG 710-1 : disks/prg710-1_bs620/prg710-1_bs620_system.hfe = Original BS620 (Dez. 1988), Bytes unveraendert (im Paket;
#               am echten 710-1 laufen "low" und "high" - doc/bs600/marken_ff.md §9/§10)
#   PRG 710   : disks/prg710_bs610_rekonstruktion/prg710_bs610_rek.hfe = 'BS610-Rekonstruktion' (AP-B8, EIGENBAU, kein Original,
#               am echten PRG 710 UNGETESTET, EXPERIMENTELL; im Emulator geprueft, im Paket): die BS620-Diskette, in der Datei BS620 der
#               Tastaturteil durch den von BS600 V02 ersetzt (K7609 am 8279 statt K7672 an der K8025-SIO), Bytes aus BS600 V02 an
#               REK_BEREICHE (doc/bs600/tastatur.md §4).  Urlader, Dateiname, Plattentreiber (Doppelschritt) und alle anderen
#               Dateien bleiben die von BS620.
#
# Mit --geraetetest <Ordner> entstehen AUSSERHALB von disks/ die vier Disketten des Geraetetests (doc/bs600/geraetetest.md):
#   prg710-1_bs620_ff_low.hfe   = Original BS620 (= die Systemdiskette)
#   prg710-1_bs620_ff_high.hfe  = BS620 mit gepatchtem Plattentreiber
#   prg710_bs600_ff_low.hfe     = BS600 VERSION 02 (PRG-700-Diskette) mit Urladerkopf 'SYL FOR BS600 ' (EIGENBAU, siehe unten)
#   prg710_bs600_ff_high.hfe    = dasselbe mit gepatchtem Plattentreiber
# Die drei abgeleiteten (high, bs600 low/high) liegen als Testeingabe in tests/fixtures/bs600/; --check vergleicht sie byteweise
# mit einem frischen Bau.
#
# Patch "high": die vier Wartestellen auf das Marken-FF (K5122 Tor B Bit 1) im Plattentreiber, JR NZ (20H) -> JR Z (28H):
#   IN A,(12H) / AND 02H / JR NZ,<selbst>              (Sektorkopf, zwei Stellen)
#   IN A,(12H) / AND 02H / IN A,(16H) / JR NZ,<selbst> (Datenfeld, zwei Stellen)
# Sonst KEIN Byte; bei BS600 (aktive Pruefsumme 2400H, Sollbytes bei AEC0H + Block) wird das Sollbyte des betroffenen
# Blocks mitgefuehrt (je Patch +8 in der Summe -> Sollbyte -8).  BS620 hat die Pruefung stillgelegt (RET bei 2400H).
#
# Urladerkopf 'SYL FOR BS600 ': die PRG-700-Diskette traegt den Urlader des 3-KB-ROMs ('BOOTSTRAP FOR BS600'); das
# 1-KB-ROM des PRG 710 verlangt 'SY' bei Byte 2/3 (NKM-Lader).  Die Sektoren 1-3 sind deshalb die des BS620-Urladers mit
# dem Namen 'BS600 ' (Byte 10-15) - eine EIGENBAU-Abwandlung, KEIN Original.
#
# Aufruf (aus dem Repo-Wurzelverzeichnis):
#   python3 -I tools/bs600/bau_disketten.py [--check] [--geraetetest ORDNER] [--lib build/libk1520disk.so]
import argparse, ctypes, os, shutil, sys, tempfile

FIXT = 'tests/fixtures/bs600/'
BS620 = FIXT + 'bs620_prg710-1.hfe'
BS600 = FIXT + 'bs600_prg700.hfe'
AUS = {
    'prg710-1_bs620_system': 'disks/prg710-1_bs620/prg710-1_bs620_system.hfe',
    'prg710_bs610_rek': 'disks/prg710_bs610_rekonstruktion/prg710_bs610_rek.hfe',
}
PAARE = ('prg710-1_bs620_ff_low', 'prg710-1_bs620_ff_high', 'prg710_bs600_ff_low', 'prg710_bs600_ff_high')
FIXTURE_ABGELEITET = ('prg710-1_bs620_ff_high', 'prg710_bs600_ff_low', 'prg710_bs600_ff_high')   # in tests/fixtures/bs600/


def paar(ordner, name): return os.path.join(ordner, name + '.hfe')
# Tastaturteil (Laufadressen, beide Grenzen einschliesslich) und sein Dateioffset (Monitor ab 4300H, Hauptprogramm ab 6D00H).
REK_BEREICHE = [
    (0x000E, 0x000E, 0x4300),   # RST 08: JP 004D statt JP 0048
    (0x0030, 0x0037, 0x4300),   # RST 30: 8279-Status statt CALL 0201
    (0x0048, 0x005C, 0x4300),   # CALL 0115 / RST-08-Init statt K8025-CTC/SIO-B-Init
    (0x0115, 0x0131, 0x4300),   # Warten + Umsetzen ueber 02B3, Holen IN (C9H)/(C8H)
    (0x0B98, 0x0B99, 0x4300),   # Sprungleiste 0B97: JP 012A statt JP 0201
    (0x74A8, 0x74AD, -0x6D00),  # Hauptprogramm: 8279-Init (C9H := 02H, C1H) statt RST 08
]
PAT_KOPF = bytes([0xDB, 0x12, 0xE6, 0x02, 0x20, 0xFA])             # JR NZ steht an Position 4
PAT_DATEN = bytes([0xDB, 0x12, 0xE6, 0x02, 0xDB, 0x16, 0x20, 0xF8])  # JR NZ steht an Position 6
SEG_EXT = 0x4F00      # Dateioffset der Quellkopie von 1000H..28FFH (Ladeplan, doc/bs600/selbsttest.md §1)
SOLL = 0x41C0         # Dateioffset von AEC0H (Sollbytes der Pruefsumme, Block 1000H = AEC4H, ...)


class Lib:
    def __init__(self, pfad):
        L = ctypes.CDLL(pfad)
        L.k1520d_open_raw.restype = ctypes.c_void_p
        L.k1520d_open_raw.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_bool]
        L.k1520d_sector_read.argtypes = [ctypes.c_void_p] + [ctypes.c_int] * 3 + [ctypes.c_char_p, ctypes.c_int]
        L.k1520d_sector_write.argtypes = [ctypes.c_void_p] + [ctypes.c_int] * 3 + [ctypes.c_char_p] + [ctypes.c_int] * 2
        L.k1520d_sector_write.restype = ctypes.c_bool
        L.k1520d_save_as.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        L.k1520d_save_as.restype = ctypes.c_bool
        L.k1520d_close.argtypes = [ctypes.c_void_p]
        self.L = L

    def oeffne(self, pfad, schreibbar=False):
        h = self.L.k1520d_open_raw(pfad.encode(), None, not schreibbar)
        if not h: sys.exit('kann %s nicht oeffnen' % pfad)
        return Disk(self.L, h)


class Disk:
    def __init__(self, L, h): self.L, self.h = L, h

    @staticmethod
    def anzahl(c): return 26 if c == 0 else 15

    def lies(self, c, i):
        b = ctypes.create_string_buffer(512)
        r = self.L.k1520d_sector_read(self.h, c, 0, i, b, 512)
        if r <= 0: sys.exit('Sektor %d/%d nicht lesbar' % (c, i + 1))
        return b.raw[:r]

    def schreibe(self, c, i, daten):
        if not self.L.k1520d_sector_write(self.h, c, 0, i, bytes(daten), len(daten), -1):
            sys.exit('Sektor %d/%d nicht schreibbar' % (c, i + 1))

    def alle(self):
        return [self.lies(c, i) for c in range(40) for i in range(self.anzahl(c))]

    def speichere(self, pfad):
        os.makedirs(os.path.dirname(pfad), exist_ok=True)
        if os.path.exists(pfad): os.remove(pfad)
        if not self.L.k1520d_save_as(self.h, pfad.encode()): sys.exit('Speichern %s fehlgeschlagen' % pfad)

    def schliesse(self): self.L.k1520d_close(self.h)


def datei_lage(disk, name):
    """(Spur, Index) des ersten Sektors und Sektorzahl der Datei @p name (Verzeichnis Spur 0, Sektor 8-25, 3 x 42 B)."""
    for s in range(7, 25):
        sek = disk.lies(0, s)
        for k in (0, 42, 84):
            e = sek[k:k + 42]
            if e[:6] == name.ljust(6).encode():
                t0, s0, t1, s1 = e[7], e[8], e[9], e[10]
                return t0, s0 - 1, (t1 - t0) * 15 + (s1 - s0) + 1
    sys.exit('Datei %s nicht im Verzeichnis' % name)


def datei_lesen(disk, name):
    t0, i0, n = datei_lage(disk, name)
    lin = (t0 - 1) * 15 + i0
    out = b''
    for k in range(n):
        out += disk.lies(1 + (lin + k) // 15, (lin + k) % 15)
    return out


def sektor_von(offset, name_lage):
    t0, i0, _ = name_lage
    lin = (t0 - 1) * 15 + i0 + offset // 256
    return 1 + lin // 15, lin % 15, offset % 256


def patch_stellen(d):
    """Dateioffsets der JR-NZ-Bytes der vier Wartestellen (nur im Treiberbereich ab SEG_EXT)."""
    st = []
    for pat, pos in ((PAT_KOPF, 4), (PAT_DATEN, 6)):
        i = SEG_EXT
        while True:
            i = d.find(pat, i)
            if i < 0 or i >= SEG_EXT + 0x1900: break
            st.append(i + pos); i += 1
    return sorted(st)


def bloecke():
    """Die 25 geprueften 1-KB-Bloecke von BS600 (Pruefsumme 2400H): (Laufadresse, Dateioffset).
    3 Bloecke ab 0000H (Datei 4300H...), 6 ab 1000H (Datei 4F00H...), 16 ab 7000H (Datei 0300H...)."""
    r = [(0x0000 + 0x400 * k, 0x4300 + 0x400 * k) for k in range(3)]
    r += [(0x1000 + 0x400 * k, SEG_EXT + 0x400 * k) for k in range(6)]
    r += [(0x7000 + 0x400 * k, 0x0300 + 0x400 * k) for k in range(16)]
    return r


def block_rest(d, run, off):
    """Summe ueber den Block + Sollbyte (bei AEC0H + Block/400H); 0 = gut."""
    return (sum(d[off:off + 0x400]) + d[SOLL + (run >> 10)]) & 0xFF


def pruefsumme_aktiv(d): return d[SEG_EXT + 0x1400] != 0xC9


def sollbytes_nachfuehren(d):
    """Nach dem Patch jeden Block wieder auf Rest 0 bringen (nur das Sollbyte aendern).  Die Sollbytes stehen selbst im Block
    AC00H-AFFFH: dort gilt Rest + 2 * delta = 0."""
    for _ in range(4):
        fertig = True
        for run, off in bloecke():
            r = block_rest(d, run, off)
            if r == 0: continue
            fertig = False
            so = SOLL + (run >> 10)
            innen = off <= so < off + 0x400
            if innen:
                if r % 2: sys.exit('Block %04XH: Rest %02X nicht korrigierbar' % (run, r))
                delta = ((-r) % 256) // 2
            else:
                delta = (-r) % 256
            d[so] = (d[so] + delta) & 0xFF
        if fertig: return
    sys.exit('Pruefsumme laesst sich nicht nachfuehren')


def hoch_patchen(disk, name):
    """Sektorweise patchen; liefert die Liste der geaenderten (Dateioffset, alt, neu)."""
    d = bytearray(datei_lesen(disk, name))
    lage = datei_lage(disk, name)
    st = patch_stellen(bytes(d))
    if len(st) != 4: sys.exit('%s: %d statt 4 Wartestellen gefunden' % (name, len(st)))
    alt = bytes(d)
    aktiv = pruefsumme_aktiv(alt)
    if aktiv:
        for run, off in bloecke():
            if block_rest(alt, run, off): sys.exit('%s: Pruefsumme Block %04XH stimmt schon vorher nicht' % (name, run))
    for o in st: d[o] = 0x28
    if aktiv: sollbytes_nachfuehren(d)
    aend = [(o, alt[o], d[o]) for o in range(len(d)) if alt[o] != d[o]]
    for o, a_, n_ in aend:
        t, i, bo = sektor_von(o, lage)
        sek = bytearray(disk.lies(t, i))
        assert sek[bo] == a_
        sek[bo] = n_
        disk.schreibe(t, i, sek)
    if aktiv:
        d2 = datei_lesen(disk, name)
        for run, off in bloecke():
            if block_rest(d2, run, off): sys.exit('%s: Pruefsumme Block %04XH nach dem Patch falsch' % (name, run))
    return aend, lage


def urlader_eigenbau(disk600, disk620):
    """Sektoren 1-3 des BS620-Urladers mit dem Namen 'BS600 ' (Byte 10-15) in die PRG-700-Diskette."""
    for i in range(3):
        s = bytearray(disk620.lies(0, i))
        if i == 0:
            assert s[2:10] == b'SYL FOR ' and s[10:16] == b'BS620 ', s[:20]
            s[10:16] = b'BS600 '
        disk600.schreibe(0, i, s)


def rek_patchen(disk, quelle):
    """Datei BS620 in @p disk: REK_BEREICHE aus der Datei BS600 von @p quelle uebernehmen (sektorweise).  Liefert die Aenderungen."""
    d = bytearray(datei_lesen(disk, 'BS620')); alt = bytes(d)
    q = datei_lesen(quelle, 'BS600')
    if len(q) != len(d): sys.exit('BS600/BS620 verschieden lang')
    if alt[SEG_EXT + 0x1400] != 0xC9: sys.exit('BS620: Pruefsumme nicht stillgelegt (2400H <> RET)')
    for a, e, basis in REK_BEREICHE:
        o = a + basis
        d[o:o + e - a + 1] = q[o:o + e - a + 1]
    lage = datei_lage(disk, 'BS620')
    aend = [(o, alt[o], d[o]) for o in range(len(d)) if alt[o] != d[o]]
    for o, a_, n_ in aend:
        t, i, bo = sektor_von(o, lage)
        sek = bytearray(disk.lies(t, i))
        assert sek[bo] == a_
        sek[bo] = n_
        disk.schreibe(t, i, sek)
    return aend


def rek_erwartet(lib):
    """Die erwartete Abweichungsliste (Dateioffset, BS620, BS600) aus den Fixtures."""
    a = lib.oeffne(BS620); b = lib.oeffne(BS600)
    x, y = datei_lesen(a, 'BS620'), datei_lesen(b, 'BS600')
    a.schliesse(); b.schliesse()
    r = []
    for s_, e, basis in REK_BEREICHE:
        r += [(o, x[o], y[o]) for o in range(s_ + basis, e + basis + 1) if x[o] != y[o]]
    return r


def bauen_paare(lib, ordner):
    """Die vier Geraetetest-Disketten nach @p ordner (ausserhalb von disks/)."""
    stand = {}
    os.makedirs(ordner, exist_ok=True)
    # PRG 710-1 / BS620
    for ff in ('low', 'high'):
        ziel = paar(ordner, 'prg710-1_bs620_ff_' + ff)
        if ff == 'low':
            shutil.copyfile(BS620, ziel)                                  # Original, Byte fuer Byte
        else:
            d = lib.oeffne(BS620, True)
            aend, _ = hoch_patchen(d, 'BS620')
            d.speichere(ziel); d.schliesse()
            stand[ziel] = aend
    # PRG 710 / BS600
    for ff in ('low', 'high'):
        ziel = paar(ordner, 'prg710_bs600_ff_' + ff)
        d = lib.oeffne(BS600, True); q = lib.oeffne(BS620)
        urlader_eigenbau(d, q); q.schliesse()
        if ff == 'high':
            aend, _ = hoch_patchen(d, 'BS600')
            stand[ziel] = aend
        d.speichere(ziel); d.schliesse()
    return stand


def bauen(lib, wurzel):
    stand = {}
    ziel = os.path.join(wurzel, AUS['prg710-1_bs620_system'])
    os.makedirs(os.path.dirname(ziel), exist_ok=True)
    shutil.copyfile(BS620, ziel)                                          # Original, Byte fuer Byte
    # PRG 710 / BS610-Rekonstruktion (Eigenbau)
    ziel = os.path.join(wurzel, AUS['prg710_bs610_rek'])
    d = lib.oeffne(BS620, True); q = lib.oeffne(BS600)
    stand[ziel] = rek_patchen(d, q); q.schliesse()
    d.speichere(ziel); d.schliesse()
    return stand


def pruefen(lib, wurzel, ordner):
    """Sektorvergleich: genau die erwarteten Bytes weichen ab.  @p ordner = Geraetetest-Paare (frisch gebaut)."""
    ok = True
    for name, basis, bs in (('prg710-1_bs620', BS620, 'BS620'), ('prg710_bs600', BS600, 'BS600')):
        a = lib.oeffne(paar(ordner, name + '_ff_low')); b = lib.oeffne(paar(ordner, name + '_ff_high'))
        x, y = a.alle(), b.alle()
        lage = datei_lage(a, bs); d = datei_lesen(a, bs)
        diff = []
        for n, (s, t) in enumerate(zip(x, y)):
            if s != t:
                c = n if n < 26 else 26 + 0
                diff += [(n, k, s[k], t[k]) for k in range(len(s)) if s[k] != t[k]]
        n_diff = len(diff)
        erwartet = 4 + (3 if pruefsumme_aktiv(d) else 0)   # + Sollbytes der Bloecke 1000H, 1400H und AC00H (traegt die Sollbytes)
        print('%s: %d Bytes weichen ab (erwartet %d)' % (name, n_diff, erwartet))
        for n, k, s, t in diff: print('   Sektor-laufnr %d Byte %d: %02X -> %02X' % (n, k, s, t))
        ok &= n_diff == erwartet
        a.schliesse(); b.schliesse()
    # Eigenbau-Urlader: nur Sektor 1 Byte 10-15 weicht vom BS620 ab
    a = lib.oeffne(paar(ordner, 'prg710_bs600_ff_low')); q = lib.oeffne(BS620)
    ok &= a.lies(0, 0)[:10] == q.lies(0, 0)[:10] and a.lies(0, 0)[10:16] == b'BS600 ' and a.lies(0, 0)[16:] == q.lies(0, 0)[16:]
    ok &= a.lies(0, 1) == q.lies(0, 1) and a.lies(0, 2) == q.lies(0, 2)
    a.schliesse(); q.schliesse()
    # BS610-Rekonstruktion: Sektorvergleich gegen das BS620-Original, genau die Bytes der REK_BEREICHE weichen ab
    a = lib.oeffne(BS620); b = lib.oeffne(os.path.join(wurzel, AUS['prg710_bs610_rek']))
    lage = datei_lage(a, 'BS620')
    erw = {sektor_von(o, lage): (o, x, y) for o, x, y in rek_erwartet(lib)}
    ist = {}
    for c in range(40):
        for i in range(Disk.anzahl(c)):
            s_, t = a.lies(c, i), b.lies(c, i)
            for k in range(len(s_)):
                if s_[k] != t[k]: ist[(c, i, k)] = (s_[k], t[k])
    print('prg710_bs610_rek: %d Bytes weichen vom BS620-Original ab (erwartet %d)' % (len(ist), len(erw)))
    for (c, i, k), (x, y) in sorted(ist.items()):
        o = erw.get((c, i, k))
        lauf = '' if o is None else (' = %04XH' % (o[0] - 0x4300 if 0x4300 <= o[0] < 0x4F00 else o[0] + 0x6D00))
        print('   Spur %d Sektor %d Byte %3d (Dateioffset %s%s): %02X -> %02X' % (c, i + 1, k, '%04X' % o[0] if o else '????', lauf, x, y))
    ok &= set(ist) == set(erw) and all(ist[k] == erw[k][1:] for k in ist)
    a.schliesse(); b.schliesse()
    # BS620 low = Original, Byte fuer Byte
    ok &= open(os.path.join(wurzel, AUS['prg710-1_bs620_system']), 'rb').read() == open(BS620, 'rb').read()
    ok &= open(paar(ordner, 'prg710-1_bs620_ff_low'), 'rb').read() == open(BS620, 'rb').read()
    # abgeleitete Testeingaben in tests/fixtures/bs600/ == frischer Bau
    for n in FIXTURE_ABGELEITET:
        f = FIXT + n + '.hfe'
        gleich = os.path.exists(f) and open(f, 'rb').read() == open(paar(ordner, n), 'rb').read()
        print('Fixture %s: %s' % (f, 'gleich' if gleich else 'ABWEICHEND/FEHLT'))
        ok &= gleich
    print('OK' if ok else 'FEHLER')
    return ok


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--lib', default='build/libk1520disk.so')
    ap.add_argument('--wurzel', default='.')
    ap.add_argument('--check', action='store_true', help='nur pruefen, nichts in disks/ bauen')
    ap.add_argument('--geraetetest', metavar='ORDNER', help='die vier Geraetetest-Disketten (low/high) hierhin bauen (nicht disks/)')
    a = ap.parse_args()
    lib = Lib(a.lib)
    if not a.check:
        for z, aend in bauen(lib, a.wurzel).items():
            print(z)
            for o, alt, neu in aend: print('   Dateioffset %04X: %02X -> %02X' % (o, alt, neu))
    with tempfile.TemporaryDirectory() as tmp:
        ordner = a.geraetetest or tmp
        for z, aend in bauen_paare(lib, ordner).items():
            if a.geraetetest:
                print(z)
                for o, alt, neu in aend: print('   Dateioffset %04X: %02X -> %02X' % (o, alt, neu))
        ok = pruefen(lib, a.wurzel, ordner)
    sys.exit(0 if ok else 1)

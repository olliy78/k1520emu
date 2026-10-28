"""Waechter fuer tools/z80_disasm2.py - DER Z80-Disassembler (Entwurf 40, W1-W3).

(a) Befehlsvollstaendigkeit: alle 256 Grundopcodes und die CB/ED/DD/FD-Raeume werden
    gegen eine UNABHAENGIGE Laengentabelle in dieser Datei geprueft; Undokumentiertes
    und nicht eindeutig Assemblierbares kommt als DB statt als falscher Befehl.
(b) Rundlauf (nur mit CP/M-Werkzeugkette, sonst uebersprungen): Abzug -> mac-Quelle
    -> M80/LINKMT -> Bytes muessen gleich sein.  Ist das nicht so, ist es ein
    Werkzeugfehler - nicht den Test aufweichen.
(c) Karten- und Segmentfaelle an kleinen synthetischen Eingaben.
Dazu der Waechter von W3: aus Abzug + Karte entsteht die Quelle von mutos_io."""

import importlib.util
import subprocess
import sys
import zipfile

import pytest

from conftest import PROJECT_ROOT

TOOLS = PROJECT_ROOT / "tools"
WERKZEUG = TOOLS / "z80_disasm2.py"
MUTOSIO = TOOLS / "mutos8000" / "mutos_io"
ARCHIV = PROJECT_ROOT / "disks" / "a5120_mutos8000" / "a5120_mutos8000_boot_udos.zip"


def _lade(name, pfad):
    spec = importlib.util.spec_from_file_location(name, pfad)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


dis = _lade("z80_disasm2_unter_test", WERKZEUG)


def mac(datei, *args, karte=None):
    cmd = [sys.executable, "-I", str(WERKZEUG), str(datei), "--format", "mac", *args]
    if karte is not None:
        cmd += ["--karte", str(karte)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stdout + r.stderr
    return r.stdout


# ---------------------------------------------------------------------------
# (a) Befehlsvollstaendigkeit gegen eine unabhaengige Laengentabelle
# ---------------------------------------------------------------------------
FUELL = [0x12, 0x34, 0x56, 0x78]       # Operandbytes hinter dem Opcode (d = +18, n = 34H ...)

LEN2 = {0x06, 0x0E, 0x16, 0x1E, 0x26, 0x2E, 0x36, 0x3E,          # LD r,n
        0x10, 0x18, 0x20, 0x28, 0x30, 0x38,                       # DJNZ, JR
        0xC6, 0xCE, 0xD6, 0xDE, 0xE6, 0xEE, 0xF6, 0xFE,           # ALU n
        0xD3, 0xDB}                                               # OUT/IN (n)
LEN3 = {0x01, 0x11, 0x21, 0x31, 0x22, 0x2A, 0x32, 0x3A,
        0xC2, 0xC3, 0xC4, 0xCA, 0xCC, 0xCD, 0xD2, 0xD4, 0xDA, 0xDC,
        0xE2, 0xE4, 0xEA, 0xEC, 0xF2, 0xF4, 0xFA, 0xFC}
PRAEFIX = {0xCB, 0xDD, 0xED, 0xFD}
# alle 256 Grundopcodes ausser den Praefixen sind dokumentiert (0x76 = HALT)

ED2 = ({0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x78}                # IN r,(C)  (ohne 70H)
       | {0x41, 0x49, 0x51, 0x59, 0x61, 0x69, 0x79}              # OUT (C),r (ohne 71H)
       | {0x42, 0x52, 0x62, 0x72, 0x4A, 0x5A, 0x6A, 0x7A}        # SBC/ADC HL,rr
       | {0x44, 0x45, 0x4D, 0x46, 0x56, 0x5E, 0x47, 0x4F, 0x57, 0x5F, 0x67, 0x6F}
       | {0xA0, 0xA1, 0xA2, 0xA3, 0xA8, 0xA9, 0xAA, 0xAB,
          0xB0, 0xB1, 0xB2, 0xB3, 0xB8, 0xB9, 0xBA, 0xBB})
ED4 = {0x43, 0x53, 0x73, 0x4B, 0x5B, 0x7B}                       # LD (nn),rr / LD rr,(nn)
ED4_LANGFORM = {0x63, 0x6B}                                      # M80 nimmt die Kurzform -> DB

DD2 = {0x09, 0x19, 0x29, 0x39, 0x23, 0x2B, 0xE1, 0xE5, 0xE3, 0xE9, 0xF9}
DD3 = ({0x34, 0x35}
       | {0x46, 0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x7E}               # LD r,(IX+d)
       | {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x77}               # LD (IX+d),r
       | {0x86, 0x8E, 0x96, 0x9E, 0xA6, 0xAE, 0xB6, 0xBE})        # ALU (IX+d)
DD4 = {0x21, 0x22, 0x2A, 0x36}


def _speicher(bs):
    m = dis.Speicher()
    m.laden(0x1000, bytes(bs))
    return m


def _dekodiere(bs):
    """-> ('i', laenge, vorlage) oder ('db', laenge)"""
    try:
        v, n, _, _, _ = dis.entschluesseln(_speicher(bs), 0x1000)
        return ('i', n, v)
    except dis.Undokumentiert as u:
        return ('db', u.laenge)


@pytest.mark.parametrize("op", range(256))
def test_grundopcode_laenge_stimmt(op):
    if op in PRAEFIX:
        pytest.skip("Praefix")
    erg = _dekodiere([op] + FUELL)
    soll = 2 if op in LEN2 else 3 if op in LEN3 else 1
    assert erg[0] == 'i' and erg[1] == soll, (hex(op), erg)


def test_alle_cb_befehle_sind_zwei_byte_oder_db():
    for op in range(256):
        erg = _dekodiere([0xCB, op] + FUELL)
        if 0x30 <= op <= 0x37:                  # SLL ist undokumentiert
            assert erg == ('db', 2), hex(op)
        else:
            assert erg[:2] == ('i', 2), hex(op)


def test_ed_raum():
    for op in range(256):
        erg = _dekodiere([0xED, op] + FUELL)
        if op in ED2:
            assert erg[:2] == ('i', 2), hex(op)
        elif op in ED4:
            assert erg[:2] == ('i', 4), hex(op)
        elif op in ED4_LANGFORM:
            assert erg == ('db', 4), hex(op)
        else:
            assert erg == ('db', 2), f"ED {op:02X}: {erg}"


@pytest.mark.parametrize("praefix", [0xDD, 0xFD])
def test_dd_fd_raum(praefix):
    for op in range(256):
        if op == 0xCB:
            continue
        erg = _dekodiere([praefix, op] + FUELL)
        if op in DD2:
            assert erg[:2] == ('i', 2), hex(op)
        elif op in DD3:
            assert erg[:2] == ('i', 3), hex(op)
        elif op in DD4:
            assert erg[:2] == ('i', 4), hex(op)
        else:                                    # Halbregister, Schattenpraefix ...
            assert erg == ('db', 1), f"{praefix:02X} {op:02X}: {erg}"
    # DD CB d op: nur (IX+d)-Formen mit Registerfeld 6, ohne SLL
    for op in range(256):
        erg = _dekodiere([praefix, 0xCB, 0x05, op])
        if (op & 7) == 6 and not 0x30 <= op <= 0x37:
            assert erg[:2] == ('i', 4), hex(op)
        else:
            assert erg == ('db', 4), hex(op)


def test_mnemonik_stichproben():
    assert _dekodiere([0x76])[2] == "HALT"
    assert _dekodiere([0xDD, 0x7E, 0xFB])[2] == "LD A,(IX-5)"
    assert _dekodiere([0xFD, 0x36, 0x02, 0x99])[2] == "LD (IY+2),99H"
    assert _dekodiere([0xED, 0xB0])[2] == "LDIR"
    assert _dekodiere([0xCB, 0x7E])[2] == "BIT 7,(HL)"
    assert _dekodiere([0xDD, 0xCB, 0x01, 0xC6])[2] == "SET 0,(IX+1)"
    assert _dekodiere([0xE3])[2] == "EX (SP),HL"
    assert _dekodiere([0xD9])[2] == "EXX"
    v, n, ziele, ende, absz = dis.entschluesseln(_speicher([0xC3, 0x00, 0x90]), 0x1000)
    assert (v, n, ziele, ende, absz) == ("JP {j}", 3, [0x9000], True, ('j', 0x9000))
    v, n, ziele, ende, absz = dis.entschluesseln(_speicher([0x18, 0xFE]), 0x1000)
    assert ziele == [0x1000] and ende                     # JR $ : Sprung auf sich selbst


# ---------------------------------------------------------------------------
# (b) Rundlauf Abzug -> mac -> Bytes (braucht die CP/M-Werkzeugkette)
# ---------------------------------------------------------------------------
sys.path.insert(0, str(TOOLS))
import cpm_bau  # noqa: E402


@pytest.fixture
def assembliere(tmp_path):
    if not cpm_bau.werkzeugkette_da():
        pytest.skip(f"CP/M-Werkzeugkette nicht gefunden ({cpm_bau.CPA_TOOLS}; CPA_TOOLS=<pfad>)")
    gemerkt = cpm_bau.log
    cpm_bau.log = lambda msg: None

    def lauf(quelle_text, name="rt"):
        (tmp_path / "src").mkdir(exist_ok=True)
        (tmp_path / "src" / f"{name}.mac").write_text(quelle_text)
        cpm_bau.baue(cpm_bau.Programm(name, str(tmp_path)),
                     str(tmp_path / "b"), str(tmp_path / f"{name}.com"))
        return (tmp_path / f"{name}.com").read_bytes()
    yield lauf
    cpm_bau.log = gemerkt


def _rundlauf(assembliere, datei, *args):
    original = datei.read_bytes()
    text = mac(datei, "--org", "0x100", "--auffuellen", "0", *args)
    neu = assembliere(text)
    assert neu == original, "Rundlauf nicht bytegleich: Werkzeugfehler (erste Abweichung " \
        f"bei {next((hex(0x100 + i) for i, (a, b) in enumerate(zip(original, neu)) if a != b), 'Laenge')})"


@pytest.mark.parametrize("com", ["sertest/sertest.com", "romread/romread.com"])
@pytest.mark.parametrize("modus", [(), ("--linear",)], ids=["rekursiv", "linear"])
def test_rundlauf_eigene_programme(assembliere, com, modus):
    _rundlauf(assembliere, TOOLS / com, *modus)


def test_rundlauf_gesamter_befehlsraum(assembliere, tmp_path):
    """Jeder Opcode in jedem Praefixraum, linear dekodiert: alles muss bytegleich zurueckkommen
    (M80 muss also jede ausgegebene Schreibweise genauso kodieren - sonst haette der Dekoder
    etwas als Befehl ausgegeben, das nicht eindeutig ist)."""
    roh = bytearray()
    for praefix in ([], [0xCB], [0xED], [0xDD], [0xFD], [0xDD, 0xCB], [0xFD, 0xCB]):
        for op in range(256):
            if not praefix and op in PRAEFIX:
                continue
            roh += bytes(praefix) + bytes([op, 0x05, 0x44, 0x06, 0x80])
    datei = tmp_path / "alles.bin"
    roh += bytes(-len(roh) % 128)          # LINKMT fuellt die .COM auf 128-Byte-Saetze
    datei.write_bytes(bytes(roh))
    _rundlauf(assembliere, datei, "--linear")


# ---------------------------------------------------------------------------
# (c) Karten- und Segmentfaelle
# ---------------------------------------------------------------------------
def _datei(tmp_path, bs, name="x.bin"):
    p = tmp_path / name
    p.write_bytes(bytes(bs))
    return p


def _karte(tmp_path, text):
    p = tmp_path / "karte.txt"
    p.write_text(text)
    return p


def test_standardsegment_ohne_karte(tmp_path):
    d = _datei(tmp_path, [0x3E, 0x05, 0x18, 0xFC])         # LD A,5 ; JR $-2 (zurueck auf 0)
    t = mac(d, "--org", "0x4000", "--auffuellen", "0")
    assert ".PHASE\t4000H" in t and "SEG4000:" in t and "SEG4000E:" in t
    assert "LD A,05H" in t and "JR L4000" in t and "L4000:" in t
    assert "REPT" not in t.split("SEG4000E:")[1]           # keine Auffuellung


def test_auffuellung_auf_256_und_ff_lauf(tmp_path):
    d = _datei(tmp_path, [0xC9] + [0xFF] * 20)              # RET + 20 Fuellbytes
    t = mac(d)
    assert "\tRET" in t and "FILLFF\t20" in t
    assert "(100H-(($-SEG0000) AND 0FFH)) AND 0FFH" in t    # Auffuellung wie in mutosio.mac


def test_ff_lauf_beendet_die_codeverfolgung(tmp_path):
    # NOP, danach FFH-Lauf (RST 38H) - die FFH sind Fuellung, kein Code
    d = _datei(tmp_path, [0x00] + [0xFF] * 12)
    t = mac(d, "--auffuellen", "0")
    assert "RST" not in t and "FILLFF\t12" in t


def test_mehrere_segmente_mit_dateioffset(tmp_path):
    # Datei: [A: LD A,1 ; RET][B: 4 Bytes Daten]  -> Segment 8000 (Offset 0, 4 B) und 9000 (Offset 4, 4 B)
    d = _datei(tmp_path, [0x3E, 0x01, 0xC9, 0x00, 0xAA, 0xBB, 0xCC, 0xDD])
    t = mac(d, "--seg", "0x8000:0:4", "--seg", "0x9000:4:4", "--entry", "0x8000", "--auffuellen", "0")
    assert t.index(".PHASE\t8000H") < t.index(".PHASE\t9000H")
    assert "SEG8000E:" in t and "SEG9000E:" in t
    assert "LD A,01H" in t and "DB\t0AAH,0BBH,0CCH,0DDH" in t


def test_karte_seg_zeilen_und_dateireihenfolge(tmp_path):
    # Die Reihenfolge der Segmente folgt der Datei, nicht der Adresse
    d = _datei(tmp_path, [0xC9, 0x00, 0x11, 0x22])
    k = _karte(tmp_path, "seg 9000 0 2\nseg 8000 2 2\ncode 9000\ndb 8000 8001\n")
    t = mac(d, "--auffuellen", "0", karte=k)
    assert t.index(".PHASE\t9000H") < t.index(".PHASE\t8000H")
    assert "\tRET" in t and "DB\t11H,22H" in t


def test_karte_name_kom_ik_eq_und_adresskommentar(tmp_path):
    # 4000: CALL 4005 ; 4003: JR 4003 ; 4005: RET
    d = _datei(tmp_path, [0xCD, 0x05, 0x40, 0x18, 0xFE, 0xC9])
    k = _karte(tmp_path,
               "seg 4000 0 6\ncode 4000\n"
               "name 4005 UNTER\n"
               "kom 4000 erste Zeile\nkom 4000 zweite Zeile\n"
               "ik 4000 ruft UNTER\n"
               "eq PORT 00A8\n")
    t = mac(d, "--auffuellen", "0", "--adr", karte=k)
    zeilen = t.splitlines()
    i = zeilen.index("; erste Zeile")
    assert zeilen[i + 1] == "; zweite Zeile"                   # kom darf mehrfach stehen
    assert zeilen[i + 2] == "\tCALL UNTER\t; 4000 ruft UNTER"   # Adresse + Zeilenkommentar
    assert "UNTER:" in zeilen and "PORT\tEQU\t00A8H" in t
    ohne = mac(d, "--auffuellen", "0", karte=k)
    assert "\tCALL UNTER\t; ruft UNTER" in ohne.splitlines()  # Adresskommentar nur per Schalter
    assert "; 4000" not in ohne


def test_dwc_zeigertabelle_liefert_einstiege(tmp_path):
    # 5000: DW 5004 ; 5002: DW 5005 ; 5004: RET ; 5005: RET   (nur die Tabelle fuehrt dorthin)
    d = _datei(tmp_path, [0x04, 0x50, 0x05, 0x50, 0xC9, 0xC9])
    k = _karte(tmp_path, "seg 5000 0 6\ndwc 5000 5003\n")
    t = mac(d, "--auffuellen", "0", karte=k)
    assert "DW\tL5004" in t and "DW\tL5005" in t
    assert t.count("\tRET") == 2


def test_ld_rr_nn_zeiger_als_einstieg(tmp_path):
    # 6000: LD HL,6004 ; JP (HL) ; 6004: RET   -> 6004 nur ueber den Zeiger erreichbar
    d = _datei(tmp_path, [0x21, 0x04, 0x60, 0xE9, 0xC9])
    k = _karte(tmp_path, "seg 6000 0 5\ncode 6000\n")
    mit = mac(d, "--auffuellen", "0", karte=k)
    assert "LD HL,L6004" in mit and "\tRET" in mit
    ohne = mac(d, "--auffuellen", "0", "--no-zeiger", "--no-folge", karte=k)
    assert "\tRET" not in ohne and "DB\t0C9H" in ohne


def test_str_txt_und_db_dw_arten(tmp_path):
    d = _datei(tmp_path, [0x03, 0x41, 0x42, 0x43, 0x41, 0x42, 0x07, 0x00, 0x00, 0x70, 0x10, 0x20])
    k = _karte(tmp_path, "seg 7000 0 C\nstr 7000 7003\ntxt 7004 7005\ndb 7006 7007\ndw 7008 7009\ndb 700A 700B\n")
    t = mac(d, "--auffuellen", "0", "--entry", "0xFFFF", karte=k)
    assert "DB\t03H\n\tDB\t'ABC'" in t           # Laengenbyte als Zahl, Text in Anfuehrungszeichen
    assert "DB\t'AB'" in t and "DB\t07H,00H" in t
    assert "DW\tL7000" in t
    assert "DB\t10H,20H" in t


def test_marke_mitten_im_befehl_wird_zur_zahl(tmp_path):
    # 3000: LD HL,3001 (Zeiger auf das Operandbyte des eigenen Befehls) ; RET
    d = _datei(tmp_path, [0x21, 0x01, 0x30, 0xC9])
    k = _karte(tmp_path, "seg 3000 0 4\ncode 3000\n")
    t = mac(d, "--auffuellen", "0", karte=k)
    assert "LD HL,3001H" in t and "L3001:" not in t        # keine Marke in der Luft


def test_undokumentiertes_wird_db_und_bleibt_bytegleich(tmp_path):
    d = _datei(tmp_path, [0xDD, 0x24, 0xED, 0x70, 0xCB, 0x30, 0xC9])   # INC IXH, IN F,(C), SLL B, RET
    t = mac(d, "--linear", "--auffuellen", "0")
    assert "IXH" not in t and "SLL" not in t
    assert "DB\t0DDH" in t and "DB\t0EDH,70H" in t and "DB\t0CBH,30H" in t


def test_listing_modus_unveraendert(tmp_path):
    d = _datei(tmp_path, [0x3E, 0x05, 0xC9])
    r = subprocess.run([sys.executable, "-I", str(WERKZEUG), str(d), "--org", "0x100"],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    assert "0100  3E 05" in r.stdout and "LD A,05H" in r.stdout and "RET" in r.stdout
    # mac-Schalter im Listing-Modus sind ein Fehler, kein stilles Ignorieren
    r = subprocess.run([sys.executable, "-I", str(WERKZEUG), str(d), "--karte", "x"],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode != 0


# ---------------------------------------------------------------------------
# W3: aus Abzug + Karte entsteht der Ausgangsstand der Quelle von mutos_io
# (seit M4 ist src/mutosio.mac der Master und wird von Hand weiterentwickelt;
#  die Karte belegt nur, dass der Ausgangsstand reproduzierbar war)
# ---------------------------------------------------------------------------
def test_mutos_io_karte_erzeugt_den_ausgangsstand(tmp_path):
    with zipfile.ZipFile(ARCHIV) as z:
        abzug = tmp_path / "mutos_io"
        abzug.write_bytes(z.read("dateien/mutos_io"))
    karte = MUTOSIO / "karte.txt"
    ohne = mac(abzug, karte=karte)
    mit = mac(abzug, "--adr", karte=karte)
    # Beide Fassungen unterscheiden sich nur in den Adresskommentaren
    def kahl(t):
        return [z.split("\t;")[0] for z in t.splitlines()]
    assert kahl(ohne) == kahl(mit)
    # jede Marke der Karte steht in der erzeugten Quelle
    namen = [z.split()[2] for z in karte.read_text().splitlines() if z.startswith("name ")]
    assert namen and all(f"\n{n}:" in ohne for n in namen)
    # und die Master-Quelle kennt dieselben Marken (Umbenennen ist erlaubt, Verlieren nicht stillschweigend)
    master = (MUTOSIO / "src" / "mutosio.mac").read_text()
    fehlend = [n for n in namen if f"\n{n}:" not in master]
    assert len(fehlend) <= 3, fehlend


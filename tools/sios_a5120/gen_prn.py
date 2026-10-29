#!/usr/bin/env python3
"""
Baut doc/sios/sysare11.prn (und syl_lader.prn): kommentierte Listings des SIOS 1526/1.1
(Datei SYSARE11 der Diskette SIOS1526_SYLs.hfe), in dem Format, das `k1520dbg -l` und
`boot_trace -l` als Annotation lesen (Adresse, Objektbytes, Quelltext).

Ladeablauf (belegt am laufenden Emulator, Einzelheiten doc/sios/README.md):
  Lade-ROM  -> Spur 0 Sektor 1-4 (SYL-Lader) nach 0400H, Einsprung 0437H
  SYL-Lader -> liest 272 Sektoren (= 8800H Byte) ab Spur 2 nach 0800H..8FFFH,
               setzt AAWA (004CH) = 9000H, kehrt ins Lade-ROM zurueck
  Lade-ROM  -> JP 0074H -> LD HL,(0462H) / LD L,9DH / JP (HL) = Systemstart 089DH
Der Systembereich besteht aus 34 Bloecken zu 1 KB: Kopf (2 Byte: Modulkennung, Flags),
Code/Daten, am Ende CRC-CCITT (Start FFFFH) ueber die ersten 1022 Byte.  Der Modul-
einsprung liegt bei Block+2 (der Lader springt ueber 005DH dorthin).

STATISCHES Disassemblat (tools/z80_disasm2.py, rekursiver Abstieg) mit den SIOS-Eigenheiten:
  * relative Spruenge/Aufrufe:  LD DE,nn / CALL 002BH  = Sprung  nach (Folgeadresse + nn)
                                LD DE,nn / CALL 003BH  = Aufruf  von (Folgeadresse + nn)
  * CALL 0036H / LD DE,n / ADD HL,DE / PUSH HL / RET(I): Fortsetzung bei (Folgeadresse + n)
  * CALL 0042H mit L = ungerader Index: Sprung ueber die Tabelle 0800H (Wort + 0800H)
  * RST 08H (SVC) hat EIN Kennbyte hinter dem Befehl
Aufruf:  python3 tools/sios_a5120/gen_prn.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, HERE)
sys.setrecursionlimit(100000)
from z80_disasm2 import Z80Disassembler  # noqa: E402
import dump  # noqa: E402

ABBILD = os.path.join(HERE, "original", "SIOS1526_SYLs.hfe")
OUT_SYS = os.path.join(ROOT, "doc/sios/sysare11.prn")
OUT_SYL = os.path.join(ROOT, "doc/sios/syl_lader.prn")

SYS_ANF = 0x0800          # Ladeadresse
SYS_LEN = 0x8800          # 272 Sektoren
SYS_AB_SEKTOR = 26        # Datei SYSARE11: Spur 1 (26 Sektoren) wird nicht geladen, Laden ab Spur 2
RELOC = 0x08              # Hochbyte-Versatz (Seite) der Vorlage 0840H.. gegenueber Seite 0


def lade():
    ab = dump.Abbild(ABBILD)
    syl = ab.linear(0, 1)[:512]
    e = [x for x in dump.etiketten(ab) if x["name"] == "SYSARE11"][0]
    datei = dump.datei_bytes(ab, e)
    img = datei[SYS_AB_SEKTOR * 128: SYS_AB_SEKTOR * 128 + SYS_LEN]
    assert len(img) == SYS_LEN
    return syl, img


def tiefe_seite(img):
    """Seite 0 so, wie sie 089DH..0935H aus der Vorlage 0840H..088BH aufbaut."""
    vorl = bytearray(img[0x40:0x40 + 0x4C])
    for a in range(0x0A, 0x0A + 8 * 7, 8):         # Hochbyte der JP-Ziele RST 08..38
        vorl[a] = (vorl[a] + RELOC) & 0xFF
    seite = bytearray(0x100)
    seite[0:0x4C] = vorl
    seite[0x66:0x69] = bytes([0xC3, 0x8F, 0x0A])    # NMI: JP 0A8FH
    return bytes(seite)


class SiosDis(Z80Disassembler):
    """Z80Disassembler mit den relativen Sprung-/Aufrufformen von SIOS."""

    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.auto_kom = {}

    def decode_one(self, addr):
        m, ln, t, e = super().decode_one(addr)
        d = self.data
        o = addr - self.org
        if ln == 3 and (d[o] == 0xCD or (d[o] & 0xC7) == 0xC4):
            n = d[o + 1] | d[o + 2] << 8
            if n in (0x2B, 0x3B) and o >= 3 and d[o - 3] == 0x11:
                de = d[o - 2] | d[o - 1] << 8
                t = [(addr + 3 + de) & 0xFFFF]
                if n == 0x2B and d[o] == 0xCD:
                    e = True                       # unbedingter relativer Sprung
            elif n == 0x36 and d[o] == 0xCD and o + 7 < len(d):
                # CALL 0036H ; LD DE,n ; ADD HL,DE ; PUSH HL ; RET/RETI  -> Folgeadresse + n
                if d[o + 3] == 0x11 and d[o + 6] == 0x19 and d[o + 7] == 0xE5:
                    t = [(addr + 3 + (d[o + 4] | d[o + 5] << 8)) & 0xFFFF]
        if m == "RST 08H" and o + 1 < len(d):
            # SVC: ein Kennbyte hinter dem Befehl gehoert dazu
            self.auto_kom[addr] = f"SVC, Kennbyte {d[o + 1]:02X}H"
            return m, 2, t, e
        return m, ln, t, e


def block_grenzen(a):
    return (a - SYS_ANF) % 0x400


# ---------------------------------------------------------------------------------------
# Einstiege, die nicht aus dem Fluss folgen
# ---------------------------------------------------------------------------------------
EINSTIEGE = {
    0x089D: "SYSTART",
    0x0066: "NMI",
    0x0A8F: "NMI_BEHANDLUNG",
    0x0A8C: "ISR_LEER",
}


def seeds(mem):
    s = dict(EINSTIEGE)
    for v in (0x08, 0x10, 0x18, 0x28):                   # RST-Vektoren (JP-Ziele)
        s[mem[v + 1] | mem[v + 2] << 8] = None
    for i in range(0, 0x40, 2):                          # Sprungtabelle 0800H (CALL 0042H, L = 2i+1)
        w = (mem[0x800 + i] | mem[0x801 + i] << 8) + 0x800
        if w >= 0x0A00 and w < 0x9000:
            s[w & 0xFFFF] = None
    for a in range(0x274, 0x280, 2):                     # IM-2-Tabelle (Vorlage 088CH.. + 0800H)
        w = (mem[0x88C + a - 0x274] | mem[0x88D + a - 0x274] << 8) + 0x800
        s[w & 0xFFFF] = None
    for k in range(34):                                  # Modulinit bei Block+2
        s[0x800 + 0x400 * k + 2] = None
    return s


def block_kopf_oder_crc(a):
    r = (a - SYS_ANF) % 0x400
    return a >= SYS_ANF and (r < 2 or r >= 0x3FE)


def bauen(img):
    mem = bytearray(0x10000)
    mem[0x800:0x800 + SYS_LEN] = img
    mem[0:0x100] = tiefe_seite(img)
    return bytes(mem[:SYS_ANF + SYS_LEN])


def ist_ende(m):
    return m.startswith(("RET", "JP ", "JR ")) and not m.startswith(("RET ", "JP ", "JR ")) or \
        m in ("RET", "RETI", "RETN", "JP (HL)", "JP (IX)", "JP (IY)") or \
        (m.startswith(("JP ", "JR ")) and "," not in m)


def gueltig_befehl(m):
    return not m.startswith("DB ")


def abdecken(dis, mem, seeds_):
    for a in seeds_:
        if 0 <= a < len(mem) and not block_kopf_oder_crc(a):
            dis.recursive_decode(a)
    # CALL-/JP-Ziele aus dem Fluss sind schon verfolgt; jetzt die Luecken tentativ fuellen
    tentativ = set()
    for _ in range(6):
        neu = 0
        a = SYS_ANF
        ende = SYS_ANF + SYS_LEN
        while a < ende:
            if a in dis.instructions:
                a += dis.instructions[a][1]
                continue
            if (a not in dis.code_addrs and not block_kopf_oder_crc(a) and a - 1 >= 0
                    and mem[a] not in (0x00, 0xFF)):
                vor = [x for x in range(a - 4, a) if x in dis.instructions
                       and x + dis.instructions[x][1] == a]
                if vor and ist_ende(dis.instructions[vor[-1]][0]):
                    probe = SiosDis(mem, org=0)
                    probe.auto_kom = {}
                    x = a
                    n = 0
                    ok = False
                    while x < ende and not block_kopf_oder_crc(x) and n < 400:
                        m, ln, t, e = probe.decode_one(x)
                        if ln == 0 or not gueltig_befehl(m) or x in dis.code_addrs:
                            break
                        n += 1
                        x += ln
                        if e or ist_ende(m):
                            ok = n >= 3
                            break
                    if ok:
                        dis.recursive_decode(a)
                        tentativ.add(a)
                        neu += 1
            a += 1
        if not neu:
            break
    return tentativ


# ---------------------------------------------------------------------------------------
# Namen und Kommentare (Beleg: [Lauf] am Emulator, [Code] aus dem Disassemblat, [?] Annahme)
# ---------------------------------------------------------------------------------------
NAMEN = {
    # Seite 0 (aus der Vorlage 0840H.. aufgebaut, Stand nach 089DH..0935H)
    0x0000: "RESTART", 0x0004: "Z_0004", 0x0008: "RST08_SVC", 0x0010: "RST10_MONITOR",
    0x0018: "RST18", 0x0020: "RST20_LEER", 0x0028: "RST28", 0x002B: "REL_JMP",
    0x0030: "RST30_LEER", 0x0033: "GETPC_DE", 0x0036: "GETPC_HL", 0x0038: "RST38",
    0x003B: "REL_CALL", 0x0042: "TAB_JMP", 0x004C: "AAWA", 0x0066: "NMI",
    # Systembeginn und Pruefung
    0x089D: "SYSTART", 0x0949: "INDEX_SUCHE", 0x0983: "BLOCK_CRC", 0x09A3: "ERR_SY02",
    0x09B6: "BLOCK_FOLGE", 0x09C2: "ERR_SY06", 0x09D7: "BLOCK_OK", 0x0A2C: "SYSTART_2",
    0x0A56: "GERAETE_VORBELEGUNG", 0x0A6E: "SPS_INT_FREI", 0x0A8C: "ISR_LEER",
    0x0A8F: "NMI_BEHANDLUNG", 0x0ABA: "RAM_ABTASTEN", 0x0AE1: "RAM_ABTASTEN_ENDE",
    0x0AFF: "SPS_SCHARF", 0x0B08: "CRC_INIT", 0x0B0B: "CRC_BLOCK", 0x0B32: "FEHLER_ANZEIGE",
    0x0D4B: "ISR_BSPIO_A", 0x0D56: "ISR_BSPIO_A_KOERPER", 0x0D6A: "ISR_CTC3",
    0x0E7F: "MONITOR", 0x1AC8: "RST18_ROUTINE", 0x1BA5: "SVC", 0x14D2: "RST28_ROUTINE",
    0x1E48: "ISR_BSPIO_B", 0x27AB: "ISR_CTC2_ZEIT", 0x6A53: "FLOPPY_NEUSTART",
}

KOMMENTAR = {
    0x0000: "Vorlage: JP 0000H; nach dem Start JP 6A53H (Neustart ueber den Floppytreiber) [Lauf]",
    0x0004: "Wortzelle, Vorlage 07FCH, im Lauf 07E8H [?]",
    0x0008: "RST 08H = SVC mit EINEM Kennbyte hinter dem Befehl; Ziel 1BA5H [Code]",
    0x0010: "RST 10H = zurueck in den Monitor (Fehlerausstieg); Ziel 0E7FH [Code]",
    0x0020: "RST 20H und 30H zeigen auf 0800H (Daten: 00 90 C9) - unbenutzt [Code]",
    0x0028: "RST 28H, Ziel 14D2H (Prozess-/Aufgabenstart? [?])",
    0x002B: "CALL 002BH mit DE = n: Sprung nach (Rueckkehradresse + DE) - relativer JP, kein Ruecksprung",
    0x0033: "CALL 0033H: DE = Folgeadresse, Ausfuehrung geht dort weiter",
    0x0036: "CALL 0036H: HL = Folgeadresse, Ausfuehrung geht dort weiter (Lage der Position)",
    0x003B: "CALL 003BH mit DE = n: Aufruf von (Rueckkehradresse + DE) - relativer CALL, RET kehrt zurueck",
    0x0042: "CALL 0042H mit L = ungerader Index: Sprung ueber die Wort-Tabelle 0800H (Eintrag + 0800H)",
    0x004C: "Zelle 004CH/004DH = AAWA (Anfang Anwenderbereich), nach dem Laden 9000H [Lauf]",
    0x0066: "NMI-Vektor: JP 0A8FH (Q240: unerlaubter E/A-Befehl)",
    0x0800: "Tabelle der Einstiege (CALL 0042H, L = 2*i+1): Wort + 0800H; Wort 0 = 9000H (AAWA)",
    0x0840: "Vorlage der Seite 0 (0840H..088BH nach 0000H..004BH kopiert, Hochbytes +08H)",
    0x088C: "Vorlage der IM-2-Tabelle (ab 0274H, 6 Worte + 0800H)",
    0x0898: "'INDEX' - Kennung, nach der SYSTART in der Seite FC00H abwaerts den ROM-Bereich sucht",
    0x089D: "[Lauf] Einsprung vom Lade-ROM (JP 0074H -> HL=(0462H), L=9DH): EI, I=02, loescht 0400H..07FFH (Lader weg)",
    0x08AE: "BS-PIO A Daten (08H) / B Steuerung (0BH) einrichten, Port 0AH Bit0+3 = 0: Lade-ROM aus, ZVE2 wartet",
    0x08C1: "ERAM (0060H) um 03FFH erhoehen; 0070H Bit5 loeschen; SP = 0400H",
    0x08D0: "Seite 0 aufbauen: 0840H..088BH -> 0000H.., Vorlage Hochbytes +08H (RST-Ziele, NMI-Ziel 0066H=JP 0A8FH)",
    0x0921: "CTC 0CH/0EH/0FH und BS-PIO 09H/0BH einrichten (Vektoren 78H.., PIO A 74H, PIO B 76H)",
    0x0942: "005DH := JP (Sprung zum Moduleinsprung, Zeiger 005EH/005FH)",
    0x0949: "Suche 'INDEX' (0898H-Vorlage) in 1-KB-Seiten abwaerts ab FC00H; Fund -> AAWO (006CH)",
    0x0964: "Blockpruefung: CRC-CCITT je 1-KB-Block (Kopf 2 Byte, 1022 Byte CRC, 2 Byte Soll)",
    0x0983: "4 x CALL 0042H L=19H/1BH = CRC_INIT/CRC_BLOCK: 254+3*256 = 1022 Byte [Lauf]",
    0x09A3: "Fehler: BC='02' DE='SY' -> 'ERR SY 02' (CRC falsch)",
    0x09B6: "Kopf des Blocks: Low-Nibble = Blockfolge (005CH) [Code]",
    0x09C2: "Fehler: 'ERR SY 06' (Blockfolge falsch)",
    0x09D7: "CRC gut: Moduleinsprung (005DH -> Block+2) wenn Kopf-Flag es verlangt [Lauf: 10 Module]",
    0x0A3C: "AAWA auf 64-Byte-Grenze aufrunden; dann relativer Aufruf von RAM_ABTASTEN (DE=0075H -> 0ABAH)",
    0x0A56: "Geraetetabelle 0300H..0315H: Vorbelegung FFH ersetzen (Dialog <- Drucker, Programmgeraet <- Kassette)",
    0x0A6E: "BS-PIO A: Steuerwort 97H, Maske F7H = nur A3 (SPS-Indikator, Q240) loest Interrupt aus",
    0x0A76: "SP = 0400H; Ziel 0010H (Monitor) auf den Stapel; AAWO<>0 -> relativer Sprung nach ROM-Programm",
    0x0A8C: "Platzhalter-ISR (EI/RETI) in der IM-2-Tabelle, Vorlage fuer CTC-Kanal 2",
    0x0A8F: "NMI (Q240: unerlaubter E/A-Befehl): Fehlercode BC='10' (3130H), DE='SY'; BS-PIO A Bit2 gesetzt -> ERR SY ausgeben, sonst Code '29' [Code]",
    0x0ABA: "[Lauf] DI; Speicher in 64-Byte-Schritten lesen (RAM-Groesse/Lage), AAWA..ERAM abtasten - hier endete der Lauf",
    0x0AFF: "[Lauf] SPS_SCHARF: IN A,(0AH); RES 2 (= /SPS-ESR low = Schutz AKTIV laut Unterlagen); OUT; EI; RET",
    0x0B08: "CRC_INIT: BC := FFFFH, laeuft in CRC_BLOCK weiter",
    0x0B0B: "CRC_BLOCK: E Byte ab (IY) in BC (CRC-CCITT, B=hoch); IY zeigt danach auf die Soll-CRC",
    0x0B32: "FEHLER_ANZEIGE (Tabelle L=2BH): BC/DE (z. B. '02','SY') nach 0016H/0026H retten, Systemzellen sichern (0600H) [Code]",
    0x0BA4: "Fehlertexte: CANC NFND UNIT R/W  VOL  HDR1 SEQU SAVE AREA SIZE",
    0x0D4B: "ISR BS-PIO A (SPS-Indikator): EI; HL=Folgeadresse; RETI nach 0D56H (Trampolin)",
    0x0D56: "Koerper der ISR - bei dauernd anstehendem SPS-Indikator nie erreicht (Interrupt kommt vorher) [Lauf]",
    0x0D6A: "ISR CTC-Kanal 3 (Vektor 7EH)",
    0x0E7F: "MONITOR [Code/?]: RST 10H-Ziel; 00FEH=04H, 00FFH=0, Tabelle 0107H..; Systemzeile ' MONITOR ===== SIOS 1526/1.1 (M)' bei 0EEBH",
    0x0EEB: "' MONITOR ===== SIOS 1526/1.1 (M)' (Anzeigecode, '_' = Zeilenende)",
    0x10DA: "' PROGRAM ==:   SIOS 1526/1.1  P==' (Programm-Systemzeile)",
    0x1BA5: "SVC (RST 08H): IX = Adresse des Kennbytes, Register retten, Kennbyte C=(IX+0): Bit7/Bit5 steuern die Auswertung",
    0x1AC8: "RST 18H: Fehler-/Abbruchbehandlung (BC='10' = 3130H, Tabelle L=27H) [Code]",
    0x14D2: "RST 28H: setzt 00FEH Bit0=0, 0100H=02xx (Prozesszeiger), 000CH/005EH aus Folgeadresse",
    0x1E48: "ISR BS-PIO B (Bit1 /INT-BS: OUT 00H aus der Makroebene): 16 Eintraege ab 0111H (je 16H Byte) durchsuchen",
    0x27AB: "ISR Zeitgeber (CTC-Kanal 2, Vektor 7CH, nach 0A8CH gepatcht); 0114H Bit7, (00BCH)=Port, (00FBH)",
    0x6A53: "Neustart-Einsprung (RST 00H-Vektor): Floppytreiber K5122 (Ports 10H..17H) im Bereich 6800H..6BFFH",
}

# Datenbereiche (nicht als Code ausgeben): (von, bis_exklusiv, Text)
DATEN = [
    (0x0800, 0x0840, "Einstiegstabelle fuer CALL 0042H (Worte, + 0800H)"),
    (0x0840, 0x088C, "Vorlage Seite 0 (RST-Vektoren, Zellen)"),
    (0x088C, 0x0898, "Vorlage IM-2-Tabelle (6 Worte + 0800H)"),
    (0x0898, 0x089D, "'INDEX'"),
]


# Modulanfaenge (Block mit Folge-Nummer 0 im Kopf); der Lader ruft Block+2 [Lauf: 10 Aufrufe].
# Zweck nach den benutzten Ports und Texten abgeleitet - [?] wo nicht belegt.
MODUL_INFO = {
    0: "Modul 0 (0800H-1FFFH): Grundmodul - Start, Pruefung, Fehler, Monitor, SVC; Init = RET",
    6: "Modul 1 (2000H-2FFFH): Systemdienste, viele OUT 00H (SVC aus der Makroebene), Zeitgeber-ISR 27ABH [?]",
    10: "Modul 2 (3000H-3FFFH): benutzt Port 40H und 0AH [?]",
    14: "Modul 3 (4000H-43FFH, ein Block)",
    15: "Modul 4 (4400H-47FFH, ein Block)",
    16: "Modul 5 (4800H-4FFFH, zwei Bloecke)",
    18: "Modul 6 (5000H-5FFFH): K8025: CTC A34 (58H), SIO A32 Kanal B (5EH/5FH) - Drucker/Tastatur-Treiber [Ports]",
    22: "Modul 7 (6000H-6FFFH): Floppytreiber K5122 (10H-17H), Neustart 6A53H; 'PHASELB' bei 6CEEH [Ports/Text]",
    26: "Modul 8 (7000H-83FFH, fuenf Bloecke) [?]",
    31: "Modul 9 (8400H-8FFFH): SIO A33 (50H/51H), Register 56H, CTC ZRE 0CH - DFUE/V.24 [Ports]; 'OFF-LINE' bei 8EE8H",
}


def modul_kopf(k, mem):
    a = SYS_ANF + 0x400 * k
    t = f"Block {k:2d}: Kopf {mem[a]:02X} {mem[a + 1]:02X}, CRC {mem[a + 0x3FE]:02X}{mem[a + 0x3FF]:02X}"
    return t + (" - " + MODUL_INFO[k] if k in MODUL_INFO else "")


def fmt_daten(mem, a, e, text):
    zeilen = []
    for i in range(0, e - a, 16):
        stk = mem[a + i: min(a + i + 16, e)]
        if len(stk) > 3 and all(0x20 <= b < 0x7F for b in stk):
            db = "'" + bytes(stk).decode("ascii").replace("'", "''") + "'"
        else:
            db = ",".join(f"{b:02X}H" for b in stk)
        kom = f"\t\t;{text}" if i == 0 and text else ""
        zeilen.append(f"{a + i:04X}  {'':<14}\tDB\t{db}{kom}")
    return zeilen


HDR_SYS = """; ============================================================================
; sysare11.prn  -  SIOS 1526/1.1 (A5120), Systembereich SYSARE11, 0800H-8FFFH
;
; Quelle: Datei SYSARE11 (HDR1 der Etikettenspur) der Diskette
; tools/sios_a5120/original/SIOS1526_SYLs.hfe, ab Datei-Sektor 26 (Spur 2) geladen
; wie vom SYL-Lader: 272 Sektoren = 8800H Byte nach 0800H..8FFFH, Einsprung 089DH.
; Seite 0 (0000H-00FFH) ist die Folge von SYSTART (Vorlage 0840H nach 0000H kopiert).
; Erzeugt von tools/sios_a5120/gen_prn.py - nicht von Hand aendern.
;
; Aufbau: 34 Bloecke zu 1 KB.  Je Block: 2 Byte Kopf (Kennung/Flags), CRC-CCITT (FFFFH)
; ueber die ersten 1022 Byte am Ende.  Moduleinsprung bei Block+2.
; Relative Spruenge: LD DE,n / CALL 002BH (Sprung) bzw. 003BH (Aufruf) -> Ziel = Folgeadresse+n.
; Angaben: [Lauf] am Emulator belegt, [Code] aus dem Code gelesen, [?] Annahme.
; Ports: 00H INT-BS (Makroebene), 02H /RES-SPA, 08H-0BH BS-PIO, 0CH-0FH CTC ZRE,
;        10H-17H K5122, 40H (AUP?), 50H-53H SIO A33 (DFUE), 54H-57H Register A31,
;        58H-5BH CTC A34, 5CH-5FH SIO A32 (Tastatur K7637/Drucker).
; STATISCH: tentativ zusammengesetzte Luecken sind mit '[vermutet]' markiert.
; ============================================================================"""


def rendere(mem, dis, tentativ):
    out = [HDR_SYS, "", "\tORG\t0000H", ""]
    daten = {a: (e, t) for a, e, t in DATEN}
    for k in range(34):                       # Kopf und CRC jedes Blocks sind Daten
        a = SYS_ANF + 0x400 * k
        daten[a] = (a + 2, modul_kopf(k, mem))
        daten[a + 0x3FE] = (a + 0x400, "CRC-CCITT")
    addr = 0
    ende = SYS_ANF + SYS_LEN
    while addr < ende:
        if addr == 0x100:                     # Zellen/Tabellen 0100H..07FFH sind Laufzeitdaten
            out.append(f"{0x100:04X}  {'':<14}\tDS\t{0x800 - 0x100}\t\t;Laufzeitdaten 0100H-07FFH (Systemzellen, Tabellen, Stapel)")
            addr = 0x800
            continue
        if addr in NAMEN:
            out.append(f"{NAMEN[addr]}:")
        if addr in daten:
            e, t = daten[addr]
            out += fmt_daten(mem, addr, e, t)
            addr = e
            continue
        if addr in dis.instructions:
            m, ln, raw = dis.instructions[addr]
            hexb = " ".join(f"{b:02X}" for b in raw)
            kom = KOMMENTAR.get(addr, "") or dis.auto_kom.get(addr, "")
            if addr in tentativ:
                kom = (kom + " " if kom else "") + "[vermutet]"
            out.append(f"{addr:04X}  {hexb:<14}\t{m}" + (f"\t\t;{kom}" if kom else ""))
            addr += ln
            continue
        e = addr + 1
        while (e < ende and e not in dis.instructions and e not in daten and e not in NAMEN
               and e - addr < 16 and e not in KOMMENTAR):
            e += 1
        out += fmt_daten(mem, addr, e, KOMMENTAR.get(addr, ""))
        addr = e
    out += ["", "\tEND"]
    return out


def erzeuge_sys(syl, img):
    mem = bauen(img)
    dis = SiosDis(mem, org=0)
    s = seeds(mem)
    s[0x27AB] = None                          # zur Laufzeit in die IM-2-Tabelle gepatcht
    s[0x6A53] = None                          # zur Laufzeit JP 6A53H bei 0000H [Lauf]
    tent = abdecken(dis, mem, s)
    zeilen = rendere(mem, dis, tent)
    open(OUT_SYS, "w", encoding="utf-8").write("\n".join(zeilen) + "\n")
    n = sum(l for _, l, _ in dis.instructions.values())
    print(f"geschrieben: {os.path.relpath(OUT_SYS, ROOT)} ({len(zeilen)} Zeilen, "
          f"{n} Byte als Code, {len(tent)} vermutete Einstiege)")


SYL_NAMEN = {
    0x0400: "SYL_KENNUNG", 0x0410: "CRC_CCITT", 0x0437: "SYL_START", 0x0450: "SEKTORZAHL_128",
    0x045C: "LADEN_FERTIG", 0x0475: "LADEN_PARAMETER", 0x04A0: "LADEN_GROESSE", 0x04BF: "SPUR_SCHLEIFE",
    0x04DC: "SPUR_NAECHSTE", 0x04FF: "SCHRITT", 0x0511: "SEKTOREN_LESEN", 0x051C: "WARTE_ZVE2",
    0x052D: "SEKTOR_PRUEFEN", 0x056B: "WIEDERHOLUNG", 0x0575: "FEHLER_ZAEHLER", 0x057E: "SEKTORZAHL_TAB",
}

SYL_KOMMENTAR = {
    0x0400: "'SYL' (vom Lade-ROM bei 01B6H geprueft); 0403H=04 (OUT 12H, Bit2 = MFM-Pfad), 0404H/0405H = 8800H Byte, 0406H = 00 (Ladeadresse 0800H; <>0 -> 4000H, OUT EEH=3CH)",
    0x0410: "CRC-CCITT je Byte: BC (B hoch) fortschreiben, IY Zeiger, E Anzahl [Lauf: gleiche Rechnung wie CRC_BLOCK im System]",
    0x0437: "Einsprung vom Lade-ROM (CALL 0437H aus 0174H): CRC ueber 0410H..05E9H (2 x EDH Byte), Soll in 05FEH/05FFH",
    0x044D: "CRC falsch -> JP 0140H (Lade-ROM: naechstes Laufwerk)",
    0x0450: "Sektorlaenge 128 B: Bytezahl*2, High/Low getauscht = Sektorzahl (8800H -> 0110H = 272)",
    0x045C: "Fertig: AAWA (004CH) := Endzeiger [03F0H] = 9000H; (0069H) := Endzeiger-0400H; OUT 11H,03H; DI; K5122 abwaehlen; RET ins Lade-ROM (0177H)",
    0x0475: "Ladeadresse HL=0800H nach [03F0H] (laufender Zeiger) und [03FAH] (Start; das Lade-ROM springt spaeter nach [03FAH]+9DH = 089DH)",
    0x048A: "HL = (0404H) Bytezahl; A = (0403H) -> OUT 12H; B = A; Pfadbyte [03FDH]=85H (MFM)/E=7EH",
    0x04BF: "(07F0H) = BC; [03F3H] = 02H = erste Spur; BC=0001H; C = Sektoren je Spur aus Tabelle 057EH (Index = Groessencode)",
    0x04DC: "HL -= Sektoren je Spur; Rest <0 -> letzte Spur mit dem Rest; (07F2H) = Sektoren dieser Spur",
    0x04FF: "Schrittimpulse an K5122 (Port 10H: 2DH/ADH), D = Schrittzahl 2: Spur 2 anfahren",
    0x0511: "CALL 0194H (Lade-ROM: ZVE2 starten, liest die Spur nach 0700H..), Interrupt-Vektoren von Port 11H freigeben",
    0x051C: "wartet auf [03F8H]: 0 = laeuft, 1 = Fehler, sonst fertig",
    0x052D: "je Sektor: CRC (CALL 0410H) ueber die gelesenen Daten gegen die Tabelle ab 0700H pruefen; bei Fehler -> 0575H",
    0x0557: "Spur fertig: Zeiger [03F0H] = IY; [03F3H] (Spur) += 1; weiter 04DCH",
    0x0575: "Fehler: Wiederholungszaehler [07F6H] (7) -1; 0 -> JP 0140H (Lade-ROM-Fehlerweg)",
    0x057E: "Sektoren je Spur nach Groessencode: 1A 0F 08 04 2C | 1A 0F 08 (26/15/8/4, 44)",
    0x05FE: "CRC-CCITT des Laders",
}


def erzeuge_syl(syl):
    dis = SiosDis(syl, org=0x400)
    for a in (0x437, 0x410, 0x450, 0x45C, 0x475, 0x4A0, 0x4BF, 0x4DC, 0x4FF, 0x511, 0x51C, 0x52D, 0x56B, 0x575):
        dis.recursive_decode(a)
    out = ["; ============================================================================",
           "; syl_lader.prn  -  SYL-Lader der SIOS-1526-Systemdiskette (Spur 0, Sektor 1-4), 0400H-05FFH",
           "; Das Lade-ROM (A26) laedt die 4 Sektoren nach 0400H und ruft 0437H.  Der Lader liest",
           "; die Systemdatei (272 Sektoren ab Spur 2) nach 0800H und kehrt ins Lade-ROM zurueck.",
           "; Erzeugt von tools/sios_a5120/gen_prn.py.  [Lauf] = am Emulator belegt.",
           "; ============================================================================",
           "", "\tORG\t0400H", ""]
    ende = 0x400 + len(syl)
    a = 0x400
    while a < ende:
        if a in SYL_NAMEN:
            out.append(f"{SYL_NAMEN[a]}:")
        if a in dis.instructions:
            m, ln, raw = dis.instructions[a]
            kom = SYL_KOMMENTAR.get(a, "")
            out.append(f"{a:04X}  {' '.join(f'{b:02X}' for b in raw):<14}\t{m}" + (f"\t\t;{kom}" if kom else ""))
            a += ln
            continue
        e = a + 1
        while e < ende and e not in dis.instructions and e not in SYL_NAMEN and e - a < 16 and e not in SYL_KOMMENTAR:
            e += 1
        out += fmt_daten(b"\0" * 0x400 + bytes(syl), a, e, SYL_KOMMENTAR.get(a, ""))
        a = e
    out += ["", "\tEND"]
    open(OUT_SYL, "w", encoding="utf-8").write("\n".join(out) + "\n")
    print(f"geschrieben: {os.path.relpath(OUT_SYL, ROOT)} ({len(out)} Zeilen)")


def main():
    syl, img = lade()
    erzeuge_sys(syl, img)
    erzeuge_syl(syl)


if __name__ == "__main__":
    main()

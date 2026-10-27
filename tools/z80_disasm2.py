#!/usr/bin/env python3
"""
z80_disasm2.py - DER Z80-Disassembler der Werkzeugkiste (Entwurf 40, W1-W3)
===========================================================================

Zwei Ausgabearten (--format):

  listing  (Vorgabe) Listing `ADDR  hexbytes  Befehl  ; Kommentar` fuer Analysen.
           Recursive descent ab --org/--entry, optional --linear/--range,
           ASCII-Zeichenketten.  Die Klasse Z80Disassembler wird von
           gen_*_prn.py und disasm_difftest.py importiert - API nicht aendern.
  mac      ASSEMBLIERBARER M80-Quelltext (Marken, DB/DW/Text, FILLFF-Makro,
           .PHASE je Ladesegment, Segmentmarken SEGnnnn/SEGnnnnE, Auffuellung
           auf 256 B).  Richtig ist, was beim Neubau BYTEGLEICH herauskommt;
           ob eine Stelle als Code oder Daten gefuehrt wird, darf falsch sein.
           Aufteilung, Namen und Kommentare stehen in einer Kartendatei
           (--karte), damit sich die Quelle aus Abzug + Karte reproduzierbar
           erzeugen laesst.

Kartendatei (hexadezimale Zahlen; `#` beginnt eine Kommentarzeile):

    seg  <adresse> <datei-offset> <laenge>   ein Ladesegment der Datei
    code <adresse> [<adresse> ...]           Einstieg(e) der Codeverfolgung
    db   <von> <bis>                         Daten, Bytes        (bis einschl.)
    dw   <von> <bis>                         Daten, Woerter (Adressen -> Marken)
    dwc  <von> <bis>                         wie dw, die Ziele sind zugleich Code-Einstiege
    str  <von> <bis>                         Zeichenketten mit vorangestelltem Laengenbyte
    txt  <von> <bis>                         Text (Zeichen als 'abc', Rest als Zahlen)
    name <adresse> <marke>                   eigene Marke statt Lxxxx
    kom  <adresse> <text>                    Kommentarzeile VOR der Stelle (mehrfach moeglich)
    ik   <adresse> <text>                    Zeilenkommentar HINTER dem Befehl/der Datenzeile
    eq   <marke> <adresse>                   Marke ausserhalb der Segmente (EQU)

Nicht eindeutig assemblierbare oder undokumentierte Befehle (SLL, ED-Dubletten,
DD/FD-Halbregister, ED 63/6B-Langform ...) gibt der mac-Modus als DB aus.
"""

import sys
import struct
import re

ORG = 0x0100

def read_binary(path):
    with open(path, 'rb') as f:
        return f.read()

# =================== Z80 INSTRUCTION TABLE ===================
# Each entry: (mnemonic_template, length, operand_types)
# Operand types: 'n'=byte, 'nn'=word, 'e'=relative, 'd'=displacement

def signed8(b):
    return b - 256 if b >= 128 else b

class Z80Disassembler:
    def __init__(self, data, org=0x0100):
        self.data = data
        self.org = org
        self.labels = {}  # addr -> label_name
        self.code_addrs = set()  # addresses that are code
        self.data_ranges = []  # (start, end) ranges that are data
        self.string_ranges = []  # (start, end) ranges that are strings
        self.comments = {}  # addr -> comment
        self.instructions = {}  # addr -> (mnemonic, length, raw_bytes)
        self.call_targets = set()
        self.jump_targets = set()
        
    def byte_at(self, addr):
        offset = addr - self.org
        if 0 <= offset < len(self.data):
            return self.data[offset]
        return None
    
    def word_at(self, addr):
        offset = addr - self.org
        if 0 <= offset + 1 < len(self.data):
            return self.data[offset] | (self.data[offset+1] << 8)
        return None
    
    def decode_one(self, addr):
        """Decode one Z80 instruction at addr. Returns (mnemonic, length, targets, is_end)"""
        offset = addr - self.org
        if offset < 0 or offset >= len(self.data):
            return None, 0, [], False
        
        d = self.data
        o = offset
        
        def b(n):
            if o+n < len(d): return d[o+n]
            return 0
        
        def w(n):
            if o+n+1 < len(d): return d[o+n] | (d[o+n+1] << 8)
            return 0
        
        def rel(n):
            v = b(n)
            return addr + n + 1 + (v - 256 if v >= 128 else v)
        
        opcode = b(0)
        targets = []
        is_end = False  # True if this instruction ends a basic block (unconditional jump/ret)
        
        # === Prefix CB ===
        if opcode == 0xCB:
            op2 = b(1)
            r_names = ['B','C','D','E','H','L','(HL)','A']
            r = r_names[op2 & 7]
            bit = (op2 >> 3) & 7
            group = (op2 >> 6) & 3
            if group == 0:
                ops = ['RLC','RRC','RL','RR','SLA','SRA','SLL','SRL']
                return f"{ops[bit]} {r}", 2, [], False
            elif group == 1:
                return f"BIT {bit},{r}", 2, [], False
            elif group == 2:
                return f"RES {bit},{r}", 2, [], False
            else:
                return f"SET {bit},{r}", 2, [], False
        
        # === Prefix ED ===
        if opcode == 0xED:
            op2 = b(1)
            rp_names = ['BC','DE','HL','SP']
            rp = rp_names[(op2 >> 4) & 3]
            
            ed_map = {
                0x40: ("IN B,(C)", 2), 0x41: ("OUT (C),B", 2),
                0x42: (f"SBC HL,BC", 2), 0x43: (f"LD ({w(2):04X}H),BC", 4),
                0x44: ("NEG", 2), 0x45: ("RETN", 2),
                0x46: ("IM 0", 2), 0x47: ("LD I,A", 2),
                0x48: ("IN C,(C)", 2), 0x49: ("OUT (C),C", 2),
                0x4A: (f"ADC HL,BC", 2), 0x4B: (f"LD BC,({w(2):04X}H)", 4),
                0x4C: ("NEG", 2), 0x4D: ("RETI", 2),
                0x4E: ("IM 0", 2), 0x4F: ("LD R,A", 2),
                0x50: ("IN D,(C)", 2), 0x51: ("OUT (C),D", 2),
                0x52: (f"SBC HL,DE", 2), 0x53: (f"LD ({w(2):04X}H),DE", 4),
                0x54: ("NEG", 2), 0x55: ("RETN", 2),
                0x56: ("IM 1", 2), 0x57: ("LD A,I", 2),
                0x58: ("IN E,(C)", 2), 0x59: ("OUT (C),E", 2),
                0x5A: (f"ADC HL,DE", 2), 0x5B: (f"LD DE,({w(2):04X}H)", 4),
                0x5C: ("NEG", 2), 0x5D: ("RETN", 2),
                0x5E: ("IM 2", 2), 0x5F: ("LD A,R", 2),
                0x60: ("IN H,(C)", 2), 0x61: ("OUT (C),H", 2),
                0x62: (f"SBC HL,HL", 2), 0x63: (f"LD ({w(2):04X}H),HL", 4),
                0x64: ("NEG", 2), 0x65: ("RETN", 2),
                0x66: ("IM 0", 2), 0x67: ("RRD", 2),
                0x68: ("IN L,(C)", 2), 0x69: ("OUT (C),L", 2),
                0x6A: (f"ADC HL,HL", 2), 0x6B: (f"LD HL,({w(2):04X}H)", 4),
                0x6C: ("NEG", 2), 0x6D: ("RETN", 2),
                0x6E: ("IM 0", 2), 0x6F: ("RLD", 2),
                0x70: ("IN (C)", 2), 0x71: ("OUT (C),0", 2),
                0x72: (f"SBC HL,SP", 2), 0x73: (f"LD ({w(2):04X}H),SP", 4),
                0x74: ("NEG", 2), 0x75: ("RETN", 2),
                0x76: ("IM 1", 2), 0x77: ("NOP", 2),
                0x78: ("IN A,(C)", 2), 0x79: ("OUT (C),A", 2),
                0x7A: (f"ADC HL,SP", 2), 0x7B: (f"LD SP,({w(2):04X}H)", 4),
                0x7C: ("NEG", 2), 0x7D: ("RETN", 2),
                0x7E: ("IM 2", 2), 0x7F: ("NOP", 2),
                0xA0: ("LDI", 2), 0xA1: ("CPI", 2),
                0xA2: ("INI", 2), 0xA3: ("OUTI", 2),
                0xA8: ("LDD", 2), 0xA9: ("CPD", 2),
                0xAA: ("IND", 2), 0xAB: ("OUTD", 2),
                0xB0: ("LDIR", 2), 0xB1: ("CPIR", 2),
                0xB2: ("INIR", 2), 0xB3: ("OTIR", 2),
                0xB8: ("LDDR", 2), 0xB9: ("CPDR", 2),
                0xBA: ("INDR", 2), 0xBB: ("OTDR", 2),
            }
            if op2 in ed_map:
                mn, ln = ed_map[op2]
                end = op2 in (0x45, 0x4D, 0x55, 0x5D, 0x65, 0x6D, 0x75, 0x7D)
                return mn, ln, [], end
            return f"DB 0EDH,{op2:02X}H", 2, [], False
        
        # === Prefix DD/FD (IX/IY) ===
        if opcode in (0xDD, 0xFD):
            ir = "IX" if opcode == 0xDD else "IY"
            op2 = b(1)
            
            # DD CB dd xx - bit operations on (IX+d)
            if op2 == 0xCB:
                disp = signed8(b(2))
                op3 = b(3)
                ds = f"+{disp}" if disp >= 0 else f"{disp}"
                bit = (op3 >> 3) & 7
                group = (op3 >> 6) & 3
                if group == 0:
                    ops = ['RLC','RRC','RL','RR','SLA','SRA','SLL','SRL']
                    if (op3 & 7) == 6:
                        return f"{ops[bit]} ({ir}{ds})", 4, [], False
                    else:
                        r_names = ['B','C','D','E','H','L','(HL)','A']
                        return f"{ops[bit]} ({ir}{ds}),{r_names[op3&7]}", 4, [], False
                elif group == 1:
                    return f"BIT {bit},({ir}{ds})", 4, [], False
                elif group == 2:
                    if (op3 & 7) == 6:
                        return f"RES {bit},({ir}{ds})", 4, [], False
                    else:
                        r_names = ['B','C','D','E','H','L','(HL)','A']
                        return f"RES {bit},({ir}{ds}),{r_names[op3&7]}", 4, [], False
                else:
                    if (op3 & 7) == 6:
                        return f"SET {bit},({ir}{ds})", 4, [], False
                    else:
                        r_names = ['B','C','D','E','H','L','(HL)','A']
                        return f"SET {bit},({ir}{ds}),{r_names[op3&7]}", 4, [], False
            
            disp = signed8(b(2))
            ds = f"+{disp}" if disp >= 0 else f"{disp}"
            
            dd_map = {
                0x09: (f"ADD {ir},BC", 2), 0x19: (f"ADD {ir},DE", 2),
                0x21: (f"LD {ir},{w(2):04X}H", 4),
                0x22: (f"LD ({w(2):04X}H),{ir}", 4),
                0x23: (f"INC {ir}", 2),
                0x24: (f"INC {ir}H", 2), 0x25: (f"DEC {ir}H", 2),
                0x26: (f"LD {ir}H,{b(2):02X}H", 3),
                0x29: (f"ADD {ir},{ir}", 2),
                0x2A: (f"LD {ir},({w(2):04X}H)", 4),
                0x2B: (f"DEC {ir}", 2),
                0x2C: (f"INC {ir}L", 2), 0x2D: (f"DEC {ir}L", 2),
                0x2E: (f"LD {ir}L,{b(2):02X}H", 3),
                0x34: (f"INC ({ir}{ds})", 3),
                0x35: (f"DEC ({ir}{ds})", 3),
                0x36: (f"LD ({ir}{ds}),{b(3):02X}H", 4),
                0x39: (f"ADD {ir},SP", 2),
                0x44: (f"LD B,{ir}H", 2), 0x45: (f"LD B,{ir}L", 2),
                0x46: (f"LD B,({ir}{ds})", 3),
                0x4C: (f"LD C,{ir}H", 2), 0x4D: (f"LD C,{ir}L", 2),
                0x4E: (f"LD C,({ir}{ds})", 3),
                0x54: (f"LD D,{ir}H", 2), 0x55: (f"LD D,{ir}L", 2),
                0x56: (f"LD D,({ir}{ds})", 3),
                0x5C: (f"LD E,{ir}H", 2), 0x5D: (f"LD E,{ir}L", 2),
                0x5E: (f"LD E,({ir}{ds})", 3),
                0x60: (f"LD {ir}H,B", 2), 0x61: (f"LD {ir}H,C", 2),
                0x62: (f"LD {ir}H,D", 2), 0x63: (f"LD {ir}H,E", 2),
                0x64: (f"LD {ir}H,{ir}H", 2), 0x65: (f"LD {ir}H,{ir}L", 2),
                0x66: (f"LD H,({ir}{ds})", 3),
                0x67: (f"LD {ir}H,A", 2),
                0x68: (f"LD {ir}L,B", 2), 0x69: (f"LD {ir}L,C", 2),
                0x6A: (f"LD {ir}L,D", 2), 0x6B: (f"LD {ir}L,E", 2),
                0x6C: (f"LD {ir}L,{ir}H", 2), 0x6D: (f"LD {ir}L,{ir}L", 2),
                0x6E: (f"LD L,({ir}{ds})", 3),
                0x6F: (f"LD {ir}L,A", 2),
                0x70: (f"LD ({ir}{ds}),B", 3), 0x71: (f"LD ({ir}{ds}),C", 3),
                0x72: (f"LD ({ir}{ds}),D", 3), 0x73: (f"LD ({ir}{ds}),E", 3),
                0x74: (f"LD ({ir}{ds}),H", 3), 0x75: (f"LD ({ir}{ds}),L", 3),
                0x77: (f"LD ({ir}{ds}),A", 3),
                0x7C: (f"LD A,{ir}H", 2), 0x7D: (f"LD A,{ir}L", 2),
                0x7E: (f"LD A,({ir}{ds})", 3),
                0x84: (f"ADD A,{ir}H", 2), 0x85: (f"ADD A,{ir}L", 2),
                0x86: (f"ADD A,({ir}{ds})", 3),
                0x8C: (f"ADC A,{ir}H", 2), 0x8D: (f"ADC A,{ir}L", 2),
                0x8E: (f"ADC A,({ir}{ds})", 3),
                0x94: (f"SUB {ir}H", 2), 0x95: (f"SUB {ir}L", 2),
                0x96: (f"SUB ({ir}{ds})", 3),
                0x9C: (f"SBC A,{ir}H", 2), 0x9D: (f"SBC A,{ir}L", 2),
                0x9E: (f"SBC A,({ir}{ds})", 3),
                0xA4: (f"AND {ir}H", 2), 0xA5: (f"AND {ir}L", 2),
                0xA6: (f"AND ({ir}{ds})", 3),
                0xAC: (f"XOR {ir}H", 2), 0xAD: (f"XOR {ir}L", 2),
                0xAE: (f"XOR ({ir}{ds})", 3),
                0xB4: (f"OR {ir}H", 2), 0xB5: (f"OR {ir}L", 2),
                0xB6: (f"OR ({ir}{ds})", 3),
                0xBC: (f"CP {ir}H", 2), 0xBD: (f"CP {ir}L", 2),
                0xBE: (f"CP ({ir}{ds})", 3),
                0xE1: (f"POP {ir}", 2), 0xE3: (f"EX (SP),{ir}", 2),
                0xE5: (f"PUSH {ir}", 2), 0xE9: (f"JP ({ir})", 2),
                0xF9: (f"LD SP,{ir}", 2),
            }
            if op2 in dd_map:
                mn, ln = dd_map[op2]
                end = (op2 == 0xE9)  # JP (IX/IY) is end of block
                return mn, ln, [], end
            # Fall through - treat as NOP + next instruction
            return f"DB {opcode:02X}H", 1, [], False
        
        # === Main unprefixed opcodes ===
        r_names = ['B','C','D','E','H','L','(HL)','A']
        rp_names = ['BC','DE','HL','SP']
        rp2_names = ['BC','DE','HL','AF']
        cc_names = ['NZ','Z','NC','C','PO','PE','P','M']
        
        # 8-bit load group: LD r,r'
        if (opcode & 0xC0) == 0x40 and opcode != 0x76:  # HALT is 76
            dst = r_names[(opcode >> 3) & 7]
            src = r_names[opcode & 7]
            return f"LD {dst},{src}", 1, [], False
        
        # ALU group: op A,r
        if (opcode & 0xC0) == 0x80:
            alu_ops = ['ADD A,','ADC A,','SUB ','SBC A,','AND ','XOR ','OR ','CP ']
            op_name = alu_ops[(opcode >> 3) & 7]
            src = r_names[opcode & 7]
            return f"{op_name}{src}", 1, [], False
        
        single = {
            0x00: ("NOP", 1, False),
            0x02: ("LD (BC),A", 1, False),
            0x07: ("RLCA", 1, False),
            0x08: ("EX AF,AF'", 1, False),
            0x0A: ("LD A,(BC)", 1, False),
            0x0F: ("RRCA", 1, False),
            0x12: ("LD (DE),A", 1, False),
            0x17: ("RLA", 1, False),
            0x1A: ("LD A,(DE)", 1, False),
            0x1F: ("RRA", 1, False),
            0x22: (f"LD ({w(1):04X}H),HL", 3, False),
            0x27: ("DAA", 1, False),
            0x2A: (f"LD HL,({w(1):04X}H)", 3, False),
            0x2F: ("CPL", 1, False),
            0x32: (f"LD ({w(1):04X}H),A", 3, False),
            0x37: ("SCF", 1, False),
            0x3A: (f"LD A,({w(1):04X}H)", 3, False),
            0x3F: ("CCF", 1, False),
            0x76: ("HALT", 1, True),
            0xC9: ("RET", 1, True),
            0xD9: ("EXX", 1, False),
            0xE3: ("EX (SP),HL", 1, False),
            0xE9: ("JP (HL)", 1, True),
            0xEB: ("EX DE,HL", 1, False),
            0xF3: ("DI", 1, False),
            0xFB: ("EI", 1, False),
        }
        
        if opcode in single:
            mn, ln, end = single[opcode]
            return mn, ln, [], end
        
        # LD r,n
        if (opcode & 0xC7) == 0x06:
            dst = r_names[(opcode >> 3) & 7]
            return f"LD {dst},{b(1):02X}H", 2, [], False
        
        # INC r / DEC r
        if (opcode & 0xC7) == 0x04:
            r = r_names[(opcode >> 3) & 7]
            return f"INC {r}", 1, [], False
        if (opcode & 0xC7) == 0x05:
            r = r_names[(opcode >> 3) & 7]
            return f"DEC {r}", 1, [], False
        
        # 16-bit LD
        if (opcode & 0xCF) == 0x01:
            rp = rp_names[(opcode >> 4) & 3]
            return f"LD {rp},{w(1):04X}H", 3, [], False
        
        # ADD HL,rp
        if (opcode & 0xCF) == 0x09:
            rp = rp_names[(opcode >> 4) & 3]
            return f"ADD HL,{rp}", 1, [], False
        
        # INC rp / DEC rp
        if (opcode & 0xCF) == 0x03:
            rp = rp_names[(opcode >> 4) & 3]
            return f"INC {rp}", 1, [], False
        if (opcode & 0xCF) == 0x0B:
            rp = rp_names[(opcode >> 4) & 3]
            return f"DEC {rp}", 1, [], False
        
        # PUSH/POP
        if (opcode & 0xCF) == 0xC5:
            rp = rp2_names[(opcode >> 4) & 3]
            return f"PUSH {rp}", 1, [], False
        if (opcode & 0xCF) == 0xC1:
            rp = rp2_names[(opcode >> 4) & 3]
            return f"POP {rp}", 1, [], False
        
        # JP nn
        if opcode == 0xC3:
            target = w(1)
            return f"JP {target:04X}H", 3, [target], True
        
        # JP cc,nn
        if (opcode & 0xC7) == 0xC2:
            cc = cc_names[(opcode >> 3) & 7]
            target = w(1)
            return f"JP {cc},{target:04X}H", 3, [target], False
        
        # JR e
        if opcode == 0x18:
            target = rel(1)
            return f"JR {target:04X}H", 2, [target], True
        
        # JR cc,e  (NZ,Z,NC,C)
        if opcode in (0x20, 0x28, 0x30, 0x38):
            cc = ['NZ','Z','NC','C'][(opcode - 0x20) >> 3]
            target = rel(1)
            return f"JR {cc},{target:04X}H", 2, [target], False
        
        # DJNZ e
        if opcode == 0x10:
            target = rel(1)
            return f"DJNZ {target:04X}H", 2, [target], False
        
        # CALL nn
        if opcode == 0xCD:
            target = w(1)
            return f"CALL {target:04X}H", 3, [target], False
        
        # CALL cc,nn
        if (opcode & 0xC7) == 0xC4:
            cc = cc_names[(opcode >> 3) & 7]
            target = w(1)
            return f"CALL {cc},{target:04X}H", 3, [target], False
        
        # RET cc
        if (opcode & 0xC7) == 0xC0:
            cc = cc_names[(opcode >> 3) & 7]
            return f"RET {cc}", 1, [], False
        
        # RST
        if (opcode & 0xC7) == 0xC7:
            target = opcode & 0x38
            return f"RST {target:02X}H", 1, [target], False
        
        # ALU A,n
        if (opcode & 0xC7) == 0xC6:
            alu_ops = ['ADD A,','ADC A,','SUB ','SBC A,','AND ','XOR ','OR ','CP ']
            op_name = alu_ops[(opcode >> 3) & 7]
            return f"{op_name}{b(1):02X}H", 2, [], False
        
        # IN A,(n) / OUT (n),A
        if opcode == 0xDB:
            return f"IN A,({b(1):02X}H)", 2, [], False
        if opcode == 0xD3:
            return f"OUT ({b(1):02X}H),A", 2, [], False
        
        # Fallback
        return f"DB {opcode:02X}H", 1, [], False
    
    def recursive_decode(self, start_addr, visited=None):
        """Recursively decode instructions following control flow"""
        if visited is None:
            visited = set()
        
        addr = start_addr
        while True:
            if addr in visited:
                break
            offset = addr - self.org
            if offset < 0 or offset >= len(self.data):
                break
            
            visited.add(addr)
            self.code_addrs.add(addr)
            
            mnemonic, length, targets, is_end = self.decode_one(addr)
            if length == 0:
                break
            
            raw = self.data[offset:offset+length]
            self.instructions[addr] = (mnemonic, length, raw)
            
            # Mark all bytes of this instruction
            for i in range(1, length):
                self.code_addrs.add(addr + i)
            
            for t in targets:
                if self.org <= t < self.org + len(self.data):
                    self.jump_targets.add(t)
            
            # Check for CALL targets specifically
            if self.data[offset] == 0xCD or (self.data[offset] & 0xC7) == 0xC4:
                for t in targets:
                    if self.org <= t < self.org + len(self.data):
                        self.call_targets.add(t)
            
            if is_end:
                # Follow targets for jumps (but not for RET)
                for t in targets:
                    if self.org <= t < self.org + len(self.data):
                        self.recursive_decode(t, visited)
                break
            else:
                # Follow fall-through and branch targets
                for t in targets:
                    if self.org <= t < self.org + len(self.data):
                        self.recursive_decode(t, visited)
                addr += length
    
    def linear_decode(self, start, end):
        """Linearly decode a range, for code areas missed by recursive descent"""
        addr = start
        while addr < end:
            if addr in self.instructions:
                _, length, _ = self.instructions[addr]
                addr += length
                continue
            
            offset = addr - self.org
            if offset >= len(self.data):
                break
            
            mnemonic, length, targets, is_end = self.decode_one(addr)
            if length == 0:
                addr += 1
                continue
            
            raw = self.data[offset:offset+length]
            self.instructions[addr] = (mnemonic, length, raw)
            self.code_addrs.add(addr)
            for i in range(1, length):
                self.code_addrs.add(addr + i)
            
            for t in targets:
                if self.org <= t < self.org + len(self.data):
                    self.jump_targets.add(t)
                    if self.data[offset] == 0xCD or (self.data[offset] & 0xC7) == 0xC4:
                        self.call_targets.add(t)
            
            addr += length
    
    def is_printable(self, b):
        return 0x20 <= b <= 0x7E or b in (0x0D, 0x0A)
    
    def find_string_at(self, offset):
        """Find string length starting at offset, returns 0 if not a string"""
        i = offset
        printable_count = 0
        while i < len(self.data):
            b = self.data[i]
            if self.is_printable(b):
                printable_count += 1
                i += 1
            elif b == 0x24 and printable_count >= 3:  # '$' terminator (BDOS string)
                return i - offset + 1
            elif b == 0x00 and printable_count >= 3:
                return i - offset
            else:
                break
        if printable_count >= 5:
            return printable_count
        return 0


def format_label(addr, call_targets, jump_targets):
    """Generate a label name for an address"""
    if addr in call_targets:
        return f"sub_{addr:04X}"
    else:
        return f"L{addr:04X}"


# ============================================================================
# Generic listing renderer + CLI
#
# Originally hard-wired to FORMAT.COM (ORG 0100H, CP/M). Rewritten 2026-06-05
# into a generic, scriptable disassembler so any K1520 image (boot ROMs at
# ORG 0000H, .COM files at 0100H, EPROM dumps, ...) can be disassembled, with
# user-supplied entry points, symbol labels and comments.
# ============================================================================

def _parse_int(s):
    """Parse an int in hex (0x.., ..H) or decimal."""
    s = s.strip()
    if s.lower().endswith('h'):
        return int(s[:-1], 16)
    return int(s, 0)


def render(dis, data, org, user_labels, user_comments, string_regions, title):
    """Produce an assembler listing: 'ADDR  hexbytes  instruction  ; comment'."""
    out = []
    end_addr = org + len(data)

    out.append("; " + "=" * 74)
    out.append(f"; {title or 'Z80 disassembly'}")
    out.append(f"; Size: {len(data)} bytes (0x{len(data):04X})")
    out.append(f"; Address range: {org:04X}H - {end_addr-1:04X}H")
    out.append("; Disassembled by tools/z80_disasm2.py (recursive descent + linear)")
    out.append("; " + "=" * 74)
    out.append("")
    if user_labels:
        out.append("; --- Symbolic labels (user-supplied) ---")
        for a in sorted(user_labels):
            out.append(f"{user_labels[a]:<16}EQU\t{a:04X}H")
        out.append("")
    out.append(f"\tORG\t{org:04X}H")
    out.append("")

    # Label map: auto labels for discovered targets, overridden by user labels.
    label_map = {}
    for a in sorted(dis.jump_targets | dis.call_targets):
        label_map[a] = format_label(a, dis.call_targets, dis.jump_targets)
    label_map.update(user_labels)

    addr_re = re.compile(r'([0-9A-F]{4})H')

    def sub_labels(text):
        def repl(m):
            v = int(m.group(1), 16)
            return label_map.get(v, m.group(0))
        return addr_re.sub(repl, text)

    string_starts = {sa: sl for (sa, sl) in string_regions}

    addr = org
    while addr < end_addr:
        offset = addr - org

        if addr in label_map:
            out.append(f"{label_map[addr]}:")

        # String region (only if not decoded as code)
        if addr in string_starts and addr not in dis.instructions:
            slen = string_starts[addr]
            raw = data[offset:offset + slen]
            parts, text_buf = [], ""
            for b in raw:
                if 0x20 <= b <= 0x7E:
                    text_buf += chr(b)
                else:
                    if text_buf:
                        parts.append(f"'{text_buf}'"); text_buf = ""
                    parts.append(f"{b:02X}H")
            if text_buf:
                parts.append(f"'{text_buf}'")
            out.append(f"{addr:04X}              \tDB\t{','.join(parts)}")
            addr += slen
            continue

        if addr in dis.instructions:
            mnemonic, length, raw_bytes = dis.instructions[addr]
            hex_str = ' '.join(f'{b:02X}' for b in raw_bytes)
            display = sub_labels(mnemonic)
            line = f"{addr:04X}  {hex_str:<11}\t{display}"
            if addr in user_comments:
                line = f"{line:<48}; {user_comments[addr]}"
            out.append(line)
            addr += length
        else:
            b_val = data[offset]
            line = f"{addr:04X}  {b_val:02X}         \tDB\t{b_val:02X}H"
            if addr in user_comments:
                line = f"{line:<48}; {user_comments[addr]}"
            out.append(line)
            addr += 1

    out.append("")
    out.append("\tEND")
    return '\n'.join(out)



# ============================================================================
# Format "mac": assemblierbarer M80-Quelltext (Entwurf 40, W1-W3)
#
# Eigener tabellengetriebener Dekoder: er liefert Befehlsvorlagen mit {a}/{j}
# fuer Adresswoerter bzw. Sprungziele, die Marken setzt erst die Ausgabe ein.
# Was M80 nicht bytegleich zurueckuebersetzen wuerde (undokumentiert, Dubletten),
# kommt als DB heraus - das ist der Grund, warum der Rundlauf haelt.
# ============================================================================

R8 = ['B', 'C', 'D', 'E', 'H', 'L', '(HL)', 'A']
RP = ['BC', 'DE', 'HL', 'SP']
RP2 = ['BC', 'DE', 'HL', 'AF']
CC = ['NZ', 'Z', 'NC', 'C', 'PO', 'PE', 'P', 'M']
ALU = ['ADD A,', 'ADC A,', 'SUB ', 'SBC A,', 'AND ', 'XOR ', 'OR ', 'CP ']
ROT = ['RLC', 'RRC', 'RL', 'RR', 'SLA', 'SRA', 'SLL', 'SRL']
BITOP = ['BIT', 'RES', 'SET']


def hx(v, n=2):
    """Hexzahl im M80-Stil: fuehrende 0, wenn die erste Ziffer ein Buchstabe ist."""
    s = f"{v:0{n}X}"
    return ('0' + s if s[0] in 'ABCDEF' else s) + 'H'


class Undokumentiert(Exception):
    """Kein (eindeutig assemblierbarer) Befehl: @laenge Bytes werden als DB ausgegeben."""

    def __init__(self, laenge):
        super().__init__(laenge)
        self.laenge = laenge


class Speicher:
    """Die Ladesegmente im Adressraum; ausserhalb: unbekannt."""

    def __init__(self):
        self.b = {}

    def laden(self, adr, daten):
        for i, x in enumerate(daten):
            self.b[adr + i] = x

    def __contains__(self, a):
        return a in self.b

    def __getitem__(self, a):
        return self.b[a]


def entschluesseln(m, a):
    """Ein Befehl bei @a.  Rueckgabe (vorlage, laenge, ziele, ende, abs_ziel).

    @vorlage ist Text mit {a} fuer ein Adresswort (nn) bzw. {j} fuer ein
    Sprungziel; @ziele sind Code-Folgeadressen, @ende ist wahr hinter einem
    unbedingten Sprung/Ruecksprung, @abs_ziel ('j'|'c', adr) bzw. adr bzw. None.
    Raises Undokumentiert (DB-Ausgabe) und IndexError (Segmentende).
    """
    def b(n):
        if a + n not in m:
            raise IndexError
        return m[a + n]

    def w(n):
        return b(n) | (b(n + 1) << 8)

    def d(n):
        v = b(n)
        return f"+{v}" if v < 128 else f"-{256 - v}"

    op = b(0)
    if op == 0xCB:
        x = b(1)
        r = R8[x & 7]
        if x < 0x40:
            if (x >> 3) == 6:               # SLL: undokumentiert
                raise Undokumentiert(2)
            return f"{ROT[x >> 3]} {r}", 2, [], False, None
        return f"{BITOP[(x >> 6) - 1]} {(x >> 3) & 7},{r}", 2, [], False, None
    if op in (0xDD, 0xFD):
        ix = 'IX' if op == 0xDD else 'IY'
        x = b(1)
        if x == 0xCB:
            y = b(3)
            b(2)
            if (y & 7) != 6 or (0x30 <= y < 0x38):
                raise Undokumentiert(4)
            t = f"({ix}{d(2)})"
            if y < 0x40:
                return f"{ROT[y >> 3]} {t}", 4, [], False, None
            return f"{BITOP[(y >> 6) - 1]} {(y >> 3) & 7},{t}", 4, [], False, None

        def rr(i):
            return ix if i == 2 else RP[i]
        if x == 0x21:
            return f"LD {ix},{{a}}", 4, [], False, w(2)
        if x == 0x22:
            return f"LD ({{a}}),{ix}", 4, [], False, w(2)
        if x == 0x2A:
            return f"LD {ix},({{a}})", 4, [], False, w(2)
        if x == 0x23:
            return f"INC {ix}", 2, [], False, None
        if x == 0x2B:
            return f"DEC {ix}", 2, [], False, None
        if x in (0x09, 0x19, 0x29, 0x39):
            return f"ADD {ix},{rr((x >> 4) & 3)}", 2, [], False, None
        if x == 0xE1:
            return f"POP {ix}", 2, [], False, None
        if x == 0xE5:
            return f"PUSH {ix}", 2, [], False, None
        if x == 0xE3:
            return f"EX (SP),{ix}", 2, [], False, None
        if x == 0xE9:
            return f"JP ({ix})", 2, [], True, None
        if x == 0xF9:
            return f"LD SP,{ix}", 2, [], False, None
        if x == 0x36:
            return f"LD ({ix}{d(2)}),{hx(b(3))}", 4, [], False, None
        if x in (0x34, 0x35):
            return f"{'INC' if x == 0x34 else 'DEC'} ({ix}{d(2)})", 3, [], False, None
        if (x & 0xC7) == 0x46 and x != 0x76:
            return f"LD {R8[(x >> 3) & 7]},({ix}{d(2)})", 3, [], False, None
        if (x & 0xF8) == 0x70 and x != 0x76:
            return f"LD ({ix}{d(2)}),{R8[x & 7]}", 3, [], False, None
        if (x & 0xC7) == 0x86:
            return f"{ALU[(x >> 3) & 7]}({ix}{d(2)})", 3, [], False, None
        raise Undokumentiert(1)             # DD/FD-Schattenpraefix bzw. Halbregister
    if op == 0xED:
        x = b(1)
        if (x & 0xC7) == 0x40 and x != 0x70:
            return f"IN {R8[(x >> 3) & 7]},(C)", 2, [], False, None
        if (x & 0xC7) == 0x41 and x != 0x71:
            return f"OUT (C),{R8[(x >> 3) & 7]}", 2, [], False, None
        if (x & 0xCF) == 0x42:
            return f"SBC HL,{RP[(x >> 4) & 3]}", 2, [], False, None
        if (x & 0xCF) == 0x4A:
            return f"ADC HL,{RP[(x >> 4) & 3]}", 2, [], False, None
        if (x & 0xCF) in (0x43, 0x4B):
            if (x >> 4) & 3 == 2:           # Langform LD (nn),HL: M80 nimmt die Kurzform
                b(3)
                raise Undokumentiert(4)
            if (x & 0xCF) == 0x43:
                return f"LD ({{a}}),{RP[(x >> 4) & 3]}", 4, [], False, w(2)
            return f"LD {RP[(x >> 4) & 3]},({{a}})", 4, [], False, w(2)
        t = {0x44: 'NEG', 0x45: 'RETN', 0x4D: 'RETI', 0x46: 'IM 0', 0x56: 'IM 1',
             0x5E: 'IM 2', 0x47: 'LD I,A', 0x4F: 'LD R,A', 0x57: 'LD A,I',
             0x5F: 'LD A,R', 0x67: 'RRD', 0x6F: 'RLD', 0xA0: 'LDI', 0xA1: 'CPI',
             0xA2: 'INI', 0xA3: 'OUTI', 0xA8: 'LDD', 0xA9: 'CPD', 0xAA: 'IND',
             0xAB: 'OUTD', 0xB0: 'LDIR', 0xB1: 'CPIR', 0xB2: 'INIR', 0xB3: 'OTIR',
             0xB8: 'LDDR', 0xB9: 'CPDR', 0xBA: 'INDR', 0xBB: 'OTDR'}.get(x)
        if t is None:
            raise Undokumentiert(2)
        return t, 2, [], x in (0x45, 0x4D), None
    x = op >> 6
    y = (op >> 3) & 7
    z = op & 7
    if x == 1:
        if op == 0x76:
            return "HALT", 1, [], False, None
        return f"LD {R8[y]},{R8[z]}", 1, [], False, None
    if x == 2:
        return f"{ALU[y]}{R8[z]}", 1, [], False, None
    if x == 0:
        if z == 0:
            if y == 0:
                return "NOP", 1, [], False, None
            if y == 1:
                return "EX AF,AF'", 1, [], False, None
            e = b(1)
            t = (a + 2 + (e - 256 if e >= 128 else e)) & 0xFFFF
            if y == 2:
                return "DJNZ {j}", 2, [t], False, ('j', t)
            if y == 3:
                return "JR {j}", 2, [t], True, ('j', t)
            return f"JR {CC[y - 4]},{{j}}", 2, [t], False, ('j', t)
        if z == 1:
            if y & 1:
                return f"ADD HL,{RP[y >> 1]}", 1, [], False, None
            return f"LD {RP[y >> 1]},{{a}}", 3, [], False, w(1)
        if z == 2:
            if y < 4:
                return ['LD (BC),A', 'LD A,(BC)', 'LD (DE),A', 'LD A,(DE)'][y], 1, [], False, None
            return ['LD ({a}),HL', 'LD HL,({a})', 'LD ({a}),A', 'LD A,({a})'][y - 4], 3, [], False, w(1)
        if z == 3:
            return f"{'DEC' if y & 1 else 'INC'} {RP[y >> 1]}", 1, [], False, None
        if z == 4:
            return f"INC {R8[y]}", 1, [], False, None
        if z == 5:
            return f"DEC {R8[y]}", 1, [], False, None
        if z == 6:
            return f"LD {R8[y]},{hx(b(1))}", 2, [], False, None
        return ['RLCA', 'RRCA', 'RLA', 'RRA', 'DAA', 'CPL', 'SCF', 'CCF'][y], 1, [], False, None
    # x == 3
    if z == 0:
        return f"RET {CC[y]}", 1, [], False, None
    if z == 1:
        if y & 1:
            return ['RET', 'EXX', 'JP (HL)', 'LD SP,HL'][y >> 1], 1, [], y in (1, 5), None
        return f"POP {RP2[y >> 1]}", 1, [], False, None
    if z == 2:
        return f"JP {CC[y]},{{j}}", 3, [w(1)], False, ('j', w(1))
    if z == 3:
        if y == 0:
            return "JP {j}", 3, [w(1)], True, ('j', w(1))
        if y == 2:
            return f"OUT ({hx(b(1))}),A", 2, [], False, None
        if y == 3:
            return f"IN A,({hx(b(1))})", 2, [], False, None
        if y == 4:
            return "EX (SP),HL", 1, [], False, None
        if y == 5:
            return "EX DE,HL", 1, [], False, None
        if y == 6:
            return "DI", 1, [], False, None
        return "EI", 1, [], False, None     # y == 7 (y == 1 ist CB, oben erledigt)
    if z == 4:
        return f"CALL {CC[y]},{{j}}", 3, [w(1)], False, ('c', w(1))
    if z == 5:
        if y & 1:
            if y == 1:
                return "CALL {j}", 3, [w(1)], False, ('c', w(1))
            raise Undokumentiert(1)         # DD/ED/FD sind oben erledigt
        return f"PUSH {RP2[y >> 1]}", 1, [], False, None
    if z == 6:
        return f"{ALU[y]}{hx(b(1))}", 2, [], False, None
    return f"RST {hx(y * 8)}", 1, [y * 8], False, None


def _zahl(s):
    s = s.strip()
    if s.lower().endswith('h'):
        s = s[:-1]
    return int(s, 16)


def lies_karte(pfad):
    k = {'seg': [], 'code': [], 'db': [], 'dw': [], 'dwc': [], 'txt': [], 'str': [],
         'name': {}, 'kom': {}, 'ik': {}, 'eq': {}}
    with open(pfad, encoding='utf-8') as f:
        for nr, z in enumerate(f, 1):
            z = z.rstrip('\n')
            if not z.strip() or z.lstrip().startswith('#'):
                continue
            t = z.split(None, 2)
            try:
                if t[0] == 'seg':
                    _, a, o, l = z.split()
                    k['seg'].append((_zahl(a), _zahl(o), _zahl(l)))
                elif t[0] == 'code':
                    k['code'] += [_zahl(x) for x in z.split()[1:]]
                elif t[0] in ('db', 'dw', 'dwc', 'txt', 'str'):
                    _, v, bis = z.split()[:3]
                    k[t[0]].append((_zahl(v), _zahl(bis)))
                elif t[0] == 'name':
                    k['name'][_zahl(t[1])] = t[2].split()[0]
                elif t[0] in ('kom', 'ik'):
                    k[t[0]].setdefault(_zahl(t[1]), []).append(t[2] if len(t) > 2 else '')
                elif t[0] == 'eq':
                    k['eq'][t[1]] = _zahl(t[2].split()[0])
                else:
                    raise ValueError(f"unbekannte Anweisung '{t[0]}'")
            except (ValueError, IndexError) as e:
                raise SystemExit(f"{pfad}:{nr}: {e}: {z}")
    k['dw'] += k['dwc']
    return k


def karte_leer():
    return {'seg': [], 'code': [], 'db': [], 'dw': [], 'dwc': [], 'txt': [], 'str': [],
            'name': {}, 'kom': {}, 'ik': {}, 'eq': {}}


class MacAbzug:
    """Codeverfolgung + Ausgabe eines Abzugs als M80-Quelle."""

    def __init__(self, daten, karte, zeiger=True, folge=True, linear=()):
        self.k = k = karte
        self.m = m = Speicher()
        self.segs = []                      # (adresse, ende) in Dateireihenfolge
        for a, o, l in k['seg']:
            m.laden(a, daten[o:o + l])
            self.segs.append((a, a + min(l, max(0, len(daten) - o))))
        self.kind = {}                      # adresse -> ('i', laenge, vorlage, abs_ziel)
        self.ziele_marke = set()
        self.seen = set()
        self.gesperrt = set()
        for art in ('db', 'dw', 'txt', 'str'):
            for v, bis in k[art]:
                self.gesperrt.update(range(v, bis + 1))
        work = list(k['code'])
        for v, bis in k['dwc']:
            for a in range(v, bis, 2):
                if a in m and a + 1 in m:
                    t = m[a] | (m[a + 1] << 8)
                    if t in m and t not in self.gesperrt:
                        work.append(t)
        self._verfolge(work, zeiger, folge)
        for von, bis in linear:
            self._linear(von, bis)

    def _verfolge(self, work, zeiger, folge):
        m, kind, seen, gesperrt = self.m, self.kind, self.seen, self.gesperrt
        while work:
            a = work.pop()
            while a in m and a not in seen and a not in gesperrt:
                try:
                    vorlage, n, ziele, ende, absz = entschluesseln(m, a)
                except (Undokumentiert, IndexError):
                    break
                if any((a + i) in gesperrt or (a + i) not in m for i in range(n)):
                    break
                # FF-Lauf (Fuellbytes): RST 38H gefolgt von FFH ist kein Code
                if n == 1 and all(m.b.get(a + i) == 0xFF for i in range(4)):
                    break
                kind[a] = ('i', n, vorlage, absz)
                seen.update(range(a, a + n))
                for t in ziele:
                    if t in m:
                        self.ziele_marke.add(t)
                        work.append(t)
                # Zeiger auf Code: LD rr,nn / LD (nn),.. mit Ziel im Segment
                if zeiger and isinstance(absz, int) and vorlage.startswith('LD ') and \
                        vorlage.endswith(',{a}') and absz in m and absz not in gesperrt:
                    work.append(absz)
                if ende:
                    # Hinter einem unbedingten Ende beginnt meist der naechste Einstieg
                    nxt = a + n
                    if folge and nxt in m and nxt not in gesperrt and m[nxt] != 0xFF:
                        work.append(nxt)
                    break
                a += n

    def _linear(self, von, bis):
        """Alles noch nicht Erfasste in [von, bis) linear dekodieren (Undokumentiertes als DB)."""
        m, kind, seen, gesperrt = self.m, self.kind, self.seen, self.gesperrt
        a = von
        while a < bis:
            if a in kind:
                a += kind[a][1]
                continue
            if a not in m or a in seen or a in gesperrt:
                a += 1
                continue
            try:
                vorlage, n, ziele, ende, absz = entschluesseln(m, a)
            except Undokumentiert as u:
                n = u.laenge
                vorlage, ziele, absz = None, [], None
            except IndexError:
                a += 1
                continue
            if any((a + i) in gesperrt or (a + i) not in m or (a + i) in seen for i in range(n)):
                a += 1
                continue
            if vorlage is None:
                vorlage = "DB\t" + ",".join(hx(m[a + i]) for i in range(n))
            kind[a] = ('i', n, vorlage, absz)
            seen.update(range(a, a + n))
            for t in ziele:
                if t in m:
                    self.ziele_marke.add(t)
            a += n

    # ---- Ausgabe ----------------------------------------------------------
    def _art(self, x):
        for nm in ('dw', 'txt', 'str', 'db'):
            for v, bis in self.k[nm]:
                if v <= x <= bis:
                    return nm
        return None

    def ausgabe(self, adr=False, auffuellen=256, praefix='L'):
        m, k, kind = self.m, self.k, self.kind
        marke = dict(k['name'])
        refs = {t for t in self.ziele_marke if t in m}
        for e in kind.values():
            absz = e[3]
            if isinstance(absz, tuple):
                absz = absz[1]
            if isinstance(absz, int) and absz in m:
                refs.add(absz)
        for v, bis in k['dw']:
            for a in range(v, bis + 1, 2):
                if a + 1 in m:
                    t = m[a] | (m[a + 1] << 8)
                    if t in m:
                        refs.add(t)
        for a in refs:
            marke.setdefault(a, f"{praefix}{a:04X}")

        strstart = set()
        for v, bis in k['str']:
            a = v
            while a <= bis and a in m:
                strstart.add(a)
                a += 1 + m[a]

        # Erster Durchgang: die Elemente je Segment festlegen.  Datenlaeufe
        # enden an Marken, Befehlen, Kommentaren und Wechseln der Kartenart.
        stoppt = set(marke) | set(kind) | set(k['kom']) | set(k['ik'])
        elemente = []                       # (adresse, art, daten)
        for sa, se in self.segs:
            a = sa
            while a < se:
                e = kind.get(a)
                if e:
                    elemente.append((a, 'i', e))
                    a += e[1]
                    continue
                art = 'db'
                for nm in ('dw', 'txt', 'str'):
                    for v, bis in k[nm]:
                        if v <= a <= bis:
                            art = 'txt' if nm == 'str' and a not in strstart else nm
                if art == 'dw' and a + 1 < se:
                    elemente.append((a, 'dw', None))
                    a += 2
                    continue
                if art == 'str':
                    ende = a + 1
                elif art == 'dw':
                    ende = a + 2
                else:
                    ende = a + 1
                    while ende < se and ende not in stoppt and \
                            self._art(ende - 1) == self._art(ende):
                        ende += 1
                        if ende - a >= 16 and art == 'db':
                            break
                bs = [m[x] for x in range(a, min(ende, se))]
                if art == 'db' and bs[0] == 0xFF:
                    n = 0
                    while a + n < se and (a + n) not in stoppt and m[a + n] == 0xFF \
                            and self._art(a) == self._art(a + n):
                        n += 1
                    if n >= 8:
                        elemente.append((a, 'fill', n))
                        a += n
                        continue
                if art == 'str':
                    elemente.append((a, 'strlen', bs))
                elif art == 'txt':
                    elemente.append((a, 'txt', bs))
                else:
                    elemente.append((a, 'db', bs))
                a += len(bs)

        # Marken nur dort, wo ein Element beginnt (sonst haengt die Marke in der
        # Luft - Bezuege dorthin werden als Zahl ausgegeben)
        starts = {e[0] for e in elemente}
        gueltig = {a: n for a, n in marke.items() if a in starts}

        def sym(a):
            if a in gueltig:
                return gueltig[a]
            for n, v in k['eq'].items():
                if v == a:
                    return n
            return None

        def ausdruck(a):
            s = sym(a)
            return s if s else hx(a, 4)

        pad = hx(auffuellen, 3) if auffuellen else None
        out = ["\t.Z80", "\tCSEG", "",
               "; N Fuellbytes FFH (UDOS-Segmente haben unbenutzte Luecken)",
               "FILLFF\tMACRO\tN", "\tREPT\tN", "\tDB\t0FFH", "\tENDM", "\tENDM", ""]
        for n, v in sorted(k['eq'].items(), key=lambda x: x[1]):
            out.append(f"{n}\tEQU\t{hx(v, 4)}")
        eidx = 0
        for sa, se in self.segs:
            out.append(f"\n; ---- Segment {sa:04X}H .. {se - 1:04X}H ----")
            out.append(f"\t.PHASE\t{hx(sa, 4)}")
            out.append(f"SEG{sa:04X}:")
            while eidx < len(elemente) and elemente[eidx][0] < se:
                a, art, d = elemente[eidx]
                eidx += 1
                for z in k['kom'].get(a, ()):
                    out.append(f"; {z}")
                if a in gueltig:
                    out.append(f"{gueltig[a]}:")
                ik = "; ".join(k['ik'].get(a, ()))
                zeig = adr and art == 'i'       # Adresskommentare nur hinter Befehlen
                if zeig or ik:
                    tail = "\t; " + " ".join(x for x in ((f"{a:04X}" if zeig else ""), ik) if x)
                else:
                    tail = ""
                if art == 'i':
                    _, n, vorlage, absz = d
                    if absz is None:
                        text = vorlage
                    elif isinstance(absz, tuple):
                        text = vorlage.format(j=ausdruck(absz[1]))
                    else:
                        text = vorlage.format(a=ausdruck(absz))
                    out.append(f"\t{text}{tail}")
                elif art == 'dw':
                    t = m[a] | (m[a + 1] << 8)
                    out.append(f"\tDW\t{ausdruck(t) if t in m or sym(t) else hx(t, 4)}{tail}")
                elif art == 'fill':
                    out.append(f"\tFILLFF\t{d}{tail}")
                elif art == 'strlen':
                    out.append(f"\tDB\t{hx(d[0])}{tail}")
                elif art == 'txt':
                    out.append(self._text(d) + tail)
                else:
                    out.append("\tDB\t" + ",".join(hx(v) for v in d) + tail)
            out.append(f"SEG{sa:04X}E:")
            if pad:
                out.append(f"\tREPT\t({pad}-(($-SEG{sa:04X}) AND {hx(auffuellen - 1)})) AND {hx(auffuellen - 1)}")
                out.append("\tDB\t0FFH")
                out.append("\tENDM")
            out.append("\t.DEPHASE")
        out.append("\tEND")
        return "\n".join(out) + "\n"

    @staticmethod
    def _text(bs):
        """Zeichenkette als DB mit Anfuehrungszeichen; Steuerzeichen als Zahlen."""
        teile, cur = [], ''
        for v in bs:
            if 0x20 <= v < 0x7F and v not in (0x27, 0x22):
                cur += chr(v)
            else:
                if cur:
                    teile.append(f"'{cur}'")
                    cur = ''
                teile.append(hx(v))
        if cur:
            teile.append(f"'{cur}'")
        return "\tDB\t" + ",".join(teile)


def mac_main(args, data, org):
    k = lies_karte(args.karte) if args.karte else karte_leer()
    for spec in args.seg:
        try:
            a, o, l = spec.split(":")
            k['seg'].append((_parse_int(a), _parse_int(o), _parse_int(l)))
        except ValueError:
            raise SystemExit(f"--seg erwartet ADR:DATEIOFFSET:LAENGE, nicht '{spec}'")
    if not k['seg']:
        k['seg'].append((org, 0, len(data)))
    k['code'] += [_parse_int(x) for x in args.entry]
    for spec in args.label:
        name, a = spec.split("=", 1)
        k['name'][_parse_int(a)] = name.strip()
    for spec in args.comment:
        a, txt = spec.split("=", 1)
        k['ik'].setdefault(_parse_int(a), []).append(txt)
    if not k['code'] and not k['dwc']:
        k['code'].append(k['seg'][0][0])
    lin = []
    for r in args.ranges:
        a, b = r.split(":")
        lin.append((_parse_int(a), _parse_int(b)))
    if args.linear:
        for sa, so, sl in k['seg']:
            lin.append((sa, sa + sl))
    zeiger = True if args.zeiger is None else args.zeiger
    folge = True if args.folge is None else args.folge
    abz = MacAbzug(data, k, zeiger=zeiger, folge=folge, linear=lin)
    sys.stdout.write(abz.ausgabe(adr=args.adr, auffuellen=args.auffuellen))


def main():
    import argparse
    ap = argparse.ArgumentParser(
        description="Generic Z80 disassembler for the a5120emu toolbox. "
                    "Recursive-descent from entry points plus optional linear ranges. "
                    "--format mac erzeugt assemblierbaren M80-Quelltext (siehe Modulkopf).")
    ap.add_argument("file", help="binary image to disassemble (ROM/.COM/EPROM dump)")
    ap.add_argument("--format", choices=("listing", "mac"), default="listing",
                    help="listing (Vorgabe) oder mac (assemblierbarer M80-Text)")
    ap.add_argument("--org", default="0", help="origin/load address (hex 0x.. / ..H or dec). Default 0")
    ap.add_argument("--entry", action="append", default=[], metavar="ADDR",
                    help="extra recursive-descent entry point, e.g. an IM2 ISR or a "
                         "runtime JP target (repeatable). ORG is always an entry.")
    ap.add_argument("--range", action="append", default=[], dest="ranges", metavar="START:END",
                    help="linear-decode this address range (repeatable)")
    ap.add_argument("--linear", action="store_true",
                    help="linear-decode the whole image after recursive descent")
    ap.add_argument("--label", action="append", default=[], metavar="NAME=ADDR",
                    help="symbolic label for an address (repeatable)")
    ap.add_argument("--comment", action="append", default=[], metavar="ADDR=TEXT",
                    help="inline comment at an address (repeatable)")
    ap.add_argument("--no-strings", action="store_true", help="disable ASCII string detection (listing)")
    ap.add_argument("--title", default=None, help="title line for the listing header")
    mac = ap.add_argument_group("nur --format mac")
    mac.add_argument("--karte", metavar="DATEI", help="Kartendatei (seg/code/db/dw/dwc/str/txt/name/kom/ik/eq)")
    mac.add_argument("--seg", action="append", default=[], metavar="ADR:OFFSET:LAENGE",
                     help="Ladesegment (Zahlen hex 0x../..H oder dez; wiederholbar, "
                          "ersetzt das Standardsegment ORG:0:Dateilaenge)")
    mac.add_argument("--adr", action="store_true", help="Adresskommentar '; 9000' hinter jedem Befehl")
    mac.add_argument("--auffuellen", type=lambda x: _parse_int(x), default=256, metavar="N",
                     help="Segmente mit FFH auf ein Vielfaches von N Bytes auffuellen "
                          "(Zweierpotenz, 0 = nicht; Vorgabe 256 wie UDOS-Dateien)")
    mac.add_argument("--zeiger", action=argparse.BooleanOptionalAction, default=None,
                     help="LD rr,nn-Zeiger als Code-Einstiege verfolgen (Vorgabe: an im Format mac)")
    mac.add_argument("--folge", action=argparse.BooleanOptionalAction, default=None,
                     help="Befehl hinter unbedingtem Sprung/RET als Einstieg versuchen (Vorgabe: an im Format mac)")
    args = ap.parse_args()

    org = _parse_int(args.org)
    data = read_binary(args.file)
    if args.format == "mac":
        mac_main(args, data, org)
        return
    if args.karte or args.seg or args.adr:
        ap.error("--karte/--seg/--adr gelten nur mit --format mac")
    dis = Z80Disassembler(data, org=org)

    # Recursive descent from ORG plus user entry points.
    for e in [org] + [_parse_int(x) for x in args.entry]:
        if org <= e < org + len(data):
            dis.recursive_decode(e)

    # Decode any call/jump targets discovered but not yet visited.
    for t in sorted(dis.jump_targets | dis.call_targets):
        if t not in dis.instructions and org <= t < org + len(data):
            dis.recursive_decode(t)

    # User-requested linear ranges + optional whole-image linear pass.
    for r in args.ranges:
        a, b = r.split(":")
        dis.linear_decode(_parse_int(a), _parse_int(b))
    if args.linear:
        dis.linear_decode(org, org + len(data))

    user_labels = {}
    for spec in args.label:
        name, a = spec.split("=", 1)
        user_labels[_parse_int(a)] = name.strip()

    user_comments = {}
    for spec in args.comment:
        a, txt = spec.split("=", 1)
        user_comments[_parse_int(a)] = txt

    string_regions = []
    if not args.no_strings:
        i = 0
        while i < len(data):
            a = i + org
            if a not in dis.code_addrs:
                slen = dis.find_string_at(i)
                if slen >= 5:
                    string_regions.append((a, slen)); i += slen; continue
            i += 1

    print(render(dis, data, org, user_labels, user_comments, string_regions,
                 args.title or args.file))


if __name__ == '__main__':
    main()

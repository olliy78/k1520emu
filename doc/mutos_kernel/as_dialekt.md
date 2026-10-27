# Der Dialekt des MUTOS-`as` (Stand 2026-10-27, Entwurf 41, AP K3)

Wofuer: `z8kdis --format as` (`tools/z8000/z8kdis.h`) soll Quelltext schreiben, den der `as` von der Diskette `usr` annimmt.
Dieses Blatt trennt, was **belegt** ist, von dem, was **vermutet** wird.  Ein Rundlauf gegen den Gast-`as`
(Quelle -> `as` -> `ld` -> Bytevergleich) ist **noch nicht** gefahren; er ist AP K4.  Bis dahin prueft `z8kdis --pruefen`
nur gegen die eigene Befehlstabelle (`z8kasm`-Syntax), nicht gegen den `as`.

## Was der `as` ist (belegt)

Aus den Zeichenketten von `bin/as` (35 814 B, `disks/a5120_mutos8000/a5120_mutos8000_best_usr.zip`, `dateien/bin/as`):
**Zilog `PLZ/ASM - ASSEMBLER V 1.0`**, ein yacc/lex-Assembler (`yacc stack overflow`, `plzlex`, `wrong end of module`,
`wrong END of procedure`).  Es ist also **nicht** die Zilog-Assembler-Syntax von `z8kasm`, sondern die PLZ/ASM-Modulsprache
mit `module` / `constant` / `global` / `internal` / `external` / `procedure` / `entry` / `end`.

Schluesselwoerter in der Tabelle des `as` (Reihenfolge wie in der Datei):
`MODULE CONSTANT TYPE GLOBAL INTERNAL EXTERNAL SIZEOF SEG OFFSET BYTE WORD LONG ARRAY RECORD PROCEDURE LABEL ENTRY END LOCAL DO OD EXIT FROM REPEAT IF THEN ELSE FI CASE LOR LXOR LAND MOD SHL SHR LNOT`,
Bedingungen `NZ NE EQ MI PL NC OV NOV LT GE LE GT ULT UGE ULE UGT PO PE`, Steuerregister `FCW FLAGS NSPOFF NSP NSPSEG PSAPOFF PSAPSEG REFRESH`,
Rundungsarten/Fliesskomma `RZ RN RP RM INV FOV UN DZ INX DE NAN IX`, `VI NVI`, Datenanweisungen `BVAL WVAL LVAL`,
Direktiven `$N $NONSEG $NONSEGMENTED $E $EVEN $ABS $R $REL $S $SECTION $F $SDEFAULT $D $DATA $C $CODE $LISTON`.

* **Alle Befehle der Z8000-Tabelle** (`core/primitives/z8000/z8k_table.h`, 196 Mnemoniken) stehen auch im `as` -- gemessen:
  einzig die EPA-Erweiterungen `EXT0E/0F/4E/4F/8E/8F` fehlen (kein Kernelbefehl).  Dazu kennt der `as` die Fliesskomma-Befehle (`FADD`, `FLD` ...).
* **`PSAP` fehlt, `PSAPOFF` steht drin**: der nichtsegmentierte Name des Steuerregisters 5 heisst im `as` `PSAPOFF` (Register 7: `NSP` und `NSPOFF`).
  `z8kdis` schreibt deshalb `ldctl psapoff,r0`.
* Fehlertexte, die das Verhalten belegen: `initialization only in TEXT or DATA!` (Daten mit `wval`/`bval` sind im Code- **und** im Datenabschnitt erlaubt, nicht im BSS),
  `more initiators then expected`, `hex_digit expected`, `illegal section name`, `too many generic labels`, `unimplmented ASSDIR`.

## Aus `cc -S` belegt (Probe K1/K3)

Belegmaterial: `tools/mutos8000/kernel/probe/p1.s` (3166 B, aus `p1.c`: Zuweisungen, Schleifen, `switch`, Strukturen, `long`, `char`, Zeiger auf Funktion)
und `t1.s` (Probe K1, `doc/mutos_kernel/k1_cc_im_gast.md`).  `p2.s` ist **abgeschnitten** (1090 B, endet mitten in `main` bei `sub SP,#%2C`,
`cc` meldet keinen Fehler; `printf`/`fopen`/`static`-Variable) -- gebrauchbar als Beleg fuer `$DATA`/`$SECTION`/`$CODE` und die Kopfzeilen, nicht fuer Vollstaendigkeit.

```
c_ module                      ← "<name> module", Name = Quelldatei + "_"
$NONSEGMENTED
constant                       ← Abkuerzungen, jede Datei gleich:
	FP	:= r14                    FP, SP; X G I A PR E B W BV WV LV L
	SP	:= r15
	X	:= external   G := global   I := internal   A := array   PR := procedure
	E	:= entry      B := byte     W := word       BV := bval   WV := wval   LV := lval   L := label
X	csv	PR                     ← Abschnittswechsel "X"/"G"/"I" + Vereinbarung
X	cret	PR;
G	_f1	PR                     ← globales Unterprogramm
~~f1:                          ← Beschriftung (Debugger?), danach "E" = entry
E
call	csv                    ← Befehle: kleingeschrieben, Mnemonik TAB Operanden
~a:=%4                         ← "~name:=wert": Namen fuer Parameter/Lokale (Pseudoangaben)
jp	L3
L4:                            ← Marken im Unterprogramm lokal: "L<n>:"
ld	r3,%4(FP)             ← Zilog "indiziert": adresse(register); "%" = hexadezimal
ld	-%E(FP),r2            ← negativ als Ausdruck: -%E  (= %FFF2)
inc	-%E(FP),#%1           ← "#" = unmittelbar; Zaehler hexadezimal
jp	ge	L7                    ← Bedingung nach "jp", dann Ziel; unbedingt: ohne Bedingung
jp	eq	L6000
exts	rr2    div  rr2,%6(FP)    ldl rr2,..   multl rq0,..    ← Register r0..r15, rr0.., rh/rl, rq0..
push	@SP,%4(FP)            ← "push @SP,<quelle>" (kein "@-SP" im p1.s)
call	@r2                   ← indirekt
end _f1
I                              ← internal-Abschnitt: Daten
L2:
BV %77;BV %65;BV %0           ← Bytes: je Wert ein "BV", mit ";" verbunden
WV %1                          ← Wort;  "WV _tab" Marke,  "WV %4+_tab" Ausdruck
LV %186A0                      ← Langwort
$EVEN                          ← Ausrichtung
G
_tab	L                        ← Datenmarke vereinbaren: "<name> label"
_tab:
end c_                         ← Modulende
```

Weiteres aus `p2.s`: im Unterprogramm darf zwischen Befehlen Daten angelegt werden:
`$DATA` / `$SECTION c__d` / `I` / `L4:` / `WV %7` / `$CODE` (Abschnittswechsel mitten im Code), danach `~cnt:=L4`.
Direktadressen sind einfach Marken: `ld r2,_sp`, `push @SP,L4`, `inc L4,#%1`.  `%1+_zb` ist `_zb+1`.
Aus `t1.s` (K1, Bildschirmabzug): `push @SP,#%4` und `push @-SP,#_tab`, `inc SP,#%4`, `jp cret`.

## Was `z8kdis --format as` daraus macht

```
<modul> module
$NONSEGMENTED               (bei --segmentiert: $SEGMENTED  -- unbelegt)
constant                    ← die Abkuerzungen von cc, danach Karten-`eq` und Marken mitten im Befehl als Konstanten
G	_<modul>	PR              ← EIN globales Unterprogramm um das ganze Abbild (alle Marken lokal, alle Spruenge erlaubt)
E
$ABS %0                     ← unbelegt, s.u.
start:
	ld	r0,#%0000          ← kleingeschrieben; Hexzahlen bleiben gross; Tabulator zwischen Mnemonik und Operanden
	ldctl	psapoff,r0         ← PSAP -> PSAPOFF
	jp	ne,start           ← Z -> eq, NZ -> ne, C -> ult, NC -> uge (der as nennt keine Z/C/T/F in der Bedingungsliste)
	WV %0001;WV %0203       ← Daten wie cc: je Wert eine Anweisung, mit ";"
end _<modul>
end <modul>
```

## Unsicherheiten (VERMUTET, nicht belegt)

1. **`$ABS %0`** (absolute Ladeadresse): die Direktive steht in der Tabelle des `as`; ihre Wirkung (und ob `ld` ein Rohabbild ohne Kopf liefert) ist nicht erprobt.  Fuer den Kernel ist K4 zu klaeren: `as` + `ld` mit `-T`/`-r`/Segmentbasis.
2. **Befehle mit Operandenform `<marke>(rN)`**: cc schreibt `%4(FP)`; ob `L0200(r2)` (Marke als Basis) akzeptiert wird, ist nicht belegt (Zilog-ueblich: ja).
3. **`push @SP,...` vs. `push @-SP,#_tab`**: in `t1.s` (K1) stand beides; hier immer `push @r15,...`.  Ob `@-SP` ein eigener Adressierungsfall ist oder nur Schreibweise, ist offen.
4. **Hexzahlen**: cc schreibt `%FFF2`, `%1E`, `%A`; `z8kdis` schreibt vierstellig mit Nullen (`%0004`, `#%0000`).  Die Zeichensatzzeile des `as` kennt `0-9 a-f A-F`; fuehrende Nullen sollten gehen.
5. **Bedingungsnamen `z`/`c`/`t`/`f`**: in der Tabelle des `as` stehen nur `NZ NE EQ MI PL NC OV NOV LT GE LE GT ULT UGE ULE UGT PO PE`; `z8kdis` bildet Z -> eq, NZ -> ne, C -> ult, NC -> uge ab.  `PE`/`PO` (= `OV`/`NOV`) wurden nicht gesehen.
6. **Kleine Zaehler**: cc schreibt `#%1`, `#%2` fuer `inc/dec/sla`; `z8kdis` schreibt Dezimalzahlen (`#2`).  Dezimal ohne Praefix ist in PLZ/ASM ueblich, bei diesem `as` nicht gemessen.
7. **`jr`/`calr`/`djnz`**: in `p1.s`/`p2.s` kommt nur `jp`/`call` vor (keine Peephole-Stufe `c2` im `-S`-Lauf?).  Kernel-Code enthaelt viele `jr`/`djnz`; ob der `as` sie mit Marken ausserhalb der Reichweite sauber meldet, ist unbekannt.
8. **Kommentare**: `z8kdis` schreibt `! ... !` (Zilog), cc schreibt keine; unbelegt.  Notfalls weglassen (`kom`/`ik` der Karte nutzen).
9. **`$SEGMENTED`** (segmentierter Modus) und segmentierte Adressschreibweise `<<s>>%o`: weder `$SEGMENTED` noch `<<` kommen in den Proben vor; `$S` steht in der Tabelle.
10. **Zweitkodierung (`LDB.L`) und unbekannte Worte**: der `as` hat dafuer keine Schreibweise; `z8kdis` schreibt `WV`-Woerter (mit Kommentar) -- zugleich der Grund, warum solche Befehle auch fuer den eigenen Rundlauf als Daten gefuehrt werden.
11. **Mehrfach-Bytes je Anweisung** (`BV %1,%2`) wurde nicht verwendet, weil cc nur `BV %x;BV %y` schreibt.
12. **Groesse eines einzigen Unterprogramms** (36 KB, ~3300 Marken): der `as` hat Grenzen (`symbol table overflow`, `too many local areas`, `too many generic labels`, `HASHSZ too small`); ob der Kernel in einem Stueck durchgeht, zeigt erst K4.  Rueckfall: Unterprogramm je Funktion (K5).

## So wird der Dialekt gemessen (K4)

`z8kdis --format as ... > kernel.s`, Quelldiskette mit dem DiskTool (`create`/`put`) bauen, im Gast `as kernel.s` und `ld`
(Rezept `tools/mutos8000/kernel/probe/cc_probe.dbg`; ~1 Min. Maschinenzeit je Kleinstdatei, ein 36-KB-Kernel deutlich laenger).
Jede Ablehnung des `as` wird hier unter "belegt" nachgetragen und `asQuelle()` angepasst; bytegleich zum Abbild heisst: Dialekt stimmt.

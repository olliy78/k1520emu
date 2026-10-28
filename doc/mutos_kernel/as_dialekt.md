# Der Dialekt des MUTOS-`as` (Stand 2026-10-27, Entwurf 41, AP K3/K4)

Wofuer: `z8kdis --format as` (`tools/z8000/z8kdis.h`) soll Quelltext schreiben, den der `as` von der Diskette `usr` annimmt.
Dieses Blatt trennt, was **belegt** ist, von dem, was **vermutet** wird.  **Der Rundlauf gegen den Gast-`as` ist gefahren (AP K4): der ganze Kernel `mutos_1216` (36 352 B) kommt aus `z8kdis --format as`
-> Gast-`as` bytegleich zurueck** (`tools/mutos8000/kernel/rundlauf_gast.py`, ~3 Min. Wanduhr).  Abschnitt "Gemessen" unten; die
Unsicherheitenliste dahinter ist mit dem Stand je Punkt versehen.

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

## Gemessen gegen den Gast-`as` (AP K4, 2026-10-27)

Verfahren: `tools/mutos8000/kernel/gast_as.py` (Quelldiskette bauen, MUTOS im Emulator fahren, `as x.s`, Objekt per `get` zurueck),
Objekt-Text mit `objformat.py`.  Der `as` schreibt immer **`asm.out`** (a.out-Objekt, Magic E807, `flag 0`, Text + je Wort ein
Relokationswort + Symbole) -- nicht `a.out`/`l.out`; er braucht keinen `ld`.  Meldungen stehen auf der Standardfehlerausgabe, Format
`<modul> :   line: N.  <text>`.  ~25 s Wanduhr (Emulator) je Lauf plus ~20 s je Quelldatei.

Belegt (Bytes gegen Erwartung):
* Alle Befehls- und Operandenformen, die im Kernel vorkommen (270 verschiedene Formen), ausser den unten aufgefuehrten, uebersetzen sich zu den
  Originalbytes (Bytevergleich am ganzen Kernel, s. Rundlauf unten).
* `jr`/`calr`/`djnz`/`dbjnz` mit **Marken** laufen (Kurzform, Reichweite wie im Handbuch).  Mit einer **Konstante** als Ziel meldet der `as`
  `ILLEGAL VALUE` -- ein relativer Sprung braucht eine echte Marke im selben Stueck.
* Bedingungen `z nz c nc ne eq ult uge ugt ule lt ge le gt pl mi` gehen bei `jp` **und** `jr`; `t` ist keine Bedingung (`undefined symbol`).
* Hexzahlen vierstellig mit fuehrenden Nullen (`%0004`), kleingeschrieben (`%ff`) und `#%FFF2`/`-%E` gehen; Dezimalzahlen ohne Praefix
  (`#12`, `#4660`) gehen, auch bei `inc r2,#16` (Kodierung 15).
* Konstante als Adresse: `jp Q`, `call Q`, `ld r2,Q`, `ld r2,#Q`, `clr Q`, `Q(r5)`, `Q+2(r5)` -- absolut, keine Relokation (`:= %1234`).
* Marke als Index-Basis `L1(r5)`, `L1+%2(r5)`, `WV %1234+L1`, `WV L1-%2` gehen.
* `push @r15,#%0001` / `push @SP,#%0002` gehen; **`push @-SP,...` ist ein Syntaxfehler** (z8kdis schreibt es nie).
* `ldps %0100`/`ldps @r4`, `ldctl psapseg|psapoff|nsp|fcw|refresh`, `mset`, `resflg c`, `setflg z`, `di vi,nvi`, `ei nvi`, `in`/`out` mit `%0080`: ok.
* Daten: `WV a;WV b` und `BV a;BV b` (je Wert eine Anweisung); `WV %1,%2` (mit Komma) und mehrere hundert Anweisungen je Zeile gehen ebenfalls.
* Kommentare `! ... !` (Zilog) werden angenommen.
* **`$ABS %0` ist wirkungslos in dem Sinn, dass der `as` dann gar keinen Text anlegt** (Objekt `text 0`) -> z8kdis schreibt es nicht mehr.
  Der Text beginnt ohne `$ABS` bei 0, das ist der gewuenschte Anfang.
* **Namen:** Beschriftungen/Modulnamen, die wie ein Fliesskommaregister aussehen (`f1`, `f2` ... `f15`), liefern `illegal floating register`;
  ein Name `s` gibt `syntax error` (vermutlich Schluesselwort/Direktive `$S`).  z8kdis verwendet `Lxxxx`, `start`, `kteilNN`, `_modul`.
* **Grenzen des `as`** (gemessen): ein Unterprogramm mit **~1500 lokalen Marken** liefert `fatal error: symbol table overflow`
  (1500 Marken: ueberlaufen; ~500 Marken/Stueck gingen); mehr als **~3000 Konstanten** `overflow of __constk__`/`expression/operands list`.
  Beim Kernel (1437 Marken + Konstanten) reicht ein einziges Unterprogramm daher NICHT -- er wird in Stuecken von ~4 KB uebersetzt
  (`z8kdis --teile`/`--teil`), Marken ausserhalb des Stuecks als absolute Konstanten.  Quellgroesse 36 B bis 185 KB je Gesamtlauf kein Problem.
* Ladeadresse: der `as` setzt jedes Stueck ab 0; auf dem Host wird wie bei `ld -T` die Textbasis auf Marken-Relokationen (Art 1) addiert
  (`rundlauf_gast.py::objekt_text`).

## Stand der urspruenglichen Unsicherheiten (K4)

1. `$ABS %0`: **belegt unbrauchbar** (kein Text); entfernt.  Kein `ld` noetig, Ladeadresse setzt der Host (Relokation Art 1).
2. `<marke>(rN)`: **belegt** (`L1(r5)`, `L1+%2(r5)`, auch im Kernel vielfach).
3. `push @SP,...` / `@-SP`: `@SP` **belegt**, `@-SP` Syntaxfehler (nicht benutzt).
4. Hexzahlen vierstellig mit fuehrenden Nullen: **belegt**.
5. Bedingungen `z c nz nc` bleiben ungenutzt -- `eq ne ult uge` **belegt**; `t` ist keine Bedingung.  `pe`/`po` kommen im Kernel nicht vor (ungeprueft).
6. Dezimalzahlen bei `inc/dec/sla/ldk/bit`: **belegt**.
7. `jr`/`calr`/`djnz`: **belegt**, mit der einen Ausnahme `jr` bei -127 Woertern (`ILLEGAL VALUE`); z8kdis schreibt dort `WV`.  Ziel muss Marke im selben Stueck sein.
8. `! ... !`-Kommentare: **belegt**.
9. `$SEGMENTED`/`<<s>>`: **nicht belegt**, der Kernel braucht es nicht (nichtsegmentiert).
10. Zweitkodierung/unbekannte Worte -> `WV`: **belegt** (WV-Zeilen im Kernel; im Kernel kommt keine Zweitkodierung vor).
11. `BV a,b`: **belegt** (geht auch mit Komma).
12. Groesse: **gemessen** -- ein Unterprogramm mit ~1437 Marken + Konstanten scheitert (`symbol table overflow`), Stuecke von 4 KB gehen; siehe oben.

## So wird der Dialekt gemessen (K4, erledigt)

`z8kdis --format as ... > kernel.s`, Quelldiskette mit dem DiskTool (`create`/`put`) bauen, im Gast `as kernel.s` und `ld`
(Rezept `tools/mutos8000/kernel/probe/cc_probe.dbg`; ~1 Min. Maschinenzeit je Kleinstdatei, ein 36-KB-Kernel deutlich laenger).
Jede Ablehnung des `as` wird hier unter "belegt" nachgetragen und `asQuelle()` angepasst; bytegleich zum Abbild heisst: Dialekt stimmt.

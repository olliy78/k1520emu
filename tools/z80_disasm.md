# Z80-Disassembler — Werkzeuge & Anwendung

Die Emulation bringt mehrere Z80-Disassembler mit, gestaffelt nach Zweck. Diese
Doku erklärt **welchen wofür** und wie man sie einsetzt.

| Werkzeug | Zweck | Status |
|----------|-------|--------|
| **`z80_disasm2.py`** | generischer, vollständiger Disassembler für **Listings** | **kanonisch** |
| **`z80dis_min.h`** | eingebauter Ein-Instruktions-Decoder (C++) für `k1520dbg` | header-only |
| `disasm_difftest.py` | Regressionswächter: vergleicht `z80_disasm2.py` gegen `z80dis` | Verifikation |

Faustregel: **`z80_disasm2.py` für jede Datei/jedes Listing**, der **eingebaute
Decoder** für interaktives Debuggen (`k1520dbg u/s/n`), `disasm_difftest.py` **vor
jedem Umbau** der Engine.

---

## 1. z80_disasm2.py — generischer Disassembler (kanonisch)

Multi-Pass (recursive descent + linear) mit eigener, vollständiger Instruktions-
tabelle (Basis/ED/CB/DD/FD). Konfigurierbare ORG, Einsprungpunkte, Symbol-Labels und
Kommentare. Ausgabe als Listing `ADDR  hexbytes  instruktion  ; kommentar`.

```
python3 tools/z80_disasm2.py <datei> [optionen]

  --org ADDR            Lade-/Ursprungsadresse (hex 0x.. / ..H oder dez). Default 0
  --entry ADDR          zusätzlicher Recursive-Descent-Einstieg (wiederholbar).
                        Nötig für Code, der nur zur Laufzeit angesprungen wird
                        (IM2-ISRs, ZVE2 @ 01DD über RAM-Vektor JP 01DD, …).
  --range START:END     diesen Bereich linear dekodieren (wiederholbar)
  --linear              gesamtes Image nach Recursive Descent linear dekodieren
  --label NAME=ADDR     symbolisches Label für eine Adresse (wiederholbar)
  --comment ADDR=TEXT   Inline-Kommentar an einer Adresse (wiederholbar)
  --no-strings          ASCII-String-Erkennung abschalten
  --title TEXT          Titelzeile im Listing-Header
```

**Recursive descent vs. linear.** Standardmäßig folgt der Disassembler dem
Kontrollfluss ab `--org`/`--entry` und markiert nicht erreichten Bytes als Daten
(`DB`). Code, der nur über einen RAM-Vektor oder eine Interrupttabelle angesprungen
wird, **muss** als `--entry` geseedet werden — sonst erscheint er als `DB`-Bytes
(siehe das ZVE2-Beispiel unten). `--linear` erzwingt zusätzliche lineare
Dekodierung des gesamten Images.

### `--format mac` — assemblierbarer M80-Quelltext aus Abzug + Karte

Statt eines Listings entsteht eine `.MAC`-Quelle (Marken `Lxxxx`, `DB`/`DW`/Text,
`FILLFF`-Makro, je Ladesegment `.PHASE` mit `SEGnnnn:`/`SEGnnnnE:` und Auffüllung
auf 256 B wie bei UDOS-Dateien). Richtig ist, was beim Neubau **bytegleich**
herauskommt; ob eine Stelle Code oder Daten ist, darf falsch sein. Undokumentierte
oder nicht eindeutig assemblierbare Befehle (SLL, ED-Dubletten, IXH/IXL, `ED 63/6B`)
kommen als `DB`.

```
--format mac            statt Listing
--karte DATEI           Kartendatei (unten)
--seg ADR:OFFSET:LAENGE Ladesegment (wiederholbar); ohne: ORG:0:Dateilänge
--entry/--label/--comment/--linear/--range   wie im Listing (label = name, comment = ik)
--adr                   Adresskommentar "; 9000" hinter jedem Befehl
--auffuellen N          Segmente auf Vielfaches von N auffüllen (Zweierpotenz, 0 = aus; Vorgabe 256)
--[no-]zeiger           LD rr,nn-Zeiger als Code-Einstieg (Vorgabe an)
--[no-]folge            Befehl hinter JP/JR/RET als Einstieg versuchen (Vorgabe an)
```

Die Codeverfolgung bricht an `FFH`-Läufen ab (Fülldaten sind kein `RST 38H`-Code).
Kartendatei (hex, `#` = Kommentar; Beispiel `tests/fixtures/mutos8000/mutosio_quelle/karte.txt`):

```
seg <adr> <dateioffset> <länge>   code <adr>…   db|dw|dwc|str|txt <von> <bis>
name <adr> <marke>   kom <adr> <text>   ik <adr> <text>   eq <marke> <adr>
```

`dwc` = Wortetabelle, deren Ziele zugleich Code-Einstiege sind; `str` = Zeichenketten
mit Längenbyte; `kom` = Kommentarzeile **vor** der Stelle (mehrfach möglich), `ik` =
Zeilenkommentar **hinter** der Zeile. So entsteht z. B. die Quelle von `mutos_io`
reproduzierbar (Wächter `tests/python/test_z80_disasm.py`):

```sh
python3 -I tools/z80_disasm2.py --format mac --karte tests/fixtures/mutos8000/mutosio_quelle/karte.txt mutos_io > mutosio.mac
```

### Kanonischer Aufruf für das ZRE-Boot-ROM (K2526)

ZVE2-DMA-Code (`0x01DD`) und der Index-Puls-ISR (`0x01C7`) werden nur zur Laufzeit
über RAM-Vektoren erreicht und müssen als `--entry` geseedet werden:

```sh
python3 tools/z80_disasm2.py doc/EPROMS/zre.rom --org 0 \
  --entry 0x007A --entry 0x01C7 --entry 0x01DD \
  --label LOADADDR=0x03F0 --label EXP_CYL=0x03F3 --label EXP_HEAD=0x03F4 \
  --label EXP_SEC=0x03F5 --label EXP_SIZE=0x03F6 --label IDXCNT=0x03F7 \
  --label DONEFLAG=0x03F8 --label PATHBYTE=0x03FD --label BYTECNT=0x07F1 \
  --label SECCNT=0x07F2 --label ISR_IDXDEC=0x01C7 --label ZVE2_STROBE=0x01DD \
  --label ZVE2_DONE=0x0267 --title "ZRE Boot-ROM K2526"
```

### Geladenen Code aus dem laufenden Emulator disassemblieren

Code, der erst zur Laufzeit im RAM liegt (Bootloader-Stufen, geladenes OS,
`format.com`), zuerst mit `boot_trace -d` oder `k1520dbg save` aus dem Speicher
sichern, dann disassemblieren:

```sh
# RAM-Bereich sichern (boot_trace) …
./build_trace/boot_trace -d 0x1C00:0x2080 /tmp/loader.bin tests/fixtures/disks/cpa_cpa780_k5601_clock.img
# … oder interaktiv (k1520dbg):  save /tmp/loader.bin 0x1C00 0x480
python3 tools/z80_disasm2.py --org 0x1C00 --entry 0x1F7D /tmp/loader.bin
```

---

## 2. z80dis_min.h — eingebauter Ein-Instruktions-Decoder

Header-only-C++-Decoder (`tools/z80dis_min.h`), den `k1520dbg` für Inline-Disassembly
an jedem Stopp/Step, für `step-over`/`step-out` (er liefert Länge + CALL/RET/Block-
Klassifikation) und für das `u`-Listing nutzt — **ohne** externen Python-Aufruf.

```cpp
#include "tools/z80dis_min.h"
z80dis::Insn d = z80dis::decode(readByte, pc);
//  d.len        Instruktionslänge in Byte
//  d.text       Mnemonik (Operanden hex, ..H-Notation)
//  d.is_call    CALL/CALL cc/RST   → step-over setzt BP bei pc+len
//  d.is_ret     RET/RET cc/RETI/RETN
//  d.is_jump    JP/JR/DJNZ
//  d.is_repeat  LDIR/CPIR/INIR/OTIR/…  (auto-repetierend → step-over)
//  d.has_target / d.target   absolute Zieladresse (CALL/JP/JR/RST)
```

Vollständige Z80-Tabelle (Basis/ED/CB/DD/FD) per Octal-Dekodierung. Gegen das
gesamte Boot-ROM **längengenau** verifiziert (515 Instruktionen, identischer
Adressstrom zu `z80_disasm2.py`). Bewusst kompakt: der undokumentierte IXH/IXL-
Sonderfall in kombinierten `(IX+d)`-Befehlen ist kosmetisch vereinfacht; die *Länge*
ist in allen Fällen korrekt (entscheidend für step-over). Für vollständige,
gelabelte Listings bleibt `z80_disasm2.py` kanonisch.

---

## 3. disasm_difftest.py — Regressionswächter

Vergleicht die handgeschriebene Engine von `z80_disasm2.py` an **jedem** Byte-Offset
gegen den unabhängigen Drittanbieter-Decoder `z80dis`. Instruktionslänge ist die
objektive Invariante; numerische Operanden werden notationsunabhängig verglichen
(hex `..H` / `0x..` / dezimal). Findet Off-by-one-Operanden, falsche Längen, Lücken.

```sh
venv/bin/python tools/disasm_difftest.py doc/EPROMS/zre.rom --org 0
venv/bin/python tools/disasm_difftest.py boot_disk/format.com --org 0x100
```

Benötigt `z80dis` (`venv/bin/pip install z80dis`). Exit 0 = deckungsgleich.
**Vor jedem Umbau der Disassembler-Engine laufen lassen.**

Verifikationsstand 2026-06-05: zre.rom (1024 Offsets) und format.com (17408 Offsets)
— je 0 Längen- und 0 Operanden-Abweichungen.

---

## 4. Zweitwerkzeuge

`z80_disasm.py` und `z80_disasm3.py` (format.com-spezifisch) sowie `mutos_io/dis.py`
sind seit 2026-10-27 gelöscht — **ein Disassembler, ein Ort**. Als Orakel dient
`disasm_difftest.py` mit dem Drittanbieterpaket `z80dis`.

---

## 5. Bekannte Grenzen

* **DD/FD-Schatten-Präfix:** Vor einem Nicht-Index-Opcode zeigt `z80_disasm2.py`
  `DB DDH` + Folgebefehl, `z80dis` ignoriert das Präfix (hardware-näher). Betrifft
  nur fehlausgerichtete Offsets/Datenbytes; ausgerichteter Code ist bit-genau.
* **IXH/IXL** in kombinierten `(IX+d)`-Befehlen: beim eingebauten Decoder kosmetisch
  vereinfacht (Länge korrekt). Für solche Sonderfälle `z80_disasm2.py` heranziehen.

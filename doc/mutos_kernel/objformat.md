# MUTOS a.out-/Archivformat (AP K2, 2026-10-27)

Belegt an `lib/crt0.o`, `lib/mcrt0.o`, `lib/libc.a` (140 Mitglieder), `bin/ld` (`…_best_usr.zip`) und `include/a.out.h`, `include/ar.h`.
Code: `tools/mutos8000/kernel/objformat.py` (Lesen/Schreiben, CLI `dump|nm|ar-list|ar-extract`),
Wächter `tests/python/test_mutos_objformat.py`.  **Rundlauf: Lesen -> Schreiben ist für das ganze Archiv, alle 140 Mitglieder, `crt0.o` und
`mcrt0.o` bytegleich.**  Alles big-endian, Wortgröße 16 Bit.

## Objekt / Programm

```
Kopf 16 B:  magic text data bss syms entry unused flag     (8 x u16)
text | data | Relokation Text | Relokation Daten | Symbole
```
* Magics `E807` (A_MAGIC1), `E811`, `E805` (`a.out.h`); alle gemessenen Dateien tragen `E807`.  `AF_STRIP` = `flag` Bit 0.
* Objekt (`flag 0`): hinter `text`+`data` steht **je Wort ein Relokationswort** (Text: `text/2` Wörter, danach Daten: `data/2` Wörter), dann `syms` Bytes Symbole.
  Belegt: `crt0.o` 200 B = 16 + 48 + 2 + (48+2) + 84; `getgrgid.o` 228 = 16 + 40 + 40 + 132.
* Gestrippte Programme (`flag 1`: `bin/ld`, `bin/as`, `mutos_*`-Werkzeuge): **keine Relokation, keine Symbole**; Dateilänge = 16 + text + data
  (`ld`: 10726 = 16 + 0x25D6 + 0x400).  `entry` 0 = Start am Textanfang (dort liegt `crt0`).
* `a_unused` war überall 0.

### Relokationswort  (PDP-11-Schema)
`wort = (symnr << 4) | (art << 1) | pcrel`; Wort 0 = nicht relokiert.

| art | Bedeutung | Beleg |
|---|---|---|
| 0 | absolut (nie ungleich 0 beobachtet) | – |
| 1 | Text | 91 Stellen in libc.a |
| 2 | Daten | 115 + 49 (Daten) |
| 3 | BSS | `crt0.o` Textwort `0x20` = `0032` (= text 0x30 + data 2) mit Wort `0006` |
| 4 | extern, `symnr` = Index in die Symboltabelle | `crt0.o`: `CALL _main` Wort `0x24` -> `0018` (sym 1), `CALL _exit` Wort `0x2C` -> `0008` (sym 0) |

`pcrel` (Bit 0) kam in **keiner** gemessenen Datei vor (Z8000 `CALL`/`LDA` mit absolutem Wort; `JR`/`CALLR` sind ohnehin selbstbezüglich).
Der Wert im Textwort ist der Addend (bei extern meist 0; bei Text/Daten/BSS die objektinterne Adresse).

### Symbol (12 B)
`char name[8]` (NUL-aufgefüllt, 8 Zeichen ohne Terminator), `u8 typ`, `u8 unused`, `u16 wert`.
`typ & 0x1F`: 0 undef, 1 abs, 2 text, 3 data, 4 bss; `0x14` (`N_REG`) Registervariable (Debug, Wert = Registernummer), `0x1E` (`N_SN`) Abschnittsmarke
(`BSS_B`, `crt0_s_d`), `0x1F` (`N_FN`); `0x20` = extern.  Beispiel `crt0.o`: `_exit` `20` (undef extern), `start` `22` (text extern, 0), `$1` `02` (lokal text 0x1A), `_environ` `24` (bss extern 0x32).
Namen sind auf 8 Zeichen gekürzt (`_getgrgi`); Compilernamen tragen `_`-Präfix, lokale Marken `Ln`/`$n`/`~name` (Debug-Name der Funktion).
Es gibt **keine Common-Symbole** (undef mit Wert ≠ 0 wurde nicht beobachtet; vermutet wie V7: Wert = Größe) – Lücke.

## Archiv (`libc.a`)
* Magic `FF65` (0177545) als u16 big-endian; danach je Mitglied 26-B-Kopf **V7-artig, aber big-endian und `ar_size` als u32**:
  `name[14]`, `date u32` (Beispiel `1f02a970`), `uid u8`, `gid u8`, `mode u16` (`81b4` = 0100664), `size u32`; dann `size` Bytes Objektdatei.
* Auffüllung ungerader Größen: **nicht belegt** (alle 140 Größen gerade); `objformat.py` merkt sich ein Füllbyte, falls eines folgt – vermutet wie V7.
* Reihenfolge ist die Reihenfolge der Auswahl durch `ld` (s.u.).  **Kein `__.SYMDEF`** in `libc.a`, obwohl `ld` die Zeichenfolge kennt (ranlib-Unterstützung vorhanden, ungenutzt).

## `ld` – aus den Eingaben und dem Binärabbild abgeleitet
Belegt (Gegenprobe `finde_signaturen.py suche libc.a bin/ld`, Versatz = Datei − 16):
* `crt0` steht am Textanfang von `ld`, feste Bytes identisch.
* Archivmitglieder liegen im Text **lückenlos hintereinander in Archivreihenfolge** (`stty.o` 52 B unmittelbar vor `strncmp.o`; Systemrufe `access chmod close creat fstat … write cerror csv` in Archivreihenfolge; `csv.o` zuletzt).  Textlängen sind gerade, eine Ausrichtung zwischen Mitgliedern ist nicht nötig/nicht sichtbar.
* Reihenfolge der Abschnitte im Programm: Text | Daten | (BSS nur als Größe im Kopf).  BSS hinter Daten (`_environ` = text + data).
* `ld` kennt (Zeichenfolgen): Ausgabe `a.out` (Vorgabe) bzw. `l.out`, Temporärdatei `/tmp/ldaXXXXX`, `/usr/lib/lib…` (`-l`), `__.SYMDEF`, Meldungen
  `Multiply defined`, `Relocation error`, `Entry point not in text`, `Bad 'use' or 'entry'`, `Symbol table overflow`, `No pages`,
  und **`CALLR : illegal destination` / `CALLR : out of range`** – `ld` kann also `CALL` zu `CALLR` (12-Bit-PC-relativ) verkürzen/prüfen.
Vermutet (nicht belegt, Z8000-Code von `ld` nicht disassembliert): V7-`ld`-Optionen `-r -s -x -u -d -l -o -e -n -i -T`; Archivmitglieder werden iterativ gezogen, bis keine undefinierten Symbole mehr auflösbar sind; `-r` erhält Relokation und Symbole (Objekt wieder `flag 0`).  Die Zwei-Pass-Struktur (Temporärdatei) deutet auf Text/Daten getrennt gesammelt und am Ende konkateniert.

## Signaturen und Kernel
`doc/mutos_kernel/libc_signaturen.txt` (Name, Textlänge, Wildcard-Wörter, SHA-1 über maskierten Text + Maske), erzeugt/geprüft von `finde_signaturen.py liste`.
Relokierte Wörter sind Wildcards.  Gegenprobe an `bin/ld` findet u.a. `printf.o`, `stty.o`, `strncmp.o`, `signal.o`, `sbrk.o`, `csv.o`.

**Kernel `mutos_1216` (36352 B, Basis 0 vermutet): keine einzige exakte libc-Funktion.**  Unscharf (>= 70 %) nur `csv.o` bei `05F8` (86 %: 14 Wörter, drei Wörter
weichen ab – der Kernel sichert andere Register); `fopen.o` bei `1636` (70 %) ist Zufall (28 B).  Der Kernel hat also dieselbe Aufruf-/Rahmenkonvention
(`csv`/`cret`, 226 `CALL 05F8`), nutzt aber nicht die Anwenderbibliothek; Hilfsfunktionen (`bcopy`, `strlen`, `ldiv`, Zahlenausgabe) sind eigene Kernelfassungen.
Folgerung: derselbe Compilertyp ist plausibel, bytegleiche Wiedererkennung per libc-Signatur trägt nicht.  Besser: Kernel-eigene Funktionsanfänge (Aufrufziele) und
Strukturvergleich mit den Compilerausgaben (`c1`) im Gast.

## Lücken
* `pcrel`-Relokation, `art 0`, Common-Symbole, Füllbyte ungerader Archivgrößen, `E811`/`E805` (Pure-/getrennte-I&D-Programme) nie beobachtet.
* `ld`-Optionen/Algorithmus nicht aus dem Code belegt (Disassembler steht aus).
* `nm` (`best_cc2`) nicht ausgewertet.

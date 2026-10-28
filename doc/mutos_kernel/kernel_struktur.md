# Kernel `mutos_1216`: Struktur nach Ausfuehrungsbeleg (AP K6a, 2026-10-27)

Abbild: 36 352 B, Ladeadresse 0, nichtsegmentiert (Befund K0).  Quelle der Aussagen: **Laufprotokoll** (Befehlsabdeckung des U8001 im Emulator),
statische Ablaufverfolgung von `z8kdis` auf den gelaufenen Befehlen, Aufbau der Tabellen, Vergleich mit dem WEGA-Kernel (`OlliL/P8000`,
`WEGA/src/uts`, nur als Orientierung).  Jede Aussage ist als **[belegt]** (Lauf, Tabellenaufbau oder Zeichenkette im Abbild) oder **[vermutet]** gekennzeichnet.

## 1. Laufprotokoll

* Werkzeug: `k1520dbg --em em256`, im Kontext `cpu u8000` neu `cover on|off|reset|report [datei]` (zaehlt jeden Befehlsanfang des U8001, getrennt nach
  System-/Normalbetrieb; Wächter `dbg_u8000_cover`).  Ein Lauf = ein Skript, kein Haltepunkt je Adresse.
* Läufe (`tools/mutos8000/kernel/lauf/lauf.py`, Rohdaten nur Kernel = Segment 0, Systembetrieb, Adresse < 8E00):

| Lauf | Inhalt | verschiedene Befehlsanfaenge |
|------|--------|------------------------------|
| `boot` | Kaltstart, Ladediskette, Wurzel, Anmeldung `root`, `ls -l /`, `cat /etc/passwd` | 5 049 |
| `shell` | 57 Befehle: `ls cat pwd date df who ps pstat stty sync`, Fehlerfaelle (fehlende Datei, Verzeichnis lesen, Nicht-Verzeichnis, Ausfuehren ohne x-Recht, `rm`/`cd`/`ln` auf Fehlendes, `kill -9 99`), `mkdir cp mv ln rm rmdir chmod chown cmp od wc` (auch 18-KB-Dateien), Pipes (`ls \| cat`, `\| wc \| cat`, `\| pr`), Umlenkung, Hintergrund (`&`, `wait`, `kill $!`), `umount`/`mount`, `ed` | 6 880 |
| `cc` | `cc -c` (zwei Dateien), `cc` Binden, `./a.out`, Fehler im Quelltext, `cc -S`, fehlende Datei | 6 111 |
| **gesamt** | Vereinigung | **6 888** |

  (`cc` liefert gegenueber `boot`+`shell` nur 8 neue Anfaenge: Prozesserzeugung, Swap und Dateisystem sind nach 57 Befehlen ausgereizt;
  `gesamt` ist also nahe der Obergrenze dessen, was MUTOS auf der Konsole ohne weitere Hardware ausloest.)
* **Nicht erreicht** (nicht gefahren): zweites Terminal `tty1` (Telnet-Gegenstelle noetig, Entwurf 39 §11), Band (`tm`: Zeichenkette `tmintr`), Druckerausgabe `lp`,
  Speicherdruck (`out of swap space`), Fehlerpfade im Dateisystem (`bad block`, `Out of inodes`), echte Signalzustellung an einen laufenden Prozess.
  Die Läufe bilden die haeufigen Pfade ab; fehlerhafte Platten und Panics werden bewusst nicht ausgeloest.
* Wiederholung: `python3 -I tools/mutos8000/kernel/lauf/lauf.py alle` (Laufzeit ca. 4 Minuten Wanduhr; Arbeitsordner Umgebungsvariable `K6A_ARBEIT`),
  danach `python3 -I tools/mutos8000/kernel/lauf/abdeckung_zu_karte.py`.  Ergebnis je Lauf `lauf/abdeckung_<name>.txt`, Vereinigung `abdeckung_gesamt.txt`.

## 2. Karte vorher/nachher

`tools/mutos8000/kernel/karte.txt` wird jetzt erzeugt: handgepflegte Basis `lauf/karte_basis.txt` (Segment, PSA, Vektoren, Tabellen, Namen, Kommentare)
plus `code`-Einstiege aus dem Lauf.  Der Erzeuger nimmt so wenige Einstiege wie moeglich (nur 61, weil die Ablaufverfolgung den Rest erreicht) und prueft
**jeden** der 6 888 ausgefuehrten Befehlsanfaenge gegen die Verfolgung.

| Stand | Code | Daten (Karte) | Unerreicht |
|-------|------|---------------|-----------|
| vorher (nur PSA, Vektoren, 027E) | 28,2 % (10 252 B, 3 365 Befehle) | 1,6 % | 70,2 % |
| nur Lauf (61 Einstiege) | 76,3 % | 1,6 % | 22,1 % |
| **nachher** (Lauf + Tabellen, 1 853 Einstiege der Verfolgung) | **85,2 %** (30 988 B, 10 039 Befehle) | **6,3 %** (2 274 B) | **8,5 %** (3 090 B) |
| Obergrenze mit allen `--vorschlaege` | 92,8 % | | |

`z8kdis --pruefen`: Rundlauf 36 352 B, **0 Abweichungen**.  Überlappung 0, ungueltig 0, nicht kanonisch 0, Marken mitten im Befehl 0.

**`--vorschlaege` gegen das Protokoll** (498 Kandidaten der Ausgangskarte):

| Urteil | Zahl | Kriterium |
|--------|------|-----------|
| bestaetigt | 224 | Ziel ist ein ausgefuehrter Befehlsanfang |
| widerlegt | 46 | Ziel liegt **mitten in** einem ausgefuehrten Befehl (Zufallstreffer: Adresskonstante im Befehl, Datenwort) |
| statisch erreicht, ohne Lauf | 26 | die Verfolgung der gelaufenen Befehle fuehrt ohnehin hin |
| ungeklaert | 202 | nie ausgefuehrt; davon 142 mit >= 8 Befehlen |

Nach der Karte bleiben 41 Kandidaten; die meisten sind Sprungoperanden in selbst nie ausgefuehrtem Code des Treibers `7E16..8750` und von `0D56..1111` [vermutet: Code].

## 3. Speicherbild

| Bereich | Inhalt | Beleg |
|---------|--------|-------|
| `0000-0037` | Programmstatus-Feld: 7 Eintraege, Reset `027E` (FCW `C000`), Traps FCW `40xx` -> `030E` | [belegt] |
| `0038-023D` | Eintrag 7 (VI), 128 Vektoren `[Kennung, Handler]`; fast alle `000F -> 024E` (Standardbehandlung) | [belegt] |
| `023E-0673` | Assembler: Start `027E` (144 B), Trap-Eintritt `030E` (234 B), kleine Helfer `0474-0488` (Stack-/PS-Zugriff, 4-12 B), **`csv` = `05F8`** (182 Aufrufer), `cret` = `0606` | [belegt] (csv/cret), Rest [vermutet: `mch.s`] |
| `0674-875F` | C-Funktionen (Prolog `call csv`), 207 von 235 Funktionsanfaengen | [belegt] |
| `875A-8765` | Konstanten (`C000 03F6 0000 1000 ...`) | [belegt] Daten |
| `8766-8795` | **bdevsw** (4 Worte: open, close, strategy, d_tab) | [belegt] |
| `8796-8875` | **cdevsw** (7 Worte: open, close, read, write, ioctl, `2438`, tty-Zeiger); letzter Eintrag `8868` weicht ab | [belegt] |
| `887C` | rootdev (Major 4, Minor 02), `887E` swapdev, `8880` pipedev, `8884` swplo (4 B, 250), `8888` nswap (2 B, 244) | [belegt] (Offsets wie `mutos_patch.py`) |
| `888A-88B5` | Zeigertabelle in den BSS-Bereich (`A11C`, `9EEE`, `A530`, `8C94`, `A620` ...) | [belegt] Form, Bedeutung [vermutet: Tabellenanfaenge `buf/proc/inode/file` o. ae.] |
| `88C6-8DB8` | Texte (Abschnitt 5), `sysent` ab `8B56`, Vorbesetzungen | [belegt] |
| `8C90-8DFF` | Tabellen/Bytes (u. a. Zuordnung Tastatur `8CF2-8D13` als Zeichentabelle) | [vermutet] |

### Geraetetabellen (belegt)

* **bdevsw** bei `8766` (8-Byte-Eintraege), Nummern 0..5: Major 0 = `7E16/7E94/7F94`, Tab `8E6A` (Treiber-Code im Lauf nie erreicht: **nicht** `rk`);
  Major 1..3 = `nodev` (`242A`); **Major 4 = `rk`**: open `6ADA`, close `6AFE`, strategy `6B06` (im Lauf ausgefuehrt), Tab `8E82`; Major 5 = leer.
  Passt zu `rootdev = (4,2)` und Entwurf 39 (`brw 4,n`).
* **cdevsw** bei `8796`, Eintrag = 7 Worte: Major 0 = Konsole/tty (`60CC 616C 618E 61B0 623C`), 1 = `mem/kmem/null` (`6C8E - 6CC6 6CEC 6D12`), 2 = Tauschgeraet [vermutet] (`69CE 6A52`),
  7 = `6444 64DA - 6570` [vermutet: Band oder Drucker], 10 = Rohzugriff auf Major 0 (`7E16 7E94 84E4 851C 85B0`), **13 = `rrk`** (`6ADA 6AFE 6C52 6C70`, = `crw 13,n` der Wurzel), `8868` = anderer Aufbau (`6D44 - 7C90 7DA6 - 7222 7C62`) [vermutet: Mailbox-Terminal `tty1` aus `mutos_io`].
* **sysent** ab `8B56`, je 4 B `[Funktion, Argumentzahl<<8]`: Argumentzahlen und Funktionsanfaenge stimmen mit der V7-Reihenfolge (`fork 0`, `read 3`, `write 3`, `open 2`, `close 1`, `wait 0`, `creat 2`, `link 2`, `unlink 1`, `exec 2` ...) ueberein [belegt: Zahlenfolge];
  `0B2A` und `0B38` sind die Platzhalter (`nosys`/`nullsys`).  Genaue Nummer-zu-Funktion-Zuordnung der ersten Eintraege [vermutet].

## 4. Funktionen

`lauf/funktionen.txt` (erzeugt von `lauf/struktur.py`): **235 Funktionsanfaenge** (Aufrufziele + `call csv`-Prologe + `027E`/`030E`), davon **207 mit Prolog `call csv`** (C-Funktionen), 28 Assembler-/Kleinroutinen.
**189 von 235 wurden ausgefuehrt**; **46 nie**: u. a. `0B3A-0F12` (Signal-/Core-Teil, 524 B am groessten), `1856` (`panic`), `2A4C` (`timeout`-Fehlerpfad),
`35AE`, `3630`, `5F56`, `6444-6570` (Gerät 7), `6668-690A`, `75B2/772E/78A8/79E0` (Mailbox-/tty-Nebenpfad), `7E16-7F94`, `7FDC` (574 B), `84E4-85B0` (Treiber Major 0 / 10).
Spalten der Datei: Anfang, Laenge (bis letztes Code-Byte), `csv`, Zahl Aufrufer, ausgefuehrte/alle Befehle, Angerufene, Texte.

Haeufige Angerufene [belegt]: `05F8` csv (182), `599A` (31), `12FE` (31), `19A8` (28), `04B2` (23), `0484` (20), `0474` (17), `18D4` (17).
Deutung ueber WEGA/V7 [vermutet]: `19A8` = `wakeup`, `18D4` = `sleep`, `12FE` = `iget`-Aufruf-Hilfe (`ifind`/`iget`), `599A` = `brelse`/`getblk`-Familie (`bio.c`), `04B2/0484/0474` = `spl`-Gruppe (Prioritaet setzen/wiederherstellen).

## 5. Textpool (belegt, Adressen)

Vollstaendig in `lauf/textpool.txt` (42 Zeichenketten; zweite Spalte = Zahl der Befehle mit `#Adresse`).  Zusammenfassung:

| Adresse | Text | Zugehoerigkeit [vermutet] |
|---------|------|----------------------------|
| `88C6` | `iinit` | `panic("iinit")` in `main.c` (WEGA `main.c` gleiches Wort) |
| `88E4`, `8990` | `parity during `, `parity` | Paritaetstraps (`trap.c`) |
| `8920-89C3` | `seg = %x`, `aps = %x`, `pc = %x %x ps = %x`, `trap type %x`, `ident = %x`, `trap`, `%s running - segment`, `%s waiting - segment`, `Random interrupt (ident = %x) ignored` | `trap.c` (Trap-Anzeige, Z8000) |
| `8A10` | `core` | `core()` (Abbild schreiben) |
| `8A1C-8A39` | `no imt`, `Inode table overflow`, `iaddress > 2^24` | `iget.c` |
| `8A4A-8A7B` | `0123456789ABCDEF`, `panic: %s`, `%s on dev %u/%u`, `bn=%D er=%o,%o` | `prf.c` |
| `8A8D-8AC2` | `Tzero wchan`, `proc on q`, `Running a dead proc`, `no procs`, `Timeout table overflow` | `slp.c` / `subr.c` (`timeout`) |
| `8ADA` | `no file` | `fio.c` |
| `8AE4` | `map overflow` | `malloc.c` (`mfree`) |
| `8AF2-8B2C` | `Bad free count`, `no space`, `bad block`, `Out of inodes`, `bad count`, `no fs` | `alloc.c` |
| `8B42`, `8B4E` | `/etc/init`, `mem = %D\n` | `main.c` (Start von `init`, Speichergroesse) |
| `8C57`, `8C64` | `Out of swap`, `out of swap space` | `sys1.c` (`fork`), `text.c` |
| `8C76-8C84` | `blkdev`, `devtab`, `IO err in swap` | `bio.c` |
| `8D8C`, `8DB8` | `tm%d: bn = %d %d old er = %x err stat = %x`, `tmintr` | Bandtreiber `tm` (Major 7?) |

## 6. Modulreihenfolge (Hypothese)

Anker: **WEGA-Makefile** (`sys/Makefile`, `conf/make.wega`): Linkreihenfolge `wpar event mch u ver fpe conf` + `LIB1` = `main trap sig iget prf slp subr rdwri fio malloc alloc machdep nami pipe sysent sys3 sys1 sys4 sys2 acct text ureg break lock mkseg debug timer` + `LIB2` (Treiber) `bio siok koppel ud fd mem sys tty tt0 ...`.
Die Zeichenketten des MUTOS-Abbilds liegen in **genau dieser** Reihenfolge (Textpool-Reihenfolge: trap, iget, prf, slp, subr, fio, malloc, alloc, ... bio), und die Funktionsfolge im Text stimmt mit WEGA Funktion fuer Funktion ueberein
(`prf.c`: `printfv printn panic prdev deverror` = `1724 17E0 1856 187A 18A4`; `slp.c`: `sleep wakeup setrq setrun setpri sched swapin qswtch swtch newproc expand` = `18D4 19A8 1A30 1A7C 1AE8 1B34 1CCA 1D8E 1DA4 1EA2 20FE`).
MUTOS ist demnach ein Verwandter/Vorlaeufer der WEGA-`uts`-Quelle (aelter: kein `prsave/prstate`, `event.s`-Teil anders).

| Bereich (Text) | Modul | Stuetze |
|----------------|-------|---------|
| `0000-0461` | PSA + Vektoren (aus `conf`/`mch.s`) | [belegt] |
| `0462-0673` | `mch.s`-Teil: `csv/cret`, `spl`-Helfer, Registerzugriff | [belegt] csv, Rest [vermutet] |
| `0674-0879` | `main.o` (`main`, `iinit` mit `panic("iinit")`, `0746`) | Zeichenkette `iinit`, `/etc/init`, [belegt] |
| `087A-0B39` | `trap.o` | `trap type`, `ident`, `seg = ` [belegt] |
| `0B3A-1121` | `sig.o` (`0F12` 524 B = `core`) | Zeichenkette `core`, Reihenfolge [vermutet] |
| `1122-1723` | `iget.o` (`iget 1122`, `1372` = `iexpand/iupdat`) | `no imt`, `Inode table overflow` [belegt] |
| `1724-18D3` | `prf.o` | s. o. [belegt] |
| `18D4-20FD`(bis `236B`) | `slp.o` | 11 Funktionen deckungsgleich [belegt] |
| `236C-2C0B` | `subr.o` + `rdwri.o` (`2A4C` = `timeout`, davor `bmap/passc/cpass`; `2B06` `readi/iomove`-Familie) | `Timeout table overflow` bei `2A4C` [belegt], Grenze [vermutet] |
| `2C8C-2DEF` | `fio.o` (`2D9A` `falloc`) | `no file` [belegt] |
| `2DF0-2F47` | `malloc.o` (`2DF0` `malloc`, `2E5A` `mfree`) | `map overflow` [belegt] |
| `2F48-34F5` | `alloc.o` (`alloc free badblock ialloc ifree getfs update`) | sechs Texte in dieser Reihenfolge [belegt] |
| `34F6-369F` | `machdep.o` (`34F6` mit `mem = %D`, `35AE`, `3630`) | [vermutet] |
| `36A0-39B9` | `nami.o` (`36A0` `namei`, 768 B) | Groesse/Aufrufer [vermutet] |
| `39BA-3D03` | `pipe.o` (`3A68`, `3AFE`, `3BBE`...), danach `sysent` (nur Daten, `8B56`) | [vermutet] |
| `3D04-41AB` | `sys3.o` | [vermutet] |
| `41AC-4C13` | `sys1.o` (`41AC` `exec` 778 B, `44B6` `exece/getxfile`, `47E2`, `48C4`) | `Out of swap` bei `8C57` [vermutet] |
| `4C14-5555` | `sys4.o`, `sys2.o` (`4D6A`.. kleine Systemaufrufe; `4DBC` `exec`-Verwandter, `5100/5132` ...) | Argumentzahlen in `sysent` zeigen auf `48A6..5556` [belegt] |
| `55AE-57D3` | `acct.o`/`text.o` (`562C` `xalloc`, `out of swap space`) | [belegt] Text bei `562C` |
| `57D4-5C61` | `bio.o` (`57D4` `bread/getblk`-Gruppe, `5A64` `blkdev`/`devtab`, `5D4A` `swap`: `IO err in swap`) | [belegt] Texte |
| `5C62-60A7` | Block-E/A-Hilfen, `5F10`, `60A8` | [vermutet] |
| `60CC-6431` | Konsolen-/tty-Treiber (cdevsw Major 0: `60CC 616C 618E 61B0 623C`), `6274-6384` Zeitgeber/`tty`-Hilfen | cdevsw [belegt] |
| `6432-69CD` | `mem`-Gerät (Major 1), Gerät 7 (Band? `6444-6570`), `6668-690A` | cdevsw [belegt], Zuordnung [vermutet] |
| `69CE-6D43` | `rk`-Treiber (`6ADA 6AFE 6B06 6B60 6BCA`), `rrk` (`6C52 6C70`), `mem`/`kmem` (`6C8E-6D12`) | bdevsw/cdevsw [belegt] |
| `6D44-7E15` | Mailbox-Terminal (`6D44`, `6DF0`, `6E7A` 738 B, `7222` 912 B, `75B2-7C62`), `7C90/7DA6` = read/write | cdevsw Major 15 [vermutet] |
| `7E16-874F` | Treiber Major 0 (`bdevsw 0`, `cdevsw 10`; `7E16 7E94 7F94`, `84E4 851C 85B0`) -- im Lauf nie erreicht | Tabellen [belegt], Art des Geraets [vermutet] |
| `8752-875A` | letzter Code (Sprung) | [belegt] |
| `875A-8DFF` | Daten: `conf.c`-Tabellen (`bdevsw`, `cdevsw`, `rootdev..nswap`), Texte aller Module in Modulreihenfolge, `sysent` | [belegt] |

Daten stehen **hinter dem Text** in der Reihenfolge des Texts (Texte `trap` -> `iget` -> `prf` -> `slp` -> `subr` -> `fio` -> `malloc` -> `alloc` -> `main`(`/etc/init`, `mem`) -> `sys1`/`text` -> `bio` -> Band):
Das ist das Linkmuster Text/Daten/BSS aus Entwurf 41 §4.2 und stuetzt die Modultrennung fuer K5.  Auffaellig: `main.c` liefert seine Zeichenketten erst **hinter** `alloc.c`, obwohl `iinit` in `main.c` steht (`88C6`
steht aber am Anfang des Textpools) -- die Strings einiger Module liegen in der Datenreihenfolge des Linkers, nicht strikt in der Funktionsreihenfolge [vermutet: Compiler legt Konstanten je Modul an das Ende]; K5 muss das am Verschiebetest entscheiden.

## 7. Offene Punkte

* **tty1/Mailbox-Pfad** (`6D44-7C62`) und **Gerät 7** (`tm`) nicht gelaufen: Telnet-Gegenstelle bzw. Band fuer einen Folgelauf.
* **Signalzustellung** (`0B3A-0F12`): `kill $!` an `cat &` hat die Funktionen nicht getroffen (der Prozess war vermutlich schon beendet); ein Lauf mit lang lebendem Hintergrundprozess (`sleep` fehlt in den Disketten -> `cat` auf `tty1` oder `ed`) steht aus.
* 41 `--vorschlaege`-Kandidaten und 3 090 B Unerreichtes (8,5 %), vor allem in `7E16..874F`, `0D56..1111`, `6668..6909`.
* Zuordnung der ersten `sysent`-Eintraege (Nummer 0..3) und der Zeigertabelle `888A..88B5`.
* Linkkarte K5: die obige Tabelle ist Hypothese; Modulgrenzen zwischen `subr/rdwri`, `sys3/sys1/sys4/sys2` brauchen den Verschiebetest.

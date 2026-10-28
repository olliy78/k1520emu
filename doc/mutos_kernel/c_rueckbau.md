# K7-Pilot: erzeugt der MUTOS-`cc` den Kernel `mutos_1216` aus C? (2026-10-27)

**Antwort: ja.**  Mit `cc -O` (Optimierer `c2` an) liefert der Disketten-`cc` aus K&R-C **bytegleiche** Kernelfunktionen
(`call csv` ... `jp cret`, Registerwahl, Sprungform, Konstanten).  Ohne `-O` ist keine einzige Funktion gleich (0 von 25):
der Kernel wurde mit `-O` gebaut.  Von 25 Pilotfunktionen sind **22 bytegleich**, 3 weichen nur in einem erklaerten Detail ab.
Ein C-Rueckbau des ganzen Kernels ist damit erreichbar; der Aufwand liegt im Erraten der Quellform, nicht im Werkzeug.

Dateien: `tools/mutos8000/kernel/c/pilot.c` (Quellen), `gast_lauf.sh` (Gastlauf), `vergleich.py` (Vergleich).

## Verfahren
* `gast_lauf.sh ARBEIT pilot.c`: legt die Quelle auf eine Quelldiskette, bootet MUTOS im Emulator (wie `make/vorbereiten.sh`),
  fuehrt `cc -S`/`cc -c` ohne und mit `-O` aus (Ausgaben nach `err_*`) und holt `.s`/`.o` zurueck.
  **Ein ganzer Lauf dauert unter 2 Minuten Wanduhr**, unabhaengig von der Zahl der Funktionen in der Datei (Fixkosten Boot + Passen).
  Falle: im Gast am Ende `sync`, sonst stehen die Dateien nicht im Abbild (Gast-Puffer); das Skript tut es.
* `vergleich.py KERNEL OBJ QUELLE [-v]`: je `/* KERNEL xxxx */`-Marke vor einer Funktion wird deren Text (Symbol `_name` bis zum
  naechsten externen Textsymbol) mit den Kernelbytes verglichen; relokierte Woerter (Aufrufziele, Globale, `u`) sind Wildcards.
  Symbolnamen sind auf 8 Zeichen gekuerzt (`_` + 7), Funktionsnamen im Pilot daher kurz halten.  `-v` zeigt Wort fuer Wort.
* Quelle: nur so viel `struct user`/`proc`/`buf` wie noetig, Abstaende aus dem Kernel (u bei `FC00`: `u_error` +13H, `u_uid` +16H,
  `u_gid` +18H, `u_ruid` +1AH, `u_rgid` +1CH, `u_procp` +1EH, `u_ap` +20H, `r_val1/2` +22H/24H, `u_ofile[20]` +62H, `u_pofile` +8AH).

## Ergebnis je Funktion (Kerneladresse, -O)
Bytegleich (22): `2766 min` (unsigned), `2D48 suser`, `2D62 ufalloc`, `2AD4 getf`, `4C14` (long `lbolt` nach `u_r_val1`),
`4CF4 getuid`, `4D32 getgid`, `4D4A getpid`, `4CC6 setuid`, `4D0C setgid`, `5100 nice`, `5132 umask`, `60A8 geterror`,
`5C62 iodone`, `5C2E notavail`, `3BBE plock`, `3BE0 prele`, `5A22 incore`, `20F6 nulldev`, `6ADA` (Argumentpruefung), `6432`, `48A6`.
Quellformen siehe `pilot.c`; V7-Quellen (`OlliL/P8000`, `WEGA/src/uts/sys`) passen bis auf die Struktur-Abstaende, z. B. `setuid`:
`if (u.u_ruid == uid || suser()) { u.u_uid = uid; u.u_procp->p_uid = uid; u.u_ruid = uid; }`.

Nahe (3, je ein erklaerter Unterschied):
* `243A` Byte-Kopierschleife und `5C86 clrbuf`: Kernel hat `DEC Rn,#1 ; JR NZ,loop`, unser `c2` macht daraus `DJNZ Rn,loop` (ein Wort kuerzer,
  Rest der Funktion verschoben).  Die uebrigen Woerter sind gleich.  Schleifenformen (`do..while(--n)`, `do{..;n--;}while(n)`, `while(--n!=0)`,
  `unsigned`) aendern daran nichts.  Der Kernel enthaelt selbst zwei `DJNZ` (`283C`, `63C0`), den Peephole kennt der Kernel-Compiler also;
  welche Bedingung ihn dort ausloest/verhindert, ist **offen** (Hypothese: anderer Stand von `c2` als auf den Disketten, oder eine Schleifenform,
  die wir noch nicht getroffen haben -- die Disketten-`c0/c1/c2/cpp/as` sind in `usr`, `cc1`, `cc2` identisch, SHA-1 gleich).
* `3D42` `a+b+c > 256`: Kernel legt die Konstante 12 ueber `LDK R2,#12 ; LDB u_error,RL2` ab, `cc` erzeugt `LDB u_error,#12`.  Alle anderen Fehlernummern im
  Kernel (1, 6, 9, 24) gehen als Direktwert; hier steckt also noch etwas anderes dahinter (Quelle unbekannt).  Restliche Funktion gleich.

## Muster der Abweichungen (alle durch Quellform loesbar, ausser DJNZ)
1. **Registervariablen:** Reihenfolge der `register`-Deklarationen bestimmt `R13`, `R12`, ...; bei Parametern gilt die Reihenfolge der Parameterliste.
   Soll ein spaeter Parameter `R13` bekommen, steht er im Quelltext als lokale Registervariable, die zuerst aus dem Parameter geladen wird
   (`bcopy2`: `i = n; f = from; t = to;`).
2. **Bedingungsform:** `if (a && b && c) return(x); error; return(0)` statt `if (!(...)) { error; return }` -- bestimmt, welcher Zweig hinter dem
   `JR` liegt (`getf`, `chkmem`).
3. **Vorzeichen:** ein unbenutzter `register`-Parameter/-lokal erhaelt `csv`/`cret`, ohne ihn kollabiert `c2` eine leere Funktion zu `ret`
   (`nulldev` im Kernel: `register i;` unbenutzt); ein zweiter unbenutzter `int` lokal vergroessert den Rahmen (`DEC SP,#4`, `6ADA`).
4. **Shift:** `devtab[dev>>8]` mit `unsigned dev` liefert `SRL` + `SLL`; der Kernel hat `SRL` + `SLA` -- erreicht mit `devtab[(int)(dev >> 8)]`.
   `sll`/`sla` stehen im Assembler-Text, der Assembler kodiert sie verschieden.
5. **Strukturabstaende** muessen stimmen (INC statt ADD bei kleinen Abstaenden aendert die Laenge).  Der MUTOS-`cc` verlangt eindeutige Glied-
   und Tag-Namen (siehe `make/README.md`); in `pilot.c` tragen sie Vorsaetze.

## Compilerfrage
* Der MUTOS-`cc` der Disketten (`c0`/`c1`/`c2`/`cpp`/`as`) ist **dieselbe Familie** wie der Kernel-Compiler: Prolog/Epilog, Registerzuteilung
  (`R13` abwaerts), Adressierung, Optimierungen (`JR`-Auswahl, `LDK`, `INC/DEC #n`, `CLRB`, Konstantenfaltung) stimmen in 22 von 25 Faellen Byte fuer Byte.
* **Optionen:** `-O` (c2) ist noetig; ohne `-O` ist der Text um etwa die Haelfte laenger und in keinem Fall gleich.
* **Version:** wahrscheinlich nicht ganz dieselbe -- DJNZ-Unterschied und `LDK`-Fall deuten auf einen leicht anderen `c2`-Stand (oder auf Quellformen).
  Das ist erst mit mehr Funktionen mit Schleifen zu entscheiden; kein Hindernis fuer 90 % des Kernels.
* `as` kodiert unsere Ausgabe wie den Kernel (Relokation ausgenommen); der Kernel wird nicht als ein Objekt, sondern aus vielen `.o` gelinkt worden sein
  (Textabstaende: Funktionen liegen lueckenlos, gerade ausgerichtet).

## Empfehlung fuer den Rueckbau des ganzen Kernels
1. Reihenfolge: Blattfunktionen und kleine Hilfen zuerst (V7-Semantik bekannt, hohe Trefferquote), dann Puffer/Inode/Dateisystem (`bio`, `iget`, `alloc`,
   `nami`, `rdwri`), zuletzt Prozess/Speicher/Treiber/Trap.  Pro Quelldatei des V7-Kernels (`WEGA/src/uts/sys/*.c` als Vorlage) eine `.c`, in der
   Reihenfolge der Kerneladressen -- die Funktionen liegen dateiweise zusammen (`funktionen.txt`).
2. Gemeinsame Header (`struct user/proc/buf/inode/file`) zuerst aus den gemessenen Abstaenden und den V7/WEGA-Headern ableiten; **eine** Wahrheit,
   Glieder mit Vorsaetzen (V7-cc-Namensraum).
3. Ein Lauf kostet unter 2 Minuten: `gast_lauf.sh` mit vielen Funktionen pro Datei, `vergleich.py` je Marke; Varianten als Zusatzfunktionen mit
   derselben Marke (Name <= 7 Zeichen) im selben Lauf, nicht als neue Laeufe.  Danach nur Abweichler per `-v` ansehen.
4. Kernelfunktionen, deren Namen noch fehlen, tragen Platzhalternamen `fXXXX`; die Marke `/* KERNEL xxxx */` ist der Schluessel zum Vergleich.
5. DJNZ-Frage frueh mit 5-10 Schleifenfunktionen klaeren (Kernel-Schleifen sind der Rest-Unterschied); wenn es ein `c2`-Stand ist, genuegt
   notfalls ein Nachbessern des Objekts bzw. die Schleife in `asm` -- oder `c2` im Gast patchen.
6. Endziel: `ld` im Gast mit der Kernel-Objektreihenfolge, Vergleich des Gesamtabbilds (Rest: Daten, Textpool, Vektortabelle).

# AP K7a (2026-10-28): Header, DJNZ-Frage, erste Module

Dateien: `tools/mutos8000/kernel/c/h/*.h` (gemeinsame Header, Stand je Feld in `h/README.md`), `prf.c`, `slp.c`, `subr.c`, `rdwri.c`, `fio.c`,
`djnz.c`/`djnz3.c`/`varianten.c` (Versuche), `vergleich.py -m` (Modulvergleich), `gast_lauf.sh` (`NUR_O=1` nur `-O`, `KEIN_S=1` ohne `-S`, Header flach beigelegt).
Korrektur an K6b: `p_sig` ist EIN Wort (+06), `p_uid` +08, `p_pgrp` +0A; `NSIG` 17; `NCALL` 9 (callout CE58..CE8E); `nodev` setzt `ENODEV` (19);
`236C passc`/`23CA cpass` benutzen `subyte`/`fubyte`.

## Trefferliste (cc -O, je Funktion bytegleich gegen mutos_1216)
28 von 35 Funktionen bytegleich.
* `prf.c` 5/5: printf 1724, printn 17E0, panic 1856, prdev 187A, deverror 18A4.  Daten: Ziffern 8A4A, "panic: %s\n" 8A5B, "%s on dev %u/%u\n" 8A66, "err" 8A77, "bn=%D er=%o,%o\n" 8A7B; `panicstr` CEC4 (BSS).  `putchar` = 6384 (Treiberteil, nicht hier).
* `slp.c` 8/11: sleep, wakeup, setrq, setrun, setpri, sched, qswtch, expand gleich.  Offen: `swapin` 1CCA (8 Woerter, vermutl. Argumentreihenfolge von `swap`), `swtch` 1DA4 und `newproc` 1EA2 (Quellform noch nicht getroffen; Grundgeruest steht).  Daten: slpque CEC6, Texte 8A8E/8A99/8AA4/8AB8, mpid 8DD4, runq 8DC8, sflag 8DE0/8DE2.
* `fio.c` 8/8: getf, closef, openi, access, owner, suser, ufalloc, falloc.  Daten: file[] 9EEE..A07E, Text "no file\n" 8ADA.
* `subr.c` 5/7: passc, cpass, nodev, nulldev, timeout (2A4C) gleich.  Offen: `bmap` 20FE (Quellform weicht stark ab; Kernel kennt Indirektstufen mit Zaehler 13-j und `SDAL`), `bcopy` 243A (nur DJNZ, s. u.).  Daten: rablock 8DE4, callout CE58, Text 8AC2.
* `rdwri.c` 2/4: writei 25FE, min 2766 gleich.  Offen: `readi` 2460 (Kernel testet `diff` vor `bmap` und nutzt `breada`-Zweig; Reihenfolge des Blocks 24F2..2514 noch nicht getroffen), `iomove` 2780 (Variante `ioB` mit `do..while(--n)` bis auf 9 Woerter, davon 2 Woerter DJNZ-Effekt).

## DJNZ-Frage
Ergebnis (Versuche `djnz.c`, `djnz3.c`, `varianten.c`): der hiesige `c2` erzeugt `DJNZ` fuer fast jede `do..while(--n)`-/`while(n--)`-Schleife mit Registerzaehler (t1..t6, fh, fi, fn, `ioB`),
und `DEC Rn,#1 ; JR NZ` nur, wenn der Zaehler in einer Schleife steht, deren Rumpf mit einem Test auf `n` am Anfang beginnt (`while (n--)` mit Kopf-Test, `iR2/iR3`: `if/else` mit zwei Schleifen)
oder wenn Zaehler und Zeiger anders belegt sind (`cbA`/`cbD`: Kernel-`clrbuf` wird ueber `do *p++=0; while(--c);` mit `bp` ALS STACKPARAMETER bytegleich -- dort setzt c2 kein DJNZ).
Fuer `bcopy` (243A) fand keine der acht Formen (`do..while(--i)`, `while(--i>=0)`, `i--`/`!=0`, `for(;i!=0;i--)`) den Kernelcode; vermutlich anderer `c2`-Stand.
**Empfehlung (Plan B):** `bcopy` und `283C`-Verwandte als Handassembler-Modul oder mit manuell gepatchter `.s` (ein Befehl `DEC/JR NZ` statt `DJNZ`) belassen; betrifft 1 bis 2 Funktionen von 235.
`clrbuf` (5C86) ist mit Form `cbA` gelost.

## Offen / naechste Schritte
`swapin`, `swtch`, `newproc`, `bmap`, `readi`, `iomove`, `bcopy`.  Alle Datenanteile der Module sind in den Dateikoepfen mit Kerneladresse vermerkt (Eingabe fuer K5).


# AP K7b (2026-10-28): Dateisystem und Puffer -- iget, alloc, nami, pipe, bio, Reste von subr/rdwri

Dateien: `tools/mutos8000/kernel/c/{iget,alloc,nami,pipe,bio}.c`, `subr.c`/`rdwri.c` (bmap, readi, iomove nachgefuehrt), Versuche `k7bvar.c`, `k7bv2.c`;
Header `h/{buf,inode}.h` ergaenzt, neu `h/user_ext.h` (Felder von `u`, die `user.h` noch nicht benennt).  Quelle der Funktionskoerper: Disassemblat `kernel/s/*.s` plus V7/WEGA-Form.
**35 von 47 Funktionen bytegleich**, die uebrigen 12 sind "nahe" (ein bis wenige erklaerte Befehle, kein einziger Fall mit anderer Struktur).

## Trefferliste (cc -O, gegen mutos_1216)
Status: **gleich** = alle Woerter (ohne Relokationen); **nahe** = nur die genannte c2-Abweichung; offen = keine.
* `iget.c` 5/8: iget 1122, iexpand 126A, iput 12FE, maknode 1652, wdir 16C2 gleich.  **nahe:** iupdat 1372 (A), itrunc 14F6 (B), tloop 15B2 (B).
* `alloc.c` 4/7: free 3086, badblock 316A (intern, `static`, liegt im Bereich von free), ifree 335A, getfs 339E gleich.  **nahe:** alloc 2F48 (B), ialloc 319E (B, C), update 3400 (B).
* `nami.c` 1/1: namei 36A0 gleich (768 B).  `schar` gibt es im Kernel nicht; `uchar` 39BA steht hinter namei in `pipe.c`.
* `pipe.c` 8/8: uchar 39BA, pipe 39E2, readp 3A68, writep 3AFE, plock 3BBE, prele 3BE0, getc 3BFC, putc 3C80 gleich.  (K6b: `openp`/`closep` = V7 `readp`/`writep`; `getc`/`putc` sind die Zeichenlisten-Routinen, `cfreelist` D266.)
* `bio.c` 17/19: bread 57D4, breada 581A, bwrite 58F0, bdwrite 594A, bawrite 5984, brelse 599A, incore 5A22, getblk 5A64, geteblk 5B70, iowait 5BFC, notavail 5C2E, iodone 5C62, clrbuf 5C86, swap 5D4A, bflush 5F10, physio 5F56, geterror 60A8 gleich.
  **nahe:** mapalloc 5CAE und mapstart 5CF6 (D; Warteschlange `mapq` D57C mit 4 Eintraegen, Merker D584, Mailbox-Wort bei Adresse 6, Aufruf von 065E).  `binit` (07D2) liegt im Kernel in `main.c` (K7c).
* `subr.c`/`rdwri.c`: bmap 20FE nahe (B), readi 2460 nahe (E), iomove 2780 nahe (F), bcopy 243A nahe (F).

## Die Abweichungen sind alle Eigenheiten des Kernel-`c2`, nicht der Quelle
Der Kernel wurde mit einem **aelteren `c2`** (Peephole-Optimierer) gebaut als dem auf den Disketten: er kennt weniger Zusammenfassungen, nur DJNZ kennt er in bestimmten Faellen.
* **(A)** `ldb rl3,@r2 ; xorb rh3,rh3 ; testb rl3` (Kernel) statt `testb @r2`: iupdat `s_ronly`.  Versuche `k7bvar.c` (`||`, `!= 0`, Hilfsvariable) und `k7bv2.c` (vc1..vc4) liefern keine der beiden Formen anders.
* **(B)** Wert nicht weiterverwenden: Kernel `ldl rr4,@r2 ; ldl bn,rr4 ; testl bn(r14)` bzw. `push bn(r14)` statt `testl rr4` / `push r3` (itrunc, tloop, alloc `bno`, ialloc `ino`, bmap an drei Stellen, update `ld r2,fp(r14)`).  Zuweisung-in-Bedingung, `!x`, `x == 0` aendern daran nichts (`k7bv2.c` vb1..vb3).
* **(C)** ialloc: Anfangswert `adr = SUPERB+1` ergibt im Kernel `ldl rr2,#2`, hier immer `ldk r3,#2 ; exts rr2` (alle Schreibweisen va1..va7 gleich).
* **(D)** mapalloc/mapstart: Kernel `test @r2`, hier `ld r3,@r2 ; test r3` (Vorwaertsverwendung desselben Elements).
* **(E)** readi: Kernel hat ein ueberzaehliges `testl rr2` hinter `subl`, das `c2` hier weglaesst.
* **(F)** Schleifen: bcopy (Kernel `dec/jr ne`, hier `djnz`), iomove Schreibast (Kernel `dec/jr ne`, hier Kopftest-Form, mit `do..while(--n)` `djnz`), siehe K7a.
Folgerung fuer den Neubau (K5): die C-Quellen sind funktional und bis auf diese Stellen byteweise richtig; fuer ein bytegleiches Abbild bleibt Plan B (Nachbesserung der `.s` an den 12 Stellen oder altes `c2`).

## Gelernt (fuer weitere Module)
* Registerfolge: erst die `register`-Parameter in Reihenfolge, dann lokale `register`; automatische Variablen stehen mit der **zuerst deklarierten am hoechsten Stapelplatz** (`iget`: ip, mp, oip, bp, dp = R13..R9).
* `(bp->b_flags & B_ERROR) != 0` ergibt `ld/and/jr`, `if (bp->b_flags & B_ERROR)` ergibt `bit` -- iget verlangt die erste, alle anderen Stellen die zweite Form (alloc: `== 0`).
* `while (plock(ip), ip->i_size == 0) {...}` legt den Schleifenkopf **hinter** den Sprung ueber den Rumpf (readp); `plock` am Ende des Rumpfes ergibt eine andere Blockfolge.
* Wortbreite von `(unsigned)up + 0xFF >> 8` entscheidet `srl`/`sra` (swap).  Lange Argumente brauchen die Umwandlung `(daddr_t)` an der Aufrufstelle (update: `getblk(dev, (daddr_t)SUPERB)` = `ldk;exts;pushl`).
* Mehrere Gastlaeufe laufen parallel (eigener Arbeitsordner je Lauf); je Lauf Minuten statt der frueher geschaetzten zehn.
* Buffer: `b_xmem` +14, `b_error` **+15** (K7a: +14 falsch), `B_TAPE` 02000; Inode: `NADDR` 13, `PIPSIZ` 4096.  `u+156` = umask, `u+113` = Byte `u_sep`, `u+104` Zeiger (bei K7c `u_ar0`).

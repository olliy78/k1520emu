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

# AP K7c (2026-10-28): main, trap, sig, sys1-4, text, machdep, malloc, ureg, slp-Rest

Dateien: `tools/mutos8000/kernel/c/{main,malloc,machdep,text,ureg,sig,trap,sys1,sys2,sys3,sys4}.c`, `slp.c` ergaenzt, `var7c.c` (Varianten),
Header `h/user.h` (ab 104h aufgeloest), `h/param.h` (NPROC 30, MAXUPRC 15).  Stand: **82 von 97 Funktionen bytegleich** (cc -O, `vergleich.py`).

## Trefferliste (Funktion: Status)
* `text.c` 10/10: xswap 562C (K6b: xalloc), xfree, xalloc-Stumpf 5790, 579C, xlock, xunlock, xccdec, xumount, xrele, 57CC.
* `sys3.c` 8/8: fstat, stat, stat1, dup, smount, sumount, getmdev, exec.
* `sys4.c` 22/24: gtime, ftime, stime, setuid, getuid, setgid, getgid, getpid, sync, nice, unlink, chdir, chroot, chmod, chown, kill, times, profil,
  alarm (5100, nicht nice), pause, umask, utime.  Offen: `chdirec` 4EB0 (Layout der Fehlerbloecke), `ssig` 4FB0 (Kernel laedt `uap` nach dem Speichern
  nochmals vom Stack, `c2` hier nicht).
* `sys2.c` 10/11: read, write, open, creat, copen, close, seek, link, mknod, saccess.  Offen: `rdwr` 51D6 (Kernel prueft FPIPE mit AND, hier BITB; Variante rdwrA).
* `sig.c` 8/10: signal, psignal, issig, stop, psig, fsig, grow, ptrace.  `core` 0D56: 2 Woerter (Rest des Layouts); `procxmt` 0F12 (K6b: core): 67 Woerter,
  Fallverteilung/Registerzuteilung offen.
* `sys1.c` 6/8: setregs, rexit, exit, wait, fork, sbreak.  Offen: `exece` 41AC (201 von 383 Woertern), `getxfile` 44B6 (177 von 401): die zwei Kopfformate
  (Magic E807 klein/16 B, E707 gross/40 B) und der Text-busy-Block liegen im Kernel in anderer Anordnung; Locals-Reihenfolge ist angepasst (zuerst deklariert = hoechste Adresse).
* `slp.c` 9/11: swapin jetzt gleich (Deklarationsreihenfolge xp, a, t, x).  Offen: `swtch` 1DA4, `newproc` 1EA2.
* `main.c` 2/3: main, iinit gleich; `binit` 1 Wort (b_error liegt im Objekt bei +15 statt +14; Variante mit `b_pad15` uebersetzt nicht, Feld ist nicht lesbar).
* `trap.c` 2/3: nosys, nullsys gleich; `trap` 087A 37 von 343 Woertern (Rahmen, Fallverteilung ueber 10..31 stimmen; Rest: Layout der Faelle 10/11/15).
* `machdep.c` 3/4: kstub 35AC, sendsig, sigret gleich; `startup` 34F6 noch 45 Woerter (Konstante 0xFCFF).
* `ureg.c` 2/3: cinit, sureg gleich; `estabur` 3D42 (LDK-Form fuer ENOMEM nicht getroffen, Varianten in var7c.c).
* `malloc.c` 0/2: `mfree` bis auf 1 Wort; `malloc` 24 Woerter (der Kernel hat nach `m_size -= size` ein `test r2`).

## Befunde
* **Nutzerraumzugriffe vertauscht in der Karte**: 04CC liest ein Byte (fubyte), 04DA ein Wort (fuword), 0508 schreibt ein Byte (subyte), 0514 ein Wort (suword).
* 3630 ist `sigret` (Rueckkehr aus dem Signal, sysent 0), nicht `backup`; 0F12 ist `procxmt`, `core` ist 0D56; 5100 `alarm`.
* Prozess-Layout: `u_ar0` (u+104) zeigt auf den Trap-Rahmen (ar0[8] = Nutzer-SP, [10] FCW, [11] Segment-PC, [12] PC); 256-Byte-Klicks, Prozess = 64-KB-Segment.
* Compilerregel: zuerst deklarierte Stack-Locals liegen an der hoechsten Adresse; `<<` auf unsigned liefert SLL, auf int SLA.

## Handassembler (nicht in C)
csv/cret, savu 0614, resume 0620, spl0..spl7/splx, fubyte/fuword/subyte/suword, copyin/copyout/copyseg/clearseg, savfp 03F8/restfp 03FE, addupc 0414, idle, mapaddr 065E,
Trap-Eintritt 030E, start 027E, 6432 (Geraeteinit, dev1).

# AP K7d (2026-10-28): Treiber und Konfiguration -- rk, mem, /dev/tty, tty, lp, Major 0 (fd), conf

Dateien: `tools/mutos8000/kernel/c/{rk,tty,tty2,lp,dev0,conf}.c`, Varianten `var7d.c`, `var7d2.c`, Header `h/mbx.h` (neu), `h/tty.h` (jetzt gemessen), `h/user_ext.h` (U_TTYP/U_TTYD),
neues Werkzeug `werkzeug/datenvergleich.py` (Datenteil eines `.o` gegen den Kernel, Relokationen als Wildcard) und `werkzeug/uebersicht.py` (Gesamtuebersicht unten).
Der Kernel-Quellcode steckt hier wieder in einer Tabelle der Daten: **conf.c ist als C-Datei mit initialisierten Tabellen bytegleich** (`datenvergleich.py KERNEL conf.o 8766`: 352 Byte, 0 abweichende Woerter, 130 Adressfelder als Wildcard).

## Trefferliste (cc -O, je Funktion bytegleich gegen mutos_1216; Stand nach drei Gastlaeufen)
* `rk.c` **13/13**: mmread 69CE, mmwrite 6A52 (cdevsw[2], /dev/mem, /dev/kmem, /dev/null: Segment ueber `usrseg` 8DC0, Minor 1 = Kernelraum, 2 = null), rkopen 6ADA, rkclose 6AFE, rkstrategy 6B06, rkstart 6B60, rkintr 6BCA,
  rkread 6C52, rkwrite 6C70, syopen 6C8E, syread 6CC6, sywrite 6CEC, syioctl 6D12 (/dev/tty = cdevsw[1], V7 sys.c: leitet an `cdevsw[major(u_ttyd)]` weiter).
  rkstart/rkintr/rkstrategy sind V7-`rk.c` mit der Mailbox statt der Controllerregister: Auftrag in D532 ([Befehl 0, Einheit, Richtung, Status | Segment, Adresse, Zaehler, Blocknummer]), Start ueber `mapalloc(mp, 0)` (5CAE),
  Ende ueber `rkintr` (Status 1 = ok, C8 = Einheit fehlt -> ENXIO, sonst `deverror` und bis zu drei Wiederholungen, Fehlerzaehler in `rktab.b_resid` 8E98).  `rktab` 8E82 und `fdtab` 8E6A sind `struct buf`-Koepfe
  (b_bcount = "Auftrag aktiv").  Blockgrenze 0x260 (608 Bloecke = 76 Spuren zu 8 Bloecken, passt zur Geraetegroesse).
* `tty.c` **11/11** (60CC..6432): tcopen 60CC, tcclose 616C, tcread 618E, tcwrite 61B0, tcxint 61D2, tcrint 620E, tcioctl 623C, ttstart 6274, ttxfer 633A, putchar 6384, ttyinit 6432.
  Namen der Wrapper frei (`tc...`), Kernfunktionen nach V7/System III.  Die Konsole `putchar` fuehrt einen Ring (8DE8..8E68, Zeiger 8C96) und wartet bis zu 10000 Schleifen auf die Leitung 0.
  **DJNZ-Fund:** die Wartschleife wird nur mit `do { if (hp->mb_stat) break; } while (--t != 0);` zum Kernel-`DJNZ` (mit `while (--t)` oder `for` entsteht `DEC/JR NZ`) -- die Formen sind in `var7d2.c` (pcA..pcE).
* `tty2.c` **11/13** (6D44..7E16 ohne Eingabe-/Ausgabeweg): ttyopen 6D44, ttinit 6D8C, ttyclose 6DD0, ioctl 6DF0 (Systemaufruf 54: FIOCLEX 6601/FIONCLEX 6602, sonst `cdevsw[major].d_ioctl`), ttywait 715C, ttyflush 71A4,
  bitset 7948, ttflow 79E0, ttrstrt 7C4A (Zeitgeberfunktion, die `ttstart` an `timeout` uebergibt), ttkick 7C62, ttwrite 7DA6 gleich.  **nahe:** ttyspec 797A (18 von 51 Woertern, Blockreihenfolge der Endabfragen `0x12/0x17/0x10`),
  ttread 7C90 (Eigenheit des Kernel-c2, siehe unten: der frueh liegende `return(0)` springt mit `JR` auf das letzte `JP cret`, hier `JP cret` -- ein Wort mehr, danach ist alles verschoben).
* `lp.c` **4/7**: lpopen 6444, lpclose 64DA, lpoutput 680E, lpxfer 690A gleich.  **offen:** lpwrite 6570 (42 von 124 Woertern), lpcanon 6668 (104 von 212), lpintr 6874 (21 von 75) -- alle drei nur in der Blockanordnung:
  der Kernel-`c2` legt hinter `JP cret` liegende Fortsetzungsbloecke ans Funktionsende und fuehrt gleiche Endstuecke (`INC R15,#2 ; JP cret`) zusammen, unser `c2` tut das hier nicht (derselbe Befund wie bei K7b "A..F").
  Struktur der Funktionen (V7-`lp.c`, Mailbox-Befehl 2, Zeilenpuffer 128 Byte, Drucker 0/1, Merker 0x80 = Ausgabe laeuft, 0x40 = Schlaf) ist belegt: die Schalterliste von lpcanon (`{ } ` | ~` Reihenfolge der Faelle = Quellreihenfolge, die Verzweigungskette steht aufsteigend sortiert) stimmt.
* `dev0.c` (Major 0, cdevsw[10], nie gelaufen) **4/7 gleich, 3 nahe**: fdsub 7F00, fdstrategy 7F94, fdread 84E4, fdwrite 851C gleich; fdopen 7E16, fdclose 7E94, fdphys 8554 nahe.
  **Dieses Modul ist im Kernel ohne `-O` uebersetzt** (alle Spruenge `JP`, der Rumpf hinter einem `JP` am Funktionsanfang); verglichen wird `dev0.o`, nicht `dev0O.o`.  Der aeltere Compiler vergleicht Registervariablen mit Konstanten ueber `R2`
  (`LD R2,R13 ; CP R2,#1` statt `CP R13,#1`) -- das ist die einzige Abweichung der drei "nahen" Funktionen.  **Nicht zurueckgebaut:** fdstart 7FDC (574 B, Zustandsautomat ueber `fdtab`+C = 8E76 mit Mailbox-Befehl 6, Seek) und fdioctl 85B0 (426 B, Sprungtabelle).
  Art des Geraets: die Mailbox-Befehle 1/6, `fdopen`s Anlage und die Abschlussbefehle 0x51/0x61 in `fdclose` deuten auf ein Diskettenlaufwerk der U880-Seite (Befund 39 §11); in dieser Version nie erreicht.
* `conf.c` Daten bytegleich: bdevsw (Major 0 = fd, 1..3 `nodev`, 4 = rk, Abschluss), cdevsw (14 Eintraege + Abschluss), `linesw` (ttyopen, nulldev, ttread, ttwrite, nodev, ttyinput, ttkick), `rootdev/swapdev/pipedev` 0402, `swplo` 250, `nswap` 244,
  die 22 Adressen der ps/pstat-Tabelle (888C..88B7, davor ein Nullwort 888A), 6 Nullworte, `proctop` 8A8C bei 88C4.  **Korrektur an K6b:** der Drucker ist `cdevsw[5]` (nicht 7), Major 13 = rk roh, Major 10 = fd roh, `cdevsw[0]` = tty ueber `tc...`.
  Nicht in conf.c: sysent 8B58 (63 Eintraege) -- bleibt ein eigenes Datenstueck; die Datenreihenfolge im Abbild haengt an der Linkreihenfolge (K5).

## Neue Befunde
* **Die tty-Struktur ist System III/ZEUS, nicht V7:** EINE Eingabewarteschlange `t_inq` (die kanonische Verarbeitung geschieht im Eingabeweg `ttyinput`, `t_delct` zaehlt Zeilenenden, Trenner ist 0xFF im Zeichenstrom), Zeichenfolge `t_intrc..t_brkc` (03, 1C, 11, 13, 1A, FF),
  Vorgabe-Flags 0518H.  Layout in `h/tty.h`.
* **Mailbox je Leitung/Drucker ist dieselbe 14-Byte-Struktur wie die der Platte** (`h/mbx.h`); `mb_stat` wirkt bei tty/lp als "Leitung frei"-Merker.
* **Handassembler/nicht C-erreichbar in diesem AP: keiner.**  Alle Treiberfunktionen haben `call csv` (kein OUT/IN mit festem Port im Treibercode -- der Verkehr laeuft vollstaendig ueber die Mailbox und `mapalloc`/`mapstart` in bio.c;
  die Eintritte der Geraeteinterrupts liegen im Trap-Eintritt 030E, Assembler aus K7c).
* **Gast-`cpp`:** Makronamen sind nur auf 8 Zeichen signifikant (`USER_EXT_K7D` == `USER_EXT_H`) -- Include-Waechter kurz und verschieden waehlen.  Ein Name darf in Initialisierern erst nach einer Deklaration (`int f();`) vorkommen (conf.c).

## Offen aus K7d
`ttioctl` 6E7A (738 B, Sprungtabelle ueber cmd/6 und die Befehlstabelle 8D3A..), `ttyinput` 7222 (912 B) mit den Teilfunktionen 75B2, 769A, 772E, 78A8 und `ttyoutput` 7A34 (534 B, Sprungtabelle 8D46, Tabellen 8D1A/8D52) -- alle in System-III-Form, noch ohne Zeichenvorlage;
`fdstart` 7FDC, `fdioctl` 85B0.  Dazu die Blockanordnung von lpwrite/lpcanon/lpintr/ttread/ttyspec (Kernel-c2).

# AP K7e (2026-10-28): restliche Funktionen in C, sysent

Dateien: neu `c/clock.c`, `c/sysent.c`; erweitert `tty2.c` (ttioctl, ttyinput samt ttycol/ttyecho/ttyrub/unputc, ttyoutput, Tabellen), `dev0.c` (fdstart, fdintr 821A, fdadj 8468, fdioctl), `alloc.c` (badblock mit Marke);
Header `h/tty.h` (Glieder 1C..21, 26, 2D, 34 benannt, Flag-Makros), `h/conf.h` (`struct linesw`, 20 Byte), `h/systm.h` (`sy_narg` char).  Ein Gastlauf (r1: clock, tty2, alloc, sig, sysent, conf mit -O; r2: dev0 ohne -O).
* **Daten bytegleich:** `sysent.c` (8B58, 64 Eintraege, `datenvergleich.py`: 0 abweichende Woerter) und weiterhin `conf.c` (nach Umzug von `linesw` nach `conf.h` mit den drei Nullworten, 0 Abweichungen).  Die Eintraege 64/65 hinter der Tabelle sind Text ("tm..."), kein sysent.
* **gleich:** `badblock` 316A (Marke gesetzt; `alloc.c` 4/7), `unputc` 78A8, `fdadj` 8468.  **nahe:** `ttycol` 75B2 (1 Wort).  Weitere Stufen siehe Uebersicht.
* **Zurueckgebaut, aber nicht bytegleich:** `clock` 2842 (die Form stimmt; Kernel ruft am Anfang ein Hilfsstueck 040C, `lbolt` wird ueber R2 verglichen -- Eigenheit des aelteren c2), `ttioctl` 6E7A (der Kernel-Compiler legt die
  switch-Verteilung ueber `cmd/6` und eine Sprungtabelle 8D3A an, unser cc nicht), `ttyinput` 7222, `ttyecho` 769A, `ttyrub` 772E, `ttyoutput` 7A34 (Sprungtabelle 8D46; Fallverteilung im Kernel ueber `c-8`, in `c` hier anders gebaut),
  `fdstart`, `fdintr`, `fdioctl` (Mailbox-Zustandsautomat; ohne -O uebersetzt, Abweichungen vor allem Registerzuteilung und Konstanten-Vergleiche ueber R2).  Funktional nach System III/Mailbox belegt, Namen frei.
* **`procxmt` 0F12:** schon aus K7c in `sig.c`, Status offen (kein Zusatzlauf).
* **Gleiche Kernelbefunde:** ttyoutput/ttyrub nutzen `t_dtab` (1E) als Tabulatorspalte bei XTABS; `t_width` (20) 0 = 255; Seitenbremse ueber `t_page` (21) und `t_cnt2c`.  `fdintr` hat keinen Aufrufer in der Aufrufkarte (Interrupt) -- fehlt in `funktionen.txt`, wird von `uebersicht.py` nicht gezaehlt.
* **Nicht erreicht (Zeit):** exece/getxfile (nur `swplo`-Typ: `(daddr_t)bno` ergaenzt, ungeprueft), trap, malloc, newproc/swtch, readi, bmap: keine neuen Quellformen versucht.

# GESAMTUEBERSICHT C-Rueckbau (Stand 2026-10-28, nach K7a-K7e)

Quelle: `werkzeug/uebersicht.py KERNEL <Objektverzeichnisse> --md` (Universum `kernel/lauf/funktionen.txt`, Modulgrenzen `linkkarte.txt`, Status je Funktion; Schwelle "nahe" = hoechstens 30 abweichende Woerter, ein verschobenes Wort
zaehlt alle folgenden mit).  Status: **gleich** = alle Woerter wie im Kernel; **nahe** = Eigenheit des aelteren Kernel-`c2`/`c1` (siehe K7b "A..F", K7d); **offen** = in C vorhanden, aber weiter entfernt; **kein C** = noch nicht zurueckgebaut;
**Assembler** = Funktion ohne `call csv` (mch.s-Art), bleibt Assembler.

| Modul | gleich | nahe | offen | kein C | Assembler | Summe |
|-------|-------:|-----:|------:|-------:|----------:|------:|
| low | 0 | 0 | 0 | 0 | 26 | 26 |
| main | 2 | 1 | 0 | 0 | 0 | 3 |
| trap | 0 | 0 | 1 | 0 | 0 | 1 |
| sig | 7 | 0 | 1 | 0 | 0 | 8 |
| iget | 5 | 1 | 2 | 0 | 0 | 8 |
| prf | 5 | 0 | 0 | 0 | 0 | 5 |
| slp | 9 | 0 | 2 | 0 | 0 | 11 |
| subr | 2 | 1 | 1 | 0 | 0 | 4 |
| rdwri | 2 | 1 | 1 | 0 | 0 | 4 |
| clock | 1 | 0 | 1 | 0 | 0 | 2 |
| fio | 8 | 0 | 0 | 0 | 0 | 8 |
| malloc | 0 | 1 | 1 | 0 | 0 | 2 |
| alloc | 4 | 0 | 3 | 0 | 0 | 7 |
| machdep | 3 | 0 | 1 | 0 | 0 | 4 |
| nami | 1 | 0 | 0 | 0 | 0 | 1 |
| pipe | 8 | 0 | 0 | 0 | 0 | 8 |
| sys3 | 10 | 1 | 0 | 0 | 0 | 11 |
| sys1 | 6 | 0 | 2 | 0 | 0 | 8 |
| sys4 | 9 | 0 | 0 | 0 | 0 | 9 |
| sys2 | 16 | 1 | 2 | 0 | 0 | 19 |
| text | 9 | 0 | 0 | 0 | 0 | 9 |
| bio | 17 | 2 | 0 | 0 | 0 | 19 |
| tty | 10 | 0 | 0 | 0 | 0 | 10 |
| dev1 | 5 | 0 | 2 | 0 | 0 | 7 |
| rk | 13 | 0 | 0 | 0 | 0 | 13 |
| mbx | 11 | 3 | 5 | 0 | 0 | 19 |
| dev0 | 4 | 2 | 3 | 0 | 0 | 9 |
| **Summe** | 167 | 14 | 28 | 0 | 26 | 235 |

Zu den Modulnamen: Grenzen sind die Linkreihenfolge-Hypothese aus K5 (`linkkarte.txt`); `mbx` ist darin der Block ttyopen..ttwrite (6D44..7E16), `dev1` Konsole-Rest + Drucker, `low` der gesamte Handassembler (26 Funktionen:
Start 027E, Trap-Eintritt 030E, spl*, fubyte/fuword/subyte/suword, copyin/copyout/copyseg/clearseg, savu/resume, savfp/restfp, mapaddr, addupc ...).
**Fuer einen Gesamtbau fehlen noch** (kein C): `clock` 2842 (522 B) und `badblock` 316A (liegt in alloc.c ohne eigene Marke), `procxmt` 0F12 (524 B), `ttioctl`/`ttyinput`+4 Teilfunktionen/`ttyoutput`, `fdstart`/`fdioctl`;
dazu die **Datenstuecke** (sysent 8B58, Zeichenketten 88C6..8B51, Zeichenkettenreste der Module, BSS-Zuteilung 8DC0..D586) und der **Assembleranteil** `low` (aus K5/K4 als `.s`).
Die **offenen** Funktionen (20) und **nahen** (12) sind funktional gleich, nicht bytegleich: fuer ein bytegleiches Abbild bleibt Plan B (Nachbesserung der `.s` an den benannten Stellen oder ein `c2` im Gast, der Blockreihenfolge und Endstueckzusammenfuehrung des Kernel-Compilers nachbildet).

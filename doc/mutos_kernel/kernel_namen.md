# Kernel `mutos_1216`: Namen, Strukturen, Tabellen (AP K6b, 2026-10-27)

Abbild 36 352 B, nichtsegmentiert, Ladeadresse 0; Grundlage `kernel_struktur.md` (K6a), Funktionsliste `tools/mutos8000/kernel/lauf/funktionen.txt`,
Karte `tools/mutos8000/kernel/lauf/karte_basis.txt`.  Referenz: WEGA-`uts` (`OlliL/P8000`, `WEGA/src/uts/{sys,dev,conf}`, `WEGA/src/head/sys/*.h`), nur als Namens- und
Reihenfolgegeber; WEGA ist mit einem anderen Compiler gebaut, byteweise Gleichheit gibt es nicht.  Alles, was nicht aus dem Abbild folgt, ist als Vermutung gekennzeichnet.

## 0. Methode und Sicherheitsstufen

Belege (Spalte "Beleg"):
* **Reihenfolge**: die C-Funktionen liegen modulweise in Linkreihenfolge, und die Funktionsfolge stimmt mit WEGA (`sys/*.c`) ueberein; wo WEGA eine Funktion mehr hat, fehlt sie auch im Abbild nur, wenn ein Stub dort steht.
* **Text**: Zeichenketten (`panic: %s`, `Timeout table overflow`, ...) im Textpool, Verweise aus `textpool.txt`.
* **Aufrufgraph**: Aufrufer-/Angerufenenlisten in `funktionen.txt` (z. B. `sleep` ruft `spl6`/`spl0`/`issig`, `wait` ruft `fsig`).
* **Tabelle**: `sysent` (Anzahl Argumentworte, Reihenfolge = WEGA/V7-Nummern), `bdevsw`/`cdevsw`.
* **Lauf**: Disassemblat der laufenden Befehle (z. B. `sleep`: u.u_procp = `FC1E`, `p_wchan` bei +0x14, Hashkette `CEC6`); ein Speicherabzug am laufenden Kernel (`k1520dbg --em em256`, Skript `tools/mutos8000/versuche/kernel_namen_abzug.dbg`) bestaetigt `proc[0]` (Abschnitt 3).

Stufen: **sicher** = Text, Tabelleneintrag oder eindeutiges Verhalten (Konstanten, Offsets) am Code; **wahrscheinlich** = Reihenfolge + Aufrufgraph stimmen, aber kein Text/keine Tabelle;
**vermutet** = nur Lage/Groesse, hier steht der Name nur im Dokument (nicht in der Karte).  In die Karte (`name`-Zeilen) gehen nur sicher und wahrscheinlich, mit fuehrendem Unterstrich (`_sleep`).

Zahlen: sicher 108, wahrscheinlich 87, vermutet 52 (zusammen 247 Eintraege: 235 Funktionsanfaenge aus `funktionen.txt` plus 11 nur ueber Tabellen erreichbare).

## 1. Funktionen

Spalten: Adresse, Laenge in Byte laut `funktionen.txt` (`-` = nicht dort gelistet, Einstieg nur ueber Tabelle, Code im Lauf nie erreicht), Name, Modul (WEGA-Datei), Beleg, Sicherheit.
Modulnamen ohne Endung `.c`: `mch.s`, `clist.s` sind Assembler.  `dev/...` = Treiber (WEGA-Verzeichnis `dev`); fuer MUTOS-eigene Treiber (`rk`, Mailbox-Terminal) gibt es keine WEGA-Quelle.

| Adresse | Laenge | Name | Modul | Beleg | Sicherheit |
|---------|--------|------|-------|-------|------------|
| 027E | 144 | `start` | mch.s | Reset-Eintrag PSA | sicher |
| 030E | 234 | `trapent` | mch.s | Trap-Eintritt der PSA-Eintraege 1-6 (FCW 40xx), ruft trap 087A | wahrscheinlich |
| 03F8 | 6 | `parchk1` | mch.s | TEST %D57A + RET, 3 Aufrufer (Paritaets-/Fehlerlatch) [Deutung offen] | vermutet |
| 03FE | 14 | `parchk2` | mch.s | wie 03F8 | vermutet |
| 040C | 2 | `nullret` | mch.s | nur RET | vermutet |
| 0414 | 96 | `addupc` | mch.s | (pc-base)*scale>>14 mit Zugriff auf Nutzerspeicher; Aufrufer aclock-Pfad, WEGA trap.c aprof | wahrscheinlich |
| 0474 | 12 | `splx` | mch.s | Arg -> OUT 0080 (Prioritaet); sleep: s=spl6(); ... splx(s) | sicher |
| 0480 | 4 | `spl7` | mch.s | LDK R1,#7 -> OUT 0080 | sicher |
| 0484 | 4 | `spl6` | mch.s | LDK R1,#6; liefert alte Prioritaet (sleep) | sicher |
| 0488 | 4 | `spl5` | mch.s | LDK R1,#5 | sicher |
| 048C | 4 | `spl4` | mch.s | LDK R1,#4 | sicher |
| 0498 | 26 | `spl1` | mch.s | LDK R1,#1 | wahrscheinlich |
| 04B2 | 26 | `spl0` | mch.s | XOR R1,R1 -> OUT 0080 (sleep: spl0()) | sicher |
| 04CC | 14 | `fuword` | mch.s | Wort aus Nutzerraum (Segment L8DC0), Rueckgabe R2 | wahrscheinlich |
| 04DA | 46 | `fubyte` | mch.s | wie fuword, Byte (RES R5,#0) | wahrscheinlich |
| 0508 | 12 | `suword` | mch.s | Wort in Nutzerraum schreiben | wahrscheinlich |
| 0514 | 66 | `subyte` | mch.s | Byte in Nutzerraum schreiben | wahrscheinlich |
| 0556 | 10 | `copyin` | mch.s | LDIR Nutzer->Kern; ioctl/tty benutzen es fuer Argumente | sicher |
| 0560 | 66 | `copyout` | mch.s | LDIR Kern->Nutzer; ftime 4C24 liefert damit seine Struktur | sicher |
| 05A2 | 36 | `copyseg` | mch.s | 256 B Segment->Segment (EXB-Segmentbyte, LDIR 0x80 Worte) | wahrscheinlich |
| 05C6 | 36 | `clearseg` | mch.s | 0x80 Worte nullen (LD @R4,#0 + LDIR) | wahrscheinlich |
| 05EA | 14 | `idle` | mch.s | spl0; EI; HALT; splx | wahrscheinlich |
| 05F8 | 28 | `csv` | mch.s | Compiler-Prolog (182 Aufrufer) | sicher |
| 0606 | - | `cret` | mch.s | Compiler-Epilog | sicher |
| 0614 | 12 | `savu` | mch.s | Kontext sichern (5 Aufrufer: swtch/newproc/sleep) [Deutung ueber V7] | vermutet |
| 0620 | 62 | `resume` | mch.s | Kontext wechseln: sleep/swtch rufen mit (u-Adresse, *p_addr); setzt spl7/spl0 | vermutet |
| 065E | 22 | `mapaddr` | mch.s | 22 B, einziger Aufrufer 5CF6 | vermutet |
| 0674 | 210 | `main` | main.c | ruft iinit, binit, iget(2), newproc, sched, cinit, estabur | wahrscheinlich |
| 0746 | 140 | `iinit` | main.c | Text "iinit" (panic), bread/getfs | sicher |
| 07D2 | 168 | `binit` | main.c | Pufferliste aufbauen (ruft brelse 599A) | wahrscheinlich |
| 087A | 688 | `trap` | trap.c | Texte "trap type %x", "ident = %x", "seg = %x", "aps = %x" | sicher |
| 0B2A | - | `nosys` | sys4.c | sysent-Platzhalter (u_error=EINVAL) | sicher |
| 0B38 | - | `nullsys` | sys4.c | sysent-Platzhalter ohne Fehler (0, 38, 39) | sicher |
| 0B3A | 52 | `signal` | sig.c | ruft psignal 0B6E (WEGA: signal(pgrp,sig)) | wahrscheinlich |
| 0B6E | 90 | `psignal` | sig.c | setrun-Aufruf wenn SSLEEP (WEGA psignal) | sicher |
| 0BC8 | 80 | `issig` | sig.c | von sleep als Bedingung benutzt: if (p_pri>PZERO && issig()) | sicher |
| 0C18 | 100 | `stop` | sig.c | wakeup(parent), swtch, exit(fsig) wie WEGA stop() | wahrscheinlich |
| 0C7C | 176 | `psig` | sig.c | ruft sendsig 35AE, stop/exit | wahrscheinlich |
| 0D2C | 42 | `fsig` | sig.c | 4 Aufrufer (issig, stop, psig, wait); naechstes Signal der Maske | wahrscheinlich |
| 0E38 | 10 | `grow` | sig.c | liefert 0 (nichtsegmentiert), einziger Aufrufer sendsig | vermutet |
| 0E42 | - | `ptrace` | sig.c | sysent 26 | sicher |
| 0F12 | 524 | `core` | sig.c | Text "core", schreibt Abbild (524 B) | sicher |
| 1122 | 328 | `iget` | iget.c | Texte "no imt", "Inode table overflow"; Tabelle 907A, 0x4A B je Inode | sicher |
| 126A | 148 | `iexpand` | iget.c | Platteninode -> Kerninode (Blockadressen 3 B -> long), 148 B Blattfunktion | wahrscheinlich |
| 12FE | 116 | `iput` | iget.c | 31 Aufrufer, ruft iupdat/itrunc/pipe-Freigabe | sicher |
| 1372 | 388 | `iupdat` | iget.c | Text "iaddress > 2^24" | sicher |
| 14F6 | 188 | `itrunc` | iget.c | ruft tloop 15B2 und free 3086 | wahrscheinlich |
| 15B2 | 160 | `tloop` | iget.c | rekursiv (indirekte Bloecke), bread+free | wahrscheinlich |
| 1652 | 112 | `maknode` | iget.c | ialloc 319E + wdir 16C2 | wahrscheinlich |
| 16C2 | 98 | `wdir` | iget.c | Verzeichniseintrag schreiben: iput, writei 25FE | wahrscheinlich |
| 1724 | 188 | `printf` | prf.c | Formatausgabe, Text-Aufrufer ueberall (printfv in WEGA) | sicher |
| 17E0 | 118 | `printn` | prf.c | Text "0123456789ABCDEF" | sicher |
| 1856 | 36 | `panic` | prf.c | Text "panic: %s\n"; 14 Aufrufer; endet in idle/Schleife | sicher |
| 187A | 42 | `prdev` | prf.c | Text "%s on dev %u/%u\n" | sicher |
| 18A4 | 48 | `deverror` | prf.c | Text "bn=%D er=%o,%o\n"; rkintr ruft es | sicher |
| 18D4 | 212 | `sleep` | slp.c | Lauf: u_procp=FC1E, p_wchan 0x14, p_link 0x18, Hash slpque CEC6[(chan>>5)&63]; spl6/spl0/issig-Muster | sicher |
| 19A8 | 136 | `wakeup` | slp.c | 30 Aufrufer; durchlaeuft slpque, ruft setrun/setrq | sicher |
| 1A30 | 76 | `setrq` | slp.c | Text "proc on q" | sicher |
| 1A7C | 108 | `setrun` | slp.c | Text "Running a dead proc" | sicher |
| 1AE8 | 76 | `setpri` | slp.c | Prioritaetsberechnung (V7) | wahrscheinlich |
| 1B34 | 406 | `sched` | slp.c | ruft swapin 1CCA, sleep, xalloc | wahrscheinlich |
| 1CCA | 196 | `swapin` | slp.c | swap-Einlesen, malloc/mfree | wahrscheinlich |
| 1D8E | 22 | `qswtch` | slp.c | 22 B: setrq + swtch | wahrscheinlich |
| 1DA4 | 254 | `swtch` | slp.c | Prozesswechsel: resume/spl; Aufrufer sleep, exit, psig | sicher |
| 1EA2 | 596 | `newproc` | slp.c | Text "no procs"; kopiert proc/u, ruft malloc | sicher |
| 20F6 | 8 | `expand` | slp.c | nur csv+cret (Stub; nichtsegmentiert) | vermutet |
| 20FE | 622 | `bmap` | subr.c | ruft alloc 2F48, bread, bdwrite | wahrscheinlich |
| 236C | 94 | `passc` | subr.c | Byte an Nutzer (ruft suword-Gruppe 0508); von tty-read/ readi | wahrscheinlich |
| 23CA | 112 | `cpass` | subr.c | Byte vom Nutzer (04CC-Gruppe) | wahrscheinlich |
| 242A | - | `nodev` | subr.c | Tabelleneintrag "nodev" in bdevsw/cdevsw (ID: u_error=ENXIO) | sicher |
| 2438 | - | `nulldev` | subr.c | leerer Eintrag cdevsw ioctl-Spalte | sicher |
| 243A | 38 | `bcopy` | subr.c | Byte-Kopierschleife (src,dst,n) | wahrscheinlich |
| 2460 | 414 | `readi` | rdwri.c | bread/breada, iomove; 120/128 gelaufen | sicher |
| 25FE | 334 | `writei` | rdwri.c | bwrite/bdwrite, iomove | sicher |
| 2766 | 26 | `min` | rdwri.c | CP a,b: kleineres | sicher |
| 2780 | 194 | `iomove` | rdwri.c | copyin/copyout/bcopy je u_segflg | wahrscheinlich |
| 2842 | 522 | `clock` | trap.c | vektorgerufen (0 Aufrufer); wakeup(lbolt) x3, setpri, psignal | wahrscheinlich |
| 2A4C | 136 | `timeout` | subr.c | Text "Timeout table overflow" | sicher |
| 2AD4 | 50 | `getf` | fio.c | fd (<0x14) -> u_ofile[fd] (FC62), EBADF=9 | sicher |
| 2B06 | 262 | `closef` | fio.c | iput, closep, indirekter Aufruf d_close | wahrscheinlich |
| 2C0C | 128 | `openi` | fio.c | indirekter Aufruf d_open (cdevsw/bdevsw) | wahrscheinlich |
| 2C8C | 126 | `access` | fio.c | Modus 0x80=IWRITE, Pruefung s_ronly (getfs+0x19D), u_error EROFS/EACCES; 10 Aufrufer | sicher |
| 2D0A | 62 | `owner` | fio.c | namei(uchar), vergleicht u_uid (FC16) mit i_uid (+0xA), 2D48 suser | sicher |
| 2D48 | 26 | `suser` | fio.c | TEST u_uid FC16; sonst u_error=EPERM(1) | sicher |
| 2D62 | 56 | `ufalloc` | fio.c | erster freier u_ofile[0..19] (FC62), Ergebnis in u_r.r_val1 FC22, ENFILE-Muster 0x18 | sicher |
| 2D9A | 86 | `falloc` | fio.c | Text "no file"; Filetabelle 9EEE..A07E, 8 B je Eintrag, 0x17=ENFILE | sicher |
| 2DF0 | 106 | `malloc` | malloc.c | Karte (mp) first fit; Aufrufer newproc, xalloc, swap | sicher |
| 2E5A | 238 | `mfree` | malloc.c | Text "map overflow" | sicher |
| 2F48 | 318 | `alloc` | alloc.c | Texte "Bad free count", "no space" | sicher |
| 3086 | 228 | `free` | alloc.c | Gegenstueck zu alloc, ruft badblock 316A | wahrscheinlich |
| 316A | 52 | `badblock` | alloc.c | Text "bad block" | sicher |
| 319E | 444 | `ialloc` | alloc.c | Text "Out of inodes" | sicher |
| 335A | 68 | `ifree` | alloc.c | Superblock s_ninode (getfs) | wahrscheinlich |
| 339E | 98 | `getfs` | alloc.c | Texte "bad count", "no fs"; mount-Tabelle A07E | sicher |
| 3400 | 246 | `update` | alloc.c | sync-Kern (iupdat ueber inode-Tabelle, bwrite, bflush) | wahrscheinlich |
| 34F6 | 182 | `startup` | machdep.c | Text "mem = %D\n", mfree(coremap) x2, Aufrufer main | wahrscheinlich |
| 35AC | 2 | `nullstub1` | machdep.c | 2 B (RET), von main gerufen | vermutet |
| 35AE | 130 | `sendsig` | machdep.c | Aufrufer psig; schreibt Rahmen auf Nutzerstapel (subyte/suword) | wahrscheinlich |
| 3630 | 112 | `backup` | machdep.c | 3 x fubyte-Gruppe 04DA, von trap gerufen (WEGA backup) | vermutet |
| 36A0 | 768 | `namei` | nami.c | 768 B, 13 Aufrufer, iget/access/Verzeichnissuche (func uchar/schar) | sicher |
| 39BA | 40 | `uchar` | nami.c | namei-Argument bei owner 2D0A: Zeichen aus Nutzerraum (04CC-Gruppe) | wahrscheinlich |
| 39E2 | 134 | `pipe` | pipe.c | sysent 42 | sicher |
| 3A68 | 150 | `openp` | pipe.c | ruft readi 2460, wakeup | wahrscheinlich |
| 3AFE | 192 | `closep` | pipe.c | ruft writei 25FE, wakeup | wahrscheinlich |
| 3BBE | 34 | `plock` | pipe.c | sleep(ip) | wahrscheinlich |
| 3BE0 | 28 | `prele` | pipe.c | wakeup(ip), 13 Aufrufer | wahrscheinlich |
| 3BFC | 132 | `getc` | clist.s | spl-gesichert, Warteschlange (tty/lp) | wahrscheinlich |
| 3C80 | 132 | `putc` | clist.s | 17 Aufrufer, tty-Ausgabe | wahrscheinlich |
| 3D04 | 60 | `cinit` | main.c | 1 Aufrufer main | wahrscheinlich |
| 3D40 | 2 | `sureg` | ureg.c | nur RET | vermutet |
| 3D42 | 40 | `estabur` | ureg.c | Summe ≤ 0x100 sonst ENOMEM(12), von getxfile | wahrscheinlich |
| 3D6A | 56 | `fstat` | sys3.c | sysent 28 | sicher |
| 3DA2 | 58 | `stat` | sys3.c | sysent 18 | sicher |
| 3DDC | 214 | `stat1` | sys3.c | gemeinsamer Teil von stat/fstat | wahrscheinlich |
| 3EB2 | 138 | `dup` | sys3.c | sysent 41 | sicher |
| 3F3C | 324 | `smount` | sys3.c | sysent 21 (3 Worte) | sicher |
| 4080 | 188 | `sumount` | sys3.c | sysent 22; ruft xumount 57BC | sicher |
| 413C | 92 | `getmdev` | sys3.c | namei, iput; von mount/umount | wahrscheinlich |
| 4198 | 20 | `exec` | sys1.c | sysent 11, Stub nach exece 41AC | sicher |
| 41AC | 778 | `exece` | sys1.c | sysent 59 | sicher |
| 44B6 | 812 | `getxfile` | sys1.c | ruft estabur 3D42 x3, readi | wahrscheinlich |
| 47E2 | 196 | `setregs` | sys1.c | Signale zuruecksetzen, Register | wahrscheinlich |
| 48A6 | 30 | `rexit` | sys1.c | sysent 1 | sicher |
| 48C4 | 306 | `exit` | sys1.c | freigeben: closef, iput, mfree, wakeup, swtch | wahrscheinlich |
| 49F6 | 222 | `wait` | sys1.c | sysent 7, Zombie 5 (p_stat), Lauf ueber proc A11C bis 8A8C, 0x1C B je Eintrag | sicher |
| 4AD4 | 252 | `fork` | sys1.c | sysent 2, newproc | sicher |
| 4BD0 | 68 | `sbreak` | break.c | sysent 17 | sicher |
| 4C14 | 16 | `gtime` | sys4.c | sysent 13 | sicher |
| 4C24 | 112 | `ftime` | sys4.c | sysent 35: time(8DCC), lbolt(8DCA), 50 Hz, copyout | sicher |
| 4C94 | - | `stime` | sys4.c | sysent 25 (code im Lauf nie erreicht, nicht in funktionen.txt) | sicher |
| 4CC6 | 46 | `setuid` | sys4.c | sysent 23 | sicher |
| 4CF4 | 24 | `getuid` | sys4.c | sysent 24 | sicher |
| 4D0C | 38 | `setgid` | sys4.c | sysent 46 | sicher |
| 4D32 | 24 | `getgid` | sys4.c | sysent 47 | sicher |
| 4D4A | 32 | `getpid` | sys4.c | sysent 20 | sicher |
| 4D6A | 12 | `sync` | sys4.c | sysent 36 -> update 3400 | sicher |
| 4D76 | 70 | `nice` | sys4.c | sysent 34 | sicher |
| 4DBC | 200 | `unlink` | sys4.c | sysent 10 | sicher |
| 4E84 | 18 | `chdir` | sys4.c | sysent 12 | sicher |
| 4E96 | - | `chroot` | sys4.c | sysent 61 (nicht in funktionen.txt) | sicher |
| 4EB0 | 108 | `chdirec` | sys4.c | gemeinsam fuer chdir/chroot | wahrscheinlich |
| 4F1C | 92 | `chmod` | sys4.c | sysent 15 | sicher |
| 4F78 | 56 | `chown` | sys4.c | sysent 16 | sicher |
| 4FB0 | 108 | `ssig` | sys4.c | sysent 48 | sicher |
| 501C | 148 | `kill` | sys4.c | sysent 37 | sicher |
| 50B0 | - | `times` | sys4.c | sysent 43 (nicht in funktionen.txt) | sicher |
| 50D6 | - | `profil` | sys4.c | sysent 44 (nicht in funktionen.txt) | sicher |
| 5100 | 30 | `alarm` | sys4.c | sysent 27 | sicher |
| 511E | - | `pause` | sys4.c | sysent 29 (nicht in funktionen.txt) | sicher |
| 5132 | 30 | `umask` | sys4.c | sysent 60 | sicher |
| 5150 | - | `utime` | sys4.c | sysent 30 (nicht in funktionen.txt) | sicher |
| 51B2 | 18 | `read` | sys2.c | sysent 3 | sicher |
| 51C4 | 18 | `write` | sys2.c | sysent 4 | sicher |
| 51D6 | 232 | `rdwr` | sys2.c | gemeinsam: getf, readi/writei, pipe-plock | wahrscheinlich |
| 52BE | 52 | `open` | sys2.c | sysent 5 | sicher |
| 52F2 | 82 | `creat` | sys2.c | sysent 8 | sicher |
| 5344 | 186 | `copen` | sys2.c | gemeinsam open/creat: namei, maknode?, falloc, openi | wahrscheinlich |
| 53FE | 46 | `close` | sys2.c | sysent 6 | sicher |
| 542C | 126 | `seek` | sys2.c | sysent 19 | sicher |
| 54AA | 172 | `link` | sys2.c | sysent 9 | sicher |
| 5556 | 88 | `mknod` | sys2.c | sysent 14 | sicher |
| 55AE | 126 | `saccess` | sys2.c | sysent 33 (access) | sicher |
| 562C | 214 | `xalloc` | text.c | Text "out of swap space"; malloc/swap | wahrscheinlich |
| 5702 | 142 | `xfree` | text.c | iput, mfree x2; ruft 57A4/57B4 | wahrscheinlich |
| 5790 | 12 | `xtext1` | text.c | 12 B, 3 Aufrufer | vermutet |
| 57A4 | 8 | `xlock` | text.c | 8 B Stub (nichtsegmentiert: leer), Aufrufer swapin, xfree | vermutet |
| 57AC | 8 | `xunlock` | text.c | 8 B Stub, swapin x2 | vermutet |
| 57B4 | 8 | `xccdec` | text.c | 8 B Stub; xalloc, xfree | vermutet |
| 57BC | 8 | `xumount` | text.c | 8 B Stub; sumount ruft es | wahrscheinlich |
| 57C4 | 8 | `xrele` | text.c | 8 B Stub; unlink/chmod/access rufen bei ITEXT (BITB #5) | wahrscheinlich |
| 57D4 | 70 | `bread` | bio.c | Lauf: getblk 5A64, B_DONE (Bit1), b_bcount=0x200, bdevsw[maj]+4 = d_strategy | sicher |
| 581A | 214 | `breada` | bio.c | Read-Ahead: getblk, incore, strategy | wahrscheinlich |
| 58F0 | 90 | `bwrite` | bio.c | iowait, geterror, brelse | sicher |
| 594A | 58 | `bdwrite` | bio.c | B_DELWRI setzen; ruft bawrite/brelse | wahrscheinlich |
| 5984 | 22 | `bawrite` | bio.c | B_ASYNC, bwrite | wahrscheinlich |
| 599A | 136 | `brelse` | bio.c | 31 Aufrufer; spl, wakeup(B_WANTED), Freilistenende | sicher |
| 5A22 | 66 | `incore` | bio.c | 66 B Hash-/Listensuche, 2 Aufrufer | wahrscheinlich |
| 5A64 | 268 | `getblk` | bio.c | Texte "blkdev","devtab" | sicher |
| 5B70 | 140 | `geteblk` | bio.c | getblk fuer ungebundenes Blatt: sleep/notavail | wahrscheinlich |
| 5BFC | 50 | `iowait` | bio.c | sleep bis B_DONE, geterror | sicher |
| 5C2E | 52 | `notavail` | bio.c | spl6, aus Freiliste entfernen (B_BUSY), 4 Aufrufer | wahrscheinlich |
| 5C62 | 36 | `iodone` | bio.c | wakeup, brelse; rkstrategy und rkintr rufen es | sicher |
| 5C86 | 40 | `clrbuf` | bio.c | 40 B, Wortschleife nullen | wahrscheinlich |
| 5CAE | 72 | `mapalloc` | bio.c | Treiber (rk, 6384, 633A, 690A) rufen vor E/A; ruft 5CF6 | vermutet |
| 5CF6 | 84 | `iomap` | bio.c | ruft 065E | vermutet |
| 5D4A | 454 | `swap` | bio.c | Text "IO err in swap"; rekursiv, malloc-Gegenstueck mfree | sicher |
| 5F10 | 70 | `bflush` | bio.c | durch alle Puffer: bwrite, notavail | wahrscheinlich |
| 5F56 | 338 | `physio` | bio.c | ruft sleep/wakeup/geterror; von rrk, fd-Raw gerufen | sicher |
| 60A8 | 36 | `geterror` | bio.c | b_error -> u_error | wahrscheinlich |
| 60CC | 160 | `ttopen_sy` | dev/tty | cdevsw[0].d_open; ruft 6D44/6D8C | vermutet |
| 616C | 34 | `ttclose_sy` | dev/tty | cdevsw[0].d_close; ruft 6DD0 | vermutet |
| 618E | 34 | `sysread` | dev/sys.c | cdevsw[0].d_read -> 7C90 (ttread) | wahrscheinlich |
| 61B0 | 34 | `syswrite` | dev/sys.c | cdevsw[0].d_write -> 7DA6 (ttwrite) | wahrscheinlich |
| 61D2 | 60 | `tt_intr1` | dev/tty | Aufrufer: 7C62 + wakeup | vermutet |
| 620E | 46 | `tt_intr2` | dev/tty | ruft ttyinput 7222 | vermutet |
| 623C | 56 | `sysioctl` | dev/sys.c | cdevsw[0].d_ioctl -> 6E7A (ttioctl) | wahrscheinlich |
| 6274 | 198 | `ttystart` | dev/tty | ruft timeout 2A4C, getc 3BFC, wakeup | vermutet |
| 633A | 74 | `ttstart1` | dev/tty | ruft mapalloc 5CAE | vermutet |
| 6384 | 174 | `ttyout1` | dev/tty | splx x2, mapalloc, rekursiv | vermutet |
| 6432 | 18 | `physinit` | machdep.c | 18 B, von startup gerufen | vermutet |
| 6444 | 150 | `lpopen` | dev/lp | cdevsw[7].d_open (Muster lpopen lpclose nodev lpwrite wie WEGA) | vermutet |
| 64DA | 150 | `lpclose` | dev/lp | cdevsw[7].d_close | vermutet |
| 6570 | 248 | `lpwrite` | dev/lp | cdevsw[7].d_write | vermutet |
| 6668 | 422 | `lpcanon` | dev/lp | putc 3C80 x7 | vermutet |
| 680E | 102 | `lpoutput` | dev/lp | getc 3BFC | vermutet |
| 690A | 80 | `lpint` | dev/lp | mapalloc 5CAE | vermutet |
| 69CE | 132 | `dev2read` | dev | cdevsw[2].d_read: passc 236C | vermutet |
| 6A52 | 136 | `dev2write` | dev | cdevsw[2].d_write: cpass 23CA | vermutet |
| 6ADA | 36 | `rkopen` | rk.c | bdevsw[4].d_open (rootdev 4,2) | wahrscheinlich |
| 6AFE | 8 | `rkclose` | rk.c | bdevsw[4].d_close | wahrscheinlich |
| 6B06 | 90 | `rkstrategy` | rk.c | bdevsw[4].d_strategy, Entwurf 39 Mailbox; ruft iodone 5C62, rkstart | sicher |
| 6B60 | 106 | `rkstart` | rk.c | Befehl an Mailbox, mapalloc | wahrscheinlich |
| 6BCA | 136 | `rkintr` | rk.c | deverror 18A4, iodone 5C62 | wahrscheinlich |
| 6C52 | 30 | `rkread` | rk.c | cdevsw[13].d_read -> physio | sicher |
| 6C70 | 30 | `rkwrite` | rk.c | cdevsw[13].d_write -> physio | sicher |
| 6C8E | 56 | `mmopen` | mem.c | cdevsw[1].d_open | vermutet |
| 6CC6 | 38 | `mmread` | mem.c | cdevsw[1].d_read | wahrscheinlich |
| 6CEC | 38 | `mmwrite` | mem.c | cdevsw[1].d_write | wahrscheinlich |
| 6D12 | 50 | `mmioctl` | mem.c | cdevsw[1].d_ioctl | vermutet |
| 6D44 | 72 | `ttopen` | dev/tty | 72 B, von 60CC | vermutet |
| 6D8C | 68 | `ttclose_hlp` | dev/tty | 68 B, von 60CC | vermutet |
| 6DD0 | 32 | `ttclose` | dev/tty | von 616C, ruft 715C (ttywait) | vermutet |
| 6DF0 | 138 | `ioctl` | sys3.c | sysent 54; getf 2AD4, indirekter Aufruf d_ioctl | sicher |
| 6E7A | 738 | `ttioctl` | dev/tty | 738 B, copyin x3/copyout x5, ruft 715C | wahrscheinlich |
| 715C | 72 | `ttywait` | dev/tty | sleep bis Ausgabe leer, ruft 71A4 | wahrscheinlich |
| 71A4 | 126 | `ttyflush` | dev/tty | splx, getc x2, wakeup x2 | wahrscheinlich |
| 7222 | 912 | `ttyinput` | dev/tty | 912 B, signal 0B3A (^C), putc x5, ruft 75B2..7C62 | wahrscheinlich |
| 75B2 | 232 | `tty_sub1` | dev/tty | 232 B Teil von ttyinput (Zeichenbehandlung) | vermutet |
| 769A | 148 | `tty_sub2` | dev/tty | 148 B; ruft 7948, 7A34 | vermutet |
| 772E | 378 | `tty_sub3` | dev/tty | 378 B; ruft 7A34 x6 (Echo/Ausgabe) | vermutet |
| 78A8 | 160 | `tty_sub4` | dev/tty | 160 B; spl | vermutet |
| 7948 | 50 | `ttxput` | dev/tty | 50 B, 7 Aufrufer | vermutet |
| 797A | 102 | `tty_sub5` | dev/tty | 102 B | vermutet |
| 79E0 | 84 | `tty_sub6` | dev/tty | 84 B; putc, 71A4, 7C62 | vermutet |
| 7A34 | 534 | `ttyoutput` | dev/tty | 534 B, rekursiv, putc x3 | wahrscheinlich |
| 7C62 | 46 | `ttrstrt` | dev/tty | 46 B; spl; 7 Aufrufer | vermutet |
| 7C90 | 278 | `ttread` | dev/tty | 278 B, sleep, getc, passc x2 | wahrscheinlich |
| 7DA6 | 112 | `ttwrite` | dev/tty | 112 B, sleep, cpass, ttyoutput | wahrscheinlich |
| 7E16 | 126 | `fdopen` | fd.c | bdevsw[0].d_open, cdevsw[10].d_open (Floppy ueber U880?) | vermutet |
| 7E94 | 108 | `fdclose` | fd.c | bdevsw[0].d_close | vermutet |
| 7F00 | 148 | `fdsub` | fd.c | 148 B: sleep, wakeup, iowait; ruft 7F94 | vermutet |
| 7F94 | 72 | `fdstrategy` | fd.c | bdevsw[0].d_strategy | wahrscheinlich |
| 7FDC | 574 | `fdstart` | fd.c | 574 B: iodone, clrbuf, mapalloc | vermutet |
| 84E4 | 56 | `fdread` | fd.c | cdevsw[10].d_read -> physio | wahrscheinlich |
| 851C | 56 | `fdwrite` | fd.c | cdevsw[10].d_write -> physio | wahrscheinlich |
| 8554 | 92 | `fdphys` | fd.c | 92 B; von 84E4/851C | vermutet |
| 85B0 | 426 | `fdioctl` | fd.c | cdevsw[10].d_ioctl, 426 B | vermutet |

Auffaelligkeiten:
* MUTOS ist **V7/System-III-nah** (kein `prsave/prstate`, kein `pgrp` in `proc`, `NOFILE` = 20, 16-Bit-Zeiger) und nicht die WEGA-Segmentversion; Namen der Funktionen sind gleich, Argumente/Strukturen sind die der nichtsegmentierten Z8002-Fassung.
* `text.c` (`xlock`, `xunlock`, `xccdec`, `xumount`, `xrele`) besteht bis auf `xalloc`/`xfree` aus **8-Byte-Stubs** (nur `csv`/`cret`) -- es gibt keinen gemeinsamen Textabschnitt (Prozess = 64 KB ohne MMU-Teilung).
  Die Zuordnung der fuenf Stubs (57A4..57C4) zu den Namen folgt WEGA-Reihenfolge und Aufruferlage; 57BC (nur `sumount` ruft es) und 57C4 (`unlink/chmod/access` bei ITEXT) sind sicherer als die drei davor.
* `Treiber Major 0 / cdevsw 10` (7E16..85B0, im Lauf nie erreicht) folgt dem Muster von WEGA `fd` (open/close/strategy + Raw read/write/ioctl).  Auf `fd` deutet WEGA `conf.c` (`bdevsw[1]`/`cdevsw[1]`: `fdopen fdclose fdstrategy`, Raw `fdread fdwrite fdioctl`);
  im MUTOS-Abbild ist es Major 0.  Die Art des Geraetes ist **nicht** belegt (Diskette ueber die U880-Seite ist naheliegend).
* `cdevsw[7]` hat das WEGA-`lp`-Muster (`open close nodev write ioctl`, siehe `conf.c`) -> Drucker; 6444..690A als `lp*` vermutet.
* `tty` (7222, 7A34, 7C90, 7DA6, 6E7A): Namen aus dem V7-/WEGA-Vorbild; die Einzelteile 75B2..79E0 sind Teilfunktionen von `ttyinput` ohne eindeutigen Namen.

## 2. Systemaufruftabelle `sysent` (8B58, 63 Eintraege)

Eintrag = 4 Byte `[Argumentworte<<8, Funktion]`.  Die Tabelle beginnt bei **8B58** (die Karte nannte bisher 8B56; die zwei Bytes davor sind das Ende der Zeichenkette `mem = %D\n`).
Die Nummern folgen der WEGA-Tabelle (`sys/sysent.c`); alle Argumentzahlen stimmen mit der **V7-Zaehlung in Worten** ueberein (`seek` 4 = fd + long + whence, `stime` 2 = long, `ptrace` 4).
Beleg fuer die Zuordnung Nummer -> Funktion: Argumentzahl + Funktionsrumpf (z. B. `sysent[54]` = 6DF0 ruft `getf` und einen indirekten Aufruf = `ioctl`; `sysent[11]` ist 4198, 20-Byte-Stub vor `exece` 41AC).
Nicht belegte Nummern zeigen auf `nosys` 0B2A (Fehler EINVAL) bzw. `nullsys` 0B38 (ohne Fehler: 0, 38, 39).

| Nr. | Name (WEGA) | Argumentworte | Funktion | MUTOS-Name |
|-----|-------------|---------------|----------|------------|
| 0 | indir | 0 | 0B38 | nullsys
 |
| 1 | exit | 1 | 48A6 | rexit
 |
| 2 | fork | 0 | 4AD4 | fork
 |
| 3 | read | 3 | 51B2 | read
 |
| 4 | write | 3 | 51C4 | write
 |
| 5 | open | 2 | 52BE | open
 |
| 6 | close | 1 | 53FE | close
 |
| 7 | wait | 0 | 49F6 | wait
 |
| 8 | creat | 2 | 52F2 | creat
 |
| 9 | link | 2 | 54AA | link
 |
| 10 | unlink | 1 | 4DBC | unlink
 |
| 11 | exec | 2 | 4198 | exec
 |
| 12 | chdir | 1 | 4E84 | chdir
 |
| 13 | time | 0 | 4C14 | gtime
 |
| 14 | mknod | 3 | 5556 | mknod
 |
| 15 | chmod | 2 | 4F1C | chmod
 |
| 16 | chown | 3 | 4F78 | chown
 |
| 17 | break | 1 | 4BD0 | sbreak
 |
| 18 | stat | 2 | 3DA2 | stat
 |
| 19 | seek | 4 | 542C | seek
 |
| 20 | getpid | 0 | 4D4A | getpid
 |
| 21 | mount | 3 | 3F3C | smount
 |
| 22 | umount | 1 | 4080 | sumount
 |
| 23 | setuid | 1 | 4CC6 | setuid
 |
| 24 | getuid | 0 | 4CF4 | getuid
 |
| 25 | stime | 2 | 4C94 | stime
 |
| 26 | ptrace | 4 | 0E42 | ptrace
 |
| 27 | alarm | 1 | 5100 | alarm
 |
| 28 | fstat | 2 | 3D6A | fstat
 |
| 29 | pause | 0 | 511E | pause
 |
| 30 | utime | 2 | 5150 | utime
 |
| 31 | stty | 0 | 0B2A | nosys
 |
| 32 | gtty | 0 | 0B2A | nosys
 |
| 33 | access | 2 | 55AE | saccess
 |
| 34 | nice | 1 | 4D76 | nice
 |
| 35 | ftime | 1 | 4C24 | ftime
 |
| 36 | sync | 0 | 4D6A | sync
 |
| 37 | kill | 2 | 501C | kill
 |
| 38 | fcntl | 0 | 0B38 | nullsys
 |
| 39 | setpgrp | 0 | 0B38 | nullsys
 |
| 40 | bpt | 1 | 0B2A | nosys
 |
| 41 | dup | 2 | 3EB2 | dup
 |
| 42 | pipe | 0 | 39E2 | pipe
 |
| 43 | times | 1 | 50B0 | times
 |
| 44 | prof | 4 | 50D6 | profil
 |
| 45 | ssgbrk | 0 | 0B2A | nosys
 |
| 46 | setgid | 1 | 4D0C | setgid
 |
| 47 | getgid | 0 | 4D32 | getgid
 |
| 48 | sig | 2 | 4FB0 | ssig
 |
| 49 | lkdata | 0 | 0B2A | nosys
 |
| 50 | unlnk | 0 | 0B2A | nosys
 |
| 51 | acct | 0 | 0B2A | nosys
 |
| 52 | sprofil | 0 | 0B2A | nosys
 |
| 53 | lock | 0 | 0B2A | nosys
 |
| 54 | ioctl | 3 | 6DF0 | ioctl
 |
| 55 | mkseg | 0 | 0B2A | nosys
 |
| 56 | sgstat | 0 | 0B2A | nosys
 |
| 57 | utssys | 0 | 0B2A | nosys
 |
| 58 | sptrace | 0 | 0B2A | nosys
 |
| 59 | exece | 3 | 41AC | exece
 |
| 60 | umask | 1 | 5132 | umask
 |
| 61 | chroot | 1 | 4E96 | chroot
 |
| 62 | mdmctl | 0 | 0B2A | nosys |

## 3. Strukturen

Alle Offsets in Byte; Beleg `Lauf` = aus dem Code der laufenden Funktion, `WEGA` = aus `head/sys/*.h` uebernommen und mit dem Code nicht widerlegt, `Abzug` = Speicherabzug am laufenden Kernel.

### `user` (u), feste Adresse **FC00**

Kernel-Stapel und `u` des laufenden Prozesses stehen bei `FC00` (Referenzen `%FCxx` im Code: FC13 244x, FC1E 90x ...).  Offsets (relativ FC00):

| Adresse | Feld | Beleg |
|---------|------|-------|
| FC12 | `u_segflg` (Byte) | WEGA, iomove-Schalter |
| FC13 | `u_error` (Byte) | Lauf (244 Zugriffe, z. B. ENFILE 0x17 in `falloc`, EBADF 9 in `getf`, EPERM 1 in `suser`) |
| FC16 | `u_uid` | Lauf (`suser`: TEST FC16) |
| FC18/1A/1C | `u_gid`, `u_ruid`, `u_rgid` | WEGA |
| FC1E | `u_procp` | Lauf (`sleep`: `LD R13,FC1E` -> proc) |
| FC20 | `u_ap` (Zeiger auf die Argumente) | Lauf (`ftime`: `LD R13,%FC20`) |
| FC22 | `u_r.r_val1` (Rueckgabe) | Lauf (`ufalloc`: fd -> FC22) |
| FC24 | `u_r.r_val2` | Lauf (`wait`: Status -> FC24) |
| FC26 | `u_base` | WEGA, 48 Zugriffe |
| FC28 | `u_count` | WEGA, 82 Zugriffe |
| FC2A | `u_offset` (long) | WEGA (Lauf: LDL FC2A) |
| FC2E | `u_fmode` | WEGA |
| FC30/FC32 | `u_cdir`, `u_rdir` | WEGA |
| FC34..FC41 | `u_dbuf[14]` | WEGA (DIRSIZ 14) |
| FC42 | `u_dirp` | WEGA |
| FC44 | `u_dent` (16 B: ino, name[14]) | WEGA, FC52 16x |
| FC54.. | `u_pdir`, `u_break` | WEGA |
| **FC62** | `u_ofile[20]` (bis FC89) | Lauf (`getf`: `CP fd,#0x14`; `ufalloc`: `ADD #FC62`) |
| **FC8A** | `u_pofile[20]` (Byte) | Lauf (`ufalloc`: `CLRB FC8A(R13)`) |
| FC9E.. | `u_arg[]` (Argumentworte des Systemaufrufs) | Lauf (`syscall`/`ptrace`-Pfad, FC9E 7x) |
| FCAE | `u_qsav`-Gruppe (`sleep` uebergibt FCAE an den Kontextwechsel) | Lauf |
| FCF4 | long (Zeit-/Zaehlerfeld; `ftime`-Nachbarschaft) | Lauf (LDL FCF4 in 2842 `clock`) |
| FCFC/FD00 | long (`u_utime`/`u_stime`-Muster: `wait` addiert 0x12/0x16 des Zombie-proc dort) | Lauf |

Weitere `u`-Felder (FD04 70x, FD0A.. = Signaltabelle, Profiling, `u_comm`) sind aus den Zugriffsmustern **nicht** einzeln zugeordnet; WEGA-Feldfolge ist wegen des nichtsegmentierten Aufbaus nicht uebertragbar.

### `proc`, 28 Byte (0x1C), Tabelle ab **A11C**, Ende-Zeiger in `8A8C`

| Offset | Feld | Beleg |
|--------|------|-------|
| 0 | `p_stat` (Byte; 3 = SRUN, 5 = SZOMB, 6 = SSTOP) | Lauf (`setrun`, `wait`), Abzug (`proc[0]`: 03) |
| 1 | `p_flag` (Byte; SLOAD bit0, SWTED 0x20) | Lauf, Abzug (03) |
| 2 | `p_pri` (Byte) | Lauf (`sleep`, `setrun`: Vergleich mit curpri 8DDA), Abzug (0x14) |
| 3 | `p_time` | WEGA, Abzug (0x71) |
| 4 | `p_cpu` | WEGA, Abzug (0xE8) |
| 5 | `p_nice` | WEGA, Abzug (0x14 = NZERO) |
| 6 | `p_sig` (long) | Lauf (`wait` loescht 6) |
| 0xA | `p_uid` | Lauf (`wait` loescht 0xA) |
| 0xC | `p_pid` | Lauf (`wait`: Vergleich/Rueckgabe) |
| 0xE | `p_ppid` | Lauf (`wait`: `CP u_procp->p_pid, p_ppid`) |
| 0x10 | `p_addr` (Zombie: Exit-Status) | Lauf (`sleep` -> `*(p+0x10)` an Kontextwechsel; `wait`: 0x10 -> FC24) |
| 0x12 | `p_size` (Zombie: `xp_utime`, long) | Lauf (`wait`: ADDL 0x12 -> FCFC) |
| 0x14 | `p_wchan` | Lauf (`sleep`, `setrun`) |
| 0x16 | `p_textp` | WEGA |
| 0x18 | `p_link` (Kette Schlafwarteschlange `slpque` und Ablaufkette) | Lauf (`sleep`: `LD 0x18(R13), @slpque[..]`) |
| 0x1A | `p_clktim` | WEGA |

Anzahl der Eintraege nicht aus dem Code gefolgert (Tabelle `A11C..` bis hoechstens `A518`, also <= 36); `proc[0]` im Abzug: `03 03 14 71 E8 14 ...`, nach dem Kaltstart `8A8C` = `A154`.
Sleep-Warteschlangen: `slpque[64]` bei **CEC6**, Index `(chan >> 5) & 63`.

### `inode`, 74 Byte (0x4A), Tabelle **907A..9EEE** (50 Eintraege)

| Offset | Feld | Beleg |
|--------|------|-------|
| 0 | `i_flag` (Byte; ILOCK 1, IMOUNT 8, IWANT 4?..) | Lauf (`iget`: TESTB, `AND #1`, `AND #8`, SETB #4 vor sleep) |
| 1 | `i_count` (Byte) | Lauf (`iget`: INCB 1) |
| 2 | `i_dev` | Lauf (`iget`: Vergleich) |
| 4 | `i_number` | Lauf |
| 6 | `i_mode` | WEGA/V7 (Reihenfolge, `owner` liest 0xA als uid) |
| 8 | `i_nlink` | V7 |
| 0xA | `i_uid` | Lauf (`owner`: `CP u_uid, 0xA(R13)`) |
| 0xC | `i_gid` | V7 |
| 0xE | `i_size` (long) | V7 |
| 0x12 | `i_addr[13]` (je 4 Byte, 52 B) | V7/`iexpand` |
| 0x46 | `i_lastr` (long) | Lauf (`iget`: LDL 0x46 loeschen) |

### `file`, 8 Byte, Tabelle **9EEE..A07E** (50 Eintraege)

| Offset | Feld | Beleg |
|--------|------|-------|
| 0 | `f_flag` (Byte) | Lauf (`falloc` prueft `TESTB 1` fuer frei) |
| 1 | `f_count` (Byte) | Lauf (`INCB 1`) |
| 2 | `f_inode` | V7 (nicht einzeln gelesen) |
| 4 | `f_offset` (long) | Lauf (`LDL 4(R13)` = 0) |

### `buf`, 24 Byte (0x18), Tabelle **8E9A**, 20 Puffer, Daten **A628 + 512*i**

| Offset | Feld | Beleg |
|--------|------|-------|
| 0 | `b_flags` (B_READ 1, B_DONE 2, B_ERROR 4, B_BUSY 8, B_WANTED 0x40) | Lauf (`bread`: `BIT #1`, `SET #0`; `getblk`: `BIT #3`, `SET #6`; `binit`: `LD @R13,#8`) |
| 2 | `b_forw` | Lauf (`binit`: Hashkopf) |
| 4 | `b_back` | Lauf |
| 6/8 | `av_forw`, `av_back` (Freiliste `bfreelist` = **A518**..A520) | Lauf (`binit`) |
| 0xA | `b_dev` (initial FFFF) | Lauf (`binit`) |
| 0xC | `b_bcount` (0x200) | Lauf (`bread`: `LD 0xC(R13),#0200`) |
| 0xE | `b_addr` (A628 + i*512) | Lauf (`binit`) |
| 0x10 | `b_blkno` (long) | Lauf (`getblk`: CPL 0x10) |
| 0x14 | `b_error` (Byte) | Lauf (`binit`: CLRB 0x14) |

`bdevsw`: je 8 Byte, `d_tab` bei +6 (`getblk` liest `876C` = `bdevsw[0].d_tab`); `nblkdev` bei **8DD0** (von `binit` gezaehlt).

### `mount`, 6 Byte, Tabelle **A07E..A09C** (5 Eintraege)
`m_dev` 0, `m_bufp` 2, `m_inodp` 4 (Lauf: `iget` sucht in der Tabelle den Eintrag mit `m_inodp == ip`).

### `callo`
`timeout` (2A4C, Text "Timeout table overflow") verwaltet eine Tabelle; Layout und Lage wurden nicht aus dem Code gelesen (die Funktion lief im Lauf nie).  Offen.

### `text`
Siehe Stubs in Abschnitt 1: es gibt keine Texttabelle (`xlock` & Co. leer).  `p_textp` bleibt in `proc` fuer die Form.

## 4. Globale Variablen (Kernel-Datenbereich, Auswahl)

| Adresse | Name | Beleg |
|---------|------|-------|
| 8766 | `bdevsw[]` | K6a |
| 8796 | `cdevsw[]` | K6a |
| 887C/887E/8880 | `rootdev`, `swapdev`, `pipedev` | K6a/`mutos_patch.py` |
| 8884 | `swplo` (long = 250) | K6a, Abzug-Wert 0x00FA |
| 8888 | `nswap` (= 244) | K6a, Wert 0x00F4 |
| 888A..88B5 | Adresstabelle (22 Zeiger, Form `proc`, `file`, ..): A11C(proc), 9EEE(file), A530, 8C94, A620, 8E9A(buf), A628(Pufferdaten), A518(bfreelist), CE28, CE40, 8DD6(runin), 8DD8(runout), FC00(u), 8DCA(lbolt), 8DCC(time), CE58, CE94, A464, 907A(inode), 887E(swapdev), 8884(swplo), CEAC | Zuordnung der Eintraege mit Namen ueber die Adressen im Code; die Tabelle selbst wirkt wie eine nlist-Ersatztabelle fuer `ps`/`pstat` (vermutet) |
| 8A8C | `proctop` (Zeiger auf das Ende der benutzten `proc`-Eintraege) | Lauf (`wait`: Schleifengrenze, Abzug A154) |
| 8DC0 | Basis/Segment des Nutzerzugriffs (`copyin`/`copyout`) | Lauf (`LD R4,L8DC0`) |
| 8DC2 | Kopie der Ausgabe an Port 0080 (aktuelle Prioritaet `spl`) | Lauf (`spl*`: Maske `C0`) |
| 8DC4 | `runrun` | Lauf (`setrun`: INCB bei besserer Prioritaet) |
| 8DCA | `lbolt` (Wort, 0..49 bei 50 Hz) | Lauf (`ftime`: `CP #0x32`) |
| 8DCC | `time` (long) | Lauf (`ftime`) |
| 8DD0 | `nblkdev` | Lauf (`binit` zaehlt, `getblk` prueft, Text `blkdev`) |
| 8DD6 | `runin` | Lauf (`sleep`: TESTB/CLRB, `wakeup(&runin)`) |
| 8DD8 | `runout` | Lauf (`setrun`) |
| 8DDA | `curpri` | Lauf (`setrun`: CPB 2(p), 8DDA) |
| 907A..9EEE | `inode[50]` | Lauf |
| 9EEE..A07E | `file[50]` | Lauf |
| A07E..A09C | `mount[5]` | Lauf |
| A11C | `proc[]` | Lauf |
| A518 | `bfreelist` | Lauf |
| 8E9A | `buf[20]` | Lauf |
| A628 | Pufferdaten 20 x 512 | Lauf |
| CEC6 | `slpque[64]` | Lauf |
| D578 | Fangadresse fuer Zugriffsfehler (`EX R1,%D578` in `copyin`/`copyout`/`fuword`) | Lauf (`nofault`-Muster, Wert wird gerettet und zurueckgesetzt) |
| FC00 | `u` | Lauf |

## 5. Offene Fragen

Nicht benennbar ohne weitere Laeufe sind die tty-Teilfunktionen (75B2..79E0), der Treiber Major 0 (Geraeteart), `cdevsw[2]`, die fuenf `text`-Stubs im Einzelnen,
`startup` (34F6)/`35AC`/`3630`, `savu`/`resume` (0614/0620), `mapalloc`/`iomap`, die `callo`-Tabelle, die weiteren `u`-Felder (ab FD04), Anzahl `proc`-Eintraege, Adresstabelle 888A, die Ablauf-Bedeutung von 03F8/03FE.

# make fuer MUTOS (K1/K11, Entwurf 41)

`make.c` ist ein **Eigenbau** (kein Fremdcode, ~750 Zeilen K&R-C) im Funktionsumfang des
Unix-V7-make; `make.bin` ist das im Emulator-Gast mit dem MUTOS-`cc` uebersetzte und
gebundene Programm (19866 B, lauffaehig unter MUTOS, `make.bin.fileinfo` traegt Modus 0775).
Warum kein V7-Original: dessen `gram.y` braucht yacc, und der Quellbaum waere ein Mehrfaches
an Uebersetzungszeit; der Gast hat ohnehin kein `yacc`.

## Funktionsumfang
`ziel... : abh... [; befehl]`, Befehlszeilen mit TAB, `NAME = wert`, `$(NAME)` `${NAME}` `$N`,
`$(NAME:.c=.o)`, `$$`; Sondermakros `$@ $< $* $? $^`; `.SUFFIXES:`, Suffixregeln `.c.o`/`.s.o`
(eingebaut `$(CC) $(CFLAGS) -c $<`, `CC=cc`), Vorsatz `-`/`@` an Befehlen, `\`-Fortsetzung,
`#`-Kommentare; Aufruf `make [-f datei] [-n] [-s] [-i] [NAME=wert] [ziel...]`.
Zeitstempel ueber `stat()` (`st_mtime`, Sekunden; "neuer" heisst echt groesser), Befehle ueber
`system()` = `/bin/sh -c`.  Nicht vorhanden: `include`, Bedingungen, `%`-Muster, Archivglieder
`lib(x.o)`, `.PHONY`/`.PRECIOUS`/`.DEFAULT`, `-k`/`-t`/`-p`, Umgebungs-Ueberschreibung
(Umgebung gilt nur fuer undefinierte Makros).  Ein Ziel ohne Datei (z. B. `all:`) gilt immer
als zu bauen.  Eine zweite Regelzeile mit Befehlen **ersetzt** die erste (so ueberschreibt
ein Makefile die eingebaute `.c.o`-Regel).  Ein fehlschlagender Befehl beendet make (Code 1).

## Was der MUTOS-`cc` verlangt (V7-cc!)
* **Strukturtags und -glieder teilen EINEN Namensraum mit Variablen** (alle Glieder global
  eindeutig, nicht mit lokalen Namen kollidierend): Glieder tragen darum Vorsaetze
  (`d_name`, `n_deps`, `m_v` ...), Tags heissen `nameent`/`macent`.  Verstoss meldet cc als
  `x redeclared` / `Bad structure/union/enum name` und die Folgefehler ueberfluten den Schirm.
* Altes K&R: Parameterdeklarationen hinter der Kopfzeile, keine Prototypen, kein `void`,
  kein `enum`; Funktionen vorab ohne Parameter deklarieren (`char *malloc();`).

## Gast-Bau und Pruefung (gemessen)
**`/tmp` im Gast muss ein eigener Datentraeger sein**: die Wurzeldiskette ist fuer die
cc-Zwischendateien zu klein (`no space on dev 4/2`, dann `syntax error` im Assembler).
`bau_make.dbg` haengt deshalb eine leere Diskette in Laufwerk A (`/etc/mount /dev/rk0 /tmp`).

    tools/mutos8000/kernel/make/vorbereiten.sh /pfad/arbeit        # Kopien + Skripte
    tools/dev.sh tool k1520dbg --rw --em em256 --laufwerke K5601,K5601,K5601,K5601 \
        /pfad/arbeit/cp/a5120_mutos8000_boot_udos_k5601.hfe -x /pfad/arbeit/test_make.dbg > test.log
    build/k1520disktool get /pfad/arbeit/quelle.hfe make --to /pfad/arbeit/out --fileinfo

(`--rw` + KOPIEN: ohne `--rw` verwirft k1520dbg alle Schreibzugriffe; auf die Originale in
`disks/` darf `--rw` nie zeigen.)  `bau_make.dbg` baut nur, `test_make.dbg` baut und prueft.

Rechenzeit (Wanduhr, Emulator, dev.sh-Lauf inkl. Boot ~1 min): `cc -c make.c` ca. 30-40 s
Wanduhr (Maschinenzeit nicht einzeln gemessen), `cc -o make make.o` aehnlich; der ganze
Lauf Bau + Pruefung mit drei Uebersetzungen von t1/t2 dauert **3,5 min Wanduhr**.
Ergebnis: `make` 19866 B, `make.o` 21040 B.

Gast-Ausgabe der Pruefung (`test/Makefile`: `prog: t1.o t2.o`, Befehl `cc -o prog $(OBJS)`):

    # make -n
    cc  -c t1.c
    cc  -c t2.c
    cc -o prog t1.o t2.o
    # make
    cc  -c t1.c
    cc  -c t2.c
    cc -o prog t1.o t2.o
    # make
    make: 'prog' ist aktuell.
    # ./prog
    t1: f=7
    # cat t2.c >t2n.c; cat t2n.c >t2.c          (t2.c veraendert -> neuer Zeitstempel)
    # make -n
    cc  -c t2.c
    cc -o prog t1.o t2.o
    # make
    cc  -c t2.c
    cc -o prog t1.o t2.o
    # ./prog
    t1: f=7

Nur Veraltetes wird uebersetzt (t1.o bleibt), ein zweiter Lauf baut nichts.

## Gast-Stolpersteine
* `cp`/`mv` fehlen in `/bin` der Wurzel (Doku k1 nannte sie): Zeitstempel erneuern mit
  `cat a >b; cat b >a`.
* Zeitstempel: Quellen auf der Quelldiskette tragen `mtime` 1987 (`attr --mtime 543018037`),
  der Gast steht nach `date 8710270000` auf 1987-10-27; Dateien vom Host haetten sonst die
  Hostzeit (2026) und waeren stets "neuer" als jedes im Gast erzeugte `.o`.
* Die Leerzeichen-Doppelung `cc  -c` kommt vom leeren `$(CFLAGS)`; harmlos.
* `make.bin` nach `/usr/bin` o. ae. einzuspielen: `k1520disktool put <usr-abbild> make.bin`
  (dann `attr ... --mode 0755`); das Ziel im Gast heisst `make`.

## Spaetere Schritte (offen)
* Ein Makefile fuer den Kernel selbst (AP K11) und `-t`/`-k`; Archivglieder `libx.a(y.o)`,
  falls der Kernelbau `ar` benutzt.
* `ar`/`ranlib`-Verhalten und `ld`-Kernelskript sind nicht Teil dieses Pakets.

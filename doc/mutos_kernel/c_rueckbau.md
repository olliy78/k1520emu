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

# K8: Gesamtbau des Kernels aus C und Assembler (2026-10-28)

`tools/mutos8000/kernel/gesamtbau.py` baut `mutos_1216` aus den C-Quellen (`c/*.c`, K7) und dem Handassembler (`s/low.s`, K5) neu,
bindet ihn mit dem Gast-`ld`, legt ihn auf eine Ladediskette (`mutos_1216` ersetzt) und bootet.  Wächter: `tests/python/test_mutos_gesamtbau.py`
(schnell: Bausteine da; voll nur mit `K1520_GESAMTBAU=1`, Minuten bis Stunden, nicht in der Standardregression).

## Ergebnis
| Bau | Funktionen aus C | Ergebnis |
|---|---|---|
| **Stufe 1** (`--stufe 1`) | 177 von 227 (die bytegleichen) | **36 352 von 36 352 Byte bytegleich** zum Original, Boot bis `#` + Arbeitstests wie das Original |
| **Stufe 2** (`--stufe 2`) | **227 von 227** (nur `low` = 26 Assemblerfunktionen bleiben) | funktionsgleich, nicht bytegleich: Text 34 376 B (Original 34 650), bootet bis `#`; ls, cat, Pipes, `cc -c`, `cc t2.c; a.out`, `cc -O -c prf.c` laufen |
| **Natürlich** (`--natuerlich`) | alle 29 C-Dateien mit `cc` im Gast, `make` im Gast | siehe unten |

Arbeitstests (`BOOTTEST` im Skript, Ausgabe des Gasts, Stufe 2 und Original gleich): `ls -l /`, `ls /etc | cat`, `cat t.c | cat | cat`,
`cc -c t.c`, `date`, `cat t.c >/tmp/e1; cat /tmp/e1`, `cc t2.c; a.out; a.out | cat` -> `summe = 5050` zweimal, `cc -O -c prf.c` -> `prf.o` 1600 B.
Bildschirm nach dem Boot: `mem = 196608`, `single user login: root`.

## Verfahren (Mischbau je FUNKTION, Stufe 1/2)
1. **Übersetzen** (`--uebersetzen`): `c/gast_lauf.sh` in sechs gleichzeitigen Gastläufen (`cc -O -c`, `dev0.c` ohne `-O`), ~4 Min. Wanduhr insgesamt.
2. **Zerlegen**: der Kernel ist die Folge seiner Funktionen (`lauf/funktionen.txt` + Marken `/* KERNEL xxxx */` der C-Quellen).  Je Funktion
   C oder Assembler; Assembler-Strecken schreibt `z8kdis --modul` für genau den Adressbereich (`linkkarte_neu.txt` wird erzeugt),
   zusammenhängende C-Funktionen werden auf dem Wirt zu EINEM Objekt (`komposit`, `objformat.py`) geschnitten.
3. **Kernelfertig machen**: das `cc`-Objekt kennt Namen und eigene Daten, der Kernel Adressen.  Jede Relokationsstelle wird auf den Namen der
   ADRESSE abgebildet (Kartenname bzw. `Lxxxx`, wie z8kdis sie schreibt); Adressen ab D256 und `u` (FC00) werden Konstanten.
   **Die Adressen der C-Namen sind gemessen**, nicht geraten: Kernelwort minus Zusatzwert an jeder Relokationsstelle einer bytegleichen Funktion,
   Mehrheitsabstimmung (eine Funktion mit vertauschter Argumentreihenfolge, `setregs`, sieht der Maskenvergleich nicht -- die Abstimmung schon).
   Rest: `adressen.txt` (Kopfkommentare der C-Quellen, `kernel_namen.md`).  Zeichenketten/Tabellen ohne Zeiger werden per Inhalt im Kernel gefunden;
   **Sprungtabellen des neuen Codes werden mitgebunden** (Zeiger in den neuen Text), die alten werden als Waisen aus den Daten gelassen (124 B).
4. **Binden im Gast**: `as -u` je Assemblermodul, `ld -s -o lk` in Adressreihenfolge, danach Wirt: 16-B-Kopf und Null-BSS abschneiden, auf 512 B runden.
   Die Daten (`da*`) bleiben Assembler; Sprungtabellen des ALTEN Codes zeigen ins Leere und werden auf 0 gesetzt (`tote_marken`).
5. **Auffüllen**: ein Nullfeld (`pad`, 236 B) hält den BSS-Anfang bei **8DC0**; alle BSS-Variablen (8DC0..D256), Postfach D532, `u` bleiben an
   ihren Originaladressen.  Ist der Kernel zu groß, bricht der Bau ab (BSS reichte über D256).
6. **Ladediskette**: Kopie von `a5120_mutos8000_boot_udos_k5601.hfe`, `rm` + `put mutos_1216` (Dateiangaben des Originals), `gast_as.gast_befehle(boot_hfe=...)`.

## Stolpersteine (gemessen)
* **`ld: Local symbol botch`**: eine Relokation `extern` auf ein im SELBEN Objekt definiertes Symbol.  Der `cc` nimmt dafür Textrelokation
  (Art 1, Zusatzwert = Offset); `komposit` tut dasselbe.
* `ld` meldet Bezüge auf nicht definierte Marken (`L70D6`) aus Datenmodulen: Tabellen des alten Codes -> 0.
* Gast hat nur `cat date login ls sh stty sync` in `/bin`, `as cc ld` in `/usr/bin`: kein `echo`, `wc`, `dd` -- die Arbeitstests benutzen `ls`/`cat`.
* Kopf (16 B) und BSS-Nullen können im Gast nicht abgeschnitten werden (kein `dd`): Host-Nachschritt in `gesamtbau.py`.

## Abweichungen zum Original (Stufe 2)
* **Text** 34 376 B statt 34 650 (-274); die erste Funktion, die abweicht, steht bei 0249 (`low` ist bytegleich); danach verschieben sich alle Funktionen
  um 0..-122 B (`gesamtbau_adressen.txt`: Name, alt, neu, Abstand; 227 Zeilen).  Data: 1676 B statt 1638 (+162 mitgebundene Tabellen, -124 Waisen).
* **BSS unverändert** (8DC0..D256, durch `pad`); Postfach D532, Konsolenpuffer, `u` bei FC00 unverändert.  Alle Daten- und Textadressen sonst anders;
  die Zeigertabelle für ps/pstat (88B8..) zeigt auf BSS-Adressen und stimmt weiter, auf Text-/Datenadressen nicht (nur Stufe 2, `ps` ungeprüft).
* Gleichwertig aber nicht bytegleich (aus K7): 14 "nahe" und 28 "offen" Funktionen (anderer Blockaufbau des Kernel-`c2`, Konstantenvergleich über R2 ...).
  **Nicht erprobt**: Floppy-Treiber `fd*` (im Kernel nie erreicht), Drucker `lp*`, Swap unter Last, `exec` großer Programme.

## Natürlicher Bau mit `make` im Gast (`--natuerlich`)
Ziel: ohne Host-Umbau der Objekte.  Alle 29 C-Dateien (+`conf.c`, `sysent.c`) unverändert mit `cc -O -I/mnt -c` (`dev0.c` ohne `-O`), dazu
* `low.s` (Handassembler, aus `z8kdis` mit **C-Namen** als Kartenüberlagerung `namen_c.txt`: `name ADR _x` für Funktionen und BSS-Variablen),
* `bss.s` (BSS in Adressfolge, gleiche Namen), `dn0..3.s` (vier kleine Anfangswerte, die keine C-Datei definiert: `regloc`, `proctop`, `icode/szicode`, `conptr/nlp`),
* `shim.s` (zwei Sprünge: `_save` -> `savu`, `_mchinit` -> `ttyinit`; die Namen im C weichen vom Kernel ab),
* `abs.o` (absolute Symbole `u`, `cfreelist`, Postfach-Puffer ab D256; auf dem Wirt mit `objformat.py` erzeugt),
* ein **Namensabgleich über `-D`** im Makefile (`machdep.c`: `-Dusegm=usrseg`, `conf.c`: `-Dcf_a628=buffers` ...), kein Quelltext geändert.
`Makefile` (erzeugt, im Gast mit `tools/mutos8000/kernel/make/make.bin`): `cd /tmp; /mnt/make -f /mnt/Makefile`.  Datenträger: Quellen+Header (172 KB)
auf D (/mnt), Objekte, `lk` und `cc`-Zwischendateien auf A (/tmp); die Wurzel ist zu klein.

**Stand (gemessen):** `make` im Gast uebersetzt alle 31 C-Dateien und bindet; Bauzeit im Gast **736 s Wanduhr** bei sechs parallelen Laeufen
(allein bei hoher Last des Wirts ueber 25 Min.).  Erster Lauf: `ld` meldete fehlende Marken (`low`-Globale ohne Nachbarmodul, Anfangswerte,
`cf_*`-Namen); behoben im Skript, **der letzte Lauf mit Boot-Nachweis war beim Abschluss dieses Commits noch nicht fertig -- ungeprueft:**
`gesamtbau.py --arbeit DIR --objekte DIR/obj --natuerlich --boot`.  Der natuerliche Kernel hat KEIN `pad`: die BSS schwimmt (ab Ende der Daten),
muss aber unter D256 enden (das Skript prueft das).  Die ps/pstat-Tabelle zeigt dann auf falsche Adressen.

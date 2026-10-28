# K1: Werkzeugkette im MUTOS-Gast (gemessen 2026-10-27)

Aufbau: Ladediskette K5601 (A:), `usr_mfm` in B: (`/usr`, `cc as ld`, `lib/c0 c1 c2 cpp`, `libc.a`, Header), Wurzel in C:, Quelldiskette (26x128 MFM, `mutos8k_26x128_mfm`) in D: (`/mnt`).
Skript: `tools/mutos8000/kernel/probe/cc_probe.dbg`.  `/etc/mount /dev/rk1 /usr`, `/etc/mount /dev/rk3 /mnt`, `PATH=/bin:/usr/bin:.`.

| Schritt | Maschinentakte | bei 2,4576 MHz |
|---------|----------------|----------------|
| `cc -c t2.c` (9 Zeilen) | 194 Mio. | ~79 s |
| `cc -c t3.c` (6 Zeilen, `#include <stdio.h>`) | 180 Mio. | ~73 s |
| `cc t2.o t3.o` (Binden mit `libc.a`) | 137 Mio. | ~56 s |
| `a.out` (`printf`) | 12 Mio. | ~5 s |

**Ergebnis: die ganze Kette uebersetzt, bindet und laeuft im Gast** (`sum=10`, `a.out` 7142 B).  Der MUTOS-`cc` ist ein V7-Compiler im Zilog-Stil:
`cc -S` liefert nichtsegmentierte Assemblerquelle (`call csv`, `jp cret`, `push @-SP,#_tab`, `end _main`), `cc -c` ein `.o` (320 B), ein `cc a.c b.c` loescht die `.o` wieder.

Befunde:
* **Zeit pro Aufruf ist fix und hoch:** ~1,2 Min. Rechenzeit schon fuer ein Neun-Zeilen-Programm (Pass-Start und Floppy-Lesen der Passen `c0 c1 c2 as` dominieren).
  Ein Kernel aus einigen Dutzend Dateien waere auf Originaltakt Stunden; im Emulator laeuft die Maschinenzeit schneller (Wanduhr noch nicht gemessen).
  Bau in Teilschritten (`-c` je Datei, nur Geaendertes) ist Pflicht, deshalb `make` (oder Skript mit Zeitstempelvergleich).
* **Mit der Wurzel mitgelieferte Programme reichen nicht:** `echo` fehlt in `/bin` (nur `cat cp date ls mkdir mv rm sh stty sync`, `/usr` hat nur `as cc ld`).
  Die Marke `ENDE` geht daher mit `cat datei`; `cmd`-Diskette (`ar cat chmod cp dd echo ed ls mv nm size strip wc …`) liegt als weitere Datendiskette bereit.
* **`.` steht nicht im PATH** der Wurzel; `a.out` nur als `./a.out` bzw. mit `PATH=…:.`.
* **Tastatur:** `keys` zu frueh nach einem Befehl verschluckt Zeichen (`cd # /mnt` im Echo); nach jedem `keys`-Befehl mit `g` oder `gscreen` den Prompt abwarten; Wartemarken muessen eindeutig sein (Bildinhalt bleibt stehen).
* Offen: Wanduhrzeit der Emulation; `cc` an einer groesseren Datei (V7-`make`), ob `libc` die `printf`-Formate des Kernels deckt, die Frage nach dem Compiler des Originalkernels (K6 §5.3: Muster-Vergleich `cc -S` gegen Kernelcode).

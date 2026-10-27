# MUTOS-Kernel: Befund K0 (2026-10-27)

Gemessen an den Disketten-Archiven (`disks/a5120_mutos8000/…_best_{usr,cc1,cc2,bin}.zip`), der WEGA-3.1-Platte und `OlliL/P8000`.

## Werkzeuge auf den MUTOS-Disketten

* `bin/cc as ld`, `lib/c0 c1 c2 cpp`, `crt0.o`, `libc.a`: alle `a.out` mit Magic `E807` (`A_MAGIC1`), Kopf 8 Worte
  (`a_magic a_text a_data a_bss a_syms a_entry a_unused a_flag`, 16 B), `a_flag = 1` (gestrippt, keine Symbole).
  Symbole: `struct nlist` mit 8-Byte-Namen, Typ `N_UNDF…N_BSS`, `N_EXT = 0x20` (`include/a.out.h`).
* **Alle Werkzeuge laufen nichtsegmentiert** (Z8002-Modus).  `c1` erzeugt nur `$NONSEGMENTED`; `as` und `ld` kennen die Direktiven
  `$NONSEG[MENTED]` und `SEG`, `ld` schreibt `$SECTION %s_d` (Zilog-Modulformat).
* Es gibt auf den Disketten **kein `scc`, `sld`, `cas`, `scrt0.o`, `slibc.a`**.

## Der Kernel ist segmentiert

`mutos_1216`: Vektortabelle mit 4-Wort-Einträgen (`FCW C000`, Segment `0000`, Offset), also Z8001-Modus (Entwurf 39).

**Folge: der MUTOS-`cc` von den Disketten kann diesen Kernel nicht erzeugt haben.**  Er stammt aus einer segmentierten Kette
(Zilog-/WEGA-Familie: `scc` → `cas` → `sld`).  Der WEGA-Kernel wird so gebaut (`uts/conf/Makefile`: `scc`; `make.wega`:
`sld -Ns -o wega -e start -X -i -Ms62 wpar.o event.o mch.o u.o ver.o fpe.o conf.o ../sys/LIB1 ../dev/LIB2`; `mch.s` beginnt mit `$segmented`).

## Referenz-Werkzeuge: WEGA-3.1-Platte

`disks/p8000_wega31/p8000_wega31_platte.avr.img.gz`, Teil `md2`: `bin/{as,cas,cc,ld,scc,sld}`, `lib/{cpp,crt0.o,scrt0.o,libc.a,slibc.a}`;
Handbuch `md0/man/man1/{as,cas,cc,ld,scc,sld}.1`; Kernelquellen `md0/sys/conf/{wega,make.wega}`.  Läuft im Emulator (`tools/dev.sh test-wega`).
Das ist die **einzige bekannte segmentierte Kette** — sie ist der Kandidat für Stufe A/C (nicht die MUTOS-Disketten).

## Offen / zu messen
* Ob `scc`/`cas`/`sld` aus WEGA 3.1 den MUTOS-Kernel bytegleich erzeugen (gleiche Familie?) — Test: WEGA-Modul bauen, mit Funktionen in `mutos_1216` vergleichen.
* Ob `mutos_1216` aus WEGA-ähnlichen Quellen stammt (Abgleich der Funktionen, K6 §5.3).

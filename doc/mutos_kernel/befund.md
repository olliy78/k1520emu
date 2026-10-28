# MUTOS-Kernel: Befund K0 (2026-10-27)

Gemessen an den Disketten-Archiven (`disks/a5120_mutos8000/…_best_{usr,cc1,cc2,bin}.zip`), der WEGA-3.1-Platte und `OlliL/P8000`.

## Werkzeuge auf den MUTOS-Disketten

* `bin/cc as ld`, `lib/c0 c1 c2 cpp`, `crt0.o`, `libc.a`: alle `a.out` mit Magic `E807` (`A_MAGIC1`), Kopf 8 Worte
  (`a_magic a_text a_data a_bss a_syms a_entry a_unused a_flag`, 16 B), `a_flag = 1` (gestrippt, keine Symbole).
  Symbole: `struct nlist` mit 8-Byte-Namen, Typ `N_UNDF…N_BSS`, `N_EXT = 0x20` (`include/a.out.h`).
* **Alle Werkzeuge laufen nichtsegmentiert** (Z8002-Modus).  `c1` erzeugt nur `$NONSEGMENTED`; `as` und `ld` kennen die Direktiven
  `$NONSEG[MENTED]` und `SEG`, `ld` schreibt `$SECTION %s_d` (Zilog-Modulformat).
* Es gibt auf den Disketten **kein `scc`, `sld`, `cas`, `scrt0.o`, `slibc.a`**.

## Der Kernel laeuft NICHTSEGMENTIERT (Berichtigung, gleicher Tag)

Erste Fassung dieses Befunds hielt ihn wegen der 4-Wort-Vektortabelle fuer segmentiert -- falsch.  Das Z8001 liest seine PSA immer im 4-Wort-Format;
entscheidend ist das FCW-Bit 15 (SEG) des Eintrags.  Gemessen an `mutos_1216`:
* Eintrag 0 (Reset): FCW `C000` (System + SEG) -> `027E`.  Alle Trap-Eintraege (`0008`..`0030`): FCW `4004`, `4008`, `4018`, `4024`, `4028`, `400C` = SEG **geloescht**,
  alle auf dieselbe Routine `030E` (die Low-Bits sind vermutlich eine Trap-Kennung -- noch zu belegen).
* Alle 996 `CALL`-Befehle (`5F00`) im Code haben die Kurzform ohne Segmentwort; Speicherbedarf endet bei `CEFF` = ein 64-KB-Raum.
* Der Rechner hat keine MMU; das Z8001 laeuft ohne MMU auch segmentiert (Segmentbits direkt auf dem Bus) -- die fehlende MMU entscheidet nichts.

**Folge:** die MUTOS-Werkzeuge (`cc`, `as`, `ld`, nichtsegmentiert) sind voraussichtlich die richtige Kette; WEGA `scc/cas/sld` ist nur Vergleichsreferenz.
Offen: segmentierter Start-Stub (`027E`..) und wie der Kernel die 192 KB (`mem = 196608`) ohne MMU erreicht (EM-Speichermodell, Entwurf 17).
WEGA 3.1 hat `scc/sld/cas` (segmentiert) und `cc/ld/as` (nichtsegmentiert) -- beide auf `md2` der Platte, falls ein Vergleich noetig wird.

## Offen / zu messen
* Ob `scc`/`cas`/`sld` aus WEGA 3.1 den MUTOS-Kernel bytegleich erzeugen (gleiche Familie?) — Test: WEGA-Modul bauen, mit Funktionen in `mutos_1216` vergleichen.
* Ob `mutos_1216` aus WEGA-ähnlichen Quellen stammt (Abgleich der Funktionen, K6 §5.3).

## WEGA-Werkzeuge sind NICHT die MUTOS-Werkzeuge (gemessen, 2026-10-27)

Vergleich `md2/{bin,lib}` der WEGA-3.1-Platte gegen die MUTOS-Disketten: alle Dateien verschieden (cc 14208 gegen 6536 B, as 74536 gegen 35814, ld 21732 gegen 10726).
* **WEGA:** a.out mit Magic `E707`/`E711`, 32-Bit-Groessenfelder; `cc` ist ein Zilog-Treiber ueber `/lib/{cpp,cparse,gopt,codgen,popt,clist}` und `/bin/cas`.
* **MUTOS:** Magic `E807`, 16-Bit-Felder (16-B-Kopf), V7-/PCC-Aufteilung `cpp, c0, c1, c2, as, ld`.
* Zwei verschiedene Werkzeugfamilien: MUTOS-Binaerdateien laufen nicht unter WEGA (anderes Kopfformat), WEGA-`cc` erzeugt anderen Code.
  WEGA taugt als **Quellreferenz** (Namen, Struktur), nicht als Bausystem fuer den MUTOS-Kernel -- ausser ein Test zeigt bytegleiche Ergebnisse.

## Speichermodell: Prozess = eigenes 64-KB-Segment (gemessen 2026-10-27)

* EM nach dem Boot: `A33=C0` = **Segmentweiche Modus 2: die Segmentnummer kommt von den SN-Leitungen des U8001** (`dev em`).
  Ein Z8001 im nichtsegmentierten Betrieb gibt auf SN die Segmentnummer seines PC aus; ein Prozess erreicht nur sein Segment,
  ein Wechsel (`ldctl` FCW, segmentiertes `iret`) ist privilegiert.  Das wirkt wie eine grobe Speicherverwaltung ohne MMU.
* `cover` im U8001-Kontext waehrend `ls -l /bin | cat` (Segment, S/N, Adressen, Befehle):
  `00 S 4828 / 874860` (Kernel) — `01 N 1382 / 18787`, `02 N 988 / 7031`, `03 N 2766 / 55084` (drei Anwenderprozesse: sh, ls, cat).
* `mem = 196608` = 3 x 64 KB = Segmente 1–3 fuer Anwender; Segment 0 = Kernel (System, Vektoren mit Segment 0).
* Folge: bis zu drei Prozesse liegen gleichzeitig im Speicher und wechseln ohne Plattenzugriff; ab dem vierten lagert `sched`/`swap`
  ganze Prozesse nach rk2 ab Block 250 aus.  Geteilte Texte gibt es nicht (`xlock`/`xunlock` sind Leerfunktionen, `expand` ebenso).
* Offen: ob ein Prozess immer ein ganzes Segment belegt (Karte `malloc` in welcher Einheit), wie `copyin`/`copyout` das Fremdsegment erreichen
  (vermutlich kurz segmentiert mit `<<seg>>`-Adresse), Swap-Verhalten mit vier und mehr Prozessen gemessen.

## Geraetetabellen und Treiber (aus dem Abbild gelesen, 2026-10-28)

`bdevsw` 8766 (8 B je Eintrag) und `cdevsw` 8796 (14 B), abgeglichen mit den Knoten in `/dev` der Wurzel:

| Tabelle | Major | Treiber | `/dev` | im Lauf |
|---------|-------|---------|--------|---------|
| bdevsw | 0 | `fd*` (7E16..85B0, Tab 8E6A) | — | nie |
| bdevsw | 4 | `rk` (Disketten ueber `mutos_io`) | rk0-3, swap | ja |
| cdevsw | 0 | Konsole + tty1 (60CC..) | console 0,0; tty1 0,1 | Konsole ja, tty1 nein |
| cdevsw | 1 | 6C8E..6D12 | tty 1,0 | — |
| cdevsw | 2 | open/close `nulldev`, read/write ueber `passc`/`cpass` (69CE/6A52) | mem 2,0; kmem 2,1; null 2,2 | — |
| cdevsw | 5 | `lp` (6444..690A) | lp/lp0 5,0; rlp/rlp0 5,4 | nie |
| cdevsw | 10 | `fd` roh (84E4/851C ueber `physio`) | — | nie |
| cdevsw | 13 | `rk` roh | rrk0-3 | ja |
| cdevsw | 15 | `ttopen/ttread/ttwrite`, Zusatzworte 7222/7C62 | — | nie |

**Berichtigung zu `kernel_namen.md`:** nach `/dev` ist cdevsw[1] = `/dev/tty` (Kontrollterminal, V7 `sy`), cdevsw[2] = `mem` (V7 `mmread` arbeitet mit `passc`/`cpass`);
die Namen `mm*` (6C8E..) und `dev2*` (69CE..) sind vertauscht, `lp` ist cdevsw[5], nicht [7].  Der `fd`-Treiber benutzt `mapalloc` (also ebenfalls die Mailbox zur U880-Seite);
welches Geraet er bedient, ist offen.

## Schutzversuch: koennen sich Prozesse ohne MMU gegenseitig ueberschreiben? (gemessen 2026-10-28)

`tools/mutos8000/kernel/versuch_schutz/` (`wache.c`, `schreib.c`, `lauf.sh`): zwei `wache` im Hintergrund (4-KB-Puffer mit 5A, Pruefung alle 2 s),
dann `schreib`, das alles zwischen seinem Datenende und seinem Stapel mit AA fuellt.  Ergebnis am Bildschirm:
```
wache A pid=22 buf=12C6..22C5 sp=FFB6
wache B pid=23 buf=12C6..22C5 sp=FFB6
schreib pid=24 end=1534 top=FEB6
schreib: 59778 Bytes ueberschrieben
wache B: heil
```
(`wache A` war schon vor `schreib` fertig; `wache B` lief waehrend `schreib` und blieb heil.)  Abdeckung je Segment: S0 Kernel, N1/N2/N3 Anwender.

**Befund:** jeder Prozess hat den GANZEN 64-KB-Raum seines Segments (Stapel bei FFB6, beide `wache` liegen auf denselben Adressen), ein zweiter Prozess
kann also nicht im selben Segment liegen.  Die Vermutung "zwei kleine Prozesse teilen sich ein Segment und koennen sich ueberschreiben" ist **widerlegt**:
Prozesse sind gegeneinander geschuetzt, weil das Segment nur der Kernel wechseln kann.  Die Vergabe in 256-Byte-Stuecken (`coremap`) dient der Groesse,
nicht dem Teilen.  Folge: hoechstens drei Prozesse gleichzeitig im Speicher, jeder weitere wird ganz ausgelagert (hier: `init`, `sh`, zwei `wache`, `schreib`).
Nicht geprueft: Schutz des Kernels vor einem Prozess (Segment 0 ist fuer N-Betrieb nicht erreichbar, E/A-Befehle sind privilegiert -- erwartet, nicht versucht).

## Belegung des Kernelsegments 0 und freier Platz (gemessen 2026-10-28)

| Bereich | Groesse | Inhalt | Beleg |
|---------|---------|--------|-------|
| 0000-8DC0 | 36 288 B | Text + initialisierte Daten (Datei 36 352 B mit Fuellung bis 8E00) | Linkkarte K5 |
| 8DC0-D532 | 18 290 B | BSS: u. a. Puffer 20 x 512 (A628..CE28), Pufferkoepfe 20 x 24, Inodes 50 x 74 (907A..9EEE), Dateien 50 x 8, proc 30 x 28 (A11C..), mount, coremap/swapmap, slpque, Zeichenlisten, tty | `start` loescht 8DC0..D586; K6b/K7 |
| D532-D586 | 84 B | Postfach (Adresse fest in `mutos_io`) | linkbau.md |
| **D586-EC00** | **5 754 B** | **unbenutzt** | `start` loescht ihn nicht; kein Kernelbefehl adressiert ihn; Speicherabzug nach Boot und nach `cc`/Pipe/`schreib` byteweise gleich |
| EC00-FC00 | 4 096 B | ausgelagerte u-Bereiche (4 x 1 KB, `resume` 0620: EC00 + Index x 400H) | Disassemblat |
| FC00-FFFF | 1 024 B | `u` des laufenden Prozesses samt Kernelstapel (R15 startet bei 0) | `start`, `resume` |

Frei sind also **5 754 B** zusammenhaengend oberhalb des Postfachs, dazu 64 B Fuellung am Abbildende.  Das reicht z. B. fuer **10 weitere Puffer**
(je 512 B Daten + 24 B Kopf = 5 360 B, also 30 statt 20) oder ~75 weitere Inodes.  Ein Ausbau muss das Postfach bei D532 festhalten: die zusaetzlichen
Tabellen gehoeren HINTER das Postfach (eigener Abschnitt ab D586), nicht an das Ende des BSS.

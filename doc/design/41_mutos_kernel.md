# 41 — MUTOS-Kernel `mutos_1216`: Rückwärts durch die Werkzeugkette (Konzept)

Stand 2026-10-27, Zweig `MUTOS8000`.  **Nur Konzept** — nichts umgesetzt, Entscheidungen des Anwenders stehen in §9 aus.
Bezug: Entwurf 39 (MUTOS auf dem A5120.16), Entwurf 40 (`mutos_io`, gleiche Methode für die U880-Seite).

## 0. Primärziel: MUTOS baut sich selbst (Anwender, 2026-10-27)

Das erste Ziel ist **nicht** ein Linux-Rückbau, sondern **Selbsthosting**: der Kernel `mutos_1216` entsteht **unter MUTOS8000 selbst** aus Quellen, mit `cc`, `as`, `ld`, `ar`, `sh`.
Linux-Werkzeuge (Disassembler, Abgleich, ggf. Interpreter) sind nur Hilfsmittel auf dem Weg dorthin, nicht das Ergebnis.  Daraus folgt:

* **Der Quellbaum muss auf MUTOS-Disketten passen und dort übersetzbar sein** (Floppy 26×128 = 250 Blöcke Wurzel, Datendisketten 494 Blöcke; Hauptspeicher 192 KB, je Prozess 64 KB).
  Stufe C ist nicht „nice to have": Assemblerquellen allein (K4/K5) sind groß und der Assembler ist langsam — die Größen- und Zeitmessung gehört früh in K1.
* **Vorhanden unter MUTOS** (gemessen, Befund): `sh cc as ld ar ed cp mv mkdir rm size strip nm cat ls sync tar` (+ `c0 c1 c2 cpp`, `libc.a`).
  **Fehlt:** `make`, `sed`, `awk`, `grep`, `diff`, `cmp` (cmp ist in `cmd` vorhanden, `diff` nicht).  Der Bau läuft also über ein **Shell-Skript** (`build.sh` je Modul, Neubau komplett);
  fehlende Werkzeuge sind selbst Kandidaten für Eigenbau, falls nötig.
* **Verteilung der Quellen:** mehrere Datendisketten (Quellen, Objekte, Ausgabe), Einhängen mit `/etc/mount`; die Datenträger werden mit dem DiskTool befüllt (Linux-Seite ist Hilfe, der Bau selbst läuft im Gast).
* **Abnahme:** im Gast `sh build.sh` → neuer `mutos_1216`; Bytevergleich mit dem Original (`cmp`), dann Boot mit dem selbstgebauten Kernel bis `login:`.
  Zweite Abnahmestufe: der selbstgebaute Kernel baut den Kernel erneut (Fixpunkt).
* **Reihenfolge ändert sich:** K1 (Werkzeuge im Emulator, Zeit/Platz messen) steht vorn und ist **kein Orakel unter mehreren**, sondern der Hauptweg; der Linux-Interpreter (K9) bleibt Beschleuniger für das Matchen, nicht Voraussetzung.
  Neues AP **K11**: Selbstbau-Skript, Quellplatzbedarf, Datenträgerschnitt, Ausführung im Gast.

## 1. Ziel

Vom Kernel `mutos_1216` (Z8000, nichtsegmentiert, 36 352 B, Rohabbild ab Adresse 0) zu einer Quelle, die **mit den originalen
MUTOS-Werkzeugen** (`cpp`, `c0`/`c1`/`c2`, `as`, `ld` von den Disketten cc1/cc2/usr) wieder einen funktionierenden Kernel ergibt.
Drei Stufen, jede für sich nützlich und jede mit einem **mechanischen Maßstab**:

| Stufe | Ergebnis | Maßstab |
|-------|----------|---------|
| **A. Un-link** | Kernel zerlegt in Module als **Assemblerquellen**; `as` + `ld` setzen sie zum Original zusammen | Bau == Originalabbild, **bytegleich** |
| **B. Verstehen** | Jede Funktion benannt, Strukturen/Tabellen beschrieben, Gerätetreiber und Z8000↔U880-Verbindung (Mailbox) erklärt | Kommentierte Quelle; Laufbelege (Trace-Abdeckung) |
| **C. Zurück nach C** | C-Quellen, die der **Original-Compiler** zu denselben Objekten übersetzt | je Modul `.o` == Original-`.o` (nach A), Gesamtbau bootet |

Bytegleichheit ist der Beweismittel-Standard (wie bei `mutos_io`): ohne sie bleibt jede Rekonstruktion eine Vermutung.
Stufe C wird **nicht** für jede Funktion erreichbar sein (handgeschriebenes Assembler, Compiler-Versionsunterschied, Patches im
Original); wo sie scheitert, bleibt die Assemblerquelle aus A stehen — so macht es auch das WEGA-Projekt (§3).

## 2. Ausgangslage (gemessen / belegt)

* **Datei:** `mutos_1216`, 36 352 B, **ohne Kopf**, Ladeadresse 0, Vektortabelle `0000 C000 0000 027E 0000 4004 0000 030E …`
  (Zilog-Z8000-Programmstatusfeld: FCW `C000` = System+segmentiert, Einsprung `0000:027E`; Trap-Einträge `4004 030E`, `4008 030E` …).
  Kein a.out-Kopf, **keine Symboltabelle**, kein Relokationsteil: der Kernel ist ein fertig gebundenes, abgezogenes Abbild
  (`LOAD16` lädt es nur).  Speicherbedarf bis `CEFF` (Entwurf 39 §A).
* **Texte:** `panic: %s`, `%s on dev %u/%u`, `Tzero wchan`, `proc on q`, `Running a dead proc`, `Timeout table overflow`, `no imt`,
  `Inode table overflow`, `iaddress > 2^24`, `trap type %x`, `ident = %x` — **UNIX-Schicht III / Zilog-ZEUS-Erbe**, sehr nahe am
  WEGA-Kernel (§3).  Swap, Gerätenummern (`brw 4,2`), `rkstrategy` und `mutos_io`-Mailbox sind bereits vermessen (Entwurf 39 §11).
* **Bekannte Eingriffe ins Abbild:** `mutos_patch.py` ändert `rootdev`/`pipedev`/`swapdev`, `swplo`/`nswap` (Datei-Offset 0x8884/0x8888)
  — die Kernel-Konfigurationstabellen (`conf.c`) sind also schon lokalisiert.
* **Werkzeugkette liegt auf den Disketten** (Best-of-Sätze `disks/a5120_mutos8000/…_best_*.zip`, fertig in `disks/a5120_16_MUTOS/`):
  `cc1`: `bin/cc as ld ed`, `lib/c0 c1`, Header (`a.out.h`, `symbol.h`, `stdio.h` …); `cc2`: `lib/c2 cpp crt0.o libc.a mcrt0.o`, `bin/nm`;
  `bin`: `adb tar mt`; `cmd`: `ar nm size strip od dd` …  Layout = V7-Cc (`cpp → c0 → c1 → c2 → as → ld`).
* **Emulator-Seite:** A5120.16 mit EM256/U8001 läuft, MUTOS bootet bis `login`, Wurzel + Datendisketten beschreibbar (Entwurf 39).
  Z8000-Werkzeuge im Repo: `core/primitives/z8000.*`, `tools/z8000/z8k_disasm.h`/`z8k_asm.h`, `z8kasm`, Debugger `k1520dbg --em em256`, `cpu u8000`.

> **Befund K0 (2026-10-27, `doc/mutos_kernel/befund.md`):** der Kernel laeuft **nichtsegmentiert** (Trap-FCWs ohne SEG-Bit, 996 Kurz-`CALL`, ein 64-KB-Raum; nur der
> Reset-Eintrag hat `C000`), die MUTOS-Werkzeuge sind nichtsegmentiert -- die Kette `cc/as/ld` von den Disketten ist also voraussichtlich die richtige.
> Die erste Fassung dieser Anmerkung ("segmentiert, WEGA `scc` noetig") war falsch.  Offen: Start-Stub bei `027E`, Zugriff auf 192 KB ohne MMU (Entwurf 17).

## 3. Was es schon gibt — und warum das alles verändert

**Oliver Lehmann (`OlliL/P8000`, `WEGA/src/uts`, siehe Quellen)** hat den **WEGA-Kernel** (Robotron P8000, ebenfalls Z8001, von
Zilog ZEUS abgeleitet) auf genau diesem Weg zurückgewonnen: Original-Objekte disassembliert, in C zurückübersetzt (mit Hilfe der im Netz
verfügbaren Quellen von V7, System III und Plexus), `lock.c` nur als Assembler.  Sein Kernel ist **vollständig übersetzbar und laufkompatibel**.
Er hat zusätzlich (`WEGA/toolchain/HANDOFF_wega_disasm.md`, 2026-10-16) denselben Gedanken für die **Werkzeuge** (`cc`, `scc`, `cas`, `ld`)
angestoßen: Rückübersetzung in C, ausführbar unter Linux, byteweise identisch zur nativen Kette.

Folgerungen für uns:

1. **Referenzquelle statt Rätselraten.**  Der WEGA-Kernel ist vermutlich ein naher Verwandter von `mutos_1216` (gleiche Textmeldungen, gleiche Hardwarefamilie U8001+Z8010-MMU).
   Das Abbild lässt sich Funktion für Funktion **gegen übersetzte WEGA-Objekte matchen** (§5.3): jeder Treffer liefert Namen, Struktur und C-Quelle.
   Abweichungen (Gerät `rk`, Mailbox statt Plattencontroller, Konsole über `mutos_io`) sind dann gerade die MUTOS-Eigenheiten, die zu erklären sind.
2. **Compiler-Verwandtschaft prüfbar:** WEGA-`cc` und MUTOS-`cc` sind vielleicht dieselbe Familie (Zilog-PCC).  Test (§5.1): ein WEGA-Kernelmodul mit dem MUTOS-`cc`
   übersetzen und mit dem WEGA-Objekt vergleichen — gleiche Bytes heißt, dass die WEGA-Quelle den MUTOS-Kernel mitträgt.
3. **Abstimmen statt doppelt arbeiten:** seine Werkzeug-Rückübersetzung ist dieselbe Aufgabe wie unser Schritt „nativer/schneller Compiler".  Zu klären (§9) ist, ob wir zusammenlegen.

## 4. Methodik — die Kette rückwärts

Ein UNIX-Kernel dieser Zeit entsteht so: `*.c` → `cpp` → `c0` (Parser) → `c1` (Codegenerator, Assembler-Text) → `c2` (Peephole) → `as` → `*.o`; `*.s` → `as`;
`ld` bindet in der Reihenfolge der Kommandozeile **Text, dann Daten, dann BSS** (`ld -x -r` für die Teilsätze, danach der Gesamtlauf, `strip`).  Der Rückweg:

```
fertiges Abbild ──(1 Disassemblat)──► eine Assemblerquelle ──(2 Un-link)──► Module je .o ──(3 Rückübersetzung)──► C je Modul
        ▲                                     │ as+ld == Abbild?                │ cc == .o?                        │ cc+ld == Abbild?
        └─────────────────────────────────────┴─────────── Rundlauf-Wächter ──┴──────────────────────────────────┘
```

### 4.1 Disassemblat (Stufe A, Schritt 1)

* Disassembler: **`tools/z80_disasm2.py` hat das Z80-Gegenstück** (Entwurf 40, W1–W3: Segmente, Kartendatei `code/db/dw/str/name/kom`, `--format mac`, Rundlauf `--pruefen`).
  Für Z8000 gibt es `tools/z8000/z8k_disasm.h` (C++-Bibliothek, kein Kommandowerkzeug).  Nach der Regel „ein Disassembler je Ort" wird daraus ein
  **Kommandowerkzeug mit derselben Kartendatei und demselben Rundlauf** (`tools/z8000/z8kdis`): Ausgabe in der Syntax des **MUTOS-`as`** (nicht der Zilog-Syntax von `z8kasm`!), weil nur sie zum Original-`as` zurückläuft.
* **Code/Daten-Trennung** ist das Kernproblem (kein Symbol, kein Relokationsteil).  Quellen: Vektortabelle (Einsprünge), Sprung-/Aufrufziele (`call`, `jp`, `calr`), Textmuster der Compiler-Prologe/-Epiloge
  (`ldm`/`push` am Funktionsanfang, `ret`), Zeigertabellen (`bdevsw`/`cdevsw`/`sysent`), Zeichenketten (`panic`-Argumente), vor allem aber der **Lauf**: `k1520dbg`-Ausführungsprotokoll (welche Adressen
  wurden beim Boot, bei `ls`, `cc`, `ed`, Swap … ausgeführt) als Beweis „Code", der Rest bleibt Kandidat.

### 4.2 Un-link (Stufe A, Schritt 2) — der eigentliche Gewinn

Der Linker hat Module in Reihenfolge hintereinandergelegt und Adressen eingesetzt.  Rückgängig machen heißt: **Modulgrenzen finden** und **Adressfelder von Konstanten unterscheiden**.

* **Modulgrenzen:** (a) Reihenfolge aus der Referenz (WEGA `Makefile`/`conf/Makefile`: `mch.s` + `sys/*` + `dev/*` + `conf.c` …), (b) Funktionsfolge im Text — im Kernel stehen die Funktionen eines Moduls zusammenhängend,
  die Datenteile (initialisierte Tabellen, Strings) aller Module später im Datenbereich **in derselben Modulreihenfolge**, (c) Struktur-Wiederholungen (jedes Modul hat typisch eigene lokale Stringpools),
  (d) Abgleich mit dem WEGA-Objektsatz (Funktionsreihenfolge je Datei).  Ergebnis: eine **Linkkarte** (Modul → Text-/Daten-/BSS-Bereich).
* **Adressfelder:** jedes Wort im Text, das auf einen Text-/Datenbereich zeigt, wird zur **Marke** (Relokation), jedes andere bleibt Konstante.  Fehlklassifikation ist hier der teuerste Fehler — sie fällt sofort beim Rundlauf auf (Bytegleichheit), aber
  *zufällig* gleiche Zahlen (Konstante, die wie eine Adresse aussieht) fallen erst bei einer **Änderung** auf (Verschiebung eines Moduls).  Gegenmaßnahme: **Verschiebe-Test** — jedes Modul um n Byte verschoben bauen und mit dem Verhalten (Boot, Gerätelauf) abgleichen.
* **Handassembler vs. Compiler:** Module aus `mch.s`-Art (Trap-Eintritt, Kontextwechsel, `copyin`/`copyout`, Segment-/MMU-Zugriffe) sind **kein** Compiler-Erzeugnis und werden Assembler bleiben.

### 4.3 Verstehen (Stufe B)

Parallel, nicht danach: ohne Verständnis ist keine Marke benennbar.  Werkzeuge: `k1520dbg --em em256` (Haltepunkte, `hist`, Ausführungsabdeckung, `itrace`, Speicherblick auf `u`/`proc`/`inode`),
`adb` **in MUTOS selbst** (auf der `bin`-Diskette; `/dev/kmem` liest Strukturen am laufenden System), Querbezug zu `mutos_io` (Mailbox `D532`) und Entwurf 39 §11.  Schwerpunkte, die MUTOS vom WEGA-Kernel abheben dürften:
Plattentreiber `rk` (Block→Mailbox statt Controller), Konsole/`tty` über `mutos_io`, Swap auf `rk2`, Uhr, Speicherverwaltung (Z8010).

### 4.4 Rückübersetzung nach C (Stufe C)

Bottom-up und **Match-getrieben** (wie die bekannten „matching decompilation"-Projekte): je Funktion C-Entwurf → Original-`cc` → Vergleich mit dem Assembler aus A → Anpassung, bis die Bytes gleich sind.
Reihenfolge: Blätter (kleine Hilfen: `bcopy`, `min`, `sleep`-Teile), dann Tabellen (`conf.c`), dann Kern (`slp`, `iget`, `alloc`), zuletzt Treiber.  C-Quellen von V7/System III/WEGA liefern die Struktur; die **Registerbelegung** des Compilers
entscheidet über Bytegleichheit, deshalb zuerst Compiler-Eigenheiten (Registerfolge, Konstantenbehandlung, Optimierer `c2`) an Beispielen kennenlernen (§5.1).  Wo c2 oder die Zufallsfolge nicht trifft: Funktionsgröße prüfen, Assembler als Rückfall.

## 5. Werkzeugkette und Prüfstand

### 5.1 Die Originalwerkzeuge zum Laufen bringen (Phase 0/1)

Ohne Orakel ist alles Rätselraten.  Zwei Wege, beide vorbereiten, einer wird Standard:

* **(i) Im Emulator**: A5120.16 booten (Ladediskette + Wurzel + cc1/cc2/usr), Quellen per DiskTool auf eine Datendiskette bringen, `cc -c`, `as`, `ld` in MUTOS laufen lassen, `.o` zurückholen.  Vorteil: ist garantiert das Original.  Nachteil: Geschwindigkeit
  (zu messen!) und manuelle Handgriffe — wird durch eine **Stapelsteuerung** (`k1520dbg -x`/Python `K1520Emulator` mit Tastaturstrom, `typeString`) und Austausch per Diskettenabbild automatisiert.  Das ist die verlässliche Referenz.
* **(ii) Nativ auf Linux** (später): Z8000-Benutzermodus-Interpreter mit MUTOS-Systemaufrufen, der `cc`/`as`/`ld` direkt ausführt (Lehmanns Vorschlag Nr. 4, `HANDOFF_wega_disasm.md`) — schnell genug für tausende Probeübersetzungen im Matchen.
  Der Interpreter ist klein (Z8000-Kern haben wir, MUTOS-Syscalls sind V7-nah), setzt aber voraus, dass (i) die Referenz geliefert hat.
* **Vorversuche (billig, früh):** a) `cc -S` an einer kleinen Datei, Ausgabe lesen; b) ein WEGA-Kernelmodul (`iget.c`) mit MUTOS-`cc` bauen und mit dem WEGA-`.o` (aus der WEGA-Platte `disks/p8000_wega31`) vergleichen (Compiler-Verwandtschaft!);
  c) `as` + `ld` auf das eigene Z8000-Testprogramm (`tools/em256`) loslassen und `a.out`-Format/Segmentierung (`-s`? Kernel-Flags?) klären.

### 5.2 Rundlauf-Wächter

`tools/mutos8000/kernel/build.py --check` (wie `mutos_io/build.py`): baut aus Quelle, vergleicht mit `mutos_1216`; Exit 77 ohne Werkzeugkette.  Jeder Zwischenstand der Phasen unten besteht ihn, sonst wird nicht eingecheckt.
Gleiches Muster für Module (`--modul iget`) und, ab Stufe C, je Funktion.

### 5.3 Abgleich mit WEGA (Namens- und Quellgewinn)

Funktionssignaturen (normiert: Adressfelder maskiert, Registernamen kanonisiert, ggf. nur Befehlsfolge ohne Operanden) aus dem WEGA-Kernel gegen `mutos_1216` — Treffer = gleiche Funktion; Beinahe-Treffer = Fassung/Patch/Compilerunterschied.
Auch ohne Symbole im Abbild lässt sich so ein Großteil benennen.  Abgleich im Werkzeug festhalten (nicht als Einmalskript).

## 6. Arbeitspakete

Jedes AP endet mit Dokumentation + Commit; Abnahme = der Wächter in der Spalte.

| AP | Inhalt | Ergebnis / Wächter |
|----|--------|--------------------|
| **K0** | Bestandsaufnahme: Abbild vermessen (Segmente/Bereiche, Vektortabelle, Textpool, Zeigertabellen), Header `a.out.h`/`symbol.h` der Disketten lesen, Werkzeugversionen (`what`/Strings in `cc`,`as`,`ld`,`libc.a`) und Plattform-Angaben ermitteln, WEGA-Repo + Platte als Referenz einrichten | `doc/mutos_kernel/befund.md` |
| **K1** | Werkzeugkette im Emulator: Quelltext rein, `.o` raus, automatisiert; Messung der Übersetzungsdauer; Vorversuche a–c | `tools/mutos8000/kernel/cc_im_emulator.py`, Probeübersetzung bytegleich reproduzierbar |
| *K1 Stand 2026-10-27* | cc/as/ld laufen im Gast (`doc/mutos_kernel/k1_cc_im_gast.md`): ~1,2 Min. Maschinenzeit je Kleinstdatei, Binden ~1 Min. | `tools/mutos8000/kernel/probe/cc_probe.dbg` |
| **K2** | `a.out`-/Objektformat lesen (`ld`-Eingabe/-Ausgabe, Symbol-/Relokationsformat), Leser/Schreiber in Python; **Linkkarte des Kernels** aus Abbild + WEGA-Reihenfolge (Hypothese) | `tools/mutos8000/kernel/objformat.py`, Test mit den Objekten aus `libc.a` (erstes Un-link-Übungsstück) |
| **K3** | Z8000-Kommandodisassembler (`tools/z8000/z8kdis`): Karte, Segmente, `--format as` (MUTOS-`as`-Syntax), Rundlauf `--pruefen`; Prüflinge: ein `crt0.o`, `libc.a`-Mitglieder, ein kleines Testprogramm | Tests `tests/python/test_z8000_dis.py` (Befehlsvollständigkeit gegen Tabelle, Rundlauf) |
| **K4** | Kernel als rohe Assemblerquelle (eine Datei, `ORG`/Segmente), `as`+`ld`-Rundlauf == Abbild (**Entwurf-40-M2-Stand**) | `build.py --check` |
| **K5** | Un-link: Module, Marken statt Adressen, Linkkarte, Verschiebe-Test; Handassembler-Module (`mch`-Art) getrennt | je Modul `.s`, Gesamt-Rundlauf bytegleich |
| **K6** | Namen und Kommentare (Karte): Abgleich mit WEGA (§5.3), Lauf-Abdeckung (`k1520dbg`), `adb` am laufenden System; Etappen wie `mutos_io` M4 (Start/Trap/Zeit → Prozess/Swap → Dateisystem/`iget`/`alloc` → Treiber `rk`/`tty` → Konfiguration) | kommentierte Quelle + Kapitel in diesem Entwurf |
| **K7** | Compilerkunde und Rückübersetzung nach C, Blätter zuerst; Match je Funktion gegen K5-Objekt | je Funktion Match-Quote (Liste), Modul `.c` → `.o` == Original-`.o` |
| **K8** | Gesamtbau aus C (Rückfälle in Assembler), Boot im Emulator bis `login:`, Swap-/Mehrbenutzertest; danach erst Änderungen (z. B. `mutos_patch`-Inhalte als Quellvarianten, Wurzel in rk0, Mehrbenutzer/tty1 — Entwurf 39 „Offen") | Wächter `cli_mutos_kernel_bau` |
| **K11** | Selbstbau im Gast: `build.sh`, Datenträgerschnitt (Quellen/Objekte/Ausgabe), Zeitmessung, Fixpunkt-Test (§0) | Wächter `cli_mutos_selbstbau` |
| **K9** | Nativer Z8000-Benutzer-Interpreter für die Werkzeuge (optional, §5.1 ii) | Beschleunigung des K7-Matchens |
| **K10** | Merkposten `doc/merkposten/mutos_kernel.md`, `CLAUDE.md`-Eintrag, `README`, `test_auswahl.py`-Label | |

Reihenfolge: K0 → K1 (entscheidet über alles Weitere) → K2/K3 parallel → K4 → K5 → K6/K7 abwechselnd.  Delegation (Projektregel): Disassembler, Objektleser, Abgleichwerkzeug und Matchen einzelner Funktionen an Sonnet-Agenten (`cpp-coder`/`boot-disasm-analyst`),
Linkkarte und Entscheidungen bei uns; Bau-Kollisionen beachten (Worktrees).

## 7. Risiken

* **Compiler/Kernel passen nicht zusammen:** der MUTOS-`cc` (Disketten, 1986/87) kann eine andere Version sein als die, die den Kernel erzeugt hat (bei WEGA vermutet).  Dann ist Stufe C nur „funktional gleich", nicht bytegleich — Stufe A bleibt gültig.  Früh messen (K1).
* **Kernel von Hand verändert:** Eingriffe wie das Setzen der Gerätewörter ohne Neubau.  Der Befund stützt sich dann auf Abbild, nicht auf Quelle; Hinweise liefert nur der Abgleich mit WEGA.
* **Reine Zufall-Gleichheit:** eine Zahl, die wie eine Adresse aussieht (siehe §4.2), verfälscht erst nach Verschiebung — Verschiebe-Test ist Pflicht, nicht Kür.
* **Geschwindigkeit der Emulator-Kette** (K1): MUTOS-`cc` auf einem U8001 mit Floppy-Platten könnte pro Datei Minuten brauchen.  Gegenmaßnahme K9.
* **Rechtliches:** Kernel, Werkzeuge und Header sind fremder Code.  Wie bei `mutos_io`/`doc/EPROMS`: Arbeitsgrundlage im Repo, kein Weiterverkauf; **Veröffentlichung der Rekonstruktion ist eine Anwenderentscheidung** (§9), die Rückübersetzung nach WEGA/ZEUS-Erbe hat zudem Lizenzfragen (System III, Zilog).
* **Umfang:** 36 KB Code sind überschaubar (Größenordnung 400–600 Funktionen), die Kunst liegt im Beweis, nicht im Volumen.

## 8. Recherche — Fundstellen

* WEGA-Kernel-Rückübersetzung und Handoff: <https://github.com/OlliL/P8000/tree/master/WEGA> (`src/uts`, `toolchain/HANDOFF_wega_disasm.md`); Aussagen dort: Quellen teils im Original, teils aus Objekten rückübersetzt, `lock.c` nur Assembler, `u.s` (Segment 62, `$ABS` nur in Segment 1) und einige Objekte weichen ab.
* Hintergrund zu Z8000-Unix (ZEUS, Onyx, V7-Portierungen, Rückübersetzung von System III): TUHS-Liste, Threads „Unix on Zilog Z8000?" (<https://www.tuhs.org/pipermail/tuhs/2020-January/020192.html>) und „Introduction" (<https://www.tuhs.org/pipermail/tuhs/2008-June/004962.html>).
* Allgemeine Quellen für die Referenzstruktur: V7/System III-Kernel (`slp.c`, `iget.c`, `alloc.c`, `main.c`, `sysent.c`) im TUHS-Archiv; Lions-Kommentar (V6) für die Gesamtstruktur.
* Lokal: `~/projects/A5120.16/docs/MUTOS8000_Teil_II_abschn3_adb.pdf`; Entwurf 39 (Gerätenummern, Mailbox, Swap), `doc/p8000/` (Z8000-/MMU-Beschreibungen), `doc/design/25_p8000.md` (WEGA-Kernel läuft bei uns).
* **Noch nicht eingesehen** (K0): Sourcetexte des WEGA-`uts` im Detail, `adb`-Handbuch, Z8000-Assembler-Syntax des MUTOS-`as`, MUTOS-Handbuch-Teile zu `cc`/`ld`.  Alles in diesem Entwurf, was darüber hinausgeht, ist Annahme und im Text als „vermutlich" gekennzeichnet.

## 9. Zu klären (Anwender)

1. **Maßstab:** bytegleich (Beweis) oder „funktional gleich" genügt? Empfehlung: bytegleich für A, Match-Quote für C.
2. **Umfang des Abbilds:** nur `mutos_1216` (dieser Stand 1986) oder auch die andere Fassung (Foto `MUTOS8000_4_k.jpg`, ältere `mutos_io`-Fassung)? Sind weitere Kernelabzüge vorhanden?
3. **Abstimmung mit Oliver Lehmann:** Nutzung seiner `uts`-Quellen als Referenz erlaubt/erwünscht? Zusammenarbeit bei der Werkzeug-Rückübersetzung (gleiches Ziel, sein Handoff vom 2026-10-16)?
4. **Referenzmaterial:** gibt es Handbuchteile zu `cc`/`as`/`ld`/`a.out` für MUTOS (außer `adb`-Teil)? Ein WEGA-Kernel mit Symbolen (`/unix`) und das Objektarchiv zum Abgleich (`disks/p8000_wega31` vorhanden — reicht das)?
5. **Ausführungsweg:** zuerst im Emulator (K1, langsam, sicher) und später nativ (K9), oder Interpreter gleich vorziehen?
6. **Veröffentlichung:** bleibt alles im privaten Zweig / in `doc/` als Arbeitsgrundlage, oder ist Veröffentlichung geplant (Lizenz)?
7. **Priorität** gegenüber Entwurf 40 (`mutos_io` M4–M6) und den offenen Punkten aus Entwurf 39 (rk0-Wurzel, Mehrbenutzer): sequenziell oder parallel (Agenten)?

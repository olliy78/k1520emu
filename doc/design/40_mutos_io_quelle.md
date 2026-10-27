# 40 — `mutos_io`: Quelle rekonstruieren, kommentieren, neu bauen (Plan)

Stand 2026-10-27, Zweig `MUTOS8000` (nach Merge von `main`).  **Nur Plan** — noch nichts umgesetzt.
Bezug: Entwurf 39 (§4.1, §4.6.3, §11 kennen `mutos_io` schon in Teilen, aber nur als Patchliste).

## Ziel

`mutos_io` (U880-Seite von MUTOS 8000: Disketten- und Konsolentreiber des Z8000-Kernels, UDOS-Programm,
Ladeadresse 9000H) liegt als **kommentierte, wieder assemblierbare Quelle** vor.  Eine Änderung an der
Quelle führt über einen Bau zu einer `mutos_io`-Datei, die per vorhandener Ladedisketten-Pipeline auf die
UDOS-Ladediskette kommt.  Maßstab: **der Erstbau ohne Änderung ist bytegleich** zur Originaldatei
(Archiv `disks/a5120_mutos8000/a5120_mutos8000_boot_udos.zip`, `dateien/mutos_io`).

## Befund zum Ausgangszustand (gemessen)

* Datei 5376 B, vier Segmente `9000+022D, 9400+0EB0, A400+01A8, C8B6+00B2` (`.fileinfo`: `segs=`,
  `mem=9000:CB68:0080`, `zusatz=…`).  Dateioffsets 0x000 / 0x300 / 0x1200 / 0x1400: **jedes Segment ist auf
  256-B-Sektorgrenzen aufgefüllt**, die Segmentlängen stehen nur in `segs=`, nicht in der Datei.
* Bereits bekannt (Entwurf 39 §11.1/§11.2): Mailbox `D532`, Umrechnung Block → (Zylinder, Sektor) `9800–9966`,
  Aufruf des UDOS-Nukleus über `CALL 0BFDH`, Rückruf `9893`, Init `919A`, Bedienmaske `A12A[4]`.
  Der Rest (Konsole, Verbindung zum EM, Maskenabfrage, Segmente 9400/A400/C8B6) ist **unkommentiert**.
* **Bau und Ablegen existieren schon:** `tools/mutos8000/bau_ladediskette.py` baut die Ladediskette aus dem Archiv;
  eine Datei im Archiv unter `dateien/` zu tauschen genügt (Entwurf 39 §A6, `mutos_patch.py` macht das für
  Byte-Patches).  Neu nötig ist nur: Quelle → `mutos_io`-Datei.
* **Z80-Werkzeugkette:** `tools/cpm_bau.py` (M80 + LINKMT über `cparun`, aus `~/projects/CPA_Workbench/tools`,
  `CPA_TOOLS=…`; eingecheckte Ergebnisse, `--check` bytegleich, Exit 77 = Werkzeugkette fehlt) — gleiche Bauart
  wie SERTEST/ROMREAD/EM256*.  Die Workbench selbst (`cpa_builder.py`) ist ein CPA-Konfigurator und für
  `mutos_io` **nicht** nötig; wir brauchen nur M80/LINKMT.  Kein nativer Z80-Assembler installiert.
* Disassembler: `tools/z80_disasm2.py`/`z80_disasm3.py`, `k1520dbg` (`lst`, `.prn`-Annotation), Entwurf 39 §11.

## Arbeitspakete

| AP | Inhalt | Ergebnis / Wächter |
|----|--------|--------------------|
| **M1** | Segmente aus der Datei lösen (`segs=` → vier Binärstücke mit Ladeadresse), Disassemblat mit Code-/Datenerkennung je Segment (Einstiege: Segmentanfänge, `CALL`-/`JP`-Ziele, Tabellen `A12A`, Textmasken).  Z80-Disassembler so erweitern, dass er **assemblierbare Ausgabe** erzeugt (Marken, `DB`/`DW`/`DEFM` für Texte, relative Sprünge als Marken). | `tools/mutos8000/mutos_io/mutos_io.mac` (roh, ohne Kommentare) |
| **M2** | **Round-Trip-Beweis vor jedem Kommentieren:** M80/LINKMT bauen die rohe Quelle, Ergebnis == Originalbytes je Segment.  Entscheiden, wie die vier Segmente gebunden werden (ein Modul mit `.PHASE`/`ORG` je Segment und Füllbytes, oder je Segment ein Modul); Füllbytes der Sektorgrenzen ebenfalls aus dem Original übernehmen (nicht raten: ihr Inhalt ist Teil der Bytegleichheit). | `tools/mutos8000/mutos_io/build.py --check` (Exit 77 ohne Werkzeugkette) |
| **M3** | **Neu bauen aus der Quelle → Datei + `.fileinfo`** (`segs=`, `mem=`, `zusatz=` neu berechnen, nicht kopieren; `geaend=` bleibt ein bewusster Wert) → in ein Ladedisketten-Archiv einsetzen → `bau_ladediskette.py` → Boot bis `single user login: root`.  Wächter im CLI-Test: Bau aus Quelle liefert dieselbe Ladediskette wie der Bau aus dem Archiv. | `build.py --in-archiv`, Test `cli_mutos_io_bau` |
| **M4** | Kommentieren in Etappen, jede Etappe **nach M2 wieder bytegleich**: (a) Einstieg, Initialisierung, Bedienmaske (Meldetexte, `A12A`, `[0003H]`); (b) Plattenweg `90EB–9966` (bekannt, übertragen aus §11.2); (c) Rückrufe und Übertragung zum EM (`9163…`, LOAD16-/IFSS-/PIO-Weg, Mailbox `D532`); (d) Konsolentreiber (IFSS/PIO, SIO, Interrupts); (e) Datenbereiche, Variablen, Puffer (`996B–997E`, `A55A`, `A401`…); (f) Segmente `A400`/`C8B6`.  Gegenprobe im Emulator (`k1520dbg` Haltepunkte), wo das Disassemblat mehrdeutig ist. | kommentierte Quelle + Kapitel in Entwurf 39/40 |
| **M5** | Die vorhandenen Patches (`mutos_patch.py io-8x512`) **als Quellvarianten** (Bedingte Assemblierung `IF K8X512`) nachbauen und gegen die gepatchten Dateien auf Bytegleichheit prüfen — das ist der Nachweis, dass die Quelle Änderungen trägt.  Danach erst neue Änderungen. | Wächter: Variante == `mutos_patch`-Ergebnis |
| **M6** | Merkposten `doc/merkposten/mutos_io.md` (Festlegungen + Wächter), Eintrag in `CLAUDE.md` und `tools/mutos8000/README.md`, `test_auswahl.py` (Fachlabel), Commit je AP. | |

### Werkzeugpflege in diesem Paket (Auftrag des Anwenders, 2026-10-27)

**Ausgangslage:** es gibt schon `tools/z80_disasm.py`, `z80_disasm2.py` (wird mit ausgeliefert, `test_packaging`) und `z80_disasm3.py`
(CP/A-`.COM`-Annotierer) — und M1 hat mit `tools/mutos8000/mutos_io/dis.py` einen **vierten** geschrieben, statt den
vorhandenen zu erweitern.  Das war ein Fehler im Vorgehen; `dis.py` ist nur als Seed gedacht und wird **nicht** dauerhaft
neben den anderen stehen.  Regel ab jetzt: **vor jedem Werkzeugbau zuerst die vorhandenen ansehen und erweitern; ein Disassembler, ein Ort.**

| AP | Inhalt | Ergebnis / Wächter |
|----|--------|--------------------|
| **W1** | **`z80_disasm2.py` zum einen Disassembler ausbauen** (Ablage bleibt, weil ausgeliefert): `--format listing\|mac` (assemblierbarer M80-Text mit Marken, `DB`/`DW`/Text, `FILLFF`), Segmente (`--seg adr:offset:laenge`, mehrere je Datei), **Kartendatei** (`code`/`db`/`dw`/`dwc`/`str`/`txt`/`name`/`eq`, Format aus `mutos_io/karte.txt`), Wurzeln aus Zeigertabellen (`dwc`) und `LD rr,nn`-Zeigern, Abbruch an FF-Läufen, **`--pruefen`-Rundlauf** (Ergebnis assemblieren, byteweise vergleichen) | Tests `tests/python/test_z80_disasm.py`: Befehlsvollständigkeit gegen eine Tabelle aller Z80-Opcodes (inkl. DD/FD/ED/CB, undokumentierte als `DB`), Rundlauf an mehreren Prüflingen (`sertest.com`, `romread.com`, `mutosio.bin`), Karten-/Segmentfälle |
| **W2** | `dis.py` und `mutos_io/karte.txt`-Format dorthin überführen; **`z80_disasm.py` und `z80_disasm3.py` stilllegen** (3 ist ein reiner CP/A-Annotierer → als Kartendatei + Aufruf von W1 abbilden oder entfernen), Verweise in Doku/Paket nachziehen (`packaging/*`, `test_packaging.py`, `doc/*.md`) | ein Disassembler; `rg z80_disasm` nennt nur noch ihn |
| **W3** | **Kommentieren/Umbenennen als Werkzeug statt Einmalskript:** `--karte` trägt Namen und Kommentare (`name`, `kom`, `ik` für Zeilenkommentare), damit sich eine Quelle aus Abzug + Karte **reproduzierbar** erzeugen lässt; Wächter: Quelle aus Karte == eingecheckte Quelle | `mutos_io` und künftig K8915/PRG-710-Abzüge nach demselben Weg |
| **W4** | Weitere Lücken, die bei M4 auffallen, **sofort im Werkzeug** schließen (z. B. Textformate mit Längenbyte `str`, Variablenbereiche, Rückwärtssuche nach Zeigern auf Marken mitten im Befehl) und in `tools/how_to_debug_and_trace.md` festhalten | Merkposten `doc/merkposten/mutos_io.md` |

Reihenfolge: W1 **vor** dem Kommentieren (M4), weil M4 genau die Funktionen braucht (Namen, Kommentare, Karte); `dis.py` trägt
bis dahin als Übergang.  Der Rundlauf aus M2 (`build.py --check`) bleibt der Maßstab für jede Werkzeugänderung.

## Entscheidungen / Risiken

* **Reihenfolge M1→M2→M4:** erst die rohe Quelle bytegleich bauen, dann kommentieren.  Kommentare sind
  frei von Bytewirkung; jeder Kommentar-Stand muss `--check` bestehen, sonst schleicht sich ein Übertragungsfehler ein.
* **Mehrdeutigkeit Code/Daten:** inline eingestreute Tabellen und Texte lassen sich nur durch Lauf klären.
  Gegenmaßnahme: `k1520dbg` mit Ausführungsprotokoll (welche Adressen werden je Boot tatsächlich ausgeführt)
  als Gegenprobe zur statischen Erkennung — ungelaufene Bereiche in der Quelle als `; ungelaufen` markieren.
* **Zusatz `zusatz=EB09400`** (UDOS-Segmentkopf-Rest?) und `mem=9000:CB68:0080` — Bedeutung der Felder aus dem
  UDOS-Segmentformat (`doc/udos_diskettenformat.md`, DiskTool `udos_*.cpp`) belegen, bevor M3 sie berechnet.
* **Werkzeugkette nicht in der CI:** wie bei SERTEST — Quelle **und** gebaute Datei eingecheckt, `--check`
  überspringt ohne CPA_Workbench (Exit 77).  Nicht in der CI-Pflicht.
* **Quelle bleibt ASCII** (keine Umlaute im Code, Projektregel); Kommentare deutsch.
* Urheber: `mutos_io` ist fremder Code (Disassemblat einer Originaldatei).  Eingecheckt wird die Quelle nur
  als Arbeitsgrundlage zum Verständnis/zur Änderung, dem Projektstil von `tools/em256`/`doc/EPROMS/*.prn` folgend.
* Der Kernel `mutos_1216` (Z8000) bleibt ausgenommen; Gegenseite der Mailbox wird nur gelesen, nicht gebaut.

## Nächster Schritt

M1 + M2 (rohe Quelle, bytegleicher Round-Trip) — erst danach lohnt das Kommentieren.  Danach M3 als
Gegenprobe, dass der ganze Weg bis zur booteten Diskette trägt.

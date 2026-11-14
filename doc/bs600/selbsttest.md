# BS620 — Selbsttest („GRUNDDIAGNOSE“) und Weg bis zum Hauptmenü

Entwurf 48, AP-B0.  Grundlage: `doc/bs600/bs620.mac` (vollständig disassemblierte Datei `BS620` der Diskette
`BS600.hfe`, Lade- und Startadresse 6D00H/6D5FH), das Gerätehandbuch PRG 700/PRG 710
(`~/projects/robotron/PRG710/`, Abschnitt 1.8 „Grunddiagnose“ und 1.4 „Adressenbelegung“) und der Vergleich mit der
PRG-710-Fassung der zweiten Diskette (`BS600_*Tracks*`, Datei `BS600`).

Gewissheitsgrade: **[Disk]** unmittelbar aus dem Programmcode, **[Plan]** aus dem Gerätehandbuch (kein Schaltplan der
Karten 590068/590069 veröffentlicht), **[?]** Annahme/Deutung, **[ber.]** aus dem Code gerechnet (Takte, ±5 %).

## 1. Ladeplan der Datei (Voraussetzung für alles Weitere) [Disk]

| Datei-Offset | Ladeadresse | Inhalt |
|---|---|---|
| 0000–42FF | 6D00–AFFF | Programm + Tabellen, bleibt dort (Selbsttest ab 6D5F, Menüs, Überlagerungsroutinen, Sprungleiste AF60–AFFF) |
| 4300–4EFF | B000–BBFF | Quellkopie des **Monitors**; START kopiert sie nach **0000–0BFF** (dort Sprungleiste 0B90–0BFF) |
| 4F00–67FF | BC00–D4FF | Quellkopie; START kopiert sie nach **1000–28FF** (Dateidienste, Menütexte, Bedienroutinen) |

D400–D4FF (Fehlerausgabe des Selbsttests) wird in Stufe 1 **am Ladeort** ausgeführt (`JP D400`, `CALL D452`), danach in
der Kopie bei 2800–28FF.  Die Bytes B000–D3FF bleiben als Quellkopie liegen; der RAM-Test B prüft sie deshalb **nach**
dem Kopieren mit (Stufe 3, 4.).

Folge für den Emulator: **Der untere Monitor (0000–0BFF) ist nach dem Kopieren RAM**, nicht ROM
(`E8H[0] = 00`).  Der ROM-Lader des PRG 710-1 (1 KB U555) ist dann nicht mehr sichtbar.

## 2. Übersicht der Tests

Reihenfolge laut Code (= Handbuch 1.8).  Alle Tests laufen nach **jedem** Laden von BS600 und bei jedem Neustart der Datei,
nicht beim Wiederkehren ins Hauptmenü.

| Nr | Test | Adresse | Ports | Erwartung | Text auf dem Bildschirm | Code F5H / F4H | Wirkung eines Fehlers |
|---|---|---|---|---|---|---|---|
| 0 | RAM A | 6D6D | – | 0000–0BFF und 1000–28FF halten AAH, dann 55H (10 Lesedurchläufe je Muster) | `RAM-ZELLE xxxx` (Adresse) über `CALL 04BC` | – / – | **fatal**, `RST 38` (s. 3.1) |
| 1 | Bild, Titel | 6E22 | VRAM F800–FFFF | – | `GRUNDDIAGNOSE` (Position 1DH) | – | – |
| 2 | PIO ZE | 6E6C | 84H–87H | beide Tore: 10 × (AAH, 55H) schreiben und lesen gleich | `PIO ZE PORT` | 01H (Tor A) / 02H (Tor B), 3EH | Meldung, Warten, Prüfung des **anderen** Tores läuft weiter |
| 3 | RAM B | 6E82 | – | 0C00–0D8F, 2900–5FFF, B000–D3FF, D500–EFFF: AAH, 55H, je 2 Lesedurchläufe | `RAM-ZELLE xxxxH` | – | Meldung, Warten, **Rest von RAM B entfällt**, weiter mit Test 4 |
| 4 | CTC ZE | 6EF4 | 80H–83H | je Kanal Zeitgeber-Interrupt innerhalb der Wartezeit | `CTC ZE KANAL` | 30H…33H (Kanal 0…3) / 3EH | Meldung, Warten, nächster Kanal |
| 5 | CTC ATP | 6F4A | CCH, CDH | Zähler zählt sofort mit (1FH wird gelesen ≠ 1FH) | `CTC ATP KANAL` | 31H (Kanal 1, CDH) / 30H (Kanal 0, CCH) / 3EH | Meldung, Warten, nächster Kanal |
| 6 | SIO | 6F7A | C0H–C3H | AAH gesendet auf A, dann B wird empfangen | `SIO  KANAL` | 01H (Kanal A) / 02H (Kanal B) / 3EH | Meldung, Warten, Kanal B bzw. Hauptmenü |
| 7 | Prüfsumme | 2400 | – | 25 Blöcke zu 1 KB: Summe + Sollbyte (AEC0+Blocknr.) = 0 | `PRUEFSUMME xxxxH` | – | in **BS620 stillgelegt** (RET bei 2400), in BS610 aktiv |

Bildschirmtexte im Programm (6D22–6D5E, Länge als Byte davor bzw. in B): `RAM-ZELLE` (9), `PIO ZE PORT` (11),
`CTC ZE KANAL` (12), `CTC ATP KANAL` (13), `SIO  KANAL` (10: zwei Leerstellen).  **Die Kanalnummer steht nicht im Text**,
sie steht nur im Code an F5H.  Das Handbuch zeigt „… KANAL X“; wo X erscheint (Bildschirm oder Anzeige), ist aus dem Code
nicht abzulesen [?].

## 3. Ablauf im Einzelnen

### 3.1 Stufe 0 — Speicherverwaltung und RAM A (6D5F–6DF6) [Disk]

1. `E8H` mit B = 0 (Seite 0) := 00H, mit B = F0H (Seite F) := FFH.  Seite 0 = RAM, Seite F zeigt das VRAM.
2. RAM-Test A: Muster AAH; `LDIR` füllt 0000–0BFF (BC = 0BFFH) und 1000–28FF (BC = 18FFH); danach 10 Durchläufe
   `CPI … JP PE` über beide Bereiche.  Danach `RRA`: aus AAH wird 55H (Übertrag 0) → zweite Runde; aus 55H wird 2AH
   (Übertrag 1) → weiter.  ≈ 5,3 Mio. Takte [ber.].
3. Fehler (6DA9): `E8H[0] := FF`, `E8H[F] := 00`; die Zelladresse (HL − 1) wird als vier Hexziffern in den Text `RAM-ZELLE xxxx `
   eingetragen; `CALL 04BC` (Ausgabe), `(0FDA) := 11H`, `RST 38`.  04BC und 0038 gehören zum 3-KB-ROM des PRG 700 (das
   Handbuch: „mit Rücksprung in den Debugger“); im 1-KB-ROM der 710/710-1 liegt dort kein passender Code (0400–0BFF
   unbestückt) [?].  Der Fehlerpfad ist für den Emulator unerheblich, solange das RAM stimmt.

### 3.2 Stufe 1 — Kopieren, Bild, Seiten (6DF7–6E48) [Disk]

- B000–BBFF → 0000–0BFF und BC00–D4FF → 1000–28FF (`LDIR`).
- **`E8H[1], [7], [8], [9], [A] := 04`** (B = 10H, 70H, 80H, 90H, A0H).  Das sind die Seiten 1000–1FFF und 7000–AFFF.
  Später stellt `25E0` die Seiten 7–A zwischen **00H** und **04H** um (vor/nach dem Nachladen einer Überlagerung:
  `25C4: XOR A / CALL 25E0 … CALL 1DE5 … LD A,04H / CALL 25E0`).  Deutung: **04H = Schreibschutz** dieser Seiten (Programm
  geschützt, solange nichts nachgeladen wird) [?].  **Folge für den Emulator:** `Prg710Speicher::ortVon` behandelt jedes
  untere Halbbyte ≠ 0 bei `EBH ≠ 0` als „Systemkarte“ (Seite 1 und 7–A wären **leer**); BS620 liefe damit schon bei
  `JP AFDF` (Seite A) in leeren Speicher.  Näheres in `doc/bs600/atp_hardware.md` §5.
- Bildschirm löschen: `HL = FFFF; (HL) = 20H; LDDR` bis F800.  `CP (HL)` mit F800H = 20H ist wahr → `HL = F8FC` → `(03FE) :=
  FCH`, `(03FF) := F8H` (nicht wahr: FCF8).  Der ROM-Lader prüft `(03FF) = F8H`; Zweck hier [?].
- `SP := 2900`, `(0E50) := 0` (Fehlermerker, Bit 0 = „ein Fehler wurde gemeldet“; `D458` setzt, `D452` löscht), Bild löschen
  (`CALL 0BA6`), Titel `GRUNDDIAGNOSE` (74F2, 13 Zeichen) auf Position 1DH (`CALL 0BA3` mit vier Bytes hinter dem Aufruf:
  Länge, Position, Zeiger lo/hi), Merker löschen.

### 3.3 Test 2 — PIO ZE (6E4E–6E81) [Disk]

- `OUT (86H) = CFH, 00H` und `OUT (87H) = CFH, 00H`: beide Tore Betriebsart 3, alle Bits Ausgang.  (86H/87H sind die Steuerwörter
  von Tor A/B; Daten 84H/85H, wie im Merkposten `prg710.md`.)
- Je Tor (C = 84H bzw. 85H): `B = 10`; `A = AAH`; Schleife: `OUT (C),A / IN A,(C) / CP D / JP NZ,D400 / RRA / JR NC`:
  AAH, 55H, dann (55H → 2AH mit Übertrag) weiter mit `DJNZ` → insgesamt 10 × (AAH, 55H).  Abschluss: `OUT (C),0`.
- Fehler: `D400` → `D464` wertet `RR C` aus (Bit 0 der Portnummer: 84H → 01H, 85H → 02H), `OUT (F5H),A`, `OUT (F4H),3EH`,
  Fehlermerker; `B = 0BH` / `HL = 6D31` → Text `PIO ZE PORT` über `0BC4`; `RET` zum Aufrufer: das **andere** Tor wird noch geprüft.
- Das Zurücklesen ist nur erfüllt, wenn das Lesen eines Ausgangsports den **Ausgangsspeicher** liefert (Z80-PIO Betriebsart 3).

### 3.4 Test 3 — RAM B (6E82–6EF3) [Disk]

- Gebiete (Länge): 0C00 (0190H), 2900 (3700H), B000 (2400H), D500 (1B00H).  Ausgespart: 6000–6CFF (nicht bestückt?), 6D00–AFFF
  (das laufende Programm), D400–D4FF (die Fehlerroutinen), 0E00–0FFF-Variablen außerhalb 0C00–0D8F.
- Zwei Muster (AAH, 55H), je zwei Lesedurchläufe `CPI` + `CALL NZ,D40C`.
- Fehler: `D40C` → `DEC HL`, `D46D` (HL nach `0E80`, `CALL 0BAC` → vier Hexziffern nach `0E90`, `'H'`, Ausgabe über `0BA3` mit
  Länge 5 auf Position 3FH), Fehlermerker, `RAM-ZELLE` über `0BC4`, **`POP HL; JP 6EF4`**: der Rest des RAM-Tests entfällt,
  weiter mit dem CTC-Test.  Kein F4H/F5H-Code.
- Handbuch: Bereiche 0000–0FFF, 2900–6FFF, B000–D3FF, D500–EFFF (Abweichung zum Code: Handbuch ungenau) [Plan].
- ≈ 5,7 Mio. Takte [ber.].

### 3.5 Test 4 — CTC ZE (6EF4–6F49), Interrupt IM 2 [Disk]

- `DI; I := 6DH; IM 2`.  Vektortabelle 6D00, 6D02, 6D04, 6D06, alle → 6F38 (die `JR C`-Bytes am Anfang des Disassemblats sind diese Tabelle).
- Je Kanal n (80H…83H): `OUT 00H` (Vektor 00H, Grundvektor), `OUT B7H` (Zeitgeber, Vorteiler 256, Interrupt frei, Zeitkonstante
  folgt, Reset); danach `OUT (n) := 27H` (39 × 256 = **9984 Takte**).
- Wartezeit ≈ **10 380 Takte** ab dem `OUT` [ber.]: der Interrupt (ISR 6F38: `OUT (C) := 33H` setzt Kanal zurück, `INC SP`
  ×2, `PUSH IX` (= 6F18), `RETI`) muss ≈ 400 Takte **vor** dem Ende der Wartezeit kommen — **Spielraum ≈ 4 %**.  Er springt
  in die Kanalschleife 6F18 zurück; kein Interrupt → Wartezeit ende: `OUT := 33H`, `D41F`: `A = 34H − D`, `OUT (F5H)`,
  `OUT (F4H) := 3EH`, Text `CTC ZE KANAL`, weiter mit `JP 6F18` und `INC C` (**nächster** Kanal).
- Zuerst bekommen **alle vier** Kanäle `00H, B7H` (Grundvektor, Steuerwort); jeder Kanal wartet dann auf seine Zeitkonstante.  Erst `OUT := 27H` startet
  den Kanal der aktuellen Runde.  Kanal 3 ist hier ebenfalls Zeitgeber (Betriebsart Zeitgeber ignoriert CLK/TRG); die gesteckte Kaskade ZC/TO2 → Kanal 3
  der ZRE ist für den Test ohne Bedeutung [Disk].

### 3.6 Test 5 — CTC ATP (6F4A–6F7A), **Zähler ohne Interrupt** [Disk]

- Ports **CCH** (Kanal 0) und **CDH** (Kanal 1).  Abfolge: `OUT (CCH) := 57H` (Zähler, steigende Flanke, Zeitkonstante folgt, Reset),
  `OUT (CDH) := 57H`, `OUT (CDH) := 1FH`, `IN D,(CDH)`; **Fehler, wenn D = 1FH**; dann `OUT (CCH) := 1FH`, `IN D,(CCH)`; Fehler, wenn D = 1FH.
- Der Zähler muss also **zwischen `OUT` und `IN` (12 Takte) mindestens eine Flanke** an CLK/TRG des Kanals gesehen haben — der
  Takt der ATP-CTC muss **laufen und ≥ ≈ 200 kHz** sein.  Kanal 1 wird **vor** Kanal 0 geladen und geprüft: Kanal 1 kann deshalb
  nicht von ZC/TO0 getaktet werden (sonst Fehler), jeder Kanal hat seinen eigenen CLK/TRG-Eingang [Disk].
- Fehlercodes: Kanal 1 (`D436` → `D488`) 31H, Kanal 0 (`D431` → `D484`) 30H, F4H 3EH, Text `CTC ATP KANAL`.
- Danach (6F68): beide Kanäle `OUT 55H, 01H` (Zähler, Zeitkonstante 1, ohne Reset): **ZC/TO folgt jeder CLK-Flanke**; so liegt der
  CLK-Takt unverändert am Ausgang an (Baudratentakt der SIO, wahrscheinlich Kanal 0 → SIO A, Kanal 1 → SIO B) [?].

### 3.7 Test 6 — SIO C0H–C3H (6F7A–6FB7) [Disk]

- Portordnung wie die SIO der ASS 590069: **C0H Daten A, C1H Daten B, C2H Steuer A, C3H Steuer B** (A0 = B/A, A1 = C/D).
- Init (zehn Bytes ab 6D10): `00 18` (Kanalreset), `04 4F` (WR4: **×16**, 2 Stoppbits, **gerade Parität**, Parität an), `05 68` (WR5: Sender an,
  8 Bit), `01 00` (WR1: keine Interrupts), `03 C1` (WR3: Empfänger an, 8 Bit).
- Test: `DI`, `OUT (C0H) := AAH`; Unterprogramm 6FA2: bis zu 255 Abfragen von RR0 Bit 0 (Empfangszeichen verfügbar,
  ≈ 40 Takte je Runde ⇒ ≈ 10 000 Takte ≈ **4 ms**), `IN (C0H)` muss AAH sein.  Dann dasselbe mit Kanal B (C3H/C1H).
- **Die Schleife Sender → Empfänger ist Hardware** (nicht BS600): ob fest verdrahtet („Diagnosebetrieb“ im Handbuch [Plan]) oder Prüfstecker, ist offen.
- Baudrate: mindestens 11 Bit ≤ 4 ms ⇒ ≥ 2750 Bd ⇒ Takt ≥ 44 kHz; mit dem Zähler-Durchreichen aus 3.6 (CLK/TRG gleich dem Systemtakt φ = 2,4576 MHz → 153,6 kBd,
  bei 614,4 kHz 38,4 kBd) ist das erfüllt [?].
- Fehler (Zeitablauf oder falsches Zeichen): `D442` → `D464` (`RR C`: C2H/C0H → 01H, C3H/C1H → 02H) → F5H, F4H 3EH, Text `SIO  KANAL`.
- Nach Kanal B: `JP AFDF` → Sprungleiste → **2561 Hauptprogramm**.

### 3.8 Fehleranzeige und Warten (`0BC4` → 097F) [Disk]

- Der Text wird auf Position 30H des Bildes geschrieben (Beobachtung im Emulator: Zeile 5, Spalte 16 [?]) und **blinkt**:
  Text ein, Wartezeit, Text aus (ein Block blank), Wartezeit.  Wartezeit (09A8) = 65 535 × 23 Takte ≈ **0,61 s**, ein Zyklus ≈ 1,2 s.
- Am Ende jedes Zyklus fragt `RST 30` (`CALL 0201`) die Tastatur.  Zyklusende bei Tastencode **82H oder 83H**, jeder andere Code
  (auch 80H, 81H, 84H …) wiederholt den Zyklus.  Zu **Beginn** jedes Zyklus löscht `RST 08` den Tastaturpuffer (`(0E4A) := FF`) und
  **initialisiert die Tastaturschnittstelle neu** (`JP 0048`: `OUTI`×2 an Port 58H, `OTIR` an 5FH: K8025 CTC-Takt + SIO B 9600 Bd 8N2).
  Ein Zeichen, das **vor** dem `RST 08` eintrifft, geht verloren; eines während der Wartezeiten wird beim nächsten Zyklusende gelesen.
- Danach löscht `D45E` die Meldung (`CALL 0BA9` ab Position 30H) und der Test läuft **weiter** (Fortsetzungspunkte s. Übersicht).
- **Tastencodes 710-1** (K7672 an K8025 SIO B, Daten 5EH, Status 5FH, 9600 Bd 8N2; Übersetzung bei 0201): ESC (1BH) **allein** (kein
  Folgezeichen in ≈ 256 Abfragen ≈ 5 ms) → **83H**; 0DH → 84H; Kleinbuchstaben → Großbuchstaben; `ESC [`/`ESC O`/`ESC ?` + `u`/`v`/`w`/`s`/`t` → 80H/81H/82H/87H/88H;
  `ESC C`/`D`/`A`/`B` (auch hinter `[`) → 89H/8AH/8BH/8CH; `ESC Z` → 1EH.  **83H = ESC, 82H = eine Funktionstaste.**
- **Tastencodes 710 (K7609 am 8279)**: BS610 übersetzt den Matrixcode über die Tabelle bei 02B3: Code 28H (**Taste CL**, links von der A-Reihe) → **83H**; 18H → 82H;
  10H → 81H; 08H → 80H; Rückkehr (ET) → 84H.  Das ist der Sinn von „CL **bzw.** ESC“ im Handbuch: derselbe interne Code 83H.

## 4. Hauptprogramm (2561) [Disk]

`LD SP,0FFFH` → `74A8` (`RST 08`; Tabellen an `OTIR_PAAR`: **K6022-Ports E6H/E7H (Leser) und E2H/E3H (Stanzer)** und **ZRE-PIO 86H/87H** programmieren — bei fehlender Karte ohne
Wirkung) → Bild löschen (`0BA6`) → `EI` → Überschrift `BS600` (Position 0) und `VERSION:` (Position 20H) mit der Versionszahl aus `(AFFE)` (**04** in BS620, **02** in BS610) →
`CALL 2400` (Prüfsumme; in BS620 sofort `RET`) → Menü (`0BBE` aus der Tabelle 2509…; Zeilen: `PROG/TEST PC600`, `DIENSTPROGRAMME`,
`PROG PMC600/CNC600`, `PROG PMC600/CNC-H`, `PROG NC-WERKSTUECKE`) → `0BC1` (Cursortasten, ENTER) → Auswahl lädt die Überlagerung (`SYSCN6`, `SYSCNH`, `SYSDMF`, `SYSNCW`, `SYSRE1`, `SYSRE2`,
`SYSVSP`, Namen bei 251F, Startadressen in der Tabelle 2549) über `1D83`.  Die Überlagerung läuft in 7000–AFFF mit `E8H[7..A]` = 00H beim Laden, dann 04H (3.2).

Das Hauptmenü ist also **kein** Test, aber derselbe Weg: kommt es nicht, so scheitert `JP AFDF` (Seite A) oder die Seitenregister.

## 5. Prüfsumme (nur BS610) [Disk, bestätigt]

`2400`: für 3 Blöcke ab 0000, 6 Blöcke ab 1000 und 16 Blöcke ab 7000 (je 0400H Bytes): 8-Bit-Summe + Byte bei `AEC0 + H/4` = 0.  An der statischen
BS610-Datei stimmen **alle 25 Blöcke** (Bestätigung des Abzugs und des Algorithmus).  In der BS620-Datei stimmen sechs Blöcke nicht (0000, 0400, 0800, 2400, 9000, AC00) — dort steht
`RET` bei 2400: die Prüfung ist abgeschaltet.  Fehlermeldung: `PRUEFSUMME xxxxH` + Warten (`0BC4`).

## 6. Weitere Beobachtungen

- Zweite Kopie der Fehlerroutinen **26A7–26E8** (`OUT (F5H)`, `OUT (F4H) := 3EH`) im Monitor und eine Schleife **1C32** (`F5H := 60H`, dann `F4H :=` C bis DFH):
  die Ports F4H/F5H gehören einer Anzeige; Aufbau unbekannt (§5 Hardware) [?].
- Stufe-1-Code und -Tabellen (6D00–6FB7, D400–D493) sind in BS610 und BS620 **bytegleich**; die Tests laufen also auf dem PRG 710 genauso.
- Gesamtdauer der Selbsttests ≈ 11,3 Mio. Takte ≈ 4,6 s bei 2,4576 MHz, davon nur ≈ 0,05 s ohne die zwei RAM-Tests [ber.].
- Der Merker `(0E50)` Bit 0 wird von vielen Menüroutinen abgefragt (>50 Stellen) — er markiert „Fehlermeldung steht auf dem Bildschirm“ [?].

## 7. Folgerungen für AP-B1/B2

1. **`E8H` = 04H** (und 00H, FFH): Wirkung unbelegt; der Kern muss mindestens „RAM sichtbar“ liefern, solange 04H nicht „leer“ heißt.  Ohne das bricht BS620 spätestens bei `JP AFDF`.
2. **CTC CCH–CFH mit laufendem CLK/TRG** je Kanal (≥ ≈ 200 kHz), Zähler zählt in Betriebsart „Zähler“ sofort; **ZC/TO ≙ CLK bei Zeitkonstante 1**.
3. **SIO an C0H–C3H** (A0 = B/A, A1 = C/D, Z80-SIO), ×16, Schleife Tx → Rx in beiden Kanälen, Zeichen innerhalb ≈ 4 ms.
4. F4H/F5H beschreibbar (Protokoll genügt; nur `OUT`, kein `IN`).
5. Tastatur: ESC allein → 83H; **das Zeichen muss ≥ 5 ms vor dem nächsten ankommen oder allein bleiben**.  Ein Zeichen unmittelbar vor `RST 08` geht verloren (Wartezyklus 1,2 s).
6. Stillstandsmelder: die Selbsttests brauchen ≈ 11 Mio. Takte ohne Bildänderung.

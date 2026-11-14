# K6311 ↔ K6313 (10/16 KiB) ↔ K6316 — E/A- und Speicherplan-Diff aus den Abzügen (WP8a)

Stand 2026-11-14.  Werkzeug: `tools/z80_disasm2.py <abzug>` (rekursiver Abstieg ab 0000H; kein `--linear`, damit Datenbereiche nicht als Befehle zählen).  Ausgewertet: OUT/IN-Operanden
(`OUT (n),A`, `IN A,(n)`), `LD SP,nn`, `LD IX,nn`, `LD I,A`, die Vektor-Schreibungen `LD (38xxH),HL`, der ROM-Kennungstext.  K6316: `doc/EPROMS/K6316/{ifss,v24}/` (I1…I5 / V1…V5 hintereinander gelegt,
0000–27FFH).  Marken: `[E]` gemessen/erschlossen aus dem Abzug, `[S]` Schaltplan (`hardware.md`), `[?]` Annahme.

## 1  Ergebnis in drei Sätzen

1. **K6311 und K6313 teilen eine E/A-Landkarte (7CH–FBH, „eine Adressleitung tief"), die K6316 nicht kennt (80H–AFH, DS8205-Dekoder).**  Die Abzüge beginnen alle gleich
   (`DD 21 24 .. 31 ..` = `LD IX`, `LD SP`), die Firmware-Wurzel ist die gleiche, die Hardware **nicht**.
2. **K6313 hat zwei Speicherpläne:** 10 KiB (ROM 0000–27FFH, RAM 3800H, Stapel 3C00H — wie K6316/K6311) und 16 KiB (ROM 0000–3FFFH, **RAM F000–F7FFH**, Stapel F800H); beide laufen auf **derselben** CCB.
3. Die 16-KiB-Familie trägt Prüfprogramme im Bereich 2800–3FFFH (Stapel `F400H`) und andere Interfaceroutinen; der Rest des Bausteinsatzes ist unverändert.

## 2  E/A-Adressen je Fassung

| Gruppe | Adressen (Abzüge) | Baustein (`hardware.md` §2.2) |
|---|---|---|
| **F8–FB** | alle 19 | PIO D104 (Motoren, Sensoren, Nadel 9) |
| **F4–F7** | alle 19 (`F7` fehlt nur in `33538012`) | CTC D101 |
| **EC–EF** | alle 19; `ED` in 13 von 19 | CTC D102 (EE = Baudtakt) |
| **DC–DF** | alle 19 | PIO D105 (Matrix, Nadeln 1…8) |
| **BC–BF** | alle außer `13325212` (nur IFSS) | PIO der Centronics-Kassette (D1); in V.24-Abzügen mit Baudwahl über `IN A,(BCH)` (K6311) bzw. Kennung ungenutzt `[?]` |
| **7C–7F** | alle außer `12228062`, `37239503` (reine Centronics-Abzüge); `13325212` nur `7D, 7F` | SIO: IFSS über **Kanal B** (7D/7F), V.24 über **Kanal A** (7C/7E, Vektor über 7F) |
| K6316 **80–AB** | IFSS: 80–84, 87, 88, 8A–8D, A5, A7–A9 (+ 8E, 8F, AA, AB über `OUT (C)`); V.24 zusätzlich A4, A6 | PIO D12/D14/D11, CTC D5/D10, SIO am Modul (`doc/printer/k6316/hardware.md` §2) |

**K6313/K6311 und K6316 haben keine gemeinsame E/A-Adresse.**  Die Zuordnung der K6313-Gruppen zu Bausteinen ist durch den Schaltplan `[S:4, S:2]` gedeckt (Gruppen F8/F4/EC/DC/BC aus den Leitungen A2…A6;
7C aus A7, Plan der V.24-Kassette fehlt `[?]`), die Registerfolge aus den Einrichtungssequenzen: PIO-Steuerwörter `FFH`/Maske an `FA/FB/DE/DF`, CTC-Vektorschreibung `00H`/`08H` an `F4`/`EC`, SIO-WR0-Kanalreset `18H` an `7F`/`7E`.

## 3  Je Abzug

`SP` = `LD SP,nn`; `I` = `LD A,n; LD I,A` (Hochbyte der Vektortabelle); SIO/PIO-Spalten: tatsächlich benutzte Adressen.

| Abzug | SP | I | SIO (7x) | PIO (Bx) |
|---|---|---|---|---|
| `12128062` (K6311, V.24, …802) | 3BFE | 38H | 7C 7E 7F | BC BD BE BF |
| `12128062_1` (K6311, V.24, …803) | 3BFE | 38H | 7C 7E 7F | BC BD BE BF |
| `12228062` (K6311, Centronics) | 3BFE | 38H | – | BC BD BE BF |
| `13120012` (K6311, 8 KiB V.24) | 3C00 | 38H | 7C 7E 7F | BC BD BE BF |
| `13325212` (K6311, 8 KiB IFSS) | 3C00 | 38H | 7D 7F | – |
| `33538012` | 3C00 | 38H | 7C 7E 7F | BC BD BE BF |
| `33538013` | 3C00 | 38H | 7C 7E 7F | BC BD BE BF |
| `33538028` | 3C00 | 38H | 7C 7E 7F | BC BD BE BF |
| `33538511` (defekt) | 3C00 | 38H | 7C 7E 7F | BC BD BE BF |
| `33538513` | 3C00 | 38H | 7C 7E 7F | BC BD BE BF |
| `33538514` | 3C00 | 38H | 7C 7E 7F | BC BD BE BF |
| `33830803` | 3C00 | 38H | 7C 7E 7F | BC BD BE BF |
| `3b834000` (Verdacht Nullfläche) | 3C00 | 38H | 7C 7E 7F | BC BD BE BF |
| `53538500` (neue Platine, 10 KiB) | 3C00 | 38H | 7C 7E 7F | BC BD BE BF |
| `30835007` (16 KiB, Prüfprogramme) | F800 (Prüfprogramm F400) | F0H | 7C 7E 7F | BC BD BE BF |
| `30835207` (16 KiB) | F800 (F400) | F0H | 7C 7E 7F | BC BD BE BF |
| `36539211` (16 KiB, Kennung 4.65) | F800 | F0H | 7C 7E 7F | BC BD BE BF |
| `37239503` (16 KiB, nur Centronics) | F800 | F0H | – | BC BD BE BF |
| `5c53a001` (16 KiB auf 32-KiB-Etikett) | F800 | F0H | 7C 7E 7F | BC BD BE BF |

`IX` = `3824H` bzw. `F024H`; beim Anlauf `LDIR` über `1 KiB` ab `IX`: löscht in der 10-KiB-Fassung `3824…3C23H` (auch hinter dem RAM, wie K6316 — Schreiben ins Leere ist harmlos
`[E]`), in der 16-KiB-Fassung `F024…F423H`.  Die ersten Befehle der 10-KiB-K6313-Abzüge sind byteidentisch mit K6316 bis `0018H` (`LD IX,3824H / LD SP,3C00H / LD A,38H / LD I,A / … LDIR`),
danach laufen die Familien auseinander: K6313 → `OUT (F4H),00H / OUT (ECH),08H` (CTC-Vektoren), K6316 → `CALL 02DEH`, `OUT (8EH),…`.

**Vektortabellen** (`LD (38xxH),HL` beim Anlauf; 12128062 | 33538013 | 30835007 mit Basis `F0xxH`) `[E]`:

| Slot | K6311 `12128062` | K6313 10 KiB `33538013` | K6313 16 KiB `30835007` | Quelle |
|---|---|---|---|---|
| 00/02/04/06 | 1EDF / 10DF / 10BD / – | 1B60 / – / – / 1B4F | 1523 / – / – / 1512 | CTC D101 (Kanal 0…3) |
| 08/0A/0C/0E | 1EEE / – / – / 1F20 | – / 1EA1 / – / 03C0 | – / 187D / – / 03F3 | CTC D102 |
| 10/12 | – / – | 0540 / 054D | 051B / – | PIO D1 (Centronics) bzw. SIO B |
| 1A/1C/1E | – / 055A / 05D8 | 0800 / 07B5 / 07F9 | 084B / 07F8 / 0840 | SIO Kanal A |
| 20/22 | 0491 / 1098 | 1B3C / – | 14FD / – | PIO D104 Tor A/B |

Vektoren, deren Slot die Firmware nicht schreibt, sind unbenutzt oder werden an anderer Stelle gesetzt `[?]`.  Die ISR-Adressen unterscheiden sich je Fassung, die Slotbelegung (CTC bei 00–0E,
SIO/PIO-Kassette bei 10–1E, PIO D104 bei 20/22) ist in K6311 und K6313 gleich.

## 4  Speicherplan: braucht jede Fassung einen eigenen?

| Fassung | ROM | RAM | Beleg |
|---|---|---|---|
| K6311 8 KiB | 0000–1FFFH (nur 4 × 2 KiB) | 3800–3BFFH | `[E]`; Platine „nur 8k bestückt" (Liste) |
| K6311/K6313 **10 KiB** | 0000–27FFH | 3800–3BFFH (`SP` bis 3C00H) | `[E]`; alle 12 Abzüge; K6316 identisch |
| K6313/K6314 **16 KiB** | 0000–3FFFH | **F000–F7FFH** | `[E]`; 4 Abzüge (alle F-Adressen in `F000H…F800H`); Modulplan `[S:3]` |
| **32 KiB** (neue Platine, `5c53a001`) | Abzug nur 16 KiB, Etikett 32 KiB | F000–F7FFH wie 16 KiB | `[E]`: dieselbe Adresslage wie die 16-KiB-Abzüge; was bei 32 KiB oberhalb 4000H liegt, ist nicht belegt `[?]` |

* **10 KiB und 16 KiB brauchen je einen eigenen Speicherplan im Kern** (RAM-Lage 3800H gegen F000H, ROM-Länge, Vektortabelle `I` = 38H gegen F0H).  Das ist keine Lappalie: in der
  16-KiB-Fassung läge RAM bei 3800H gar nicht — dort steht ROM.
* **32 KiB braucht keinen eigenen** Plan, solange nur die 16-KiB-Abzüge laufen; `5c53a001` ist ein 16-KiB-Abzug auf einem 32-KiB-Chip und gehört in den 16-KiB-Plan.  Ein echter
  32-KiB-Abzug läge erst vor, wenn die obere Hälfte benutzt ist `[?]`.
* K6311 8 KiB und 10 KiB können **einen** Plan teilen (ROM 0000–27FFH, die obere Hälfte leer/FF).

## 5  Zeichengenerator

* **K6316** `[doc/printer/k6316/hardware.md §10]`: 8-Byte-Zellen bei `2400H` (Zweitsatz `2100H`), Code & 7FH.
* **Berichtigung WP8d:** Zelle = 10 Byte (Nadel-9-Maske + 9 Spalten) bei **2300H + 10·(Code−20H)**, nicht 2380H (Merkposten Nr. 33).
* **K6313 10 KiB** `[E]` (Stand WP8a, überholt): **9-Byte-Zellen** (9×9-Raster, `[H]`: „7 × 9 Punkte + 2 × 9 für Unterlängen"), Daten im Bereich `2380H…26BFH` (z. B. 'A' = `E1 DF B7 7F F7 7F B7 DF E1`, invertiert wie im K6316,
  Bit 0 = Nadel schlägt an), danach FF-Lauf bis `27F7H`; **16-Byte-Ersatztabellen** je Land bei `2130H` (`#$@[\]^`{|}~()<>` für US) — K6311 bei `2220H`.  Index der Tabelle = DIL 8-1…9-2 (`(3884H)`).
  Das 16-Byte-Modell des K6316 passt **nicht**; das Zellenformat, die Spaltenreihenfolge und die Zeilenzuordnung sind nicht nachgebildet (`[?]`, Orakel nötig).
* **K6313 16 KiB:** Lage des Generators nicht ermittelt (Tabelle `#$@[…` nicht als zusammenhängende Zeile auffindbar); die Nullfläche `25ACH…2A7DH` in `36539211` und das Fehlen der 10-KiB-Tabellenstruktur
  deuten auf eine andere Anordnung `[?]`.

## 6  K6311 (ohne Schaltplan — **alles `[?]`**, nur aus den fünf Abzügen erschlossen)

* **Dieselbe E/A-Landkarte wie K6313** (F8–FB, F4–F7, EC–EF, DC–DF; SIO bei 7x, PIO bei Bx): die Platine des K6311 ist also entweder eine Vorstufe der K6313-CCB oder ihr gleich — **Annahme**, dass
  alle Bausteinadressen aus §2 gelten `[?]`.  Das K6311 fehlt in `[S:*]`; nichts davon ist am Gerät geprüft.
* **Speicher:** ROM 0000–27FFH (8 KiB bei zwei Abzügen), RAM 3800–3BFFH; Stapel `3BFEH` (V.24 1.21) bzw. `3C00H` `[E]`.  Platine mit 2 × 0,5 k RAM (1 KiB) wie K6316 oder 1 × 2 k RAM (Liste) — Firmware nutzt 1 KiB `[E]`.
* **Unterschiede zum K6313 im Anlauf** (`12128062` ab `0000H`): `CALL 03ABH` statt der CTC-Vektorschreibung; Initialwert aus ROM-Byte `(27F7H)` nach `(3836H)`, `(386DH)`, `(383BH)`
  (Zeichensatz-/Betriebsvorgabe `[?]`); `IN A,(BCH)` liest beim Anlauf einen Eingang, aus dem Bit 5/6/7 die **Baudrate** ableitet (`OUT (EEH),n`, n = 1, 2, 8, 20H, ×2) `[E]` — im K6313
  kommen Baud und Format dagegen aus der DIL-Matrix (D105).  `BC–BF` ist beim K6311-V.24 also ein Eingangs-PIO für die Schalter der V.24-Platine `[?]`.
* **Centronics-Variante** `12228062`: kein SIO-Zugriff, `BC/BD/BE/BF` als Datenpfad (wie K6313); **IFSS-Variante** `13325212`: nur SIO-Kanal B (`7D`, `7F`), kein `BC–BF` `[E]`.
* **Die zwei Abzüge …802 und …803 unterscheiden sich in 1 Byte** (`27F7H`: `20H` / `30H`), das Programm kopiert es an den Anlauf in drei RAM-Zellen.  Die Annahme „Zeichensatz-Voreinstellung
  (Deutsch / UK)" ist durch die Listenangaben (GERMAN / UK ASCII) gestützt, nicht belegt `[?]`.
* Bedienmatrix, Sensoren, Motoren, LED-/Tastenbelegung des K6311: **nicht aus dem ROM erkundet**; solange kein Plan vorliegt, gelten die K6313-Annahmen mit dem Etikett „Hardware erschlossen" (Entwurf 49 §11.3, WP8e).

**Berichtigung WP8f (2026-11-14):** die Annahme „Bedienmatrix, Sensoren, Motoren, LED-/Tastenbelegung des K6311: nicht aus dem ROM erkundet, es gelten die K6313-Annahmen“ galt nicht: der K6311 hat eine **andere Bit-Belegung** (Wagen F9H, Papier F8H, Nadeln DDH/DCH.0, Tasten D104 A2 + D105 A4–A6, Papierende D104 A6) und einen 8-Byte-Zeichengenerator bei 2280H.  Volltext: Merkposten drucker.md Nr. 35.  Die Zuordnung `27F7H = 20H/30H` ist ebenfalls berichtigt (US-ASCII bzw. deutscher Satz mit Pfund, nicht Deutsch/UK).

## 7  Empfehlung

1. **Zuerst fahren: `33538013`** (3.35-3-80/13, 10 KiB, V.24/CL/CEN EPS INT.): häufigstes Etikett, 10-KiB-Speicherplan = K6316-Plan, Schaltplan (CCB) vorhanden.
   Danach **`33538513`** (nur 113 Byte Unterschied, „1715"), `33538028` (HCD 80), `33830803` (ISO/IFSP), `53538500` (neue Platine).
2. **Dann die 16-KiB-Fassung mit `30835007`** (3.08-3-50/07, Prüfprogramme): eigener Speicherplan (ROM 0000–3FFFH, RAM F000–F7FFH, `I` = F0H).  `37239503` (nur Centronics) ist der einfachste Fall (kein SIO).
3. **Nicht als Vorgabe:** `33538511` (defekt), `36539211` (Kennung ≠ Etikett, Nullfläche), `30835207` (Quelle: „nur 10k"; Messung dagegen), `3b834000` (Nullfläche, Verdacht), `5c53a001` (32-KiB-Etikett, 16 KiB Daten).
4. **K6311 zuletzt** (kein Plan): erst `12128062` (V.24) und `12228062` (Centronics), 8-KiB-Abzüge danach.
5. **Zwei Speicherpläne** im Kern (10 KiB / 16 KiB, jeweils mit eigener RAM-Lage und Vektortabelle); ein dritter für 32 KiB erst, wenn ein Abzug mit Daten oberhalb 4000H vorliegt.
6. Neue **Karte K6313-CCB** (nicht die K6316-Karte aufweichen): Dekoder „eine Leitung tief", 2 PIO + 2 CTC, SIO/PIO-Kassette als austauschbares Modul (Kennung über PIO D104 B4).

## 8  Offene Fragen

* Plan der V.24-/IFSS-/IFSP-Kassette und des 10-KiB-Speichermoduls fehlen — SIO-Adressen, Interrupt-Kette und Kassettenkennung sind nur aus der Firmware belegt.
* Reihenfolge der Interrupt-Kette (Drahtnummern) und die Zuordnung O0/O1 gegenüber O2/O3 des 16-KiB-Moduls sind Drahtlesung, durch die Firmware **plausibilisiert**, aber nicht durchverfolgt.
* Zeichengenerator-Format (9-Byte-Zelle, Ersatztabellen) und die Lage im 16-KiB-ROM sind nicht ausgewertet; das braucht ein Orakel wie `doc/printer/k6316/hardware.md` §10.
* Tastenmatrix-Belegung (welche Taste/welcher DIL-Schalter an welchem Kreuzungspunkt) ist nirgends belegt.
* Bedeutung von `PE1`-Kennung (V.24 = 0 / Centronics = 1) bei IFSS/IFSP; ob `BC–BF` im V.24-K6311 wirklich ein PIO oder ein Eingangsregister ist.
* Zahlen aus dem Servicehandbuch (nur gescannt) stammen aus OCR; vor der Modellierung am Bild gegenprüfen.

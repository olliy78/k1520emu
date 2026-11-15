# K6313 / K6314 — Hardwarebefund (Entwurf 49 §11, WP8a)

Stand 2026-11-14.  Quellen in `original/`.  Marken: `[S:n]` Seite *n* von `K6313_14_Serviceschaltplaene.pdf` (5 Seiten, Bilder, handgelesen),
`[M]` Manual Teil 1–3 (Textebene), `[D]` `K6313_DIPswitches.png` / `Drucker_K6313_K6314_Dipschalter_usw.pdf`, `[H]` `K6313_14_ServiceHB.pdf`
(**nur Bilder; Text per OCR gelesen, Zahlen daraus sind `[OCR]` und unsicher**), `[F]` Firmware (`doc/EPROMS/K6313/`, Disassembler `tools/z80_disasm2.py`),
`[E]` aus der Firmware erschlossen, `[?]` Annahme.  Gliederung wie `doc/printer/k6316/hardware.md`; Firmwareseite und K6311: `firmware_diff.md`.

**Schaltpläne vorhanden:** `[S:1]` Bestückungsplan CCB 1.92.606800.7/09 („für neues Keyboard"), `[S:2]` Centronics-Interface 1.92.606275.8/04 (56-260-6275-6),
`[S:3]` Speichermodul 1.92.606316.4/04 (56-260-6316-2), `[S:4]` **CCB 1.92.606800.7/04** (Stromlaufplan 201.1, Stand 3.89), `[S:5]` Änderung CCB (Lötbrücke am
Mikroprogramm-Stecker `XBC1`).  **Nicht vorhanden:** Stromlaufplan der **V.24-/IFSS-/IFSP-Interfacekassette** und des **alten 10-KiB-Speichermoduls**
(05-260-6111-0 / BIMAR 6312) sowie der Leistungsteil (Endstufen, Netzteil).

## 1  Bausteine

CCB `[S:4]` (Steuerplatine, „Control Circuit Board"): ZVE **U880** (D103), **2 × PIO U855** (D104, D105), **2 × CTC U857** (D101, D102), Quarz **9830,4 kHz** mit
Teiler DL093 (D107) → CP = **2,4576 MHz** (wie K6316), Monoflops DL123 (D116) und Zähler DL093 (D106, D117) für Nadelimpuls und Chopper, Flip-Flop DL175 (D115,
Wagenmotor-Phasen), Dekoder **CD4028** (D110, Tastenmatrix), Leitungstreiber DL037 (D109, D111), NAND DL000 (D108, D112–D114).  **Kein E/A-Dekoder-IC**:
die Bausteine werden **direkt von den Adressleitungen A2…A7** freigegeben (§2) — das ist der große Unterschied zum K6316 (DS8205-Dekoder, 3 PIO).
Kein SIO auf der CCB: der SIO sitzt auf der V.24-/IFSS-Kassette `[M, H]`, die Centronics-Kassette trägt statt dessen eine **PIO** (D1 U855) `[S:2]`.
Interrupt-Kette `[S:4]`: CTC D101 → CTC D102 → PIO D104 → PIO D105 → /JEO zur Interfacekassette (Drahtnummern 51/54/52/55) `[?]` — die Verbindung der IEI/IEO ist im Plan
nur über Drahtnummern gegeben; die Reihenfolge ist aus der Beschriftung gelesen, nicht durchverfolgt.

Der Drucker besteht aus **CCB + Speichermodul** (EPROM = Mikroprogramm **und** Zeichengenerator, RAM) **+ Interfacekassette** `[H]`; beide Module stecken
(`XS01` Speichermodul, `XS11`/`XB02` Interface) — ein Austausch verlangt passende DIL-Schalter und passendes Speichermodul `[H 2.3.17]`.

## 2  Speicher und E/A

### 2.1  Speicherplan: zwei Fassungen, ein Bus

Die Bausteine auf der CCB tragen **keinen** Speicher.  Speicher sitzt nur im Speichermodul; dessen Steckverbinder führt **A0…A14** (kein A15), `/MREQ`, `/RD`, `/WR`,
`/PGM`, `/VPP5`, D0…D7 `[S:3]`.  Daraus folgen zwei Speicherpläne, **beide aus der Firmware belegt `[E]`**:

| Fassung | ROM | RAM | Stapel | Beleg |
|---|---|---|---|---|
| **10 KiB** (5 × 2716, Platine 05-260-6111-0 / BIMAR 6312) | 0000–27FFH | **3000–33FFH** (Punktdaten, WP8d) und **3800–3BFFH** (Variablen), Stapel von 3C00H | `LD SP,3C00H`, `LD IX,3824H`, `I` = 38H | `[F:0000]` aller 10-KiB-Abzüge, wie K6316.  Das Speichermodul-Schaltbild fehlt `[?]` (Dekodierung des 10-KiB-Moduls unbekannt) |
| **16 KiB** (1 × 27128, Modul 10-260-6316-x / SDM 80-5) | 0000–3FFFH | **F000–F7FFH** (2 KiB) | `LD SP,F800H`, `LD IX,F024H`, `I` = F0H | `[F:0000]` der 16-KiB-Abzüge; Dekodierung `[S:3]` unten |

**Dekodierung des 16-KiB-Moduls `[S:3]`** (Sheet „201.1 Stromlaufplan", Drahtlesung): Dekoder **DS8205 (D8)**, `A0` ← A13, `A1` ← A14, `A2` ← 0 (Masse);
Freigabe `E0` = `E1` = `/MREQ`, `E2` = NAND(`/WR`, `/RD`) (also nur bei Lesen oder Schreiben).  Damit zerfällt der 32-KiB-Bereich in vier 8-KiB-Viertel:

| Viertel (A14,A13) | Ausgang | Wirkung |
|---|---|---|
| 0000–1FFFH, 2000–3FFFH | O0, O1 | über D9 (NAND + Inverter) → `/CE` des **27128 (D1)**, EPROM adressiert A0…A13 selbst `[?]` — die Zuordnung O0/O1 gegenüber O2/O3 ist wegen kreuzender Leitungen gelesen, nicht durchverfolgt; sie stimmt mit der Firmware (ROM ab 0, RAM bei F000H ≡ 7000H) überein |
| 4000–5FFFH | O2 (Draht 28) | `/CE` **6116 D3** (2 KiB; Variante „nicht bestückt" bei 10-260-6316-4) |
| 6000–7FFFH | O3 (Draht 29) | `/CE` **6116 D2** und die vier **U214 (D4…D7)** als 2 × 1 KiB über A10 (D10 teilt: `/CS` D4+D6 = A10 oder O3, D5+D7 = nicht A10 oder O3) |

* **A15 fehlt am Modul.**  Die Firmware benutzt F000–F7FFH; auf dem Modul landet das auf 7000H (O3), das 2-KiB-RAM wiederholt sich (6116: 11 Adressleitungen) `[E]`.
  Das ROM erscheint zusätzlich bei 8000–BFFFH.  Der Emulator braucht für die 16-KiB-Fassung deshalb **RAM bei F000–F7FFH (2 KiB)**, ROM 0000–3FFFH; ein Spiegeln
  ist nicht nötig, solange die Firmware nur F000–F7FFH anfasst (Wertebereich `F000H…F800H` in allen vier 16-KiB-Abzügen gemessen).
* **Bestückungsvarianten `[S:3]`** (Tabelle „nicht bestückt"): 10-260-6316-4: D1 (Fassung statt Bauteil), D2, D3, C2, C3 · 6317-2: D3…D7, D10, C3…C7 · 6318-0: D4…D7, D10, C4…C7 ·
  6319-7: D2, D3, C2, C3.  Passt zu den Angaben der Liste (`1×16k ROM + 4×0,5k RAM` = D1 + D4…D7; `1×16k ROM + 1×2k RAM` = D1 + D2; `2×8k & 2×2k = 20k RAM` = Nachrüstungen).
* **ROM/RAM-Umschaltung der Kassette** (Forum 6421, Beiträge 13–16, `[?]` widersprüchlich): die LS00-Logik „oberhalb der ROM-Endadresse auf RAM" ist im
  Plan als O0…O3 + D9/D10 zu sehen; „Pin B8" des Forums kommt im Plan des 16-KiB-Moduls **nicht** vor (`XS01` führt dort A0…A14, `/MREQ` = A8 — das bestätigt die Forumsangabe zu A8 —, `/WR`, `/RD`, `/PGM`, `/VPP5`, D0…D7 `[S:3]`); eine A15-/Refresh-Umschaltung
  ist für das 16-KiB-Modul nicht belegbar, die Umschaltung ROM/RAM ist die Viertel-Dekodierung.  Für die 32-KiB-Module der neuen Platine (A15 über `XBC1`, Änderung `[S:5]`) liegt kein Plan vor `[?]`.
* **Mikroprogramm-Stecker `XBC1`, Änderung `[S:5]`:** Leiterzug auftrennen, Brücke löten an der U880-Ecke (vermutlich A15) — nur Skizze `[?]`.

### 2.2  E/A-Adressen (Dekodierung „eine Leitung tief")

Jeder Baustein hat `/CE` an **einer** Adressleitung (aktiv low) und `B/A` = A0, `C/D` = A1 (PIO) bzw. `CS0/CS1` = A0/A1 (CTC) `[S:4, S:2]`.  Die Firmware benutzt genau diese
Adressen (alle 14 K6313-/5 K6311-Abzüge; OUT/IN-Operanden und `OUT (C)` aus der Disassembly):

| E/A | Leitung | Baustein | Registerfolge (Gegenprobe `[F]`) |
|---|---|---|---|
| 7CH–7FH | A7 = 0 | **SIO** auf der V.24-/IFSS-Kassette `[?]` (Kassettenplan fehlt) | 7C = A-Daten, 7D = B-Daten, 7E = A-Steuer, 7F = B-Steuer: `OUT (7FH),18H` (Kanalreset) + `02H`,`10H` (WR2 = 10H, Vektor über B) und `OUT (7EH),…` mit WR4 = 44H/…, WR5, WR3 `[F:0641–06C8]` |
| BCH–BFH | A6 = 0 | **PIO D1** der Centronics-Kassette `[S:2]`; im K6311-V.24 ein weiterer PIO/Eingang `[?]` | BC = A-Daten (8 Datenleitungen, Eingang), BD = B-Daten (Handshake/Statusleitungen), BE = A-Steuer, BF = B-Steuer `[F:03DC–0402]` |
| DCH–DFH | A5 = 0 | **PIO D105** (Tastenmatrix, Nadeln 1…8) `[S:4]` | DC = A-Daten, DD = B-Daten, DE = A-Steuer, DF = B-Steuer |
| ECH–EFH | A4 = 0 | **CTC D102** `[S:4]` | Kanal 0 = EC (Vektor-Schreibung `OUT (ECH),08H`), 1 = ED, 2 = EE (Baudtakt), 3 = EF |
| F4H–F7H | A3 = 0 | **CTC D101** | Kanal 0 = F4 (Vektor `OUT (F4H),00H`), 1 = F5, 2 = F6 (Wagenmotor-Takt `CMT`), 3 = F7 |
| F8H–FBH | A2 = 0 | **PIO D104** (Motoren, Sensoren, Nadel 9) | F8 = A-Daten, F9 = B-Daten, FA = A-Steuer, FB = B-Steuer |

Es gibt keinen Baustein bei 80H–AFH (K6316) und keine Mischadressen — die beiden Familien teilen **keine** E/A-Adresse (`firmware_diff.md` §2).
Bei „eine Leitung tief" würden sich Bausteine überlagern, wenn mehrere der Leitungen A2…A7 gleichzeitig tief sind; die Firmware erzeugt das nie (alle gemessenen Adressen haben genau eine tiefe Leitung) `[E]`.

### 2.3  Unterbrechungsvektoren (`IM 2`, `I` = 38H bzw. F0H)

| Vektor | Quelle | Beleg |
|---|---|---|
| 00H–06H | CTC D101 Kanal 0–3 | `OUT (F4H),00H` `[F:001C]` |
| 08H–0EH | CTC D102 Kanal 0–3 | `OUT (ECH),08H` `[F:0020]` |
| 10H–1EH | SIO (V.24/IFSS) **oder** PIO D1 (Centronics) | SIO: WR2 = 10H; Centronics: `OUT (BEH),12H` (Tor A), `OUT (BFH),10H` (Tor B) `[F:03DC/03E8]` |
| 20H / 22H | PIO D104 Tor A / B | `OUT (FAH),20H` `[F:0084]` |

Tor-Einrichtung D104/D105 (`[F:002A–0096]`, 33538013): `DE`: Betriebsart 3 (`FFH`) + Maske `0FH` (A0…A3 Eingang, A4…A7 Ausgang), `DF`: Betriebsart 3 + Maske 00H (alle Ausgang),
`FA`: Betriebsart 3 + Maske `05H` (A0, A2 Eingang), `FB`: Betriebsart 3 + Maske `10H` (B4 Eingang).

## 3  PIO-Belegung (CCB, `[S:4]`)

| Tor | Bit | Signal | Richt. | Bedeutung |
|---|---|---|---|---|
| D104 A (F8H) | 0 | — | ein | unbeschriftet; Maske sagt „Eingang" `[?]` (Kandidat Haubenkontakt) |
| | 1 | /CM1 | aus | Wagenmotor Phase 1 (→ D115 DL175 → /CMA1) |
| | 2 | PE | ein | Papierende `[S:4]`, Maske 05H bestätigt Eingang; treibt zugleich /LPE `[?]` |
| | 3 | /CM2 | aus | Wagenmotor Phase 2 |
| | 4 | /CMHC | aus | Wagenmotor Strom: **Halten = L** (1 Phase, 350 mA gechoppert) / **Lauf = H** (2 Phasen, 600 mA) `[H, OCR]` |
| | 5 | /CM3 | aus | Wagenmotor Phase 3 |
| | 6 | LON | aus | LED ON LINE (über DL037 D111 → /LON) |
| | 7 | /CM4 | aus | Wagenmotor Phase 4 |
| D104 B (F9H) | 0 | PP9 | aus | **Nadel 9** (über NAND D112 mit /IPC) |
| | 1 | PFA1 | aus | Formularmotor Phase 1 |
| | 2 | /PC | aus | Freigabe der Nadelendstufen / Taktzähler (`/IPC` an D112–D114, Rücksetzen von D106/D117) `[?]` |
| | 3 | PFA2 | aus | Formularmotor Phase 2 |
| | 4 | PE1 | ein | **Kassettenkennung**: Centronics-Kassette zieht über R14 (27 k) nach +5 V (`[S:2]`) → 1; die Firmware liest `IN A,(F9H)` Bit 4 beim Anlauf und setzt bei 0 das Kennzeichen „V.24/SIO-Betrieb" `(IX+40)` Bit 4 `[F:0096–009C]` |
| | 5 | PFA3 | aus | Formularmotor Phase 3 |
| | 6 | TDS | ein | **Taktscheibe** Druckwagen (Interrupt-/Abfrageeingang, X807 Pin 1) `[S:4]` |
| | 7 | PFA4 | aus | Formularmotor Phase 4 |
| D105 A (DCH) | 0…3 | KB04…KB01 | ein | **Zeilen** der Tastenmatrix (Pull-ups R110–R113, Anschlüsse X502…X505) |
| | 4 | /LERR | aus | LED ERR |
| | 5, 6, 7 | KBD2, KBD3, KBD1 | aus | Spaltenwahl: 3 Bit → **D110 CD4028** (4. Eingang = 0) → KB11…KB18 (8 Spalten, X511…X518) |
| D105 B (DDH) | 0…7 | PP8…PP1 | aus | **Nadeln 8…1** (B7 = PP1) über NAND D112–D114 mit /IPC → /IPP1…/IPP8 |

Beim Anlauf (`[F:0036–003D]`) werden `DFH←00H` (Nadeln inaktiv), `DDH←FFH` und `FAH←FFH,05H` ausgegeben; Nadeln und `/PC` sind also high im Ruhezustand `[?]`
(Polarität der Endstufen nicht aus dem Plan lesbar; im K6316 `0` = Nadel schlägt an).  Die LEDs `/LPE` (PE, PEX), `/LPON` (PON) kommen nicht aus den PIOs:
**/LPE** = NAND(PE, PEX) `[S:4, D109]`, **/LPON** = NAND(PON, …) — das Netz-Lämpchen hängt am Power-On-Signal `PON` (Pin A2 des Rückwand-Steckers).

## 4  Zeitgeber

* **WP8d [S:4]:** D102 `ZC/TO0` (HCP, Draht 57) → D101 **C/TRG1 und C/TRG2** (Spalten- bzw. Schrittzähler); C/TRG0/C/TRG3 nur Prüfpunkte 169/170.  Maße und Zeichengenerator: Merkposten Nr. 33.
* **CTC D101** (F4–F7): `ZC/TO0` → Monoflop **D116/DL123** (R119 20,5 kΩ, C109 68 nF; zweites Monoflop R120 10 kΩ, C110 1 nF) → Nadelimpulsbreite `[S:4]`;
  `ZC/TO2` = **CMT** → Takt des Flip-Flops D115 (Übernahme der Wagenmotor-Phasen /CM1…/CM4 → /CMA1…/CMA4) `[S:4]`; `ZC/TO1` unbeschaltet.
* **CTC D102** (EC–EF): `ZC/TO0` = **HCP** (Chopper-Takt `[?]`), `ZC/TO2` = **RTC** → Sende-/Empfangstakt der SIO (Baudrate).  Firmware: `OUT (EEH),n` mit Teiler
  **1 = 9600 Bd, 2 = 4800, 8 = 1200, 32 = 300** (Takt φ/16 = 153,6 kHz, ×16-Betrieb) `[F:00AD–00CC, 062F–063C]`; Auswahl über die DIL-Schalter 14-2/15-1/15-2 (§5).
* **Power-On-Reset** `[S:4]`: Zeitglied R104/C103 mit Schwellenschalter A302 (N101) → `/RES`; `PON` führt über R105/C105 auch auf **NMI** (Netzausfall `[?]`).

## 5  Bedienfeld, Tasten, LEDs, DIL-Schalter

Das Bedienfeld (Keyboard-Platine, Lötbrücken zur CCB) trägt `[M, H]`: **4 Tasten** `ON/OFF LINE`, `LINE FEED ↑` (vorwärts), `LINE FEED ↓` (rückwärts), `FORM FEED`;
**4 LEDs** `ON LINE` (gelb), `POW` (grün), `ERR` (rot), `PE` (gelb); **14 DIL-Paare** 5-1…18-2 sowie einen **Summer** (3,3 kHz `[H, OCR]`; im K6316 gibt es keinen).
Tasten und DIL-Schalter hängen in **einer Matrix** 4 Zeilen (KB01…KB04 = D105 A0…A3) × 8 Spalten (KB11…KB18 = 4028-Ausgänge): ein PIO-Tor trägt alle Eingänge `[H]`.
Welche Taste/welcher Schalter auf welchem Kreuzungspunkt liegt: **offen** — dazu fehlt der Keyboard-Plan (`[S:1]` ist nur ein Bestückungsplan; `X501…X520` und die Sammelleitungen sind nicht verfolgbar) `[?]`.
Firmwarebefund: nach dem Einschalten wird `IN A,(DCH)` mit Maske `0CH`/`50H` ausgewertet (Einschalttasten LINE FEED ↑/↓/FORM FEED: Selbsttest, ständiger Selbsttest, Hexdump `[M 1.4.5]`) `[F:018C–0199]`.

**LED-Treiber:** DL037-NANDs D109/D111 mit Vorwiderständen R114–R117: `/LON` (D104 A6), `/LERR` (D105 A4), `/LPE` (PE & PEX), `/LPON` (PON); Anschlüsse X506…X509 (Keyboard-LED-Seite).

### 5.1  DIL-Schalter `[D, M Teil 1 S. 25/26/69]`

Gemeinsame Schalter (EPSON-Fassung; „mit NLQ" nur bei Programmvarianten mit NLQ):

| Schalter | Funktion | OFF | ON |
|---|---|---|---|
| 5-1 | Haubenkontakt | **darf nicht betätigt werden** (Service) | — |
| 5-2 | nicht benutzt | | |
| 6-1 | Papierendekontakt | wirksam | nicht wirksam |
| 6-2 | Summer | wirksam | nicht wirksam |
| 7-1 | automat. Zeilenschaltung (LF) | kein LF bei CR | automat. LF bei CR |
| 7-2 | automat. Wagenrücklauf (CR) | autom. CR bei LF | kein CR bei LF |
| 8-1, 8-2, 9-1, 9-2 | Internationaler Zeichensatz | Tabelle 1 | |
| 10-1, 10-2 | Schriftart | Tabelle 2 | |
| 11-1 | Nulldarstellung | 0 | Ø |
| 11-2 | Zeilenabstand | 1/6 Zoll | 1/8 Zoll |
| 12-1, 12-2 | Formatlänge | Tabelle 3 | |
| 13-1 | 1 Zoll Papiervorschub über die Falzkante | wirksam | nicht wirksam |
| 13-2 | Steuercodes kompatibel zu | — | EPSON |
| 14-1 | NLQ (nur Varianten mit NLQ) | nicht wirksam | wirksam |
| 14-2…18-2 | Interface-Spezifikation | siehe unten | |

Tabelle 1 (8-1, 8-2, 9-1, 9-2): US ASCII OFF OFF OFF OFF · französisch OFF ON OFF OFF · deutsch ON OFF OFF OFF · UK ASCII ON ON OFF OFF · dänisch OFF OFF ON OFF · schwedisch OFF ON ON OFF ·
italienisch ON OFF ON OFF · spanisch ON ON ON OFF · US ASCII mit japan. Sonderzeichen (Yen) beliebig/beliebig/OFF/ON.  Die Firmware wählt damit eine der 16-Byte-Ersatztabellen bei `2130H`
(33538013; `#$@[\]^`{|}~()<>` für US, je Land abweichend), Index = `(3884H)` `[F:0C6E–0C84]`.
Tabelle 2 (10-1, 10-2): PICA 1/10″ = OFF OFF · ELITE 1/12″ = ON OFF · komprimiert 1/17″ = ON ON · Fettschrift 1/10″ = OFF ON.  Tabelle 3 (12-1, 12-2): 12 Zoll OFF OFF · 11 Zoll ON OFF ·
6 Zoll OFF ON · 5,5 Zoll ON ON.  (Die Spalten sind aus einem Scan gelesen; die markierten Ausgangsstellungen auf `K6313_DIPswitches.png`: 6-1, 6-2, 7-1, 7-2 OFF, 11-1 ON, 11-2 OFF, 13-1 OFF, 13-2 ON, 14-1 ON,
Zeichensatz deutsch, Schriftart PICA, Format 12 Zoll — ein ausgefülltes Beispiel des Anwenders, **kein** Auslieferungszustand.)

**V.24-Kassette** (14-2…18-2) `[D, M Teil 1 S. 69]`: Baudrate 14-2/15-1/15-2: 9600 OFF OFF OFF · 4800 OFF ON OFF · 2400 ON ON OFF · 1200 OFF OFF ON · 600 ON OFF ON · 300 OFF ON ON · 200 ON OFF OFF · 150 ON ON ON.
16-1 Parität (OFF mit, ON ohne), 16-2 (OFF ungerade, ON gerade), 17-1 Protokoll (OFF DTR, ON XON/XOFF), 17-2 (DTR: RTS Ein/Aus; XON/XOFF: Duplex/Halbduplex), 18-1 Datenbits (OFF 7, ON 8), 18-2 Stopbits (OFF mind. 1, ON mind. 2);
14-1 nicht benutzt.  Die Firmware wertet 14-2…18-2 in `sub_0600…06CB` (SIO-Einrichtung) aus `[F]`; Pufferung 1/2 KByte `[M]`.
Centronics: 14-2…18-2 entfallen/haben andere Bedeutung `[M Teil 1 Abschn. 10/11]` — nicht ausgewertet `[?]`.  Für IFSS/IFSP liegt keine Tabelle vor.

## 6  Sensoren, Motoren, Leistungsseite (nur als Signal)

| Teil | Signal | Beleg |
|---|---|---|
| Wagenmotor (Permanentmagnet-Schrittmotor 6°, 60 Schritte/U `[H, OCR]`) | /CM1…/CM4 → D115 → /CMA1…/CMA4, /CMHC (Halte-/Laufstrom) | `[S:4]` Stecker X5530 „Wagenmotor"; Chopper 18 kHz `[H, OCR]` |
| Taktscheibe | TDS (D104 B6), ca. 30 Impulse je Umdrehung `[H, OCR: „38"]`, Interpretation `[?]` | `[S:4]` |
| Formularmotor (3,6°/Schritt, unipolar, gechoppert) | PFA1…PFA4 (D104 B1, B3, B5, B7) | `[S:4]`, `[H 1.2.5]` |
| Druckkopf | 9 Nadeln, PP1…PP9 über D112–D114 (/IPP1…/IPP9, Ausgänge X 5-xx) | `[S:4]` |
| Papierende | PE (D104 A2), PEX (X807 Pin 6 + R106/R107), PE1 an der Kassette | `[S:4]` |
| Haubenkontakt | nicht eindeutig; DIL 5-1 nennt ihn „Haubenkontakt" `[D]` | `[?]` |
| Summer | 3,3 kHz, per Steuerbefehl/PE | `[H]`; Ansteuerleitung `[?]` |
| Netz | `PON` → NMI und /LPON | `[S:4]` |

Die Zahlen aus `[H]` stammen aus einem OCR-Lauf; sie müssen vor jeder Modellierung am Bild geprüft werden.

## 7  Interfacekassetten

* **Centronics** `[S:2]`: U855 (D1) an **BCH–BFH** (`/CS` = A6), `D0…D7` auf `A0…A7`-Tor (Datenleitungen DATA1…DATA8 über je ein NAND D2/D3, Pegelwandler → Tor A, Eingang);
  Tor B: `/STROBE` (B2 am Stecker `XS11`), `/INIT`, `/AUTOFEED`, `/SLCTIN` als Eingänge; `/ACKNLG`, `BUSY`, `PE`, `SLCT`, `ERROR` als Ausgänge über D4/D5-NANDs
  (`XS11` B3, C11, B1, B4, A9) `[S:2]`.  Welches Bit welches Signal trägt, ist aus dem Plan nicht sicher lesbar `[?]` (die Firmware testet `IN A,(BDH)` Bit 3 und 4,
  setzt Bits 1, 2, 5, 6, 7 `[F:0169–026B]`).  `PE1` (Pin A5) ist über R14 (27 k) gegen +5 V und Teil der Kassettenkennung.  `INT` über Pin A2, `JEO` über C2, `IORQ`/`RD`/`M1`/`CP` an der Rückwand `XB02`.
* **V.24 / IFSS / IFSP** (SIO bei 7CH–7FH): kein Plan.  Aus der Firmware: SIO-Kanal A für die Datenleitung (WR4 = 44H ×16, 1 Stopp), Vektor 10H über Kanal B, Taktquelle = CTC D102 Kanal 2 `[F]`.
  Das Servicehandbuch nennt IFSS und IFSP als bitserielle bzw. bitparallele Schnittstellen `[H]`; die Kennzeichnung der Interfacekassette liest die Firmware aus `PE1` (V.24: 0, Centronics: 1).
* **Rückwandstecker `XB02/XS02`:** D0…D7, A0, A1, A6, `/M1`, `/IORQ`, `/RD`, CP, JEO, `/INT`, PE1 (an der Centronics-Seite gelesen) — der Interfacebus ist **kein** K1520-Bus.

## 8  Unterschiede zum K6316 (Kurzfassung; Einzelheiten `firmware_diff.md`)

| | K6316 | K6313/K6314 |
|---|---|---|
| E/A | 80H–AFH, DS8205-Dekoder, 3 PIO + 2 CTC + Modul-SIO/PIO | **7CH–FBH, „eine Leitung tief" (A2…A7)**, 2 PIO + 2 CTC + SIO/PIO auf der Kassette |
| Speicher | 0000–27FFH ROM, 3800–3BFFH RAM | 10 KiB wie K6316; **16 KiB: ROM 0000–3FFFH, RAM F000–F7FFH** |
| Stapel | 3C00H | 3C00H (10 KiB) / **F800H** (16 KiB) |
| Zeichengenerator | 8-Byte-Zellen (`2400H`, Zweitsatz `2100H`) | **9-Spalten-Zellen** (9×9-Raster), Ersatztabellen 16 B bei `2130H` |
| Tasten | Diodenmatrix, DIL über Adapterplatte | Matrix 4 × 8 über CD4028, **4 Tasten**, 14 DIL-Paare, Summer |
| Nadeln | 8 + 1 (Port 8DH/8CH) | 8 + 1 (D105 B / D104 B0), **Impulsbreite über Monoflop** D116 |
| Sensoren | Lichtschranken FSO/FSU (Vorsteckbeleg), PE, Haube MHA | PE, PEX, TDS, PE1 (Kassettenkennung); kein Vorsteckbeleg |
| LEDs | H1…H5 | ON LINE, POW, ERR, PE |
| Takt | 9830,4 kHz / 4 = 2,4576 MHz | gleich |

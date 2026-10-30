# N10a — AUP 062-8531, K7633, K3521, OSS 062-8471: Auswertung der Unterlagen

Stand 2026-10-17, Zweig `feature/a5130_k8924`.  Reine Analyse, kein Kerncode.  Kennzeichnung wie in Entwurf 33:
**[SRC]** = im Beleg gelesen (mit Fundstelle), **[?]** = Annahme/offen, **[Schluss]** = gefolgert, nicht belegt.

## 0. Gelesene Quellen und ihr Zustand

| Quelle | Umfang | Zustand |
|---|---|---|
| `~/projects/robotron/K1520/Adapter_Universelle_Ports_AUP_062_8531_Betriebsdokumentation.pdf` | 11 S. | **alle Seiten lesbar**. Aber **unvollständig/gemischt**: das Inhaltsverzeichnis (S. 2) nennt Abschnitte, die im Scan fehlen — „Technische Daten / Konstruktiver Aufbau K 7633 und Kundentastatur“, „Reparaturanleitung/Wartungsvorschrift“, **„Serviceschaltpläne“**.  Die Kapitelnummern im Text (III Bauelementebasis, IV Funktionsbeschreibung) passen nicht zum Verzeichnis (IV/V).  Vorhanden sind: Verwendung, Technische Daten, Bauelemente, Funktionsbeschreibung 1–3, Kontaktbelegung X1/X2.  Handschriftliche Randnotizen eines Vorbesitzers (S. 4, 5). |
| `…/tiffe_de_Robotron/www.tiffe.de/Robotron/K1520/K3820/Speichersteckeinheiten_K3525_K3521.30_OSSTyp062-8471_Betriebsdokumentation.pdf` | 22 S. | **neu gefunden** (Entwurf 33 §5 führte OSS als „nicht gefunden“). Vollständig lesbar: K3521.30 (§14) und **OSS 062-8471 (§15) mit Seitensteuerung, Speicher-/E/A-Schutz, Steuerwörtern**. |
| `…/Speichersteckeinheiten_K3525_K3521.30_OSSTyp062-8471_Serviceschaltplaene.pdf` | 13 S. | vorhanden, **nicht gelesen** (für N10f nötig). |
| `…/K1520/K3520_K3820_K3620_Heft2.pdf` | 18 S. | S. 1–6 gelesen: enthält **K3520** (4 K statisch, nMOS, Datenerhalt nur über externe Schlafspannung 5PG), **nicht K3521** — K3521 steht in Heft 3 (`K1520_Heft3.rar`, nicht entpackt) bzw. im OSS-Heft oben (K3521.30). |
| `…/Robotron_Technik/…/html/computer/k8924.html` | — | Kartenliste des K8924 Sparkasse (s. §5). |
| `…/A5120/docs/Serviceinformationen_A5120,30, K8924,27/` | 9 Bilder | S. 1–2 angesehen: Monitor K7222 (Funkentstörung) und Druckerkabel — für N10 ohne Belang. |

Nichts gefunden (rg/fd über `~/projects/robotron` inkl. Spiegel Robotron_Technik, sax.de, tiffe.de, und `doc/`):
**K7633-Beschreibung** (außer Foto `bilder/Zubehoer/Tastaturen/Tastatur_K7633_3_k.jpg`), **SIMO**, **AHL/Kartenleser** (nur Fotos
`bilder/Zubehoer/Magnetkartentechnik/K6503/`), **SIOS** (nur die Beschreibungsseite).
**Berichtigung zu Entwurf 33 §5:** die Kartenliste auf robotrontechnik.de führt den AHL als **K1003 062-8660** („Adapter für
Handlese-Einheit für maximal 4 Magnetkarten-Lesegeräte“), nicht K6003 [SRC k8924.html]; K6503 ist der Leser selbst [?].

## 1. AUP 062-8531 — Funktion

[SRC AUP S. 1] Adapterkarte mit Anschlüssen für **zwei serielle Tastaturen** (numerische Terminaltastatur **K7633**, **Kundentastatur**)
und **eine Sonderschnittstelle**; „vorzugsweise im Schalterterminal K 8924“.  Die Tastaturen werden von einer **eigenen CPU U880
(A9)** mit **1-K-EPROM U555 (A7)** bedient (Programm + Codetabellen); zur Tastatur führt außer Betriebsspannung/Masse **eine einzige
Signalleitung** (DIO1 = Terminaltastatur, DIO2 = Kundentastatur).  Die Sonderschnittstelle steuert ein **SIO U856 (A13)**, getaktet von
**Kanal 0 eines CTC U857 (A12)**.  Adressen über Stanzbrücken.  **Fester Steckplatz**, weil über einen 3-reihigen Koppelbus-Stecker
X2 ausgekoppelt wird.  Varianten mit 2. EPROM, V.24 oder RS 232 C „derzeit nicht im Produktionsprogramm“.

Zuordnung zu SIMO: die Unterlage nennt das Wort **SIMO nicht**; dass die Sonderschnittstelle das SIMO-Sicherheitsmodul anschließt,
steht nur auf robotrontechnik.de („Anschluss von Terminaltastatur, Kundentastatur und SIMO“) [SRC k8924.html, Schluss].

### 1.1 Bausteine [SRC AUP S. 2]

| Teil | Bausteine |
|---|---|
| Tastatursteuerung | U880 (A9), U555 EPROM 1 K (A7), U212-Register (A3 Takt/Anzeige, A4 Kommandobyte, A5 Gültigkeit, A6 Tastencode), U216-Bustreiber (A25 u. a.) |
| Busschnittstelle | DS8205 1-aus-8 (A10, A11, A20), U212, U216 (A1, A2) |
| Sonderschnittstelle | SIO U856 (A13), CTC U857 (A12) |

### 1.2 E/A-Adressen (Vorgabe für SIOS 1526) [SRC AUP S. 2]

| Ports | Zweck |
|---|---|
| **40H … 43H** | Tastaturen (Text gescannt „40 … 4?“, die benutzten Befehle sind IN 41, IN 42, OUT 43) |
| **B0H … B3H** | CTC A12 |
| **B4H … B7H** | SIO A13 |

Decodierung: A10/A11 bei inaktivem /M1 und /RESET; A20 (über W1:6/W2:2) für die Tastaturen, W1:7/W1:8 → /CS CTC, W1:9 → /CS SIO.
Brücken W1:1…W1:5 „ohne Bedeutung“ in dieser Variante.

**Kollisionen im Kern: keine.**  A5120 belegt 00–0FH (ZRE), 10–18H (AFS), 50–5FH (ASS), optional 30–37H (K5020), 88/89H (RAF),
A8H (EM), E0–E7H (K6022).  40–43H und B0–B7H sind frei.  (Die OSS liegt auf EEH/EFH, §4 — ebenfalls frei.)

### 1.3 Registermodell der Tastaturseite (Sicht der ZRE) [SRC AUP S. 5–6]

| Befehl | Register | Wirkung |
|---|---|---|
| `IN 41H` | Gültigkeitsregister A5 | zeigt an, dass ein gültiger Tastencode bereitsteht — **für beide Tastaturen unterschiedlich**, so dass die ZRE erkennt, auf welcher Tastatur gedrückt wurde. **Belegt (2026-10-30, Treiber T-K7633, `doc/sios/module_k7633_kut.md`): Bit 1 = Terminal K7633, Bit 0 = Kundentastatur**, Rest egal (`AND 03H`). |
| `IN 42H` | Tastencoderegister A6 | liefert den Tastencode; **das Lesen löscht das Gültigkeitssignal** (A6-23 → A16-12 → A5), danach darf die AUP-CPU den nächsten Code eintragen.  **Nach /RESET und nach `OUT 43H,FFH` steht das Typzeichen `80H \| Variante` darin**, ohne Gültigkeitsbit (der Treiber liest es ohne Test bis zu 65535 Mal; Low-Nibble der K7633 [?], Vorgabe 0). |
| `OUT 43H` | Kommandobyteregister A4 | Kommando an die Tastaturen, von der AUP-CPU zyklisch gelesen („laufend aktualisiert … sofort ausgewertet“). |
| `IN 40H` | — | nicht beschrieben [?]. |

**Kein Interrupt für die Tastaturen**: die ZRE fragt A5 zyklisch ab — Randnotiz „x CTC auf der ZRE-Karte“ zur „2-ms-CTC-Routine“
[SRC S. 5, handschriftlich]. Die AUP-CPU gilt eine Taste als gültig, wenn sie **zweimal hintereinander beim gleichen Zählerstand**
erkannt wurde (Entprellung) [SRC S. 5].

**Kommandobyte (OUT 43H)** [SRC S. 5, Bildzuordnung der Linien]:

| Bit | Bedeutung |
|---|---|
| 7 | Kennung, fest 1 |
| 6 | akustisches Signal (1×): 1 = aus, 0 = ein |
| 5 | Fehlerlampe (blinkend): 1 = aus, 0 = ein |
| 4 | LED der Kundentastatur: 1 = „GET inaktiv“, 0 = „GET aktiv“ (LED leuchtet) |
| 3, 2 | frei |
| 1 | ALPHA-LED: 1 = aus, 0 = ein |
| 0 | SEL-LED: 1 = aus, 0 = ein |

**FFH = RESET AUP** — „dient der Synchronisation beider Mikrorechnersysteme“, die übrigen Kommandos steuern unmittelbar die
optischen/akustischen Anzeigen [SRC S. 6].  **Gegenprobe am laufenden SIOS (2026-10-30):** der Treiber schreibt `FFH` auch als
gewöhnliches Lampenbyte „alles aus“ (Bit 7 = 1, Rest 1) — Reset und „alle Lampen aus“ sind nicht zu unterscheiden; deshalb meldet
das Modell nach `FFH` KEINEN Anlauf der Schlüsselschalter (ein erneuter Code kippte dort den Zustand).

**Annahmen-Stand nach dem Treiberbefund (T-K7633, SYSORG09, Gegenprobe `SiosSgen.GeneriertEinSystemMitK7633UndBootetAmAup`):**

| Annahme | Stand |
|---|---|
| A1 Gültigkeitsbits | **widerlegt und korrigiert**: Bit 1 = K7633, Bit 0 = Kunde (`Config::bit_terminal/bit_kunde`) |
| TYP nach Reset | **belegt, neu**: `80H \| Config::typ` ohne Gültigkeit; Low-Nibble [?] |
| A7 Schlüssel nur beim Einschalten | **widerlegt**: jede Drehung (ein und aus) meldet AAH (SS1) / A9H (SS2); der Treiber kippt je Code. Stecken/Abziehen unbekannt (Anwendervorgabe: nicht erkennbar). Anlauf nach /RESET: je eingeschalteter Schalter ein Code nach dem Typzeichen — [?] |
| A5 Hupe, A6 Kennung, Lampenbyte | **bestätigt** (Bit 0 SEL, 1 ALPHA, 4 GET, 5 Fehler, 6 Hupe einmalig, 7 immer 1) |
| A3 Alpha | **gestützt**: Tabellenwahl folgt dem ALPHA-Bit des Kommandos (SIOS führt den Zustand nach, F05 sendet A1H in beiden Tabellen) |
| A2, A4, A8 | unverändert (A4 Entprellzeit [?]) |

### 1.4 Innenleben (nur zur Einordnung, nicht nachzubilden) [SRC AUP S. 3–5]

Interne Ports der AUP-CPU: `OUT 01` Tastencoderegister A6, `OUT 02` Gültigkeit A5, `IN 04` Tastenbetätigung (A25, DI3 = „A6 noch
nicht abgeholt“), `OUT 08` Takt-/Anzeigeregister A3, `IN 10` Kommandobyte A4.  Taktbildung ≈ **10 kHz** durch zyklisches `OUT 08`;
Leitungspegel low 0–0,8 V, high 7,5–9 V, **„erhöhter high“ 11,2–12,6 V schaltet die Anzeige der gerade abgefragten Spalte ein**
(A3-03/A3-05 = 1/1 low, 0/1 high, 0/0 erhöht).  **Synchronisation**: nach jedem Matrixdurchlauf ein auf ≥ 700 µs verlängerter
high-Takt = RESET der Tastaturzähler.  Tastendruck: Leitung sinkt in der low-Phase nicht unter 2,5 V → V1:1 leitet → A25-04 (K7633)
bzw. A25-07 (Kundentastatur) low.  Tastenposition = Zählerstand.

Für den Emulator folgt daraus [Schluss]: die AUP wird **auf Registerebene** nachgebildet (Hochebene, wie K7637/K7672 ohne deren
Firmware): Taste → nach einer Entprellzeit Code in A6, Bit in A5; `IN 42` löscht; `OUT 43` schaltet Lampen/Hupe.  Die AUP-Firmware
ist **nicht** vorhanden (nur ihre Codetabellen, §1.5) — eine Nachbildung des U880 auf der AUP ist weder möglich noch nötig.

### 1.5 EPROM-Belegung und Codetabellen [SRC AUP S. 6–7]

| EPROM-Adresse | Inhalt |
|---|---|
| 000–330H | Programm |
| 331–338H | Positionscodes der **Dauerfunktionstasten** (1AH, 16H, 0EH, Rest 00) |
| 339–348H | Codetabelle **Kundentastatur** (Positionen 38H–47H) |
| 349–390H | Codetabelle K 7633.51/52 **ohne Alpha** (Positionen 00H–47H) |
| 391–3D8H | Codetabelle K 7633.51/52 **mit Alpha** |

Kundentastatur (Pos → Code): 38–3B → 00, 3C → 36, 3D → 19, 3E → 30, 3F → 19, 40 → 35, 41 → 32, 42 → 31, 43 → 34, 44 → 33,
45 → 39, 46 → 38, 47 → 37.  Also Ziffern `0`–`9` plus zweimal 19H [? Lesart der beiden 19H; Bedeutung, z. B. Korrektur/Ende, offen].

K 7633 (Tastenposition, Pos.-Code, Code ohne Alpha / mit Alpha; `*` = Dauerfunktion):

| Taste | Pos | o. A. | m. A. | | Taste | Pos | o. A. | m. A. | | Taste | Pos | o. A. | m. A. |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| SS1 | 00 | AA | AA | | D07 | 1D | 19 | 19 | | E03 | 36 | C6 | 47 |
| SS2 | 01 | A9 | A9 | | E07 | 1E | 0C | 0C | | F03 | 37 | C2 | 43 |
| A10 | 02 | 12 | 12 | | F07 | 1F | 10 | 10 | | — | 38 | 00 | 00 |
| B10 | 03 | 33 | 33 | | D13 | 20 | F9 | F9 | | — | 39 | 00 | 00 |
| C10 | 04 | 36 | 36 | | D12 | 21 | FD | FD | | A02 | 3A | D9 | 57 |
| D10 | 05 | 39 | 39 | | A05 | 22 | DC | DC | | B02 | 3B | D4 | 52 |
| E10 | 06 | 0A | 0A | | B05 | 23 | D7 | 55 | | C02 | 3C | CF | 4E |
| F10 | 07 | A0 | A0 | | C05 | 24 | D2 | 2C | | D02 | 3D | CA | 4A |
| A13 | 08 | FC | FC | | D05 | 25 | CD | 2E | | E02 | 3E | C5 | 46 |
| C13 | 09 | FA | FA | | E05 | 26 | C8 | 2F | | F02 | 3F | C1 | 42 |
| A09 | 0A | 11 | 11 | | F05 | 27 | A1 | A1 | | A12 | 40 | FF | FF |
| B09 | 0B | 32 | 32 | | B13 | 28 | FB | FB | | C12 | 41 | FE | FE |
| C09 | 0C | 35 | 35 | | B12 | 29 | 00 | 00 | | A01 | 42 | D8 | 56 |
| D09 | 0D | 38 | 38 | | A04 | 2A | DB | 59 | | B01 | 43 | D3 | 51 |
| E09* | 0E | 07 | 07 | | B04 | 2B | D6 | 54 | | C01 | 44 | CE | 4D |
| F09 | 0F | 01 | 01 | | C04 | 2C | D1 | 50 | | D01 | 45 | C9 | 49 |
| — | 10 | 00 | 00 | | D04 | 2D | CC | 4C | | E01 | 46 | C4 | 45 |
| F12 | 11 | AF | AF | | E04 | 2E | C7 | 48 | | F01 | 47 | C0 | 41 |
| A08 | 12 | 30 | 30 | | F04 | 2F | C3 | 44 | | | | | |
| B08 | 13 | 31 | 31 | | — | 30 | 00 | 00 | | | | | |
| C08 | 14 | 34 | 34 | | — | 31 | 00 | 00 | | | | | |
| D08 | 15 | 37 | 37 | | A03 | 32 | DA | 58 | | | | | |
| E08* | 16 | 06 | 06 | | B03 | 33 | D5 | 53 | | | | | |
| F08 | 17 | 0B | 0B | | C03 | 34 | D0 | 4F | | | | | |
| E13 | 18 | F8 | F8 | | D03 | 35 | CB | 4B | | | | | |
| E12 | 19 | DD | DD | | | | | | | | | | |
| A07* | 1A | 20 | 20 | | | | | | | | | | |
| B07 | 1B | 00 | 00 | | | | | | | | | | |
| C07 | 1C | 2D | 2D | | | | | | | | | | |

Plausibilitätsprobe: LOC = 349H + Pos bzw. 391H + Pos stimmt an allen im Beleg angegebenen Stützstellen (36E/36F/370, 3B6–3B8, 390,
3D8).  Ziffernblock A08…D10 = 30H…39H; „mit Alpha“ liefert ASCII-Großbuchstaben (F01 = `A`, … A04 = `Y`), „ohne Alpha“ Funktionscodes
C0H–DCH.  **SS1/SS2 (AAH/A9H) sind die beiden Schlüsselschalter** [Schluss aus Entwurf 33 §4: Anwender/Administrator].
Offen [?]: wer zwischen den Tabellen umschaltet (AUP-CPU anhand ALPHA-LED-Bit oder einer eigenen ALPHA-Taste — „K7633.51/52 ohne/mit
Alpha-Taste“ kann auch zwei *Tastaturtypen* meinen), Wiederholrate der Dauerfunktionstasten, Beschriftung der Tasten (nur Foto),
Senden beim Loslassen/Drehen der Schlüssel.

### 1.6 Sonderschnittstelle (SIO/CTC) [SRC AUP S. 8]

- SIO A13 „im Interruptbetrieb“ mit der ZRE; Takt von **CTC A12 Kanal 0**.
- Nur **SIO-Kanal B** geht nach außen, **TTL-Pegel**, kein Pegelwandler: X2 A16 /CTSB, A17 RXDB, A18 /DTRB, A19 /RTSB, A20 TXDB
  (drei Ausgänge, zwei Eingänge über Gatter A24/A15).
- Interruptkette: X1 A10 /IEO, C10 /IEI (AUP hängt in der IEI/IEO-Kette); /INT X1 C23, /RDY C25, /IODI A22.  Der Vektor kommt vom
  SIO (A22-08 → DIEN) [SRC S. 3 §1.2].  CTC-Interrupts: nicht erwähnt [?].
- Weitere X2-Leitungen: DIO2 (KT) A9, DIO1 (TT) A10 — die Tastaturleitungen; die B/C-Reihe von X2 trägt PIO-/Koppelbussignale
  (PA*, PD*, /PIORQ …), deren Rolle hier nicht beschrieben ist [?].
- Registerreihenfolge innerhalb B4–B7 (Daten/Steuer A/B) und Lage in der Kette relativ zu K8025/K5122 [?] — üblich wäre die
  K1520-Ordnung (A0 = Kanal, A1 = Daten/Steuer bzw. umgekehrt); am Schaltplan zu prüfen (fehlt im Scan).

**Was am anderen Ende hängt (SIMO), ist völlig unbelegt** — Protokoll, Baudrate, Befehle.  Nachbildbar ist nur die Schnittstelle selbst.

## 2. K3521.30 — akkugepuffertes statisches RAM [SRC OSS-Heft §14]

- 4 KByte CMOS (4 × 8 × KM537RU1 1 K × 1), STE 045-8120; Varianten 012-7012 (K3521.10) und 045-8063 (K3521.20).
- Adressbereich **4-K-Raster** über S1:01 (07/08, 05/06, 03/04, 01/02 binär, 0000H…F000H).  **/MEMDI-Wahl** S1:02 (MEMDI X1:B09,
  MEMDI1 X2:A21, MEMDI2 X2:B21).
- **2 WAIT-Zyklen je Zugriff** (M1-synchronisiert).
- Datenerhalt ≥ 200 h über 3 NK-Knopfzellen; Unterspannung (≤ 2,65 V) setzt ein Flipflop → Signal **/SUE**, „das über die zentrale
  Baugruppensteuerung der ZRE ausgewertet wird (BS PIO – ZRE K 2526/K 2527)“ [SRC S. 7] — **welches PIO-Bit** [?] (ZRE-Schaltplan).
  Rücksetzen nur durch RESET oder S1:03-03/04.
- CE gesperrt während RESET (optional X3:04-05).

Für den Emulator [Schluss]: ein RAM-Block 4 K an wählbarer 4-K-Adresse, Inhalt in einer Datei (wie die RAF-Stand-by-Ablage
`raf_<programm>.bin`), RESET erhält den Inhalt, optional /SUE als Zustand „Akku leer“.  Die 2 WAIT-Takte sind im Kern zählbar, wenn
die Bus-Taktbilanz das je Gerät kann [? prüfen]; sonst weglassen und vermerken.  Überlappung mit K3525/K3526 regelt /MEMDI bzw. die
Adresswahl — die K8924-Sparkassenbestückung (K3521 + K3525 + OSS + K3526) ist ohne SIOS-Speicherplan nicht festzulegen [?].

## 3. OSS 062-8471 — viel mehr als „48 KB zweite Bank“ [SRC OSS-Heft §15]

Entwurf 33 §5 schätzte „klein, wenn wie K3526“.  **Das trifft nicht zu.**  Die OSS ist ein **Operativspeicher mit Seitensteuerung und
Schreibschutz** — ein vollständiger Speicher-/E/A-Schutz für den Betrieb Betriebssystem ↔ Anwenderprogramm:

- 48 KByte dyn. RAM als **Betriebssystemseite (BS)**, dazu 48 KBit **Seitensteuerspeicher** (1 Bit je BS-Adresse: nächster
  Datenzugriff auf BS = 0 oder AP = 1) und 16 KBit **Speicherschutz-RAM** (1 Bit je Adresse 0000–3FFFH der Anwenderseite).
  Gesamt bis 112 KB.  „Nur mit SIOS funktionstüchtig“, fester **Steckplatz 7**, Wickelbrücke ZRE X1:A27 → OSS X2:A15 (BAOZE [Schluss]),
  /MEMDI-Bildung „entspricht nicht der Busrichtlinie“.  Schutz der ZRE (Q240) ist bei gesteckter OSS „unwirksam“.
- **Zentrale Steuerung auf E/A EEH/EFH** (OUT): Steuerwörter **00H** RESET (OSS inaktiv), **3CH** BS-Speicher laden, **3AH**
  Seitensteuerspeicher + Speicherschutz-RAM laden, **78H** NMI zurücksetzen (/BSS = 0).  Register A23 (U212) mit Ausgängen WRSS,
  WRBS, /BSA, /DBS, /BSS, BSE, /RCPU; Bitzuordnung DB→DI aus Abb. 5 nur teilweise sicher lesbar [?].
- **Seitensteuerung**: mit jedem M1 wird die BS-Seite zugeschaltet; der Seitensteuerbit-Wert der Befehlsadresse entscheidet, ob die
  *folgenden* Datenzyklen des Befehls auf BS- oder AP-Seite gehen (umgeschaltet mit /MEMDI).  **Getrennt für 1. und 2. CPU** (ZVE1/ZVE2,
  unterschieden über /BAI, /BAO, /BAOZE); für die 2. CPU Rückschaltung nach jedem Zyklus (kein M1 auf dem Bus), /RCPU legt sie fest
  auf AP.
- **Interruptzähler** A52/A59: +1 je INT-Quittung (M1 ∧ IORQ), −1 je **RETI (ED 4D) und RETN (ED 45)**; Stand 0 = BS-Seite (in
  Interruptroutinen läuft das BS), FFH = AP-Seite/Steuerung inaktiv.
- **Speicherschutz**: Schreibzyklus aus dem AP-Bereich auf geschützte Adresse < 16 K → /MEMDI sperrt, **NMI**.  **E/A-Schutz**:
  E/A-Zyklus aus der Anwenderebene → NMI.  Unwirksam bei RFSH, DMA (/BAOZE = 0), M1-Zyklen.
- „Wird bei einer OSS-Anlage der Systemlader V.0.X (X = 1…8) verwendet, bleibt die OSS inaktiv“ [SRC S. 11 Punkt 6].

Beurteilung [Schluss]: gut genug beschrieben, um das Verhalten nachzubilden, aber **teuer und riskant**: der Kern müsste dem
/MEMDI-Treiber je Zugriff **Zyklusart (M1/Daten/RFSH), aktive CPU und RETI/RETN** mitgeben — `MemdiDriver::drivesMemdi(addr)` kennt
heute nur die Adresse; `signalRETI()` gibt es, RETN nicht; NMI aus einer Karte gibt es.  Das berührt den heißen Speicherpfad und die
ZVE1/ZVE2-Arbitrierung (Boot-Invarianten).  **Und ohne SIOS lädt niemand die OSS** — ein selbst geschriebenes Prüfprogramm könnte
sie nur gegen unser eigenes Verständnis prüfen.

## 4. Was ohne SIOS prüfbar wäre

| Teil | Prüfweg ohne Originalsoftware | Aussagekraft |
|---|---|---|
| AUP-Tastaturregister 41/42/43 | GoogleTest am Bus; eigenes CP/A-Programm `AUPTEST.COM` (Muster SERTEST/ROMREAD, `tools/cpm_bau.py`) zeigt Codes und schaltet Lampen | prüft Modell gegen Beleg, nicht gegen Gerät |
| Codetabellen K7633/KuT | Tabelle aus §1.5 als Kerndaten, Wächter: Pos → Code beidseitig | hoch (Beleg wörtlich) |
| SIO/CTC der Sonderschnittstelle | vorhandene `Z80SIO`/`Z80CTC`; Kanal B über `SerialHub` (Telnet/Datei); Prüfprogramm sendet/empfängt (SERTEST-Muster), Loopback | hoch für die Bausteine, null für SIMO |
| K3521 | GoogleTest + CP/A: schreiben, Kaltstart/Neustart des Emulators, lesen | hoch |
| OSS | nur synthetisch (GTest mit handgebauten Befehlsfolgen) | gering — prüft nur das eigene Modell |
| SIMO, AHL/Kartenleser | — | nicht möglich |

## 5. Vorschlag Teil-APs

Reihenfolge nach Nutzen/Risiko; alle unter den Regeln von Entwurf 33 §8.1 (Bitgleich-Gebot: ohne gesteckte Karte keine Änderung).

| AP | Inhalt | Aufwand | Modell | hängt ab |
|---|---|---|---|---|
| **N10b** | K3521 als Option | klein (½–1 Tag) | Sonnet | — |
| **N10c** | AUP-Tastaturteil im Kern + C-ABI | mittel (1–2 Tage) | Sonnet/Opus | — |
| **N10d** | AUP-Sonderschnittstelle (CTC B0–B3, SIO B4–B7, Kanal B nach außen) | klein–mittel (1 Tag) | Sonnet | N10c (gleiche Karte) |
| **N10e** | Oberfläche: K7633 + Kundentastatur, Schlüsselschalter, Lampen, Option „AUP“ | mittel (1–2 Tage) | Sonnet | N10c |
| **N10f** | OSS 062-8471 | groß (≥ 4 Tage), Kernrisiko | Opus | **zurückstellen bis SIOS vorliegt** |

**N10b — K3521.**  Karte `core/cards/k3521/` (oder Variante von `k3525`): 4 K an Adresse `n·1000H`, /MEMDI-Quelle wählbar,
Inhalt aus/in Datei (Ablageort und Schreibzeitpunkt wie RAF: Schreibpause/Beenden), RESET erhält.  /SUE nur als Zustand, solange das
BS-PIO-Bit unbelegt ist.  Konfig/C-ABI additiv (`k1520_create_a5120_konfig`-Text um `k3521=<adr>`), Python, Optionsfeld.
*Abnahme:* GTest (Lesen/Schreiben, Adresswahl, Persistenz über zwei Maschineninstanzen, RESET erhält); CP/A-Integrationstest:
Bytes schreiben, Maschine neu anlegen, Bytes wieder da; alle bisherigen Tests unverändert grün.

**N10c — AUP Tastaturteil.**  Karte `core/cards/aup/` auf 40–43H (Basis konfigurierbar), Hochebenenmodell: Tastendruck (Matrixposition
00–47H, getrennt K7633/KuT) → Entprellzeit [? festlegen, z. B. 2 Abtastdurchläufe] → Code nach Tabelle §1.5 in A6, Gültigkeitsbit in
A5; `IN 42` liefert und löscht; `OUT 43` setzt Lampen-/Hupenzustand, FFH = Reset; Dauerfunktionstasten wiederholen [? Rate].
Die offenen Bitlagen von A5 und die Bedeutung von `IN 40` werden als **Annahme** gesetzt, im Code markiert und im Merkposten
geführt.  C-ABI `k1520_aup_*` (Taste drücken/lösen je Tastatur, Lampenzustand lesen), Python.
*Abnahme:* GTest je Register und je Tabelleneintrag (Wächter: alle 72 + 16 Positionen), `AUPTEST.COM` auf einer CP/A-Diskette
(Beigaben) zeigt Codes am Bildschirm; Option aus ⇒ Ports 40–43H unbelegt wie bisher.

**N10d — Sonderschnittstelle.**  `Z80CTC` auf B0–B3, `Z80SIO` auf B4–B7 in der AUP, CTC K0 → SIO-Takt Kanal B, Kanal B als
`SerialAnschluss` „Sonderschnittstelle (SIMO)“ am `SerialHub`; Platz in der IEI/IEO-Kette [? — Vorschlag: hinter K8025].
*Abnahme:* GTest Interruptvektor/Kette; Prüfprogramm (SERTEST-Muster) sendet über Datei/Telnet und empfängt im Loopback.

**N10e — Oberfläche.**  Bildschirmtastatur K7633 nach Positionsraster A01…F13 (Zeile A–F, Spalte 01–13 aus §1.5) + Foto,
Kundentastatur 0–9 + zwei Funktionstasten, Schlüsselschalter SS1/SS2 als Schalter, Lampen SEL/ALPHA/Fehler/GET + Hupe
(Statuszeile/Kasten).  Nur bei Modell „K8924 Sparkasse“ bzw. Option „AUP“; kein Tastenkürzel.  Beschriftung der Tasten [?] — Foto
`Tastatur_K7633_3_k.jpg` auswerten, Rest als Platzhalter mit Code.
*Abnahme:* GUI-Wächter (Taste → `k1520_aup_*`), Handbuch-Abschnitt, Kürzelvertrag unverändert.

**N10f — OSS (zurückgestellt).**  Erst Serviceschaltpläne (13 S.) lesen; Busschnittstelle erweitern (Zyklusart, aktive CPU, RETN an
`MemdiDriver`), Seitensteuerung je CPU, Interruptzähler, Speicher-/E/A-Schutz mit NMI, Steuerwörter EEH/EFH, ZRE-Schutz aus.
Ohne SIOS bleibt jede Abnahme synthetisch; deshalb **nicht vor N11 (SIOS)** beginnen.

### Machbarkeit in einem Satz

Für **K3521** und den **AUP-Tastaturteil samt Sonderschnittstelle** reichen die Unterlagen für ein Registermodell (mit drei
markierten Annahmen: A5-Bitlage, `IN 40`, Alpha-Umschaltung); die **AUP-Firmware, SIMO und der Kartenleser** sind nicht nachbildbar,
und die **OSS** ist zwar beschrieben, aber ohne SIOS weder sinnvoll prüfbar noch nötig.

### Was fehlt (für Anwender/Forum)

1. AUP-Serviceschaltpläne (laut Inhaltsverzeichnis Teil des Hefts, im Scan fehlend) — klären A5-Bits, `IN 40`, SIO-Registerordnung,
   Interruptlage.
2. Technische Beschreibung K7633/Kundentastatur (im Inhaltsverzeichnis, fehlt) — Tastenbeschriftung, Alpha-Taste, Schlüsselschalter.
3. AUP-EPROM-Abzug (U555, 1 K) — machte die Tabellen und das Firmwareverhalten (Entprellung, Dauerfunktion) belegbar.
4. ZRE-Schaltplan: BS-PIO-Bit für /SUE (K3521).
5. SIOS (Disketten) — Voraussetzung für OSS, SIMO-Protokoll und jede Gesamtprüfung.

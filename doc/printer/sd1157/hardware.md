# SD 1157 — Hardware, rekonstruiert aus der Firmware (WP0b, Entwurf 49)

Stand 2026-11-05.  Kein Schaltplan vorhanden; alles unten ist das, was die Firmware tut.  Herkunft je Angabe:

* `[Q:n]` Zeile n von `doc/printer/sd1157/1157.mac` (Quelle, M80)
* `[F:aaaa]` aus dem ROM `doc/EPROMS/SD1157/1157.rom` erschlossen (ROM-Adresse; ab Quelladresse 0D29H gilt ROM = Quelle + 38H, s. §1)
* `[Z]` aus `1157zg.rom`, `[T]` aus `1157.txt` (Neuerervorschlag), `[?]` Annahme, nicht belegt

Quellenzeilen gelten auch fürs ROM (der Code ist bis auf §1 identisch).  Namen wie `m1a0d` sind Marken der Quelle, ihre Zahl
ist **nicht** die ROM-Adresse; die ROM-Adressen stehen in §8.

## 1  ROM gegen Quelle

`1157.mac` mit M80 3.50 (`cparun m80`, Werkzeugkette `~/projects/CPA_Workbench/tools`) übersetzt, Listing gegen `1157.rom`
verglichen.  LINKMT lehnt das `aseg`/`.phase`-Modul ab ("Incompatible relocatable file"); darum wurden die Bytes aus dem
Listing gelesen (Skripte `.../scratchpad/wp0b/cmp2.py`, `rebuild.py`).

**Ergebnis: nicht identisch.**  Das ROM ist die Quelle plus drei Abweichungen; mit ihnen lässt sich das ROM byteweise
rekonstruieren (0 Restabweichungen).  Es gilt das ROM.

| # | Ort im ROM | Quelle | ROM | Bedeutung |
|---|---|---|---|---|
| 1 | 0D20H–0D60H (65 B) | 0D20H–0D28H (9 B): `LD HL,(ipzl)` / `LD A,(HL)` / `POP HL` / `EI` / `JP if03` [Q:1641-1645] | `DI`, `PUSH BC/DE`, `LD A,(3800H)`, **`LDIR` 3801H→3800H, 3FFH Byte**, `ipzl`-2 (**Abzug-Befund, s. unten**), `ipzf`-1, ist `ipzf`<3B00H: CTC0 mit TC aus `(IX-20)` neu starten, **SIO A WR5=0AAH, DC1 (11H) an V.24-Kanal A (7CH)**; dann `POP`…, `EI`, `JP 1023H` [F:0D1F] | Der Eingabepuffer 3800H–3BFFH ist im ROM ein **Schiebepuffer**: jedes geholte Zeichen schiebt den Rest um 1 Byte (LDIR 1023 Byte ≈ 21 000 T ≈ 8,5 ms bei 2,4576 MHz, mit DI) und sendet dabei DC1 (zeichenweise Freigabe, nur auf V.24-Kanal A; am IFSS wirkungslos) |
| 2 | ab 0D29H | Adressen n | n+38H | Folge von 1 (56 Byte mehr): alle Sprungziele, Vektoreinträge und die zwei `JR` bei 0D0BH/0D11H verschoben |
| 3 | 2907H–297FH (121 B) | endet bei 28CEH | Kopie von 2887H–28FFH | Auffüllung hinter dem letzten `RET` (2906H), nie ausgeführt [F:2906] |

Gleich geblieben: `ds 3,0FFH` (0008H, 0010H, 0020H → FFH) und `ds 30,0` (ROM 0DAEH–0DCBH → 00H).  Das ROM ist 0000H–297FH; der
Raum 2980H–2BFFH ist im Abzug nicht belegt (FFH angenommen [?]).

**Befund 0D34H (2026-11-05, Hypothese [?]):** `ipzl` wird vor dem Holen um 1 erhöht (Quelle m0d06) und im Patch um **2** (`2B 2B` bei 0D33H/0D34H), `ipzf` um 1
verringert; der Abstand `ipzf - ipzl` bleibt dadurch 1, der Puffer wird nie leer, die Firmware holt endlos NUL.  Mit **einem** `DEC HL` ist die Logik schlüssig (beide Zeiger
nach dem letzten Zeichen = 37FFH = leer).  Wahrscheinlichste Erklärung: **Bitkipp 0 → 1** (Ladungsverlust einer 2708-Zelle, Bit 5): ursprünglich `0BH` = `DEC BC` (wirkungslos, BC wird bei 0D5AH
zurückgeholt), gelesen `2BH`.  Von den Ein-Bit-Nachbarn von `2BH` ist nur `0BH` sinnvoll (`2AH` = `LD HL,(nn)`, `29H` = `ADD HL,HL`, `23H` = `INC HL` zerstören den Zeiger).  **Kein Beleg durch
Prüfsumme:** die Firmware hat keine ROM-Prüfsumme (Quelle durchsucht), die Quelle enthält den Patch nicht.  Reparierte Fassung als eigene Datei `doc/EPROMS/SD1157/1157_repariert.rom` (nur 0D34H = 0BH;
`0D33H` statt `0D34H` wäre gleichwertig), Vorgabe der Karte (`Config::rom_repariert`, Konfigurationstext `rom=original|repariert`).  Wirkung am Lauf: Zeichen werden gedruckt, der Puffer leert sich.

Folge fürs Verhalten: Lesen aus dem Eingabepuffer ist im ROM sehr langsam und läuft mit gesperrten Interrupts.  Es geschieht nur,
wenn der Wagen steht (§10.1); der Emulator darf Takt-Impulse während eines solchen Fensters nicht „nachliefern“.

## 2  Speicherkarte

| Bereich | Inhalt | Beleg |
|---|---|---|
| 0000H–297FH | ROM (laut [T] 12 EPROMs 2708, je 1 KiB, auf der Speicherleiterkarte 56-264-6702-7; 0000H–2BFFH = 11 KiB Programm, 12. = ZG1) | [T], [F] |
| 2C00H–2FFFH | RAM 1 KiB (`ram1`): Stapel (SP=2C28H, nur ≈ 40 B), Vektortabelle 2CE0H–2CFFH, Arbeitszellen 2C3CH–2CDFH (beim Start gelöscht), `mbytes`=2C50H (IX), Zeilenspeicher 2D09H–2E7FH (Bahn 1) und 2E7FH–2FFFH (Bahn 2) | [Q:61-102,339-346] |
| 3000H–33FFH | Zeichengenerator 1 = **`1157zg.rom`** (§3), ab 3300H Testtext/-steuerbytes | [Q:104-111] |
| 3400H–37FFH | Zeichengenerator 2, **optional und nicht vorhanden** (ZG2 ist bestückt, wenn Byte 37FFH = 00H) | [Q:2053-2057,1960-1966] |
| 3800H–3BFFH | SIO-Eingabepuffer 1 KiB RAM (`siobuf`); `ipzf`/`ipzl` starten bei 37FFH; XOFF ab `ipzf` ≥ 3BF5H, Überlauf bei 3BFFH | [Q:3-5,1726-1733,1756-1762] |

Rest nicht belegt.  Der Emulator muss für 3400H–37FFH **FFH** liefern (nicht 00H), sonst hält die Firmware ZG2 für bestückt
(`SO` würde angenommen, Glyphen = Leerdaten) [F:10C6].  `IM 2`, I=2CH [Q:123-125]; NMI-Vektor 0066H `JP 0433H` [Q:179, F:0066].

## 3  Zeichengenerator

**`1157zg.rom` ist ZG1 (`azg1` = 3000H).**  Beleg: `konst1` = azg1+300H ist die „Ausschrift im Testmodus“ [Q:107]; im Abzug steht
ab Offset 0300H `0D 0A "SERIENDRUCKER" 1B 5B 31 6D " robotron 1157 " 1B 5B 33 6D "Autotestprogramm 1 :"` und ab 035BH die
Teststeuerbytes [Z].  Offset 03FBH–03FFH = `"1100" 00` (Kennung [?]).

Aufbau [Q:3379-3409 (`m1bb2`), 3612-3635 (`m1d5b`)]:

* Zeiger = 3000H + (Code − 20H)·8, Codes 20H–7FH; 96 Zeichen × 8 Byte = 0300H Byte.  `m1bb2` rechnet `RLD`-Trick 3000H+Code·8 und
  zieht danach per `DEC B` 0100H ab.  Bei aktivem `SO` (Bit 0 von D' [Q:3474-3480]) kommt `+0400H` dazu: ZG2 = 3400H + (Code−20H)·8.
* Byte 0–6 = **Spalte 0–6** (links→rechts); Bit n von Spalte = Zeile n (Bit 0 = obere Zeile = Nadel 1 [?]); **0 = Nadel feuert**
  (FFH = leer), die Byte gehen unverändert auf Port BEH [Q:3628-3629].
* Byte 7 = Nadel 9 für alle 7 Spalten: **Spalte c = Bit (6−c)**, Bit 7 immer 1.  Beleg: die Firmware schiebt das Byte vor jeder Ausgabe
  `SLA E` und gibt `E` an BFH aus; Nadel 9 steht in D7 [Q:3630-3632]; mit dieser Zuordnung haben `y` und `,` den Schweif links (geprüft am
  Abzug: 9-Zeilen-Raster, Großbuchstaben Zeilen 0–6, Unterlängen bis Zeile 8).
* Rückwärtsdruck liest die Spalten 6→0 und `RRC E` [Q:3942-3967].
* Auch dort ist ein Zeichenvorrat Latin-nah (`A`…`z` geprüft); Steuerzeichen 00H–1FH haben keine Glyphe.

ZG2 fehlt, d. h. es gibt keinen zweiten Satz: `SO` (0EH) ergibt dann `zmb ub` (Fehlerbit, wird ignoriert) [Q:2052-2058].

## 4  E/A-Adressen

Alle Adressen sind in der Quelle als `equ` benannt, bis auf `IN 6FH` und `IN BCH` [Q:246, 368].  Dekodierung: SIO/CTC/PIO auf den
üblichen Z80-Adresslagen (A0 = B/A, A1 = Steuer/Daten bei PIO/SIO).

| E/A | Dir. | Baustein / Funktion | Beleg |
|---|---|---|---|
| 6FH | IN | **Schalterblock 3** (V.24-Betrieb): D1 OPR, D3 HDUPL, D4 EXT/ETX, D5 8-Bit; D0/D2/D6/D7 nie gelesen (§7) | [Q:246,1531,1540,1600,1617,1689,1804,1819,2790] |
| 7CH / 7DH | R/W | SIO Daten Kanal A (V.24) / Kanal B (IFSS) | [Q:33-36] |
| 7EH / 7FH | R/W | SIO Steuer A / B | [Q:33-36] |
| BCH | IN | **8212-System-Eingang** (Papierende, 6/8 lpi, Bahnzahl, Breite, Formularlängen) | [Q:368,512,527,546] |
| BEH | OUT | Nadelimpulse Nadel 1–8, **low = feuern**, FFH = aus | [Q:77,3629] |
| BFH | OUT | Nadel 9 in **D7** (low = feuern); D0–D6 ohne Wirkung (die Firmware gibt dort beliebige Glyphbits mit aus) | [Q:78,3632,3682] |
| DCH–DFH | R/W | CTC Kanal 0–3 | [Q:38-41] |
| ECH / EDH / EEH / EFH | R/W | PIO 3: Daten A, Daten B, Steuer A, Steuer B | [Q:55-58] |
| F4H / F5H / F6H / F7H | R/W | PIO 1: Daten A, Daten B, Steuer A, Steuer B | [Q:43-46] |
| F8H / F9H / FAH / FBH | R/W | PIO 2: Daten A, Daten B, Steuer A, Steuer B | [Q:49-52] |

Sonst keine Adresse (kein zweiter 8212-Port, keine Ausgabe an BCH).

### 4.1  BCH (8212-System), nur gelesen

| Bit | Name | Wert | Beleg |
|---|---|---|---|
| 0 | /PE | 0 = Papierende, 1 = Papier da | [Q:528,822-824] |
| 1 | Taste 6/8 lpi | 1 = 6 lpi (8 PV-Schritte/Zeile), 0 = 8 lpi (6 Schritte) | [Q:512-514,2219-2221] |
| 2 | EBD | 1 = Einbahndrucker, 0 = **2 Bahnen** (Befehle LF2/VT2/FF2 erlaubt) | [Q:1919-1921,2036-2038] |
| 3 | BDA | 1 = breite Ausführung (Rand 220, max. 210 Zeichen), 0 = schmal (Rand 142, max. 132) | [Q:1268-1270,2403-2440] |
| 4,5 | Formularlänge Bahn 1 | 00 = 4″ (192 Schritte), 01 = 6″ (288), 10 = 8″ (384), 11 = 12″ (576); 1 Schritt = 1/48″ | [Q:366-378] |
| 6,7 | Formularlänge Bahn 2 | wie 4,5 | [Q:379-389] |

Schritt = 1/48″ folgt aus 6 lpi = 8 und 8 lpi = 6 Schritten [Q:4217-4221, 2219]; Zoll als Einheit [?] (6/8 lpi sind Zeilen je Zoll).

## 5  PIO

Alle drei im Modus 3 außer PIO 3 A beim Parallelinterface.  **Eingangsbits lesen den Pin, Ausgangsbits die Ausgangszwischenspeicher
(Lese-ändern-schreiben im Code)** — ein Ausgang liefert beim Lesen immer sein eigenes Latch, nie einen Pin.  Interruptwort: 97H =
frei, ODER, aktiv **low**, Maske folgt; B7H = frei, ODER, aktiv **high**, Maske folgt.  Maskenbit 0 = überwacht.

### 5.1  Initialisierung [Q:145-203]

| Port | Steuerbytes | Bedeutung |
|---|---|---|
| PIO 2 A (FAH) | CF, **29H**, B7, FE, E8 | Mode 3, Eingänge D0 D3 D5; Int aktiv high, nur D0 überwacht; Vektor **E8H** |
| PIO 2 B (FBH) | CF, **01H**, B7, FE, EA | Eingang nur D0; Int aktiv high, nur D0; Vektor **EAH** |
| PIO 1 A (F6H) | CF, **30H**, 97, CF, EC | Eingänge D4 D5; Int aktiv low, D4 und D5 überwacht; Vektor **ECH** |
| PIO 1 B (F7H) | CF, **31H**, 97, CF, EE | Eingänge D0 D4 D5; Int aktiv low, D4 D5; Vektor **EEH** |
| PIO 3 A (EEH) | 8F, 87, FC | Mode 2 (bidirektional), Int frei, Vektor **FCH** — nur wirksam beim Parallelinterface |
| PIO 3 B (EFH) | CF, FF, 87, FE | alles Eingänge, Vektor **FEH** (Mask-Register nach Reset: nichts überwacht, kein Int) |

Danach liest die Firmware `IN P3BD` D0 (**ser/par**) [Q:204-206]: 1 = Parallelinterface (PIO 3 A bleibt Mode 2, SIO wird nicht
initialisiert, Vektoren FCH/FEH = `iparc`/`ipard`); 0 = serielles Interface: PIO 3 A wird auf Mode 3, alles Eingänge, Int aus
umprogrammiert (`07H`, CF, FF) und liefert dann die Interface-Schalter (§5.4) [Q:212-221].

Startwerte der Ausgänge [Q:353-362]: P2AD=10H (vorwärts), P2BD=18H (Farbbandmotor an, /TF=1), P1AD=P1BD=08H (xpwv=1).

### 5.2  PIO 2 (Wagen, Druck, Lampen)

| Port.Bit | Rolle | Name | Pol. / Wirkung | Beleg |
|---|---|---|---|---|
| 2A.0 | **E** | Takt vorwärts (Int Vektor E8H) | aktiv high; Impuls je Takt (Taktscheibe) bei Vorwärtslauf: `h'++`, D'.5=1 | [Q:3163-3188] |
| 2A.1 | A | a0-i = **Online-Lampe** (und Interface-Signal) | 1 = an | [Q:532,690,760,795] |
| 2A.2 | A | VTAB | 1 = Tabulatorgeschwindigkeit (schnell), 0 = Druckgeschwindigkeit | [Q:461,743,1074,1363] |
| 2A.3 | **E** | /VK Vorwarnkontakt | 0 = geschlossen | [Q:433,456] |
| 2A.4 | A | VW Richtung | 1 = vorwärts (Druckrichtung, nach rechts), 0 = rückwärts | [Q:436,457,909,915] |
| 2A.5 | **E** | LAUF | 1 = Wagen läuft | [Q:474,579,1242] |
| 2A.6 | A | **LFG** Lauffreigabe | 1 = Wagen fahren | [Q:442,467,577] |
| 2A.7 | A | a3-i (Interface-Signal, „Zeilenspeicher belegt“ [?]) | 1 = aktiv; nur Latch, nie Eingang | [Q:679,857,936,2168] |
| 2B.0 | **E** | Takt rückwärts (Int Vektor EAH) | aktiv high; `h'--`, D'.5=0 | [Q:3190-3208] |
| 2B.1 | A | **DTFG** Drucktaktfreigabe | 1 solange ein Zeichen gedruckt wird | [Q:3266,3648] |
| 2B.2 | A | **KAS** Kopfanschwenken | 1 = Kopf an den Walze | [Q:1049,1440,4652] |
| 2B.3 | A | **/TF** technischer Fehler = Fehlerlampe | 0 = Lampe an (reines Firmware-Latch, kein Eingangssignal) | [Q:428,4870,5016] |
| 2B.4 | A | **FMA** Farbbandmotor | 1 = an | [Q:1441,2171] |
| 2B.5 | A | **FGDI** Freigabe-/Auslöseimpuls Druck | kurzer 1→0-Impuls je Takt (§10.5) | [Q:3169-3171] |
| 2B.6 | A | IFG (Interface-Freigabe) | Latch; 1 = aktiv | [Q:538,767,778] |
| 2B.7 | A | ac-i (Interface-Quittung) | 1 = aktiv; Latch | [Q:535,768,789] |

`[Q:428]` Anmerkung: Die Bit-3-Abfrage „/tf“ liest das **Latch**, nicht einen Pin — PIO 2 B ist bis auf D0 Ausgang (Maske 01H).  Das Bit
wird beim Einschalten 1 und nur von der Firmware auf 0 gesetzt (Havarie, Zeitüberschreitung) [Q:4870,5016,645].

### 5.3  PIO 1 (Papiervorschub, Ränder, optionaler Karteneinzug 1161)

Bahn 1 = Port A, Bahn 2 = Port B (nur bei 2 Bahnen).

| Port.Bit | Rolle | Name | Pol. / Wirkung | Beleg |
|---|---|---|---|---|
| 1A.0 | A | ungenutzt | – | – |
| 1A.1 | A | **FRH1** Frequenzhochlauf PV 1 | 1 = Schnellfrequenz, 0 = Anlauf/letzter Schritt | [Q:4800,4812] |
| 1A.2 | A | **XPFG1** PV-Freigabe 1 | 1 = Papier fahren | [Q:1231,4673,4763] |
| 1A.3 | A | **XPWV1** PV-Richtung 1 | 1 = vorwärts (Papier vor), 0 = Rückzug | [Q:4659,4859] |
| 1A.4 | **E** | PV-Schritt 1 (Int ECH, nur während `XPFG1`) | aktiv low, 1 Impuls = 1 Schritt | [Q:1229-1232,4668-4673] |
| 1A.5 | **E** | **/LR** linker Rand | 0 = Wagen am/links vom linken Randsensor (Int ECH) | [Q:487,1229-1249,3276] |
| 1A.6 | A | ZB1 (Karteneinzug) | Latch; ohne Karteneinzug nie 1 | [Q:5130,4986] |
| 1A.7 | A | FTB/KAB (Karteneinzug) | wie 1A.6 | [Q:4970] |
| 1B.0 | **E** | INA (Karteneinzug vorhanden) | muss **0** lesen (sonst läuft `zf12`-Zweig) | [Q:976-977,4991] |
| 1B.1 | A | FRH2 (Quelle nennt es irrtümlich „ina“) | wie 1A.1 für Bahn 2 | [Q:4825,4836] |
| 1B.2 | A | XPFG2 | wie 1A.2 | [Q:1264,4708] |
| 1B.3 | A | XPWV2 | wie 1A.3 | [Q:4699,4887] |
| 1B.4 | **E** | PV-Schritt 2 (Int EEH) | wie 1A.4 | [Q:1262-1265,4703-4708] |
| 1B.5 | **E** | **/RR** rechter Rand | 0 = Wagen am/rechts vom rechten Randsensor (Int EEH) | [Q:912,3273,1262] |
| 1B.6 | A | ZB2 | Karteneinzug | [Q:5135] |
| 1B.7 | A | FSB | Karteneinzug | [Q:4988] |

Während eines PV wird der Interruptfilter von PIO 1 A (B) auf **nur D4** umprogrammiert (`97H, EFH`) und danach auf D4+D5
zurückgesetzt (`97H, CFH`) [Q:4668-4671,4898-4901].  Karteneinzug-Bits sind optional, §11.

### 5.4  PIO 3 (Bedienfeld und Interface-Schalter)

Serielles Interface (PIO 3 A Mode 3, alles Eingänge):

| 3A.Bit | Bedeutung | Beleg |
|---|---|---|
| 0 | **0 = IFSS (SIO B), 1 = V.24 (SIO A)** | [Q:219-221,1524-1526] |
| 1,2 | Stoppbits: Bit 2/Bit 1 = 01 → 1, 10 → 1½, 11 → 2, 00 → WR4 „sync“ (nicht verwenden) (nur V.24) | [Q:233-238] |
| 3 | Parität gerade (1) / ungerade (0) | [Q:230-232] |
| 4 | Parität ein | [Q:227-229] |
| 5,6,7 | Baudrate (V.24), s. §7 | [Q:396-416] |

PIO 3 B, alles Eingänge (Tasten, 0 = gedrückt außer Bit 0/4):

| 3B.Bit | Name | Wert | Beleg |
|---|---|---|---|
| 0 | ser/par | 0 = serielles Interface, 1 = Parallel | [Q:204-206] |
| 1 | Taste **ONLINE** | **0 = online**, 1 = offline | [Q:524-526,448,836-838] |
| 2 | Taste **PV1** | 0 = gedrückt | [Q:805,4494] |
| 3 | Taste **PV2** | 0 = gedrückt | [Q:807,4523] |
| 4 | Taste Kopfanschwenken | 1 = Kopf dauernd angeschwenkt | [Q:4649-4653] |
| 5 | ZFV (Karteneinzug) | 1 = nicht aktiv; **1 lassen**, sonst läuft der Einzug-Zweig | [Q:493-495] |
| 6 | Taste **TEST** | 0 = gedrückt/aktiv | [Q:809-811,1913-1915,903-905] |
| 7 | (Karteneinzug) | im normalen Ablauf nur im Zweig ZFV=0 gelesen | [Q:498-500] |

PV1/PV2/TEST werden **nur offline** (D1 = 1) ausgewertet [Q:793-815].  ONLINE ist ein Pegel, kein Taster-Flankensignal [?]
(die Firmware fragt den Pegel ab; ob die Taste rastet, ist unbekannt).

Parallelinterface (PIO 3 A Mode 2): Datenübernahme `ipard` (Vektor FEH) liest 3AD [Q:1973-1979]; Statusausgabe an 3AD nach jedem
Befehl [Q:2339-2347]; Vektor FCH = `iparc` [Q:1321].  Nicht weiter analysiert, nicht Ziel des Emulators.

## 6  CTC (Basis DCH, Vektor E0H [Q:135-137])

Steuerwerte: 17H = Zeitgeber, Vorteiler 16, steigende Flanke, Auto-Start, TC folgt, Reset, kein Int;
B7H = wie 17H aber **Vorteiler 256, Int frei**; 13H bzw. 33H = Reset ohne TC = Kanal stoppt; 15H = TC folgt ohne Reset.

| Kanal | Port | Verwendung | Zeitkonstanten | Beleg |
|---|---|---|---|---|
| 0 | DCH | **Baudratengenerator der SIO** (ZC/TO → SIO RxC/TxC, laut Kommentar [Q:38]); kein Int. Start `17H, TC` (TC = `(IX+0ECH)`, §7), Stopp `13H`, Neustart `15H, TC`. **Wird in jedem Takt-Int mit `17H,17H` (TC=23) neu gestartet** und am Zeilenende mit `13H` gestoppt — während des Drucks gilt die Baudrate nicht, Empfang ist dann gesperrt (DC3 vorher gesendet) | 1…128 (Baud), 23 (Druck) | [Q:145,391,729-731,3164-3167,3864,605-607] |
| 1 | DDH | Zeitgeber ms-Bereich, Int E2H (`ictc1`): `B7H, FFH` (26,6 ms) Kopf-Beruhigung (`kberz`, 12 → 13 Int ≈ 345 ms beim Start), Anschwenkzeit (1–2 Perioden), Leerlaufzeit, Zeichenschluss; `B7H, 19H` (2,60 ms, im Kommentar „2,56 ms“) Zeichen-/Pufferüberwachung | FFH, 19H | [Q:418-426,735-739,1052-1056,1649-1655] |
| 2 | DEH | **Wächter/Beruhigung PV Bahn 1**, Int E4H; `B7H` mit TC 44H (**7,08 ms**) nach jedem Schritt-Int, 64H (10,4 ms) Beruhigungszeit nach dem letzten Schritt, FFH Tastenhaltezeit PV1 (32 Int ≈ 0,85 s → Dauer-Formularvorschub); `B7H,C0H` (20,0 ms) Karteneinzug | 44H, 64H, FFH, C0H | [Q:4498-4500,4679-4681,4771-4774,4919-4924,5062-5065] |
| 3 | DFH | wie 2 für Bahn 2, Int E6H | 44H, 64H, FFH | [Q:4527-4529,4712-4714,4876-4935] |

Abgeleitet (f = CPU-Takt, Zeitgeber T = 256·TC/f, Baud = f/(256·TC)):

* **IFSS „9600 Bd“ mit TC = 1, Vorteiler 16, x16-SIO-Takt ⇒ f ≈ 9600·256 = 2,4576 MHz** [F]; die Baudtabelle (§7) stimmt damit.
* Kommentar „2,56 ms“ für 25·256 Takte [Q:1651] setzt f = 2,5 MHz voraus; B7H/C0H ergibt bei 2,4576 MHz genau 20,000 ms.
  Beides passt zu etwa 2,5 MHz.  Gegenprobe K6316 (`doc/printer/k6316/hardware.md` §4, φ = 2,4576 MHz aus Quarz 9830,4 kHz, `[S:03]`): dort CTC-Kanal `17H`, TC = 1 → 153,6 kHz = 16 × 9600 Bd — **dieselbe Programmierung wie hier** (Steuerwort 17H, TC = 1 = IFSS 9600 Bd).  Die Baudtabelle (§7) und alle CTC-Zeiten sind mit 2,4576 MHz verträglich; der Kommentar „2,56 ms“ ist Rundung des Autors.  Für den SD 1157 bleibt der Takt **abgeleitet, nicht aus einem Schaltplan** [F, ?]: zwei Belege (Baud exakt, 20,000 ms) stützen ihn, widerlegt ist er nicht.
* PV-Wächter 7,08 ms bei 2,4576 MHz (7,0 bei 2,5): PV-Schritte müssen **kürzer als 7 ms** auseinanderliegen.

## 7  SIO (Basis 7CH; Kanal A = V.24, Kanal B = IFSS)

Der Kanal B ist der IFSS, der Kanal A der V.24-Anschluss; **WR2 (Vektor) wird immer in Kanal B geschrieben** [Q:255-258,298-301].

| | IFSS (B) [Q:286-311] | V.24 (A) [Q:225-275] |
|---|---|---|
| WR4 | 44H = x16, 1 Stopp, ohne Parität (8N1) | Schalter: Bit0 Parität ein, Bit1 gerade, Bit3:2 Stopp, **40H** x16 |
| WR3 | C1H = Rx 8 Bit, Rx ein, keine Autoenables | 61H (7 Bit) oder E1H (8 Bit, Schalter 6FH.5=1), **Autoenables an** (CTS/DCD steuern Tx/Rx) |
| WR5 | 68H = Tx 8 Bit, Tx ein, DTR=RTS=0 | 28H (7 Bit) oder 68H; später 28H/2AH/A8H/AAH = DTR(108.2)/RTS(105) wechselnd, Tx 7 Bit |
| WR2 | F0H (Basisvektor, für beide Kanäle) | – |
| WR1 | 16H = Tx-Int, Status wirkt auf Vektor, Rx-Int bei jedem Zeichen, kein Ext-Int | 17H = wie links, **mit Ext-Int**; Kanal B dazu `WR1 = 04H` (nur Status wirkt auf Vektor) |

Daraus die SIO-Vektoren (Status wirkt auf Vektor, F0H + 2·Code):

| Vektor | Ereignis | Handler (ROM) | Beleg |
|---|---|---|---|
| F0H | B Tx leer | `isiote` 172AH | [Q:311] |
| F2H | B Ext/Status | ungenutzt | [Q:105] |
| F4H | B Rx-Zeichen | `serinp` 0E25H | [Q:307] |
| F6H | B Sonderfall | `errint` 0F10H | [Q:309] |
| F8H | A Tx leer | `isiote` 172AH | [Q:268] |
| FAH | A Ext/Status | `serest` 0F44H | [Q:270] |
| FCH | A Rx / **PIO 3 A** | `serinp` 0E25H bzw. `iparc` 0A4CH | [Q:272,332] |
| FEH | A Sonderfall / **PIO 3 B** | `errint` 0F10H bzw. `ipard` 1016H | [Q:274,334] |

Die Tabelle 2CE0H–2CFFH wird im RAM gefüllt, vor dem ersten `EI` bei 022AH [Q:312-335,424].

**Baudtabelle V.24** (`B` startet 1; D5: +1, D6: ×4, D7: ×16; Baud = 9600/B, TC = B; IFSS fest TC = 1) [Q:396-421]:

| D7 D6 D5 | 000 | 001 | 010 | 011 | 100 | 101 | 110 | 111 |
|---|---|---|---|---|---|---|---|---|
| TC (B) | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 |
| Baud (f = 2,4576 MHz) | 9600 | 4800 | 2400 | 1200 | 600 | 300 | 150 | 75 |

**Schalter 6FH (V.24)** [Q:1531-1624,1689-1722,1804-1833,2790-2796]:

| Bit | Name | Wirkung |
|---|---|---|
| 1 | OPR | 1 = nur V.24-Leitungen (DTR/RTS), keine Protokollzeichen |
| 3 | HDUPL | 1 = Halbduplex (RTS 105 wird je Senden gewechselt), 0 = Vollduplex |
| 4 | EXT/ETX | 1 = Protokoll mit ACK (06H) / NAK (15H) nach ETX, 0 = DC1/DC3 |
| 5 | 8 Bit | 1 = 8 Bit, 0 = 7 Bit |

Im V.24-Betrieb braucht die Gegenstelle (SerialAnschluss) CTS und DCD **aktiv** (Pin low, sonst bleibt Tx/Rx wegen WR3.5 gesperrt) und
SYNC/„107“ = 0 (aktiv): `serest` liest RR0 D4 und hält 107 bei D4 = 1 für inaktiv [Q:1865-1882].  Pegel der Gegenstelle [?].

Protokoll (IFSS): 9600 Bd 8N1; vom Host kommen `STX`/`ETX` (02H/03H, werden nicht gespeichert), Freigabe/Sperre mit DC1 (11H)/DC3 (13H) vom
Drucker; `dc4` = 11H (JDE: DC4→DC1) [Q:20,1572,1613].  Übertragungsfehler → `?` im Puffer.  DA (`ESC [ 0 c`) antwortet `ESC [ 4|5|6 c`,
DSR (`ESC [ 5 n`) `ESC [ 0..4 n` (0 Nullstatus, 1 Übertragungsfehler, 2 Papierende, 3 Havarie, 4 Operationsfehler) [Q:2571-2603,2803-2873].
Empfänger-Freigabe `WR3 = 41H` (ein)/`40H` (aus) in Kanal B [Q:726-727,1517-1518,1663].

## 8  Interruptvektoren (I = 2CH, IM 2)

| Vektor | Quelle | Handler (ROM) | Aufgabe |
|---|---|---|---|
| E0H | CTC 0 | – | kein Int |
| E2H | CTC 1 | `ictc1` 0A8FH | Beruhigungs-/Anschwenk-/Leerlaufzeit, Zeichenschluss |
| E4H | CTC 2 | `ictc2a` 269CH (alternativ `ictc2b` 27BEH, `ictc2c` 27FAH beim Karteneinzug) | PV-1-Wächter/Beruhigung, Tastenhaltezeit |
| E6H | CTC 3 | `ictc3` 26DCH | dasselbe für Bahn 2 |
| E8H | PIO 2 A.0 | 1A54H | **Takt vorwärts**: Fire-Impuls, `h'`++, Druckablauf |
| EAH | PIO 2 B.0 | 1A83H | **Takt rückwärts** |
| ECH | PIO 1 A | 0993H (alt. 283FH?) | **linker Rand** bzw. PV-1-Schritt (`XPFG1`=1 → `pt00` 2599H) |
| EEH | PIO 1 B | 09D3H (alt. `m2803` 281FH, Karteneinzug) | **rechter Rand** bzw. PV-2-Schritt (`pt50` 258AH) |
| F0H…FEH | SIO / PIO 3 | siehe §7 | |
| NMI (0066H) | extern | 0433H | **Farbbandhavarie + Spannungsausfall** [Q:179] |

(„alt. 283FH?“ ist nicht geprüft; der `ECH`-Eintrag wird nur durch `m098d` belegt [Q:320-321].)

Reihenfolge der Daisy-Chain: **nicht belegbar** [?].  Die Vektoradressen (CTC E0, PIO 2 E8/EA, PIO 1 EC/EE, SIO F0…, PIO 3 FC/FE)
legen CTC → PIO 2 → PIO 1 → SIO nahe; PIO 3 teilt sich die Vektoren FCH/FEH mit SIO A (alternative Bestückung).  Die NMI-Routine
[Q:625-638] stellt vier `RETI` hintereinander und räumt damit bis zu **vier** IUS-Zustände (es sind vier PIO-Kanäle) — das ist der einzige
Hinweis; die Firmware verlässt sich nirgends auf eine feste Rangfolge (Takt-Int laufen mit gesperrtem Int, `EI` erst am Ende).

## 9  Bedienfeld

| Element | Anschluss | Pol. | Verhalten der Firmware |
|---|---|---|---|
| Taste ONLINE | 3B.1 | 0 = online | Pegel; offline → `mb off`, Tasten PV/TEST werden gelesen, Lampe aus [Q:793-815] |
| Taste PV1 | 3B.2 | 0 = gedrückt | nur offline; Entprellung ≈ 10 ms (`zs00`, TC 6), kurz = LF1, ≥ 32·26,6 ms halten = FF1 [Q:4492-4511] |
| Taste PV2 | 3B.3 | 0 = gedrückt | wie PV1 für Bahn 2 (LF2/FF2) [Q:4521-4540] |
| Taste TEST | 3B.6 | 0 = aktiv | nur offline; startet den Selbsttest (§12) |
| Taste Kopfanschwenken | 3B.4 | 1 = dauernd angeschwenkt | 0 = automatisch (KAS nur beim Druck) |
| Taste 6/8 lpi | BCH.1 | 1 = 6 lpi | |
| Lampe ONLINE (a0-i) | 2A.1 | 1 = an | an bei online; aus bei Papierende/Havarie |
| Lampe FEHLER (/TF) | 2B.3 | 0 = an | Havarie, Zeitüberschreitung PV |
| Papierende | BCH.0 | 0 = Papierende | eine eigene Lampe gibt es im Code nicht (`hpv2` setzt Online-Lampe aus) [?] |
| Formularlängen, Bahnzahl, Breite | BCH.2–7 | s. §4.1 | Schalter/Brücken |
| V.24-Schalter | 6FH, 3A | s. §5.4, §7 | |

## 10  Mechanik, soweit die Firmware sie erwartet

### 10.1  Wagen (Gleichstromantrieb mit Geschwindigkeits- und Lagerückmeldung)

**Keine Phasenfolge, kein Schritttakt**: die Firmware gibt nur drei Pegel aus — LFG (fahren), VW (Richtung), VTAB (schnell) — und liest
**LAUF** (Wagen bewegt), /VK und die Taktscheibe zurück.  Ein-/Auslauframpen macht die Hardware.

* **Takt** (Taktscheibe, richtungsdekodiert): je Position ein Impuls auf **2A.0 (vorwärts)** oder **2B.0 (rückwärts)**, aktiv high.
  Pro Zeichenspalte (`kpz`, Zähler C') **10 Takte** (H' 0…9) [Q:3175-3181,3205-3207].  Taktdichte [?] (bei 10 Zeichen/Zoll 100/Zoll).
* **Lauf**: LFG 0→1 → **LAUF muss binnen weniger µs 1 werden** (`hp00` prüft nach `LFG`=1 sofort LAUF; 0 = Havarie, NMI-Pfad
  [Q:610-615,3317]).  Nach LFG→0 bleibt LAUF 1 bis der Wagen steht, dann 0.  LAUF ≡ (LFG ODER Wagen bewegt) [F].
* **Zielabstand**: `tabz` zählt in **Spalten** (Zählschritt bei Takt-Phase 5 [Q:3306-3331]); ≥ 20 Spalten → VTAB=1 (schnell), < 16 → VTAB=0 (Druckgeschwindigkeit),
  `LFG`=0 etwa 2 (bei VTAB=1: 7) Spalten vor dem Ziel [Q:3333-3366,3350-3356].
* **Druckgeschwindigkeit**: 180 Z/s ([Entwurf §2.3]) ⇒ 1800 Takte/s [?], also ≈ 1365 Takte der CPU je Taktimpuls.  Der Takt-Int braucht
  ≈ 350–600 T (grob gezählt, Druckpfad am längsten) [?].
* **Ränder**: **/LR (1A.5) = 0** für Positionen ≤ **Takt 76** (Zähler setzt C'=7, H'=6; Druckspalte 0 beginnt bei Takt 80),
  Int ECH; **/RR (1B.5) = 0** für Positionen ≥ **Spalte 142** (schmal) bzw. **220** (breit) (setzt C' auf 142/220, H' bleibt)
  [Q:1239-1240,1269-1284].  Pegelsignale; die Randsensoren bleiben aktiv, solange der Wagen links/rechts davon steht.
  Randereignis bei laufendem Wagen in falschem Zustand = `nmi` (Havarie) [Q:1249]; im `asl`-Modus setzt der Rand `tabz`=7 (Spalten) [Q:1254].
* **/VK**: 0 = Wagen im Vorwarnbereich am linken Anschlag [?]; bestimmt nur die Richtung des ersten Anlaufs (§11).
* Wagenbewegung und `H'`/`C'` laufen weiter, auch wenn LFG=0 (Auslaufen); Takt-Int zählen immer.

### 10.2  Papiervorschub (je Bahn)

Hardware-Schrittgeber mit Rückmeldung; die Firmware zählt Schritte (1 Schritt = 1/48″ [?]).

1. Richtung `XPWV` setzen, Frequenz `FRH`=0, `XPFG` := 1 (Start), PIO-Filter auf D4 allein, CTC 2/3 `B7H,44H` (7,08 ms) [Q:4644-4690].
2. Hardware liefert Impulse auf **1A.4 / 1B.4 (aktiv low)**.  Je Impuls: CTC neu `44H`, Zähler `(IX+38H/39H)` −1, `FRH`:=1 solange > 1 Schritt übrig, bei
   genau 1 `FRH`:=0, bei 0 `XPFG`:=0 [Q:4728-4833].
3. Nach dem letzten Impuls CTC `64H` (10,4 ms) Beruhigung; dann Ende bzw. bei Rückzug 4 Schritte vorwärts [Q:4771-4776,4848-4867].
4. **Bleibt ein Impuls länger als 7,08 ms aus**: `/TF`:=0, Havarie [Q:4869-4873].
5. Papierende (`BCH.0`=0) beim Vorschub: 82 weitere Schritte (`(IX+3CH)`=52H), dann „ape“ [Q:4733-4747].
6. Beim Einschalten einmal 24 Schritte je Bahn (Anfangs-PV, dann Rückzug) [Q:1445-1450,1456-1467].

Impulsabstand muss < 7 ms sein; Anlauf (FRH=0) langsamer als Lauf (FRH=1) [?]; Zeilenvorschub = 6 (8 lpi) oder 8 (6 lpi) Schritte [Q:4217-4221].

### 10.3  Kopfanschwenken / Farbband

`KAS` (2B.2) und `FMA` (2B.4) sind reine Ausgänge ohne Rückmeldung.  Vor dem Druck `KAS`=1, danach 1–2 Perioden CTC 1 (≈ 27–53 ms)
Wartezeit [Q:1046-1057]; im Leerlauf `KAS`=0, `FMA`=0 [Q:1439-1442].  Beim Einschalten ist `FMA`=1.  **Farbbandhavarie** = NMI (§8).

### 10.4  Nadeln

* 9 Nadeln: **BEH D0…D7 = Nadel 1…8**, **BFH D7 = Nadel 9**, jeweils **low = feuern** [Z, Q:3281-3282].  Nadel 1 = obere Zeile [?].
* Die Daten stehen in einem Speicher (Latch), bis sie überschrieben werden; FFH nach jedem Zeichen und beim Anlauf [Q:3280-3282,3641-3643].
* Ausgelöst wird mit **FGDI (2B.5)**: `SET` — `OUT` — `RES` — `OUT`, d. h. ein Impuls von ≈ 19 T (≈ 7,7 µs bei 2,4576 MHz) **am Anfang jedes Takt-Int**
  (vor der Datenausgabe) [Q:3168-3171,3195-3199].  Das Muster wird in Takt *n* ausgegeben und in Takt *n+1* gefeuert [F].
* Die Impulsbreite bestimmt die Hardware, nicht die Firmware [?]; die CPU wartet nicht darauf.  Auffällig: der Takt-Int startet CTC 0 neu
  (TC 23 × Vorteiler 16 = 368 T ≈ 150 µs bei 2,4576 MHz) — möglicherweise die Nadelimpulsbreite [?], jedenfalls ist die SIO-Baudrate dann gestört.
* DTFG (2B.1) = 1 vom Anlauf bis nach der letzten Spalte; gatet vermutlich FGDI [?] (für das Punktbild unerheblich: außerhalb stehen FFH-Muster).

### 10.5  Punktraster und Zeichenzelle [Q:3163-3250,3612-3650]

Zelle = **10 Takte** je Zeichen (Normaldruck): Takt-Phase `b'` 9 (Anlauf: DTFG=1, Spalte 0 ausgeben), 0…5 (Spalte *k*+1 ausgeben, Spalte *k* feuert),
Spalte 6 feuert bei `b'`=6, 7 (ZEICHENENDE: `DTFG`=0, nächstes Zeichen holen), 8, 9.  **7 Punktspalten × 9 Zeilen**, danach 3 Takte Abstand.

* Breitdruck (`ESC [ 1 m`, BDE): Spalten auf jedem zweiten Takt, Zelle 20 Takte, Position ×2 [Q:1083-1089,3652-3709].
* Schrägdruck (`ESC [ 3 m`, SDE) und zweiter Satz (`SO`) [Q:3474-3480]: Markerbytes 88H/8CH/8EH im Zeilenspeicher; Verschiebung der Spalten [Q:3711-3855].
* Zeichenposition `dpz` 0…131 (schmal) bzw. 0…209 (breit); Zeichen *n* beginnt bei Spalte `kpz` = *n* + 8 (`at` = dpz + 8, `et` = dpz + 7) [Q:2141,2182,2403-2440].
* Zeile = 9 Nadeln, 8 lpi = 6 PV-Schritte [Q:2217-2221]; vertikale Nadelteilung unbekannt (9 Zeilen in 1/8″ Zeilenabstand, 1/72″ üblich [?]).

## 11  Grundstellung beim Einschalten (und Reset `sy00`) [Q:123-476]

1. `IM 2`, I=2CH, SP=2C28H; CTC 0 Vektor E0H, Steuerwort 17H ohne TC; PIO 1/2/3 programmieren (§5.1).
2. `IN P3BD` D0: serielles Interface → PIO 3 A Mode 3, `IN P3AD` D0 wählt IFSS/V.24; SIO nach §7; Vektortabelle 2CE0H… füllen.
3. RAM 2C3CH–2CDFH löschen, Tab-Marken im Abstand 8 (nur die ersten 132 Zeichen, JDE), Ausgänge P2AD=10H, P2BD=18H, P1xD=08H.
4. **Formularlängen** aus BCH.4–7 (Bahn 1/2), `kanalreset` auf CTC 0 (`01H` wirkt als **TC=1** → CTC 0 läuft sofort mit 9600 Bd).
5. Baudrate nach 6FH/3A (`(IX+0ECH)`), CTC 1 `B7H,FFH`, `EI`, warten bis `kber`=0: **13 CTC-1-Interrupts ≈ 345 ms** (`sy22`).
6. `sy23`: Havarie, wenn Latch /TF=0 (nie nach Einschalten).  `syn`=0: **/VK** (2A.3):
   * /VK=1 → `VW`=0 (rückwärts), `tabz`=FFH; VTAB=1; LFG **kurz auf 1 und wieder 0** (zwei `OUT` hintereinander), `asl`=1; dann **warten bis LAUF=0**
     (`sy30`) → erneut `sy18`.
   * /VK=0 → `VW`=1 (vorwärts), `tabz`=1EH, VTAB=1, dieselbe LFG-Spitze, dann wie oben.
7. Zweiter Durchlauf (`asl`=1): **LFG=1 (Dauer), `syn`=1**, die Hauptschleife läuft.  Der Wagen fährt (rückwärts, wenn /VK=1) bis **/LR=0** → Int ECH:
   Zähler C'=7, H'=6, `tabz`=7; Bremsen ≈ 5 Spalten später; LAUF→0.  Erst danach ist die Grundstellung erreicht, `sy23` wiederholt, `sy32`: /LR=0 → weiter.
8. Anfangs-PV 24 Schritte je Bahn (`apv`), Rückzug im Druckbild; Online/Offline nach 3B.1, `ifg`/Lampen gesetzt (§5.2); Empfänger frei, DC1/„ACK“ zum Host.

**Worauf die Firmware wartet** (sonst hängt sie): CTC-1-Interrupts (13×); LFG→LAUF (µs); Takt-Impulse bei laufendem Wagen (sonst bleibt `tabz`
stehen); **/LR=0** nach Rückwärtslauf; **LAUF→0** nach dem Auslaufen; PV-Schrittimpulse < 7 ms; /PE=1, INA=0, ZFV=1.

## 12  Selbsttest (TEST-Taste) [Q:1889-1969, Z]

Bedingung: offline und 3B.6=0 [Q:809-815].  Ablauf: `test`-Bit, Papierende prüfen, dann `konst1` (3300H) als Zeichenstrom an `if03` (gedruckt wie empfangen).
Ablauf des Stroms (Bytes aus `1157zg.rom` ab 0300H):

1. `CR LF`, `SERIENDRUCKER`, `ESC [ 1 m` (Breitdruck), ` robotron 1157 `, `ESC [ 3 m` (Schrägdruck), `Autotestprogramm 1 :`; dann 39 × `NUL`.
2. `LF LF2 CR`, dann **Zeichen 21H…5FH** (`!` … `_`), `CR LF LF2`, dann **60H…7EH** (`` ` `` … `~`).
3. Ohne ZG2 (hier der Fall) `SI LF` Ende.  Mit ZG2: `LF2 SO`, 20H…3FH, `LF LF2 CR`, 40H…7EH im zweiten Satz, `SI LF`.
4. Am Zyklusende: TEST noch gedrückt und offline → Wiederholung; online oder TEST losgelassen → FF1 (`0CH`) bzw. bei 2 Bahnen FF2 (`13H`), dann Neustart `sy00`.

Erwartetes Druckbild (Typ 264): drei Zeilen — Titelzeile (Mitte breit, hinten schräg), Zeile mit 63 Zeichen `!"#…_`, Zeile mit 31 Zeichen `` `abc…~ `` —, Seitenvorschub.  `LF2` ist
bei einer Bahn unzulässig (`ub`-Bit, wird ignoriert, nichts gedruckt).  Der Wagen fährt im Test zwischen den Rändern (`m06cc`: `tabz`=255 zum rechten/linken Rand), der Test braucht
also beide Randsensoren [Q:903-918].

## 13  Typ 264 / 267

Die Firmware unterscheidet die Ausführung nur an **BCH.2 (EBD)** und **BCH.3 (BDA)**; DA-Antwort `ESC [ n c` [Q:2803-2819]:

| EBD | BDA | Kennung `n` | Zeichen/Bahn | Rand `/RR` | Bemerkung |
|---|---|---|---|---|---|
| 1 | 0 | `4` | 132 (dpz 0…131) | Spalte 142 | **Typ 264** (laut [T]/Entwurf) |
| 1 | 1 | `5` | 210 (dpz 0…209) | Spalte 220 | Einbahn breit — Typ unbekannt [?] |
| 0 | 0/1 | `6` | je nach BDA | je nach BDA | **2 Bahnen** (Typ 267 [?] — 210 Spalten = BDA=1) |

Zwei Bahnen: zweiter Zeilenspeicher (2E7FH–2FFFH), zweiter PV (PIO 1 B, CTC 3, Vektor EEH/E6H), zweite Formularlänge (BCH.6/7), Befehle LF2/VT2/FF2/VPA2…/LPF2
(`11H`, `12H`, `13H`), und das ZG2/ZFV-Gerüst [Q:2036-2045,2665-2667].  Beide Bahnen liegen **nebeneinander und werden vom selben Kopf bedient** (gleiche `dpz`-Skala, eigener Zeilenspeicher
je Bahn, `m1117`: `at2`=dpz+8).  Das Bahnumschalten läuft über `IX` = `mbytes` bzw. `m2c71`, Bit 3 von `(IX+34H)` = Bahn 2 [Q:4625-4628].
Hardwareseitig bleibt alles andere gleich; der Gesamtvorrat 210 gilt für beide Bahnen zusammen.

## 14  Offen [?] und Prüfkriterium

| # | Offen | Prüfkriterium |
|---|---|---|
| 1 | CPU-Takt des SD 1157 (2,4576 abgeleitet, gestützt durch K6316-Quarz und gleiche Baudprogrammierung; kein eigener Schaltplan) | IFSS 9600 Bd (TC=1) im Emulator mit 2,4576 MHz exakt; Zeitgeberzeiten (26,6 ms vs. 26,2 ms) nirgends kritisch, PV-Wächter 7,0/7,08 ms nur Obergrenze |
| 2 | Takte je Zoll, Zeichen/Zoll, Nadelteilung | Bildlauf: Zeichenbreite 7 Spalten; Seitenverhältnis gegen Druckmuster [Original `1157`-Beschreibung] |
| 3 | Rolle von CTC 0 beim Druck (Nadelimpulsbreite oder nur Baudstopp) | Emulator mit CTC 0 als SIO-Takt: SIO-Baud während des Drucks ändert sich; Empfangsfehler dort nur im Fenster Druck |
| 4 | Daisy-Chain-Reihenfolge | Lauf ohne Hänger mit Reihenfolge CTC>PIO2>PIO1>SIO>PIO3; Tick-Int dürfen nicht länger als 1 Taktperiode blockiert werden |
| 5 | /VK-Lage, Anlauf-/Bremsweg, Takt-/Randnullpunkt | Grundstellung läuft aus beliebigem Startpunkt durch (zwei Startlagen testen); Bremsweg 8…70 Takte egal |
| 6 | PV-Impulsform, Frequenzen, ob Schritt = 1/48″ | PV-Zähler: 6/8 Schritte je Zeile; Formularlänge 192…576 Schritte = FF-Länge in Zoll |
| 7 | ONLINE rastend oder Taster; TEST rastend | Selbsttest läuft bis TEST losgelassen wird |
| 8 | Typ 265/266 ↔ Kennung `5` | – |
| 9 | Genaue Bedeutung von OPR / EXT / HDUPL | nur V.24, für IFSS ohne Wirkung |
| 10 | Pegel V.24-Gegenstelle (CTS/DCD/SYNC) | V.24 nur mit CTS=DCD=aktiv und SYNC=0 sendet/empfängt |
| 11 | a0-i/a3-i/ac-i/IFG als Interface-Signale | nur Latch; kein Eingang beim Emulator nötig |
| 12 | Parallelinterface (PIO 3 A Mode 2) | nicht untersucht; nicht Ziel |
| 13 | Papierende-Lampe | kein Ausgang im Code gefunden |
| 14 | Inhalt 2980H–2BFFH | im Abzug FFH |
| 15 | `original/SD1157_Interfacebeschreibung_IFSS.pdf` ist ein Scan ohne Text | nicht ausgewertet (OCR offen) |
| 16 | Karteneinzug 1161 (`ina`, ZFV, 3B.7, ZB/ZLB/KAB/FSB, CTC 2 `C0H`) | nicht Ziel; ZFV=1 und INA=0 genügen |

## 15  Was der Emulator liefern muss (Sd1157Steuerung + Nadeldruckwerk)

**CPU/Speicher**

1. Z80 mit IM 2, I-Vektor 2CH, NMI auf 0066H; ROM 0000H–297FH (`1157.rom`, **das ROM-Abbild, nicht die Quelle**), RAM 2C00H–2FFFH und 3800H–3BFFH, `1157zg.rom` auf 3000H–33FFH, **3400H–37FFH liest FFH**.
2. Takt 2,4576 MHz [?] (alle Zeiten in Maschinentakten).

**E/A**

3. SIO (7CH–7FH): A = V.24, B = IFSS, Vektor wie in §7 (F0H…FEH), „Status wirkt auf Vektor“; IFSS: 9600 8N1 über `SerialAnschluss`; CTC 0 als Baudquelle (Baud = f/(256·TC)) — Rx während des Drucks nicht zugesagt.
4. CTC (DCH–DFH): Kanäle 0–3, Vorteiler 16/256, Reset ohne TC stoppt, Int-Vektor E0H; Kanäle 1–3 mit Int (E2H/E4H/E6H).
5. PIO 1/2/3 (F4H…FBH, ECH…EFH) mit **Modus 3 samt Interrupt-Logik** (ODER, aktiv high/low, Maske); Ausgangsbits lesen ihr Latch; Eingänge laut §5.
6. 6FH (Schalterblock 3, V.24) und BCH (8212: /PE=1, 6/8 lpi, EBD, BDA, Formularlängen; **frei einstellbar je Typ**).
7. BEH/BFH: Nadelmuster speichern (kein Auslösen beim Schreiben), BFH nur D7.
8. PIO 3 B: 3B.0=0 (seriell), 3B.1=0/1 (online), 3B.2/.3/.6=1 (Tasten nicht gedrückt), 3B.4 je Schalter, **3B.5=1**; PIO 3 A: D0 = 0 (IFSS) oder 1 (V.24), Rest Schalterlage.

**Wagen (Maschinenzeit)**

9. LFG 0→1: **LAUF=1 innerhalb weniger µs**; Wagen läuft in Richtung VW (bei Auslauf nicht ändern), Geschwindigkeit nach VTAB; Takt je Position als Impuls auf 2A.0 (vorwärts) / 2B.0 (rückwärts), Impulsbreite ≥ 1 µs, aktiv high; Takt-Abstand > Takt-Int-Dauer (≥ 1000 T bei Druckgeschwindigkeit, ≥ 600 T bei VTAB=1 [?]).
10. LFG→0: nach 8…70 Takten stehen, **LAUF→0**; Takte zählen weiter.
11. **/LR (1A.5) = 0** wenn Position ≤ 76; **/RR (1B.5) = 0** wenn Position ≥ 1420 (schmal) bzw. 2200 (breit); sonst 1.  Position = Takte; Startlage z. B. Takt 300; /VK (2A.3) = 0 nur ganz links (z. B. ≤ 10).
12. Weiche Anschläge (Position −200…2400), kein NMI.

**Papier**

13. Mit XPFG=1: Impulse auf 1A.4/1B.4 (aktiv low, ≥ 10 µs), Abstand 2–4 ms (FRH=1) bzw. 4–5 ms (FRH=0), **immer < 7 ms**; Richtung nach XPWV; **je Impuls ein Schritt = 1/48″**; keine Impulse bei XPFG=0; /PE=1 (Papier da) und ein Papiermodell mit Seitenlänge.

**Druck**

14. **Punkt** bei FGDI 0→1 (2B.5, Pulsbeginn): Nadel *k* (1…8) feuert, wenn BEH-Bit *k*−1 = 0; Nadel 9, wenn BFH-D7 = 0; Ort = aktuelle Wagenposition (Takt); Zeile = aktuelle Papierposition; Punktkreis (Breite/Höhe [?]).
15. Zeichenzelle = 10 Takte, Spaltenabstand 1 Takt; nichts zu simulieren außer Takten und Nadelmuster.
16. KAS=0 (Kopf weg): Punkte verwerfen [?], FMA unbeachtet; NMI-Eingang für Farbbandhavarie (Anwenderknopf).

**Bedienfeld/Anzeige** (`Sd1157Bedienfeld`): ONLINE (3B.1), PV1 (3B.2), PV2 (3B.3), TEST (3B.6), Kopf (3B.4), 6/8 lpi (BCH.1) als Eingabe; Lampen ONLINE (2A.1, 1=an), FEHLER (2B.3, 0=an).

**Abnahme**: Einschalten bis Grundstellung (/LR-Fahrt, 24-Schritt-PV) ohne Fehlerlampe; TEST druckt Titelzeile + 63 + 31 Zeichen (ohne ZG2); `ESC [ 0 c` antwortet `ESC [ 4 c`; 9600 Bd IFSS: DC1 nach dem Einschalten, DC3 ab 3BF5H.

## 16  Ergebnisse WP3/WP2b (2026-11-05) — Abweichungen von §10/§15

Der SD 1157 druckt mit dem Original-ROM seinen Selbsttest auf ein Blatt im Kern (`doc/design/49_k1520printer.md` §5b, `doc/merkposten/drucker.md`).  Was sich gegenüber den
Annahmen oben geändert hat:

| § | Annahme | Befund |
|---|---|---|
| §10.1, §15 Nr. 11 | /VK = 0 nur ganz links (≤ 10) | **/VK deckt den /LR-Bereich ab** (`vk_bis ≥ 76`, Vorgabe 100): die LFG-Spitze beim Einschalten bewegt den Wagen um den Bremsweg, danach wird /VK erneut gelesen; mit ≤ 10 bleiben Startlagen im Randbereich am Anschlag hängen |
| §10.1, §15 Nr. 10 | Bremsweg 8…70 Takte | **20 Takte bei Druckgeschwindigkeit, 70 bei VTAB = 1** (~ v², dieselben Zahlen, mit denen die Firmware vor dem Ziel bremst) |
| §10.2 Nr. 6 | „einmal 24 Schritte je Bahn (Anfangs-PV, dann Rückzug)“ | **56 Schritte**: 24 vor, Rückzug 24 + 4 zurück (Spielausgleich, `add a,4`), 4 vor; netto 0 |
| §5 Kopf | Eingänge der PIOs wirken als Pegel | im Modus 3 sind die Interrupts **Ereignisse** (`setModus3Flanke`); als Pegel fordert jede Pegelauffrischung den Randinterrupt neu an |
| §10.4 | Impuls = Punkt an der Wagenposition | der Punkt landet eine **Flugzeit von 0,5 Takt** in Fahrtrichtung hinter der Ausloeseposition (vorwärts rechts, rückwärts links); sonst liegt der Rückwärtsdruck der Firmware 1 Takt rechts |
| §5.4, §9 | 6/8 lpi, ONLINE als Pegel | die Firmware liest die Taste 6/8 lpi (BCH.1) und den Online-Zustand (Flag `off`, Lampe) nur **beim Anlauf**; PV/TEST-Pegel gelten zur Laufzeit |
| §10.5 | 7 Punktspalten je Zelle | bestätigt: Zelle 10 Takte ab Takt 80, Spalte k bei +k; Breitdruck 20 Takte (Spalte k bei +2k, Spalte 6 wiederholt bei +14, UND mit der Vorspalte), Schrägdruck Nadel j um 8−j Takte versetzt |
| §12 | Selbsttestbild | bestätigt Punkt für Punkt (Orakel aus `1157zg.rom`); die mittlere Zeile (63 Zeichen) druckt die Firmware rückwärts |
| §7, §1 | Protokoll DC1/DC3, Schiebepuffer | Original-Abzug: der Puffer wird nie leer (Byte 0D34H, §1, Hypothese Bitkipp) → **reparierte Fassung ist Vorgabe**.  Gemessen (`Sd1157Fluss.*`, `Sd1157Text.*`): **IFSS** — DC1 nach dem Einschalten (~2,9 Mio. Takte), **DC3 bei Druckbeginn einer Zeile** (m0d70) und bei `ipzf` ≥ 3BF5H (nach 1014 Zeichen), **DC1 erst, wenn Puffer leer und Druck fertig** (m0c8c); ein Host, der DC3 nicht beachtet, verliert hinter ~1 KiB Daten (Puffer 3800H–3BFFH).  **V.24** — zusätzlich DC1 nach JEDEM aus dem Puffer geholten Zeichen (ROM-Patch).  Offline im Betrieb: Druck läuft weiter, **keine Freigabe (DC1) bis wieder online**; ein Zeichen im Leerlauf offline löst DC3 aus (seri8).  DC1 am V.24 geht erst nach Anbinden des Transports hinaus (CTS/DCD) |
| §10.4, Nr. 4 | Flugzeit | Takt-/FGDI-Abfolge des Emulators ist richtungssymmetrisch (Latenz FGDI nach Takt-Impuls vorwärts 139 / rückwärts 136 Takte, max 211 von 1365 je Takt-Periode, Wächter `Sd1157Selbsttest.FgdiFolgtDemTaktImpulsInBeidenRichtungenGleichSchnell`): der 1-Takt-Versatz stammt aus dem **Trigger der Firmware** (rückwärts eine Zählstelle später) und wird nur durch Flugzeit 0,5 Takt je Richtung (oder eine gleichwertige Phasenlage des richtungsdekodierten Takts [?]) ausgeglichen — kein Emulatorartefakt |
| §5.1 Text | Schrägdruck | Hinter einem Schräg-Lauf bleibt **eine Zelle leer** (m14a5: `inc dpz` beim Verlassen von `sdrf`); am Zeilenende ohne Folgen |
| §7 | Zeile > 132 Spalten | nicht umgebrochen: ab dpz ≥ 84H wird `fof` (zmb Bit 3) gesetzt, weitere Zeichen entfallen (m13ab) |
| §7 | Antworten | `ESC [ 0 c` → `ESC [ 4|5|6 c` (264 / breit / 2 Bahnen); `ESC [ 5 n` → `ESC [ 0 n` (nichts), `ESC [ 2 n` (Papierende, nur wenn /PE = 0) — am IFSS und V.24 (`ist13..ist20`) |
| §9 | Online-Taste | zur Laufzeit wirken nur PV1/PV2; TEST und Lampe nur nach /RESET bei offline (`off` nur `sy40`/`m05b3`), Merkposten Nr. 10 |

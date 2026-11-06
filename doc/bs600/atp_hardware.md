# ATP 590068 / ASS 590069 — was real darauf sitzt (Befund AP-B0)

Entwurf 48, AP-B0 (2026-11-04).  Quellen: das **Gerätehandbuch PRG 710** (48 Seiten JPG, mit Tesseract gelesen: Abschnitte 1.4 „Adressenbelegung“, 1.5 „Schnittstellen“, 1.8 „Grunddiagnose“) und das
**Gerätehandbuch PRG 700** (`PRG700_Geraetehandbuch.pdf`, Textschicht; gleiche Tabellen), `doc/design/20_prg710.md`, `doc/prg710/sif1000_fernschreiber.md`, die Programmcodes `BS620`/`BS600`
(`doc/bs600/bs620.mac`, `doc/bs600/selbsttest.md`).  **Ein Stromlaufplan der Karten 590068/590069 ist nirgends veröffentlicht** (Entwurf 20 §1); die Fotos in `Fotos_PRG700/` zeigen Gehäuse und Laufwerke, keine ATP.

Gewissheit: **[Plan]** Gerätehandbuch (kein Schaltplan, nur Tabelle/Text), **[Disk]** aus dem Programmcode (BS600-Abzüge u. a.), **[?]** Annahme.

## 1. Die Adresstabelle des Geräts [Plan]

Wortlaut der Tabelle „Adressenbelegung“ (PRG 700 und PRG 710 gleich; Spalte Bausteine = die Typen der Kartentabelle, Spalte Baugruppe):

| Port | Funktion | Baustein | Baugruppe |
|---|---|---|---|
| 80–83 | CTC | U857 | ZRE K2521 |
| 84–87 | PIO | U855 | ZRE K2521 |
| **C0–C3** | **SIO – IFSS (Diagnosebetrieb)** | U856 | **ATP 600** |
| **C4, C6** | SIO – IFSS für **PC-Anschluss** | U856 | ATP 600 |
| **C5, C7** | SIO – IFSS für **FS-1200-Anschluss** | U856 | ATP 600 |
| **CC** | CTC – IFSS für PC-Anschluss | U857 | ATP 600 |
| **CD** | CTC – IFSS für FS-1200-Anschluss | U857 | ATP 600 |
| **CE, CF** | CTC Kanal 2, 3 — **nicht benutzt** | | ATP 600 |
| C8, C9 | Tastaturschaltkreis | 8279 | ATP 600 |
| D0–D3 | PIO — EPROM-Programmierung | U855 | ATP 600 |
| D4 | EPROM-Programmierung | 8212 | ATP 600 |
| DC | Rücksetzen EPROM-Programmierung | 8205 | ATP 600 |
| E8–EF | Umschalten von Speicherbereichen des Arbeitsspeichers | | RAM-Karte (TRAM 3702 / 939702) |
| 10–18 | PIO Floppy-Anschluss | U855 | AFS K5122 |
| 50–53 / 54–57 / 58–5B / 5C–5F | SIO V.24 / PIO Übertragungsparameter / CTC Taktversorgung / SIO IFSS für Drucker | U856/U855/U857/U856 | ASS K8025.60 |
| E0–E3 / E4–E7 | PIO Stanzer / PIO Leser | U855 | ADA K6022 |

Die Spalte „Baugruppe“ ist in der Handbuchtabelle für die Zeilen C0 bis DC **eine** Gruppe: **ATP 600**.  Das Kapitel „Serielle Schnittstellen“ (§1.5.2) sagt dazu:
„Auf den funktionell zusammengehörigen Kartenbaugruppen **ATP/ASS 600** befinden sich die Schaltungen für die serielle Ankopplung von Steuerungen (X3) und für Fernschreiber (X1) sowie Schaltungen für den
Tastaturanschluss (X2) und die EPROM-Programmierung.  … Der Sender der seriellen Schnittstelle arbeitet im Aktiv-, der Empfänger im Passivmodus; die Stromeinspeisung erfolgt über eine Konstantstromquelle“ [Plan].
Die Karte ASS 600 trägt also (wenigstens auch) die Treiber der 20-mA-Schleifen; welche Bausteine **auf welcher** der zwei Karten sitzen, steht nirgends [?].

Das Gerätehandbuch führt außerdem „4 × 20-mA-Stromschleife/IFSS“ und „1 × V.24“ als Gesamtzahl: 1 × Fernschreiber FS 1201.03 (X1), 1 × PC/Steuerung (X3), 2 × Drucker (K8025.60 X5/X6), 1 × V.24 (K8025.60) [Plan].

## 2. Was daraus für die ATP folgt

1. **Zwei SIO-Bausteine (U856) liegen auf der ATP/ASS**: ein Baustein auf **C0–C3** (nur „Diagnosebetrieb“) und einer auf **C4–C7** (Kanal A = PC-Anschluss, Kanal B = FS 1200) [Plan].
   Die Portordnung beider Blöcke ist dieselbe: **Daten A, Daten B, Steuer A, Steuer B** (A0 = B/A, A1 = C/D) — der Code von BS620 (`OUT (C0H)`, `IN (C1H)`, Steuer C2H/C3H) und das SCPX-BIOS der 590069 (`OUT (C5H)`, `IN (C7H)`) [Disk].
2. **Eine CTC (U857) auf CCH–CFH**: Kanal 0 (CCH) ≙ „IFSS für PC-Anschluss“, Kanal 1 (CDH) ≙ „IFSS für FS 1200“; Kanäle 2 und 3 unbenutzt [Plan].  Der Code des Fernschreibertreibers programmiert CDH als **Zeitgeber**
   (07H, 18H; ZC/TO1 = φ/384 = 6400 Hz, ×64 = 100 Bd) [Disk] — der Kanal ist also die **Baudratenquelle** der SIO-Kanäle.  Die Zuordnung Kanal 0 → PC (C4/C6) ist die des Handbuchs; BS600 programmiert die
   ATP-CTC **außer im Selbsttest nie** (keine `OUT`-Befehle nach CCH–CFH im ganzen Abzug) [Disk]; wie der PC-Anschluss von BS600 getaktet wird, bleibt offen [?].
3. **Der Kern bildet die SIO C4H–C7H und die CTC CCH–CFH heute auf der `Ass590069` ab** (Entwurf 20 §3.5), die ATP trägt nur 8279 und EPROMmer.  Nach dem Handbuch gehören **beide** zur Gruppe ATP/ASS 600 — für den Kern ist
   die Zuordnung zu einer Klasse eine Frage der Ordnung, nicht des Verhaltens.  Der Test „CTC ATP KANAL“ in BS620 trifft **dieselbe** CTC wie der Fernschreibertreiber (CDH) [Disk].
4. **Tastatur und EPROM-Programmierung** (8279 C8/C9, PIO D0–D3, 8212 D4, 8205 DC) sind bereits modelliert (`atp590068`, `eprommer590068`).  `DC` (Rücksetzen) wird vom Code geschrieben
   (`OUT (DCH),A` bei 924B nach dem Brennen) [Disk]; im Kern ist DC unbelegt (folgenlos).

## 3. CLK/TRG der CTC CCH–CFH  [Disk] / [?]

Der Selbsttest „CTC ATP KANAL“ (`6F4A`) stellt **Zähler**betrieb (57H) ein, lädt 1FH und liest **zwölf Takte später** zurück; er fordert **Zählerstand ≠ 1FH**.  Daraus:

- An **CLK/TRG0 und CLK/TRG1** (und nur dort: es sind zwei getrennte Eingänge, jeder mit eigener Taktquelle) liegt ein Takt, der in 12 Prozessortakten (≈ 5 µs) **mindestens eine** Flanke liefert: **≥ ≈ 200 kHz** [Disk, gerechnet].
- Die Quelle ist **nicht** ZC/TO eines Nachbarkanals (Kanal 1 wird vor Kanal 0 geladen) [Disk].
- Danach stellt der Test beide Kanäle auf Zähler mit Zeitkonstante **1** (55H, 01H): ZC/TO = CLK (÷1).  Das spricht dafür, dass CLK/TRG0/1 **ein Takt aus der Schnittstellenschaltung** sind (z. B. ein fester Quarz/Teiler),
  der unverändert als Bit-Takt an die SIO geht [?].  Der Fernschreibertreiber benutzt dagegen den **Zeitgeber** (φ-getaktet) — beide Betriebsarten sind belegt.
- Welche **Frequenz** tatsächlich anliegt, ist unbekannt (Kandidaten: φ = 2,4576 MHz; 614,4 kHz; 153,6 kHz …).  Für den Test genügt jeder Takt ≥ ≈ 200 kHz [Disk].
- **Die Einstellung des Tests bleibt stehen:** BS600 programmiert die CTC CCH–CFH sonst nie; der PC-Anschluss (SIO C4H/C6H, Interrupt-Treiber `8430`–`8493`) läuft also mit **ZC/TO0 = CLK0** (Zähler, Zeitkonstante 1).  Das stützt „fester Bittakt am CLK/TRG-Eingang“ [?].
- Die Weiterverwendung nach dem Test ist offen: ob ZC/TO0/1 die **Sende-/Empfangstakte der SIO C0–C3** (Diagnosekanäle) speisen [?].

## 4. SIO C0H–C3H „Diagnosebetrieb“  [Plan] + [Disk]

- [Plan]: „SIO – IFSS (Diagnosebetrieb)“; [Disk]: BS620 initialisiert beide Kanäle gleich (WR4 = 4FH ×16, 2 Stoppbits, gerade Parität; 8 Bit; keine Interrupts), sendet **AAH**, wartet höchstens ≈ 4 ms auf **Empfangszeichen**
  und vergleicht — für Kanal A **und** B.  Das setzt voraus, dass der **Sender des Kanals mit seinem eigenen Empfänger verbunden** ist (Schleife) und der Bittakt (×16) vorhanden ist.
- Wie die Schleife entsteht, ist **nicht belegt**: (a) fest auf der ATP verdrahtet („Diagnosebetrieb“ legt dafür einen eigenen SIO-Block an), (b) über die 20-mA-Schleife (SD+/− auf ED+/− gebrückt), (c) ein **Prüfstecker**.  Das Handbuch nennt
  keinen Stecker; die Bezeichnung „Diagnosebetrieb“ spricht für (a) oder (b) [?].
- Schwelle: Zeichen (11 Bit) in ≤ ≈ 4 ms ⇒ Bittakt ≥ ≈ 44 kHz bzw. ≥ ≈ 2750 Bd [Disk, gerechnet].
- SIO C4H–C7H (Kanal A = PC, Kanal B = FS 1200): vom Selbsttest **nicht** geprüft.  BS620/BS600 benutzen C4H/C6H (Kanal A) in einem **Interrupt**-Treiber (`8430`–`8493`, PC-Anschluss „ORIGINAL → PC → DATEINAME“, `KEIN EMPF.`, `SENDERFEHLER`) [Disk].

## 5. Anzeige F4H/F5H  [Disk] / [?]

- **Gefunden:** Ausgabe `OUT (F5H),A` + `OUT (F4H),A` in vier Stellen: im Selbsttest (`D48A`, später `26DF`/`288A`: **F5H := Fehlercode, F4H := 3EH**) und in einer Schleife bei `1C32` (**F5H := 60H, dann F4H := C, C+1, … DFH**,
  danach `CALL 0BE5`).  **Kein Lesen** (`IN F4H/F5H` kommt nicht vor).  Die Codes bei Fehlern: **01H/02H** (PIO ZE Tor A/B und SIO Kanal A/B), **30H…33H** (CTC ZE Kanal 0…3), **30H/31H** (CTC ATP Kanal 0/1), F4H immer **3EH** [Disk].
- **Das Handbuch kennt eine solche Anzeige nicht**; in der Grunddiagnose heißt es nur „CTC ZE KANAL X“ — die Kanalzahl X steht im Text des Programms **nicht** (der Text endet auf „KANAL“) [Disk].  Es ist möglich, dass die zwei
  Ports eine **Zweiziffernanzeige** (7-Segment/LED) an der ATP oder Frontplatte treiben, deren Ziffern der Code als *Fehlerkennung* nutzt (F5H = Ziffernpaar, F4H = Betriebsart/Zeichen), oder der Kanalnummer-Ausgabe auf dem Bildschirm [?].
- Die Schleife bei 1C32 (F5H := 60H, F4H läuft CH…DFH) sieht eher wie eine **Bild-/Bargraph- oder Zeichen-Aktualisierung** aus als eine Fehleranzeige; benutzt wird sie beim Plattenzugriff (Aufrufer 1C59, vor „DISK VOLL“-Meldungen) [?].
- Für den Kern genügt: **Schreibzugriffe an F4H/F5H annehmen und protokollieren** (kein Wartezustand, kein Lesen).

## 6. Speicherumschaltung E8H–EFH (nur nebenbei)  [Plan] + [Disk]

- Das Handbuch weist **E8–EF** (acht Ports) dem „Umschalten von Speicherbereichen des Arbeitsspeichers“ zu — nicht nur E8–EB wie das Arbeitsmodell des Kerns (`prg710.md`, „Speicherverwaltung E8H–EBH“).  Die Karte heißt TRAM 3702 (PRG 700) bzw. 939702 (710).
- **BS620/BS600 schreiben `E8H`-Seitenregister mit Werten, die das Arbeitsmodell nicht kennt:** `E8H[0] := 00H` (RAM), `E8H[F] := FFH` (VRAM) wie bekannt, aber **`E8H[1,7,8,9,A] := 04H`** (6E0A) und später abwechselnd 00H/04H für 7–A (`25E0`, um jedes Nachladen einer Überlagerung).
  `EBH` und `EAH` schreibt BS620 nie.  Deutung **04H = Schreibschutz** (geschütztes Programm, beim Nachladen frei) [?].
- **Kernbefund:** `Prg710Speicher::ortVon` behandelt bei `EBH ≠ 0` jedes untere Halbbyte ≠ 0 als „Systemkarte“ (Seite 1 und 7–A **leer**).  Der ROM-Lader setzt `EBH := 0FH`; BS620 stürzt also **spätestens bei `JP AFDF`**
  (Seite A) in leeren Speicher.  Das ist ein **zweites Hindernis neben CTC/SIO** und muss in AP-B1 mitbehoben werden (Wert 04H = RAM sichtbar [+ Schreibschutz optional]); UDOS/SCPX berühren die Werte nie.

## 7. Aufstellung der Folgerungen für AP-B1 (nicht umgesetzt)

| Bedarf von BS620 | Stand im Kern | Vorschlag |
|---|---|---|
| SIO C0–C3, A0 = B/A, A1 = C/D, ×16, Schleife Tx→Rx je Kanal | fehlt | zweiter Z80SIO neben dem auf C4–C7 (`Ass590069`-Ordnung); Schalter „Prüfschleife“, Vorgabe an bei BS600, in der Konfiguration |
| CTC CCH/CDH: Zähler zählt sofort | `Ass590069` hat Z80CTC CCH–CFH ohne CLK/TRG-Takt | CLK/TRG0/1 an einen festen Takt (≥ ≈ 200 kHz); ZC/TO0/1 zu den SIO-Kanälen verdrahten [?] |
| F4H/F5H | unbelegt | Schreiben annehmen, protokollieren, `atpAnzeige()` für Tests |
| `E8H` = 04H | wirkt als „leer“ | 04H wie „RAM“ behandeln |
| Tastatur 710-1 | vorhanden (K7672 an K8025 SIO B) | ESC allein → 83H (BS620-Eigenheit, s. `selbsttest.md` §3.8) |

## 8. [Anwender]-Fragen

1. **Prüfschleife der SIO C0–C3:** gibt es am echten PRG 710-1 einen **Prüfstecker** oder eine Brücke für den „Diagnosebetrieb“, oder besteht BS620 den Test ohne Zutun?  Bitte die Meldung beim fehlenden Stecker (die beste Gegenprobe, Entwurf 48 §7.3).
2. **Anzeige F4H/F5H:** was liegt an diesen Ports?  (Zwei 7-Segment-Ziffern an der ATP, eine LED-Reihe an der Frontplatte, nichts?)  Wo erscheint beim echten Gerät die **Kanalnummer X** der Meldungen „CTC ZE KANAL X“, „SIO KANAL X“?
3. **CLK/TRG-Takt der ATP-CTC:** Frequenz und Quelle (Quarz, Teiler vom Systemtakt?) und wohin ZC/TO0/1 gehen (SIO-Kanalbittakt?).  Hilfreich: die Kartenaufnahme der ATP 590068 und ASS 590069 (Chip-Typen, Beschriftungen).
4. **Verteilung auf die zwei Karten:** welche Bausteine (SIO ×2, CTC, 8279, PIO, 8212, 8205) stehen auf der **ATP 590068**, welche auf der **ASS 590069**?  (Handbuch nennt nur „ATP/ASS 600“.)
5. **E8H = 04H:** was bedeutet der Wert am echten Gerät (Schreibschutz)?  Gibt es Wirkungen von `EC`–`EF`?
6. **PRG 710 mit BS610:** die vorliegende zweite Diskette (`BS600_*`) bootet nur über das 3-KB-ROM-Protokoll (PRG 700); welche Diskette gehört zu **Ihrem PRG 710** (NKM-Lader 1 KB)?  Gibt es eine „SYL FOR BS610“?
7. **K6022:** beim Hauptmenü beschreibt BS620 die PIO-Ports E2/E3/E6/E7 (Stanzer/Leser) — ist die Karte im Gerät, und was meldet BS600 ohne sie?
8. **BS620 am echten Gerät:** läuft der Selbsttest ohne Meldungen durch und erscheint das Hauptmenü sofort?  (Bei einem Befund „PRUEFSUMME …“: s. `selbsttest.md` §5, BS620 hat die Prüfung abgeschaltet.)
9. **Entwurf 48 §7.1/7.2:** dürfen `BS600_*` (zweite Diskette) als Prüfling und beide Abzüge als Testfixtures/Auslieferung dienen?  (Empfehlung: ja für Prüfling, Auslieferung nach Freigabe.)

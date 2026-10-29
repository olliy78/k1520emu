# SIOS 1526 — Auszug aus den Unterlagen (AP S0)

Stand 2026-10-29.  Arbeitsauszug für Entwurf 43 (`doc/design/43_sios1526.md`), kein Volltext.

**Quellen**
- **[BH]** *Bedienhandbuch Bürocomputer A 5120/30*, VEB Kombinat Robotron, Robotron-Vertrieb Berlin 1984,
  AG 706/HB/49/84, 204 S. — durchsuchbare Fassung (OCR deutsch, Text als unsichtbare Schicht):
  `doc/original_docs/SIOS1526_Bedienhandbuch_A5120_A5130_ocr.pdf`; Original-Scan
  `~/projects/robotron/A5120/docs/A5120_A5130_Bedienhandbuch.pdf`.  Seitenzahlen unten = PDF-Seite.
  OCR-Qualität mäßig (Fraktur-artige Schreibmaschine) — Zahlen immer am Bild nachprüfen.
- **[K7637]** `doc/original_docs/SerielleTastatur_K7637_XX_Betriebsdokumentation.pdf` (Scan, hier per OCR gelesen),
  Abschnitte 2.1.1, 2.1.2, 3 (Bediensicherungsbaugruppe); ROM-Abzug `doc/EPROMS/robotron-k7637_50-2716.bin`.
- **[K763x]** `doc/original_docs/Tastaturen_K7632_34_35_36_Betriebsdokumentation.pdf` §4.3 (hat Textschicht).
- **[Web]** robotrontechnik.de, Seite „Betriebssystem SIOS" (lokaler Spiegel
  `~/projects/robotron/Robotron_Technik/www.robotrontechnik.de/html/software/sios.html`).
- Die beiden Lehrhilfe-Archive (`…/A5120/docs/Lehrhilfe_BC_Uebersicht_A5110_A5120_A5130.rar`,
  `…/K1520/Lehrhilfe_Software-Anwendungen_Buerocomputer_A5120-A5130.rar`, zusammen 35 Fotos) enthalten
  Folien „Software des Betriebssystems SIOS 1526", „Monitorkommandos" und „Speicherorganisation … BS SIOS 1526"
  — nur Übersicht, nichts über [BH] hinaus gefunden.

## 1. Einschalten, Systemzeile

- Nach dem Einschalten wird der **Monitor** angesprungen, sofern kein Anwenderfestprogramm im Speicher ist
  [BH S. 6].  Kommt die Anzeige des Monitorgrundzustands nicht, „liegt ein Gerätefehler vor" [BH S. 39].
- **Systemanzeige = letzte Bildschirmzeile** [BH S. 6–7]:

  | Bildschirm | Systemanzeige | bei Programmunterbrechung |
  |------------|---------------|---------------------------|
  | 24 × 80 (K7024) | letzte Zeile, Pos. 32–79 (28–31 leer) | Pos. 0–27 bleiben, 28–79 gerettet |
  | 16 × 64 (K7023) | letzte Zeile, Pos. 16–63 (12–15 leer) | Pos. 0–11 bleiben, 12–63 gerettet |
  | 12 × 40 | Zeile 11 Pos. 32–39 + Zeile 12 Pos. 0–39 | alles gerettet |

- Anzeigen [BH S. 7–8]:
  - `MONITOR ===== SIOS 1526/n.n (M)_` — Monitorgrundzustand
  - `MONITOR ===== SIOS 1526/n.n (P)_` — nach Abbruch mit der **Monitortaste M**; Programm bleibt geladen
  - `PROGRAM xx: SIOS 1526/n.n Pxx` — laufendes Anwenderprogramm (nur wenn programmiert)
  
  Im Systembereich von `SYSARE11` stehen genau diese Texte (`MONITOR ===== SIOS 1526/1.1 (M)`,
  `PROGRAM ==:   SIOS 1526/1.1  P==`) [SRC].
- Ausschalten: Kommando `OFF`; Havarie: Taste „Betriebsbereitschaft" 3× in 2,5 s [BH S. 5–6].

## 2. Bediensicherung (Sicherheitsstecker)

- Besteht aus dem **Bedienelement** (Schlüssel mit 8 Codierstopfen) und der **Bediensicherungsbaugruppe BES**
  in der Tastatur [BH S. 40, 56; K7637 §3].
- **Ohne oder mit falschem Bedienelement „verbleibt das Gerät im Monitorstatus"**; wird es während der Arbeit
  gezogen, ist jede Tastatureingabe gesperrt, das Programm hält am nächsten E/A-Befehl [BH S. 40, 56].
  ⇒ Die **Systemzeile muss auch ohne Stecker erscheinen** — der fehlende Stecker erklärt den Absturz in
  Entwurf 43 §1.2 also *nicht*, wird aber zum Arbeiten gebraucht (Programmiererstatus für MEM/POU/PLO/LOD/NEW).
- Codierung [K7637 §3.2]: 8 Bit, davon **3 Bit Bedienerkennung** (Stopfen 1–3) und **5 Bit Gerätekennung**
  (Stopfen 4–8).  Die Gerätekennung wird **in Hardware** gegen Drahtbrücken auf der BES geprüft; nur wenn sie
  stimmt, kommen die drei Bedienerbits auf die Sonderleitungen **SL1–SL3**.
  Bedienerkennung = **Binärzahl der Stopfen 1–3** (am Bild K7637-Doku S. 14 geprüft):

  | Kennung | Stopfen 3 2 1 | Funktion |
  |---------|---------------|----------|
  | xx/01 | – – o | **Chefschalter** |
  | xx/02 | – o – | 1. Bedienebene |
  | xx/03 | – o o | 2. Bedienebene |
  | xx/04 | o – – | 3. Bedienebene |
  | xx/05 | o – o | 4. Bedienebene |
  | xx/06 | o o – | 5. Bedienebene |

  Annahme [?]: Stopfen n legt (bei passender Gerätekennung) SLn auf Low; ohne Element liegen alle SL high.
- Übertragung [K7637 §2.1.1–2.1.2]: nach Einschalten und nach Kommando 00H (Software-RESET) sendet die
  Tastatur **TYP**, danach **die Codes der SL-Leitungen, die auf Low liegen**; im Betrieb wird bei **jeder
  Pegeländerung** einer SL-Leitung deren Code gesendet (Triggerverhalten).  Codes im Tastatur-ROM, `00H` =
  unterdrückt:

  | ROM-Adresse (CTAB 1 / CTAB 2) | K7637.50-Abzug |
  |-------------------------------|----------------|
  | 648H–64BH, 6D8H–6DBH / 758H–75BH, 7E8H–7EBH | **SL1 = A9H, SL2 = AAH, SL3 = ABH**, SL4 = 00H (in allen vier Tabellen gleich) |

  K7636 (parallel, an der ZRE): SL1–SL6, Codes auf 268H–26DH/2E8H–2EDH bzw. 368H–36DH/3E8H–3EDH (OCR-Lage
  unsicher), Abzug fehlt [K763x §4.3].
- Der Emulator sendet bisher nur TYP (80H) — **keine SL-Codes**.  Das Codierstopfenbild ↔ welche SL-Leitung
  Low ist, muss noch aus dem BES-Stromlaufplan abgelesen werden [?].

## 3. Tastatur „robotron A 5120/A 5130 lateinisch" [BH S. 200–201]

Zusätzlich zur Schreibmaschinentastatur: Block **CI · M · (X) · ⏻**, Zehnerblock mit **CE**, **0 / 00 / 000**,
Minus; Starttasten **S, S1…S9** mit Selektorlampen; **ET1**, **ET2**, **INS MODE**, **DEL**, **INS LINE**,
**DEL LINE**, Cursortasten.  Bedeutung [BH S. 41–55]:
- **M** = Monitortaste (Programm unterbrechen → Monitor), **ET1** schließt jede Monitoreingabe ab, **ET2** ohne
  Eingabe = Abbruch der Monitorfunktion; ET1/ET2 beenden auch Eingabebefehle im Programm.
- **CI** = Fehlerlöschung (Fehlerlampe blinkt rot bei E/A-Fehlern, CI löscht sie).
- **S, S1–S9** beenden Eingabebefehle mit Programmverzweigung; **Selektortasten** setzen/löschen Selektoren
  (Lampe).
- Eingaben außerhalb der Eingabefelder → Fehlerlampe + Sperre [Web].

## 4. Monitorkommandos [BH S. 9–18]

Dreistelliges Schlüsselwort, Begriffe einzeln mit **ET1**; Leereingabe + ET1 setzt den Standard ein.
„(P)" = nur im Programmiererstatus (Bediensicherung).

| Kommando | Wirkung |
|----------|---------|
| `ASN da [pa]` | Gerätezuweisung anzeigen/ändern (logisch `03xx` → physisch, Tabelle §6) |
| `MEM aaaa [ii]` (P) | Speicher byteweise lesen/schreiben (nicht im Betriebssystembereich) |
| `POU ttsszzz [aaaa]` (P) | Speicher physisch auf Diskette schreiben (Spur 1–76, Sektor 1–26, 1–511 Sektoren) bzw. Kassette |
| `PLO ttsszzz [aaaa]` (P) | physisch von Diskette/Kassette laden |
| `CAL bbbbb` | Programm aus der **Phasenbibliothek** laden und starten (Name 1–5 Zeichen, erstes ein Buchstabe) |
| `LOD bbbbb [aaaa]` (P) | nur laden |
| `RUN` | Programm ab AAWA starten bzw. nach M-Unterbrechung fortsetzen |
| `NEW [aaaa]` (P) | Programm ab AAWA bzw. aaaa starten |
| `GOO` | Festprogramm starten |
| `CAN` | unterbrochenes Programm beenden (wie CANCEL-Fehler) |
| `SYC` | Systemmodule nachladen (Nachlader-Konfiguration) |
| `TYP` | Schreibmaschinenfunktion (Bildschirm/Drucker) |
| `CLE da`, `REW da`, `FOR da` | Kassette reinigen/löschen, zurück-, vorspulen |
| `OFF` | Gerät ausschalten |
| `CLK [hhmmss]`, `DAT [ddmmyy]` | Systemzeit / Datum anzeigen bzw. setzen |

Fehleranzeigen und -gruppen A–D: [BH S. 19–38] (Tabellen je Gerät: Floppy `ERR FD nn`, Kassette `ERR CT`,
Drucker `ERR PR`, DFÜ, Lochband `ERR TP`, System `ERR SY`, Monitor `MEM ERR` …).  Fehlertexte z. B. `NFND`
(Datei nicht gefunden), `HDR1`-Fehler (`ERR FD 51 HDR1`).

## 5. Speicherraum und Systemzellen [BH S. 202]

| Bereich | Systemzellen |
|---------|--------------|
| 0000H–…: Betriebssystem-Verständigungsbereich + Module (wenn im RAM) | — |
| **AAWA** = Anfang Anwenderbereich | **004CH/004DH** |
| Anwenderbereich, evtl. gestützter Bereich (CMOS) | — |
| **AB** = Anfang Bildschirmarbeitsspeicher | **001BH/001CH** |
| **ERAM** = Ende RAM | **0060H/0061H** |
| **AGM** = Anfang Grundmodul (1. Betriebssystemmodul im ROM) | — |
| **AAWO** = Anfang Anwenderbereich im ROM | **006CH/006DH** |
| **EROM** = Adresse des 1. Byte des letzten ROM (1 K) | — |

Im Emulator nach dem Start von SIOS 1.1 gemessen: `[001BH] = F800H` (K7024), `[004CH] = 9000H` (Anwender-
bereich ab 9000H) [SRC].  Die Routine bei 0ABAH, in der der Lauf endet, liest genau AAWA (004CH) und ERAM
(0060H) — sie tastet den Anwenderbereich ab.

## 6. Gerätetabelle (Standardzuweisung) [BH S. 203]

Physische Adressen sind **Treibertabellen-Indizes, keine Portadressen** (handschriftlich: „keine Hardware-
adressen!").

| Gerät | logisch | physisch |
|-------|---------|----------|
| Dialoggerät | 0300 | D4 |
| Tastatur | 0301 | 0E |
| on-line Empfang / Senden | 0302 / 0303 | 24 / 3A |
| Floppy-Disk 1 / 2 / 3 / 4 | 0304 / 0305 / 0306 / 0307 | 50 / 66 / 7C / 92 |
| ½″-Magnetband | 0308 | A8 |
| Schreib-Lese-Einheit / Handleseeinheit | 0309 | BE |
| Bildschirm | 030A | D4 |
| Drucker / Zusatzdrucker | 030B / 030C | EA / 00 |
| Kassette 1 / 2 | 030D / 030E | 7C / 92 |
| Lochband / Handleseeinheit 4 | 030F | 16 |
| IFSS 1 / V.24 | 0310 / 0311 | 2C / 42 |
| Handleseeinheit 1 / 2 | 0312 / 0313 | 16 / 2C |
| Programmein-/ausgabegerät (Standard = Floppy 1) | 0315 | 50 |

Laufwerksnamen im Gebrauch: **04, 05, 06** = Diskette 1–3, **0D, 0E** = Kassette [Web]; `ASN 0315 66` macht
Laufwerk 2 zum Programmlaufwerk.

## 7. Diskettenformat (aus INIT, FGEN, LBSV) [BH S. 148–194]

- **Kennsätze in DKOI**, nicht reinem EBCDIC (ST RGW 358-76): Abweichungen u. a. bei 4AH, 4FH, 5AH, 5FH, 6AH
  (Tabelle BH S. 150, am Bild prüfen) [BH S. 150].  Normen: Robotron-Richtlinie 5,25″ vom April 1981,
  ISO/TC 97/SC 11 N 209/347, Vornorm DIN 66247 Teil 1 [BH S. 149].
- **INIT 1520 (SIEX) V 1.1** [BH S. 148–159]: formatiert **auf Laufwerk 05** (zweites Laufwerk).
  - 5,25″ einseitig doppelte Dichte: **40 Spuren**, Sektorlänge 128/256/512/1024 (26/15/8/4 Sektoren — OCR,
    prüfen), Sektorfolge wählbar (bei 128 B 01…13, Vorgabe 01 = natürlich; für 128 B ist „01" optimal).
    „Double Tracks" (80 Spuren) **in V 1.1 nicht möglich**.
  - 8″: einfache Dichte (MF 3200/MF 6400, ISOT) 77 Spuren, 128–1024 B; doppelte Dichte 128 B × 44 bzw.
    256 B × 26.
  - **Spur 00 immer 128 B/Sektor in natürlicher Folge**, Kennsätze in DKOI; **Spur 00 muss fehlerfrei sein**.
  - Von den übrigen Spuren dürfen **bis zu 2 defekt** sein — sie werden gekennzeichnet und übergangen; sonst
    werden **„die beiden äußeren Spuren" als Reserve** übergangen (welche genau — Spur 38/39? — am Abbild prüfen).  Nutzbar: 75 von 77 (8″) bzw.
    **38 von 40** (5,25″), davon Spur 00 als Indexspur.
  - Ablauf: Spur 00…39 durchgehend von Index zu Index schreiben, **beide CPUs zeitparallel** (ZVE1 steuert die
    Aufzeichnung, ZVE2 bereitet den nächsten Sektor vor), dann Kontrolllesen 39…00, bis 2 Wiederholungen,
    zuletzt Spur 00 neu mit den Nummern defekter Spuren.  Speicherschutz und Zeitunterbrechungen sind dabei
    abgeschaltet; am Ende Warteschleife → RESET/Aus-Ein.  Dauer ≈ 2 min (5,25″).
  - Menüpunkte: Satzlänge (0–3), Sektorfolge, 1. und 2. zerstörte Spur, Initialisierung.
- **FGEN 1520 (SIEX)** [BH S. 160–169]: Kennsätze generieren (Datei anlegen = HDR1 mit BOE/EOE), `CHANGE VOL1`,
  `DISPLAY`; Fehler „Spur 00 gefüllt, neuer HDR1 nicht möglich".
- **LBSV 1520 (SIEX)** [BH S. 170–194]: Bibliotheksdienst (Anweisungen u. a. `ASSGN`, `LBTYP`, Bibliothek
  eröffnen/erweitern, `PRINT DIC`); Verzeichnis-Ausdruck mit Spalten **NO · NAME · AADR · ML** (Beispiele
  Anlage 6, 7.1).  Abgleich mit dem Abbild: Eintrag = Name (bis 5 Zeichen, ASCII, mit 00H aufgefüllt),
  **AADR** 2 Byte (LE?), **ML** 1 Byte [?] — in D1 gegen den Code belegen.  „Der Datenteil jeder
  Zielbibliothek wird lückenlos fortlaufend" geschrieben (Verdichten) [BH S. 184].
- **CODP 1520 (SIEX)** [BH S. 195–199]: Disketten physisch kopieren.

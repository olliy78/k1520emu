# Formatbeschreibung ImageDisk (.imd) und TeleDisk (.td0)

Datum: 2026-10-22

## Quellen und Lizenz

- Lizenzvermerk: eigene Worte; Verhalten beobachtet an HxC Floppy Emulator (GPL), kein Code, keine Namen, keine Kommentare uebernommen; greaseweazle (`image/imd.py`, `image/td0.py`, gemeinfrei/Unlicense) durfte frei gelesen werden. Zahlenkonstanten, Offsets, Bitbedeutungen und Ablaeufe sind Fakten.
- IMD: `https://github.com/jfdelnero/HxCFloppyEmulator/tree/master/libhxcfe/sources/loaders/imd_loader` (Leser und Schreiber); Dunfields Formattext (im Quellordner als Kommentar).
- TD0: `.../loaders/teledisk_loader` (Leser, Dekompression, Dunfields "td0notes"); greaseweazle unter `venv/lib/python3.13/site-packages/greaseweazle/image/{imd,td0}.py`.
- Gegenprobe: 67 lokale `.td0` unter `~/projects` (47 verschiedene Inhalte, 1 Datei mit 0 Byte, 1 mit 20 Byte), 68 298 Sektoren mit Daten. Pruefskript: `scratchpad/td0chk.py` (eigener Parser plus eigener LZHUF-Dekoder nach dieser Beschreibung). Bezeichnungen der Befunde: **[K]** am Korpus bestaetigt, **[Q]** nur aus Quelle, ungeprueft.

---

## Teil 1: ImageDisk (.imd)

### 1.1 Dateiaufbau

1. Kopfzeile: ASCII, beginnt mit `IMD ` (4 Byte inkl. Leerzeichen), dann `v.vv: TT/MM/JJJJ hh:mm:ss`, endet mit CR LF. Danach beliebiger ASCII-Kommentar (beliebig lang, darf mehrzeilig sein).
2. Ein Byte `0x1A` beendet Kopfzeile + Kommentar. Gesucht wird das erste 0x1A ab Dateianfang. Fehlt es: Datei ungueltig.
3. Danach beliebig viele Track-Records bis Dateiende (kein Endemarker, kein Trackzaehler, keine Pruefsumme).

### 1.2 Track-Record

Kopf: 5 Byte.

| Byte | Bedeutung |
|---|---|
| 0 | Modus 0..5 (siehe 1.3) |
| 1 | physischer Zylinder |
| 2 | Kopf-Byte (siehe 1.4) |
| 3 | Sektoranzahl `n` (0 erlaubt) |
| 4 | Sektorgroessen-Code `s`: Groesse = 128 << s, gueltig 0..6 |

Danach der Reihe nach:
- Sektornummernfolge: `n` Byte (die Sektor-ID R je Sektor, in physischer Reihenfolge auf der Spur; nicht zwingend aufsteigend, Reihenfolge = Interleave).
- Zylinder-Map: `n` Byte, nur wenn Kopf-Byte Bit 7 (0x80).
- Kopf-Map: `n` Byte, nur wenn Kopf-Byte Bit 6 (0x40).
- `n` Sektordatensaetze (1.5), in der Reihenfolge der Nummernfolge.

Es gibt genau eine Sektorgroesse je Track (gemischte Groessen im Track sind nicht darstellbar). Die Umrechnung `128 << s` fuer s > 6 ist ungueltig. **[Q]** (Code 0xFF o. ae. wird von keiner der drei gelesenen Implementierungen als Sonderwert behandelt; greaseweazle lehnt s > 6 ab, HxC rechnet ungeprueft weiter.) Der Leser sollte s > 6 als Fehler abweisen.

### 1.3 Modus

| Code | Uebertragungsrate | Aufzeichnung |
|---|---|---|
| 0 | 500 kbit/s | FM |
| 1 | 300 kbit/s | FM |
| 2 | 250 kbit/s | FM |
| 3 | 500 kbit/s | MFM |
| 4 | 300 kbit/s | MFM |
| 5 | 250 kbit/s | MFM |

Die Rate ist die Uebertragungsrate (Zelltakt), nicht die Datenrate: FM 250 kbit/s = 125 kbit/s Daten. Gaengige Faelle: 2 = 5,25"/8" SD (bei 8" haeufig Modus 0), 5 = DD 5,25"/3,5", 3 = HD 3,5"/8" DD. HxC kennt zusaetzlich herstellerspezifische Codes 0x80..0x82 (MEMBRAIN, 360 U/min) – das ist eine Eigenart von HxC, kein IMD-Standard; wir weisen sie ab oder ignorieren sie. Alle anderen Werte > 5: Fehler (greaseweazle) bzw. HxC faellt auf MFM/250 zurueck. Empfehlung: Fehler.
Drehzahl: nicht in der Datei. Konvention (beide Implementierungen): 300 U/min, ausser FM/MFM 500 kbit/s mit genau 26 Sektoren (8"-Spur): 360 U/min.

### 1.4 Kopf-Byte

- Bits 0..5: Kopf (nur 0 oder 1 gueltig; greaseweazle maskiert mit 0x3F und prueft <= 1; HxC maskiert in Teilen mit 0x0F).
- Bit 6 (0x40): Kopf-Map vorhanden.
- Bit 7 (0x80): Zylinder-Map vorhanden.
- Fehlt eine Map, gilt als ID-Wert der physische Zylinder bzw. Kopf des Track-Records fuer alle Sektoren der Spur.
- Maps tragen die in den ID-Feldern geschriebenen Werte C bzw. H (koennen von der physischen Position abweichen; H darf also 0/1 sein, aber auch andere Werte). Beim Umrechnen in Bitzellen sind die Map-Werte in die ID-Felder zu schreiben, nicht die physischen Werte.

### 1.5 Sektordatensatz

Erstes Byte = Typcode `t`:

| t | Folge | Datenmarke | CRC |
|---|---|---|---|
| 0 | nichts | Daten nicht lesbar (kein Datenfeld) | - |
| 1 | `Groesse` Byte Daten | normal (FB) | gut |
| 2 | 1 Byte `x` | normal | gut, Sektor = `x` wiederholt |
| 3 | Daten | geloescht (F8) | gut |
| 4 | 1 Byte | geloescht | gut, komprimiert |
| 5 | Daten | normal | Datenfehler |
| 6 | 1 Byte | normal | Datenfehler, komprimiert |
| 7 | Daten | geloescht | Datenfehler |
| 8 | 1 Byte | geloescht | Datenfehler, komprimiert |

Regel: (t-1) Bit 0 = komprimiert, Bit 1 = geloescht, Bit 2 = CRC-Fehler. Andere Werte (> 8): Fehler.
Typ 0: Sektorkopf (ID) existiert, es gibt aber kein lesbares Datenfeld (bei Umrechnung: ID-Feld schreiben, Datenmarke weglassen/fehlend markieren). Der Dateizeiger rueckt nicht weiter.
Datenfehler (t >= 5): die gespeicherten Daten sind das, was gelesen wurde; bei Umrechnung ein falsches Daten-CRC erzeugen.

### 1.6 Sonderfaelle

- **n = 0**: Kopf ohne Nummernfolge/Maps/Saetze; der Record ist genau 5 Byte; Spur existiert, ist aber leer (unformatiert). Die Groessencode-Angabe ist dann bedeutungslos (Schreiber schreiben 0). Der Leser sollte die Spur als vorhanden, leer fuehren (nicht fehlend). [Q]
- **Fehlende Spuren**: Spuren, die in der Datei nicht vorkommen, sind unbekannt/leer. Reihenfolge der Records ist beliebig (ueblich: Zylinder aufsteigend, je Zylinder Kopf 0 dann 1); HxC liest sie in der Dateireihenfolge.
- **Doppelte Sektornummern** innerhalb eines Tracks sind erlaubt (Kopierschutz); alle Saetze behalten ihre Reihenfolge. **Doppelte Track-Records** (gleicher Zylinder+Kopf): letzter gewinnt (greaseweazle), unspezifiziert. [Q]
- **Abgeschnittene Datei**: greaseweazle ignoriert einen Rest < 5 Byte hinter dem letzten Satz; sonst Zugriff ausserhalb = Fehler. Empfehlung: Track mit unvollstaendigen Daten verwerfen und melden.
- Kommentar darf selbst 0x1A nicht enthalten.

### 1.7 Schreiben (so verhalten sich die Schreiber)

- Kopfzeile `IMD 1.17: TT/MM/JJJJ hh:mm:ss` CR LF, eine Textzeile mit Programmname, CR LF, 0x1A. (HxC schreibt den Monat 0-basiert falsch; Datum ist unwichtig.)
- Je Spur (Zylinder aufsteigend, Kopf innen): Modus aus Zelltakt und Aufzeichnungsart (FM: >= 500k -> 0, sonst 2; MFM: >= 500k -> 3, sonst 5; 300k nur auf Wunsch); n = Zahl der Sektoren mit ID; Groessencode = Code des ersten Sektors (bei gemischten Groessen: Fehler).
- Kopf-Byte: Kopf; Bit 7 setzen, wenn *irgendein* Sektor ein C ungleich dem physischen Zylinder hat; Bit 6, wenn irgendein H ungleich dem physischen Kopf. Dann die jeweilige Map vollstaendig schreiben.
- Je Sektor: ohne Datenfeld -> Typ 0. Sonst Typ = 1 + (komprimiert?1:0) + (geloescht?2:0) + (CRC-Fehler?4:0). "Komprimiert", wenn alle Datenbytes gleich dem ersten sind -> nur dieses Byte. Wirkt auch bei Spur gefuellt mit 0xE5.
- Spur ohne jede Sektoren: Record mit n = 0 (HxC schreibt ihn mit Modus 2 bzw. 5, Groessencode 0).

### 1.8 Umrechnung in Bitzellen

- Aus Modus: FM oder MFM, Zelltakt. Typische Lage: Index – Lueckenbytes – je Sektor (Sync, IDAM, C H R N, CRC, Luecke 2, Sync, DAM, Daten, CRC, Luecke 3) – Fuellung bis Spurende.
- Luecken stehen nicht in der Datei; Implementierung muss Lueckenlaengen selbst waehlen (HxC: Luecke 3 = 255 gesetzt, "kein Sektor unter dem Index"; Interleave steckt allein in der Nummernfolge). Spurlaenge soll Zellenzahl pro Umdrehung bei der gewaehlten Drehzahl nicht ueberschreiten; wenn Sektoren nicht passen, Luecke 3 verkleinern.
- N im ID-Feld = Groessencode. ID-C/H aus Maps oder physisch. DAM FB bzw. F8 nach Typ; bei Typ 0 gar kein Datenfeld (nicht ein leeres).
- Typ >= 5: Daten-CRC absichtlich falsch (z. B. invertiert) schreiben.

### 1.9 Handgerechnete Testvektoren (Kopfzeile 40 Byte: `IMD 1.17: 22/10/2026 12:00:00\r\nVektor\r\n\x1a`)

**Vektor A** – MFM 250, Zyl 0 Kopf 0, 2 Sektoren à 128 B, Nummern 1,2; Sektor 1 komprimiert (E5), Sektor 2 normal 00..7F.
```
0028  05 00 00 02 00        Modus 5, Zyl 0, Kopf 0, n=2, s=0
002d  01 02                 Nummernfolge
002f  02 E5                 Sektor 1: Typ 2, Fuellbyte E5
0031  01 00 01 02 ... 7F    Sektor 2: Typ 1 + 128 Byte 00..7F
```
(Offset 0x28 = Ende der Kopfzeile, nach der Nachrechnung 40 Byte: 10 + 19 + 2 + 6 + 2 + 1; Dateilaenge 0xB2.) Erwartet: Spur (0,0) MFM, 2 Sektoren mit ID (0,0,1,0),(0,0,2,0), Daten 128×E5 bzw. 00..7F, beide DAM FB, CRC gut.

**Vektor B** – FM 250, Zylinder 3, Kopf 1 mit beiden Maps, 3 Sektoren.
```
02 03 C1 03 00        Modus 2, Zyl 3, Kopf-Byte C1 (Bit7+Bit6, Kopf 1), n=3, s=0
09 08 07              Nummernfolge
04 03 03              Zylinder-Map
00 01 00              Kopf-Map
04 00                 Sektor R=9: Typ 4 (geloescht, komprimiert), Byte 00
05 AA*128             Sektor R=8: Typ 5 (Datenfehler), 128 x AA
00                    Sektor R=7: Typ 0, keine Daten
```
Erwartet: IDs (C,H,R,N) = (4,0,9,0) geloeschte Marke F8 Daten 128×00; (3,1,8,0) FB, Daten AA, Daten-CRC falsch; (3,0,7,0) ohne Datenfeld.

**Vektor C** – leere Spur + Folgetrack.
```
03 01 00 00 02        Modus 3 (MFM 500), Zyl 1 Kopf 0, n=0, s=2 -> Record nur 5 Byte
03 02 00 01 02        Modus 3, Zyl 2, Kopf 0, n=1, s=2
01                    Nummernfolge: R=1
02 5A                 Typ 2, Fuellbyte 5A (512 x 5A)
```
Erwartet: Spur (1,0) leer; Spur (2,0) MFM, 1 Sektor R=1 N=2, 512×5A. (Mit greaseweazle gegengeprueft: Vektoren A und B und C liefern die oben genannten Sektoren.)

---

## Teil 2: TeleDisk (.td0)

Alle Mehrbytewerte little endian. CRC16 ueberall: Polynom 0xA097 (MSB-first, ohne Spiegelung), Startwert 0, kein Endxor, Byte wird oben (`crc ^= byte << 8`) eingemischt. **[K]** (alle 47 Kopf-CRCs, alle Kommentar-CRCs, alle 4 Spur-CRCs stimmen). Selbsttest: CRC("123456789") = 0x0FB3.

### 2.1 Gesamtstruktur

1. Dateikopf, 12 Byte (nie komprimiert).
2. Bei Signatur `td`: ab Byte 12 alles in einem Strom LZSS+adaptives Huffman komprimiert (Abschnitt 2.7). Bei `TD` unkomprimiert.
3. Optionaler Kommentarblock.
4. Spur-Records bis Endemarke.

### 2.2 Dateikopf (12 Byte)

| Off | Groesse | Bedeutung |
|---|---|---|
| 0 | 2 | `TD` (normal) oder `td` (Advanced Compression) |
| 2 | 1 | Volume-Folgenummer (0 = erste) [K: immer 0] |
| 3 | 1 | Pruefwert der Mehrdisketten-Folge; alle Teile gleich [K: beliebige Werte, z. B. 0x1F, 0x4B] |
| 4 | 1 | Teledisk-Version, BCD-artig (0x15 = 1.5; [K] alle Dateien 0x15); sinnvoll akzeptieren 0x0A..0x15 (HxC: 10..21) |
| 5 | 1 | Datenrate: Bit 0..6: 0 = 250, 1 = 300, 2 = 500 kbit/s; Bit 7 = FM (global) [K: 0x00, 0x02, 0x82] |
| 6 | 1 | Laufwerkstyp: 1=360K, 2=1,2M, 3=720K, 4=1,44M, 5=8", 6=3,5" [K: 2 und 5 kommen vor]; informativ |
| 7 | 1 | Schrittweite: Bit 0..1: 0 einfach, 1 doppelt, 2 "gerade Spuren" (96tpi in 48tpi); **Bit 7 = Kommentarblock folgt** [K: Werte 0,1,0x80] |
| 8 | 1 | DOS-Allocation-Flag; != 0: Diskette wurde nur anhand der FAT gelesen, nichtbelegte Sektoren uebersprungen (Flag 0x10 bei Sektoren) [K: immer 0] |
| 9 | 1 | Seitenzahl: 1 = eine Seite, jeder andere Wert = zwei [K: 1 und 2] |
| 10 | 2 | CRC16 ueber Byte 0..9 [K] |

Hinweis: HxC benennt Byte 7 falsch als "Track-Dichte"; die Bedeutung (Stepping, Bit 7 = Kommentar) ist am Korpus gesichert: Dateien mit 0x80 haben Kommentarblock, deren CRC stimmt. **[K]**

### 2.3 Kommentarblock (nur wenn Kopf Byte 7 Bit 7)

Header 10 Byte: CRC16 (2), Datenlaenge `L` (2), Jahr-1900, Monat (0-basiert!), Tag, Stunde, Minute, Sekunde (je 1). Danach `L` Byte Kommentartext (NUL-terminierte Zeilen; NUL = Zeilenende; mehrere NUL am Ende moeglich). CRC deckt Byte 2..9+L (Rest des Headers ab Laengenfeld plus Text). **[K]** (z. B. scp07.td0: "Erzeugt mit JKCEMU", 28.12.2016 10:07:17 (Monat-Byte 11, 0-basiert); D02V032: Komma-/Mehrzeilentext mit trailing NULs, 11.11.2007).

### 2.4 Spur-Record

4 Byte: Sektoranzahl `n`, Zylinder, Kopf-Byte, CRC8. **Kopf-Byte**: Bit 0 = Seite (Bits 0..6 > 0 werden von HxC als Seite 1 gewertet, real nur 0/1), **Bit 7 = Spur ist FM** (Einzeldichte). Spur ist FM, wenn Bit 7 hier ODER Bit 7 im Dateikopf Byte 5 gesetzt. **[K]** (Dateien mit Kopf-Bit 7 und Datenrate 0x82 kommen vor; "FM-Bit nur an einzelnen Spuren" kam im Korpus nicht vor [Q]: CMD.td0 usw. haben global+je Spur gesetzt, eine DISK.TD0 hat einen Kopf 129 auf Spur 0.) CRC8 = niederwertiges Byte von CRC16 ueber die ersten 3 Byte. **[K]** (alle Spuren aller Dateien).
FM-Spur bei Datenrate-Feld r: Zelltakt r (250/300/500 kbit/s), Datenrate halb; fuer Umrechnung also gleiche Rate wie im Kopf, FM-Kodierung.
Zylinderzahl = groesster Zylinder + 1. Spuren erscheinen im Korpus immer nach (Zylinder, Seite) aufsteigend sortiert und je einmal. **[K]** n = 0 kommt vor (11 Spuren), d. h. Spur ohne Sektoren, Record bleibt 4 Byte. **[K]**

### 2.5 Sektor-Record

Sektorkopf 6 Byte: C (ID-Zylinder), H (ID-Kopf), R (ID-Sektornummer), N (Groessencode, Groesse = 128 << N; Werte 0..6; in der Praxis auch grosse Werte als "ohne Daten"), Flags, CRC8.

Flags (ODER-verknuepft):

| Bit | Bedeutung | Befund |
|---|---|---|
| 0x01 | Sektor im Track doppelt vorhanden | in 16 Spuren doppelte Nummern, Flag aber nie gesetzt [K]; nicht verlassen |
| 0x02 | Datenfeld mit CRC-Fehler gelesen | [K] kommt vor, Daten sind trotzdem vorhanden |
| 0x04 | Datenmarke geloescht (F8) | [Q] |
| 0x10 | Daten wegen DOS-Allocation uebersprungen | **kein Datenblock folgt** [Q] |
| 0x20 | ID vorhanden, aber kein Datenfeld | **kein Datenblock folgt** [Q] |
| 0x40 | Daten ohne ID-Feld ("Pseudo-Sektor") | [K] Flag 0x40 kommt mit R >= 100 (105, 108, 115, 121, 125, 132) und N=0 vor; Datenblock folgt normal |

Ein Datenblock folgt genau dann, wenn `(Flags & 0x30) == 0`. (HxC liest zusaetzlich nur, wenn N <= 7, sonst fehlt der Block; Teledisk selbst erzeugt N > 6 nicht. Empfehlung: N > 6 = Fehler.)
Bei fehlendem Block: Sektordaten = nichts (HxC fuellt mit Nullen, greaseweazle ebenso). Empfohlen: ID ohne Datenfeld (Typ-0-Analogon).
Pseudosektoren (0x40, R >= 100): sind keine echten ID-Felder; fuer eine Bitzellen-Abbildung zu ignorieren oder getrennt zu behandeln. Mit Flag 0x40 gekennzeichnete Saetze gibt es im Korpus nur auf Spuren des 1,2M-Sektors N=0. **[K]**

**CRC8 des Sektors**: niederwertiges Byte von CRC16 ueber die **entpackten Sektordaten** (die volle Sektorgroesse), *nicht* ueber Kopf und Datenkopf, wie Dunfields Notiz behauptet. **[K]** 68 298 von 68 298 Sektoren stimmen mit dieser Regel (auch mit Flag 0x02). Bei Sektoren ohne Datenblock bleibt der Wert unbestimmt (nie geprueft).
C/H aus dem Sektorkopf sind die ID-Werte. **[K]** Sie weichen vor: 560 Sektoren mit C != Spurzylinder (DISK_CMD.TD0, Spuren 81/82 tragen C=89/90, N=4 = 2048 Byte als Pruefspuren), H != Seite kam nicht vor. Daher ID-Werte immer aus dem Sektorkopf nehmen, nie aus dem Spurkopf.

### 2.6 Sektordatenblock

Nur wenn Flags & 0x30 == 0:
- Laenge `d` (2 Byte) = Laenge des Rests einschliesslich Methodenbyte, also `d - 1` Nutzbyte.
- Methodenbyte `m`.
- `d - 1` Byte Nutzdaten.

Methoden **[K]** (35 304 × m0, 13 060 × m1, 19 934 × m2; jede entpackte Laenge = Sektorgroesse):
- **0**: Rohdaten, Sektorgroesse Byte.
- **1**: wiederholt: Paare (Anzahl 16 Bit, Muster 2 Byte); Sektor = Muster × Anzahl, mehrere Paare bis Sektorgroesse erreicht. [K]: immer genau ein Paar (z. B. Sektor 128 B: `40 00 E5 E5` = 0x40=64 Wiederholungen von E5E5).
- **2**: RLE-Folge: Wiederhole bis `d-1` Byte verbraucht: Byte `c`, Byte `n`. Ist `c == 0`: Literalblock, die folgenden `n` Byte unveraendert uebernehmen (n = 0 erlaubt: [K] 2 376 Fälle, haeufig als Abschlusseintrag `00 00`). Ist `c != 0`: Muster aus den naechsten **`2*c` Byte** (Musterlaenge laut Dunfield und greaseweazle), `n`-mal wiederholt.
  - **Offener Punkt zur Musterlaenge**: HxC rechnet `1 << c` Byte statt `2*c`. Im gesamten Korpus kommt ausschliesslich **c = 1** vor (66 681 Faelle); dort sind beide Formeln gleich (2). Der Korpus entscheidet die Frage also **nicht**. Empfehlung: `2*c` (Quelle: Dunfield, TD0-Behandlung in greaseweazle), aber c > 1 ablehnen oder als untypisch melden. **[Q]**
- Andere Methode: Fehler.
Die entpackte Laenge muss exakt der Sektorgroesse entsprechen, sonst Fehler (greaseweazle: harte Pruefung).

### 2.7 Advanced Compression ("td")

Die Daten ab Byte 12 sind ein LZHUF-Strom (Okumura-LZSS mit adaptivem Huffman). Der Dateikopf wird 1:1 uebernommen. **[K]** Mit der folgenden Beschreibung entpackt D02V032.TD0 (309 904 B -> 464 430 B hinter dem Dateikopf, alle Spur-, Sektor- und Kommentar-CRCs stimmen, 15 Neuaufbauten des Baums).

Parameter:
- Ringpuffer 4096 Byte, Anfangsfuellung 0x20 (Leerzeichen); Schreibposition beginnt bei 4096 − 60 = 4036.
- Look-ahead 60, Schwelle 2 (Mindestlaenge 3).
- Alphabet: 314 Symbole. 0..255 = Literalbyte; 256..313 = Trefferlaenge (Symbol − 253, d. h. 3..60).
- Baum: 627 Knoten (314 Blaetter + 313 innere), Wurzel = Knoten 626.

Huffman-Initialisierung: Alle Blaetter Haeufigkeit 1. Innere Knoten werden der Reihe nach aus je zwei aufeinanderfolgenden Knoten (0/1, 2/3, … dann weiter ueber die erzeugten) gebildet, Haeufigkeit = Summe, bis die Wurzel (Index 626) entsteht. Anfangscodes (zur Probe, linkes Kind = Bit 0): Literal 0x00 = 1100011000, 0x20 = 1101011000, 0x41 = 1110011010, 0xE5 = 011100010, 0xFF = 100010110, Symbol 256 = 100011000, 300 = 101110000, 313 = 110001010. **[K]**

Decodieren eines Symbols: ab Wurzel bitweise absteigen (Bit 0 = linkes Kind, 1 = rechtes), bis ein Blatt erreicht ist. Bits werden MSB-first aus den Bytes genommen. Danach Haeufigkeit des Blatts aktualisieren (adaptiver Baum).

Aktualisierung: Zuvor, falls Haeufigkeit der Wurzel = 0x8000 erreicht hat: Neuaufbau – Blatthaeufigkeiten halbieren (aufgerundet: (f+1)/2), Blaetter dicht auf die unteren Plaetze legen, die inneren Knoten neu aufbauen, indem je zwei Nachbarn summiert und der neue Knoten sortiert (aufsteigende Haeufigkeit) eingefuegt wird; Eltern-Verweise neu setzen. Dann Haeufigkeit des Blatts um 1 erhoehen und nach oben bis zur Wurzel: erhoehe Zaehler des Knotens; ist er nun groesser als der des rechten Nachbarn (naechsthoeherer Index), tausche den Knoten mit dem weitesten Nachbarn, dessen Zaehler noch kleiner/gleich ist, und gehe weiter zum Elternknoten des getauschten Platzes. (Standardalgorithmus von Okumura; ueber 15 Neuaufbauten im Korpus bestaetigt.) Sentinel: Zaehler hinter der Wurzel = 0xFFFF.

Ist das Symbol < 256: ausgeben und in den Ringpuffer an Schreibposition legen (Position + 1 mod 4096).
Sonst Treffer: Laenge = Symbol − 253. Danach Position: 6 obere Bit ueber einen Praefixcode, 6 untere Bit roh (MSB-first). Distanz `p` = (obere << 6) | untere; Quelle beginnt bei (Schreibposition − p − 1) mod 4096 und wird Byte fuer Byte kopiert (ueberlappend erlaubt, jedes Byte wird sofort in den Ring zurueckgeschrieben).

Praefixcode fuer die oberen 6 Bit (Werte 0..63), kanonisch aufsteigend zugeteilt:

| Werte | Codelaenge | Anzahl |
|---|---|---|
| 0 | 3 | 1 |
| 1..3 | 4 | 3 |
| 4..11 | 5 | 8 |
| 12..23 | 6 | 12 |
| 24..47 | 7 | 24 |
| 48..63 | 8 | 16 |

Kanonisch: innerhalb jeder Laenge fortlaufend aufsteigend; beim Wechsel zur naechsten Laenge wird der naechste Code um ein Bit nach links geschoben. Erste Codes: 0 -> 000; 1 -> 0010; 2 -> 0011; 3 -> 0100; 4 -> 01010; 5 -> 01011; 6 -> 01100; 7 -> 01101. Ich habe diese Tabelle gegen die 256-Eintrag-Tabelle von HxC (Beobachtung) Byte fuer Byte verglichen: identisch. **[K]**

Ende: Das Stromende ist nicht markiert. Entpacke, bis die Eingabe erschoepft ist (das letzte Byte kann halb leer sein); die entpackte Laenge ist daher um wenige Padding-Bytes groesser als noetig (Korpus: nach dem Endesatz 5 Byte statt 3, bei `TD` 3). Das Format wird beendet durch den Endesatz im Strom (2.8); nachfolgende Bytes sind zu ignorieren. **[K]**

### 2.8 Endemarke

Ein Spur-Record, dessen erstes Byte 0xFF ist, beendet die Liste. Danach folgen in unkomprimierten Dateien im Korpus immer noch 3 weitere Byte (also ein 4-Byte-Record, `FF xx yy zz`; z. B. `FF 4F 01 F1`, `FF 27 00 EE`); sie sind **keine** CRC-gesicherten Nutzdaten (CRC passt nur in 11 von 46 Faellen zu den zwei mittleren Bytes; nicht pruefen, nicht auswerten). Beobachtet: Byte 1 = Zylinder der letzten Spur, Byte 2 = Kopf der letzten Spur (in allen 46 Faellen). **[K]**

### 2.9 Weitere Befunde und Besonderheiten

- **Doppelte Sektoren** (gleiche R in einer Spur) kommen vor (16 Spuren; Kopierschutz/Teledisk-Mehrfachlesung). Alle Saetze erhalten, Reihenfolge = Reihenfolge der Spur. **[K]**
- Gemischte Sektorgroessen innerhalb einer Spur: im Korpus nie. **[K]** (Dunfield: kommt vor; nicht darstellbar bei NEC-765.)
- Spur-Kopf (Zylinder, Seite) vs. Sektor-ID: ID-Werte aus Sektorkopf; Spurkopf bezeichnet die physische Position. **[K]**
- Nicht-Nullseiten: Bei Seitenzahl 1 gibt es nur Seite 0; [K] eine zweiseitige Datei mit vollstaendig Seite-0-Spuren kam nicht vor.
- Dichte: Kopf Byte 5 Bit 7 = global FM, Spur Bit 7 = FM je Spur. Rate = Kopf Bit 0..6. Kodierung je Spur = FM wenn irgendein Bit gesetzt, sonst MFM. **[K]** (CMD.td0: beide gesetzt, Rate 0x82.)
- Nicht-HD-Dichte-Kombination wie "300 kbit/s": im Korpus nicht vorhanden. [Q]
- DOS-Allocation-Flag: Teledisk laesst Sektoren weg (Flags 0x10, kein Datenblock). Im Korpus nicht vorhanden; Verhalten nur aus Quelle. [Q]
- Abgeschnittene Dateien: Korpus: 0-Byte-Datei (SCP_50.TD0) und 20-Byte-Datei (Kopf + Spur `00 00 01 97` + Endesatz); beide zeigen, dass der Leser mit Kopf-nur-Dateien und fehlendem Rest umgehen muss. Jede Bereichsueberschreitung beim Lesen = Fehler "Datei unvollstaendig" (HxC bricht ab, greaseweazle wirft Ausnahme). Vor dem Lesen Dateilaenge >= 12 pruefen.
- Pruefsummenfehler: HxC ignoriert Spur- und Sektor-CRC (loggt nur / prueft gar nicht), greaseweazle bricht ab. Empfehlung: CRC-Fehler melden, nicht abbrechen, nur Kopf-CRC (Dateikennung) als harte Pruefung.
- Kopf-Version: akzeptieren 0x0A..0x15; 0x15 ist die einzige im Korpus.

---

## Teil 3: Fallen

1. **Sektor-CRC deckt nur die entpackten Daten** (Dunfields Text sagt anderes). [K]
2. **Methode 2: Musterlaenge** `2*c` vs `1<<c` – Korpus nutzt nur c=1; beide Formeln fallen dort zusammen. [Q] Nicht pauschal "gefixt" annehmen.
3. **Datenblock fehlt bei Flags & 0x30**, nicht bei jedem Fehlerflag; Flag 0x02 (CRC-Fehler) hat Daten, 0x40 (Pseudosektor) hat Daten.
4. **FM-Kennung doppelt**: Dateikopf Byte 5 Bit 7 und Spur-Kopf Bit 7; ODER. Spur-Kopf vor dem Maskieren auswerten, sonst ist die Seite 128/129.
5. **Monat im Kommentar 0-basiert**, Jahr ab 1900; Kommentarblock-CRC startet erst ab Byte 2 des Headers.
6. **Endemarke ist ein 4-Byte-Record**; Inhalt hinter dem 0xFF nicht auswerten. Bei `td` sind es 5 Bytes Ueberhang durch Strompadding.
7. **LZHUF**: Ringpufferanfang mit Leerzeichen, Schreibposition 4036 (nicht 0); Distanz ist "−1" (Distanz 0 = letztes Byte); Treffer ueberlappen sich selbst (Byte fuer Byte kopieren, nicht memcpy); Neuaufbau bei Wurzelzaehler exakt 0x8000.
8. **Entpackter Strom endet ohne Markierung**; Dekoder darf am Eingabeende nicht in Endlosschleife/Mull laufen – bei erschoepfter Eingabe sofort abbrechen.
9. **Mehrfache Sektornummern, Nummern >= 100, Pseudosektoren** nicht als Fehler behandeln; Nummernfolge nicht annehmen sortiert/lueckenlos.
10. **Sektorgroesse > 6 oder mit entpackter Laenge != Soll**: Fehler/abweisen.
11. **IMD**: Kommentar darf alles ausser 0x1A enthalten; Zeilenenden CR LF.
12. **IMD**: Kopf-Byte mit 0x40/0x80: Maps stehen *nach* der Nummernfolge und zwar zuerst Zylinder-, dann Kopf-Map – Reihenfolge nicht vertauschen.
13. **IMD**: Typ 0 hat kein Folgebyte; Typ 2/4/6/8 genau ein Byte; die Typen 1/3/5/7 immer volle Sektorgroesse.
14. **IMD**: n = 0 ist gueltig und der Record ist dann nur 5 Byte lang.
15. **IMD Modus 1/4 (300 kbit/s)** und Drehzahl 360 bei 26-Sektor-8"-Spuren sind Konvention, nicht in der Datei.
16. **IMD aus TD0**: Dunfields td02imd bildet Flags 0x02/0x04 auf Typ 5-8 ab; Pseudosektoren (0x40) und Sektoren ohne Daten (0x10/0x20) fallen als Typ 0 weg.

---

## Teil 4: Testvektoren TD0 (alle aus dem Korpus, mit Pruefskript nachgerechnet)

Pfadkuerzel: `R=~/projects/robotron`, `A=~/projects/A5120.16/docs/tiffe/MUTOS`.

**Dateikopf, normal (TD)**: `R/PC1715/discs/scp07.td0`: Kopf `54 44 00 xx 15 00 02 80 00 02 ..` -> Sig TD, Version 0x15, Rate 250 (r=0), Laufwerk 2, Stepping 0x80 (Kommentar), DOS=0, Seiten 2, 160 Spuren, 2560 Sektoren, Kommentar "Erzeugt mit JKCEMU" (28.12.2016 10:07:17 (Monat-Byte 11, 0-basiert), CRC ok).

**Dateikopf, Advanced (td)**: `R/cm1910/discs/D02V032.TD0`: `74 64 00 4B 15 00 02 80 00 02 F0 52`, d. h. Sig `td`, Pruefwert 0x4B, Version 0x15, Rate 0, Laufwerk 2, Stepping 0x80, Seiten 2, CRC 0x52F0 = CRC16 ueber die ersten 10 Byte.
Erste gepackte Bytes (ab Offset 12): `77 6B 3A C6 7D F2 B9 7C 76 DB 7F D1 E7 EB 37 BC ...`. Entpackt (Strombeginn hinter dem Dateikopf): `EB DF AE 00 6B 09 0B 02 2A 33 45 43 20 31 38 33 34 20 20 44 43 50 ...` = Kommentar-CRC 0xDFEB, Laenge 0x00AE, Datum Jahr 107 = 2007, Monat-Byte 9 (0-basiert = Oktober; der Kommentartext nennt 11.11.2007, beides unabhaengig), Tag 11, 02:42:51, Text "EC 1834  DCP Version  3.20, Stand: 17.08.88". Die ersten 14 Literalsymbole stehen im Anfangsbaum mit den Bitfolgen `01110111 01101011 00111010 110001100 111110111 110010101 110010111 110001110 110110110 110111111 111010001 111001111 110101100 110111101` (die Codes ändern sich schon nach dem ersten Symbol, weil der Baum adaptiv ist). Die ersten Trefferbefehle im Strom: (Laenge 3, Distanz−1 = 18, obere 6 Bit = 0), (3, 90, 1), (7, 111, 1). Gesamt: 464 430 Byte entpackt, 160 Spuren, 15 Baumneuaufbauten.
Erster Spur-Record entpackt: `09 00 00 34`, danach Sektorkoepfe: `00 00 01 02 00 0C | EB ..` (hier Spur 0, 9 Sektoren).

**Methode 0**: `m8k2_root.td0` Spur Zyl 1 Kopf-Byte 0x80 (FM, Seite 0), Sektor ID (1,0,9), N=0, Flags 0, CRC8 0x71, Datenblock: Methode 0, 128 B: `00 0F 00 00 00 FA 00 1F 00 00 00 64 00 00 00 61 ...`.

**Methode 1**: dieselbe Datei, Zyl 0, Kopf-Byte 0x80, Sektor ID (0,0,1), N=0, CRC8 0xD1, Datenblock `Laenge 05 00, Methode 01`, Nutzdaten `40 00 E5 E5` -> 64 × (E5 E5) = 128 Byte E5.

**Methode 2 (nur Wiederholung)**: `R/PC1715/discs/scp07.td0` Zyl 1 Kopf 0, ID (1,0,6), N=1 (256 B), CRC8 0x76, Nutzdaten `01 80 E5 E5 00 00`: c=1, n=0x80, Muster E5E5 -> 256 × E5; Abschluss `00 00` (Literal der Laenge 0).

**Methode 2 (gemischt)**: `m8k2_root.td0` Zyl 0, ID (0,0,5), N=0, CRC8 0xD2, Nutzdaten (18 Byte) `00 05 C5 D9 D4 C1 D7 | 01 25 40 40 | 00 01 40 | 01 18 00 00`: Literal 5 Byte `C5 D9 D4 C1 D7`, dann 37 × `40 40`, dann Literal `40`, dann 24 × `00 00`. Entpackt 5 + 74 + 1 + 48 = 128 Byte.

**Endesatz**: scp07.td0 endet mit `FF xx yy zz`; im Korpus ist Byte 1/2 der Zylinder/Kopf der letzten Spur.

**Kleinstdatei**: `R/sonst/BC25/Quellen/BC25/DISK___1.035/!IMAGE/DISK_1.TD0` (20 Byte): `54 44 00 1F 15 02 05 00 00 02 4D CB | 00 00 01 97 | FF 00 01 97` = Kopf (Rate 500, 8"), eine Spur mit 0 Sektoren (Zyl 0, Seite 1, CRC8 0x97), Endesatz.

**Duplikatsektoren / Pseudosektoren**: `m8k2_usr.td0`, Zyl 5 Kopf-Byte 0x80: 13 Sektoren, Nummern 4,11,15,16,21,22,22,23,23,24,24,25,26 (doppelte 22–24); Zyl 0: Sektor R=105, Flags 0x40.

**Sektor-IDs mit abweichendem Zylinder**: `A/DISK_CMD.TD0` Zyl 81 Seite 0 -> Sektor-ID C=89, N=4 (2048 B), Methode 1.

**LZHUF-Vektor**: oben D02V032 (Dateikopf + 12 gepackte Bytes -> 14 entpackte Bytes `EB DF AE 00 6B 09 0B 02 2A 33 45 43 20 31`). Der Dekodierlauf (Pruefskript `td0chk.py::td_unpack`) stimmt ueber den ganzen Strom, alle 160 Spur-CRCs.

---

## Teil 5: Offene/ungepruefte Punkte

- IMD: Groessencode > 6, Modi > 5, Verhalten bei doppelten Track-Records, Reihenfolge unsortierter Dateien [Q]; kein einziges `.imd` im Korpus.
- TD0: Musterlaenge Methode 2 fuer c > 1 [Q]; Flags 0x04, 0x10, 0x20, DOS-Allocation, Version != 0x15, multivolume, 300-kbit/s-Dateien und gemischte FM/MFM-Spuren im Korpus nicht vorhanden [Q].
- Der FM-Bit-Befund (Kopf-Bit 7 an *allen* Spuren der FM-Dateien) stuetzt die ODER-Regel, schliesst aber gemischte Dichte nicht aus.

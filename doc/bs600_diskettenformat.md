# BS600-Diskettenformat (PRG 600/700/710/710-1, VEB Numerik Karl-Marx-Stadt)

Entwurf 48, AP-B0 (2026-11-04).  Pendant zu `doc/udos_diskettenformat.md`.  Quellen: die beiden vorliegenden Disketten

- **A** = `~/projects/robotron/Diskettensammlung_von_DL/Diskettenarbeit/Disketten_Images_RT/BS600.hfe` (Kopf `SYL FOR BS620`, Datei `BS620`, Dez. 1988, PRG 710-1),
- **B** = `~/projects/robotron/PC1715/discs/real_discs/BS600_80Tracks.scp` (Kopf `BOOTSTRAP FOR BS600`, Datei `BS600`, März 1985; die `.img` und die SS0-`.scp` sind Auszüge davon, s. §1),

dazu der Programmcode der Datei `BS620` (`doc/bs600/bs620.mac`) und die Bootsektoren beider Disketten.  Die Webseite
robotrontechnik.de (`software/bs600.htm`) nennt „15 × 256 × 40, stets einseitig, max. 54 Dateien, Verzeichnis auf Spur 0, Dateien stets unfragmentiert“ — das trifft zu (§§2, 5).

Gewissheitsgrade: **[Disk]** an beiden Abzügen **und** (soweit genannt) am Programmcode belegt, **[Code]** nur aus dem Programmcode, nicht an einer Diskette beobachtbar,
**[Web]** Webseite, **[?]** Annahme/offen.  Zahlen in „TT/SS“ sind *Spur/Sektor*, Spur = Zylinder.

## 1. Die zweite Diskette: warum 76 288 Byte?  [Disk]

`BS600_40Tracks_SS0.img` (76 288 B) und `BS600_41Tracks_SS0.img` (80 128 B) sind **abgeschnitten**: 3328 B (Spur 0: 26 × 128) + 19 bzw. 20 Spuren zu 3840 B (15 × 256).  Die
`.scp`-Aufnahmen haben nur 40 bzw. 41 *Kopfpositionen*, und ein 80-Spur-Laufwerk macht an einer 40-Spur-Diskette je Zylinder **zwei** Schritte —
also wurden nur die Zylinder 0–19 bzw. 0–20 erfasst.  Vollständig ist **`BS600_80Tracks.scp`** (80 Positionen × 2 Köpfe): `gw convert --format=ibm.scan --tracks="c=0-39:h=0:step=2"` (Greaseweazle-Hostwerkzeug, nur
lesend) ergibt eine `.hfe` mit 40 Zylindern, die **byteweise** mit der 41-Spur-`.img` übereinstimmt (und die 40-Spur-`.img` ist deren Anfang).  Damit stehen von B alle 611 Sektoren zur Verfügung
(`gw` meldet 600 von 611 beim Dekodieren; das DiskTool liest alle 611 — Abweichung offen [?]).

Geometrie beider Disketten (gemessen mit `k1520disktool measure`): Zylinder 0–39, nur Kopf 0, alles MFM, **c0h0: 26 × 128 B** (IDs 1–26), **c1…c39: 15 × 256 B** (IDs 1–15), kein Interleave.
153 088 B insgesamt (3328 + 39 × 3840).

## 2. Aufbau der Spur 0 (26 Sektoren zu 128 B)  [Disk]

| Sektor | Inhalt |
|---|---|
| 1–3 | **Urlader** (Z80-Code mit Textkopf, §3) |
| 4, 5, 6 | EBCDIC-Leerzeichen (`40H`), Sektor 5 beginnt `C5 D9 D4 C1 D7` = EBCDIC „ERMAP“; letzte 48 Byte je Sektor 00H.  **Kein Programm liest diese Sektoren** (BS620 und BS600 nicht) — Rest einer IBM-3740-artigen Vorlage [?] |
| 7 | **Datenträgerkennsatz** „VOL1“ (§4) |
| 8–25 | **Verzeichnis**, 18 Sektoren × 3 Einträge zu je 42 B + 2 Restbytes = 54 Einträge (§5) |
| 26 | **Belegungstabelle** (§6) |

Dateien liegen ab **Spur 1** (Spur 0 trägt keine).  Spuren 1…39 haben 15 Sektoren zu 256 B; **Spur = Zylinder**, die laufende Sektornummer geht 1…15, danach beginnt der nächste Zylinder bei 1.

## 3. Urlader  [Disk]

Zwei Bauarten, je nach ROM des Rechners:

- **A (PRG 710-1), `18 0E "SYL FOR BS620 "`** (Bytes 2–9 `SYL FOR `, 10–15 Name): Textkopf ab Byte 2, `SY` bei Byte 2/3 verlangt der NKM-Lader des 1-KB-ROM (Merkposten `prg710.md`).  Der Name der Datei steht **im Kopf**
  (Bytes 10–15 `BS620 `, im Code `LD DE,040AH`) und wird beim Suchen im Verzeichnis verglichen.  Er benutzt die ROM-Vektoren 03F4/03FD (Sektoren lesen) und lädt die Datei an ihre Ladeadresse.
  Meldungen `OS NOT FOUND`, `RAM DEF.`, `DISK-FEHLER: FC<xx>` (letztere nur in B).
- **B (PRG 700 / 3-KB-ROM), `18 13 "BOOTSTRAP FOR"` + Name `BS600 ` (Bytes 2–14 Text, 15–20 Name, **kein** Leerzeichen dazwischen):** das ROM lädt ab Spur 0 **26 Sektoren nach 6000H** [Schluss aus dem Puffer 6380H = Sektor 8, nicht am ROM gesehen]; der Lader (6015) sucht den Namen (Bytes 15–20, im Code `LD DE,600FH`) in den
  19 Sektoren ab 6380H (= Sektor 8; je Sektor 3 Einträge, dann `INC HL ×2` für die zwei Restbytes), verlangt **Typ `P`** (`CP 50H` an Eintragsbyte 6), rechnet
  Sektorzahl = (Endspur − Startspur)·15 + (Endsektor − Startsektor) + 1, entnimmt Ladeadresse (ASCII-Hex Byte 20–23) und Startadresse (28–31), prüft RAM an der Ladeadresse (55H schreiben/lesen)
  und springt an die Startadresse.  Meldungen über die ROM-Vektoren 0BF1/0BFD (**3-KB-ROM-Tabelle**: `0BE8`/`0BF1` …).
  Der 1-KB-NKM-Lader des 710 (Prüfung `SY` bei 0402H) bootet diese Diskette **nicht** — sie gehört zum PRG 700 (oder einem 710 mit 3-KB-ROM) [Disk, aus Code + ROM-Listing];
  die Bezeichnung „BS610 = PRG710“ der Webseite [Web] ist damit für **B** nicht bestätigt.
  *Der Suchlauf des Laders umfasst 19 Sektoren (8–26) und liest dabei auch die Belegungstabelle als „Einträge“ — folgenlos, weil der Name dort nicht vorkommt.*

## 4. Datenträgerkennsatz, Sektor 7  [Disk]

Aufbau (128 B), beobachtet an A und B:

| Byte | A | B | Bedeutung |
|---|---|---|---|
| 0–3 | `E5 D6 D3 F1` | gleich | EBCDIC „VOL1“ |
| 4–9 | `BS600-` | `BS600 ` | Datenträgername, 6 Zeichen ASCII |
| 10 | `20` | `20` | Leerzeichen (IBM: „zugänglich“) |
| 11–13 | **`PRG`** | **`PRG`** | **Pflicht**: BS600 prüft `(280BH) = "PRG"`, sonst Fehler `KEINE PRG-DISK` (1CD3) [Code] |
| 14–19 | `010989` | `170890` | Datum der Initialisierung, `TTMMJJ` ASCII |
| 20–36 | `40…` | `40…` | EBCDIC-Leerzeichen |
| 37–39 | `SCH` | `RIE` | Kürzel des Besitzers (ASCII; entspricht „Signum“) [?] |
| 40–66 | `20…` | `20…` | ASCII-Leerzeichen (Rest der Besitzerfeldes), danach `40` EBCDIC |
| 70 | `D4` | `D4` | EBCDIC „M“ (IBM-3740-Kennzeichen) |
| 75 | `F1` | `F1` | EBCDIC „1“ |
| 76–78 | `40` | `40` | EBCDIC-Leerzeichen |
| 79 | `20` | `20` | |
| 80–127 | `00` | `00` | |

Der Satz hat also **IBM-3740-Anlehnung** (VOL1 + EBCDIC-Vorlage), die ASCII-Felder wurden hineingeschrieben.  Gebraucht werden nur „PRG“ (11–13) und vermutlich Name und Datum für die Anzeige [?].

## 5. Verzeichnis, Sektoren 8–25  [Disk]

18 Sektoren zu 128 B, je **drei Einträge zu 42 Byte** (Versatz 0, 42, 84) und **zwei Restbytes `20 20`** (Versatz 126–127).  Der Namenssuchlauf des Monitors (`1A1D`) prüft Sektor 8…25 und je Sektor die Versätze
`0`, `2AH`, `54H` (`CP 54H` beendet die Suche) [Code]; die Reihenfolge ist die der Sektoren — **nicht sortiert**.

Ein Eintrag (alle Zeichenfelder ASCII, rechts mit Leerzeichen aufgefüllt, Hexzahlen als Text in Großbuchstaben):

| Byte | Länge | Feld | Inhalt / Beispiel (BS620 in A) |
|---|---|---|---|
| 0–5 | 6 | Name | `BS620 ` (Großbuchstaben, Ziffern; kein Punkt, keine Erweiterung) |
| 6 | 1 | Typ | `P` (Programm); `L` wird vom Monitor als weiterer Typ akzeptiert (§7) |
| 7 | 1 | Anfangsspur | binär, `06` |
| 8 | 1 | Anfangssektor | binär 1…15, `0F` |
| 9 | 1 | Endspur | binär, `0D` (13) |
| 10 | 1 | Endsektor | binär, `0D` (13) |
| 11–16 | 6 | Datum | `TTMMJJ`, `201288` = 20.12.1988 |
| 17–19 | 3 | Signum | Kürzel des Bearbeiters: `KRU`, `HOF`, `RIE` |
| 20–23 | 4 | Anfangsadresse | ASCII-Hex, `6D00` (Ladeadresse) |
| 24–27 | 4 | Endadresse | ASCII-Hex, `D4FF` (letztes Byte, einschließlich) |
| 28–31 | 4 | Startadresse | ASCII-Hex, `6D5F`; **`0000` = keine** (Dateien ohne Programmstart: `SD1152`, `SD1156`, `SYSKDK`, `TEST`) |
| 32–41 | 10 | Rest | **immer 10 × `20`** in allen 53 Einträgen beider Disketten |

Die Feldnamen entsprechen den Eingabefeldern des Monitors (`1732`–`17D0`): `DATEI-NAME`, `TYP`, `ANF.-ADR.`, `ENDE-ADR.`, `START-ADR.`, `DATUM`, `SIGNUM`, `LAUFWERK`.

- **Länge** = Endadresse − Anfangsadresse + 1 Byte; **Sektorzahl** = ⌈Länge / 256⌉ = (Endspur − Anfangsspur)·15 + (Endsektor − Anfangssektor) + 1.  An allen 53 Einträgen beider Disketten erfüllt, ohne Ausnahme [Disk].
  Der **letzte Sektor** wird nur bis zur Endadresse genutzt; der Rest enthält **Speicherreste** (oft `55H` vom Speichertest, `FFH`, Daten fremder Programme), kein festes Füllbyte.
  Ein Leser muss ihn ignorieren; ein Schreiber darf ihn beliebig füllen.
- **Leerer Eintrag = 42 × `20`.**  Eine Löschmarke (`E5`, `00`, `FF` …) kommt **an keiner Stelle** vor; gelöschte Dateien hinterlassen nur Leerzeichen.  Die Meldung `N.GELOESCHT` im Monitor deutet auf einen
  Lösch-Dienst „nicht gelöscht“ hin (Fehlerpfad); den Löschvorgang selbst (Eintrag mit Blanks überschreiben + Belegungstabelle) habe ich nicht gefunden [?].
- **Eintragslage:** auf beiden Disketten liegen alle Einträge in den Plätzen **0 und 1** jedes Sektors, **Platz 2 ist überall leer** (A: 28 Einträge, B: 25 Einträge; in A hat jeder der 18 Sektoren mindestens einen).  Der Suchlauf prüft Platz 2;
  ob BS600 beim Neuanlegen ihn belegt, ist offen [?].  (Der Entwurf 48 §2.1 nennt „35 belegte Einträge“ — richtig sind **28** in A.)
- **Platzwahl für neue Einträge:** kein Schema erkennbar (Einträge derselben Datei liegen in A und B auf verschiedenen Plätzen, z. B. `SYSNLD` Sektor 11/Platz 1 in A, Sektor 9/Platz 0 in B).  Wahrscheinlich wird der **erste freie Platz** genommen; nach Löschen entstehen Lücken im Verzeichnis [?].

### Dateien beider Disketten

A (BS620, 28 Dateien) und B (BS600, 25 Dateien) haben **19 Dateien bytegleich** (`SYSVSP`, `SD1152`, `SD1156`, `SYSCDI`, `SYSCMP`, `SYSCNC`, `SYSCPY`, `SYSDDA`, `SYSDKM`, `SYSDMF`, `SYSDTB`, `SYSEKK`, `SYSLST`, `SYSMSH`,
`SYSNDK`, `SYSREN`, `SYSVDK`, `SYSAUS`, `SYSKDK`; bei `SYSAUS` und `SYSKDK` liegt derselbe Inhalt an anderer Stelle der Platte).  **Verschieden:** das Hauptprogramm (`BS620` ↔ `BS600`) sowie `SYSADK`, `SYSINT`, `SYSNLD`, `FS1201`.
Nur in A: `EPSON`, `LSTRS1`, `SYSCN6`, `SYSCNH`.  Nur in B: `TEST` (1 Sektor, Arbeitsdatei vom 17.08.1990).
Typische Ladeadressen: `2A00` (Dienstprogramme `SYS…`), `2C00` (Treiber `SD1152`/`SD1156`/`FS1201`/`EPSON`: Drucker), `3000`/`3100`/`3400` (größere Programme), `7000`–`AFFF` (`SYSCN6`/`SYSCNH`: Überlagerung 16 KB), `B400`/`BA00`/`BF00`/`C000` (Tabellen).

## 6. Belegungstabelle, Sektor 26  [Disk]

**Je Zylinder 1…39 zwei Byte** (Spur *n* bei Versatz 2·(n−1)), **Bit 7 des ersten Bytes = Sektor 1**, dann absteigend bis Bit 0 des zweiten Bytes (= „Sektor 16“, immer 0); **Bit gesetzt = belegt**.
Ein voll belegter Zylinder ist `FF FE`, ein unbelegter `00 00`.  Die Tabelle stimmt an **beiden** Disketten für alle 39 Zylinder mit den Eintragsbereichen des Verzeichnisses **exakt** überein (0 Abweichungen, 381 bzw. 248 belegte Sektoren).
Hinter dem Eintrag für Spur 39 stehen Nullen bis Byte 127.  Der Monitor liest den Sektor in den Puffer 2800H, ändert ihn mit `1BD0` (Setzen/Löschen eines Sektorbits) und schreibt ihn zurück [Code].

**Freispeicher = die 0-Bits** (A: 204 Sektoren = 52 224 B, B: 337 Sektoren = 86 272 B).  Es gibt keine Löcher: **beide Disketten sind lückenlos belegt** (Spur 1 bis zur letzten belegten Stelle), die Reorganisation ist also an beiden
schon gelaufen; Löcher kommen nach dem Löschen vor [Code: erster passender Lauf, s. u.].

## 7. Allokation, Typen, Reorganisation

- **Allokation** (`1AD4`) [Code]: Sucht den **ersten zusammenhängenden Lauf freier Sektoren** der benötigten Länge, beginnend bei Spur 1/Sektor 1, **über Spurgrenzen hinweg**
  (Zähler zurücksetzen bei jedem belegten Sektor).  Erreicht der Suchzähler Spur **38** (`CP 26H`), erscheint `DISK VOLL`: **benutzbar sind Spur 1…37** (555 Sektoren = 142 080 B), Spur 38 und 39 werden nie vergeben.
  An den Abzügen liegt nichts hinter Spur 26.  Folge: Dateien sind **stets unfragmentiert** (eine Datei = ein Lauf).
- **Typen** [Code]: der Monitor (`18E6`) akzeptiert **`P`** (immer) und **`L`** (nur im Betriebszustand `(0EB4) = 0`: Listen-/Logikdateien? [?]); andere Typen → Fehler `TYP`.  Alle 53 Einträge der Abzüge sind `P`.
  Der PRG-700-Urlader (B) lädt ausschließlich `P`.  Weitere Typbuchstaben (Daten/Text/Gleichungen) sind nicht belegt.
- **Dateien ändern:** BS600 belegt eine Datei neu (Allokation, Eintrag, Belegungstabelle); „Überschreiben in derselben Länge“ ist möglich (`DATEI AUF DISK`-Meldung bei vorhandenem Namen) [?].
- **Reorganisation:** nicht untersucht, welches `SYS…`-Programm sie ausführt (Kandidat `SYSVDK`/`SYSDKM`/`SYSKDK`, „Diskette komprimieren“) [?].  Nach der Webseite [Web] beseitigt sie Lücken; an den Abzügen ist das Ergebnis
  zu sehen: lückenlos, Dateien in Eintragsreihenfolge **nicht** aufsteigend (z. B. A: `SYSCN6` 16/05, `SYSADK` 14/03; Spurfolge ≠ Eintragsfolge).

## 8. Unterschiede A (BS620) ↔ B (BS600)

| | A BS620 | B BS600 |
|---|---|---|
| Urlader | `SYL FOR BS620` (710-1, NKM-Lader) | `BOOTSTRAP FOR BS600` (3-KB-ROM, 26 Sektoren nach 6000H) |
| Hauptdatei | `BS620`, 20.12.1988, `KRU` | `BS600`, 22.03.1985, `HOF` |
| Version `(AFFE)` | 04 | 02 |
| Tastatur | K7672 serielle (Ports 5EH/5FH, Escape-Folgen) | K7609 an 8279 (C8H/C9H, Matrix → Tabelle 02B3) |
| Prüfsumme (`2400`) | **stillgelegt** (`RET`), Tabelle veraltet | **aktiv**, alle 25 Blöcke stimmen |
| Selbsttest 6D00–6FB7, D400–D493 | | **bytegleich** |
| Kennsatz | `BS600- PRG010989`, Besitzer `SCH` | `BS600  PRG170890`, Besitzer `RIE` |
| Dateien | 28 (u. a. `SYSCN6`, `SYSCNH`, `EPSON`, `LSTRS1`) | 25 (u. a. `TEST`) |
| belegt | 381 Sektoren | 248 Sektoren |
| Verzeichnisplätze 0/1 | beide; Platz 2 leer | beide; Platz 2 leer |

Das Hauptprogramm der beiden Disketten stimmt in 1767 von 26 624 Byte nicht überein (Tastatur 0030–0048/0115–0132/0201–028C, Floppy-Treiber 0447–0550/12FF–170C, Peripherie-Tabellen 74A8, Sprungleiste AF1D, Prüfsummentabelle AEC0 und
`9000–90DB`); der Rest ist gleich.

## 9. Folgerungen für das DiskTool (AP-B3…B5)

1. **Erkennung über den Inhalt:** `VOL1` (E5 D6 D3 F1) in Sektor 7 **und** `PRG` ab Byte 11 **und** ein Kopf `SYL FOR …`/`BOOTSTRAP FOR …` in Sektor 1; Geometrie 26 × 128 + 39 × 15 × 256 (SectorSpace nimmt gemischte Größen auf).
2. **Verzeichnis:** 54 Plätze (3 je Sektor 8–25), leerer Platz = 42 × `20`; Name 6 Zeichen; nur Typ `P` laden (Urlader), `L` lesbar; Ladelänge aus ASCII-Hex; **Rest[10] unverändert zurückschreiben**, bei Neuanlage Blanks.
3. **Belegung:** Tabelle Sektor 26 als Quelle der Wahrheit für Freispeicher, **gegen die Eintragsbereiche prüfen** (so passt es an beiden Abzügen); Allokation = erster Lauf ab Spur 1, Spur 1–37.
4. **fsck-Kennungen (Vorschlag):** `bs600.kennsatz.fehlt` (VOL1/PRG), `bs600.eintrag.laenge` (Sektorzahl ≠ ⌈Länge/256⌉), `bs600.eintrag.ueberlappt` (Doppelbelegung), `bs600.bitmap.abweichung` (Tabelle ≠ Einträge), `bs600.eintrag.spurende`
   (Ende hinter Spur 39), `bs600.eintrag.typ`, `bs600.datum.unplausibel`.  Reparatur: Tabelle aus den Einträgen neu rechnen (kein Datenverlust).
5. **Bootfähig neu anlegen:** Sektoren 1–3 aus einem Muster (Kopf + Name `BS620 ` bzw. `BS600 `), Kennsatz mit `PRG`, Verzeichnis (Blanks), Belegungstabelle (Nullen), Dateien ab Spur 1; Urlader-Vorlage A bzw. B je nach Zielrechner
   (Vorlagen: Sektoren 1–3 der beiden Abzüge).
6. **Keine** Annahme über die Sektoren 4–6; beim Schreiben unverändert lassen.
7. Löschen (Eintrag auf Blanks, Belegungsbits löschen) und Reorganisieren (Dateien in Eintragsreihenfolge lückenlos ab Spur 1 neu legen, Tabelle neu) sind **abgeleitet**, nicht an Originalablauf bestätigt [?].

## 10. Offene Punkte

- Löschvorgang und Reorganisation im Programmcode noch nicht aufgesucht (nur Folgen an den Abzügen gesehen).
- Typ `L`: Bedeutung und Dateien; weitere Typen.
- Platz 2 im Verzeichnis: wird er je belegt?  (Vorschlag: DiskTool darf ihn belegen — der Suchlauf des Monitors findet ihn.)
- Rolle der Sektoren 4–6 und der EBCDIC-Felder des Kennsatzes über „PRG“ hinaus.
- Zwei Sektoren-Differenz im `gw`-Dekodieren (600/611) gegenüber dem DiskTool (611).

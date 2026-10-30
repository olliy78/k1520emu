# SIOS-1526-Diskettenformat (A5120/A5130/K8924)

**Stand:** 2026-10-29 · Arbeitspaket D1 (Teil 1), Entwurf `doc/design/43_sios1526.md` · Zweig `SIOS1526`

Dieses Dokument beschreibt das Diskettenformat des Betriebssystems **SIOS 1526** so weit, wie es sich aus den
fünf Originalabbildern und dem Bedienhandbuch belegen lässt — als Grundlage für ein DiskTool-Dateisystem
(`SiosFileSystem`, D2–D6).  **Der Systemcode (SIOS-Kern, LBSV, FGEN, INIT) ist hier noch nicht ausgewertet**;
alles, was nur dort sicher zu klären ist, steht in §11 „Offen".

Markierungen: **[SRC]** am Abbild gemessen (Dateiname/Sektor angegeben) · **[BH S. n]** Bedienhandbuch,
PDF-Seite n (`doc/original_docs/SIOS1526_Bedienhandbuch_A5120_A5130_ocr.pdf`) · **[?]** Vermutung / unbelegt.

Datenbasis (`tools/sios_a5120/original/`, Beschreibung dort im `README.md`):

| Kürzel | Abbild | Datenträgername | Dateien (HDR1) |
|--------|--------|-----------------|----------------|
| SYLs | `SIOS1526_SYLs.hfe` | `ROBINI` | `SYSARE11` (Spur 1–12) |
| SYLp | `SIOS1526_SYLp.hfe` | `ROBINI` | `SYSARE09` (Spur 1–10) |
| DP1 | `SIOS1526_DP1.hfe` | `MWD051` | `PHASELB` (Spur 1–37) |
| DP2 | `SIOS1526_DP2.hfe` | `MWD051` | `PHASELB` (Spur 1–37) |
| SG1 | `SIOS1526_SG1.hfe` | `MWD002` | `SYSORG09` (Spur 1–28, Satzlänge 1024), `PHASELB` (Spur 29–34) |

Alle Positionsangaben in Etiketten sind **1-basiert** (Handbuch-Zählung), Sektornummern 1–26, Spurnummern 0–39.

---

## 1. Physische Ebene

| Eigenschaft | Wert |
|-------------|------|
| Aufzeichnung | MFM, Standard-IBM-Feldaufbau, **128 B/Sektor** (`size`-Code 0), **26 Sektoren/Spur** [SRC] |
| Seiten | einseitig; SYLp/SYLs sind 80-Zylinder-Abbilder (s. §9), DP1/DP2/SG1 40 Zylinder |
| Sektorfolge | **natürlich 1…26**, kein Interleave, kein Skew (Spur 0, 1, 2, 38, 39 in DP1 und 0, 1, 12, 13, 79 in SYLs geprüft) [SRC]. Im VOL1 steht dazu „01" (§3.3) |
| ID-Feld | Zylinder = Spurnummer, Kopf 0, N = 0 [SRC] |
| Füllbyte der Datenspuren | **E5H** (IBM-üblich) in jedem unbenutzten Sektor aller Spuren 1…39 — **auch Spur 38/39** und freie Spuren hinter der Datei [SRC] (DP1 Spur 38/39, DP2 31–39, SG1 35–39 rein E5) |
| Datenmarken | **Normal (FB)** überall außer: in Spur 0 die **gelöschte Datenmarke F8** bei jedem Sektor 9…26 bzw. 10…26, der kein aktives `HDR1` trägt (§4.4) [SRC, alle fünf Abbilder] |

Nutzbar sind laut Handbuch **38 von 40 Spuren**: Spur 0 (Index-/Etikettenspur) + Spur 1…37; „sonst werden die beiden äußeren
Spuren als Reserve übergangen" [BH S. 150/151].  In den Abbildern: kein Etikett reicht über Spur 37 hinaus (DP1/DP2: EOE = 37026),
Spur 38/39 sind E5-gefüllt wie alle freien Spuren.  Eine Sonderkennzeichnung der Reservespuren gibt es **nicht** in den Datenspuren;
ob sie in ERMAP (§3.1) steht, ist offen.

Zeichensatz der Etikettenspur: **DKOI** (Handbuch) = EBCDIC mit fünf Abweichungen, §2.

---

## 2. DKOI gegen EBCDIC

Handbuch S. 150 [BH S. 150, am Bild geprüft; Seite 151 der Nummerierung `B 23`]:

| Code | DKOI | EBCDIC (cp037) |
|------|------|----------------|
| 4AH | `[` | `¢` |
| 4FH | `!` | `\|` |
| 5AH | `]` | `!` |
| 5FH | `^` | `¬` |
| 6AH | `/` | `¦` (im Druck als Zirkumflex-artiges Zeichen erkennbar) |

Praktisch: Python `cp500` liefert für 4A/4F/5A/5F bereits genau das DKOI-Zeichen (`[ ! ] ^`); **nur 6AH** muss von Hand auf `/` gelegt
werden (in `cp500` = `¦`).  Alle übrigen Zeichen sind EBCDIC (A–I = C1–C9, J–R = D1–D9, S–Z = E2–E9, 0–9 = F0–F9, Leerzeichen = 40H).
**In keinem der fünf Abbilder** kommt eines der fünf abweichenden Bytes in Spur 0 vor (Suche über Sektoren 5…26, Pos. 1…80) [SRC] —
die Abweichung betrifft also nur Dateinamen/Datenträgernamen mit Sonderzeichen.  Ein Dateisystem sollte Namen als `[A-Z0-9 ]`
behandeln und Sonderzeichen über die DKOI-Tabelle codieren.

---

## 3. Etikettenspur (Spur 0)

Alle Etikettensektoren (5…26) tragen **80 Zeichen Text (Pos. 1–80) + 48 Bytes 00H (Pos. 81–128)** [SRC] — der IBM-Etiketten-
satz ist 80 B lang, der Rest des 128-B-Sektors ist Null (nicht Blank, nicht E5).

Belegung (DP1/DP2/SG1: Datendisketten; SYLs/SYLp: Systemdisketten):

| Sektor | Datendiskette (DP1, DP2, SG1) | Systemdiskette (SYLs, SYLp) |
|--------|-------------------------------|-----------------------------|
| 1–4 | **128 × 40H** (EBCDIC-Blank), nicht 00 [SRC] | **SYL-Lader** (Z80-Code, §3.5) |
| 5 | `ERMAP` (§3.1) | gleich |
| 6 | 80 × 40H + 48 × 00 (reserviert / leer) | gleich |
| 7 | `VOL1` (§3.3) | gleich |
| 8…26 | `HDR1` / `DDR1` / „leer" (§4) | gleich |

DP1 und DP2 sind in Spur 0 **byteweise gleich** (alle 26 Sektoren) — beide heißen `MWD051` [SRC].  SYLs und SYLp unterscheiden sich in
Spur 0 **nur in den Sektoren 1, 7 und 8** (§6).

### 3.1 Sektor 5: ERMAP (Fehlspurkarte)

Alle fünf Abbilder identisch [SRC]:

```
Pos.  1– 5   C5 D9 D4 C1 D7        "ERMAP"
Pos.  6–80   40 …                  Blank
Pos. 81–128  00 …                  Null
```

Hier ist **kein** Fehlspureintrag vorhanden (keines der Abbilder hat defekte Spuren).  Nach dem Handbuch trägt INIT als letzten
Schritt „die Nummern defekter Spuren" in Spur 00 ein („die Spur 00 noch einmal initialisiert, um die Nummer defekter Spuren
einzutragen") [BH S. 153]; die Spur-Nummern sind 01…39 (zweistellig) [BH S. 158/159], bis zu zwei defekte Spuren sind zulässig;
„fehlerhafte Spuren werden mit einem speziellen Code initialisiert" [BH S. 153].  **Wo und in welchem Format** die Nummern stehen
(hinter `ERMAP` ab Pos. 6? als zwei DKOI-Ziffernpaare?) und **ob ERMAP die Reservespuren nennt**, ist aus den Abbildern nicht zu
entscheiden → §11.  Für ein DiskTool genügt zunächst: lesen/erhalten, beim Anlegen `ERMAP` + Blank wie oben schreiben.

### 3.2 Sektor 6

`40`×80 + `00`×48, in allen fünf Abbildern, nichts weiter [SRC] (in der IBM-Struktur reserviert).

### 3.3 Sektor 7: VOL1 (Datenträgerkennsatz)

Beispiel DP1 (EBCDIC → Text), Pos. 1-basiert:

```
Pos.  1– 4  E5 D6 D3 F1                VOL1
Pos.  5–10  D4 E6 C4 F0 F5 F1          Datenträgername "MWD051" (6 Zeichen)
Pos. 11     40                         Zugriffskennzeichen (blank = frei)
Pos. 12–70  40 …                       reserviert / Besitzer (siehe unten)
Pos. 71     D4                         "M" Aufzeichnungsart (MFM)
Pos. 72     40                         Seitenkennzeichen (blank = einseitig)
Pos. 73–75  40 40 40
Pos. 76     40                         physische Satzlänge (blank = 128 B)
Pos. 77–78  F0 F1                      "01" physische Sektorfolge (natürlich)
Pos. 79     40
Pos. 80     E6                         "W" Etikettenstandard-Version
Pos. 81–128 00 …
```

Die Feldnamen stammen aus der FGEN-Anzeige für `VOL1` [im Programm `FGEN`, Abbild DP1 AADR 022D, Text: „01-04 LABEL IDENTIFIER, 05-10 VOLUME
IDENTIFIER, 11 ACCESSIBILITY INDICATOR, 38-51 OWNER IDENTIFIER, 71 RECORDING MEDIUM, 72 SURFACE INDICATOR, 76 PHYSICAL RECORD LENGTH,
77-78 PHYSICAL RECORD SEQUENCE, 80 LABEL STANDARD VERSION"] [SRC]; das ist die **IBM-Basic-Exchange-Lage**.

| Pos. | Feld | Werte in den Abbildern | Bemerkung |
|------|------|------------------------|-----------|
| 1–4 | Kennung | `VOL1` | |
| 5–10 | Datenträgername | `MWD051`, `MWD002`, `ROBINI` | 6 Zeichen, mit Blank aufgefüllt (hier nie kürzer) |
| 11 | Zugriffskennzeichen | blank | [?] ≠ blank = zugriffsgeschützt (IBM) |
| 38–51 | Besitzer | blank | in allen Abbildern leer |
| 71 | Aufzeichnungsart | **`M`** in DP1/DP2/SG1/SYLp; **blank in SYLs** | M = MFM [?]; SYLs ist physisch MFM und trägt trotzdem kein M → wahrscheinlich ältere INIT-Fassung; **kein** zuverlässiges Erkennungsmerkmal |
| 72 | Seitenkennzeichen | blank | einseitig [?] |
| 76 | physische Satzlänge | blank | blank = 128 B (INIT-Menü 0/1/2/3 = 128/256/512/1024 [BH S. 157]); andere Werte [?] ungeprüft |
| 77–78 | physische Sektorfolge | `01` | = natürliche Folge (INIT-Vorgabe `01`) [BH S. 158] |
| 80 | Etikettenstandard | `W` | in allen fünf Abbildern; Bedeutung [?] (IBM: blank/„1"; hier robotron-eigen) |

Datenträgername in **allen** Bereichen außer 5–10, 71, 77–78, 80 ist blank; Pos. 81–128 = 00.

### 3.4 Sektor 8…26: Dateikennsätze (HDR1/DDR1)

Siehe §4.  Insgesamt sind **19 Etikettensätze** (S. 8…26) je Diskette möglich; „Spur 00 gefüllt, neuer HDR1 nicht möglich" ist die
FGEN-Fehlermeldung bei Überlauf [BH S. 167/169].

### 3.5 Sektor 1–4 der Systemdiskette: SYL-Lader

Beide Systemdisketten tragen denselben 4-Sektoren-Lader, beginnend mit `53 59 4C` = **`SYL`** (Z80-Code ab Offset 16:
`FD 7E 00 A8 47 0F …`, Sektor 4 endet `… 00 9E 29`) [SRC].  Aufbau des Kopfes:

```
Offset 0–2  "SYL"
Offset 3    04            Anzahl Sektoren des Laders (S. 1–4) [?]
Offset 4–6  00 LL 00      LL = 88H (SYLs, SIOS 1.1) bzw. 6CH (SYLp, SIOS 0.9)  <- einziger Unterschied
Offset 7–15 00 …
Offset 16…  Code
```

Dasselbe Muster beginnt auch die Systemdatei selbst (`SYSARE11`, Spur 1 Sektor 1: `SYL 01 00 88 00 FD 7E …`): Byte 3 = `01`, Byte 5 = 88H.
**Zur Bedeutung von LL** gibt es einen verblüffend einfachen Zusammenhang, der an zwei Punkten trifft: LL = (belegte Sektoren der
Systemdatei − 26) / 2, mit belegt = EOD−BOE in Sektoren (SYLs: EOD 12013 → 298 Sektoren → 136 = 88H ✓; SYLp: EOD 10009 → 242 Sektoren →
108 = 6CH ✓).  Ob das Absicht (Länge in 256-B-Einheiten ab Spur 2) oder Zufall (Tastaturvariante?) ist, ist offen [?] → §11.

---

## 4. Dateikennsätze HDR1 / DDR1 (Sektor 8…26)

### 4.1 Felder

Positionen 1-basiert, EBCDIC/DKOI, Zahlen **ASCII-Ziffernstil in EBCDIC** (`F0…F9`), rechtsbündig, mit führenden Nullen bzw. Blanks:

| Pos. | Länge | Feld | Beispiel (SG1 `SYSORG09`) | Bemerkung |
|------|-------|------|---------------------------|-----------|
| 1–4 | 4 | Kennung | `HDR1` | `DDR1` = gelöscht (§4.3) |
| 5 | 1 | blank | | |
| 6–22 | 17 | Datei-/Bibliotheksname | `SYSORG09` + 9 Blank | IBM: 17 Zeichen; in allen Abbildern ≤ 8 Zeichen, linksbündig |
| 23–27 | 5 | **Satzlänge** (Dezimal) | `01024` | `00128` bei den Systemdisketten und Bibliotheken |
| 28 | 1 | blank | | |
| 29–33 | 5 | **BOE** (Beginning of Extent) | `01001` | `TTSSS`: Spur 01, Sektor 001 |
| 34 | 1 | blank | | |
| 35–39 | 5 | **EOE** (End of Extent) | `28026` | Spur 28, Sektor 026 (letzter Sektor des Bereichs) |
| 40–42 | 3 | blank | | |
| 43 | 1 | Schreibschutz | `P` bei `SYSARE11`/`SYSARE09` | IBM: P = geschützt [?]; sonst blank |
| 44 | 1 | Austauschkennzeichen | `E` bei allen Dateien der Datendisketten | IBM „E = Exchange"; bei den Systemdisketten blank [SRC] |
| 45 | 1 | Mehrdisketten-Kennzeichen | `C` bei `SYSORG09` | IBM: **C = Datei wird auf der nächsten Diskette fortgesetzt** (continued), `L` = letzte.  **Am laufenden SIOS belegt (2026-10-30):** SGEN meldet beim Zugriff auf Satz 98 `ERR FD 6 SEQU SYSORG09 98…` = „nächster Datenträger erforderlich“ (Handbuch S. 25/26, 100); Wächter `SiosSgen.ModulAufDerFolgedisketteVerlangtDenNaechstenDatentraeger` |
| 46–47 | 2 | Folgenummer der Diskette | `01` bei `SYSORG09` | Nummer dieser Diskette in der Folge |
| 48–53 | 6 | (Datum? / ?) | blank; bei `SYSARE*` `000004` | in IBM „Erstellungsdatum"; hier `000004` [?] |
| 54–57 | 4 | logische Satzlänge | `1024` bei `SYSORG09`, sonst blank | wiederholt die Satzlänge, wenn ≠ 128 [?] |
| 67–72 | 6 | (Ablaufdatum?) | `999999` bei `SYSARE*` | „nie ablaufend" [?] |
| 75–79 | 5 | **EOD** (End of Data) | `29001` | `TTSSS` des **ersten freien Sektors** hinter den Daten (§5.2) |
| 80 | 1 | blank | | |
| 81–128 | 48 | 00H | | |

Die Spalten 41–42/48–66/73–74 sind in allen Abbildern blank.  Die Felder in der Tabelle stimmen mit der Handbuch-Beschreibung
des FGEN-Menüs („NEW HDR1 / CHANGE HDR1", Handbuch-Seiten 160–169) nur sparsam überein — das Handbuch gibt die HDR1-Positionen nicht an;
die obige Zuordnung folgt der IBM-3740-Lage und den Werten der Abbilder.

**Vollständige Hex-Beispiele** (SG1, `[SRC]`):

```
S8  HDR1 SYSORG09 …:
 C8 C4 D9 F1 40 E2 E8 E2 D6 D9 C7 F0 F9 40×9 40 40 F0 F1 F0 F2 F4 40 F0 F1 F0 F0 F1 40 F2 F8 F0 F2 F6 40 40 40
 40 C5 C3 F0 F1 40×6 F1 F0 F2 F4 40×18 40 F2 F9 F0 F0 F1 40  00×48
 = "HDR1 SYSORG09         01024 01001 28026    EC01      1024                 29001 "

S9  HDR1 PHASELB:  "HDR1 PHASELB         00128 29001 34026    E                              35001 "

SYLs S8 (System):  "HDR1 SYSARE11         00128 01001 12026   P    000004             999999  12013 "
SYLp S8 (System):  "HDR1 SYSARE09         00128 01001 10026   P    000004             999999  10009 "
```

### 4.2 Bedeutung von BOE/EOE/EOD

* **BOE/EOE** = erster und letzter Sektor des **vorab reservierten, zusammenhängenden** Bereichs (`TTSSS`, Spur 01…, Sektor 001…026).  Der
  Bereich ist immer **ganze Spuren** (BOE `…001`, EOE `…026`) [SRC: alle neun HDR1-Sätze].  Die Dateien sind **nicht fragmentiert** (kein Folgezeiger,
  keine Extentliste) [SRC].
* **Linearer Sektorindex** innerhalb der Datei = `(Spur − BOE-Spur) × 26 + (Sektor − 1)`.  Spur-Wechsel wie bei jedem Abbild (Sektor 26 → nächste Spur,
  Sektor 1).
* **EOD** = Adresse des ersten *nicht* mit Daten belegten Sektors:
  * Systemdateien: EOD **innerhalb** des Bereichs: `SYSARE11` EOD 12013 (letzter nicht-E5-Sektor = Spur 12 Sektor 12 [SRC]; 12013 = der nächste),
    `SYSARE09` EOD 10009 (letzter nicht-E5-Sektor aber Spur 10 Sektor 16 — **Widerspruch, +8 Sektoren**, [?] ältere Dateiendeverwaltung oder
    Altlast; die Daten hinter EOD sind keine Nullen).
  * Bibliotheken/Datenbereiche, die vollständig reserviert werden: EOD = EOE + 1 Sektor, d. h. **auf die erste Spur hinter dem Bereich**
    (`PHASELB` DP1/DP2: EOE 37026, EOD **38001**; SG1 `PHASELB` EOE 34026, EOD 35001; `SYSORG09` EOE 28026, EOD 29001) [SRC].
    Für diese Dateien gibt es also kein „Datenende" im Etikett; der Inhalt ist selbst begrenzt (Bibliotheksverzeichnis, §7).
* **Anlegen** (FGEN „NEW HDR1"): vom Anwender zugewiesener Bereich; LBSV `NEWLB TT` belegt `TT` Spuren **unmittelbar hintereinander** in
  einer Lücke von Spur 0 aus gesehen („zusammenhängend frei, unter Berücksichtigung aller auf Spur 0 definierten Dateien" [BH S. 179/180]),
  Standard 74 Spuren (8"), erweitern nur an das Ende, wenn die Folgespuren frei sind [BH S. 180/181].
* **Verschachtelung/Reihenfolge:** die Reihenfolge der Etikettensätze in Spur 0 ist **nicht** die der Bereiche auf der Platte (Handbuch Anlage 4.1
  zeigt `DATEI 1` Spur 8–3x (Ziffer im Druck unleserlich), `DATEI 3` Spur 65–73, `SOURCELB` Spur 43–49, `DATEI 4` Spur 39–42 in Sektorfolge 8, 10, 11, 13) [BH S. 189/190].
  Freie Etikettensektoren zwischen aktiven (z. B. `DDR` in S. 9/12/14…) sind erlaubt.

### 4.3 DDR1 (gelöscht) und freie Etikettensätze

* **Gelöscht:** `HDR1` wird zu `DDR1` (FGEN „DELETE HDR1": „Anzeige Dateikennsatz als gelöscht" [BH S. 168]).  In SG1 stehen **17 DDR1-Sätze**
  `DATA10`…`DATA26` (Sektor 10…26), jeweils `"DDR1 DATAnn           080 01001 73026 …  01001"`:
  Satzlänge `080` (Pos. 23–27 = `  080`), BOE `01001`, EOE `73026`, EOD `01001`, sonst leer [SRC].  Das sind offenbar Platzhalter-Dateien
  einer 8″-Diskette (73 Spuren), die später gelöscht wurden [?].
* **Nie benutzt:** der Satz besteht nur aus `D` (C4) + 79 × 40H + 48 × 00H (z. B. DP1/DP2/SYLs/SYLp Sektor 9…26) [SRC].  Das ist der Zustand,
  den INIT erzeugt [?] — das `D` entspricht dem ersten Buchstaben von `DDR1`.
* **Lesen:** ein Dateisystem nimmt als **aktiv** nur Sektoren mit `HDR1` **und** normaler Datenmarke an.

### 4.4 Die gelöschte Datenmarke F8H

**Regel [SRC, alle fünf Abbilder, ohne Ausnahme]:** In Spur 0 tragen **genau die Etikettensektoren, die kein aktives `HDR1` enthalten**, die
gelöschte Datenmarke (F8).

| Abbild | Sektoren mit F8 | Sektoren mit FB |
|--------|-----------------|-----------------|
| DP1, DP2 | 9…26 | 1…8 |
| SG1 | 10…26 | 1…9 |
| SYLs, SYLp | 9…26 | 1…8 |

(Alle Sektoren 1…8 bzw. 1…9 inkl. ERMAP, Sektor 6, VOL1 haben die normale Marke.)  Das ist die IBM-Konvention: ein HDR1 ist normal, ein
gelöschter/leerer Etikettensatz „deleted".  Das ist also **Absicht**, und die Datenmarke ist **Teil des Formats** — der Zustand
„HDR1 vs. DDR1/leer" ist **doppelt** kodiert (Text und Marke).  Konsequenzen für ein Dateisystem:

* **Die Datenmarke muss mitgeführt und beim Schreiben gesetzt werden** (`.hfe`/`.dmk` können sie; `.img` **nicht** — ein `.img` verliert
  das Löschkennzeichen; eine reine Sektordatei ist aber zum Rekonstruieren ausreichend: aktiv = Text `HDR1`).
* Die Erkennung des Dateisystems darf sich **nicht** auf F8 verlassen (kommt nur bei den Etiketten vor).
* Emulator-Seite (S6): eine SIOS-Leseroutine, die auf `HDR1` prüft, braucht die gelöschte Marke gar nicht auszuwerten [?]; ob sie sie auswertet, wird
  am Code zu klären.

---

## 5. Physische Dateien

### 5.1 Lage

Eine physische Datei = ein Etikettensatz = ein **zusammenhängender** Bereich ganzer Spuren `[BOE, EOE]`.  Beispiele aus den Abbildern:

| Diskette | Datei | Spuren | Sektoren | Satzlänge |
|----------|-------|--------|----------|-----------|
| SYLs | `SYSARE11` | 1–12 | 312 (belegt bis EOD: 298) | 128 |
| SYLp | `SYSARE09` | 1–10 | 260 | 128 |
| DP1, DP2 | `PHASELB` | 1–37 | 962 | 128 |
| SG1 | `SYSORG09` | 1–28 | 728 = 91 Sätze zu 1024 | **1024** |
| SG1 | `PHASELB` | 29–34 | 156 | 128 |

Auf einer Diskette liegen höchstens 19 physische Dateien (Etikettensektoren 8…26).  Bibliotheken: höchstens drei je Diskette
(`SOURCELB`/`RELOCLB`/`PHASELB`) [Web, `doc/design/43_sios1526.md` §1.1; LBSV-Typen S/R/P, BH S. 179].

### 5.2 Satzlänge 128 vs. 1024

Der Satz ist die **logische Einheit der Datei**; die Diskette hat immer 128-B-Sektoren.  Satzlänge `01024` bedeutet: **ein Satz = 8
aufeinanderfolgende Sektoren des linearen Sektorindex** [SRC `SYSORG09` — 728 Sektoren = 91 Sätze, jeder Satz beginnt in der
Mitte einer Spur, wenn nötig; 26 ist kein Vielfaches von 8].  Dies wird durch die Struktur der Daten selbst bewiesen: die
Modulkopfbytes (§8) stehen an den Satzgrenzen `n × 8` Sektoren ab Spur 1/Sektor 1 (Beispiel: Satz 5 beginnt beim linearen Sektor 40 =
Spur 2 Sektor 15).  Es gibt **keinen Satzkopf und keine Satzprüfsumme**; Satzbegrenzung = Position.

**Füllung zwischen Datenende und Satzende:** in der `SYSORG09` stehen Nutzdaten und danach **E5H** bis zum Sektorende, wobei innerhalb eines Satzes
Teilbereiche sektorweise mit E5 aufgefüllt sind (Beispiele: Satz 0 nach dem Verzeichnis: Sektoren 5–7 = E5; Satz 1: Sektor 6 = E5).
Die Füllung entspricht dem Füllbyte der ungenutzten Spuren.  Bei den Bibliotheken (128-B-Sätze) endet die Datei ebenso mit E5 (z. B. DIKA, §7.3).

Anderes als 128 B/1024 B (256/512) ist an den Abbildern **nicht** gemessen.  Spur 0 ist laut Handbuch **immer** 128 B/Sektor in natürlicher Folge
[BH S. 149/150]; abweichende Sektorlängen betreffen nur die übrigen Spuren (INIT-Menü 0…3) — für ein DiskTool vorerst nur 128 B belegt.

---

## 6. Unterschiede SYLs ↔ SYLp, VOL1-Pos. 71 `M`

Spur 0 der beiden Systemdisketten unterscheidet sich **nur** in:

| Sektor | SYLs (SIOS 1526/1.1) | SYLp (SIOS 1526/0.9) |
|--------|---------------------|----------------------|
| 1 | Lader-Byte 5 = **88H** | **6CH** |
| 7 (VOL1) | **kein `M`** an Pos. 71 | `M` an Pos. 71 |
| 8 (HDR1) | `SYSARE11`, EOE `12026`, EOD `12013` | `SYSARE09`, EOE `10026`, EOD `10009` |

Sektoren 2–4 (Ladercode), 5, 6, 9–26 sind byteweise gleich.  `M` an Pos. 71 steht in **jedem** Abbild außer SYLs, `W` an Pos. 80 in jedem.
Die beiden Systeme sind nach Befund `doc/design/43_sios1526.md` §1.2 für verschiedene Tastaturen (K7636/K7637) generiert; ob 88H/6CH damit
zusammenhängt, ist offen (SG1 enthält Ladervorlagen mit **74H**, **6CH** und **70H** — die Vorlagen für SGEN, §8.3) [?].

---

## 7. Bibliotheken (PHASELB)

Eine **Bibliothek** ist eine physische Datei (HDR1 `SOURCELB` / `RELOCLB` / `PHASELB`, Satzlänge 128), deren Inhalt von **LBSV** verwaltet wird;
SIOS lädt `CAL name` aus der `PHASELB`.  Handbuch: `LBTYP S`/`R`/`P` = Quelltext-/Modul-/Phasenbibliothek [BH S. 179].  Die drei Bibliotheken auf
DP1/DP2/SG1 sind alle `PHASELB`.

### 7.1 Aufbau

Ab dem **ersten Sektor der Datei** (Spur BOE-Spur, Sektor 1), Adressierung `AADR` = **laufende Sektornummer ab 1** (nicht 0!) innerhalb der
Datei:

```
AADR  1…18  Verzeichnisbereich (18 Sektoren, Füllbyte 03H)
AADR 19…    Datenteil: Objekte lückenlos hintereinander (bis EOD/ND)
```

*Der Datenteil beginnt in allen drei Bibliotheken bei AADR 0013H = 19* [SRC DP1, DP2, SG1, und im Handbuch-Beispiel `PROG1` = 0013H [BH S. 192]].

### 7.2 Verzeichnis

Das Verzeichnis liegt im **zweiten Sektor** der Datei (AADR 2, z. B. DP1 Spur 1 Sektor 2); Sektor 1 und Sektor 3…18 sind in allen drei Abbildern
**vollständig 03H** (Füllbyte „unbenutzt/leer"; der LBSV-Standardbibliotheksvorgang schreibt offenbar den ganzen 18-Sektoren-Bereich mit 03H
vor) [SRC].  Der Sektor enthält 16 Slots à 8 Byte:

```
Slot 1…15  Eintrag
Slot 16    Fuß (Trailer), Offset 120…127
unbenutzte Slots: 03 03 03 03 03 03 03 03
```

**Eintrag (8 Byte):**

| Offset | Länge | Inhalt | Beispiel |
|--------|-------|--------|----------|
| 0–4 | 5 | **Name**, **ASCII**, rechts mit **00H** aufgefüllt (max. 5 Zeichen) | `43 41 54 4D 00` = `CATM` |
| 5–6 | 2 | **AADR**, little-endian: Sektornummer (ab 1) des ersten Sektors **in der Datei** | `13 00` = 0013H |
| 7 | 1 | **ML**, 1 Byte | `12` = 18 |

Namen sind ASCII (nicht EBCDIC) und tragen **5** Zeichen (`PRMFS`, `RAMT1`, `SUS07` auf DP2 füllen alle fünf Bytes) [SRC DP2].  Verzeichnis von
**DP1** (Spur 1 Sektor 2, 15 Einträge) [SRC]:

```
CATM  00 13 00 12  →  AADR 0013  ML 12      AADR + 2·ML = 0037  (= AADR von CODK)
CODK  00 37 00 26  →  AADR 0037  ML 26      0037 + 4C = 0083   (= COCA)
COCA  00 83 00 11  →  AADR 0083  ML 11
COPD  00 A5 00 2A  →  AADR 00A5  ML 2A      (+54H = 00F9, nächster AADR 00FB: Lücke 2 Sektoren)
DINH  00 FB 00 09  DIMO 00 0D 01 0F  (AADR 010D)  DIKA 00 2B 01 12 (012B)  DMPM 00 55 01 09 (0155)
EDIT  00 67 01 62 (0167)  FGEN 00 2D 02 34 (022D)  INIT 00 95 02 20 (0295)  DIDI 00 D5 02 17 (02D5)
KORP  00 03 03 19 (0303)  LBSV 00 35 03 28 (0335)  CODP 00 85 03 17 (0385)
Fuß:  03 00 B3 03 C2 03 03 03
```

Die Darstellung im Handbuch (`PRINT DIC`, Anlage 6/7: Spalten **NO · NAME · AADR · ML**, AADR vierstellig hex, ML zweistellig hex) stimmt exakt:
Handbuch `PROG1 0013 03`, `PROG2 0019 01`, `PROG3 001B 09` → 0013 + 2·3 = 0019 ✓, 0019 + 2·1 = 001B ✓, 001B + 2·9 = 002D ✓
(`PROG4 002D`) [BH S. 192, 193].

**Fuß (8 Byte, Offset 120…127 des Verzeichnissektors)** [SRC]:

| Offset | Länge | DP1 | DP2 | SG1 | Handbuch-Name | Bedeutung |
|--------|-------|-----|-----|-----|---------------|-----------|
| 0–1 | 2 | `03 00` | `02 68` | `02 18` | **NV** (hex, **big-endian**: `0300`, `0268`, `0218`) | [?] siehe unten |
| 2–3 | 2 | `B3 03` | `F9 02` | `87 00` | **ND** (LE): `03B3`, `02F9`, `0087` | AADR des **ersten freien** Datensektors = Ende der belegten Daten |
| 4–5 | 2 | `C2 03` | `C2 03` | `9C 00` | **BOE** (LE): `03C2`=962, `03C2`, `009C`=156 | **Gesamtgröße der Bibliothek in Sektoren** (= Spuren × 26: 37 × 26, 37 × 26, 6 × 26) |
| 6–7 | 2 | `03 03` | `03 03` | `03 03` | | Füllbyte |

Der Kopf der Handbuch-Anzeige lautet `DICTIONARY-PHASELB VOL-NAME NV: …  ND: …  BOE: …` [BH S. 192/193].  ND trifft in allen drei Abbildern
(= AADR(letzter Eintrag) + 2·ML(letzter Eintrag) = ND; DP1: 0385 + 2E = 03B3 ✓, DP2: 02C9 + 30 = 02F9 ✓, SG1: 0063 + 24 = 0087 ✓).
**BOE = Größe in Sektoren** trifft in allen drei Abbildern (Handbuch Anlage 6: `BOE: 009C` = 156, passend zu einer 6-Spur-Bibliothek wie SG1).
**NV** [?]: Werte DP1 `0300` (15 Einträge), DP2 `0268` (13), SG1 `0218` (3), Handbuch Anlage 6 `0248` (9 Einträge angelegt, 2 gelöscht) —
passt zu `NV = 0200H + 8 · (Zahl der belegten Slots)` für kleine Verzeichnisse (3 → 0218 ✓, 9 → 0248 ✓, 13 → 0268 ✓), **nicht** für
DP1 (15 → ergäbe 0278, tatsächlich 0300).  Das Handbuch zeigt bei 27 Einträgen ebenfalls `0300`.  Vermutlich „nächster freier Verzeichnisplatz" als
Pufferadresse im LBSV (Pufferbasis 0200H, 8 Byte je Eintrag), gedeckelt oder auf die nächste 256-B-Grenze gerundet.  **Nicht** zum Anlegen
nötig, solange man den Wert beim Ändern fortschreibt wie beobachtet (Rechenvorschrift am LBSV zu belegen, §11).

### 7.3 Objekte (Datenblöcke)

* **Ein Objekt = ein zusammenhängender Sektorbereich** `[AADR, AADR + len)`.  `len` ist **nicht** direkt im Verzeichnis: sie ist der Abstand zum
  nächsten AADR (in AADR-Reihenfolge; beim letzten Eintrag zu ND) [SRC].  Abbilder: 22 von 31 Einträgen (DP1 12 von 15, DP2 7 von 13, SG1 3 von 3) decken `len = 2·ML` exakt;
  Abweichungen: DP1 `COPD` (+2), `DIKA` (**+6**), `EDIT` (+2); DP2 `LINK` (+2), `MBAS` (+2), `PRMFS` (+1), `PRDI` (+2), `RAMT1` (+1), `TEXT` (+2) [SRC].
  **Fazit:** `len ≥ 2·ML`, `ML` ist demnach **kleiner oder gleich ⌈len/2⌉** (Einheit 256 B): ML ist vermutlich die **Speicherlänge** des
  geladenen Programms in 256-B-Seiten, `len` die Länge der abgelegten (mit Lade-Information versehenen) Datei in Sektoren [?].
  Beispiel DIKA: ML 12H (4608 B), belegt aber Sektor 299…340 = 42 Sektoren; der letzte Sektor (340) ist reiner E5-Füller, die Sektoren 335…339 sind
  Programmtext („DIKA 1520 (SIEX) V 1.1 … DEVICE …").
* **Keine Fragmentierung, keine Kettung** innerhalb der Abbilder: jedes Objekt hat genau **einen** Bereich.  Die im Handbuch erwähnten „Lücken" sind
  Löschlücken (`DELET`: Name wird Blank [BH S. 181/183: „Gelöschte Bücher, Moduln oder Phasen werden als Leerzeichen im Namensfeld dargestellt"], Datenteil
  bleibt stehen; `COPY` verdichtet) [BH S. 181/184].  Das Handbuch-Beispiel Anlage 7.1 zeigt gelöschte Einträge (z. B. Nr. 9, 11, 16, 19 ohne Namen,
  aber mit AADR/ML) — der gelöschte Eintrag **behält AADR und ML** [BH S. 193].
* **Kontrolle der Zuordnung:** In jedem Objektbereich steht der eigene Programmtitel als Klartext; Auszug [SRC]:

| Bibliothek | Eintrag | AADR | Fundstelle des Titels |
|------------|---------|------|-----------------------|
| DP1 | CATM | 0013 | `CATM 1520(SIEX) V0.3` im 1. Sektor des Objekts |
| DP1 | COPD | 00A5 | `COPD 1520 (SIEX) V 0.3` im 2. Sektor |
| DP1 | EDIT | 0167 | `EDITOR 1520 (SIEX) V 0.5` |
| DP1 | FGEN | 022D | `FGEN 1520(SIEX) V1.0` |
| DP1 | INIT | 0295 | `INIT  1520 (SIEX) V 1.4` — Handbuch beschreibt **V 1.1**, Abbild V 1.4 [BH S. 154] |
| DP1 | LBSV | 0335 | `LBSV  1520(SIEX)  V 1.1` |
| DP2 | LINK | 0013 | `LINK 1520 (SIEX) V 0.3` |
| DP2 | SGEN | 01D9 | `SGEN 1520 (SIEX)  V 1.4` |
| DP2 | SYSD | 0229 | `SYSD 1520 (SIEX) V 1.5` |
| SG1 | SGEN/SOPH/SYSD | 0013/0049/0063 | gleiche Objekte wie auf DP2 (Bytes identisch) |

### 7.4 Objektformat („Phase")

Die Objekte sind **keine** Rohabzüge von Z80-Code, sondern ein **Strom aus Segmenten mit eingebauter Relozierung**.  Beobachtet an allen Objekten
(Erkennung per Parser über Spur 1…37 von DP1/DP2/SG1: jedes Objekt lässt sich bis zu einer Endmarke zerlegen) [SRC]:

| Byte | Folge | Deutung [?] |
|------|-------|-------------|
| `80 LL HH` + LL…HH Bytes | langer Block | Literal: Länge `HHLL` (LE), danach Bytes |
| `81` … `8F` + n Bytes | kurzer Block | Literal: n = niedriges Nibble (1…15) |
| `40 LL HH` | Adresse | **16-Bit-Wort**, an dieser Stelle zu relozieren (z. B. `CD 40 35 0C` = `CALL` mit Adresse 0C35H) |
| `20 LL HH` | Füllung | `HHLL` Bytes Platz/Nullen (DS) |
| `04` … `08 00 00 04` | Segmentwechsel/Ende [?] | bei **jedem** Objekt tritt `08 00 00 04` bzw. `04` als Segmentgrenze auf; dahinter weitere Daten (Textkonstanten) |

Bei **`CODK`, `PRMFS`, `PHAK`** (drei Programme) beginnt das Objekt mit einem Kopf, in dem der Name steht:
`06 00 <NAME 5 Z.> 00 00 00 50 07 <w16>` (z. B. `06 00 43 4F 44 4B 20 00 00 00 50 07 C6 1F …`; w16 = 1FC6H bzw. 0F64H / 3D88H) [SRC].  Alle
übrigen Objekte beginnen **ohne Namen** mit `20 00 00`, `80 …` oder `81 …`.  **Ladeadresse, Länge und Startadresse** sind aus diesen Bytes
nicht eindeutig ablesbar (die Gesamtlänge ergibt `max(40-Adresse) ≈ decodierte Länge`, aber nicht durchgängig — CATM `3285` vs. `2977`) → am
CAL-/Ladercode zu klären (§11).  **Für ein DiskTool ist das unkritisch:** Objekte werden als Rohbytes (Bereich bis zum Nachbarn bzw. ND) behandelt.

### 7.5 Verzeichnissektor 1 (AADR 1) „voller 03H"

Der erste Sektor der Datei und die Sektoren 3…18 sind **nur 03H** [SRC].  `03H` ist das **Füllbyte des Bibliotheksverzeichnisses** (so wie E5 das der
Datenspuren und 40H das der Etiketten ist); es steht für „unbenutzt".  Weshalb gerade Sektor 1 leer bleibt (Reserve für einen Kopf?) und
ob das Verzeichnis bei mehr als 15 Einträgen in Sektor 3, 4, … fortgesetzt wird (Handbuch-Beispiel zeigt 30 Einträge → ja, mindestens ein weiterer
Sektor), ist an den Abbildern nicht zu sehen (alle haben ≤ 15) → §11.

---

## 8. SYSORG09 (SG1): Generiermodule für SGEN

`SYSORG09` = „Systemorganisation" (Generiermodule), HDR1 `SYSORG09`, Satzlänge **1024**, `EC01` = **Datei wird auf der Folgediskette (Folge 2) fortgesetzt** —
SG1 trägt nur den ersten Teil: **91 Sätze** (Spur 1…28).  Das Verzeichnis kennt aber **48 Module** und Positionen bis 160 (§8.1), der Rest liegt auf der
(nicht vorhandenen) Diskette SG2 [SRC].

### 8.1 Satz 0 = Verzeichnis

Satz 0 (Spur 1 Sektor 1…8) enthält ab Offset 0 **12-Byte-Einträge**, Rest E5:

| Offset | Länge | Inhalt | Beispiel |
|--------|-------|--------|----------|
| 0–7 | 8 | Modulname, **ASCII**, Blank aufgefüllt | `GRUND   ` |
| 8–9 | 2 | **Anzahl Sätze** des Moduls, zwei ASCII-Ziffern | `06` |
| 10–11 | 2 | **Position** = Nummer des ersten Satzes **(1-basiert)**, little-endian | `06 00` |

Reihenfolge = Position aufsteigend; Position + Anzahl = Position des nächsten Moduls (lückenlos) [SRC].  Satz mit Position `p` ist der Satz mit
dem 0-basierten Index `p − 1` (Satz 0 = Verzeichnis ist Position 1; Sätze 1…4 = Positionen 2…5 sind die **SYL-Ladervorlagen** (§8.3), die nicht im
Verzeichnis stehen).  Das Verzeichnis endet mit einem Eintrag `FF 00 00 …` (Offset 576), dahinter ab Offset 600 der **Dateiname `SYSORG09`** (8 Zeichen, mit 00 aufgefüllt)
und dann E5.  Die **letzten vier Bytes des Satzes** (Offset 1020…1023) sind `58 00 5B 00` = 88 und 91 (LE) — 91 = Anzahl Sätze auf dieser Diskette
(728 Sektoren/8), 88 = Position des letzten Moduls auf dieser Diskette (`PHY KM`) [?].

Alle 48 Einträge (Position, Anzahl):

```
GRUND   6/6   T-K7637 12/4  T-K7606 16/3  T-K7636 19/3  T-K7633 22/3  KUT     25/1  BAB     26/4
BIN ARI 30/1  DEZ ARI 31/1  BCD ARI 32/2  SD 1156 34/2  SD 56-L 36/2  SD 1-P  38/3  SD 1-I  41/4
SD 1-IZ 45/4  SD 2-P  49/3  SD 2-PZ 52/3  SD 2-I  55/3  SD 2-IZ 58/3  SD 3-I  61/4  SD 3-IZ 65/4
PH-FD320 69/4 PH-FD640 73/6 LOG FD2 79/5  LOG FD1 84/4  PHY KM  88/4  ──── ab hier nur auf Diskette 2 ────
LOG KM  92/4  LBE     96/1  MB      97/1  SLE     98/4  HLE 2  102/1  HLE 4  103/1  SIM    104/2
DFUE-A 106/3  DFUE-I 109/3  DFUE-V 112/3  V24    115/2  IFSS1  117/2  IFSS2  119/2  IFSS3  121/2
BSC7LZ 123/6  BSC7LV 129/6  BSC7CZ 135/6  BSC7CV 141/6  BSC8CZ 147/6  BSC8CV 153/6  FUELLM 159/1  FIX 160/1
```

Die Namen sind die **Generier-Optionen** der SGEN-Programmfenster (Terminals `T-K7637/K7606/K7636/K7633`, Disketten `SD …`/`PH-FD…`/`LOG FD…`, Kassette
`PHY KM`/`LOG KM`, DFÜ `DFUE-*`, `V24`, `IFSS1…3`, Bisync `BSC…`, Fließkomma `BIN/DEZ/BCD ARI` …) [?].

### 8.2 Modulsätze

Jeder Satz eines Moduls (1024 B) beginnt mit **2 Kopfbytes**, dann Z80-/Phasendaten, danach E5 bis zum Satzende [SRC, alle 86 Modulsätze dieser Diskette]:

```
Byte 0 = (Gruppe << 4) | Laufnummer des Satzes im Modul (0, 1, 2 …)
Byte 1 = 0x80 | 0x10 (wenn noch ein Satz des Moduls folgt) | Variantennummer (niedriges Nibble)
```

Beispiele (Position, Anzahl → Satzköpfe): `GRUND` (6,6) → `00 90, 01 90, 02 90, 03 90, 04 90, 05 80`; `T-K7637` (12,4) → `10 94, 11 94, 12 94, 13 84`;
`T-K7606` → `10 90, 11 90, 12 80`; `KUT` (25,1) → `10 81`; `BAB` → `70 90, 71 90, 72 90, 73 80`; `BCD ARI` → `40 92, 41 82`;
`SD 1-P` → `50 90, 51 90, 52 80`; `PH-FD640` → `30 92 … 35 82`; `PHY KM` → `90 90, 91 90, 92 90, 93 80` [SRC].  Gruppen: 0 GRUND, 1 Terminals/KUT,
3 PH-/LOG-FD, 4 ARI, 5/6 SD, 7 BAB, 9 KM.  Die **Variantenziffer** (niedriges Nibble des 2. Bytes) ist bei T-K7637 = 4, T-K7606 = 0, T-K7636 = 2, T-K7633 = 3,
`SD 1-I` 4 …  — sie unterscheidet Ausprägungen innerhalb der Gruppe.  Ein Satz kann mehrere Teile enthalten, die auf Sektorgrenzen beginnen und mit E5 aufgefüllt
sind.

### 8.3 SYL-Ladervorlagen (Sätze 1…4)

Satz 1: `SYL 01 00 74 00 …` (Sektor 0), `SYC 00 00 10 21 …` (Sektor 7); Satz 2: `SYL 01 00 6C 00 …` + `SYC 00 00 00 21 …`; Satz 3 Fortsetzung; Satz 4: `SYL 04 00 70 00 …` [SRC].
Aufbau wie der SYL-Kopf (§3.5): Byte 5 = **74H / 6CH / 70H**.  Das sind die Vorlagen, aus denen SGEN den passenden Lader in Spur 0 und den Systemanfang
schreibt [?].

---

## 9. Auffälligkeiten der Abbilder

* **SYLp** = 40-Spur-Diskette, im 80er-Laufwerk gelesen: Seite 0 trägt Zylinder 0–39 und (leer formatiert) 69–79; Zylinder 40–68 sind unformatiert;
  Seite 1 leer bis auf einen Streusektor `c15h1` ID 143 (CRC-Fehler, ID-Zylinder 9, ID-Kopf 141) [SRC `k1520disktool measure`; Entwurf 43 §1.1].
* **SYLs** ist 80 Zylinder einseitig, in **allen** 80 Zylindern formatiert (26 × 128, Spuren 13…79 E5), obwohl INIT V 1.1 nur 40 Spuren kann [BH S. 154] → anders hergestellt
  oder auf einem 80-Spur-Laufwerk neu überschrieben [?].
* **DP2**: Spur 31…39 rein E5 (Datei endet in Spur 30, aber HDR1 meldet EOE 37026 — **die Datei ist 37 Spuren groß angelegt, belegt 30**).
* **SYLp**: Datei reicht über EOD hinaus (§4.2).

---

## 10. Was ein DiskTool zum Anlegen einer leeren SIOS-Diskette schreiben muss (INIT-gleich, 5,25″ 40 Spuren 26 × 128)

Nach den Abbildern und dem Handbuch [BH S. 149–159]:

**Physische Ebene (alle Spuren 0…39):** MFM, IDs Zylinder = Spur, Kopf 0, Sektoren 1…26 **in natürlicher Folge**, N = 0 (128 B), normale Datenmarke FB;
Spuren 1…39 ganz mit **E5H** (auch 38/39).  Umdrehung genau eine Spur (Normlücken wie bei den K5601-Disketten, `doc/design/16_k8915.md` AP-F1).

**Spur 0 (Byte für Byte, Datenträgername `NAME` ≤ 6 Zeichen, Datendiskette):**

| Sektor | Inhalt | Datenmarke |
|--------|--------|------------|
| 1–4 | 128 × `40` (Systemdiskette: SYL-Lader, §3.5) | FB |
| 5 | `C5 D9 D4 C1 D7` (`ERMAP`) + 75 × `40` + 48 × `00` | FB |
| 6 | 80 × `40` + 48 × `00` | FB |
| 7 | `VOL1` + `NAME` (6) + 60 × `40` + `D4` (Pos. 71 = `M`) + `40`×4 + `F0 F1` (Pos. 77–78) + `40` + `E6` (Pos. 80 = `W`) + 48 × `00` | FB |
| 8…26 | `C4` (`D`) + 79 × `40` + 48 × `00` | **F8 (gelöscht)** |

Bei Anlage einer Datei (HDR1 in Sektor 8, 9, …): Text nach §4.1, **normale Marke FB** für den aktiven Satz; alle anderen Etikettensektoren bleiben F8.
Reihenfolge der HDR1-Sektoren: in den Abbildern immer ab Sektor 8 aufsteigend ohne Lücken.
Systemdiskette: S. 1–4 durch den SYL-Lader ersetzen, `SYSARE..` als HDR1 mit `P` an Pos. 43, `000004` an 48–53, `999999` an 67–72 (§4.1).

Offen für das Anlegen: der Inhalt des Fehlspurkartensektors bei defekten Spuren (§3.1/§11).  Die Handbuch-Vorgabe „beide äußeren Spuren sind Reserve" erfordert
**keine** Sonderkennzeichnung in den Datenspuren (sie sind E5 wie alle anderen).

---

## 11. Offen / am Systemcode zu belegen

| # | Frage | Wo klären |
|---|-------|-----------|
| 1 | **ERMAP-Format**: wo trägt INIT die Nummern defekter Spuren ein (Pos. 6…?), Kodierung, Zusammenhang mit der Reserve „äußere zwei Spuren" | INIT-Objekt (`AADR 0295` DP1), Aufbau der Spur 0 am Schluss („zuletzt Spur 00 neu mit den Nummern defekter Spuren" [BH S. 153]) |
| 2 | VOL1 Pos. 11 (Zugriffskennzeichen), 72 (Seite), 76 (Satzlänge ≠ blank), 80 `W` — Wertebereich | FGEN („CHANGE VOL1", AADR 022D DP1; Text „`01-04 LABEL IDENTIFIER …`") |
| 3 | HDR1 Pos. 43 `P` (Schreibschutz?), 44 `E`, 45 `C`/`L`, 46–47, 48–53 `000004`, 54–57, 67–72 `999999` — genaue Semantik, wer wertet sie aus (SIOS-Kern oder nur INIT/FGEN) | FGEN-Anzeige der HDR1-Felder, SIOS-Dateizugriff |
| 4 | Wertet der SIOS-Kern die **gelöschte Datenmarke F8** aus oder nur den Text `HDR1`/`DDR1`? | Kern: Dateisuche (Etikettenlesen) |
| 5 | **Bibliothek:** Fuß-Feld `NV`; Fortsetzung des Verzeichnisses bei > 15 Einträgen (Sektor 3, …?) und Lage des Fußes dann; Kodierung **gelöschter** Einträge (Name = 20H-Blanks? 00H?) | LBSV (`AADR 0335` DP1): `DELET`, Verzeichnis-Schreiben |
| 6 | **ML**: genaue Bedeutung (Seitenzahl der Speicherlänge?) und Zusammenhang mit der abgelegten Länge `len ≥ 2·ML`; ob `AADR` immer die Position der **Datei** ist (ja), ob `len` im Verzeichnis fehlt (ja) | LBSV, `CAL` |
| 7 | **Phasenformat:** Bedeutung der Tokens `20/40/80–8F/04/08` (relozierbarer Strom?), Kopf `06 00 NAME …`, Lade-/Startadresse („absolute Phase mit Startadresse" [BH S. 12]), Prüfsumme | `CAL`/Lader im Systembereich (`SYSARE11`), `LINK` (DP2) |
| 8 | SYL-Kopf Byte 3/5 (`04`/`01`, `88H`/`6CH`/`74H`/`70H`): Länge in 256-B-Einheiten ab Spur 2 (stimmt für SYLs/SYLp numerisch) oder Tastaturvariante | SYL-Lader (Spur 0 S. 1–4), SGEN |
| 9 | SYSORG09: Bedeutung des Gruppen-/Varianten-Nibbles, der Satzköpfe `xx 8y/9y`, der Endwörter `58 00 5B 00`, ob das Verzeichnis auf Diskette 2 wiederholt wird | SGEN (`AADR 01D9` DP2) |
| 10 | **Folgediskette:** `EC01` / Folgenummer — wie SIOS die Fortsetzung sucht (VOL1-Name? Folgenummer 02?) | SIOS-Kern / SGEN |
| 11 | Satzlängen 256/512/1024 **physisch** (INIT-Menü 0…3), 8″-Varianten, 80-Spur-„Double Tracks" (in V 1.1 nicht möglich) — nicht an Abbildern gemessen | INIT |
| 12 | `SYSARE09` EOD 10009, aber nicht-E5-Daten bis Spur 10 Sektor 16 | SIOS-Schreibpfad (EOD-Fortschreibung) |
| 13 | Der `D`-Etikettensatz (nie benutzt) — erzeugt INIT genau diesen Satz (Pos. 1 `C4` + Blanks) oder ein anderer Prozess? | INIT |
| 15 | **D4 (2026-10-29), gewählte Deutungen, nicht am Systemcode belegt:** Bibliothek wird nur angehängt (Verzeichnisplatz = erster freier, NV = 0200H + 8·Plätze nur bis 14 Plätze), Löschen = Name 20H, ML = Sektoren/2, neue HDR1 mit `E` an Pos. 44 und Satzlänge 128, erster freier Etikettensektor, erste Spurlücke ab Spur 1 bis 37 | DiskTool-Schreibpfad; LBSV/FGEN gegenprüfen (Rundlauf im Emulator S6) |
| 14 | Mehrere Bibliotheken je Diskette (LBSV `NEWLB` Anlage 4.1/4.2) — Reihenfolge der HDR1-Sektoren ohne Lücken oder mit `DDR`-Lücken? | FGEN/LBSV-Etikettenlogik |

**Für die DiskTool-Stufen D2–D4 reicht der heutige Stand:** Erkennung über VOL1 (`VOL1` an Spur 0 Sektor 7 + 26 × 128 + ERMAP in Sektor 5), Auflisten
über HDR1, Bibliotheken über Verzeichnis + ND, `get`/`put` ganzer physischer Dateien, `SYSORG09` als Datei mit Verzeichnis (§8), Anlegen nach §10.

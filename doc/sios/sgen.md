# SIOS 1526/1.1: SYSD und SGEN (AP S7)

Stand 2026-10-30.  Belegt am laufenden System (`tests/integration/test_sios_sgen.cpp`, `SiosSgen.*`).
**Ergebnis in einem Satz:** SYSD und SGEN starten, SGEN führt bis zur Modulauswahl und nimmt sie an; die Erzeugung der
Systemdatei endet in `ERR FD 1 NFND` auf der Ausgabediskette — **S7 ist nur teilweise erreicht**, eine generierte
Systemdiskette entsteht noch nicht.  Ursache offen (Abschn. 5).

## 1. Bedienfallen (gelten für beide Programme)

1. **Fehlersperre beim Programmstart.**  SYSD, SGEN (und der zweite Bildschirm danach) starten mit gesetztem Bit 7 in `00CBH`
   (Fehlersperre); solange sie steht, verwirft SIOS **alle** Tasten (auch ET2).  Abhilfe: **CI (`0xAF`, Taste „RESET“ der
   Bildschirmtastatur) tippen**, danach ET1.  Bei SGEN steht die Sperre nicht (`00CB = 02`), SGEN nimmt ET1 sofort an.
2. **Veraltete Kopfspur** (`befund_can_nfnd.md` Befund 2): nach dem ersten Plattenzugriff eines Programms liest SIOS beim nächsten
   Zugriff an der alten Position und meldet `ERR FD 1 NFND`.  Abhilfe: die Diskette **neu einlegen** (`unmountDisk` + `mountDisk`),
   bevor das Programm weiterliest — bei SGEN vor dem Auswahlbild.
3. **Laufwerke.**  Beide Laufwerke **K5600.20** (80 Zylinder).  SIOS doppelschrittet 40-Spur-Disketten (DP1/DP2/SG1, mit INIT
   erzeugte Ziele) selbst; in einem K5600.10 liest es dann die Spur 2n statt n (`NFND`).  Nur **INIT** braucht das 40-Zylinder-Laufwerk.
   Die Systemdiskette (SYLs, 80 Zylinder, Spur 1–12 einfachschrittig) bleibt in A: = **Laufwerk 04**; B: = **05** (Kennung `66`).
4. Aufbau wie `bedienung.md` (Sicherheitsstecker 1).  Programm von Laufwerk B: per `ASN 0315 66` (SYSD von DP2) oder SG1 nach A: tauschen (SGEN).

## 2. SYSD (Anzeige der eingebundenen Module)

`CAL SYSD` (DP2 in B:, `ASN 0315 66`).  Bildschirm (V 1.5 — das Foto von robotrontechnik.de zeigt V 1.8, anderes System V 24):

```
                            SYSD 1520 (SIEX) V 1.5
        ================================================================
        LOAD SYSTEM-DISK DRIVE 04
        ================================================================
        ================================================================
                       START ->ET1 / MONITOR ->ET2
        ================================================================
```

Die Systemdiskette (SYLs) bleibt in A: (= Laufwerk 04).  **CI, dann ET1:**

```
        PRINT Y/N:                              (N = nicht drucken)
```

nach `N` ET1:

```
                            SYSD 1520 (SIEX) V 1.5
        ================================================================
        VOL-NAME: ROBINI    OPERATING SYSTEM VERSION 11
        ================================================================
        SYSTEM-LOADER                 BOE: 01001     EOE: 12026
        ================================================================
                       START ->ET1 / MONITOR ->ET2
        ================================================================
                                PROGRAM ==: ERR FD 1  NFND          92CB     D50
```

Das Foto der V 1.8 (anderes System, `OPERATING SYSTEM VERSION 24`, 33 K-BYTE) zeigt unter `SYSTEM-LOADER` die Modulliste, **die hier fehlt**:
`GRUND  T-K7637  BAB  BIN ARI  DEZ ARI  BCD ARI  SD 1-I  PH-K5601  LOG-FD2`.  Nach dem Kopf bricht SYSD V 1.5 mit `NFND` (Adresse 92CBH,
Gerät D50 = A:) ab, ohne einen Namen zu nennen — also kein Dateizugriff, sondern ein Sektor-/Etikettzugriff auf der Systemdiskette
(vorher liest es Spur 12 und 1; dann `FORMAT-COMMIT D0 C=11`-Warnung des K5122, die auch bei jedem `CAL` auftritt und folgenlos ist).
Mit SG1 statt DP2 als Programmdiskette und mit eingelegter (neu gemounteter) Systemdiskette dasselbe Bild.

**Was das für die Hardware sagt:** SYSD V 1.5 kennt die Modulnamen (im Programm: `GRUND T-K7637 T-K7606 T-K7636 T-K7633 BIN/DEZ/BCD ARI
SD … PH-FD320 PH-FD640 LOG-FD2 LOG-FD1 PHY KM LOG KM HLE 2/4 DFUE-A/I/V V 24 IFFS1-3 BSC… FUELLM`), die Liste von **SYSARE11** bleibt aber
unbekannt.  Gesichert: `OPERATING SYSTEM VERSION 11`, Systemlader Spur 1–12 (`BOE 01001 EOE 12026`), Disketten-Etikett `HDR1 SYSARE11 … P`.

## 3. SGEN (Systemgenerierung)

`CAL SGEN` mit **SG1 in A:** (SG1 trägt SGEN und `SYSORG09`; DP2 hat ebenfalls SGEN, aber kein `SYSORG`).  Der Dialog Schritt für Schritt
(Bildschirmtext wörtlich, Leerzeilen/Trennlinien `====` weggelassen):

**Schritt 1 — Version.**

```
SGEN 1520 (SIEX)  V 1.4
GENERATE OPERATING SYSTEM  VERSION:    6
```

Eingabefeld zwei Ziffern, Vorgabe **06**; SGEN sucht die Datei `SYSORG<Version>` auf dem Quelllaufwerk.  SG1 trägt nur **`SYSORG09`**:
`06` liefert `ERR FD 1 NFND SYSORG06 920E D50`.  **`09` tippen** (jede Ziffer überschreibt die Stelle und rückt weiter), ET1.
Die Systemzeile zeigt danach `OS V 09`.

**Schritt 2 — Menü.**

```
1 SYSTEM LOADER
2 SYS-COM-LOAD.
3 COPY SGEN AND FILE SYSORG TO CASSETTE
ET1 ->GO ON
ET2 ->MONITOR
```

`1` ET1.  (2 = Kommunikationslader, 3 = SGEN und `SYSORG` auf Kassette kopieren — nicht erprobt.)

**Schritt 3 — Ausgabegerät.**

```
 5
DA/OUTPUT-DEVICE
ET1 ->GO ON
ET2 ->MONITOR
SYSTEM LOADER                                         OS V 09
```

Vorgabe **5** = Laufwerk 05 (B:), die Zieldiskette.  Jetzt die Diskette in A: **neu einlegen** (Abschn. 1.2), dann ET1.  SGEN liest das
Verzeichnis von `SYSORG09` (Satz 0, Formatdokument §8.1).

**Schritt 4 — Auswahl (`SIGN YOUR WISHES`).**

```
  GRUND    06     T-K7637  04     T-K7606  03     T-K7636  03
  T-K7633  03     KUT      01     BAB      04     BIN ARI  01
  DEZ ARI  01     BCD ARI  02     SD 1156  02     SD 56-L  02
  SD 1-P   03     SD 1-I   04     SD 1-IZ  04     SD 2-P   03
  SD 2-PZ  03     SD 2-I   03     SD 2-IZ  03     SD 3-I   04
  SD 3-IZ  04     PH-FD320 04     PH-FD640 06     LOG FD2  05
  LOG FD1  04     PHY KM   04     LOG KM   04     LBE      01
  MB       01     SLE      04     HLE 2    01     HLE 4    01
  SIM      02     DFUE-A   03     DFUE-I   03     DFUE-V   03
  V24      02     IFSS1    02     IFSS2    02     IFSS3    02
  BSC7LZ   06     BSC7LV   06     BSC7CZ   06
SIGN YOUR WISHES
ET1 ->OUTPUT                    ET2 ->TO START
SYSTEM LOADER=DA/OUT 05                               OS V 09
```

Es sind die **43 Einträge der ersten Seite** des `SYSORG09`-Verzeichnisses (die Zahl = Sätze zu 1024 B; `HLE 2`… wie in §8.1);
`BSC7CV BSC8CZ BSC8CV FUELLM FIX` stehen auf der zweiten Seite.  **Bedienung:** vor jedem Eintrag steht eine Markierungsstelle; **ein
beliebiges Zeichen** (`X`, `1`, `+` getestet) nimmt den Eintrag auf und rückt zum nächsten, **Blank** übergeht ihn.  Nach dem 43.
Eintrag blättert die Anzeige (ET1).  Cursor-Rechts (`0x97`) hat die Markierung nicht verändert.

```
X GRUND    06   X T-K7637  04     T-K7606  03     T-K7636  03
```

**Schritt 5 — Ausgabe.**  ET1 zeigt die zweite Seite mit `***LISTING***` (Druckerliste), ein weiteres ET1 startet:

```
SGEN IS WORKING
```

Hier endet der gesicherte Teil (Abschn. 5).  Aus dem Programmtext (Speicherabzug) bekannt, aber **nicht erreicht**: Ausgabe
`VOL-NAME: … FILE 1 (SIMPLE) FILE "SYSARE09"  BOE: 01001  EOE: ??026  *MODUL* SEGM`, `SGEN FINISHED`, `ET1 ->REPEATING`, sowie
`LOAD OUT-FD: 26 SEC/128 BYTE PER TRACK` und `OUT-FD IS FULL: NEW FD OR '1'->ET1`.

## 4. Zieldiskette und Systemdatei

Das Ziel ist eine mit **INIT** formatierte 40-Spur-Diskette (`create`/INIT, K5600.10 beim Formatieren).  INIT legt eine Datei **`DATA`**
(BOE 01001 EOE 37026) über die ganze Diskette.  SGEN will die Systemdatei **`SYSARE09`** (BOE 01001, bei der Systemdiskette 12 Spuren
`EOE 12026`, Kennzeichen **P**: `HDR1 SYSARE09 00128 01001 12026   P    000004   999999  12013`) vorfinden bzw. neu anlegen.  Im Test wird sie
nach INIT mit dem DiskTool-Kern vorbelegt (`DATA` gelöscht, `SYSARE09` 12 Spuren + `DATA` Rest, Etikett wie bei SYLs/SYLp); das
`FGEN`-Programm (DP1) kann Etiketten ebenfalls anlegen (Menü `NEW HDR1`, `DELETE HDR1`, Foto `SIOS_FGEN_k.jpg`).  Das SGEN-Ergebnis
läge in Spur 0 (Lader, `SYL`) und Spur 1.. (`SYSARE09`) — die Ladervorlagen sind in `SYSORG09` Sätze 2–5 (Formatdokument §8.3).

## 5. Was fehlt: `ERR FD 1 NFND` auf der Ausgabediskette

Nach `SGEN IS WORKING` schreibt SGEN **ein** Sektor in Spur 0 des Ziels (Sektor 8: das Etikett `HDR1 SYSARE09` wird zu **`DDR1`**, also
gelöscht), liest dann sechsmal Spur 0 (Etiketten) und meldet

```
PROGRAM ==: ERR FD 1  NFND          9464     D66
```

(Gerät D66 = B: = Ausgabe; **kein Name** im Fehler, anders als bei `SYSORG06`).  Das Bild ist **unabhängig** von:

- der Auswahl (nur `GRUND`, 7 Module, keine) und der Zieldiskette (leer INIT-formatiert mit/ohne `SYSARE09`, mit/ohne `DATA`, Etikett `E`
  oder `P`, SYLp selbst als Ziel), der Laufwerksart von B: (K5600.10/.20) und davon, ob beide Disketten vorher neu eingelegt werden.

Verworfen: Kopfspur/Doppelschritt (die ID-Spuren stimmen, `CRC` ok), Programmquelle (Auswahllisten kommen aus `SYSORG09`, SG1 liest sich
vollständig).  **Offen:** welches Objekt SGEN nach dem Löschen des alten Etiketts sucht; SGEN ist nicht als Z80-Code abgelegt (die PHASELB-
Objekte sind im Speicher als eigenes Zwischencodeformat, Speicherabzug nach dem Fehler: `9464`-Umgebung ist keine Z80-Befehlsfolge), ein
Hinweis im Handbuch (`SIOS1526_Bedienhandbuch…`) fehlt — SGEN/SYSD/FGEN sind dort nicht beschrieben.  Vermutung: SGEN erwartet auf der
Ausgabediskette mehr als INIT liefert — etwa vorher mit `FGEN` angelegte Systemetiketten (`SYSARE09` samt Nachfolge-`DATA`), eine
Sondermarke im Volumeetikett (`VOL1` hat bei SYLs **kein** `M`, bei SYLp/SG1/DP1 und allen INIT-Disketten schon) oder die Etikettnummer
(`HDR1`-Sektor, hinter dem Systemetikett) — **nicht belegt**.  Nächster Schritt: den SVC-Aufruf, der 84H liefert, am `k1520dbg` mit
`wb`/`bt` ab `PROGRAM ==:` zurückverfolgen und die Anforderungsblöcke (`0100H`/`0104H`) mit dem Dateinamen mitschreiben; zweitens FGEN
auf der Zieldiskette von Hand (Menü, Etikett `SYSARE09` mit EOE) und SGEN danach noch einmal.

**Folge für S7:** Bestückungen ohne fertiges Abbild lassen sich mit SGEN noch nicht herstellen.  Die vorhandenen Abbilder (SYLs = 1.1/K7637,
SYLp = 0.9/K7636) bleiben die Systemdisketten.  Bei der Erkundung ist **kein Emulatorfehler** aufgefallen; die Auffälligkeiten (Fehlersperre,
veraltete Kopfspur, K5600.10/.20) sind Gast-/Aufbauverhalten.

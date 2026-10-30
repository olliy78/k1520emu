# SIOS 1526/1.1 im Emulator bedienen (AP S5/S6)

Stand 2026-10-30.  Belegt am laufenden System (k1520dbg, `tests/integration/test_sios_boot.cpp`
`SiosMonitor.*`).  Aufbau: A5120, Laufwerke K5600.20, K7637, **Sicherheitsstecker 1** (Chefschalter),
A: = `sios1526_syls.hfe` (Format `k5601_ss80_26x128`), B: = `sios1526_dp1.hfe` (Format `k5601_ss40_26x128`,
40-Spur-Diskette im 80er-Laufwerk, Doppelschritt automatisch).

## 1. Tasten

Die K7637 sendet physische Codes; SIOS 1.1 nimmt sie in der Rx-ISR `2D12H` entgegen (Zeichen < 20H
verworfen, 9xH -> Low-Nibble, Bx -> `AND 1FH`, alles andere unverändert).

| SIOS-Taste | K7637-Code | Emulator: Wirtstaste | Bildschirmtastatur | Beleg |
|---|---|---|---|---|
| Zeichen (Kommandos, Namen, Ziffern) | ASCII 20H-7EH | Tastatur | Tasten | `DAT`, `CAL` ... getippt |
| **ET1** (Eingabe abschließen) | FFH | Return | ET1 | `DAT` ET1 zeigt das Datum |
| **ET2** (Abbruch / Programm "MON.") | FEH | **Pause** (neu) | **Rechtsklick auf ET2** (neu); Linksklick bleibt Strg-Vorsatz | ET2 holt die Systemzeile zurück, in DIDI/INIT Rückkehr |
| **M** (Monitortaste) | B0H (-> 10H) | -- | Taste M (Reihe 0) | `(M)` -> `(P)` im laufenden DIDI |
| **CI** (Fehler löschen) | AFH = "RESET" der Bildschirmtastatur | -- | RESET (Fehlerlampe sitzt darüber) | [Code] `RES 7,(00CBH)` ist die Fehlersperre, `tastatur_bediensicherung.md` §2/§3; Handbuch: CI = Fehlerlöschung. Am Bildschirm nicht gegengeprüft |
| Cursor | 94H-97H (-> 4-7) | Pfeiltasten | Pfeile | [Code] ISR |
| S, S1-S9 | **[?]** vermutlich PF-Tasten C1H-CCH (Klasse >= C0H "Abschlusstaste", `2A2FH`/`2A42H`) | F1-F12 | PF 1-PF 12 | Zuordnung PF n <-> S n **nicht belegt** (keine Anwenderfoto-Beschriftung, Programm-abhängig) |
| Selektortasten | A0H-A3H | -- | 0 1 2 3 (Reihe 0) | Klasse A0H-A7H |
| INS MD / DEL CH | A8H / BBH | Entf = DEL CH | INS MD ... | **Achtung:** A8H ist für SIOS ein Sicherheitscode (Bit 0 von 00CBH kippt) -- INS MD auf der K7637.50 meiden |

Neu in dieser Arbeit (alles aus bekannten Codes, nichts erfunden):
- Wirtstaste **Pause** -> 0FEH (`K7637::QK_PAUSE`, `translateKey`; Wächter `K7637.KeyPress_Pause_Sends_ET2_0xFE`,
  `test_keyboard_map`).  Für CP/A ist 0FEH der Strg-Vorsatz (cp37); dort weiter Strg+Buchstabe nehmen.
- **Rechtsklick auf ET2** der Bildschirmtastatur sendet 0FEH allein, ohne Strg-Flag (Wächter
  `test_right_click_on_et2_sends_0xFE_alone`).  Der Linksklick ändert sich nicht.
- Kürzelregel unberührt: kein neues Tastenkürzel (Pause ist keine QAction).
- **k1520dbg:** `keys \xNN` sendete Codes >= 80H bisher als Qt-Taste (-> Code 00H, Taste verpuffte), nur < 20H
  ging roh.  Jetzt gehen 80H-FFH als Rohcode (`tools/k1520dbg.cpp`) -- sonst war weder ET1 (`\xFF`) noch M (`\xB0`) tippbar.

## 2. Getestete Abläufe (Bildschirmzeilen wörtlich, Leerstellen als Leerstellen)

Systemzeile (letzte Zeile, Spalte 32 ff.):

```
MONITOR ===== SIOS 1526/1.1 (M)_
```

**DAT / CLK** -- Kommando tippen, ET1; ET2 bricht ab:

```
DAT ET1   ->  MONITOR ===== SIOS 1526/1.1 (M)_DAT     00.00.00     (Datum noch nicht gesetzt)
ET1       ->  MONITOR ===== SIOS 1526/1.1 (M)_DAT_                 (Eingabefeld; ET2 = abbrechen)
CLK ET1   ->  MONITOR ===== SIOS 1526/1.1 (M)_CLK     00.00.09     (Sekunden zaehlen)
```

**ASN** -- Gerätezuweisung; Standard 0315 = Diskette 1 (physisch 50), `66` = Diskette 2 = **B:**:

```
ASN ET1   ->  ...(M)_ASN     _
0315 ET1  ->  ...(M)_ASN 0315_     50
66 ET1    ->  ...(M)_ASN 0316_     00     (naechste Zeile der Tabelle; ET2 beendet)
```

**CAL** -- Programm aus dem PHASELB des Programmlaufwerks laden und starten.  Ohne `ASN 0315 66` ist
Programmlaufwerk A: (SYLs enthält nur SYSARE11):

```
CAL ET1  CATM ET1  ->  MONITOR ===.ERR.FD.1 .NFND       (ERR FD 1 NFND: Programm nicht gefunden)
```

Mit `ASN 0315 66`, B: = DP1, `CAL DIDI`:

```
                            DIDI 1520 (SIEX) V 1.1
        ================================================================
         DEVICE            :                 ET1,ET2(MON.)
        ================================================================
         DEVICE            :                 4,5,6,7
         FORM OF DISPLAY   :                 A,H
         NAME OF FILE      :                 1-8 CHARACTERS
         RECORD NUMBER     :                 00001-65532,LAST=99999
         KEY POSITION      :                 1-9999
         KEY               :                 1-32 CHARACTERS
```

**M** im Programm -> Monitor mit geladenem Programm: `MONITOR ===== SIOS 1526/1.1 (P)_` (Bildschirm des
Programms bleibt stehen).  **ET2** im Programm -> Rückkehr `(M)`.  Auch geladen werden können CATM (Bandverwaltung
`CATM 1520(SIEX) V0.3`, Menü A-F, F = Monitor), DINH, ... (alle 15 Objekte von DP1).

## 3. Befunde / Offenes

1. **`CAN` nach `M` hält das System an.**  `(P)` + `CAN` ET1 leert den Bildschirm samt Systemzeile; ZVE1 landet in
   `1DEFH` (`DI`, Lampenport 0BH toggelt in Warteschleife `1E00H`).  Weg dorthin: SVC `90H` (`RST 08H` bei `0CD1H`) ->
   Klasse 10H -> `1D7EH` -> `1DEFH`.  Ob das Verhalten des echten SIOS ist (CANCEL als Havarie) oder ein
   Emulatorfehler, ist **ungeklärt**.
2. **Ein zweites `CAL` nach Programmende liefert `ERR FD 1 NFND`**, solange die Diskette nicht neu eingelegt wird
   (`mount` im Debugger heilt es).  Beim zweiten `CAL` liest SIOS dauernd dieselbe Spur (Zylinder 58, Wiederholung)
   statt das Verzeichnis (Spur 1) anzufahren, der Kopf steht noch dort, wo das erste Programm geladen wurde.  Verdacht:
   SIOS hält die Kopfposition selbst (Bezug nach Programmlauf zurückgesetzt) und der Doppelschritt-Abbildung der
   40-Spur-DP1 im 80er-Laufwerk; **ungeklärt**.  Test-Umgehung: je Programm ein frischer Start.
3. **INIT** (siehe unten).

## 4. INIT (Leerdiskette initialisieren) -- Stand: läuft durch (2026-10-30)

**Seit 2026-10-30 läuft INIT bis `INITIAL DISK IS FINISHED`** (Wächter `SiosInit.InitialisiertDiskette`;
B: = K5600.10 für die 40-Spur-Zieldiskette).  Ursachen waren zwei K5122-Fehler (Altbyte nach dem Schreiben,
/STR beendet Schreiben) und ein Aufbaufehler (40-Spur-Medium im 80er-Laufwerk = Halbspuren), siehe
`befund_init_schreiben.md`.  Der folgende Abschnitt ist der historische Befund.

Bedienung (INIT 1520 (SIEX) **V 1.4**, nicht 1.1 wie im Handbuch), Programm von DP1 in A:, Leerdiskette in B: (= 05):

```
CAL ET1  INIT ET1                       "8" DISKETTE   Y"   ->  N ET1
(Menue 5,25" DISKETTE: 1 DOUBLE DENSITY)                    ->  ET1 (=1)
STATEMENTS: 1 PHYSICAL RECORD LENGTH (0=128) ET1,  2 SEQUENCE (01) ET1,
            3/4 DESTROYED TRACK (leer) ET1, ET1,  5 INIT ALL (Y) ET1
```

Das Programm schreibt (`INITIAL DISK IS WORKING`, ZVE2-Formatstrom am Datenport 14H) und meldet dann
`ERR 9C`; mit "INIT ALL = N" (nur Spur 00): `ERR 9B  > 2 DEFEKTIV TRACKS : 00, .., ..`.  Befund:

- Spur 00 wird als gültige MFM-Spur geschrieben (26 Sektoren zu 128 B, IDs 1-26, CRC gut -- `disk verify`/`info`),
  die **Rückleseprüfung von INIT hält die Spur trotzdem für defekt**.
- INIT formatiert mit voller IBM-Spur (80 x 4E, 12 x 00, **3 x C2 + FC = Indexmarke**, 50 x 4E, Sektoren) und liest zur
  Prüfung mit eigener ZVE2-Routine (A3xxH-A5xxH: `IN A,(16H)` je Byte, `CP FCH` bei `A30EH`, ID-Lesen per `INIR` ab
  `A517H`, `CP 42H` bei `A524H`).  Die Sektorkennung (A1 FE 00 00 01 00) kommt richtig an.
- **Berichtigt (befund_init.md):** kein Indexmarken-Problem; INIT schreibt hier keine Indexmarke.  Der Hang kommt von A1-Versatz im Kontrolllesen und dem OUT-14H-Pacing.  `N` erst nach dem Erscheinen von `8" DISKETTE` und ca. 20 Mio. Takten tippen, sonst formatiert INIT als 8" (FM).
- (alter Verdacht, widerlegt) die Verify-Routine erwartet die **Indexmarke (C2 C2 C2 FC)** am Spuranfang; der A5120-Lesestrom
  (`buildFaithfulReadTrack`) enthält keine Indexmarke und beginnt mit dem ersten A1 des ersten Sektors
  (`K5122::startReadTransfer`, "enge Verify-Resync-Routine" von SCPX-INIT).  Nicht bestätigt: der Haltepunkt
  `b2 A30E` wurde auf Spur 0 nie erreicht, der Abbruch kommt auf einem anderen Pfad (`A54FH`).  **Offen**, nächster Schritt:
  ZVE2-Pfad ab `A4ABH`/`A517H` bis zum Fehlersprung (`A5F6H`/`A5F9H`) verfolgen.
- Rundlauf SIOS <-> DiskTool (`create --fs sios_ss40`, `put`, `ls`) ist daher noch **nicht** gefahren.

SYSD/SGEN (Fehlersperre, Version `09`, Auswahlbild): siehe `sgen.md`.

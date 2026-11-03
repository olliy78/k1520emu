# Merkposten MicroDOS 1526 (A5120/A5130) — Stand 2026-10-31

Rostocker Anpassung von MicroDOS (CP/M-2.2-kompatibel, Vers. 2.6 vom 01.03.87) auf dem A5120-Kern.
**Kein Emulatorfehler gefunden, kein Kernpatch noetig.** Handbuch: `~/projects/robotron/Diskettensammlung_von_DL/Diskettenarbeit/Doc/MicroDOS/`
(`mdosb20.txt`, `msysgen.txt`, `mkey.txt`, latin1).  Waechter: `tests/python/test_microdos_ablauf.py` (`py_microdos_ablauf`),
Skript: `tools/microdos/ablauf.py`, Bau der Auslieferung: `tools/microdos/build.py`.  **Originale** der Sammlung: `tools/microdos/original/MicroDOS1526{s,p}[_5600].hfe`
(byteglich zu `Disketten_Images_RT/...`; `robotron/A5120/discs/MicroDOS1526.hfe` ist bytegleich zur seriellen K5601-Fassung).  **Ausgeliefert**
(`disks/a5120_microdos1526/`) werden seit 2026-10-31 die GEBAUTEN Disketten (siehe Abschnitt Auslieferung).

## Zuordnung Abbild -> Bestueckung

| Abbild | Tastatur (`--tastatur`) | Laufwerke | Bild |
|---|---|---|---|
| `..._k5601_seriell` (MicroDOS1526s) | **k7637** | Vorgabe 4 x K5601 | K7024 |
| `..._k5601_parallel` (…p) | **k7636** | Vorgabe | K7024 |
| `..._k5600_10_seriell` (…s_5600) | k7637 | **K5600.10** (40 Spur, einseitig-Format 5x1024) | K7024 |
| `..._k5600_10_parallel` (…p_5600) | k7636 | K5600.10 | K7024 |

Lade-ROM: die Vorgabe (V0.9_2) genuegt, `--laderom` nicht noetig.  Ein 5600-Abbild im Vorgabe-Laufwerk K5601 oder K5600.20 bleibt bei den
Punkten (Ladeschirm) stehen; die falsche Tastatur (parallel + K7637) bootet, aber der Schirm fuellt sich mit `A>` (Tastaturpuffer-Rauschen),
(seriell + K7636) bootet ebenfalls, die Eingabe geht dort nicht durch.  Das K7636-`keys` des Debuggers kennt kein Return (`MKEY20D?`); im
Python-Weg (Qt-Return) geht es.

## Befund je Punkt

1. **Kaltstart: bestanden** (alle vier Abbilder, ~110 M Maschinentakte).  Banner `IIPL/ISW`, `48K MicroDOS Vers 2.6 01.03.87`.  Danach laeuft
   `INITIAL.SUB` (MKEY20-Hinweis) und bleibt bei `A>MKEY20` stehen: **ein Return** bringt den echten Prompt `A>`.
2. **Bedienung: bestanden.** `D`, `D B:`, `E x` (fragt `delete - <Y/N>`), `B:`/`A:` Laufwerkswechsel, `DIP` (Hilfeschirm, kopiert),
   `KTP`, `S`/`U`/`O`.  Fallen: **`PIP.COM` der Sammlung ist ABGESCHNITTEN - Gastdatei, kein Emulatorfehler** (2026-10-31 geklaert): das Verzeichnis
   fuehrt 32 Saetze (RC=20H, zwei 2K-Bloecke = 4096 B); es ist DR PIP 1.5 fuer CP/M 2.0 (`COPYRIGHT 1979 ... PIP VERS 1.5`), dessen Code aber bei
   `CALL 1A40H`/`1D84H`/`1ECCH` (Daten bis 1F6FH) ueber das Dateiende 1100H hinaus laeuft; ein vollstaendiges Abbild waere rund 7,9 KB.
   Folge: Lauf ins Leere (`04CE`-Neustarts, PC streut ueber 0100-1B12), keine Ausgabe.  Nicht suchen; DIP ist der Ersatz und kopiert fehlerfrei.
   Die ausgelieferten Disketten tragen deshalb kein `PIP.COM`.
   DIP mit **einem** Zielnamen (`DIP B:=A:X.COM`) kopiert und wartet danach auf `Load System Disk on Drive A:, then Type (cr)` — Gastverhalten,
   Return druecken.  `D B:` auf unformatierter B: -> `Bdos error on B: … ST=49 T=0 S=1  ignore - <Y/N>` (richtig, `N` tippen).
3. **FORMAT20: bestanden** auf echter Leerdiskette (unformatiert, Geometrie K5601).  Ablauf: `FORMAT20`, Laufwerk `B` (**EINE Taste ohne Return** -- ein Return danach nimmt die Vorgabe 3 an; die ausgelieferten Disketten sind mit dieser Tastenfolge entstanden, `ablauf.formatiere` behaelt sie fuer Format 3 bei), Formatziffer (3 = 5 x 1024 B,
   80 Zyl., 2 Seiten; 1 = 16x256, 2 = 9x512) **mit Return**, `Richtig (J/N)` = `J`, ca. 2 Mio. Takte je Spur (~200 M gesamt), `Formatierung
   beendet! Wiederholung (J/N)?` = `N`.  Danach `D B:` = `no files`.
4. **MSYSG20: bestanden**, Bootdiskette bootet kalt als A:.  Menuefolge (jeweils Ziffer/Buchstabe OHNE Return): Tastatur `4` (K7637.51; 3 = K7636.01),
   Bildschirm `2` (K7024), Drucker `1` (IFSS; 2 V.24, 3 PIO), Typenrad `1` (PICA), Zusatzkanal `3` (V.24 K8025), je Laufwerk: logisch `A`,
   physisch `1`, Typ `3` (MFS 1.6 = K5601; 2 = MFS 1.4 K5600.20, 1 = MFS 1.2 K5600.10 (nur dies ergibt das 200K-System)), Format `8` (5 x 1024, 80 Zyl., 2 Seiten; Liste haengt
   vom Typ ab), `Wiederholung (J/N)` `J`/`N` (nach `N` beim zweiten Laufwerk B; dort 2, 3, 8); dann Zielfrage `B`, `Richtig (J/N)` `J`.
   Es folgt das Generierungsprotokoll (Tabelle Tastatur/Bildschirm/Drucker/Laufwerke A-D) mit `Wiederholung (J/N)` -> `N`.  Ergebnis: B:
   enthaelt nur `MicroDOS.SYS` (+ SYL-Urlader in Spur 0/Sektor 1, kein Systemspuren-Bereich).
   Nach dem Schreiben **`flush_disks()`** (Schreibpause ~0,5 s Maschinenzeit abwarten), erst dann ist die `.hfe` vollstaendig.
   Die neue Diskette hat **kein INITIAL.SUB** -> sofort `A>` ohne MKEY-Hinweis; `FORMAT20.COM`, `DIP` usw. gehoeren nicht dazu (mit DIP/PIP
   nachkopieren).
5. **Dienstprogramme/Kommandos: siehe Abschnitt "Bedienung der Programme" (2026-10-31).**

## Auslieferung (2026-10-31)

`tools/microdos/build.py` (`--check` = Waechter) baut je Variante `a5120_microdos26_<k5601|k5600_10>_<seriell|parallel>.hfe`: Sitzung auf der SERIELLEN
Quelle (K7637; die "parallele" Originaldiskette kennt in FORMAT20 nur 40 Spuren einseitig - `ablauf.bauen(ziel_tastatur=...)` beantwortet MSYSG20 trotzdem
mit K7636), FORMAT20 Format 3, MSYSG20 (K5601: Typ 3/Format 8; K5600.10: Typ 1/Format **4**, 5 x 1024 B 40 Spuren), danach per DiskTool die Beigaben der
Originaldiskette (ohne `INITIAL.SUB`, `PIP.COM`, `MicroDOS.SYS`, WordStar/MailMerge, Nutzerbereich 1).  Zwei Fallen beim Bau:
- Das **Drucker-BIOS der 5600-Quelle wartet bei MSYSG20s Protokoll endlos auf CTS** (Schleife `IN 51H / BIT 5` bei E79BH) -> `Sitzung` legt Schnittstelle 0 auf
  eine Datei (`os.devnull`; Datei meldet VERBUNDEN).  Loop hilft nicht.
- FORMAT20 bietet nur, was das BIOS der **Quelle** fuer B: kennt; die Geometrie der Leerdiskette folgt dem Laufwerk des Emulators.
Der Bau ist deterministisch (zweimal gebaut = gleiche Bytes); `build.py --check` laeuft im Waechter `py_microdos_ablauf`.  Seit 2026-11-03 gehoeren alle vier
zur Vorgabeauswahl des Pakets (`DISKS_DEFAULT`, Freigabe des Anwenders; Waechter `test_microdos1526_disketten_sind_in_der_vorgabeauswahl`).

## Bootabbilder des DiskTools (2026-10-31, Waechter `py_microdos_bootabbilder`)

`data/bootsektoren/boot_microdos_*.bin` (Lader 256 B + MicroDOS.SYS 16 KB) sind Auszuege der Originalabzuege (`boot-get`); eine damit per
`create --fs ... --boot` gebaute und per `put` befuellte Diskette **bootet bis `A>`** (gemessen, 5 Abbilder).  Das System ist FEST auf
Tastatur und Laufwerke generiert (MSYSG20-Tabellen in `MicroDOS.SYS`; die Abbilder unterscheiden sich nur dort -- die Lader sind in allen vier
1526-Abbildern bytegleich):

| Abbild | Profil | Tastatur | Laufwerke (Emulator) | aus |
|---|---|---|---|---|
| `boot_microdos_1526.bin` | microdos800 | K7637 seriell | Vorgabe 4 x K5601 | MicroDOS1526s |
| `boot_microdos_1526_k7636.bin` | microdos800 | K7636 parallel | Vorgabe 4 x K5601 | MicroDOS1526p |
| `boot_microdos_1526_200k.bin` | microdos200 | K7637 | **2 x K5600.10** | MicroDOS1526s_5600 |
| `boot_microdos_1526_200k_k7636.bin` | microdos200 | K7636 | 2 x K5600.10 | MicroDOS1526p_5600 |
| `boot_microdos_1715.bin` | microdos1715 | (PC 1715) | K5601, 2K-Bildschirm K7222 | MicroDOS1715 |

- **200K = K5600.10, nicht K5600.20** (frueher stand in `data/bootsektor.yaml` und Doku K5600.20 -- falsch).  Gemessen: mit K5600.20 oder dem
  Vorgabe-K5601 bleibt das 200K-System bei den Ladepunkten stehen (kein Banner nach 300 M Takten); mit 2 x K5600.10 bootet es.  Passt zur
  Geometrie: 40 Spuren x 5 x 1024 B einseitig = 200 KB = `k5601_ss40_5x1024`, MFS 1.2 (= K5600.10) in der Generierung.
- Beschreibungen im Dialog "Neue Diskette" nennen Tastatur und Laufwerke; die K7636-Abbilder sind seit 2026-10-31 dabei (vorher nur K7637).
- Der erste Treffer je Profil (Vorwahl) ist die K7637-Fassung.

## Pruefung / Rettung / Archiv (2026-10-31)

- **`cpm.dir.geloescht`** auf den Originalen ("1 freier Platz traegt noch einen lesbaren Eintrag"): ECHTER Rest, aber nicht wiederherstellbar --
  Platz 93 der `MicroDOS1526s` ist `WSNEW.COM`, geloescht, mit 0 Saetzen und lauter 0-Zeigern (ein geloeschter leerer Eintrag); `recover`
  findet dort nichts.  Die Meldung zaehlt jetzt nur Reste mit mindestens einem brauchbaren Blockzeiger (dieselbe Regel wie `recover`); die
  Originale pruefen "ohne Befund".  Wächter `DisktoolMicrodos.GeloeschtHinweisNurBeiWiederherstellbaremRest`.
- **`cpm.system.ort`** (Warnung): `MicroDOS.SYS` (1526) nicht an den Bloecken Zylinder 1-4 Kopf 0 Sektor 1-4 bzw. `MICRODOS.SYS` (1715) nicht in den ersten
  Datenbloecken -- die Diskette bootet nicht.  **`cpm.system.fehlt`** (Warnung): 1526-Diskette mit SYL-Lader ohne `MicroDOS.SYS`.  Keine Reparatur
  (es gaebe nur "neu schreiben"; `boot-put` ist der Weg).  Keine Meldung auf den vier 1526- und der 1715-Originaldiskette, den gebauten Disketten
  und Datendisketten (`microdos*_daten`).  Wächter `DisktoolMicrodos.PruefungMeldetSystemdateiAmFalschenOrt`.
- **Rettung** (`recover`, `--restore`): fuer 800/200/1715 getestet -- geloeschte Datei wird unter ihrem Namen gefunden (Guete sicher), Inhalt bitgleich,
  `--restore` traegt sie ein, danach fsck sauber, Lader + Systemdatei byteweise unveraendert; auch die Suchtiefe Oberflaeche bietet
  Lader/System nie als Fund an (`DisktoolMicrodos.RettungFindetGeloeschteDateienUndLaesstSystemUnberuehrt`, GUI: `py_disktool_microdos`).
- **Archiv** (`archive.py`): ZIP = Abbild + `dateien/` + `cpm-dateiangaben.txt` (Nutzerbereich, R/O, SYS, ARC) + Inventar.  Das Beiblatt fuehrt nur
  Dateien MIT Angaben (daher steht `MicroDOS.SYS` des 1526 nicht darin, `MICRODOS.SYS` des 1715 mit `S`).  Rueckweg: `dateien/` per `insert_all` auf eine
  neue Systemdiskette (ohne die schon vorhandene Systemdatei) stellt Namen, Nutzerbereiche und Attribute bitgleich wieder her.  Getestet fuer alle drei
  Profile (`py_disktool_microdos::test_archiv_*`).
- Fallstrick im Test: das DiskTool uebertraegt `.txt/.dat/.doc/...` standardmaessig als TEXT (LF -> CR LF); Bytemuster-Tests brauchen andere Endungen.

## Nachspielen

```sh
tools/dev.sh build
LD_LIBRARY_PATH=build venv/bin/python3 -I tools/microdos/ablauf.py /pfad/ziel [--tastatur k7636] [--quelle ABBILD.hfe] [--ohne-probe]
# Auslieferung neu bauen: LD_LIBRARY_PATH=build venv/bin/python3 -I tools/microdos/build.py --tool build/k1520disktool
# -> /pfad/ziel/microdos26_boot_k7637.hfe   (~6 s)
```
Das Skript legt das Original nie schreibend ein (Temp-Kopie), B: entsteht mit `create_disk(1, pfad, "")` (Leerdiskette).  Tasten: Qt-Return
`0x01000004`, 150 000 Takte gedrueckt + 600 000 Pause (K7637 = 9600 Baud).

## Fallen

- Bei Debugger-Sitzungen `keys \r` auf K7637; bei K7636 braucht der Debugger-`keys` einen Rohcode.
- MSYSG20 und FORMAT20 lesen die Taste ohne Return; die Formatwahl in FORMAT20 aber **mit** Return.
- Die Generierung nennt Laufwerk C/D als `MFS 1.2/1` (Vorgabe), solange man sie nicht eingestellt hat — unschaedlich.
- Offen: siehe Abschnitt "Offen" am Ende.

## Bedienung der Programme (2026-10-31, Waechter `py_microdos_bedienung`, `py_microdos_generierungen`, `py_microdos_hardware`, `py_microdos_format20`)

Alles am echten Gast bedient (Tasten, Bildschirmtext), Wirkung wie im Handbuch geprueft -- bestanden, ausser den Zeilen unter "Offen".
- **SELDSK20**: Antworten Laufwerk/Einheit/Typ/Format je EINE Taste; falsches Format auf B: -> `Bdos error on B:` `ST= 56` (ID-Feld), das
  richtige (Typ 3, Format 8) liest wieder.  Typmenue: MFS 1.2/1.4/1.6/MF 3200/MF 6400; die Formatliste haengt vom Typ ab (MF 3200: 2 Eintraege).
- **Resident**: `O B:2` verschiebt das Verzeichnis (`D B:` = no files), `O B:0` zurueck; `O` zeigt alle vier; `S B:` -> Programme mit
  Systemattribut von B: sind von A: aus startbar (`KTL?` vorher); `U 3` -> Prompt `3A>`; `<CMD.SUB` fuehrt aus (`;`-Zeile wird nur
  gezeigt; **`<CMD` ohne Typ -> `CMD?`**, der Name ist voll anzugeben); `^S` haelt an, `^Q` setzt fort (in POWER `TYPE`); `^D` in `MZSID`
  -> `>A>`, `D` laeuft, Rueckkehr in `>`; `^P` -> Ausgabe zusaetzlich an den Drucker (nochmal `^P` schaltet ab); `^C` Warmstart.
- **POWER** 3.03 (Pavel Breder; Prompt `A=`, Befehlsliste mit `?`): `DIR`, `COPY` (fragt `destination drive :`), `ERA` (`ERASE (Y/N)`), `EXIT`.
- **MKEY20.SPR** (`MKEY20`): PF10 -> `DBASE`, PF11 -> `POWER`, `M` -> `o d:1` laut Datei `MKEY20`; Tasten ueber `RAW_BASE | Code`
  (`0x02000000`).  **MKINST20** mit `<MYKEY` (Eingabedatei, erste Zeile `MKINST20`) erzeugt eine neue `MKEY20.SPR` (vorher `E MKEY20.SPR`, `Y`).
- **HARDCOPY**: `A`, `B`, `03`, Return, `J` kopiert Spuren 0-2 (Verzeichnis von B: ist danach das von A:); `Wiederholung (J/N)`.
- **KTL** zeigt logische Codes (`b` = 62, Return = 0D, Ende bei `a`); **KTP** liest Port 01H/06H (U-Bus, K7636): `b` = 62, Return = `FF` (ET1);
  an der K7637 nur `FF`.  **`MATH.COM` ist der Force-Two-WordStar-Installer, kein Rechner.**
- **SELPRT20**: Schnittstelle (1 IFSS, 2 V.24, 3 PIO), Typenrad, `Drucker bereit (ENTER)`, Test 20H-7FH (96 Zeichen), `Richtig (J/N)`.
  **Wege: Drucker IFSS -> Hub-Schnittstelle 2 `Drucker` (X3, SIO A32-A, E/A 5CH); Drucker V.24 -> 0 `DFUE/V.24` (X6).
  Drucker PIO -> E/A 54H..57H = PIO A31 der K8025 (DIL-Schalter-Register): NICHT MODELLIERT** -- der Treiber laeuft durch, nichts erreicht
  die Aussenwelt.  `SELAUX20`-Menue: IFSS (ASS K 8025), IFSP (ALB K 6025), V.24 (ASS K 8025).
- **Zusatzkanal** (Testprogramm `AUXT.COM` = BDOS 4 'X', BDOS 3, BDOS 2): V.24 -> DFUE/V.24, sendet UND empfaengt (Schleife: `X` kommt zurueck);
  **IFSP -> OUT 95H / IN 91H: ALB K6025 (E/A 90H..97H) nicht modelliert**; **IFSS -> OUT 5CH = SIO A32 Kanal A (Stecker X4)** -- GEKLAERT 2026-11-01:
  mit der **K7637** ist das der Tastaturkanal (das X geht an die Tastatur, nichts nach aussen, am Geraet ebenso; der Drucker-IFSS liegt auf Kanal B,
  5EH, und geht); mit der **K7636** ist X4 frei, die Maschine fuehrt A32-A dann als **vierten Anschluss "IFSS Zusatzkanal" (X4)** heraus
  (`A5120Machine`, `K8025::ZifssA32A` wie am PRG 710; Index 3, ein AUP-Sonderanschluss rutscht auf 4).  Waechter
  `test_zusatzkanal_ifss_sendet_auf_a32_kanal_a_...`, `..._mit_k7636_geht_an_den_freien_stecker_x4`.
- **Generierungen** (FORMAT20 + MSYSG20 auf Leerdiskette, ~10 s): Drucker V.24/PIO, Zusatzkanal V.24/IFSP/IFSS (oben); **Bildschirm K7023
  (`1`, Option `bildschirm=k7023`)** bootet; **Tastatur K7636** bootet mit `tastatur=k7636`; **K7634.54/.51**: wird generiert, der Emulator hat
  am A5120 keine K7634 (nur K7637/K7636) -> Schirm voller `A>`, **nicht modelliert**.
- **128-Byte-Systemdiskette (DiskTool-Profil `microdos128`, 2026-11-01)** (msysgen Anhang B): mit K5600.10-Laufwerken `FORMAT20` Eintrag 1 (26x128x40x1), `SELDSK20` B auf Typ 1/Format 1 (sonst
  `Schreibfehler !` in MSYSG20: das laufende System schreibt mit dem alten Format), `MSYSG20` Typ 1/Format 1 fuer A und B -> bootet mit Lade-ROM
  V0.6 **und** V0.9_2; mit den K5601-Vorgabelaufwerken bootet sie nicht.  DiskTool: Profil **`microdos128`**
  (`k5601_ss40_26x128`, Block 1 KB, 64 Plaetze, Erkennung am SYL-Lader 0.6 mit Text "MicroDOS Loader"; Spur 0 Sektor 1-2 = Lader/Plaetze 0-7, Verzeichnis ab Sektor 1,
  MicroDOS.SYS in den Bloecken 3-17 und **19** -- MSYSG20 laesst Block 18 aus, das System liegt aber in den linearen Sektoren 26..155 = Bloecke 3-19; darum sperrt
  `CpmFileSystem::allocationMap` 3-19, sonst koennte `put` das System ueberschreiben).  ls/get/put/rm/fsck/boot-get gehen (Diskette bootet nach `put`);
  **Bootabbild schreiben geht seit 2026-11-03** (`boot_microdos_1526_128.bin` = 256 B Lader + 16 384 B Datei; Lage am Ladercode belegt, Entwurf 13 §25.4: BEIDE Lader --
  V0.9 in den Plaetzen 0-7 und SYL 0.6 auf Zylinder 1 -- laden Zylinder 1-5 Kopf 0 ganz nach B900H und springen nach BD00H; `writeSystemDatei128` traegt Zeiger 3-17 und 19 ein
  wie MSYSG20 und fuellt Block 18 mit Nullen; `cpm.system.ort` gilt auch hier; Lader der anderen Sektorlaenge werden abgewiesen).  Waechter
  `test_systemdiskette_mit_128_byte_sektoren_bootet_...` (DiskTool-Teil), `DisktoolMicrodos.Systemdiskette128BootabbildRundlaufUndFremdeLader`,
  `test_128_byte_bootabbild_bootet_mit_beiden_lade_roms`, `FsCatalog.ProfilnamenSindEinStabilerVertrag`.
- **FORMAT20-Menues** (alle 14 Eintraege formatieren, Gast liest zurueck): MFS 1.2: 26x128 / 16x256 / 5x1024 (je x40x1); MFS 1.4: 16x256 / 9x512 /
  5x1024 (x80x1); MFS 1.6: 16x256 / 9x512 / 5x1024 (x80x2); MF 3200: 26x128 / [Eintrag 2 leer] / 4x1024 (x77x1); MF 6400: 26x128 / 4x1024 / 8x1024
  (x77x1).  **MF 6400 Eintrag 3 (8x1024x77): `Defekte Spuren !` -- GEKLAERT 2026-11-01:** Spur 1-76 sind in MFM sauber (je 8 Sektoren, CRC ok, 167 936 Zellen = eine
  Umdrehung); FORMAT20 formatiert **Spur 0 immer in FM** (die Eintraege 1 und 2 sind ganz FM), hier 8 x 1024 B = 139 264 Zellen > 83 968 Zellen einer 8"-FM-Umdrehung
  (360/min, 500 kZellen/s) -- die Spur passt nicht auf den Umlauf (4 x 1024 FM = 68 160 passt).  Am Geraet ebenso zu erwarten (der Schreibstrom
  ueberliefe den Index).  Gastverhalten, kein Emulatorfehler.  Waechter `test_format20_menueeintrag[MF6400-Eintrag3]` (Spurlaengen aus der HFE).
  DiskTool erkennt leere Formate nur mehrdeutig (`cpa_auto`), `16x256x80x2` -> `scpx640`, `5x1024x80x2` -> `cpa800`.

## Offen (2026-11-01)
- **Ausgabeumlenkung `>DATEI` -- GEKLAERT:** der Puffer wird erst beim Schliessen geschrieben.  Schliessen: `^C` (BDOS-Zeileneingabe wertet es nur am **Zeilenanfang**
  als Warmstart -- nach einem `^F` am Prompt steht schon ein Zeichen in der Zeile, daher blieb die Datei 0 Byte), nacktes `>` (legt zusaetzlich einen Platz mit leerem
  Namen an, `cpm.dir.name`), oder **`^S` dann `^F`** (BDOS-Code CA21/CA36: `^F` wird nur in der Warteschleife nach `^S` ausgewertet; am Prompt macht die Zeileneingabe `^F`).
  Handbuch 4.2/4.8 sagt nichts davon.  Waechter `test_ausgabeumlenkung_bis_ctrl_c_...`, `test_ctrl_s_ctrl_f_schliesst_...`.
- PIO-Drucker und IFSP ohne Gegenstelle, K7634 fehlt (bewusst nicht nachgebaut).
- **MSYSG15B** (1715 mit K5600/MFS 1.2): nur im Handbuch genannt.  Durchsuchung aller `.hfe/.img/.dmk/.td0/.cqm` und Zip-Inhalte unter `~/projects/robotron`
  (1286 Abbilder, u. a. GOTEK-Sticks, `mpm-kc85_de/.../MicroDOS_sys.zip`, `Disketten_Images_RT`) per `k1520disktool ls`: **kein `MSYSG15B`**; `MSYSG15A` steht
  nur auf `MicroDOS1715.hfe` und `MicroDOS1715_LW_C.hfe`; die `MSYSG.COM` der KC85-Abbilder gehoeren zu MicroDOS fuer KC85 (andere Hardware).

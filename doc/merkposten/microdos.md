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
3. **FORMAT20: bestanden** auf echter Leerdiskette (unformatiert, Geometrie K5601).  Ablauf: `FORMAT20`, Laufwerk `B`, Format `3` (= 5 x 1024 B,
   80 Zyl., 2 Seiten; 1 = 16x256, 2 = 9x512) **mit Return**, `Richtig (J/N)` = `J`, ca. 2 Mio. Takte je Spur (~200 M gesamt), `Formatierung
   beendet! Wiederholung (J/N)?` = `N`.  Danach `D B:` = `no files`.
4. **MSYSG20: bestanden**, Bootdiskette bootet kalt als A:.  Menuefolge (jeweils Ziffer/Buchstabe OHNE Return): Tastatur `4` (K7637.51; 3 = K7636.01),
   Bildschirm `2` (K7024), Drucker `1` (IFSS; 2 V.24, 3 PIO), Typenrad `1` (PICA), Zusatzkanal `3` (V.24 K8025), je Laufwerk: logisch `A`,
   physisch `1`, Typ `3` (MFS 1.6 = K5601; 2 = MFS 1.4 K5600.20, 1 = MFS 1.2 K5600.10), Format `8` (5 x 1024, 80 Zyl., 2 Seiten; Liste haengt
   vom Typ ab), `Wiederholung (J/N)` `J`/`N` (nach `N` beim zweiten Laufwerk B; dort 2, 3, 8); dann Zielfrage `B`, `Richtig (J/N)` `J`.
   Es folgt das Generierungsprotokoll (Tabelle Tastatur/Bildschirm/Drucker/Laufwerke A-D) mit `Wiederholung (J/N)` -> `N`.  Ergebnis: B:
   enthaelt nur `MicroDOS.SYS` (+ SYL-Urlader in Spur 0/Sektor 1, kein Systemspuren-Bereich).
   Nach dem Schreiben **`flush_disks()`** (Schreibpause ~0,5 s Maschinenzeit abwarten), erst dann ist die `.hfe` vollstaendig.
   Die neue Diskette hat **kein INITIAL.SUB** -> sofort `A>` ohne MKEY-Hinweis; `FORMAT20.COM`, `DIP` usw. gehoeren nicht dazu (mit DIP/PIP
   nachkopieren).
5. **Stichproben:** `MKINST20`, `SELDSK20`, `SELPRT20`, `SELAUX20`, `MSYSG20`: nur MSYSG20/FORMAT20 durchgespielt (oben); `SELDSK20` etc. nicht.
   `DIP`, `KTP`, `D`, `E` ok.

## Auslieferung (2026-10-31)

`tools/microdos/build.py` (`--check` = Waechter) baut je Variante `a5120_microdos26_<k5601|k5600_10>_<seriell|parallel>.hfe`: Sitzung auf der SERIELLEN
Quelle (K7637; die "parallele" Originaldiskette kennt in FORMAT20 nur 40 Spuren einseitig - `ablauf.bauen(ziel_tastatur=...)` beantwortet MSYSG20 trotzdem
mit K7636), FORMAT20 Format 3, MSYSG20 (K5601: Typ 3/Format 8; K5600.10: Typ 1/Format **4**, 5 x 1024 B 40 Spuren), danach per DiskTool die Beigaben der
Originaldiskette (ohne `INITIAL.SUB`, `PIP.COM`, `MicroDOS.SYS`, WordStar/MailMerge, Nutzerbereich 1).  Zwei Fallen beim Bau:
- Das **Drucker-BIOS der 5600-Quelle wartet bei MSYSG20s Protokoll endlos auf CTS** (Schleife `IN 51H / BIT 5` bei E79BH) -> `Sitzung` legt Schnittstelle 0 auf
  eine Datei (`os.devnull`; Datei meldet VERBUNDEN).  Loop hilft nicht.
- FORMAT20 bietet nur, was das BIOS der **Quelle** fuer B: kennt; die Geometrie der Leerdiskette folgt dem Laufwerk des Emulators.
Der Bau ist deterministisch (zweimal gebaut = gleiche Bytes); `build.py --check` laeuft im Waechter `py_microdos_ablauf`.  Im Paket nur mit `--disks all`
(wie SIOS; `DISKS_DEFAULT` ist eine feste Auswahl, `py_packaging` prueft sie).

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
- Offen: SELDSK20/SELPRT20/SELAUX20/MKINST20 nicht durchgespielt, Drucker (IFSS/V.24/PIO) nicht getestet.

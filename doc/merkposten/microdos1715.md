# Merkposten MicroDOS 1715 (PC 1715) — Stand 2026-10-31

Rostocker MicroDOS 2.6 (01.05.86, CP/M-2.2-kompatibel) fuer den PC 1715, Gegenstueck zu `microdos.md` (1526).
**Kein Emulatorfehler gefunden, kein Kernpatch am Emulator noetig** (Kern geaendert wurde nur das DiskTool-Dateisystem, s. 5).
Handbuecher: `~/projects/robotron/Diskettensammlung_von_DL/Diskettenarbeit/Doc/MicroDOS/` (`mdosb15.txt`, `msysgen.txt`, `mkey.txt`, latin1).
Wächter: `tests/python/test_microdos1715_ablauf.py` (`py_microdos1715_ablauf`), `cli_microdos1715`
(`tools/microdos/build1715.py --check`), `DisktoolMicrodos.*` (`tests/integration/test_disktool_microdos.cpp`).
Skripte: `tools/microdos/ablauf1715.py` (Leerdiskette -> FORMAT15 -> MSYSG15A -> Bootdiskette), `tools/microdos/build1715.py`
(Auslieferungsdiskette), Inhalt `tools/microdos/inhalt1715/`, Diskette `disks/pc1715_microdos/pc1715_microdos_k5601_system.hfe`.

## Zuordnung Abbild -> Bestueckung

| Abbild (Sammlung `Disketten_Images_RT/`) | Modell | Laufwerke | Bild |
|---|---|---|---|
| `MicroDOS1715.hfe` (= `pc1715_microdos_k5601_system.hfe` ohne LIESMICH, byteweise gleiche Programme) | **PC 1715** (`pc1715`) | Vorgabe 4 x K5601 (80 Spuren, 2 Seiten, 5 x 1024) | K7222 (80 x 24), `MSYSG15A`-Bildschirmwahl `2` = 2K |
| `MicroDOS1715_LW_C.hfe` | bootet bis zum Banner, dann **`Bdos error on A: disk error`** | — | — |

- Lade-ROM/Urlader S502: Vorgabe, nichts einzustellen.  Format `cpa800`-Geometrie, Dateisystem `microdos1715`.
- **MSYSG15B: nicht aufzutreiben** (alle Abbilder der lokalen Quellen durchsucht, s. `microdos.md` Offen); MSYSG15A am K5600.10 nicht verfolgt.
- **K5601 vs. K5600:** MicroDOS 1715 nennt `MFS 1.6` (K5601 u. ae., 80 Spuren) und `MFS 1.2` (K5600, 40 Spuren einseitig).  Vorhanden ist
  nur **`MSYSG15A`** (MFS 1.6); `MSYSG15B` (MFS 1.2) liegt in der Sammlung nicht vor -> **K5600-Generierung ungeprueft**.
- **PC 1715W: bootet nicht** (Bild bleibt leer) — Ursache nicht untersucht; MicroDOS 1715 kennt die 1715W-Hardware (U8272/DMA) nicht.
- **K7221 (64 x 16):** die Diskette ist fuer 2K (80 Spalten) erzeugt; auf dem K7221 laeuft sie, der Text bricht bei Spalte 64 um
  (Banner „…Ver/s 2.6“).  **GELOEST (2026-10-31):** `MSYSG15A` Bildschirm `1` (Generierung in der K7222-Sitzung) erzeugt ein System, das am Modell `pc1715-k7221` Banner, `D`-Listen und die 62 Zeichen breiten Menues von `SELDSK15` ohne Umbruch zeigt; die 2K-Originaldiskette bricht dort weiter um.  Waechter `py_microdos1715_bedienung`.
- **LW_C (Klaerung 2026-10-31):** Die leere Tabelle an Offset 0C0H..0D0H ist NICHT die Ursache -- jede selbst mit MSYSG15A erzeugte Diskette hat dort Nullen und bootet.  Die Konfiguration (Offset 27A9H..) weicht ab: Laufwerk A mit 80 Spuren/DSM 399, Einheitennummern A/B vertauscht, Typ-Bytes 'C' statt 'G'.  Beim Start scheitert BDOS-Funktion 13 (Login A:): `Bdos error on A: disk error O=13 PC=BEEEH ST=52 T=0 S=1` (Laufwerk nicht bereit), unabhaengig davon, in welches der vier Laufwerke eine zweite Diskette gelegt wird (alle Kombinationen probiert).  Zwischenstand/Diskette fuer ein anderes Laufwerksgefuege; **nicht lauffaehig, nicht ausgeliefert**.  Frueher notiert: die Laufwerkstabelle in `MICRODOS.SYS` (Offset 193…209: `02 10 05 01 02 11 05 01 07 …`) ist dort **null**, `$$$.SUB` liegt
  noch auf der Diskette (SUBMIT-Rest), `FORMAT15.COM` (12288 B) und `SELDSK15.COM` (1152 B) sind andere Fassungen, 60 Byte `MICRODOS.SYS`
  weichen ab.  A: bleibt ohne Format -> jeder Zugriff „disk error“ (ST=52).  Mit der Diskette auch in Laufwerk 3/4 geht es nicht.
  Wahrscheinlich ein Zwischenstand einer Generierung; **nicht ausgeliefert**.  `PC1715_SCP7_LW_C_als_A.hfe` wurde nicht benutzt.

## Befund je Punkt

1. **Kaltstart: bestanden.**  Banner `IIPL/ISW`, `48K MicroDOS Vers 2.6 01.05.86`, ~40 M Maschinentakte.  `INITIAL.SUB` ist **eine leere Zeile**
   -> der Prompt `A>` erscheint zweimal (kein MKEY-Hinweis wie am 1526).
2. **Bedienung: bestanden.**  `D`, `D B:` (auf unformatierter B: `Bdos error on B:????????.??? disk error  ST= 52  T= 0  S= 1  ignore - Y<CR>?`),
   `B:`/`A:` Laufwerkswechsel, `E B:NAME` (fragt `delete - Y<CR>`; `Y` + Return), **Ausgabeumlenkung `>B:DIR.TXT` … `>`** (legt die Datei an;
   DiskTool liest sie, `fsck` ohne Befund).  Auf dem 1715 gibt es **kein DIP und kein PIP**; Kopieren ginge mit `POWER`.
3. **FORMAT15: bestanden** auf echter Leerdiskette (Geometrie K5601).  Ablauf (alles **EINE Taste ohne Return**, ausser `FORMAT15`+Return):
   `FORMAT15` -> `PLEASE ENTER DRIVE:` `B` -> Formatmenue 0-5 -> `PLEASE SELECT FORMAT:` **`5`** (= DD-DS 5 x 1024 x 80; 0-2 einseitig, 3-5
   doppelseitig; 0/3 = 16x256, 1/4 = 9x512, 2/5 = 5x1024) -> `ALL FILES WILL BE SCRATCHED (Y/N)` **`Y`** -> `WAIT FORMATTING TRACK ( 80 )  ===>
   FORMATTING COMPLETE` (~1,3 Mrd. Takte).  Danach fragt FORMAT15 wieder `PLEASE ENTER DRIVE` und **hat keinen Ausgang** (das Handbuch kennt
   noch die Frage „Wiederholung Y/N“).  Danach `D B:` = `no files`.
4. **MSYSG15A: bestanden**, Bootdiskette bootet kalt als A:.  Menuefolge (Ziffern/Buchstaben OHNE Return): Bildschirm `2` (2K = K7222; `1` = 1K =
   K7221), Drucker `2` (IFSS; `1` = V.24), je Laufwerk `Drive:` logisch `A`, `Unit:` `1`, `Format:` **`4`** (Liste 1 16x256x40x1, 2 5x1024x40x1,
   3 16x256x80x2, **4 5x1024x80x2**, 5-7 = 8″), `Wiederholung (J/N)` `J`/`N` (A und B: `A 1 4`, `J`, `B 2 4`, `N`), Zielfrage `B`.  **Kein „Richtig (J/N)“**:
   das Generierungsprotokoll (Bildschirm/Drucker/Laufwerke A-D, nicht eingestellte = `0`) erscheint, sobald `MICRODOS.SYS` auf B: steht;
   `Wiederholung (J/N)` dazu = `N`.  Ergebnis: B: traegt **nur `MICRODOS.SYS`** (14 336 B, SYS-Attribut) + Bootkopf in den Verzeichnisplaetzen 0-3
   (**byteweise gleich** `boot_microdos_1715.bin`, 128 B); `MICRODOS.SYS` weicht von der der Originaldiskette in nur 7 Byte (Konfiguration) ab.
   Nach dem Schreiben `flush_disks()`.  Es gibt **kein INITIAL.SUB, kein FORMAT15** auf der neuen Diskette.
5. **DiskTool gegen die Emulator-Diskette: bestanden.**  Erkennung `microdos1715`, `ls`/`get`/`put`/`rm` (auch eine im Emulator geschriebene
   `DIR.TXT`, im Emulator per `E` geloeschte Datei, per DiskTool eingefuegte Datei im Emulator sichtbar), `fsck --full` ohne Befund, Archiv
   (`python3 -m app.disktool.archive`) enthaelt Abbild, Inhaltsverzeichnis, `dateien/…` und `cpm-dateiangaben.txt` (`MICRODOS.SYS attr=S`).
   **Neu:** `create --fs microdos1715 --boot data/bootsektoren/boot_microdos_1715.bin` legt jetzt **Kopf + `MICRODOS.SYS`** an (das Bootabbild ist
   14 464 B = 128 + 14 336); `boot-get` gibt es bitgleich zurueck, `boot-put` schreibt beides, `systemspurBytes` = 128 + 16 384.
   Kern: `CpmFileSystem::writeSystemDatei1715` (erste Datenbloecke gleich hinter dem Verzeichnis, SYS-Bit an der Endung), `istSystemDateiName`.
   Eine nur 128 B grosse Bootdatei (alter Stand) geht weiter; die Diskette wird erst mit `MICRODOS.SYS` erkannt (positiver Nachweis).
6. **Kaltstart von der neu erstellten Diskette: bestanden** — von `ablauf1715.py` (Emulator-gebaut) **und** von `create --boot` + `put`.

## Nachspielen

```sh
tools/dev.sh build
LD_LIBRARY_PATH=build venv/bin/python3 -I tools/microdos/ablauf1715.py /pfad/ziel [--bedienprobe] [--ohne-probe] [--quelle ABBILD.hfe]
# -> /pfad/ziel/microdos1715_boot_k5601.hfe   (~20 s)
python3 tools/microdos/build1715.py --tool build/k1520disktool [--check]    # Auslieferungsdiskette
```
Tasten: ASCII, Return `0x01000004`, Strg+Buchstabe ueber `key_press(code, False, True)`; 150 000 Takte gedrueckt + 300 000 Pause.

## Fallen

- **Nach FORMAT15 (auf B:) nicht Strg+C, sondern RESET.**  Strg+C -> `Bdos error on A:$$$.SUB disk error` (ST=52), A: ist danach „leer“.
  Befund am Portverkehr: FORMAT15 schreibt `20H=DD`/`21H=DD` (nur Motor B an), am Ende `20H=FF`; das MicroDOS-BIOS schreibt das Motorregister
  21H **nie** (waehlt nur ueber 20H, Timeout `OR F0H`).  A: laeuft nach dem Format ohne Motor.  Ein Neustart (`emu.reset()`) stellt 21H zurueck
  (`Sitzung.neustart()`; B: bleibt liegen, vorher `flush_disks`).  **Gastverhalten, nicht im `K5122` suchen**; an echter Hardware nicht
  nachgemessen (Annahme: dort gleich, weil `MO`-Register = Motor, s. `pc1715.md`).  **Beleg verschaerft (2026-10-31):** `MICRODOS.SYS` hat genau EIN echtes `OUT (21H),A` (Offset 55H im Bootkopf: `AF D3 20 D3 21` = beide Ports := 0; weitere `D3 21`-Bytefolgen sind Operanden anderer Befehle); im Lauf (`iow 0x21` bei Boot, `D`, `D B:`, `POWER`) gibt es nur drei Schreibzugriffe, alle beim Laden (Lade-ROM PC 056CH: 00H, FFH; Bootkopf PC BD57H: 00H), danach nie.  FORMAT15 schreibt 20H/21H bei Offset 262EH/2630H.  Gleiches wie bei SCP-INIT (~2,5 s BIOS-Abschaltzeit).
- **`MICRODOS.SYS` muss in die ersten Datenbloecke** (der Urlader liest sie von dort; Bloecke 2…8 bei 2048-B-Block, 128 Verzeichnisplaetze).  Eine per
  `put` NACH anderen Dateien eingefuegte `MICRODOS.SYS` liegt woanders und die Diskette bootet **nicht** (Bild leer).  Deshalb nur ueber
  `create --boot`/`boot-put` bzw. als erste Datei einer leeren Diskette; `put` auf ein vorhandenes `MICRODOS.SYS` wird verweigert
  („existiert bereits“) — kein stilles Zerbrechen.  Offen: `fsck`-Hinweis, wenn `MICRODOS.SYS` nicht vorne liegt.
- Der Bootkopf allein (ohne `MICRODOS.SYS`) ist `cpa1715` (gleicher Kopf wie CP/A 1715); `put` braucht dann `--fs microdos1715`.
- `FORMAT15`: der Bildschirm bleibt nach `COMPLETE` stehen; `bis('COMPLETE')` genuegt, nicht auf einen Prompt warten.
- Der Banner steht im 64-Spalten-Bild gebrochen (K7221); Suchtexte dort kuerzer waehlen.
- Offen: `MSYSG15B`/K5600.00 nicht durchgespielt; `SELDSK15`/`SELPRT`/`OC15`/`KTP15`/`POWER`/`MKEYINST` nur aufgelistet, nicht bedient; Drucker (IFSS/V.24)
  nicht getestet; `KERMIT.COM` (Kermit-80) nicht auf der Diskette, ungeprueft unter MicroDOS; 1715W; K7221-Generierung.

## Bedienung der Programme (2026-10-31, Waechter `py_microdos1715_bedienung`, `py_microdos1715_format15`, `py_microdos1715_format15b`)
- `SELDSK15`: Laufwerk/Einheit/Typ (1 MFS 1.2 .. 5 MF 6400)/Format je EINE Taste; Liste fuer MFS 1.6: 1 26x128x40, 2 16x256x40, 3 5x1024x40, 4 16x256x80x1, 5 5x1024x80x1,
  6 16x256x80x2, 7 5x1024x80x2.  Falsches Format -> `Bdos error on B: ST= 56`; **nach einem falschen Zugriff laesst sich B: im selben Lauf nicht mehr auf 7
  zurueckstellen (ST=56 bis zum Neustart)** -- Ursache offen.  **Mit einem einseitigen 80-Spur-Format (Liste 4/5) haengt `D B:`** (> 1 Mrd. Takte keine Antwort,
  auch auf einer mit FORMAT15 Eintrag 0/2 passend formatierten Diskette); doppelseitig (6/7) laeuft.
- `KTP15`: `b` -> `E062`, Return -> `E09E`, Ende bei `a`.  `K6311SEL`: Zeichensatz (1 ASCII, 2 deutsch) und Breite (1-3) je MIT Return; sendet `ESC R 2`, `ESC [ 0 SP K` an den V.24-Drucker.
- `MKEYINST` (Vers. 2.0): `<DATEI` mit erster Zeile `MKEYINST` erzeugt `MKEY.SPR`; die Wirkung der Belegung nicht geprueft.  `OC15`: `System und OC passen nicht zusammen !`
  (auch auf der Originaldiskette).  `HARDCOPY`: Maske `Source:`/Ziel/Spuren, Strg+C beendet.  `POWER` wie am 1526 (Prompt `A=`).  `SELPRT`: bleibt nach der Typenradziffer
  (mit/ohne Return) ohne Druckertest stehen, CPU im BDOS-Console-In (HALT) -- **offen**.
- **Drucker**: V.24 (`MSYSG15A` Drucker `1`, DTR-Protokoll) -> Schnittstelle 0 `Drucker` (X4): `^P` und `K6311SEL` kommen an.  **IFSS (Drucker `2`): Treiber nutzt E/A 14H/16H =
  Zusatzkarte (2 x IFSS/V.24, Entwurf 21: nicht gebaut) -> NICHT MODELLIERT**; `D` mit `^P` laeuft, es geht nichts hinaus.  Zusatzkanal V.24 nicht getestet.
- **FORMAT15-Menue** 0-5 (`PLEASE SELECT FORMAT`): alle sechs formatieren (`FORMATTING COMPLETE`, ~10 s); DiskTool: Eintrag 3 `scpx640`, 5 `cpa800`, die anderen mehrdeutig (`cpa_auto`).

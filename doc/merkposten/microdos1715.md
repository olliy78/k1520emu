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
- **K5601 vs. K5600:** MicroDOS 1715 nennt `MFS 1.6` (K5601 u. ae., 80 Spuren) und `MFS 1.2` (K5600, 40 Spuren einseitig).  Vorhanden ist
  nur **`MSYSG15A`** (MFS 1.6); `MSYSG15B` (MFS 1.2) liegt in der Sammlung nicht vor -> **K5600-Generierung ungeprueft**.
- **PC 1715W: bootet nicht** (Bild bleibt leer) — Ursache nicht untersucht; MicroDOS 1715 kennt die 1715W-Hardware (U8272/DMA) nicht.
- **K7221 (64 x 16):** die Diskette ist fuer 2K (80 Spalten) erzeugt; auf dem K7221 laeuft sie, der Text bricht bei Spalte 64 um
  (Banner „…Ver/s 2.6“).  Eine 1K-Generierung (`MSYSG15A` Bildschirm `1`) ist nicht durchgespielt.
- **LW_C:** die Laufwerkstabelle in `MICRODOS.SYS` (Offset 193…209: `02 10 05 01 02 11 05 01 07 …`) ist dort **null**, `$$$.SUB` liegt
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
  nachgemessen (Annahme: dort gleich, weil `MO`-Register = Motor, s. `pc1715.md`).  Gleiches wie bei SCP-INIT (~2,5 s BIOS-Abschaltzeit).
- **`MICRODOS.SYS` muss in die ersten Datenbloecke** (der Urlader liest sie von dort; Bloecke 2…8 bei 2048-B-Block, 128 Verzeichnisplaetze).  Eine per
  `put` NACH anderen Dateien eingefuegte `MICRODOS.SYS` liegt woanders und die Diskette bootet **nicht** (Bild leer).  Deshalb nur ueber
  `create --boot`/`boot-put` bzw. als erste Datei einer leeren Diskette; `put` auf ein vorhandenes `MICRODOS.SYS` wird verweigert
  („existiert bereits“) — kein stilles Zerbrechen.  Offen: `fsck`-Hinweis, wenn `MICRODOS.SYS` nicht vorne liegt.
- Der Bootkopf allein (ohne `MICRODOS.SYS`) ist `cpa1715` (gleicher Kopf wie CP/A 1715); `put` braucht dann `--fs microdos1715`.
- `FORMAT15`: der Bildschirm bleibt nach `COMPLETE` stehen; `bis('COMPLETE')` genuegt, nicht auf einen Prompt warten.
- Der Banner steht im 64-Spalten-Bild gebrochen (K7221); Suchtexte dort kuerzer waehlen.
- Offen: `MSYSG15B`/K5600.00 nicht durchgespielt; `SELDSK15`/`SELPRT`/`OC15`/`KTP15`/`POWER`/`MKEYINST` nur aufgelistet, nicht bedient; Drucker (IFSS/V.24)
  nicht getestet; `KERMIT.COM` (Kermit-80) nicht auf der Diskette, ungeprueft unter MicroDOS; 1715W; K7221-Generierung.

## Handbuch-Entwurf (fuer `app/help/handbuch.md`, noch nicht eingefuegt)

> **MicroDOS 1715 (PC 1715).**  Die Diskette `pc1715_microdos_k5601_system.hfe` startet MicroDOS 2.6 im PC-1715-Emulator (Modell PC 1715,
> Bildschirm K7222).  Nach dem Kaltstart erscheint zweimal `A>`.  `D` zeigt das Verzeichnis, `B:` wechselt das Laufwerk, `E NAME` loescht,
> `>B:DATEI` leitet die Ausgabe in eine Datei um (`>` beendet das).
> *Neue Systemdiskette:* eine **leere Diskette** in Laufwerk B: einlegen (*Leere Diskette*), `FORMAT15` starten, Laufwerk `B` und Format `5`
> (eine Taste, ohne Return) waehlen, die Warnung mit `Y` bestaetigen und `FORMATTING COMPLETE` abwarten.  FORMAT15 hat keinen Ausgang:
> **die Maschine neu starten** (nicht Strg+C).  Danach `MSYSG15A`: Bildschirm `2`, Drucker `2`, Laufwerk `A` Einheit `1` Format `4`, `J`, Laufwerk `B`
> Einheit `2` Format `4`, `N`, Ziel `B`.  Auf B: steht danach `MICRODOS.SYS`; die Diskette bootet allein.  Das DiskTool kennt das Dateisystem als
> `microdos1715`; mit *Neue Diskette ▸ MicroDOS 1715 (mit Bootabbild)* entsteht sie auch ohne Emulator, Dateien kommen danach mit *Einfuegen*.

## CLAUDE.md-Absatz-Entwurf (kurz, fuer „Vierte Maschine: PC 1715“)

> **MicroDOS 1715** (2026-10-31): Rostocker MicroDOS 2.6 bootet am PC 1715 (K7222, 4 × K5601); `FORMAT15` + `MSYSG15A` bauen aus einer Leerdiskette eine
> bootende Systemdiskette (`tools/microdos/ablauf1715.py`, Auslieferung `disks/pc1715_microdos/` aus `tools/microdos/build1715.py`).  DiskTool-Profil
> `microdos1715`: Bootabbild = 128-B-Kopf + `MICRODOS.SYS` (muss in die ersten Datenbloecke).  **Nach FORMAT15 RESET, nicht Strg+C.**  1715W bootet nicht.
> Vor Arbeiten daran: `doc/merkposten/microdos1715.md` lesen.

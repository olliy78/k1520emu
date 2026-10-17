# 34 — OS/M-Disketten (P8000, 8-Bit-Teil)

Stand 2026-10-17, Zweig `cqm`.  Reine Analyse und Plan — im Kern ist noch nichts geändert.
Bezug: `doc/design/33_cqm.md` (Container), `doc/merkposten/disktool.md`, `doc/merkposten/cqm.md`,
Sammlung `~/projects/robotron/P8000/discs/` (pofo.de), Übersicht
`~/projects/robotron/P8000/archive/UEBERSICHT.md`.

## 1. Frage und Kurzantwort

pofo.de führt 13 Abbilder mit der Format-Angabe „640K/OSM“ bzw. „800K/OSM“.  Das DiskTool
erkennt sie als `scpx640` (640K) bzw. `cpa_auto` (800K).  Ist OS/M ein eigener
Dateisystemtyp?

**Nein.**  OS/M ist CP/M 2.2 (eigenes CCP/BDOS/BIOS der ZFT/KEAW, Schnittstelle „voll
kompatibel mit CP/M 2.2“) und benutzt auf der Diskette **genau die SCP-Formate des PC 1715**:
gleiche Geometrie, gleicher DPB, kein Sektorversatz, keine Sonderplätze im Verzeichnis.  Die
pofo-Angabe „OSM“ ist eine Herkunftsangabe, kein Format.  Alle 15 untersuchten Abbilder werden
**heute schon vollständig und richtig gelesen** (Namen, Größen, Inhalte gegen einen
unabhängigen Leser geprüft, §4).  Belegt durch das BIOS-Listing auf der Systemdiskette selbst
(`BIOS.L`, OS/M 3.0, 15.12.87, §3) und dadurch, dass OS/M 3.0 von `osm30.cqm` und
`osmspiel.cqm` im p8000emu bis zum Prompt `A:` startet (§5).

Was fehlt, ist klein und liegt **neben** dem Dateisystem:

1. **Ein Name** für OS/M-Disketten (Anzeige, `create --fs`, Bootabbild-Prüfung) — heute
   heißt eine OS/M-Systemdiskette „scpx640“ oder „cpa_auto“.
2. **Die leere 800K-OS/M-Diskette** wird als `cpa800` gelesen (Verzeichnis ab Zylinder 0,
   192 Plätze); OS/M sucht es ab Zylinder 2.  Wer sie mit dem DiskTool füllt, schreibt in die
   Systemspuren — OS/M sieht nichts (§4.3, nachgestellt).
3. **Format 6 (9×512, 720K)** ist mit der CP/A-Regel überhaupt nicht lesbar (dort 1
   Verzeichnisplatz) — ein OS/M-Format, für das es noch kein Abbild gibt.
4. **Nebenbefund, unabhängig von OS/M:** `k1520disktool get <abbild> <muster>` bricht bei
   CP/M-Namen mit `/` ab und bildet `/Z/HOFFM` als **absoluten Pfad** — ein Ausbruch aus dem
   Zielordner (§4.4).  `extractAll` (ohne Muster) und das Archiv ersetzen `/` bereits.

## 2. Bestand: welche Abbilder „OSM“ sind

Geprüft: alle 151 `.cqm` (inkl. der vollständigen `*.crdownload`, je als Kopie umbenannt) mit
`k1520disktool info`; Kandidat ist alles, was nicht `tar720`/`wega720`/`udos1715`/FAT ist.
Dazu Kopf jedes CQM (Geometrie, Interleave/Skew, Kommentar) und die Systemspuren roh.

| Abbild | pofo | CQM-Geometrie | heute erkannt | tatsächlich | Systemspuren | Dateien | Befunde (`check`) |
|---|---|---|---|---|---|---|---|
| osm30 | 640K/OSM | 80×2×16×256 | scpx640 | OS/M-Format 5, Systemdiskette | OS/M 3.0, FDCONF 55 | 32 | – |
| osmsys | 640K/OSM | 80×2×16×256 | scpx640 | OS/M-Format 5, Systemdiskette | OS/M 3.0, FDCONF 55 | 33 | – |
| osm_org | 640K/OSM | 80×2×16×256 | scpx640 | OS/M-Format 5, Systemdiskette | OS/M 3.0, FDCONF 55 | 36 | – |
| p8000dok | 640K/OSM | 80×2×16×256 | scpx640 | Format 5, Datendiskette | leer (E5) | 13 | 1 Warnung `cpm.dir.name` (`AEND/DOC`) |
| wegrem23 | 640K/OSM | 80×2×16×256 | scpx640 | Format 5, Datendiskette | leer | 17 | – |
| rempc172 | 640K/OSM | 80×2×16×256 | scpx640 | Format 5, Datendiskette | leer | 7 | 1 Hinweis (2 gelöschte Plätze) |
| c-beschr | 640K/OSM | 80×2×16×256 | scpx640 | **PC 1715 SCPX** (Bootkopf `03 F0`, „SCPX V0/2“), gleiches Format | SCPX 1715 | 19 | 6 Warnungen `cpm.dir.name` (`/Z/…`), 1 Hinweis |
| osmspiel | 800K/OSM | 80×2×5×1024 | cpa_auto | OS/M-Format 7, Systemdiskette | OS/M 3.0 + BS1715, FDCONF 77 | 34 | – |
| osmut | 800K/OSM | 80×2×5×1024 | cpa_auto | OS/M-Format 7, Systemdiskette | OS/M 3.0, FDCONF 77 | 32 | 2 Warnungen `cpm.dir.name` (`GW/ZSID.DOC`) |
| bsosm3 | 800K/OSM | 80×2×5×1024 | cpa_auto | OS/M-Format 7, Systemdiskette | OS/M 3.0 + BS1715, FDCONF 77 | 81 | 1 Hinweis (`$$$.SUB` gelöscht) |
| osmdb | 800K/OSM | 80×2×5×1024 | cpa_auto | OS/M-Format 7, Systemdiskette | OS/M 3.0 + BS1715, FDCONF 77 | 40 | 1 Hinweis |
| osmredab | 800K/OSM | 80×2×5×1024 | cpa_auto | OS/M-Format 7, Systemdiskette | OS/M 3.0 + BS1715, FDCONF 77 | 86 | 1 Hinweis (6 gelöschte Plätze) |
| rempc171 | 800K/OSM | 80×2×5×1024 | cpa_auto | **PC 1715 SCP** (Bootkopf `03 F0`, „CCP/BDOS-V0/4 … Sömmerda“) = `scp1715` | SCP 1715 | 9 | – |
| ingdbd1 | unknown | 80×2×16×256 | scpx640 | Format 5, Datendiskette (Herkunft offen) | leer | 3 | – |
| ingdbd23 | unknown | 80×2×16×256 | scpx640 | Format 5, Datendiskette (Herkunft offen) | leer | 4 | – |

- Alle CQM-Köpfe: Interleave 0, Skew 0, erste Sektor-ID 1, kein Kommentar, Etikett
  `** NONE **`; Datenstrom vollständig (655 360 bzw. 819 200 B).  `rempc172` und `wegrem23`
  liegen nur als `*.crdownload` vor, sind aber vollständig (`doc/merkposten/cqm.md` Nr. 12).
- **Weitere Treffer:** keine mit OS/M-System.  `ingdbd1`/`ingdbd23` (pofo „unknown“) tragen
  dasselbe Format 5 und sind darum hier aufgeführt; ob sie unter OS/M oder SCP entstanden,
  sagt das Medium nicht.  `wegrem33` (WEGA-Remote 3/3) ist 360K FAT (MS-DOS 3.1), `wdos_dcp`
  720K FAT, `konsul2` ein tar-Folgeteil (`cpa_auto` mit 0 Dateien ist dort eine bekannte
  Fehldeutung, `UEBERSICHT.md`).  Die übrigen 720K-Abbilder sind tar/WEGA, die 640K-UDOS-Abbilder
  NDOS.  Das in den pofo-Etiketten genannte `osmstart.cqm` gibt es in der Sammlung nicht.
- **pofo irrt zweimal:** `c-beschr` und `rempc171` sind PC-1715-Disketten (SCPX bzw. SCP 1715 —
  „P8000/PC1715 remote“ ist die PC-Seite von WEGA-Remote), nicht OS/M.  Physisch ist das
  gleichgültig — es sind dieselben Formate.

## 3. Das Format, belegt am BIOS-Listing

Quelle: `BIOS.L` auf `osm30` (Listing `OSM.URLADER.S` + BIOS, „ASM 5.9“, P. Hoge, Version 3.0
vom 15.12.87) und `README` derselben Diskette (Version 3.0, 15.01.1988).  Beide mit dem
DiskTool geholt.  Die Quelldateien selbst (`OSM.URLADER.S`, `OSM.FORMAT.S`, `OSM.SYSGEN.S`, …)
nennt das Inhaltsverzeichnis `github_P8000/inh.p8000.doc` §4 auf einer Diskette „OS/M“, die
nicht vorliegt.

### 3.1 Formattabelle (`TYPTAB`, U8272) und DPBs

Der Laufwerkstyp steht als Nibble in `FDCONF` (BIOS F04EH; A: = Bits 0–3, B: = 4–7, …,
Vorgabe `0055H`), umschaltbar mit `SETFD`.

| Typ | Physik (TYPTAB) | DPB | SPT | BSH/BLS | EXM | DSM | DRM | AL0 | OFF | Bemerkung |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 8″ FM 77×26×128 | DPBLK1 | 26 | 3 / 1K | 0 | 242 | 63 | C0 | 2 Spuren | XLT 1,7,13,… (Versatz 6) |
| 2 | 5¼″ MFM 40×16×256 SS | DPBLK2 | 32 | 4 / 2K | 1 | 73 | 63 | 80 | 3 Spuren | |
| 3 | 40 Spuren im 80er-Laufwerk (DSTEP) | DPBLK3 | 32 | 4 / 2K | 1 | 73 | 63 | 80 | 3 Spuren | |
| 4 | 5¼″ MFM 80×16×256 SS | DPBLK4 | 32 | 4 / 2K | 1 | 153 | 63 | 80 | 3 Spuren | |
| **5** | 5¼″ MFM 80×**2**×16×256, MT=1 | DPBLK5 | **64** | 4 / 2K | 0 | **311** | **127** | C0 | **2** | **Auslieferungsformat**, 640K |
| 6 | 5¼″ MFM 80×2×9×512, MT=1 | DPBLK6 | 72 | 4 / 2K | 0 | 350 | 127 | C0 | 2 | neu in 3.0, 720K |
| **7** | 5¼″ MFM 80×2×5×1024, MT=1 | DPBLK7 | **80** | 4 / 2K | 0 | **389** | **127** | C0 | **2** | neu in 3.0, 800K, „kompatibel zu SCP“ |
| 8 | wie 7 („Nutzerformat 1“) | DPBLK8 | 80 | 4 / 2K | 0 | 394 | 191 | E0 | 1 | „790K, kompatibel DAC, Humboldt-UNI“ |
| 9 | wie 7 („Nutzerformat 2“) | DPBLK9 | 80 | 4 / 2K | 0 | 399 | 191 | E0 | 0 | „800K, kompatibel CP/A“ = `cpa800` |

Daraus, ohne Deutung:

- **Logische Spur = Zylinder** bei den doppelseitigen Typen (SPT 64/72/80 = beide Seiten,
  `MT=1` im U8272-Kommando): Kopf 0 Sektor 1…n, dann Kopf 1 Sektor 1…n.  OFF zählt also
  **Zylinder**; OFF 2 = Zylinder 0–1 = 16 KB (Typ 5) bzw. 20 KB (Typ 7).
- **Kein Sektorversatz** außer bei Typ 1 (`SK=0`; `SECTRAN` benutzt `XLT` nur für Typ 1).
  Die lineare Reihenfolge eines `.img`/`.cqm` (Zylinder → Kopf → Sektor) ist die OS/M-Reihenfolge.
- **16-Bit-Blockzeiger**, 2-KB-Blöcke, EXM 0 → ein Verzeichniseintrag = 8 Blöcke = 16 KB, `EX`
  zählt je Eintrag um 1.  Nachgemessen: `BIOS.L` mit Extents 0–3, RC 80/80/80/13.
- **Verzeichnis = Blöcke 0–1** (AL0 C0, 128 Plätze).
- Die Typen 2–5 und 7 stimmen **Zahl für Zahl** mit der CP/A-Regel (`CpaDpbRule`, Tabellen
  `dtrsl1`/`dtrsl3`, Zeilen „SCP Hausformat PC1715“ bzw. 160 Spuren 1024 B) überein; Typ 5 ist
  das Profil `scpx640`, Typ 7 deckungsgleich mit `scp1715`.  Abweichend sind nur **Typ 6**
  (die CP/A-Regel trägt für 512-B-5¼″ `dsldir=0`) und **Typ 8** (OFF 1, 192 Plätze).
- Typ 5 und 7 sind auch dieselben Zahlen, die das DiskTool heute ausrechnet
  (`check`: „4 Systemspuren, 2048-B-Blöcke, 128 Verzeichnisplätze“; 4 *einseitige* Spuren =
  2 Zylinder).

### 3.2 Systemspuren und Urlader

`OSM.URLADER.S` (ORG 0C00H) steht in **Zylinder 0, Kopf 0, Sektor 1**; der 8-Bit-Anfangslader
(MON8) holt ihn wie den UDOS-Urlader nach 0C00H.  Merkmale (an allen 8 OS/M-Systemdisketten
gleich, bis auf FDCONF und zwei Druckerparameter-Bytes bei 0x7B/0x7C):

| Offset in Sektor 1 | Inhalt |
|---|---|
| 0x00 | `FD 21 48 0C 31 00 40 3A 7E 0C …` (`LD IY,VEKTOR / LD SP,4000H / LD A,(FDCONF)`) |
| 0x7E | **FDCONF** des Laders: `55` (Typ 5) bzw. `77` (Typ 7) — passt an allen Abbildern zur Geometrie |
| 0x80 | `P8000SYS` (gleiche Kennung wie der P8000-UDOS-Urlader, z. B. `udos_org`) |

Der Lader liest über den UDOS-Floppytreiber (`FLOPPY` 0BFDH, Request-Vektor) `OSMLEN` = 28H
Sätze à 256 B (10 KB: CCP+BDOS+BIOS) nach `CCP-200H` = D800H, spurweise mit der
Spurlänge aus `LENTAB` (Typ 5: 20H Sätze = 1 Zylinder, Typ 7: 28H = 1 Zylinder), und springt
nach F000H.  Typ 5 belegt also Zylinder 0 ganz und 8 Sätze von Zylinder 1; Typ 7 nur
Zylinder 0.  Typ > 7 wird beim Laden als 7 behandelt — **eine Systemdiskette in Typ 8/9 gibt
es nicht** („Im CP/A-Format kann keine OS/M-Systemdiskette erstellt werden“).

Systemspuren-Kennung im System selbst: „OS/M P8000 (C)ZFT/KEAW Abteilung Basissoftware 1985“,
Startmeldung „P8000 OS/M 3.0  (C)ZFT/KEAW“; bei bsosm3/osmdb/osmredab/osmspiel steht davor
`BS1715` (Bildschirmtreiber mitgeneriert).

### 3.3 Verzeichnis — gemessen

Roh ausgewertet an allen 15 Abbildern (eigener Leser, `cpmread.py` im Scratchpad):

- nur **Nutzerbereich 0**; kein Platz mit Nutzerbyte ≥ 16, **kein** Etikett (20H), **keine**
  Zeitstempel (21H, DateStamper `!!!TIME&DATE` o. ä.), keine XFCB/Kennwörter, `S1` stets 0,
  keine gesetzten Bits in f1–f8.
- **Attribute:** auf den Systemdisketten tragen die Lieferdateien **R/O** (t1′); `rempc171`
  hat einmal R/O+SYS.  Das DiskTool zeigt sie (`attrs: RO`).
- **Namen mit `/`:** `AEND/DOC` (p8000dok), `GW/ZSID.DOC` (osmut), `/Z/HOFFM`, `/Z/HOFFM.BAK`,
  `/Z/NEWS/` (c-beschr) — UNIX-Pfade, die beim Übertragen aus WEGA in den Namen gerieten.  Der
  CP/M-Handbuch-Grundsatz verbietet `/` im Namen; die Warnung `cpm.dir.name` ist darum sachlich
  richtig.
- gelöschte Plätze mit lesbarem Namen: `$$$.SUB`, `*.BAK`, `TEMP.DBD`, `REFO23.$$$` … — die
  Hinweise `cpm.dir.geloescht` sind echt.
- belegte Blöcke ≤ DSM, keine Kreuzbelegung, Extents lückenlos.

## 4. Abgleich mit dem DiskTool

### 4.1 Lesen — richtig

`ls --json` (Name, Größe) des DiskTools und der unabhängige Leser stimmen an **allen 15
Abbildern** zeilengleich überein; `get` (ohne Muster) liefert für osm30/bsosm3 alle Dateien.
`check --full` an osm30/osmspiel: ohne Befund.  Kein Profil ist falsch zugeordnet, keine Datei
fehlt, keine Größe weicht ab.

### 4.2 Die Befunde aus `UEBERSICHT.md`

| Abbild | Befund | Urteil |
|---|---|---|
| osmut | 2 Warnungen | `GW/ZSID.DOC` (zwei Extents) — Name mit `/`, echt |
| bsosm3, osmdb, osmredab | je 1 Hinweis | gelöschte Plätze mit Namen (`$$$.SUB`, `LOMODATK.IDX`, 6× BAK/$$$/TEMP) — echt |
| c-beschr | 6 Warnungen, 1 Hinweis | `/Z/…`-Namen (3 Dateien × 2 Extents) + 11 gelöschte Plätze — echt |
| p8000dok | 1 Warnung | `AEND/DOC` — echt |

Keine Falschmeldung.  Einziger Schönheitsfehler: das Medium-Feld sagt bei 800K „nach der
CP/A-Regel abgeleitet“ statt „OS/M“.

### 4.3 Leere 800K-Diskette und Format 6 — nachgestellt

Drei synthetische Abbilder (Scratchpad, nicht eingecheckt):

| Abbild | erwartet (OS/M) | DiskTool heute |
|---|---|---|
| 800K, alles E5 (frisch mit OS/M `FORMAT` Typ 7) | Verzeichnis ab c2h0, 128 Plätze | `cpa800` ab c0h0, 192 Plätze; Hinweis nennt nur `scpx8915` — **falsch für OS/M** |
| osmspiel mit gelöschten Systemspuren (Datendiskette Typ 7) | ab c2h0 | `cpa_auto`, „Systemspuren leer, Verzeichnis ab c2h0“ — richtig |
| 720K 9×512, eine Datei bei c2h0 (Typ 6) | 1 Datei | `cpa_auto` mit **1** Verzeichnisplatz, 0 Dateien — **nicht lesbar** |

Der erste Fall ist der gefährliche: ein `put` auf diese Diskette landet in Zylinder 0–1 und
überschreibt bei einer späteren `SYSGEN` das System bzw. ist unter OS/M unsichtbar.  Heute
hilft nur `--fs scp1715`, und das verlangt beim Bootabbild einen PC-1715-Ladekopf.

### 4.4 Nebenbefund: `get` mit Muster und Namen mit `/`

```
$ k1520disktool get c-beschr.cqm '*' --binary --to x/
Fehler: Datei nicht schreibbar: /Z/HOFFM
$ k1520disktool get p8000dok.cqm '*' --binary --to x/
Fehler: Datei nicht schreibbar: …/x/AEND/DOC
```

`cmd_get` (tools/k1520disktool.cpp, Zweig mit Muster) ersetzt nur `:`; `ziel /= "/Z/HOFFM"`
macht daraus einen absoluten Pfad.  Hier scheiterte es nur, weil `/Z` nicht existiert — ein
Name wie `/TMP/X` (8 Zeichen) würde nach `/tmp/X` schreiben.  `DiskVolume::extractAll`
ersetzt `/` durch `_` (disk_volume.cpp, „CP/M-Namen sind beliebige Bytes“), ebenso
`physical_cli.py` und das Archiv (`dateien/_Z_HOFFM`).  Die Oberfläche
(`MainWindow.extract_refs`) nimmt `ref.split("/")[-1]` — kein Ausbruch, aber aus `/Z/HOFFM`
wird `HOFFM` (und `/Z/HOFFM` und ein echtes `HOFFM` überschrieben einander).  Gehört nicht zu
OS/M, wird an OS/M-Disketten aber zuerst sichtbar → AP 1.

## 5. Emulator-Seite

OS/M 3.0 bootet im p8000emu (8-Bit-Teil) ohne Änderung:

```
tools/dev.sh tool boot_trace --machine p8000 --keys '<CR><CR>' -c 300000000 <kopie>.cqm
  → "P8000 OS/M 3.0  (C)ZFT/KEAW" … "A:"
```

an `osm30` (Typ 5) und `osmspiel` (Typ 7).  Damit lassen sich echte Referenzdisketten in
**allen** OS/M-Formaten vom Original herstellen (`SETFD`, `FORMAT B:`, `SYSGEN`, `PIP`) —
insbesondere Typ 6, für den kein Abbild existiert (AP 4).  Ein `cqm_hinweise`-Eintrag für
das P8000-Profil ist nicht nötig (CP/M, `.img`/`.cqm`-fähig).

## 6. Entscheidung

**Kein `FsType::Osm`.**  Das Verzeichnis ist CP/M 2.2 ohne jede Erweiterung; `CpmFileSystem`
liest und schreibt es bereits richtig.  Ein eigener Typ brächte Code ohne Verhalten.

**Drei Profile in `filesystems:`**, jedes mit eigenem Grund (die Regel „kurz halten“ ist
eingehalten — Typ 2–4, 8, 9 bekommen keinen Eintrag):

| Profil | Format | Lage | Grund | Erkennung |
|---|---|---|---|---|
| `osm640` | `cpa640` | c2h0, 2K, 128 | Name; Bootabbild-Prüfung (OS/M-Urlader statt beliebig) | nur mit **positivem Nachweis**: OS/M-Urlader in c0h0s1 und FDCONF-Typ 5 |
| `osm800` | `cpa800` | c2h0, 2K, 128 | Name für `create`; **leere Diskette** (§4.3); Bootabbild-Prüfung (statt PC-1715-Kopf bei `scp1715`) | positiver Nachweis (Urlader, FDCONF-Typ 7); sonst `detect: false`-Lesart, Hinweis „ebenso gut osm800“ |
| `osm720` | `k5601_9x512` | c2h0, 2K, 128 | **die CP/A-Regel kann es nicht** | positiver Nachweis (Urlader, Typ 6) **oder** plausibles Verzeichnis ab c2h0 mit ≥ 1 Datei — Rang zwischen `wega720` (−10) und `tar720` (900); muss gegen alle 720K-Abbilder der Sammlung (77 tar, 32 WEGA, 1 FAT) gegengeprüft werden, sonst `detect: false` |

Neue Ladekopf-Art `boot_header: osm` (Urlader-Kennbytes 0x00–0x09, `P8000SYS` bei 0x80,
FDCONF-Typ passend zum Profil) — prüft `create --boot`/`boot-put` wie `k8915`/`pc1715`.

**Deckungsgleiche Profile sind keine Mehrdeutigkeit.**  Eine OS/M-640K-Systemdiskette passt auf
`osm640` **und** `scpx640` (gleiche Lage, BLS, DRM, Versatz).  Damit sie nicht als „nicht
eindeutig“ erscheint und die Gegenprobe nicht anschlägt: gewinnt das Profil mit positivem
Nachweis, wenn die übrigen Treffer dasselbe Layout haben (analog zur Festlegung „gleich gute
Treffer mit demselben Sektorraum“ bei den Formaten).  Datendisketten ohne Urlader bleiben
`scpx640`/`cpa_auto` — das Medium unterscheidet sie nicht, und das ist die ehrliche Antwort.

**Nicht gemacht:** Typ 8 (DAC/HU 790K, OFF 1, 192 Plätze) — Nutzerformat, kein OS/M-Eigentum,
kein Abbild; gehört in ein späteres `dac790`-Profil, falls eine DAC-1715-Diskette auftaucht.
Typ 1–4 sind CP/A-Regel.  Keine Zeitstempel-/Etikett-/Attributbehandlung — es gibt nichts zu
behandeln.

## 7. Was das DiskTool dafür können muss

| Bereich | heute | nach dem Plan |
|---|---|---|
| Lesen 640K/800K | richtig | unverändert, Anzeige „OS/M 640K/800K“ bei Systemdisketten |
| Lesen 720K (Typ 6) | nein | `osm720` |
| Schreiben `put`/`rm`/`attr` | richtig (CP/M) | unverändert; leere 800K nur mit `--fs osm800` bzw. Auswahl |
| `create --fs osm640|osm720|osm800 [--boot]` | nur über `scpx640`; 800K/720K nein | ja, Bootabbild mit `boot_header: osm` geprüft |
| `boot-get`/`boot-put` | 640K ja (16 KB), 800K über cpa_auto | über die Profile, mit Prüfung |
| Eigenschaften-Dialog | „SCPX 640K“ / „CP/A-Regel“ | Profilbeschreibung + Urlader-Befund (Typ, FDCONF) |
| Archiv / `pofo_archive.py` | `scpx640`/`cpa_auto` | `osm640`/`osm800`; Übersicht neu erzeugen |
| fsck | keine Falschmeldung | neu: Befund, wenn FDCONF des Urladers nicht zur Geometrie passt (Hinweis) |
| `get` mit Namen `/` | Abbruch / Pfadausbruch | `/` → `_` wie `extractAll` (AP 1) |

## 8. Risiken

- **Erkennungsverschiebung**: `osm640`/`osm800` dürfen keine heutige Erkennung umwerfen außer
  scpx640/cpa_auto → osm* an den 8 Systemdisketten.  Wächter über alle 151 Abbilder (mit
  `K1520_CQM_DIR`).
- **`osm720` gegen tar/WEGA**: ein tar-Strom kann bei Byte 36 864 zufällig plausibel wirken.
  Deshalb Gegenprobe über die ganze Sammlung und im Zweifel nur mit Urlader erkennen.
- **Verzeichnisgröße Typ 6 nicht eindeutig belegt**: `BIOS.L` (OS/M 3.0) sagt DRM 127 (AL0 C0);
  die WEGA-Beschreibung `WEGA/contrib/cpm/osm` (github.com/OlliL/P8000) beschreibt ein
  „OS/M-Format“ 80×2×9×512 mit 4 Systemspuren, 2-KB-Blöcken, aber „Directory-Größe 1
  (= 64 Einträge)“, hergestellt mit CP/A-`FORMATP` am PC 1715.  Das kann eine ältere OS/M-
  Fassung oder ein Missverständnis der FORMATP-Einheit sein.  Entscheiden kann nur eine echte
  Typ-6-Diskette → AP 4 (aus dem Emulator, OS/M 3.0) und ggf. Anwender.
- robotrontechnik.de nennt 9×512×80 als „Standard-Diskettenformat“ von OS/M — widerspricht dem
  README 3.0 („weiterhin im Format 5 ausgeliefert“); vermutlich spätere Fassung (P8000 compact).
- **Leere-Diskette-Hinweis** darf `osm800` nicht auf jeder leeren CP/A-800K-Diskette
  aufdrängen — er erscheint nur als „ebenso gut …“-Zeile wie heute `scpx8915`.

## 9. Offene Fragen an den Anwender

1. Gibt es echte OS/M-Disketten im **Format 6 (9×512)** oder eine OS/M-Fassung > 3.0 (P8000
   compact)?  Davon hängt `osm720` (DRM 127 oder 63) ab.
2. Soll die Anzeige „OS/M“ nur bei nachgewiesenem Urlader erscheinen (Vorschlag), oder sollen
   Datendisketten aus einer P8000-Sammlung per Archiv-Metadaten „OS/M“ heißen?
3. `c-beschr` und `rempc171` sind PC-1715-Disketten — Archiv-Übersicht so berichtigen
   (Erkennung `scp1715` für rempc171 nur mit `--fs`, heute `cpa_auto`)?
4. Typ 8 (DAC/HU 790K): gibt es solche Disketten?

## 10. Arbeitspakete

Reihenfolge so, dass jede Etappe für sich lauffähig und grün ist.

**AP 1 — `get` mit Muster: Namen mit `/` (Sicherheitsfehler).**
Ziel: kein Pfadausbruch, gleiche Namensregel wie `extractAll`.  Dateien:
`tools/k1520disktool.cpp` (`cmd_get`), sinnvoll eine gemeinsame Hilfsfunktion
`DiskVolume::linuxName()`/`zielName()` für `extractAll`, `cmd_get`, C-ABI und
`MainWindow.extract_refs` (dort `/`→`_` statt `split("/")[-1]`, Kollision vermeiden).
Wächter: Unit-Test mit synthetischem CP/M-Abbild, Namen `/TMP/X`, `../A`, `GW/ZSID.DOC`;
CLI-Fall `dt_get_schraegstrich.cli`; GUI-Test in `test_disktool_*`.  Abnahme: `get c-beschr.cqm '*'`
liefert 19 Dateien `_Z_HOFFM…` im Zielordner, nichts außerhalb.  Aufwand klein (½ Tag).
Modell: Sonnet.

**AP 2 — Profile `osm640`/`osm800` + Ladekopf `osm`.**
Ziel: Name, `create --fs … [--boot]`, Bootabbild-Prüfung, Erkennung nur mit Urlader,
deckungsgleiche Profile keine Mehrdeutigkeit, „ebenso gut osm800“-Hinweis auf leerer 800K.
Dateien: `data/formats.yaml` (`filesystems:`), `core/filesystem/disk_volume.cpp` (Ladekopf-
Prüfung neben `k8915`/`pc1715`, Auswahlregel), `fs_profile.h` (Doku `boot_header`),
`doc/merkposten/disktool.md`.  Wächter: `OsmProfile.*` (Erkennung an synthetischen Abbildern
mit eingesetztem Urlader aus Bytes, nicht aus der Sammlung; leere 800K → cpa800 + Hinweis;
`--fs osm800` → Verzeichnis c2h0), `CqmEchtdaten.OsmErkennung` (mit `K1520_CQM_DIR`: 3×osm640,
5×osm800, alle anderen 143 unverändert), `FsCheckGegenprobe.DerAusgelieferteKatalogIstEindeutig`
bleibt grün.  Abnahme: `info osm30.cqm` → `osm640`, eindeutig; `info osmspiel.cqm` → `osm800`;
`create x.img --fs osm800 --boot` mit OS/M-Systemspuren aus `boot-get` → bootet im p8000emu
(boot_trace bis `A:`).  Aufwand 1–1½ Tage.  Modell: Opus (Erkennungslogik, Katalog-Eindeutigkeit).

**AP 3 — fsck: Urlader-Konsistenz.**
Ziel: Hinweis `osm.urlader.typ`, wenn FDCONF-Typ des Urladers nicht zur Geometrie passt
(Diskette bootet dann falsch).  Dateien: `core/filesystem/check/cpm_check.cpp`.  Wächter:
`FsCheckOsm.*`, `FsCheckKeineFalschmeldungen.*` über die 8 Systemdisketten.  Aufwand klein.
Modell: Sonnet.

**AP 4 — Referenzdisketten aus dem Original (Emulator).**
Ziel: echte, mit OS/M 3.0 erzeugte Abbilder für Typ 5, 6, 7 (je leer formatiert, mit
SYSGEN, mit ein paar Dateien) als Fixtures — klärt DRM von Typ 6.  Weg: p8000emu bzw.
`k1520dbg --machine p8000` mit osm30 in A:, leere Diskette in B:, `SETFD 5600`/`6600`/`7700`,
`FORMAT B:`, `SYSGEN`, `PIP`.  Vorher prüfen, ob die P8000-Floppy (U8272, `floppy8`) zwei
Laufwerke und 9×512 führt.  Dateien: `tests/fixtures/disks/osm_*.img` (+ README-Eintrag),
Testtreiber nach dem Muster der FORMAT-Pipeline (`tests/system/drivers/`).  Wächter:
langsamer Test (`format_integration`-Label) „OS/M formatiert, DiskTool liest“.  Abnahme: drei
Fixtures, DiskTool liest sie; Typ-6-DPB bestätigt oder widerlegt.  Aufwand 1–2 Tage (Tastatur-
und Terminalführung am P8000).  Modell: Opus.

**AP 5 — Profil `osm720`.**
Ziel: Typ 6 lesen/schreiben/anlegen.  Voraussetzung AP 4 (DRM).  Dateien: `data/formats.yaml`,
Erkennungsrang in `disk_volume.cpp`.  Wächter: Fixture aus AP 4; Echtdaten-Gegenprobe:
keines der 77 tar720 / 32 wega720 / FAT-Abbilder kippt.  Abnahme: Fixture → `osm720`, sonst
alles wie vorher.  Aufwand ½–1 Tag.  Modell: Sonnet (Erkennungsrang mit Opus abnehmen).

**AP 6 — Oberfläche, Archiv, Handbuch.**
Ziel: Neu-Dialog bietet `osm640`/`osm800` (`osm720` nach AP 5); Eigenschaften zeigen Urlader-
Befund; `pofo_archive.py` neu laufen lassen, `UEBERSICHT.md` (c-beschr/rempc171 als PC 1715
markieren); Handbuch `app/help/` + `tools/k1520disktool.md` („OS/M ist CP/M; Formate 5/6/7“).
Wächter: `py_disktool_*` (Neu-Dialog-Liste), `py_help`.  Aufwand ½ Tag.  Modell: Sonnet.

Minimalplan, falls nur das Nötigste gewollt ist: **AP 1 + AP 2 + AP 6** (ohne fsck, ohne 720K).

# mutos_io neu aus den Quellen bauen - Anleitung

Stand 2026-10-27 (Entwurf 40).  Diese Anleitung führt von der Quelle `tools/mutos8000/mutos_io/src/mutosio.mac` bis zu einer
bootfähigen MUTOS-Ladediskette und zum Test im Emulator.  Hintergrund: `tools/mutos8000/mutos_io/README.md` (Kurzfassung),
`doc/merkposten/mutos_io.md` (Festlegungen), `doc/merkposten/mutos_io_analyse.md` (was das Programm tut).

## 1. Voraussetzungen

| Was | Wofür | Prüfen / besorgen |
|-----|-------|-------------------|
| `python3` (3.8+) | alle Bauskripte (nur Standardbibliothek, Aufruf mit `-I`) | `python3 --version` |
| CP/M-Werkzeugkette `cparun`, `m80.com`, `linkmt.com` | Assemblieren und Binden | liegt im Schwesterprojekt `CPA_Workbench` unter `tools/`; Vorgabe `~/projects/CPA_Workbench/tools`, sonst `export CPA_TOOLS=<pfad>` |
| `build/k1520disktool` | Diskette anlegen und befüllen | `tools/dev.sh build` (nie ein Binary aus `build*/` von Hand bauen oder ändern); alternativ `K1520_DISKTOOL=<pfad>` |
| `build/k1520dbg` | Test im Emulator | `tools/dev.sh build` |
| Archiv `disks/a5120_mutos8000/a5120_mutos8000_boot_udos.zip` | Vorlage: alle anderen Dateien der Ladediskette, Systemspuren, `bauangaben.yaml` | liegt im Repository |

Ohne Werkzeugkette laufen die Prüfungen mit Exit-Code 77 („übersprungen“) - die CI hat sie nicht, deshalb sind die Bauergebnisse eingecheckt.

## 2. Überblick: was wohin

```
src/mutosio.mac            Quelle (Master, M80-Syntax)
   |  build.py             M80 + LINKMT            -> mutosio.bin   (= die UDOS-Datei mutos_io, 5376 Byte, eingecheckt)
   |  ladediskette.py      Datei + .fileinfo       -> Ladediskette  (.hfe)
   v
disks/a5120_16_MUTOS/...   fertige Disketten (nur mit bau_alle.py neu bauen, wenn ausgeliefert werden soll)
```

Die Datei besteht aus vier Segmenten (Ladeadressen `9000`, `9400`, `A400`, `C8B6`), jedes auf 256 Byte mit `FFH` aufgefüllt.
Die Segmentlängen stehen nicht in der Datei, sondern in der UDOS-Dateiangabe `mutos_io.fileinfo` (`segs=`); `ladediskette.py` berechnet sie aus den
Marken `SEGnnnn`/`SEGnnnnE` der Quelle.

## 3. Schritt für Schritt

### 3.1 Quelle ändern
`tools/mutos8000/mutos_io/src/mutosio.mac` mit einem Texteditor.  Regeln: M80-Syntax, `.Z80`, `CSEG`, vier `.PHASE`-Blöcke; Marken dürfen lang sein; keine Umlaute in
Kommentaren; die `SEGnnnn`-/`SEGnnnnE`-Marken und die `REPT`-Auffüllung am Segmentende nicht entfernen.  Variantenschalter `K8X512` (Kopf der Datei): `0` = 26 x 128 B (Auslieferungszustand),
`1` = 8 x 512 B; wird beim Bau gesetzt, nicht von Hand.

### 3.2 Nur die Datei bauen und prüfen
```sh
python3 -I tools/mutos8000/mutos_io/build.py            # schreibt tools/mutos8000/mutos_io/mutosio.bin
python3 -I tools/mutos8000/mutos_io/build.py --check    # 0 = eingechecktes .bin passt zur Quelle, 1 = nicht, 77 = keine Werkzeugkette
```
Erwartete Ausgabe bei Erfolg: `M80 ... No Fatal error(s)`, `LINKMT ... Undefined Symbols:` (leere Liste) und `FERTIG: ... (NNNN Bytes)`.

### 3.3 Datei und Dateiangabe für die Diskette erzeugen
```sh
python3 -I tools/mutos8000/mutos_io/ladediskette.py datei /tmp/mutos_io_neu
# Ausgabe: mutos_io: 5376 Bytes, Segmente 9000+022D, 9400+0EB0, A400+01A8, C8B6+00B2
```
Im Ordner liegen `mutos_io` und `mutos_io.fileinfo` (Typ, Eigenschaften und Datum aus der Vorlage im Archiv; `start=`, `segment=`, `zusatz=`, `segs=`, `mem=` neu berechnet).
Die Ausgabe der Segmentlängen ist die schnellste Kontrolle, ob ein Eingriff die Größe verändert hat.

### 3.4 Ladediskette bauen
```sh
python3 -I tools/mutos8000/mutos_io/ladediskette.py diskette /tmp/lade_neu.hfe --laufwerke k5601
```
`--laufwerke` wählt die Variante (Aufzeichnung und Laufwerkstyp-Konstante, Entwurf 39 §9.13): `mf3200` (8″ FM), `k5601` (5,25″ MFM), `mf3200_k5601`,
`k5601_nur_a`, `mf3200_k5601_nur_a`, `k5601_8x512` (baut `mutos_io` mit `K8X512 = 1`).  Ein vorhandenes Ziel wird nur mit `--ueberschreiben` ersetzt.
Das Skript packt das Archiv in ein Temporärverzeichnis, ersetzt nur `mutos_io` (+ `.fileinfo`) und ruft `tools/mutos8000/bau_ladediskette.py`; dort laufen
`check --full` und eine Gegenprobe (jede Datei byteweise gleich).

Für Versuche mit einer Kopie der Quelle: `--quelle <pfad>.mac` (dann bleibt `src/mutosio.mac` unberührt).

### 3.5 Im Emulator prüfen
Boot bis zur Anmeldung (Wurzel erst nach der Bedienmaske einlegen; Ablauf wie `tests/cli/cases/mutos8000_boot_ladediskette_k5601.cli`):
```sh
tools/dev.sh tool k1520dbg --em em256 --laufwerke K5601,K5601,K5601 /tmp/lade_neu.hfe
# im Debugger:
#   g 80000000                  Lader-Menue
#   keys BOOT\sMUTOS\r          mutos_io meldet die Bedienmaske
#   g 100000000
#   screen                      Maske ("floppy - drive 0 1 2 3 ..." / "change / ready (r)")
#   eject A
#   mount C disks/a5120_16_MUTOS/a5120_mutos8000_root_mfm.hfe udos_ss77
#   keys r                      weiter  -> g 100000000 -> keys root\r -> g 100000000 -> screen
```
Erfolg: `single user login: root` und nach der Anmeldung `ls -l /` mit der Wurzel.  Hilfsmittel bei der Fehlersuche: `k1520dbg -l tools/mutos8000/mutos_io/src/mutosio.mac` (Namen
und Kommentare der Quelle in `u`/`bt`), `cover` (welche Adressen liefen), `em msg` (Nachricht wie der Z8000 einspeisen), Messskripte `tools/mutos8000/versuche/mutos_io_*.dbg`.

### 3.6 Automatisch prüfen
```sh
tools/dev.sh test -R 'mutosio|mutos_io'      # cli_mutosio_bin_passt_zur_quelle, cli_mutosio_gleicht_dem_archiv, py_mutos_io_pipeline
tools/dev.sh test -R mutos8000               # Boot-Waechter der fertigen Disketten
```
`py_mutos_io_pipeline` führt die Schritte 3.4 und 3.5 mit einer Textänderung aus.

## 4. Eine Änderung einchecken / ausliefern

1. `build.py` ausführen und das neue `mutosio.bin` einchecken (`build.py --check` bleibt sonst rot - gewollt).
2. `ladediskette.py pruefen` schlägt nach einer inhaltlichen Änderung bewusst an (Erstbau == Archivoriginal): das Archiv bleibt das Original.  Wer die Änderung dauerhaft will,
   legt die neue Datei als neue Vorlage ins Archiv (`tools/mutos8000/archiv_erzeugen.py --art ladediskette`) und passt den Wächter an.
3. Ausgelieferte Disketten (`disks/a5120_16_MUTOS/`, Paket) neu bauen: `python3 -I tools/mutos8000/bau_alle.py --lade-quelle <Archiv/Ordner mit der neuen Datei> --ueberschreiben`
   (baut alle Disketten; `--lade-quelle` zeigt auf das Archiv bzw. den ausgepackten Ordner mit dem neuen `dateien/mutos_io`; siehe `tools/mutos8000/README.md`).
   Hinweis: `bau_alle.py` benutzt `bau_ladediskette.py` und nicht `ladediskette.py`; die neue Datei muss also im Archiv/Ordner liegen.
4. Vor dem Merge `tools/dev.sh test-merge`.

## 5. Fehlerbilder

| Meldung / Symptom | Ursache | Abhilfe |
|-------------------|---------|---------|
| `UEBERSPRUNGEN: CP/M-Werkzeugkette nicht gefunden` / Exit 77 | `cparun`/`m80.com`/`linkmt.com` fehlen | `CPA_TOOLS=<pfad>` setzen (Ordner mit den drei Dateien) |
| M80: `U` vor einer Zeile / `Undefined Symbols` | Marke fehlt oder falsch geschrieben | Quelle prüfen (Schreibweise der Marke) |
| M80: `R` vor einer Zeile | relokatibler Ausdruck in einer Rechnung (z. B. `$` außerhalb von `.PHASE`) | Ausdrücke nur innerhalb der `.PHASE`-Blöcke mit `SEGnnnn` als Bezug (siehe Auffüllung) |
| `mem=... reicht nicht fuer die Segmente` | Code ist gewachsen, HIGH (`CB68`) liegt unter dem Segmentende | `HIGH` in der Vorlage anheben (Archiv `dateien/mutos_io.fileinfo`) und den Stapelbedarf bedenken |
| `Dateilaenge ist kein Vielfaches von 256` | Segmentauffüllung zerstört | `REPT`-Zeile vor `.DEPHASE` jedes Segments prüfen |
| Diskette bootet, `DISK INITIALIZATION ERROR C6` | Laufwerkstyp-Konstante passt nicht zur Bestückung | richtige `--laufwerke`-Variante wählen (Entwurf 39 §9.13) |
| Boot hängt vor der Bedienmaske | Fehler in der Initialisierung (`INIT`) | mit `k1520dbg -l ...mutosio.mac` und `cover` eingrenzen; Haltepunkt auf `MASKE` |
| `er=0,30x` / `err on dev` nach der Anmeldung | Plattenrechnung (`BLOCK_START`...) gestört | Variante prüfen: 8 x 512 braucht `k5601_8x512` und die passende Wurzel (`root_8x512`) |
| `k1520disktool` nicht gefunden | nicht gebaut | `tools/dev.sh build` oder `K1520_DISKTOOL=<pfad>` |

## 6. Grenzen

- Die Quelle ist eine Rekonstruktion aus der Originaldatei (Disassemblat mit Kommentaren), keine Herstellerquelle.
- Nicht belegt: Hardwareverhalten außerhalb des Emulators (Messung am Gerät fehlt), Roh-Floppy-Klasse (nie benutzt), tty2/tty3.
- Die Pipeline ist auf `mutos_io` zugeschnitten.  Für beliebige UDOS-/CP/M-Programme siehe die Vorschläge in Entwurf 42 (falls angelegt) bzw. `tools/z80_disasm.md`.

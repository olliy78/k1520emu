# mutos_io - Quelle, Bau und Pipeline

`mutos_io` ist das UDOS-Programm (U880), das den Z8000-Kernel von MUTOS 8000 mit Platte, Konsole,
Drucker und seriellen Leitungen verbindet.  **Master ist `src/mutosio.mac`** (kommentiert, mit sprechenden
Namen).  Semantik und Messungen: `doc/merkposten/mutos_io_analyse.md`; Festlegungen: `doc/merkposten/mutos_io.md`;
Entwurf: `doc/design/40_mutos_io_quelle.md`.  **Vollständige Anleitung (Voraussetzungen, Schritte, Fehlerbilder): `doc/mutos_io_bauanleitung.md`.**

## Eine Aenderung machen (der Weg)

```sh
# 1. Quelle aendern: tools/mutos8000/mutos_io/src/mutosio.mac  (M80-Syntax, .Z80, CSEG, vier .PHASE-Segmente)
# 2. bauen + gegen das Archiv pruefen (Werkzeugkette aus ~/projects/CPA_Workbench/tools, CPA_TOOLS=...)
python3 -I tools/mutos8000/mutos_io/build.py            # schreibt mutosio.bin (EINGECHECKT)
python3 -I tools/mutos8000/mutos_io/ladediskette.py datei /tmp/io     # mutos_io + .fileinfo (segs=, zusatz=, ... neu berechnet)
# 3. Ladediskette bauen (Varianten wie bau_ladediskette.py: k5601, mf3200, mf3200_k5601, *_nur_a, k5601_8x512)
python3 -I tools/mutos8000/mutos_io/ladediskette.py diskette ziel.hfe --laufwerke k5601
# 4. im Emulator pruefen (Boot, Bedienmaske, Platte): Skript wie tests/cli/cases/mutos8000_boot_ladediskette_k5601.cli,
#    mit disk_path auf ziel.hfe; oder von Hand: tools/dev.sh tool k1520dbg --em em256 --laufwerke K5601,K5601,K5601 ziel.hfe
```

Der Wächter `tests/python/test_mutos_io_pipeline.py` geht diesen Weg mit einer Textänderung bis zum Bildschirm.
Nach einer gewollten Änderung ist `mutosio.bin` neu einzuchecken; `ladediskette.py pruefen` (Erstbau == Archiv) schlägt
dann bewusst an - das Archiv `disks/a5120_mutos8000/a5120_mutos8000_boot_udos.zip` bleibt das Original, die
fertigen Disketten in `disks/a5120_16_MUTOS/` werden mit `bau_alle.py` neu gebaut, wenn die Änderung ausgeliefert werden soll.

## Aufbau der Datei

Vier Segmente (`9000`, `9400`, `A400`, `C8B6`), jedes auf 256 Byte mit FFH aufgefüllt (`SEGnnnn`/`SEGnnnnE` markieren
Anfang/Ende; daraus berechnet `ladediskette.py` `segs=`, `segment=`, `zusatz=`).  `mem=` (LOW:HIGH:STACK) kommt aus der Vorlage
und wird nur geprüft: HIGH muss hinter dem letzten Segment liegen.

## Varianten

`K8X512 EQU 0/1` am Kopf der Quelle: 1 = 8 x 512 (DISK-Satz).  `ladediskette.py` setzt den Schalter für `--laufwerke k5601_8x512`;
`pruefen` beweist `K8X512 = 1` == `mutos_patch.py io-8x512` (bytegleich).  Weitere Varianten bitte nach demselben Muster
(`IF`/`ELSE`/`ENDIF`, Beweis gegen den bisherigen Patch).

## Werkzeuge

| Datei | Zweck |
|-------|-------|
| `src/mutosio.mac` | Master-Quelle |
| `build.py` | M80 + LINKMT (`tools/cpm_bau.py`), `--check` = eingechecktes `mutosio.bin` passt zur Quelle |
| `ladediskette.py` | `datei` / `diskette` / `pruefen` (siehe oben), `--quelle` für Versuche |
| `karte.txt` | Ausgangsstand: Abzug + Karte -> Quelle (`python3 tools/z80_disasm2.py --format mac --karte karte.txt <mutos_io>`); nach M4 nur noch Nachweis |
| `mutosio.bin` | eingechecktes Bauergebnis (die UDOS-Datei `mutos_io`) |
| `../versuche/mutos_io_*.dbg` | Messskripte der Analyse (`k1520dbg -x`) |

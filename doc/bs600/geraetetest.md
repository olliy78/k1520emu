# BS600/BS620 am echten Gerät: Test mit je zwei Disketten (low und high)

Stand: Entwurf 48, AP-B2d.  Für den Anwender (PRG 710 und PRG 710-1).  Hintergrund: `doc/bs600/marken_ff.md`.

## Worum es geht

Der Plattentreiber von BS620 (und BS600/BS610) wartet am Marken-Flipflop der Diskettenkarte K5122 (Port 12H, Bit 1) auf einen
**niedrigen** Pegel (`low`); das ROM, UDOS und SCPX des PRG 710-1 warten auf einen **hohen** Pegel (`high`).  Ob BS620 am echten
PRG 710-1 von der Diskette liest, weiß niemand sicher.  Deshalb gibt es je Gerät **zwei** Disketten, die sich nur in diesen
vier Wartestellen unterscheiden:

| Gerät | Datei (im Ordner `disks/`) | Inhalt |
|---|---|---|
| PRG 710-1 | `prg710-1_bs620/prg710-1_bs620_ff_low.hfe`  | **Original** BS620 (Dez. 1988), kein Byte geändert |
| PRG 710-1 | `prg710-1_bs620/prg710-1_bs620_ff_high.hfe` | BS620, vier Sprungbefehle geändert (`JR NZ` → `JR Z`) |
| PRG 710   | `prg710_bs600/prg710_bs600_ff_low.hfe`      | BS600 Version 02 (PRG-700/710-Fassung), Urlader-Kopf **Eigenbau** (s. u.) |
| PRG 710   | `prg710_bs600/prg710_bs600_ff_high.hfe`     | dasselbe, vier Sprungbefehle und die Prüfsumme angepasst |

Die Disketten entstehen reproduzierbar mit `python3 tools/bs600/bau_disketten.py` aus den eingecheckten Abzügen.

**Wichtig:** Die 710-Diskette ist keine Originaldiskette.  Sie trägt die Dateien der PRG-700-Diskette (BS600, Version 02), aber den Urlader
von BS620 mit dem Namen `BS600 ` im Kopf, damit das 1-KB-ROM des PRG 710 („NKM-LOADER“) sie startet.  Ob das ROM des echten PRG 710 sie so
lädt, ist unbelegt.  Boot-Fehler dort sind deshalb **kein** Befund zur Polarität.

## 1. Disketten schreiben (Greaseweazle, 5,25-Zoll)

* Diskette: 5,25 Zoll, doppelte Dichte (DD, 360 KB), einseitig oder zweiseitig beschreibbar (die Abbilder sind einseitig).
* Laufwerk am Greaseweazle: ein **96-tpi-Laufwerk** (80 Zylinder, wie das K5601 des PRG).  Die Abbilder haben nur **40 Zylinder**
  (48 tpi); BS620/BS600 fahren am Gerät Doppelschritt und erwarten sie auf den **geraden** Zylindern.  Deshalb mit `step=2` schreiben.
* Aufruf (getestet mit `gw` 1.23, `venv/bin/gw`; `step=` steht in der Hilfe `gw write --help` unter „TSPEC“):

  ```
  gw write --drive=0 --tracks="c=0-39:h=0:step=2" prg710-1_bs620_ff_low.hfe
  ```

  `--drive` an das Laufwerk anpassen (`0`/`1`/`2`/`3` Shugart oder `A`/`B` IBM).  Mit `gw info` prüfen, dass der Greaseweazle erkannt wird.
  Das Schreiben prüft selbst (Zurücklesen); `--no-verify` nur, wenn das Laufwerk Wiederholungen braucht.
* Gegenprobe: `gw read --drive=0 --tracks="c=0-39:h=0:step=2" test.hfe` und mit `k1520disktool` ansehen
  (`k1520disktool ls test.hfe`) — die Diskette muss als BS600-Diskette mit 28 (BS620) bzw. 25 (BS600) Dateien erscheinen.
* Alternative: DiskTool-Oberfläche „Physische Diskette“, Einstellung „Doppelschritt“ (40-Spur-Diskette im 96-tpi-Laufwerk).

## 2. Am Gerät

1. Gerät ausschalten.  **Linkes Laufwerk = Laufwerk 0**; Diskette einlegen, Klappe schließen.
2. Einschalten.  Der Selbsttest läuft; es erscheint „NKM-LOADER“.
3. Starttaste: **PRG 710-1: ENTER**, **PRG 710: ET** (Taste ET1).
4. Warten (rund 5 s Selbsttest, dann lädt BS6xx weiter).  Erwartet: das Hauptmenü
   `BS600  VERSION: 04` (710-1) bzw. `VERSION: 02` (710) mit den Zeilen `PROG/TEST PC600`, `DIENSTPROGRAMME`, …
5. Bedienung: Pfeil ab auf `DIENSTPROGRAMME`, **ENTER** (am PRG 710 heißt die Taste ET2, falls kein ENTER beschriftet ist), `J`, **ENTER**.
   Jetzt liest BS6xx den Datenträger.  Erwartet: `DISK-NAME :  BS600-…`.
6. **ENTER**: Menü `LIST / COPY / FORMAT / DELETE / RENAME / COMPRESS / COPYDISK`; auf `LIST`, **ENTER**, **ENTER**, **ENTER**,
   `J`, **ENTER**; `GERAET`: einmal **Pfeil ab** (BS), **ENTER**.
   Erwartet: `DISK-NAME`, `DATUM`, `BEARBEITER`, `SEKTORLAENGE 256`, `SEKTORANZAHL 15`, **`DISK ENTHAELT 28 VON 54`** (BS620) bzw.
   `… 25 VON 54` (BS600); danach (Pfeil ab) die Dateiliste `SYSVSP 220983 HOF P 01/01 01/01` usw.
7. Nichts schreiben (kein COPY/FORMAT/DELETE) — die Disketten sind nur zum Lesen gedacht.

## 3. Beobachtungsbogen (ein Blatt je Gerät)

Gerät: ☐ PRG 710  ☐ PRG 710-1   Seriennummer/Stand der K5122 und der ROMs (U555), falls ablesbar: ______________

| Diskette | Selbsttest/Boot bis Hauptmenü? | Fehlertext (genau abschreiben) | `DISK-NAME` gelesen? | `LIST` zeigt Dateizahl? | Auffälligkeiten (Wiederholungen, Blinken, Dauer) |
|---|---|---|---|---|---|
| `…_ff_low`  | ☐ ja ☐ nein | | ☐ ja ☐ nein | ☐ ja ☐ nein | |
| `…_ff_high` | ☐ ja ☐ nein | | ☐ ja ☐ nein | ☐ ja ☐ nein | |

Hinweis: Ein Boot kann schon im **Selbsttest** scheitern (z. B. `CTC ATP KANAL`, `SIO  KANAL`, `RAM-ZELLE`), noch bevor der Diskettentreiber an der Reihe ist.
Das ist ein **eigener Befund** (Fehlertext genau notieren) und sagt nichts über die Polarität.

## 4. Was folgt daraus

| Beobachtung | Folge |
|---|---|
| nur **low** läuft (original bzw. unveränderter Treiber) | Die Diskette im Gerät verhält sich low-aktiv wie der Treiber: Der Controller des Geräts ist für BS6xx im 710-Stand; das Modell des 710-1 (high) gilt für das ROM/UDOS/SCPX, aber nicht für BS620 — dann gibt es dort zwei Controllerstände oder eine Brücke. Bitte Stand/Brücken der K5122 notieren. |
| nur **high** läuft | Der Treiber von BS620 passt nicht zum Controller; die vier gepatchten Stellen entsprechen dem 710-1-ROM.  Dann hat das 710-1 durchgehend high-aktives Marken-FF, und BS620 wäre am 710-1 nie von Platte gelaufen. |
| **beide** laufen | Die Polarität ist für das Lesen unerheblich (z. B. weil `IN 16H` erst nach erkannter Marke Daten liefert, Hypothese in `marken_ff.md`).  Das einheitliche Modell (Daten erst nach der Marke) wäre dann am wahrscheinlichsten. |
| **keine** läuft | Boot-/Selbsttestfehler zuerst klären (Fehlertext); sonst Laufwerk/Spurlage prüfen (96 tpi, `step=2`, Laufwerk 0 = links), Diskette gegenlesen. |

Bitte die ausgefüllten Bögen und, wenn möglich, ein Foto des Bildschirms an den Entwickler zurückgeben.

# WEGA 3.1 für den P8000

Eine fertig installierte Platte, eine zweite Platte und die offizielle Startdiskette dazu.  **Installationsdisketten für WEGA 3.1 gibt es
hier nicht**: die einzige vorliegende Quelle ist ein Plattenabbild.  Wie bei `disks/` insgesamt gilt:
die Tests benutzen diese Dateien nicht, und der Emulator schreibt zurück.

## Inhalt

| Datei | Inhalt |
|-------|--------|
| `p8000_wega31_platte.img.gz` | **Fertig installierte Platte**, Geometrie 1380/10/18 (127 MB → rund 6 MB gepackt), Typ `WEGA31-AVR` (steht nicht im Dateinamen: die Geometrie liefert der Parametersatz in Sektor 0, sonst die Dateigröße); **seit 2026-10-18 mit Kermit** (s. u.), **seit 2026-10-23 für zwei Platten vorbereitet** (s. u.) |
| `p8000_wega31_usr2.img.gz` | **Zweite Platte** für Laufwerk 1, gleicher Typ `WEGA31-AVR` (WDC 4.2 verlangt für alle Laufwerke denselben Parametersatz): formatiert (`sa.format`), `mkfs` 248 220 Blöcke, `labelit` als `/usr2`, leer bis auf `lost+found` (127 MB → 0,6 MB gepackt) |
| `p8000_wega31_udos_boot.dmk` | **Offizielle WEGA-3.1-Startdiskette** (`w31start`, 80×2×16×256, MFM): UDOS mit Koppelsoftware für den 8-Bit-Teil, `boot`, `wega`, die `sa.*`-Programme (mit `sa.diags` 28 962 Byte, `sa.format`, `sa.mkfs`, `sa.install` …) und die Monitor-EPROM-Abzüge `MON8_*_3.1`/`MON16_*_3.1`.  Bis 2026-10-27 lag hier dieselbe Datei wie bei WEGA 3.0; seither das echte 3.1-Original.  WEGA selbst kommt von der Platte |

## Herkunft

`WEGA_3.1.20160725.dd` aus `WEGA_3.1.20160725.zip` (pofo.de; SHA-256 des `.dd`
`a76ef89aa525f1c2fcad8419c2df22159ce5889e870a5832d559c70a06373d30`).  Das Abbild ist für den
**AVR-WDC-Emulator** gemacht (`doc/p8000/wdc_firmware.md` §11): in Sektor 0 stand statt eines
Parametersatzes die Antwort des AVR auf Kommando 28 (`"WDC_4.2\0WDC-Emulator"…`), mit der die echte
Firmware 4.2 nicht arbeitet.  **Einzige Änderung:** Sektor 0 (Z0/K0/S1) ist durch einen gültigen
PAR/BTT-Sektor ersetzt — `"DEFEKT"`, leere BTT, `"PARMTR"`, Kennung `WDC-Emulator`, 1380/10/18,
Vorkompensation 1380, Ramp 1, `ztk` 203/209, Grenzen 251/253/241/243 (Werte aus
`winchester::typen()`, `Platte::parSektor`).  Zylinder 0 liegt außerhalb des Blockraums; WEGA
bemerkt die Änderung nicht.  Damit braucht die Platte die Einstellung *Parametersatz ergänzen* nicht.

## Startdiskette: Herkunft und `sa.diags`

`p8000_wega31_udos_boot.dmk` ist `w31start.cqm` (CopyQM-Abbild der Original-Startdiskette,
`~/projects/robotron/P8000/discs/`), mit `k1520disktool save-as` nach DMK gewandelt (Inhalt unverändert, 1 024 016 B wie die übrigen Disketten).
**`sa.diags` (Hardwaretest) starten:** `O U` am 8-Bit-Prompt, `O U` am 16-Bit-Prompt, am folgenden `> ` **nicht Enter**
(das lädt den Lader `boot` und bringt WEGA in den Einbenutzerbetrieb), sondern `sa.diags` eintippen — es ist selbst ein sekundärer Lader.
Im Menü `t`, dann einen Testschritt wählen (7 = MMU) und Enter.  Am `:`-Prompt nach `boot` geht es **nicht** (`Cannot load offset segs yet`).
Hintergrund: `doc/merkposten/p8000.md` Nr. 66.

## Benutzen

Wie bei WEGA 3.0 (`../wega30/README.md`): WDC-Firmware 4.2, Platte anschließen, Startdiskette in
Laufwerk 0, *Rechner ein* → RETURN → RETURN → NMI.  Geprüft 2026-10-09: der Kern meldet
„WEGA Kernel -- Release 3.2 -- Generated 06/29/116", danach `WEGA login:` (≈ 3 min Wirtszeit).
Die Kennwörter dieser Installation sind nicht dokumentiert.

## Kermit (seit 2026-10-18, Entwurf 36 AP K1)

Auf der Platte stehen C-Kermit 4.2(030) und ein Anmeldekonto, das einen **Kermit-Server** startet:

- `/usr/bin/kermit` (Modus 4755 root, das fertige Z8000-Binary der Kermit-Diskette `kermit.cqm`/`kermit.tar`,
  Stand 1988-02-04), Handbuchseite `man kermit`.
- Benutzer **`kermit`**, Kennwort **`kermit`** (uid 7, gid 1, Home `/usr/kermit`, Shell `/bin/sh`;
  `/usr/kermit/.profile` = `exec /usr/bin/kermit`).  **Kennwortlos geht nicht** — der WEGA-`login` erzwingt bei
  leerem Kennwortfeld ein neues Kennwort.  Laborrechner: Kennwort mit `passwd` ändern, wenn die Maschine am Netz
  hängt.
- Anmeldung auf tty0/tty2/tty4 (nicht tty3, kein getty): nach dem Kennwort startet `.profile` Kermit, das
  `/usr/kermit/.kermrc` liest (`set file type binary`, `server`, `exit`) und als Server läuft; `FINISH` oder
  `LOGOUT` beendet die Sitzung, getty meldet sich wieder.
- **Stand K7 (2026-10-18, zweite Fassung der Platte):** Shell `/bin/sh` statt `/usr/bin/kermit`, weil
  C-Kermit `REMOTE DIRECTORY`/`DELETE`/`TYPE` über die Login-Shell aus `/etc/passwd` ausführt (sonst kam die
  Gebrauchsanweisung von kermit als Verzeichnis); `set file names literal` gestrichen, weil C-Kermit 4.2 damit
  keine Datei mehr hergibt (Fehler in `sfile()`, „Can't open file").  Namen gehen deshalb in Großschrift
  hinaus, ankommende werden klein geschrieben.  Empfangene Dateien gehören `wega` (kermit ist setuid root),
  Modus 666.
- Als **Client** am WEGA: als `wega` anmelden, `kermit`, `set line /dev/tty3`, `set speed 9600`,
  `set file type binary`, `send`/`get`/`remote dir` gegen einen Kermit-Server am PC (tty3 = Druckerleitung
  des P8000, im Emulator ein Hub-Kanal als Telnet-Client); `remote cwd` fragt nach einem Kennwort (RETURN).
- Eingespielt mit dem DiskTool (`put`, `mkdir`, `attr`, `md2/etc/passwd` um eine Zeile ergänzt), `check --full`
  ohne Befund.  Ablauf, Bildschirmtexte, Fallen: `doc/p8000/kermit_wega.md`, `doc/merkposten/kermit.md`; Tests
  `P8000WegaKermit.ServerHinterGetty` und die Gegenprobe der PC-Bibliothek `P8000WegaKermit.*` in
  `tests/system/test_p8000_wega_kermit_austausch.cpp` (`tools/dev.sh test-wega`).

## Zweite Platte (seit 2026-10-23, Ticket #15, Merkposten p8000 Nr. 65)

Die Platte ist für zwei Laufwerke vorbereitet; **nur gegenüber der Fassung davor geändert:**

- Kern mit `md10` (sysgen „add an second harddisk", `md10` = 248 220 Blöcke) als `/wega`, der bisherige als `/wega.old`;
  dazu die Ergebnisse von `sysgen` unter `/usr/sys/conf` (`wega`, `boot`).
- Gerätedateien `/dev/md10` (`brw-r----- bin system 0,10`) und `/dev/rmd10`, Verzeichnis `/usr2`.
- `/etc/rc_csh` fragt nach der `/dev/rz`-Prüfung **einmal** nach, ob Laufwerk 1 antwortet (`dd` auf `/dev/rmd10`), meldet das
  und legt dann die Marke `/etc/md10.da` an bzw. nicht; `/etc/mfs` (= `/etc/umfs`) hängt `md10` nur bei vorhandener Marke
  ein (nach `/dev/usr`) bzw. aus (vor `/dev/usr`).  `/etc/checklist` bleibt unverändert: `fsck` ohne Argumente ist
  interaktiv und liefe ohne Platte in einen Lesefehler.
- Sonst nichts (gleiche Benutzer, Kermit-Konto usw.).

Die Platte läuft auch ohne Laufwerk 1.  Der Kern meldet jeden Zugriff auf `md10` ohne Platte (`err on dev 0/10 … md: fatal
error`); das ist nicht abschaltbar, deshalb rahmen die Meldungen von `rc_csh` die drei Kernzeilen ein:

    Checking second hard disk (md10) ...
    err on dev 0/10
    bn=0 er=FF18,3
    md: fatal error
    Second hard disk not present - /usr2 not mounted.

Mit der zweiten Platte an Laufwerk 1 steht dort `Second hard disk found - checking /usr2`, danach die `fsck`-Ausgabe
(≈ 290 s Maschinenzeit).  Anschließen: *Einstellungen ▸ Laufwerke ▸ Winchester-Laufwerke* = 2, im Plattenkasten *Laufwerk 1*
**Anschließen…**, *Rückstellen*; Handbuch „Zweite Festplatte unter WEGA einrichten".  Die Voreinstellungen P8000 und
P8000 Compact stellen 1 Laufwerk ein.

**Erzeugt** am Emulator mit `tools/p8000/zweite_platte_bereitstellen.sh` (Systemtest
`P8000WegaZweitePlatte.Bereitstellen`) aus der Platte des Git-Stands `c8a7a744`; Wächter der fertigen Abbilder:
`OhneZweitePlatteLaeuftUnauffaellig`, `MitAusgelieferterZweiterPlatteEingebunden`.  Beide Abbilder sind mit `gzip -9 -n`
gepackt.

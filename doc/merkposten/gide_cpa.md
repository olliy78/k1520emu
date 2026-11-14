# GIDE-CP/A (Klaus Wilfling) und Urlader K2526UL4 (Stand 2026-11-14)

CP/A mit **fest im BIOS eingebautem GIDE-Treiber** (nach Heiko Poppes Plattenformat), RTC 72421 (Uhr in der Bildschirmecke),
Farb-BWS (Heiko Poppe) oder K7024, bis 4 Floppy + 8 GIDE-Laufwerke zu 8 MB; bootet von Diskette **oder** (mit neuem Lade-ROM) von der
Platte.  Quellen (nicht im Repo): `~/projects/robotron/rowikla/H_OLAF/` (GIDE-CPA-Quellen 31.07.2025, Urlader, Werkzeuge) und
`gide_cpa_1.pdf` (Anleitung).  Gebaut wird mit der **CPA_Workbench** (`~/projects/CPA_Workbench`, Variante `gide_a5120`).

| Nr | Festlegung | Wächter |
|----|-----------|---------|
| 1 | **Der Bau ist bitgleich reproduzierbar**: `@os.com` (BWS-Fassung, Vorgabe der SETUP.MAC) und `k2526ul4.bin` aus den Quellen = die ausgelieferten Dateien (m80/linkmt über `cparun`, Linkadresse `/p:` aus dem Assemblerlauf).  `crt` ist **BWSHP**; der Emulator hat nur die K7024 → Emulator-Disketten mit `crt = k7024` bauen (anderer Bildschirmtreiber, sonst gleich). | Handvergleich 2026-11-14 |
| 2 | **Lade-ROM `ul4`** (`K2526::Laderom::UL4`, Wert 3, `--laderom ul4`, Konfig `laderom=ul4`, Oberfläche *Lade-ROM: UL4 (GIDE-Boot)*): Diskette zuerst wie V0.9_2, dann GIDE **Basis D0H fest im ROM**, sonst `BOOT > X`.  Der EPROM gehört auf die ZRE K2526, ohne GIDE-Karte auf D0H und ohne Platte kommt nichts. | `py_cpa_gide` |
| 3 | **Platte = Poppe-Layout, 8 Laufwerke zu 8 MB = 500 Spuren × 16 × 63 Sektoren = 256 MB** (kleinere Abbilder: `HDFORMAT` bleibt bei Spur 130 hängen).  `HDFORMAT` (die beiliegende Fassung) formatiert **ein** Laufwerk je Aufruf, `SYSWRCPA C:` schreibt `@OS.COM` in die zwei Systemspuren von C:.  Das DiskTool (`GidePlatte`) erkennt das Ergebnis eindeutig als `poppe`. | `py_cpa_gide`, DiskTool `info`/`check` |
| 4 | **Unverträglich mit Kingstener (`gidn`)**: beide Treiber formatieren unterschiedlich, jeder zerstört die Platte des anderen. | — |
| 5 | **Gast-Eigenheiten, kein Emulatorfehler:** `POWER`/`SUBMIT` wollen ein Laufwerk A:; Programme werden auch auf dem Standardlaufwerk gesucht (Anleitung „Bugs“); bei `ccpload=2` muss `@OS.COM` auch auf C: liegen und mit den Systemspuren identisch sein. | — |

**Workbench** (eigener Zweig): `src/gide_a5120/` (Quellen, `setup.mac` ASCII-gewandelt, weil `patch_mac.py` UTF-8 liest; **alle anderen Dateien byteweise wie geliefert**),
`prebuilt/gide_a5120/` (`bdos.erl`, `ccp.erl`, `cpabas.erl` — das GIDE-`cpabas` hat `AND 7`), `config/gide_a5120/Kconfig.system` (Optionen der SETUP.MAC),
`additions/gide_a5120/` (GIDE-, RTC- und BWS-Werkzeuge), `src/gide_a5120/urlader/` → `build/k2526ul4.bin` (`CPABuilder.build_urlader`, `MAIN_SOURCE_OVERRIDE`).
Nicht übernommen: UTOOLs („Adr D0h“ passt nicht zur GUN-Vorauswahl 84H des Emulators), `FC80BC`/`FC64` (nicht in der Sammlung).

**Disketten** `disks/a5120_cpa_gide/` (README dort): Systemdiskette (K7024, 2 Floppy, 8 GIDE, D0H) und eine **fertig formatierte, bootfähige Platte**
(`.img.gz`, alle 8 Laufwerke, auf C: die Werkzeuge).  Nicht in `build_payload.sh` aufgenommen.

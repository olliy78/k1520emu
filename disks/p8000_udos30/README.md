# UDOS 3.0 für den P8000 (8-Bit-Teil)

Aus der Sammlung von Olli Lehmann (pofo.de), mit `tools/udos_p8000/build.py` neu auf Diskette gelegt (Datei für Datei gleich, Systemspuren wie `FORMAT … S`).

| Datei | Inhalt |
|-------|--------|
| `p8000_udos30_system.hfe` | Systemdiskette: Systemkommandos, `FORMAT`, `EDIT`, Assembler und Binder (U880, U881/U882, U8000), `FILE.DEBUG`, `UPROG` (EPROM-Programmierer), EPROM-Inhalte `MON8_*`, `P8T*`. Zusätzlich die EPROM-Inhalte `MON16_*` (16-Bit-Monitor) und `WDC_*` (Winchester-Firmware) sowie `GETDAY`/`SAVEDAY`. |
| `p8000_udos30_sprachen.hfe` | Programmiersprachen: PLZ, BASIC, FORTRAN, PASCAL |

Starten: Systemdiskette in A:, Return bei „Press RETURN“, am `>` Return. Zum Arbeiten eine Kopie benutzen.

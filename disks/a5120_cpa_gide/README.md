# GIDE-CP/A für A5120 / K8924 (Klaus Wilfling, 31.07.2025)

Gebaut mit der CPA_Workbench (Variante `gide_a5120`), Einstellungen: K7024, K5122, 2 Floppy, 8 GIDE-Laufwerke (C: bis J:), GIDE-Basis **D0H**, RTC 72421 (D5H), CCP im BIOS.

| Datei | Inhalt |
|-------|--------|
| `a5120_cpa_gide_k7024_2fdd_8gide_system.hfe` | CP/A-Systemdiskette mit GIDE-Treiber im BIOS (`@OS.COM`), `HDINFO`, `HDFORMAT`, `SYSWRCPA`, `GIDEHPD0` (nur für andere Systeme!), `TIME`/`DATE`/`WDAY`, Farb-BWS-Werkzeuge u. a. |
| `a5120_cpa_gide_platte_256M.img.gz` | fertige GIDE-Platte (Poppe-Layout, 8 × 8 MB formatiert, bootfähig: `@OS.COM` in den Systemspuren von C:, auf C: die Werkzeuge) |

Benutzen im Emulator (`a5120emu`, Einstellungen ▸ Allgemein): *Lade-ROM* **UL4 (GIDE-Boot)**, unter *Erweiterungskarten* die **GIDE-Karte mit Basis D0H** und die Platte
einlegen; ohne Diskette bootet CP/A von der Platte (`C>`), mit der Diskette im Laufwerk A: von dieser (`A>`).  Für die Kopie der Abbilder gilt wie immer: nicht direkt
mounten, der Emulator schreibt zurück.  Eigene Platte: `HDFORMAT C:` … `HDFORMAT J:`, danach `SYSWRCPA C:` (S, `@OS.COM`, Y).  Der EPROM-Inhalt für ein echtes Gerät
(K2526, U555/2708) ist `doc/EPROMS/K2526/laderom_ul4_gide.bin`.  Hinweise und Grenzen: `doc/merkposten/gide_cpa.md`.

Die Disketten für ein echtes Gerät mit Farb-BWS (Heiko Poppe) baut die Workbench mit `crt = bwshp` (Vorgabe der Variante).

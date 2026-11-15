# SD 1157 — Mikroprogramm und Zeichengenerator (Seriendrucker robotron 1157)

Quelle: [tiffe.de/Robotron/jde/1157/](https://www.tiffe.de/Robotron/jde/1157/), geholt 2026-11-04.  Kommentierte Quelle,
Neuerervorschlag und Beschreibung liegen unter `doc/printer/sd1157/`.

| Datei | Größe | MD5 | Inhalt |
|---|---|---|---|
| `1157.rom` | 10 624 B (0000H–297FH) | 09c70714ae52729e1352b2645ef4a90e | Mikroprogramm |
| `1157_repariert.rom` | 10 624 B | 164f693230dc07b5eb68603d56c75e0f | `1157.rom` mit **einem** geaenderten Byte: 0D34H `2BH` -> `0BH` (Vorgabe des Emulators) |
| `1157zg.rom` | 1 024 B | 44bbe490f3acbbb0ddcc11e50fad8d95 | Zeichengenerator ZG1, liegt auf 3000H (`azg1`); ZG2 auf 3400H fehlt und muss FFH lesen |

**Wichtig:** Das ist die von JDE angepasste Fassung (Version 1.00 vom 10.09.1984, letzte Änderung 24.04.1986: modulo-8-Tabs,
Übergehen von Fehlerzuständen, DC4 → DC1; Anschluss an SKR/A5120 über IFSS und V.24), **nicht** das Werksoriginal.  Gilt für die
Typen 264/267, nicht für 269 (Grafik).  **`1157.rom` ist nicht genau die Übersetzung von `1157.mac`** (WP0b, mit M80 geprüft): Abweichungen
und Hardwarebefund in `doc/printer/sd1157/hardware.md` §1.  Maßgeblich ist das ROM.

Hardware aus dem Kopf von `1157.mac` (kein Schaltplan vorhanden):

| E/A | Baustein |
|---|---|
| 7CH–7FH | SIO: A = V.24, B = IFSS (Daten A, Daten B, Steuer A, Steuer B) |
| DCH–DFH | CTC, Kanal 0 = Baudrate SIO |
| F4H–F7H | PIO 1 |
| F8H–FBH | PIO 2 |
| ECH–EFH | PIO 3 (u. a. Schalterabfrage am Interface) |
| BEH / BFH | Nadelimpulse Nadel 1–8 / Nadel 9 (aktiv low, `0FFH` = Nadeln aus) |
| BCH, 6FH | nur gelesen (Bedeutung aus der Quelle klären) |

Speicher: RAM 2C00H (1 KiB System, Interruptvektoren bei 2CE0H), Zeichengeneratoren 3000H/3400H, SIO-Puffer 3800H (1 KiB).

## `1157_repariert.rom` — Hypothese, kein Beleg durch Pruefsumme

Der Abzug `1157.rom` bleibt unveraendert; die Reparatur ist eine eigene Datei (Projektregel wie K8915-ROM 177).
**Befund:** der Schiebepuffer-Patch (0D20H–0D60H, hardware.md §1) rechnet `ipzl` -= 2 (`2B 2B` bei 0D33H/0D34H) und `ipzf` -= 1.
`ipzl` wurde vorher um 1 erhoeht; mit zwei `DEC HL` bleibt `ipzf - ipzl` konstant 1, der Puffer wird nie leer (die Quelle
`1157.mac` ohne Patch leert korrekt, Grundstellung beider Zeiger 37FFH).  Mit einem `DEC HL` ist die Logik schluessig.
**Hypothese:** Bitkipp 0 -> 1 (Ladungsverlust einer 2708-Zelle, Bit 5) in einem der beiden `2BH`: urspruenglich `0BH` = `DEC BC`
(wirkungslos: BC wird bei 0D5AH per `POP BC` wiederhergestellt).  Die Firmware hat **keine ROM-Pruefsumme** (Quelle durchsucht), es
gibt also keinen Beleg am Abzug.  Unter den Ein-Bit-Nachbarn von `2BH` sind nur `0BH` (DEC BC) sinnvoll; `2AH` (LD HL,(nn)), `29H`
(ADD HL,HL), `23H` (INC HL) wuerden den Pufferzeiger zerstoeren.  Ob 0D33H oder 0D34H gekippt ist, ist gleichwertig (ein DEC HL bleibt);
gewaehlt ist 0D34H.  Status: **[?] Hypothese**.  Die Karte nimmt `rom=repariert` (Vorgabe) oder `rom=original` (`Sd1157Steuerung::Config::rom_repariert`).

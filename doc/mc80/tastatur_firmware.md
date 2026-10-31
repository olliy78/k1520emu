# MC80.3x-Tastatur: Auswertung der Firmware `MC8030KB.bin` (2026-10-31)

Abzug: `~/projects/robotron/mc80/MC8030/MC80/MC8030/MC8030_ROMs_KW/MC8030KB.bin` (2 KB, Z8/U881, **Lage 0800H**:
Reset-Sprung `JP 0BA1`, Befehlsempfang `0A91`, Zeichentabellen `0B00`/`0B40`/`0B80`).  Disassembliert mit
`python3 -I tools/z8_disasm.py MC8030KB.bin --org 0x800 --no-vectors --entry 0x80C,0xA91,0xBA1,0x880`.
(Das Handbuch-Wort „Firmware liegt nicht vor" in `mc80_tastatur.h`/`egos.md` §3 ist damit überholt.)

## Takt
Init (0880H): `T1 = 150`, `PRE1 = 67H` (Teiler 25), `T0 = 1`, `PRE0 = 05H`, SIO 9600 Bd ⇒ Quarz **2,4576 MHz**
(Z8-Zeitgeber = Quarz/8). T1-Tick = 25 × 150 × 8 = **30 000 Takte = 12,2 ms** (Timer-ISR 0BA1H).

## Wiederholung (Autorepeat) — fehlte im Emulator
* Zähler `11H` wird in der Leerlaufschleife (keine Taste) gelöscht, läuft in der Timer-ISR hoch.
* Bei 20H (32 Ticks ≈ **390 ms**) und danach alle 4 Ticks (24H→20H, ≈ **49 ms**) wird die zuletzt gesendete
  Taste (`r10`) „vergessen"; die noch gedrückte Taste wird im Hauptzyklus neu gesendet (Präfix + Code).
* **Keine Wiederholung** (`TM r12,#29H`): Bit 0 = Wiederholung gesperrt, Bit 3 = zuletzt gesendet mit Präfix
  1EH/1FH, Bit 5 = CTRL. Modifikatortasten (34H/36H) werden nie vergessen.
* Einschaltzustand `r12 = 10H` ⇒ Wiederholung **AN**.
* Befehle vom Rechner: **12H = Wiederholung ein, 14H = aus** (beide ohne Antwort). Das Forum nannte 13H/14H;
  13H ist in diesem Abzug KEIN Kommando (→ CAN 18H), und 12H, das EGOS sendet, wird *nicht* mit CAN beantwortet.
  (Möglich, dass eine andere ROM-Fassung — ÄZ1/ÄZ3 — abweicht; Forumsmessung und Abzug widersprechen sich hier.)

## CTRL + Taste
`0A3E`: ist `r12.5` (CTRL) gesetzt, wird der Präfix 1CH → **1FH**, der Code kommt aus der **unumgeschalteten**
Tabelle (der Umschalt-Zweig `0A4A` wird übersprungen). Das gilt für **jede** Taste des Feldes:
CTRL+OFF = `1FH 13H`, CTRL+ENTER = `1FH 11H`, CTRL+a = `1FH 61H` (klein, nicht `41H`).
Der Emulator ließ bei Escape/ENTER/Kursor/Tab/CL die CTRL-Angabe fallen (⇒ CTRL+OFF kam als einfaches OFF an)
und sandte bei Buchstaben Großbuchstaben.
Hinweis: Strg+Esc fängt unter Windows (Startmenü) das Betriebssystem ab — dort die Bildschirmtastatur
(CTRL anklicken, dann OFF) benutzen.

## Umgesetzt / bewusst nicht nachgebildet
* `0EH` = CTRL fest an (`r12 |= 24H`, Umschalt aus, Modifikatortasten ignoriert): jede Taste kommt als
  1FH + unumgeschalteter Code; `0FH` loest das.  Umgesetzt (EGOS sendet 0EH nie, nur Anwenderprogramme).
* Windows: Strg+Esc faengt das Betriebssystem ab → in `mc80emu` ist zusaetzlich die **Pause-Taste = Strg+OFF**
  (`keyboard_mc80.py::map_host_key`).
* Nicht nachgebildet: Piep-Dauer (07H: Port-0-Bit 7 fuer 2 Ticks), Tastenmatrix/Entprellung (Zeichenebene).
* MC80.2x: eigene Matrix an der DPL ohne Firmware-Abzug — Wiederholung dort ungeklaert, unveraendert.

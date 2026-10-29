# SIOS 1526: Tastaturcodes der Bediensicherung (Sonderleitungen) — Befund aus dem Code

Stand 2026-10-29.  Quellen: `doc/sios/sysare11.prn` (1.1), `doc/sios/sysare09.prn` (0.9, neu, erzeugt mit
`python3 tools/sios_a5120/gen_prn.py --variante 09`).  Belegart: [Code] = aus dem statischen Disassemblat gelesen,
nicht am laufenden System bestaetigt (SIOS haengt noch in 1CD4H; keine Gegenprobe im Debugger gefahren).

## 1. Ergebnis

SIOS wertet **die Codes A8H..AFH** aus; die Pruefung ist eine Maske, kein Vergleich mit A9/AA/AB allein.
Gleicher Code in 1.1 und 0.9 (Block 8, 2800H-2BFFH ist in beiden bitgleich bis auf 28C7H, s. u.).
Erwartet werden also die **K7637-Werte A9H/AAH/ABH** (SL1/SL2/SL3); fuer die K7636 gibt es keine andere Codezuordnung
im SIOS, sie muss ebenfalls Codes in A8H-AFH liefern (K7636-Tabelle selbst bleibt [?], Abzug fehlt).

Tastatur-Eingang (Codeprozessor `28C7H`, in 1.1 ueber den Empfangs-ISR 2D12H, in 0.9 ueber den Zeitgeber-ISR):

| | SIOS 1.1 (K7637) | SIOS 0.9 (K7636) |
|---|---|---|
| Hardware | SIO A32 Kanal A; Port-Zellen 00BCH = Steuerport (5DH), 00BDH = Datenport (5CH), per Suchlauf `2C0EH` gesetzt (alternativ 52H/53H, 4EH/4FH) | U-Bus: `OUT (03H),0` Reset `202EH`, `IN (06H)` Code `28C7H`, `IN (01H)` Status `27C7H` |
| Aufruf | Rx-ISR `2D12H` (`IN A,(C)` mit C=(00BDH)); Zeichen < 20H werden verworfen (`CP 20H`, Sprung nach `27DCH`); 9xH -> A = Low-Nibble, Bx -> A and 1FH, sonst unveraendert (A9H bleibt A9H) | Zeitgeber-ISR `27C7H`: `IN A,(01H)`, `BIT 3` (0 = Code bereit) -> `28C7H` |
| TYP | Init `2C55H`: Daten and F0H == 80H; erstes Byte nach Reset ausserdem in 006FH (Flag 006EH bit0, `2D35H`); `80H`-Gruppe loescht 0114H bit5 (`28D9H`) | Init `2033H`: `IN (06H)` bis and F0H == 80H (Schleife FFFFH, sonst Fehler), Low-Nibble -> (00B1H) |

## 2. Auswertung (`2AF6H` in 1.1, `2B03H` in 0.9; Dispatch bei `28CDH`: High-Nibble A0H, Bit 3 = 1)

- `AFH`: `RES 7,(00CBH)`.
- `A8H`: Bit 0 von (00CBH) toggeln (Bedeutung [?]; Bit 0 wird bei `332AH`/`2F2AH` getestet).
- `A9H` (SL1): Bit 0 von **(076BH) toggeln** (`2B66H`); `AAH` (SL2): Bit 1; **alle anderen** (`ABH`..`AEH`, Code and 07H = 3..6): Bit 2.
  Toggle = Triggerverhalten der Tastatur: derselbe Code bei jedem Pegelwechsel (Stopfen gesteckt/gezogen); SIOS kennt keinen Pegel,
  nur die mitgefuehrte Parität.  Ein Code ohne Gegenflanke (z. B. nach Tastatur-Reset doppelt gesendet) kippt die Zelle zurueck.
- Danach `2B23H`: Ebenenwort (00CBH) neu bilden: `(00CBH) and 81H`, dann ein Bit je Wert W = (076BH):
  W=1 -> bit1, 2 -> bit2, 3 -> bit3, 4 -> bit4, 5 -> bit5, sonst (6, 7) -> bit6.
  (W ist die Binaerzahl der Stopfen 1..3 mit SL1 = Wertigkeit 1: passt zur Bedienerkennung 01 (Chef) .. 06; 07 wie 06.)
- Wird ein Bit von 076BH zurueckgetoggelt (Code zum zweiten Mal): `2B59H`: nur wenn dadurch W = 0 wird, wird (00CBH) and 81H
  geloescht; bleibt W <> 0, bleibt (00CBH) **unveraendert** (Eigenheit: Ebene wird dann nicht neu berechnet).

## 3. Zellen und Verbraucher

| Zelle | Bedeutung | Beleg |
|---|---|---|
| **076BH** | Paritaetsmaske der SL-Leitungen: bit0 = SL1 (A9H), bit1 = SL2 (AAH), bit2 = SL3 (ABH..AEH) | `2B10H`, `2B2AH` |
| **00CBH** bit1..bit6 | Bedienebene = Wert W der Stopfen 1-3 (bit1 = Kennung 01 = Chefschalter, bit2 = 02 .. bit5 = 05, bit6 = 06/07) | `2B23H`..`2B57H` |
| 00CBH bit0 | von A8H getoggelt; getestet bei `332AH` (1.1) | [?] |
| 00CBH bit7 | Sperre/Fehler: gesetzt bei E/A-Fehlern (`0BA1H`, `242DH`, `2896H` ..., gehalten bis `AFH` oder Wartschleife `0C7DH` ) | [Code] |
| 076AH | Abbild des Lampen-/Zustandsbytes (nicht Bediensicherung) | `2CA6H` |

Verbraucher der Ebene, die ich gefunden habe: `1DC6H` (1.1): bit1 (Chef) gesetzt und Zeichen `C` -> `IN A,(0AH)`, `OR 04H`, `OUT (0AH)`
= /SPS-ESR (Schutz aus); `1421H`: bit1 steuert die Eingabefeld-Aufbereitung ("_"-Marke).  Weitere Pruefungen der Ebenen 2..6
(Programmierstatus fuer MEM/POU/PLO/LOD/NEW) habe ich **nicht** gefunden — vermutlich ueber andere Zeiger (IX/IY) oder Tabellen.

## 4. Ohne Sicherheitsstecker

Kein SL-Code kommt, 076BH und (00CBH) bit1..6 bleiben 0 (RAM-Vorbelegung, Loeschen bei Start [?]); es gibt **keine Pruefung auf
Eintreffen** des Codes und keinen Timeout: das System laeuft in den Monitorstatus ohne Ebenen (passt zu Handbuch S. 40).  Der
Anlauf bricht nicht ab, die Systemzeile erscheint unabhaengig davon.  Nur das TYP-Byte wird erwartet (1.1: 3 Versuche je Port, dann
naechstes Portpaar `2C65H`; 0.9: Schleife in `2033H`, bei Ausbleiben Fehlerausgabe (`BC=2032H, DE=424BH`, `2041H`, Deutung [?])).

## 5. Folgerung fuer den Emulator

Weg 2 (Emulator sendet nach TYP die Codes der Low liegenden SL) **traegt** — fuer beide Tastaturen mit A9H/AAH/ABH:

- Nach TYP: je Stopfen 1..3 einmal **A9H / AAH / ABH senden** (Kennung 01 = Stopfen 1: nur A9H; 02: nur AAH; 03: A9H+AAH; 04: ABH;
  05: A9H+ABH; 06: AAH+ABH).  Jeder weitere Pegelwechsel sendet den Code erneut (Toggle!).  Ein einzelnes Senden pro aktiver Leitung
  nach TYP reicht, mehr als einmal desselben Codes hebt ihn auf.
- 1.1/K7637: Bytes muessen den SIO-Pfad durch `2D12H` nehmen (Wert >= 20H, nicht 9xH/Bx) — A9..AB erfuellen das.
- 0.9/K7636: Bytes ueber Port 06H, Status 01H bit3 = 0 (Code bereit), ein Code je Zeitgeber-Interrupt (CTC-Kanal 2); (K7636 puffert zwei Codes, doc/a5120_alt/k7636.md).
- Fuer die K7636 gilt der Rueckfall "K7637-Werte als Annahme", da die K7636-SL-Codes nicht belegt sind; SIOS erkennt A8H..AFH.

Offen / naechste Pruefschritte: (a) Gegenprobe im Debugger, sobald der Start bis in die Tastaturinit kommt: Breakpoint `28C7H`, Zelle 076BH
nach Einspeisen von A9H beobachten (Bit 0 toggelt, 00CBH = 02H); (b) Verbraucher der Ebenen 2-6 suchen (Zugriffe auf (00CBH) via IX/IY,
Befehlstabelle des Monitors ab `0E7FH`); (c) Bedeutung von A8H und 00CBH bit0.

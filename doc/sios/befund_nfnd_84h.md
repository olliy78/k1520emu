# SIOS 1.1: Status 84H / `ERR FD 1 NFND` beim ersten Lesen nach Programmlauf (Analyse 2026-10-30)

Fortsetzung von `befund_can_nfnd.md` Befund 2.  Aufbau wie dort (A: SYLs, B: DP1, ASN 0315 66, Stecker 1).
[Lauf] = k1520dbg, [Code] = `sysare11.prn`.  Skripte/Spuren: `/tmp/sp/nfnd/` (kein Eingriff unter `core/`).

## Ergebnis
**84H = "Sektor nicht gefunden"**: der ZVE2-Suchlauf (6A53H..6B1FH) findet die gesuchte Folge nicht innerhalb der
Zeitschranke von 3 Indexpulsen (`[041CH]`, Index-ISR 6F68H zaehlt herunter; bei 0 setzt 6F84H `[041AH]` Bit 0 und bricht
mit `OUT 10H,A9H` ab; 64CAH `BIT 0,(041AH)` -> Fehlerpfad -> `LD B,84H`/`LD (HL),B` bei 65FBH nach dem Restore 62B8H).
**Ursache im Gast, nicht im K5122-Modell (Staerke: mittel-hoch):** SIOS startet im Schritt "Kopf laden/Motoranlauf"
(`6494H`, vor dem Aufbau des ID-Blocks 63B3H..6461H) bereits einen ZVE2-Lauf mit dem, was in `0400H..0406H` und `041DH`
steht.  Im ersten CAL ist das Null, im zweiten steht dort der Rest des letzten Lesevorgangs.

## Belege
1. Treiber: Anforderung -> 6086H (Einheit 0166H) -> OUT 18H=DDH (6168H), OUT 10H=8DH, `041CH`:=9, Warteschleife 617CH, nach
   Timer-Ruf (1E48H -> 001DH -> 6086H -> 60FEH, DE=03A9H) `6494H`: `OUT 14H/04H`, `OUT 10H,A1H` (ZVE2 startet bei 6A53H),
   B9H, 85H.  Der ID-Block `0400H` (FE, Spur, Seite, Sektor, N, CRC hi/lo) wird erst danach bei 63C1H..63E0H geschrieben.
2. [Lauf] `lp 6494H dump 400 16`: CAL 1: `00 00 ...` (Suche findet nichts, ~119 Resyncs bis 5 Indexpulse ablaufen -- die
   "Wartezeit" ist ein ergebnisloser ZVE2-Lauf).  CAL 2: `FE 1E 00 10 00 63 92`, `[041DH]=00` (Rest vom letzten Lesen).
3. [Lauf] ZVE2-Spur CAL 2 (`trace` 6A22..6A98): Resync-Folge 1E/00/01, 02, ... 0F, dann **Sektor 10H trifft** (ID-CRC 6392H =
   Rest-CRC in 0405H) -> Datenfeld wird gelesen -> 6B0BH `DEC (041DH)` 00 -> **FFH** (WR 041D=FF ZVE2.PC=6B0B) -> 6B16H holt die
   naechste Soll-CRC vom Stapel/Tabelle 0780H (dort 0000H) -> `[041CH]:=3` (6B1FH) -> Sektor 11H wird nie akzeptiert (CRC-Byte 50H != 00H) -> 3 Umdrehungen
   (1738 Resyncs, c64281623/64490476/64699302: `[041CH]` 02/01/00) -> `[041AH]=1` (6F84H) -> Restore (60 Schritte, TR00 korrekt) -> 84H.
4. [Lauf] Gegenprobe (unveraenderter Emulator): `e 0x0403 00` unmittelbar vor `CAL DIDI` im zweiten CAL -> **kein NFND, DIDI laeuft**
   (Sektornummer 0 existiert nicht, Suche bleibt ergebnislos wie im ersten CAL).  Das heilt, weil 63E0H in diesem Pfad vor 6494H nicht schreibt.
   Das erklaert auch, warum `mount` heilt (Kopf auf Zylinder 0 -> ID-Spur 0 passt nicht zur Stale-ID 1EH) und warum Nullen von 010CH/D nicht half.
5. Hypothese (a) Motor/Rotation: widerlegt.  Index laeuft, Motor "an" (Port 18H DDH), Spur-Gating unverdaechtig.  Experiment in einer Kopie
   (`/tmp/sp/nfnd/src`: `ioRead(16H)` liefert erst nach `motorAtSpeed` Daten, `K5122_MOTOR_SPINUP_MS` 50/150): NFND bleibt; bei 300 ms
   laeuft SIOS nicht einmal mehr hoch.  (b) Indexzaehlung: 5 Pulse/Umdrehung 491 000 Takte, identisch in beiden CAL.  (d) Doppelschritt: Soll=Ist=1EH, ID 1E 00 10 CRC ok.
   (c) Altbyte/Lesestrom: die Treffer kommen sauber (A1 A1 A1 FE), kein Fehler im Strom.
6. Die vielen Resyncs im ersten Lesen sind also kein Emulator-Artefakt, sondern Konsequenz von 2.: Suche nach einer ID, die es nicht gibt (CAL 1/Mount) bzw.
   nach einer, die es gibt, mit anschliessendem Folgesektor ohne gueltige Soll-CRC (CAL 2).

## Einordnung
Gastverhalten: SIOS verlaesst sich darauf, dass `0400H..` beim ersten Start "nichts Passendes" enthaelt.  Offen/[?]: ob die echte Maschine an
dieser Stelle dasselbe Rest-Muster haette (RAM-Inhalt ist deterministisch gleich; denkbar waere ein anderer Kopfort nach Programmende, z. B. Restore
durch Motor-Aus-Timeout nach laengerer Pause -- im Emulator laeuft der Motor nach Programmende weiter, `OUT 18H=FFH` erst spaeter bei c47.9M).
Ein Hardware-Beleg fuer einen K5122-Fehler besteht nicht (K5122-Doku: Datenseparator/Marken-FF verhalten sich wie modelliert).

## Korrekturvorschlag
Keine Aenderung in `core/` begruendbar.  Fuer Tests/Bedienung: vor dem zweiten Programmstart `0400H..0406H` (und `041DH`) nullen -- im
Test `m.mem`-Poke `[0x0403]=0` -- oder `mountDisk` neu.  Falls spaeter doch ein Modell-Hebel gesucht wird: Motor-Aus-Verhalten nach `OUT 18H=FFH`
(SIOS schaltet nach Leerlauf ab, dann laufen ein Restore und der Index-Neuanlauf; ob SIOS dort den Kopf auf Spur 0 zurueckfaehrt, wurde nicht gemessen).
Risiko eines Eingriffs in K5122: hoch (Boot-Invarianten 3/4, CP/A, SCPX, UDOS, BCU, FORMAT) -- nicht empfohlen.

## Naechste Pruefschritte
1. SGEN-Fall (`ERR FD 1 NFND 9464 D66`): am Block `0400H..`/`041DH`/Tabelle `0780H` im Moment von `6494H` pruefen -- gleiches Muster erwartet.
2. Pruefen, ob SIOS bei laengerem Leerlauf (Motor-Aus 18H=FFH, >~7 s) den Fehler nicht mehr zeigt (Timer-Weg).
3. Test `SiosBoot`/`SiosSgen`: Poke `[0x0403]=0` statt Neu-Mount als Umgehung belegen.

## Nachtrag: Leerlauf heilt nicht (2026-10-30)

Gegenprobe am unveraenderten Emulator: `CAL DIDI`, CI + ET2 zurueck zum Monitor, dann 0 / 100 Mio. / 600 Mio. Takte
Leerlauf, zweites `CAL DIDI` — in allen drei Faellen `MONITOR ===.ERR.FD.1 .NFND`.  Ein Motor-Aus nach Leerlauf erklaert
den Unterschied zur echten Maschine also nicht.

**Die Einordnung "Gastverhalten" ist damit nicht bewiesen, nur nicht widerlegt:** ein ausgeliefertes System, das bei
jedem zweiten Programmstart scheitert, ist unplausibel.  Offen bleibt, warum `6494H` vor dem Aufbau des ID-Blocks
laeuft — vor allem, ob ein Laufwerks-/K5122-Status diesen Zweig ausloest, den die Hardware anders melden wuerde.

## Ursache gefunden: Kettenseite `getIEO()` ignoriert IUS (2026-10-30, Analyse `/tmp/sp/nfnd2/`)

**Ergebnis (Staerke: hoch, Gegenprobe im Emulator):** Emulatorabweichung, kein Gastverhalten.  `Z80CTC::getIEO` (`z80_ctc.cpp:586`,
`iei_ && !anyPending()`) und `Z80PIO::getIEO` (`z80_pio.cpp:421`) geben den nachgeordneten Baustein frei, waehrend ein Kanal in
Bedienung ist (IUS).  Auf der Hardware bleibt IEO bis zum RETI gesperrt (`doc/testsystem_rework.md` §8 nennt das selbst).  Folge: ein RETI
einer fremden ISR loescht zugleich den IUS des BS-PIO B (`K2526::onRETI` ruft beide, der PIO sieht IEI = 1), und `OUT 00H` (INT-BS) tritt
**in die noch laufende INT-BS-ISR ein**.

**Mechanismus (Adressen [Code] `sysare11`, [Lauf] Takte):**
1. 6086H/60FEH: Zweig haengt NUR an `(IX+5)` (= 016BH) Bit 1 und Einheit `(IX+4)`/Bit 5 — kein Portlesen.  Bit 1 klar → 6107H (Phase 1,
   ID-Block erst bei 63B3H); gesetzt → DE=03A9H → `CALL 002BH` (002BH = `EX (SP),HL / ADD HL,DE / EX (SP),HL / RET`, also *relativer Ruecksprung*)
   → 60EBH+03A9H = **6494H**, ZVE2 starten.  Der Zweig ist die Phase 2 des Planers.
2. Phase 1 laeuft in der INT-BS-ISR (1E48H, `CALL 001DH` bei 1E15H).  Sie setzt bei 6131H..6138H `(IX+5) := (… AND D6H) OR 10H` —
   Anforderungsbit 4 **vor** dem Motoranlauf-Warten 617CH..6186H (wartet, bis `(041CH)` = 04H, 5 Indexpulse ≈ 2,46 Mio. Takte; Zeitschranke
   BC=0000 ≈ 2,9 Mio. Takte, danach `CALL 002BH` DE=FF35H → 60C0H → `LD B,84H` = **84H ist die Zeitschranke dieses Wartens**).
3. [Lauf] 281EH (RETI der CTC-Kanal-2-ISR, alle ~5140 Takte): vorher BS-PIO B `ius=1`, nachher `ius=0`.  Der naechste `OUT 00H` (0E0DH in der
   100-ms-ISR) wird sofort angenommen (1E48H bei c37597699, SP=03B8H, **ohne** dass die erste ISR (SP=03C4H) beendet ist).  Planer
   1E26H..1E3BH: Bit 4 gesetzt, Budget `(00FDH)` E6H−99H = 4DH → SET 1 → Treiber → 60FEH → 6494H, **bevor** 63B3H den ID-Block aufbaut.
4. ZVE2 sucht mit dem Rest des letzten Lesens (CAL 2: `FE 1E 00 10`, CRC 6392H = trifft) und setzt bei 6B1DH `(041CH) := 3`.  Das Warten
   in 617CH vergleicht auf Gleichheit mit 04H und sieht die Zahl nie mehr (3, 2, 1, 0) → Zeitschranke → 60C0H → 84H → `ERR FD 1 NFND`.
   Im ersten CAL steht dort Null: kein Treffer, `(041CH)` zaehlt 9→4, Warten endet normal.
5. SYSD (`92CBH D50`) gleiches Muster: Entry 0150H, `(IX+9)=80H`, Phase 1 bei c87772979, 6494H mit Rest-ID `FE 01 00 01` (c87927665),
   84H bei c90882252 (= +3,1 Mio. Takte, die Zeitschranke).  SGEN (`9464H D66`): im Debugger-Skript nicht nachgestellt (Kopfspur-Abfolge
   haengt dort); gleiche Familie dokumentiert (sgen.md §1 Nr. 2 „veraltete Kopfspur", Neu-Mounten heilt), **nicht einzeln belegt**.

**Gegenproben (alle am unveraenderten Emulator ausser der letzten):**
- `e 0x403 00` vor dem Zugriff heilt CAL 2 und SYSD (SYSD laeuft dann bis `START ->ET1`), `e 0xFD 00` ab 6150H fuer 6 Mio. Takte (Planer
  kann nicht gewaehren, danach `e 0xFD E6`) heilt CAL 2: der ID-Block wird richtig aufgebaut (`FE 00 00 05 …`), DIDI laeuft.
- **Scratch-Bau** (`/tmp/sp/nfnd2/src`, nicht unter `core/`): `getIEO()` zusaetzlich `&& !IUS` in Z80CTC (`!anyIus()`) **und** Z80PIO
  (`porta_.iei && (porta_.ius || portb_.ius)` → false): zweites `CAL DIDI` laeuft, SYSD laeuft bis `34 K-BYTE`, erstes CAL und Boot
  unveraendert.  Nur CTC allein heilt **nicht** (der RETI der K5122-/PIO-ISR loescht den IUS des BS-PIO weiter).  Z80SIO nicht noetig.
- Port-Status (RDYL, Index, TR00) gehen nicht in den Zweig ein; die frueheren Spin-up-Versuche (50/150 ms) konnten ihn deshalb nicht aendern.

**Fix-Vorschlag (nicht umgesetzt):** `getIEO()` in `z80_ctc.cpp`/`z80_pio.cpp` (und aus Konsistenz `z80_sio.cpp`) sperrt auch bei IUS;
`RetiChain.IeoBlocksOnPendingNotOnService` kippt dann absichtlich.  Risiko: Boot-Invarianten, K8915/PRG 710/PC 1715/MC80/P8000
(dieselben Bausteine) — nur nach `tools/dev.sh test-merge --alles`.  **Offen:** SGEN einzeln nachstellen; ob das SIO die Sperre braucht
(Tastatur-ISR bei verschachtelten Kartenketten); PIO-Nachweis ohne CTC-Anteil.

## Ergebnis (2026-10-30, Zweig sios-ieo)
`getIEO()` in CTC, PIO und SIO sperrt jetzt auch bei IUS; `K1520Bus::signalRETI` verteilt das RETI nur an den ersten Baustein in
Bedienung (neue `InterruptSlave::inService()`), Karten mit mehreren Bausteinen ebenso.  Zweites `CAL DIDI` ohne Neu-Mount laeuft
(`SiosMonitor.ZweitesCalNachProgrammende`), SYSD zeigt die Modulliste, SGEN laeuft bis `SGEN FINISHED`.  Die Einordnung
"Gastverhalten" oben ist damit widerlegt: Emulatorabweichung.

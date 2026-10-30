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

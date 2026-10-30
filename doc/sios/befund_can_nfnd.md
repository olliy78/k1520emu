# SIOS 1526/1.1: CAN haengt, zweites CAL -> NFND (Analyse 2026-10-30)

Aufbau wie `bedienung.md`: A: SYLs, B: DP1 (`k5601_ss40_26x128`, Doppelschritt automatisch), Sicherheitsstecker 1.
Alle Laeufe mit `k1520dbg` (COW).  Listing `sysare11.prn`.  [Lauf] = am Emulator belegt, [Code] = aus dem Listing.

## Befund 1: CAN (nach M) endet in 1DEFH

**Pfad [Lauf]:** Befehlstabelle (0FC0H, "CAN") -> `1124H` -> `BIT 1,(00FEH)` (Programm geladen) -> SVC 40H dev 0/1
(Bildschirm raeumen) -> `10B6H` (Systemzeile "PROGRAM ==:") -> `CALL 002BH` DE=FB79H -> **`0CC1H`** (gemeinsamer
CANCEL-Ausgang, derselbe wie bei CANCEL-Fehlern).  `0CC1H`: `LD L,21H / CALL 0042H` = `0E1AH` (Kette der vom Programm
angemeldeten Ressourcen/Abbruchziele ab `(0100H)` abraeumen), dann

```
0CC9  LD HL,00FFH
0CCC  BIT 2,(HL)        ; 00FF Bit 2 gesetzt?
0CCE  JR NZ,0CD1H
0CD0  RST 10H           ; nein -> Monitor (M)       <- gewollter Weg, Handbuch 1.2.2.10 "Definierter Programmabbruch"
0CD1  RST 08H / DB 90H  ; ja   -> SVC 90H
```

SVC 90H: Kennbyte Bit 7 = Sonderfunktion, Klasse = Kennbyte AND 1FH = 10H -> Dispatch `1D19H` -> `1D7EH: JR 1DEFH` ->
`1DEFH: DI / OUT (0AH),04H / (0BH) toggeln / Warteschleife 1E00H`.  Das ist ein **eigener, nur von dieser Klasse erreichter
Block** (Schutz aus, Lampe auf PB4 blinkt, Endlosschleife).  SVC 90H steht ebenso hinter dem NMI-Fehlerpfad (`0AB7H`,
"ERR SY ..." nach Q240-Verletzung) und bei `1F53H` (`CF 90 18 FE` = SVC 90H; JR $): **SVC 90H = "System anhalten"** (Havarie-
Stopp), kein Emulator-Artefakt.

**Was 00FF Bit 2 ist [Lauf]:** vom *Programm* gesetzt (SVC-Klasse 04, `1A55H`: Masken-Byte 84H = Bit 2 setzen, Bit 3
loeschen; der Monitor setzt 00FFH bei `0E85H` auf 00H).  DIDI, DINH und CATM setzen es beim Start (00FF = 84H).
Bedeutung nach [Code]: "Programm quittiert Fehler selbst" -- `0B94H`/`0C76H`: bei Bit 2 kein Warten auf CI (00CBH Bit 7),
`0CD3H`: Fehlercode per `JP (HL)` ins Programm zurueck.

**Gegenproben [Lauf]:**
- `e 0xFF 80` (Bit 2 loeschen) vor `CAN` ET1: Bildschirm leer, danach `MONITOR ===== SIOS 1526/1.1 (M)_`, 00FE = 04H,
  00FF = 00H -> der Abbruch funktioniert einwandfrei; die gesamte Monitor-/Geraete-/Q240-Kette ist in Ordnung.
- `CAN` im Monitor ohne Programm (00FE Bit 1 = 0): Sprung zu `RST 10H`, harmlos.
- DINH (setzt ebenfalls 84H): gleiches Bild wie DIDI (1DEFH).
- CATM: anderer Verlauf -- `0E1AH` findet dort einen angemeldeten Abbruchknoten (Typ 04/00) und kehrt ueber `151CH`
  ins Programm zurueck; danach kreist der Fehlerpfad (`0CC1H` wiederholt, Hotspots 0B43H/0C9AH/0CA5H/0CB0H, SP = 03F0H),
  **kein** Halt in 1DEFH und kein Monitor.  Zweite, ungeklaerte Auffaelligkeit (nicht weiter verfolgt).

**Einordnung:** Gastverhalten.  Keine Hardware-Eingabe (Port, Interrupt, NMI, Q240) beeinflusst die Entscheidung; sie
haengt allein an 00FF Bit 2, das das Programm selbst setzt.  Dass die echte Maschine bei DIDI genauso anhaelt, ist
**nicht belegt** (das Handbuch verspricht Rueckkehr zum Monitor); denkbar ist, dass das Original-DIDI eine Abbruchstelle in
der Kette `(0100H)` anmeldet (bei CATM geschieht das, bei DIDI/DINH ist die Kette leer) -- das waere Programmdaten, kein
Emulatorthema.  Hypothese-Staerke: Pfad belegt, "so auch am Original" mittel.

**Korrekturvorschlag:** keine Aenderung in `core/`.  Bedienung/Tests: Programme mit **ET2** verlassen (Rueckkehr (M)),
`CAN` nur testen, wenn das Programm 00FF Bit 2 nicht setzt (oder `e 0xFF 80` als Gegenprobe).  `bedienung.md` Befund 1 ersetzen
durch: "CAN: nach `0CC1H` entscheidet 00FF Bit 2; gesetzt (DIDI/DINH/CATM) -> SVC 90H = Systemstopp".

## Befund 2: zweites CAL -> `ERR FD 1 NFND`

**Beleg des Pfades [Lauf]:** `NFND` entsteht bei `757AH` (`BIT 7,(IY+2)`; IY = 0566H, Einheitenblock): die **erste**
Plattenanforderung von CAL (SVC `RST 08H` bei 7573H) kehrt mit Status 84H zurueck (Schreibzeiger `65FCH`, 0568H).  Im ersten
CAL kehrt dieselbe Anforderung mit 00H zurueck.  Es ist also nicht die Verzeichnissuche, sondern der **erste Lesevorgang**
(Etikettpruefung).

**Kopfposition / Doppelschritt -- widerlegt als Ursache [Lauf]:**
- Gerufene Schrittfolge CAL 1: 119x nach aussen (Anfangs-Restore), dann 2 ein/2 aus/2 ein/54 ein/2 ein/2 ein -> Spur 0,
  1, 0, 1, 28, 29, 30 (SIOS doppelschrittet selbst: 2 Positionen je Spur) -> Kopf endet auf Position 60 (Spur 30 = DIDI-Ende).
  Die IDs, die der Emulator dort liefert, tragen Spur 1EH (= 30): Abbildung Position 60 <-> Spur 30 stimmt, identisch zu den
  DIDI-Lesevorgaengen in CAL 1.
- CAL 2: SIOS waehlt Laufwerk 1 (`OUT (18H),DDH`), liest **ohne Schritt** an Position 60, wiederholt ~1700 Marken-Resyncs,
  liest ID `1E 00 01 00 CRC 53D0` (korrekt), bricht mit Status 84H ab, fuehrt danach den Restore-Lauf `62AFH..62B8H` aus
  (genau 60 Schritte nach aussen, dann /TRACK00 = Spur 0 -- die TR00-Erkennung des Emulators ist richtig) und meldet NFND.
- Treibervariablen Laufwerk 2 (Einheitenblock **0166H**, IX+17 = 0177H Sollspur, IX+21 = 017BH gemerkte Kopfspur):
  nach CAL 1 stehen beide auf **1EH**; vor dem ersten CAL auf 00H.  SIOS sieht Soll = Ist -> `6318H JR Z,6381H` (kein Seek).
- Die Anforderung im Block 0104H (010CH/010DH) traegt in CAL 1 `00 00`, in CAL 2 den Rest des letzten Ladevorgangs
  `10 1E` (CATM -> `01 03`); `7612H` (setzt dort `08 00`) laeuft in CAL 2 nicht.  Nullen von 010CH/D heilt **nicht**.
- Folgeprogramm nach CATM (letzte Spur 3) scheitert ebenso -> allgemein "jedes CAL nach einem Programmlauf, solange der Kopf
  dort steht, wo SIOS ihn vermutet".

**Warum `mount` heilt [Lauf]:** nach `mount B ...` steht der Kopf des Emulators auf Zylinder 0, SIOS glaubt weiter 1EH.  Die erste
Leseanforderung findet dann ID-Spur 0 statt 1EH -> Fehlerzweig `66E3H` -> SIOS faehrt (2 Schritte, normaler Ablauf) und CAL
laeuft bis DIDI (gemessen: 57 Lesevorgaenge wie im Erstlauf).  Das Heilen ist also **zufaellig** (Kopf <-> Treiberwissen
verschoben), kein Beleg fuer einen Emulatorzustand, der verloren geht.

**Verbleibende Auffaelligkeit:** die erste Lese-Anforderung nach Laufwerkswahl braucht sehr viele Marken-Resyncs:
CAL 1 119, CAL 2 1714, nach `mount` 3945 (spaetere Lesevorgaenge 0..51).  ZVE2 sucht im Modus "erste ID lesen" (D = B1H,
`6A56H`) die FE-Marke (`6A88H`), jeder Fehlversuch loest `MK1`-Resync (OUT 10H B5H -> 85H) aus.  ~1 Umdrehung (200 ms =
491 000 Takte) wird so verbraucht; ob das echtem Verhalten (Datenseparator rastet erst nach Anlauf ein) entspricht oder ein
Fehler von `K5122::resyncToNextMark` (Position nach Select/Motor-Neustart) ist, ist **offen**.  Es erklaert NFND nicht, nur
die Dauer.

**Einordnung:** Gastverhalten / Bedienung -- SIOS verlaesst sich nach einem Programmlauf auf seine gemerkte Kopfspur und die
Anforderung ist veraltet; kein Anhalt fuer Fehler im Doppelschritt-Modell (`FloppyDriveV2::mediumCylinder`, `TrackPitch`).
Ob die echte Maschine das ebenfalls tut, ist offen (dort wuerde derselbe Rest in 0104H stehen; SIOS koennte die Etikettpruefung
am Original ueber "Diskette gewechselt" erzwingen -- die K5122 meldet so etwas nicht).

**Korrekturvorschlag:** keine Aenderung in `core/` begruendbar.  Fuer Tests: je Programm frischer Start, oder zwischen den
Programmen B: neu einlegen (`mount`/`mountDisk`) bzw. den Kopf auf 0 bringen.  Gegenproben, die noch fehlen (in dieser Analyse
nicht gefahren, weil SIOS selbst doppelschrittet): 80-Zylinder-Abbild von DP1 mit Doppelschritt-Belegung und K5600.10 als B:
(40-Zylinder-Laufwerk) wuerden die Schritte 1:1 ausfuehren und waeren nicht vergleichbar.

## Naechste Pruefschritte
1. Befund 1: CAN in DIDI/CATM/DINH mit Original-Handbuch (Programmierer-Anleitung DIDI) abgleichen: meldet DIDI eine
   Abbruchstelle an (`0100H`-Kette, Typ 04)?  `RST 28H` ab 6D3FH (DIDI-Start) auf Knotenanlage pruefen.
2. Befund 2: warum `0104H` +8/+9 nicht zurueckgesetzt werden (Aufrufer 7612H/71F3H, Aufruf-Bedingung in CAL-Pfad 7582H);
   Sektor- und Spurwert der ersten Leseanforderung in CAL 2 am `bp 0x6740` im Klartext mitschreiben.
3. Resync-Zahl: `K5122` nach Select/Motor-Start (headPos 12/5174 bei Transferbeginn) gegen die ZVE2-Suchschleife 6A22H..6A8AH
   messen; Vergleich mit CP/A (gleiche Treiberschleife?).

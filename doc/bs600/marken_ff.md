# Marken-FF-Polarität und Laufwerk: warum BS620 „falsch herum“ liest (AP-B2c)

Entwurf 48.  Nur Analyse — **keine Kernänderung, keine Automatik, keine Oberflächenoption**.  Ausgangslage (AP-B2b): der Plattentreiber
von BS620 wartet am Marken-FF (K5122 Tor B Bit 1, Port 12H) auf **Bit 1 = 0** (`AND 02H / JR NZ`, low-aktiv wie am PRG 710), das
ROM, der UDOS-Resident und das SCPX-BIOS des 710-1 dagegen auf **Bit 1 = 1** (`JR Z`, high-aktiv).  Im Emulator läuft BS620 deshalb
nur, wenn nach dem ROM-Boot `afs().setMkeLowAktiv(true)` gesetzt wird — **im Programm `prg710emu` ist das nicht geschehen: der
Plattenzugriff aus BS620 hängt dort weiterhin.**

Gewissheit: **[Disk]** unmittelbar aus Programmcode/Abzug, **[Lauf]** im Emulator gemessen (zeigt das Verhalten des *Modells*, nicht des
Geräts), **[Web]** Sekundärquelle robotrontechnik.de, **[Plan]** Entwurf 20, **[?]** Annahme/offen.

## 1. Wird der Treiber zur Laufzeit gepatcht oder wählt er das Gerät?

- **Alle Wartestellen auf das Marken-FF in BS620 sind low-aktiv [Disk]:** `1312H`, `1350H` (Sektorkopf scharf, 12E3H–1358H), `15A2H`
  und `161AH` (Datenfeld, 159BH/1617H).  Jedes Mal `IN A,(12H) / AND 02H / JR NZ,<zurück>` = warten, bis Bit 1 = 0.  Die übrigen
  `IN (12H)` fragen andere Bits (`10B6H` Bit 7 = /TO, `1147H` Bit 5, `11DAH` Bit 0 = /RDYL) — keine Polaritätsfrage.
  BS610 hat dieselben vier Stellen mit derselben Polarität (`1310H`, `134CH`, …) [Disk].
- **Kein Selbstverändern [Lauf]:** nach einem vollständigen `LIST` (Geräte-Menü BS) ist der Speicher `1000H–27FFH` (Quelle der Kopie:
  Dateioffset `4F00H…`) **byteweise gleich** der Datei; unten `0000H–0BFFH` weicht nur `03FEH` ab (Datenzelle, vom Selbsttest gesetzt).
  Der Treiber wird also weder gepatcht noch umgestellt.  Statisch gibt es auch keine *direkten* Schreibzugriffe `LD (nn),r` in den Treiberbereich; indirekte (`LD (HL),r`) habe ich nicht einzeln geprüft — der Lauf deckt sie ab [Disk/Lauf].
- **Keine Geräteerkennung [Disk]:** BS620 liest von Geräteports nur `5EH/5FH` (Tastatur, SIO B der K8025 — der 710-1-Weg), `84H`
  (ZRE-PIO, Bit 0 setzen/löschen), `C6H`, `E0H/E1H/E4H/E5H` (K6022) und die K5122 selbst.  Es liest weder ROM-Inhalte noch einen
  Merker, noch den `C8H/C9H`-Tastaturbaustein; es gibt keinen Zweig „wenn 710 dann …“.  BS620 nimmt **ohne Abfrage** den 710-1-Tastaturweg
  und den 710-Markenweg.

## 2. Wählt eine Leitung die Polarität?  Was BS620 vor dem ersten Lesen ausgibt

BS620 initialisiert die K5122 selbst (`K5122_INIT`, `1C5BH`), im Vergleich zum 710-/710-1-ROM (`FLOPPY_INIT`, `016AH`/`016DH`) [Disk]:

| Ausgabe | BS620 | ROM 710 und 710-1 (beide gleich) | Wirkung auf das Marken-FF? |
|---|---|---|---|
| `18H` (8212) | `FFH` (nichts gewählt), später `EEH` | `AND F0H`-Wahl | nein |
| `13H` Tor B Steuerung | `FFH`, **`F7H`**, `14H` | `FFH`, **`F3H`** | Bit 1 ist bei beiden Eingang.  Unterschied: BS620 lässt Bit 2 (**/HF**) Eingang und schreibt `12H := 00H`; das ROM treibt /HF (`12H := 04H`: /HF = 1, „FM/5-Zoll-Datenrate“ [?]).  Eine Wirkung auf das Marken-FF ist **nicht erkennbar** [?]. |
| `17H`, `11H`, `15H`, `10H` | `7FH`, `7FH`, `3FH`, `FDH`, `3FH` | gleich | nein |
| Strobe | `OUT (10H)`: `B5H` (aus), `85H` (scharf, MFM, Kopf 0) | `B5H`/`85H` | gleich |

Die ZRE-PIO `84H–87H` und die Seitenregister `E8H–EFH` (BS620 schreibt dort `04H` auf die Seiten 1, 7–A; das Modell kennt nur `E8H–EBH`) stehen
in keinem erkennbaren Zusammenhang mit dem Marken-FF; sie sind in 710 und 710-1 gleich belegt [Plan].  **Ergebnis:** es gibt in der
Ausgabefolge von BS620 **kein Bit, das eine Polarität umschalten könnte** [Disk]; ob die Hardware selbst eine Brücke hat, sagt der Code nicht [?].

## 3. Treiber im Vergleich: BS610 (PRG 710) ↔ BS620 (PRG 710-1)

- Die Dateien unterscheiden sich in 1767 Bytes.  Im Treiber (`12FFH–170CH`, rund 1000 Bytes) ist der **Aufbau derselbe** [Disk]: gleiche
  Wartestellen, gleiche Prüfreihenfolge (`A1`-Überlesen, `FEH`, Kopf, Sektor, Länge).  Der einzige sachliche Unterschied: BS620 ersetzt
  `OUT (C),D` / `OUT (C),E` durch `CALL 1DC7H`/`1DC0H`, die vor der Ausgabe **Bit 2 setzen** (`SET 2,D` / `SET 2,E`) — **BS620 erzwingt
  Kopf 0** (nur einseitig).  Alles andere (Polaritäten, Doppelschritt, K5122-Programmierung) ist der 710-Stand.
- Sonst ist BS620 nur am **Tastaturweg** und an Kleinigkeiten angepasst (Bereiche `0030H`, `0048H`, `0115H`, `0201H–028CH` = Tastatur-/SIO-Weg,
  `0447H–0550H` Tabellen; Prüfsumme in BS620 stillgelegt).  Die Selbsttests sind bytegleich (`selbsttest.md` §6).
- Folgerung [Disk]: **BS620 ist der BS610-Treiberstand mit K7672-Tastaturweg** — der Autor hat die Zeilen mit dem Marken-FF *angefasst* (für den
  Kopf-Zwang) und die Polarität **nicht** geändert.  Wäre sie am Zielgerät falsch gewesen, hätte er sie dort (Dez. 1988, `201288`)
  gemerkt — vorausgesetzt, BS620 lief am Gerät dauerhaft [?].
- Zuordnung laut Sekundärquelle [Web]: PRG710-Variante = BS610, **PRG710-1-Variante = BS620**; Start vom Laufwerk mit **2 Köpfen K5601** am
  710 und 710-1, **1-Kopf-Laufwerk K5600.10 am PRG 700** (BS600).  Ein Satz „Controllerkarte geändert“ steht auf dieser Seite **nicht**; ich konnte
  ihn nicht belegen [?].
- **Dieselbe K5122 (062-8390) auf beiden Geräten** nach der Tabelle in Entwurf 20 §2 [Plan]; Kartenstand/Brücken beider Geräte sind nicht
  verglichen [?] (Entwurf 20 §8.1 — offen, nur am Gerät klärbar).

### Wie unterscheiden sich 710 und 710-1 am Marken-FF? [Disk]

Nicht nur in der Polarität, sondern im **Ablauf** (aus `doc/EPROMS/PRG710/prg710*_zre.prn`, `resident_710*.lst`):

| | 710 (ROM `02DDH`, Resident `0A5CH`, BS610/BS620) | 710-1 (ROM `02E0H`, Resident `0A8DH`) |
|---|---|---|
| vor dem Strobe | `XOR A / OUT (14H),A / IN (16H)` | `IN (16H)` |
| Strobe | `OUT (10H),D / OUT (10H),E` — im ROM/Resident **in der Schleife wiederholt** | einmal |
| Warten | `IN 12H / AND 02H / JR NZ` (bis 0) | `IN 12H / AND 02H / JR Z` (bis 1) |
| danach | Lesen | **`OUT (14H),A`**, dann Lesen |

BS620 hat den Strobe **außerhalb** der Warteschleife (`12F9H…1312H`) und das `OUT (14H),A` **vor** dem Strobe — also den 710-Ablauf, aber *ohne*
das Wiederholen des Strobes in der Schleife, das ROM und Resident des 710 haben.

## 4. Experimente im Modell [Lauf] (zeigen, wie das *Modell* reagiert — nicht das Gerät)

- Mit der 2×2-Matrix ROM × Polarität bootet BS620: **710-ROM + low = ok, 710-ROM + high = fehlt, 710-1-ROM + high = ok, 710-1-ROM + low = fehlt.**
  Jedes ROM braucht im Modell seine eigene Polarität; keines verträgt die andere (das Modell setzt das Marken-FF an *jedem* Sync-Byte,
  `setMkeJedesSyncByte`, und hat kein Rauschen auf formatierten Spuren).
- BS620 mit **710-ROM und low-aktivem FF** läuft bis zum Hauptmenü ohne jede Anpassung (Selbsttest, Menü; Tastatur dort K7609, also nicht bedienbar).
- BS620 mit **710-1-ROM, high-aktiv (Vorgabe)** hängt im Plattentreiber nach ≤ 5 Versuchen in `1312H` (Marken-FF bleibt 1, der Treiber strobt nicht neu).
  Am Gerät könnte das anders ablaufen: dort ist die Phase zwischen Plattenumdrehung und CPU nicht starr, und ein *retry-toleranter* Ablauf
  würde mit der „falschen“ Polarität nicht sofort, sondern **zufällig** hängen (Marken-FF zufällig zwischen Strobe und Abfrage gesetzt) [?].
  Das ist eine Vermutung, am Modell nicht belegbar.

## 5. Laufwerk: Doppelschritt, K5601, K5600.10

- **BS620 fährt Doppelschritt, immer [Lauf, Pulse am Port 10H gezählt]:** Spur 0 → 1 sind **2** Schritt-Pulse (`ii`), Spur 0 → 26 sind **52**, zurück 52; die
  gelesene Sektorkennung bestätigt es (`ID-Spur 01` nach 2 Pulsen, `1AH` = 26 nach 52).  Der Treiber rechnet also schon selbst auf 96 tpi um.
- **Der Boot-Vorgang auch:** das Laden der Datei `BS620` (Spur 6 … 13) endet nach genau **26** Pulsen, Kopfposition 26 = Spur 13 × 2 [Lauf].  Das ROM
  (Vektor `03FDH`) und der Bootsektor stehen also ebenfalls auf 2 Pulsen je Spur.
- **Der Emulator verdeckt hier nichts:** `TrackPitch` (40-Spur-Abbild im 80-Spur-Laufwerk, Position 2n = Spur n) macht genau das, was der Treiber
  verlangt; ein Treiber mit *einfachem* Schritt fände im Modell auf ungeraden Positionen keine Spur.  Auf einem **K5600.10** (40 Spuren, 48 tpi)
  würde derselbe Treiber bei Spur 13 auf Zylinder 26 stehen — die Diskette ließe sich dort **nicht** lesen.
- Folge [Disk/Lauf]: **BS620 (und BS610) brauchen ein 96-tpi-Laufwerk (K5601)**; K5600.10 ist die Ausstattung des PRG 700 mit BS600 [Web].  Die Vermutung
  „BS600 braucht K5600.10“ trifft also nur für BS600 (PRG 700, 3-KB-ROM, nicht in diesem Projekt) zu, **nicht** für BS620.  Die K5601 des Anwenders
  ist richtig; am Doppelschritt liegt der Polaritätskonflikt nicht.
- Offen [?]: ob die **Spurlage** (Position 2n) am echten Laufwerk auch bei Kopf 0/Seite 0 genau stimmt (die Diskette ist einseitig); nur am Gerät prüfbar.

## 6. Welche Erklärung passt am besten?

1. **BS620 ist für einen Controllerstand gebaut, der low-aktiv liest** (der Stand des 710), mit K7672-Tastaturweg [Disk: Treiber = BS610-Treiber; Plan: 710-1 = K7672].
   Dann gäbe es zwei Controllerstände: den, auf dem 710-ROM/BS610/BS620 laufen (low), und den, auf dem 710-1-ROM/-UDOS/-SCPX laufen (high).  Beides
   am *selben* 710-1 ist dann ausgeschlossen, es sei denn, ein Controllerstand ist umschaltbar (Brücke) **[?]**.
2. **Ein Schalter/Brücke (Lötbrücke, Stiftleiste) auf der K5122** wählt die Polarität; das 710-1 des Autors/Anwenders stand für BS620 anders als für UDOS **[?]**.
3. **Die Zuordnung 710-1 = high ist für BS620 nicht gültig, weil der 710-1-Stand erst nach BS620 (Dez. 1988) kam** (UDOS.PRG710-1 trägt „V4.3_1_89“, also 1989) —
   zeitlich möglich **[?]**: BS620 wäre dann noch auf dem Controller der ersten 710-1-Geräte entstanden.  Das ist die einzige Lesart, die das Datum stützt,
   aber kein Beleg: BS620 hängt am Datenträger vom 20.12.1988 [Disk], die UDOS-Fassung ist 1989 [Disk, Dateiname].
4. **Das Modell irrt**: die Polarität 710-1 = high ist aus dem Code der drei Software-Sätze abgeleitet, nicht gemessen.  Gegen 4 spricht, dass **drei unabhängige
   Programme** (ROM, UDOS-Resident, SCPX-BIOS) auf beiden Geräten jeweils gegensätzlich schreiben [Disk]; für die Möglichkeit „retry-tolerante Programme“ spricht
   nichts Belegtes.

Reihung (nach Stützung): **3 ≈ 2 > 1 > 4**.  Keine Lesart ist am Original belegt.

## 7. Empfehlung

**In den Kern gehört (erst, wenn der Anwender am Gerät geklärt hat):** nichts.  Wenn eine Betriebsart nötig wird, dann eine ausdrückliche
Konfiguration „Marken-FF-Polarität des Controllers“ (nach Variante vorbelegt, überschreibbar), die **nach** dem ROM-Boot wirkt oder — besser — eine
Controller-Variante, bei der das **ROM** zur Polarität passt (710-ROM ↔ low).  Eine Erkennung „BS620 am Inhalt, dann umschalten“ (B3/B4) wäre
ein Umweg um eine Gerätefrage und sollte nicht ohne die Antworten unten gebaut werden.

**Nur der Anwender am Gerät kann klären:**

1. Läuft **BS620 am echten PRG 710-1** bis in `DIENSTPROGRAMME ▸ LIST` (Disk-Name, Dateiliste)?  Wenn ja: die Hardware verträgt die Polarität (→ Erklärung 2/4,
   Modell falsch/umschaltbar).  Wenn BS620 am 710-1 nicht von Platte liest: Erklärung 1/3.
2. Welche **Aufschrift/Stand** haben die K5122 (062-8390) und die ROMs (U555) beider Geräte; gibt es **Brücken/Lötbrücken** in Nähe der Port-B-PIO (U855) der K5122?
   (Entwurf 20 §8.1.)
3. Läuft **BS610 am PRG 710** (zweite Gegenprobe der Zuordnung)?  Falls keine Diskette: die PRG-710-Fassung (`BS600_*Tracks*`) ist abgezogen.
4. Zeigt am PRG 710-1 **UDOS/SCPX** Auffälligkeiten beim Lesen (Wiederholungen „DISKERROR C4“)?  Das wäre ein Hinweis auf eine unpassende Polarität.

**Was sich ohne Gerät noch tun ließe (nicht getan):** Schaltplan der K5122 (062-8390) aus dem Fundus lesen (nicht im Projekt vorhanden); nach „MKE“/„Bit 1“ und Brücken suchen.

## 8. AP-B2d: Hypothese „eine Hardware für alle Treiber“ am Modell geprüft

Hypothese (Anwender): das Marken-FF ist an **beiden** Geräten low-aktiv, und `IN (16H)` liefert erst nach erkannter Marke Daten (die CPU bleibt im `/WAIT`).
Dann liefe der 710-1-Code (`JR Z` auf sich selbst) sofort durch, BS620 und das 710-ROM warteten auf 0.  Dafür gibt es im K5122-`/WAIT`-Zweig die neue Einstellung
`setDatenErstNachMarke(true)` (`IN (16H)` hält die CPU bis zum Ende des ersten Sync-Bytes der nächsten Gruppe und liefert dieses A1) und im PRG die Konfiguration
`Prg710Machine::Config::leser_modell` (0 = bisher = **Vorgabe**, 1 = „einheitlich“, 2 = einheitlich mit Sofortmarke).  A5120 und K8915 sind unberührt (Vorgabe der Karte: aus).

**Ergebnis [Lauf, Modell — nicht Gerät]:**

| Leser-Modell | PRG 710-1 | PRG 710 |
|---|---|---|
| 0 bisher (710 low + Sofortmarke, 710-1 high) | alles grün; **BS620 hängt im Plattentreiber** | alles grün |
| 1 einheitlich: low, Daten nach Marke, **keine** Sofortmarke | **alles grün** (ROM, UDOS, SCPX, FORMAT, EPROMmer, Lochband, Sertest, BS620 LIST/COPY **ohne Umpolen**) | 710-ROM **bootet nicht** |
| 2 einheitlich + Sofortmarke (`setMkeJedesSyncByte`) | UDOS/SCPX/ROM-Tests rot, BS620 low bootet bis LIST | **alles grün** (UDOS, SCPX, FORMAT, EPROMmer, Lochband, Sertest) |

* **Die Hypothese trägt zur Hälfte:** low-aktiv **und** Daten erst nach der Marke genügen am 710-1 für *alle* Software einschließlich des **unveränderten** BS620; die Polarität
  je Variante entfällt dort.  Am 710 bleibt die Polarität low und `IN (16H)` nach der Marke verträgt sich mit allen Wächtern.
* **Ein einziges Modell für beide Geräte ist es trotzdem nicht:** Modell 1 und 2 unterscheiden sich allein in der **Sofortmarke** — gilt das zuletzt fertige Byte als Marke, wenn es ein Sync-Byte ist.
  Das 710-ROM strobt in einer 54-Takte-Schleife neu (Abfrage ≈ 12 Takte nach dem Strobe, kürzer als ein Byte) und findet ohne Sofortmarke nie eine; das 710-1-ROM strobt einmal und
  liest bei einer Sofortmarke das Byte davor (die Falle von AP-P3).  Der Unterschied der Ablaufe der beiden ROMs (§3) ist damit **die** noch offene Eigenschaft; ob sie am Gerät eine
  Hardwarefrage (FF setzt früher/später im Byte) oder ein Modellartefakt (starres Zeitraster) ist, bleibt offen [?].  Nicht verbogen: Vorgabe 0 bleibt; die Modelle 1/2 sind Prüfstände.
* Eine Variante „Marke physikalisch“ (FF setzt am Ende eines gerade laufenden Sync-Bytes) wurde ausprobiert und verworfen: bootet weder 710 noch 710-1.
* **Fehlerbilder der neuen Einstellung:** ohne jede Marke auf der Spur (unformatiert) liest `IN (16H)` wie bisher weiter (Rauschen mit Scheinmarken, AP-P3b); das Warten ist am Gerät unbekannt [?] —
  `UnformatierteDisketteMeldetFehlerUndBootetNachWechsel` ist mit Modell 1 am 710-1 rot (Meldung/Dauer anders), mit Vorgabe grün.

**Die vier Disketten am Modell** (`Prg710Bs6xxDisketten.Tabelle`, Stufe: Boot / Hauptmenü / Disk-Name / LIST):

| Diskette | 0 bisher | 1 einheitlich | 2 einh.+Sofortmarke |
|---|---|---|---|
| 710-1 BS620 low (Original) | Hauptmenü | **LIST ok** | **LIST ok** |
| 710-1 BS620 high (gepatcht) | **LIST ok** | **LIST ok** | Disk-Name |
| 710 BS600 low (Eigenbau-Urlader) | Disk-Name | Boot fehlt | Disk-Name |
| 710 BS600 high (gepatcht) | Hauptmenü | Boot fehlt | Disk-Name |

* Die **BS600** (Version 02, Datei der PRG-700-Diskette) macht **Einfachschritt** und scheitert an einem 96-tpi-K5601 mit der 40-Spur-Diskette beim Nachladen von Spur ≥ 1 mit `FC 06` — „Disk-Name“ (Spur 0)
  ist ihr Maximum am K5601 (Test `Prg710Bs600.LiestAmK5601NurSpur0FC06NachDemKennsatz`).  Das passt zu robotrontechnik.de (BS600 = K5600.10 am PRG 700).  Eine echte BS610 (Doppelschritt) liegt nicht vor;
  die 710-Diskette ist damit **kein** Beleg für das Polaritätsproblem am PRG 710, nur für die Tastenbelegung (§ Tasten am 710 in `merkposten/bs600.md`).
* **Patch `high`:** vier Bytes `20H → 28H` (JR NZ → JR Z) an `1316H`, `1354H`, `15A5H`/`161EH` (BS620: `1316H, 1354H, 15A8H, 1620H`) bzw. den entsprechenden Stellen der BS600; bei BS600 zusätzlich die Sollbytes der aktiven Prüfsumme
  (`AEC4H`, `AEC5H`, `AEEBH`) — Sektorvergleich: BS620 genau 4 Bytes, BS600 genau 7 Bytes weichen ab, Prüfsumme nach dem Patch in allen 25 Blöcken gut (`bau_disketten.py --check`).

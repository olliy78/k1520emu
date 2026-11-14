# Entwurf 49 — Druckeremulator `k1520printer` (Ticket #28)

Stand 2026-11-06, **WP0a/WP0b, WP1, WP3, WP2b, WP4/WP6b-Text, WP5, WP2a, WP6a und WP7 erledigt (siehe §5a–§5f)**; die zweite Bahn (267) entfällt (§10).  Zweig `feature/issue-28-k1520printer`.
Nummer 49, weil 48 für BS600 vorgemerkt ist (noch in keinem Zweig).

## 1  Auftrag (Ticket #28) und Festlegung des Anwenders

Zusätzliches Programm **`k1520printer`**: Hardware-Emulator eines Druckers, kein Textfilter.

* Serielle Schnittstelle wie bei Rechnern/Terminals: **Telnet oder RFC 2217** (`core/serial/`, `SerialHub`).
* **Alle LEDs, Taster und Schalter** in der GUI nachgebildet.
* Druckkopf malt **schwarze Punkte auf ein weißes Blatt**; Blatt als **PDF speichern** und über die **Druckfunktion des
  Betriebssystems** ausdrucken.
* GUI **wie die des Computeremulators**: statt des Bildschirms das Blatt (schiebt sich beim Drucken **zeilenweise nach oben**,
  **aktuelle x-Position des Druckkopfes sichtbar**); rechts Bedienelemente und Druckeinstellungen.
* Mehrere Druckertypen (Nadel, später **Typenrad**) → **Auswahlfeld Druckertyp** von Anfang an.

**Festlegung 2026-11-05:** Das Ticket nannte den K6313 (Ticket: „K3613").  Für ihn gibt es **keinen EPROM-Abzug** (§2.4).
Begonnen wird deshalb mit den beiden Nadeldruckern, deren Firmware vorliegt: **K6316** und **SD 1157**.  Der K6313 folgt als
weiterer Typ, sobald seine Kassette ausgelesen ist.

## 2  Befund

Bestand und Herkunft aller Unterlagen: **`doc/printer/README.md`**.  EPROMs: `doc/EPROMS/K6316/README.md`,
`doc/EPROMS/SD1157/README.md`.  Alle Angaben unten aus OCR der Scans — **vor Verwendung gegen das Original prüfen** (WP0).

### 2.1  Die Geräte im Überblick

| | **K6316** Schalterdrucker | **SD 1157** Seriendrucker | K6313/K6314 (später) | SD 1152/257 (später) |
|---|---|---|---|---|
| Druckwerk | 9 Nadeln, 80 Spalten, 100 Z/s, vor-/rückwärts | 9 Nadeln, 132 (264) bzw. 210 Spalten/2 Bahnen (267), 180 Z/s | 9 Nadeln, 80/136 Spalten | Typenrad |
| Rechner | U880, 3 PIO, 2 CTC, SIO am Modul, 5 × U2716, 1 KiB RAM 3800H (`k6316/hardware.md`) | U880 in Sloteinheit (11 LP), SIO, CTC, 3 PIO, RAM 2C00H + 3800H (`sd1157/hardware.md`) | U880, CTC, PIO | Master-/Subprozessor |
| Firmware | **ja**, IFSS- und V.24-Satz | **ja** + **kommentierte Quelle** (JDE, angepasste Fassung) | **nein** | nein |
| Schaltplan | **ja**, 19 Blätter + Servicehandbuch 74 S. | **nein** (Hardware aus Quelle) | ja (5 Blätter) | Servicehandbuch |
| Schnittstelle | IFSS-Modul (praktisch nur dieses), V.24 | IFSS + V.24 an einer SIO (A = V.24, B = IFSS) | Module | V.24/IFSS/Centronics |

Familienbezug: K6316 und K6313 gehören zur selben „Familie neuer mittelschneller Matrixdrucker" (gleiche Bausteine, Druckwagen
mit 6°-Schrittmotor und Taktscheibe, Interfacemodul-Kassetten).  **Was am K6316 entsteht, trägt vom K6313 Druckwerk, Taktierung und die Firmware-Wurzel — E/A-Karte, Kassettenlogik und Interfacemodul nicht** (Revision WP8a/WP8c: K6313 hat eine eigene Karte `K6313Ccb`, §11.5; der K6311 nutzt diese Karte, ist aber nur aus dem ROM erschlossen, WP8e).

### 2.2  K6316 (Schalterdrucker, Bankwesen)

* 1984–1990, 16 110 Stück; Varianten **.01–.05** nach Papiertransport (Rolle ungelocht/gelocht, Aufwickeln, Leporello);
  **programmgesteuerte Schneideinrichtung** (Papier wird nach dem Beleg abgeschnitten), Vorsteckeinrichtung für Formulare/Karten
  (eigener **Vorsteckmotor**), bewegliche Leitbleche.  Auch Einbaugerät im DDR-Geldautomaten; an A5120/K8924/DORAM.
* **Blockschaltbild** (Servicehandbuch): CPU, CTC, 3 × PIO, SIO, RAM, EPROM, Dekoder; Funktionsgruppen Druckkopfansteuerung (DA),
  Druckwagenmotor (DW, Lichtschranke/Taktscheibe), Formularvorschub (FV), **Vorsteckmotor (VS)**, Magnetansteuerung (YA,
  Schneid-/Leitblechmagnete), Anzeigenansteuerung (AA), Schalter.
* **Druckwagen**: 6°-Schrittmotor, 60 Vollschritte je Umdrehung; **Taktscheibe 30 Impulse je Umdrehung** (`/G1`); Nadelfrequenz
  500 Hz; Ansteuerzeitpunkte je Richtung und Nadelspalte versetzt.  Nadelstrom über Timer-Regelung (für uns: Impuls = Punkt).
* **Bedienung**: Netzschalter mit Lampe; Tasten 1 ON/OFF-LINE (S1), 2 LINE FEED ↑ (S2), weitere LINE FEED ↓ / Sonderfunktionen
  (Manual §5.4); **Leuchtdioden H1–H5** (H5 = ON-LINE; Kombinationen zeigen Selbsttest, Lichtschrankentest, Hex-Ausgabe);
  **Drehschalter Betriebsart** Aufwickeln / Schneiden / Endlos (beim Einschalten gelesen); **DIL-Schalter** (u. a. S13/S14 Interface,
  S18 Baudrate/Duplex).  Synchronisation nur bei angeschwenktem Kopf und geschlossener Haube (→ zwei Sensor-Eingänge).
* Selbsttest-Druckbild ist als Foto auf robotrontechnik.de belegt → **Abnahmemuster** für WP6a.
* Firmware: Speichermodul-Kennung „Programmsatz + Interfacevariante" (Servicehandbuch); unsere zwei Sätze = IFSS / V.24.

### 2.3  SD 1157 (Seriendrucker)

* 1981–1987, 16 325 Stück; Typen **264** (132 Spalten, 1 Traktor), **267** (210 Spalten, **2 Traktorbahnen nebeneinander**),
  269 (Grafik, 9×9, nicht von unserer Firmware abgedeckt).  Farbband fest im Gehäuse (Farbbandmotor), 65 kg.
* Die Quelle `doc/printer/sd1157/1157.mac` (5 151 Zeilen, kommentiert) nennt die gesamte E/A (`doc/EPROMS/SD1157/README.md`):
  SIO 7CH–7FH, CTC DCH–DFH, PIO 1 F4H, PIO 2 F8H, PIO 3 ECH (Schalter am Interface), **Nadeln BEH/BFH** (Nadel 1–8 / 9, low-aktiv);
  Interrupts „linker Rand / Papiervorschub 1", „rechter Rand / Papiervorschub 2"; Bits für Farbbandmotor, Fehlerlampe,
  **Online-Lampe**, **Tasten ONLINE, PV1, PV2 (Vorschub links/rechts), TEST, 6/8 lpi**, Papierende, Kopfanschwenksignal.
  Damit lässt sich die Hardware **aus dem, was die Firmware erwartet**, rekonstruieren — Kennzeichnung `[Q]` (aus Quelle) statt
  `[S]` (aus Schaltplan) im Hardwaredokument.
* IFSS-Beschreibung (BÜWK 1983): 9600 Bd, Puffer 1 KiB, Zeichensätze nach RöW 3565-76, Freigabe/Sperre über DC1/DC3, Statusmeldungen.
* **WP0b erledigt: `doc/printer/sd1157/hardware.md`.**  `1157.rom` ≠ Übersetzung von `1157.mac`: Quelle + drei Abweichungen ergeben das ROM
  byteweise (0D20H Schiebepuffer im Eingabepfad, Folgeverschiebung +38H, Auffüllung 2907H–297FH).  **Es gilt das ROM**, die Quelle ist der Kommentar.
  `1157zg.rom` = ZG1 auf 3000H; ZG2 (3400H) fehlt und muss FFH lesen.
* **Der SD 1157 hat keine Motorphasen.**  Die Firmware gibt nur LFG/VW/VTAB aus (Rampen macht die Hardware) und liest LAUF, /VK, Takte (richtungsgetrennt
  2A.0/2B.0) und Ränder; Papier: XPFG/XPWV, Schrittimpulse zurück (1/48″).  Bleiben Rückmeldungen aus: NMI bzw. Havarie.

### 2.4  K6313 — Abzüge seit 2026-11-13 (bis dahin: keiner öffentlich)

**Überholt durch §11:** 19 Abzüge K6311/K6313 von robotron.webhop.net liegen in `doc/EPROMS/K6311/`, `doc/EPROMS/K6313/`.  Der Rest dieses Abschnitts ist der Stand vom 2026-11-05.

Netz- und Vollsuche (Robotrontechnik-Forum, tiffe.de komplett 12 422 Dateien inkl. Archive, CPC-Wiki, oldcomputer.info, GitHub,
archive.org): **kein K6313/K6314-Abzug öffentlich**.  Mikroprogramm + Zeichensatz liegen auf einer ziehbaren EPROM/RAM-Kassette
(EPROMs gelötet); Auslesen mit Prüfklemme oder Anfrage im Forum (kaiOr, Rüdiger; Threads
[6045](https://www.robotrontechnik.de/html/forum/thwb/showtopic.php?threadid=6045),
[8310](https://www.robotrontechnik.de/html/forum/thwb/showtopic.php?threadid=8310),
[6421](https://www.robotrontechnik.de/html/forum/thwb/showtopic.php?threadid=6421),
[8849](https://www.robotrontechnik.de/html/forum/thwb/showtopic.php?threadid=8849)).  Kassette und Hauptplatine müssen
zusammenpassen (alte/neue Elektronik; Kennung `x.nn-3-ZS/PS`, x = 3/5 K6313 alt/neu).  Unterlagen in `doc/printer/k6313/`.

### 2.5  Was das Projekt schon mitbringt

* **Z80, Z80CTC, Z80PIO, Z80SIO** (`core/primitives/`), EPROM/RAM-Geräte, `K1520Bus` (als Bus taugt er auch für Nicht-K1520-Geräte,
  vgl. `Mc80Machine`).
* **Serielle Schnittstellen nach außen** (`core/serial/`: `SerialAnschluss`, `Wandler`, `SerialHub`, Telnet/RFC 2217/Datei,
  Maschinenzeit, Rückstau); IFSS existiert schon als Schnittstellentyp (K8025 DFÜ/IFSS).  **`doc/merkposten/serielle_schnittstellen.md` lesen.**
* Eigenständige Maschinen: `Mc80Machine`, `Pc1715Machine`, `P8000Machine` — Muster für Klasse, Kennung, `k1520_create_*`, Bindung,
  Profil, `boot_trace`/`k1520dbg --machine`.  Nächste freie Kennung: `K1520_MACHINE_… = 7`.
* Gemeinsame Oberfläche: Programmprofil (`app/profil.py`), Aktionen (`app/ui/actions.py`), Statuszeile, Einstellungen, Handbuch,
  Paket/Starter (`packaging/`).  `ScreenWidget` ist für das Blatt ungeeignet (§4.5).
* Z80-Disassembler `tools/z80_disasm2.py` (der einzige — keinen neuen schreiben), Assemblerbau `tools/cpm_bau.py`.

## 3  Leitsätze

1. **Keine Traps, keine Textumleitung.**  Der Original-Z80-Code läuft; Gedrucktes entsteht aus Nadel- und Motorsignalen.
2. **Der Kern kennt kein PDF.**  Er liefert Punkte (Blattmodell), die Oberfläche macht Anzeige, PDF und Druck.
3. **Druckwerk getrennt von der Steuerung** (`core/peripherals/druckwerk/`): eine Schnittstelle `Druckwerk`, Nadeldruckwerk mit
   Parametersatz je Gerät (Spalten, Schritte, Nadelversatz, Bahnen), später Typenrad als zweite Implementierung.
4. **Eine Steuerungsklasse je Gerät** (`K6316Steuerung`, `Sd1157Steuerung`), eine Maschinenklasse `PrinterMachine` darüber —
   kein `if typ == …` im Druckwerk oder in der Oberfläche.
5. **Maschinenzeit, nicht Uhr** (Motor, Taktscheibe, Nadelimpulse, Baudrate).
6. **Die Karte kennt kein Netz** — nur `SerialAnschluss`.
7. **Jede Zahl hat eine Quelle**: `[S]` Schaltplan/Servicehandbuch (Blatt/Seite), `[Q]` Firmware-Quelle (Zeile), `[F]` aus dem
   ROM erschlossen, `[?]` Annahme.  Festlegungen mit Wächter in `doc/merkposten/drucker.md`.

## 4  Entwurf

### 4.1  Schichten

```
app/ (k1520printer)    PrinterWindow · BlattWidget · Bedienfeld je Typ · Einstellungen · PDF/Druck
core/api               k1520_create_printer(typ, konfig) · k1520_printer_*  (C-ABI, additiv, K1520_MACHINE_PRINTER = 7)
core/machines/printer  PrinterMachine — Run-Schleife, Bus, SerialHub, waehlt Steuerung + Druckwerk nach Typ
core/cards/printer     k6316_steuerung (CPU-Platine nach Schaltplan) · sd1157_steuerung (Sloteinheit nach Quelle)
                       je mit Bedienfeld (Tasten, LEDs, Schalter) und Interfacemodul (SIO → SerialAnschluss)
core/peripherals/druckwerk   Druckwerk (Schnittstelle) · Nadeldruckwerk (+ Parametersatz) · Blattmodell · Papierweg
```

### 4.2  Steuerungen (WP2a/b)

* **K6316**: Adresskarte, Dekoder, 5 EPROM-Steckplätze (Reihenfolge V1…V5 prüfen), RAM, 3 PIO / CTC / SIO-Belegung, Interruptkette,
  Bedienmatrix, Sensoren (Kopf angeschwenkt, Haube zu, Papierende, Lichtschranken), Magnete (Schneiden, Leitbleche), Vorsteckmotor —
  **aus den Schaltplänen** (WP0a), Ergebnis `doc/printer/k6316/hardware.md`.
* **SD 1157**: Belegung aus `1157.mac` (WP0b), Ergebnis `doc/printer/sd1157/hardware.md`; was die Quelle nicht hergibt
  (Taktfrequenz, Interruptreihenfolge, Pegel), wird als `[?]` geführt und am Laufverhalten der Firmware geprüft.
* Interfacemodul-Wahl als Konfiguration (`K6316`: IFSS ↔ V.24 wählt zugleich den Firmwaresatz — die Sätze gehören zum Modul).

### 4.3  Druckwerk und Blattmodell (WP3)

* **Die Antriebe sind verschieden, das Blatt nicht.**  K6316: Firmware schaltet Phasen selbst, liest Taktscheibe /G1.  SD 1157: Firmware
  gibt Lauf/Richtung/Tempo vor, die Hardware regelt und meldet LAUF und Takte.  Deshalb **zwei Antriebsklassen** hinter einer gemeinsamen
  Schnittstelle zum Kopf: `Wagen` liefert `position()` und Takt-/Sensorrückmeldungen, `Papierweg` den Vorschub; `SchrittWagen` (K6316)
  und `GeregelterWagen` (SD 1157).  `Nadelkopf` + `Blattmodell` sind gemeinsam: Nadelimpuls + Position + Papierlage → Punkt.
* Ideal bis zum Gegenbeweis: keine Trägheit, außer die Firmware verlangt sie (SD 1157: LAUF binnen µs, Auslauf 8…70 Takte, Impulse < 7 ms).
* **Blattmodell**: Bitmap in Mechanik-Auflösung, wächst nach unten; **Schnitt** (K6316) bzw. Form Feed/Seitenlänge erzeugen eine
  **Blattgrenze** → natürliche PDF-Seiten.  **Zwei Bahnen** (SD 1157/267) = zwei Blätter nebeneinander mit eigenem Vorschub.
  Lesen zeilenpaketweise mit Versionszähler (`k1520_printer_rows`), nie das ganze Blatt je Bild.

### 4.4  Schnittstellen (WP4)

Beide Geräte haben eine **SIO** → Anschluss an `SerialHub` wie K8025/K7028: Name nach Gerätebeschriftung („IFSS", „V.24"), Leitungen,
Rückstau, Maschinenzeit-Baud.  Die Firmware macht das Protokoll (DC1/DC3, Status) selbst.  Centronics/IFSP: nicht im Umfang.

### 4.5  Oberfläche (WP5)

* **`BlattWidget`** (QWidget, kein `ScreenWidget`): weißes Blatt, schwarze Punkte, aktuelle Zeile unten, Blatt schiebt nach oben;
  **Druckkopfmarker** an der x-Position; Zoom, „Blatt folgen".  Zwei Bahnen nebeneinander, Schnittkante sichtbar.
* **PDF** (`QPdfWriter`) und **System-Druck** (`QPrinter`/`QPrintDialog`) über **eine** Zeichenroutine `blatt_zeichnen(painter, seite)`;
  Punkte als gefüllte Kreise in Mechanikmaß, ohne Glättung.  Blatt leeren / neu; nicht im Kern gespeichert.
* **Bedienfeld je Typ** (rechtes Dock, aus Profil/Typbeschreibung gebaut, nicht hart verdrahtet): K6316 Tasten, H1–H5, Netzlampe,
  Betriebsart-Drehschalter, DIL-Schalter, Kopfhebel/Haube; SD 1157 Tasten ONLINE/PV1/PV2/TEST/6-8 lpi, Online-/Fehler-/Netzlampe,
  Interfaceschalter (Baudrate).  Schalterbelegung mit Funktionstext aus dem Manual.
* **Auswahlfeld „Druckertyp"** (Einstellungen ▸ Allgemein, `general.model`): `K6316 (IFSS)`, `K6316 (V.24)`, `SD 1157/264`,
  `SD 1157/267`; **gesperrt** „K6313 (EPROM fehlt)", „SD 1152/257 Typenrad (in Vorbereitung)".  Wechsel = Neuanlage der Maschine,
  Blatt bleibt (Rückfrage) — **Abweichung WP5:** das Blatt liegt im Kern und lässt sich nicht in eine neue Maschine einspielen,
  der Wechsel leert es (die Rückfrage sagt es; §5d).
* **Fenster**: eigenständiges Paket **`app/printertool/`** (Entscheidung WP5, §5d), importiert nur Bausteine (Emulationstakt,
  Statuszeilen-Lampe, Schnittstellen-Reiter, Handbuch, Mehrinstanz) — **`main_window.py` wird nicht zerlegt** (berührt sonst alle sechs Programme).
* Programm `k1520printer` (`app/main.py --machine printer` oder eigener Einstieg), `k1520printer.yaml`,
  `data/default_config_printer.yaml`, Handbuch `app/help/` (Kürzeltabelle = Vertrag, nur `Strg+Umschalt`).

## 5  Arbeitspakete

Je AP: Doku + Test + Commit, **nicht pushen**.  Reihenfolge: WP0a ‖ WP0b → WP1 → WP3 → (WP2b → WP6b) ‖ (WP2a → WP6a) → WP4 → WP5 → WP7.
**Erster lauffähiger Drucker: SD 1157** — die Quelle verkürzt WP0 und jede Fehlersuche; der K6316 folgt als schaltplantreues Gerät.

| AP | Inhalt | Abhängig | Erledigt, wenn |
|---|---|---|---|
| **WP0a** | K6316-Hardware aus 19 Schaltplanblättern + Servicehandbuch: Chipliste, Adress-/E/A-Karte, PIO-Bits, Interruptkette, Sensoren/Aktoren, Bedienmatrix, DIL-Funktionen, Mechanikmaße → `doc/printer/k6316/hardware.md`; Abgleich mit den E/A-Befehlen im ROM (80H–8DH, A6H–A9H) | — | jede E/A-Adresse im ROM ist erklärt |
| **WP0b** | SD-1157-Hardware aus `1157.mac` → `doc/printer/sd1157/hardware.md`; `1157.mac` übersetzen und mit `1157.rom` vergleichen; Zeichengenerator-Lage (3000H/3400H) klären; `.prn` für den Debugger | — | Vergleich dokumentiert, E/A vollständig |
| **WP1** | Gerüst: `PrinterMachine`, Kennung 7, `k1520_create_printer`, `boot_trace`/`k1520dbg --machine sd1157|k6316`, leeres Druckwerk | WP0 | ROM läuft an, Debugger zeigt CPU/Speicher/E/A |
| **WP3** ✔ | `Druckwerk`, `Nadeldruckwerk`, Blattmodell, Papierweg (Bahnen, Schnitt), C-ABI `k1520_printer_*` | WP1 | Unit-Tests `Druckwerk.*`: Phasen → Position, Punktlage vor/zurück, Blattgrenzen (§5b: ohne `SchrittWagen`/Schnitt/zweite Bahn im Druck) |
| **WP2b** ✔ | `Sd1157Steuerung`: E/A nach WP0b, Bedienfeld, Sensoren | WP1, WP3 | Firmware erreicht Grundstellung (Wagen am Rand, Online-Lampe) — **und druckt den Selbsttest** (§5b) |
| **WP6b** ✔ | SD 1157 druckt: Selbsttest (TEST-Taste), Text über SIO (Testfall ohne Netz), Escape-Folgen; ~~2 Bahnen (267)~~ entfällt, §10 | WP2b, WP4 | Selbsttestbild + Textprobe als Wächter (Punktmuster-Prüfsumme) |
| **WP2a** | `K6316Steuerung` nach WP0a, Vorsteckmotor/Schneiden/Leitbleche, Betriebsartschalter, Haube/Kopfhebel | WP1, WP3 | Synchronisation läuft, H-LEDs wie Manual |
| **WP6a** | K6316 druckt: Selbsttest (Foto als Abnahme), Lichtschrankentest, Text über IFSS und V.24, Schnitt | WP2a, WP4 | Wächter wie WP6b |
| **WP4** | SIO beider Geräte an `SerialHub` (IFSS/V.24), Loopback-Test (Port 0) | WP2a oder b | Telnet → Firmware → Punkte (Integrationstest) |
| **WP5** | `k1520printer`: Profil, `PrinterWindow`, `BlattWidget`, Bedienfelder, Typauswahl, PDF, System-Druck, Handbuch, Bindung | WP3 (Rest parallel) | `py_k1520printer_gui` offscreen; PDF enthält die Punkte |
| **WP7** ✔ | Paket (Starter, `MASCHINEN`, Inno), Vorgabekonfiguration, `CLAUDE.md`-Abschnitt, Merkposten, `test-merge --alles` | alles | `py_packaging`, `py_programme`, Windows-Gegenprobe |
| WP8 | K6311/K6313 auf K6316-Basis — **aufgeteilt in WP8a–e, siehe §11**; SD 1152/257 Typenrad als zweite `Druckwerk`-Implementierung bleibt später | Abzüge (da) | — |

### 5a  Stand WP1 (2026-11-05)

Gebaut: `PrinterMachine` (`core/machines/printer/`, Kennung 7, `k1520_create_printer(typ, konfig)`, Python `machine="sd1157"`),
`Sd1157Steuerung` (`core/cards/printer/`: ROM/RAM/ZG nach `sd1157/hardware.md` §2, CTC, 3 PIO, SIO, Kette CTC > PIO 2 > PIO 1 > SIO > PIO 3
als Annahme), `boot_trace --machine sd1157`, `k1520dbg --machine sd1157`; `K6316_*` werden mit Meldung abgewiesen (WP2a).
**Das Original-ROM erreicht mit dem vorlaeufigen Wagen-/Papier-Platzhalter (`sd1157_mechanik_vorlaeufig.h`, faellt mit WP3 weg) die
Leerlaufschleife**: 13 CTC-1-Interrupts (~0,86 Mio. Takte), Wagenfahrt rueckwaerts bis /LR, 24 Anfangs-PV-Schritte (+ Rueckzug), Online-Lampe an,
Fehlerlampe aus, DC1 an den IFSS (Kanal B; am V.24-Betrieb an Kanal A), danach Abfrage von PIO 3 B / SIO-Puffer ab ~5,6 Mio. Takten.
Wächter: `Sd1157Karte.*`, `Sd1157Rom.*`, `K1520PrinterMaschine.*`, `py_c_api`.
Offen aus WP1, in WP3 gelöst (§5b): Startlagen im Randbereich (/VK, /LR), die 56 Papierschritte; ein echtes `.prn` für den Debugger fehlt weiter
(`-l 1157.mac` stimmt nur bis ~00C0H, 132 Zeilen unbekannt, Adressen driften).

### 5b  Stand WP3 / WP2b (2026-11-05)

**Gebaut.**  `core/peripherals/druckwerk/` (Bibliothek `k1520_druckwerk`, ohne Qt/PDF, ohne Bezug auf ein Gerät): `Blattmodell` (Bit je Punkt, Blöcke zu 64 Zeilen,
wächst nach unten, `version()`, `lies(y0, n, …)`, `blattAscii`), `Nadelkopf` (Muster + Position + Papierlage → Punkte, Flugzeit je Richtung, Kopf anliegend),
`Papierweg` (Freigabe/Richtung/Hochlauf → Rückmeldeimpulse, Schritte je Richtung, Nettolage, Papierende), die Schnittstelle `Wagen` (`WagenEingang`: LFG/VW/VTAB **und**
`phasen`; `WagenRueckmeldung`: LAUF/Takte **und** `g1`, Ränder) mit `GeregelterWagen` (SD 1157), `Nadeldruckwerk` (fügt zusammen) und `NadeldruckParameter`
(Raster, Nadelteilung, Punktmaß, Flugzeit; `[?]` kommentiert).  `SchrittWagen` (K6316) ist **nicht** gebaut; die Schnittstelle trägt ihn ohne Umbau.
`sd1157_mechanik_vorlaeufig.h` ist entfernt; `Sd1157Steuerung` besitzt ein `Nadeldruckwerk`, löst den Anschlag mit der **Flanke FGDI (2B.5) 0 → 1** aus
und bietet Tasten/Schalter als API (`setOnline`, `setTastePV1/2`, `setTasteTest`, `setKopf`, `setLpi6`, `setPapierDa`; Lampen `lampeOnline`/`lampeFehler`).
C-ABI additiv `k1520_printer_*` (Blatt lesen je Zeilenbereich, Größe, Version, Kopf, Raster, Tasten, Schalter, Lampen, Blatt leeren), ctypes-Deklaration,
`py_c_api`; Debugger `blatt [info | x0 y0 [b h [sx sy]]]` (ASCII-Abzug), Diagnose der Druckertests.

**Ergebnis der Abnahme.**  Der Selbsttest (TEST im Offline-Zustand, Original-ROM) druckt **Punkt für Punkt das, was ein Orakel aus `1157zg.rom` und dem Text ab 0300H
berechnet**: Titelzeile (13 normale Zeichen, 15 breite, 20 schräge), Zeile mit 63 und Zeile mit 31 Zeichen, Zeilenabstand 24 Rasterzeilen (6 lpi) bzw. 18 (8 lpi),
Formularvorschub am Ende = Formularlänge (192/288/576), kein Punkt außerhalb.  Die mittlere Zeile druckt die Firmware **rückwärts**, die beiden anderen vorwärts.

**Befunde** (alle in `doc/merkposten/drucker.md` mit Wächter):
1. **Flugzeit des Kopfes.**  Die Firmware löst rückwärts einen Takt später aus als vorwärts; beide Richtungen landen nur bei 0,5 Takt Flugzeit je Richtung auf denselben
   Punkten (Raster x = 2 Pixel je Takt).  Das ist am Verhalten der Firmware gemessen, nicht an einem Handbuch.
2. **PIO-Interrupts im Modus 3 sind Ereignisse** (`Z80PIO::setModus3Flanke`, U855 §6.4).  Im Pegelmodus löste jede Auffrischung der Eingangspegel bei /LR = 0 den
   Randinterrupt neu aus und setzte die Spaltenzähler zurück: der Druck fuhr in die falsche Richtung.
3. **/VK deckt den /LR-Bereich ab** (`vk_bis ≥ lr_bis`; WP0b hatte ≤ 10 angenommen).  Sonst bleiben Startlagen im Randbereich (auch „Wagen steht nach /RESET links“) am Anschlag hängen.
4. **Die 56 Papierschritte sind Firmware**: 24 vor, Rückzug 24 + 4 zurück (`add a,4` bei m24dd), 4 vor; Nettolage 0.

**Offen.**  [Zweite Bahn entfällt, §10.]  `SchrittWagen`, Schnitt/Blattgrenzen, Textprobe über die SIO (WP4/WP6b), die offenen `[?]` aus
hardware.md §14 (Takte je Zoll, Nadelteilung, Punktmaß, Flugzeit absolut).  Die Online-Lampe und das Flag `off` der Firmware folgen der Taste ONLINE nur beim Anlauf (Merkposten).

### 5c  Stand WP4 / WP6b Text (2026-11-05)

Gebaut: `Sd1157Steuerung::Anschluss` je SIO-Kanal (IFSS = B, V.24 = A), `PrinterMachine` mit `SerialHub` (Index 0 IFSS, 1 V.24), alter Unterbau, Baudfilter für CTC 0 (Druck-Zeitbasis TC = 23 zählt nicht als Baud),
gemeinsames Druckbild-Orakel `tests/support/sd1157_orakel.h`.  **ROM:** der Abzug leert den Eingabepuffer nie (Byte 0D34H, Hypothese Bitkipp 0BH → 2BH); `doc/EPROMS/SD1157/1157_repariert.rom` ist Vorgabe
(`Config::rom_repariert`, `rom=original|repariert`), der Abzug bleibt unverändert (Merkposten 17).  **Wächter:** `Sd1157Text.*` (Orakel: kurzer Text, mehrere Zeilen in beiden Richtungen, TAB, FF, > 132 Spalten,
Breit/Schräg/Normal, 8 lpi, ungültige ESC, DA/DSR, Papierende, offline im Auftrag), `Sd1157Fluss.*` (DC1/DC3 am IFSS und V.24), `Sd1157Telnet.*` (Loopback, Port 0), `Sd1157Seriell.*`, `Sd1157Online.*`,
`Sd1157Selbsttest.FgdiFolgt…` (Flugzeit: Emulator richtungssymmetrisch, Merkposten 4).  **Offen / [?]:** [Doppelbahn entfällt, §10;] HPA/VPA-Spaltenzählung, Zeichen 80H–FFH, ROM-Bestätigung 0D34H am Original-EPROM.

### 5d  Stand WP5 — Oberfläche `k1520printer` (2026-11-06)

**Entscheidung Fensteraufbau.**  Eigenes Paket `app/printertool/` (Muster `app/kermittool/`), kein Profil in `main_window.py`: dessen Fenster ist um
Bildschirm, Laufwerke und Tastatur gebaut (3 200 Zeilen, `Programmprofil` mit über 40 Feldern); ein Drucker hätte dort fast alles abschalten müssen und
jede Änderung berührte die sechs Rechnerprogramme.  Das Druckerfenster importiert nur Bausteine: `Emulationstakt` (Faden + Kernsperre), `SerialWidget` (der
vorhandene Schnittstellen-Reiter, nicht nachgebaut), `PanelLamp` (Statuszeile), `HelpWindow`, `config_io`/`paths`/`instanz`.  Eigene Dateien: `main.py` (Einstieg,
`--instance/--config/--paths/--help` vor den Qt-Importen), `fenster.py` (`PrinterWindow`, `BlattAnsicht`, `DruckerEinstellungen`), `blatt_widget.py`,
`bedienfeld.py`, `typen.py` (Typbeschreibung als DATEN), `ausgabe.py`, `aktionen.py`, `konfig.py`, `help/handbuch.md`; Starter `run_k1520printer.sh`,
Vorgabe `data/default_config_printer.yaml` (IFSS lauscht als Telnet-Server auf 5000), Konfiguration `k1520printer.yaml`.

**Gebaut.**
* `BlattWidget` (QWidget, kein OpenGL): weißes Blatt, Punkte aus `printer_sheet_rows` in `QImage.Format_Mono`; hält eine eigene Kopie und liest nur ab
  `min(Kopfzeile vorher, jetzt) − 48` nach (der Kern kennt nur einen Versionszähler); Blatt folgen (Kopfzeile bei 80 % der Höhe, das Blatt schiebt nach oben),
  Zoll/Einpassen/Strg+Mausrad, Rollbalken, Perforation an der Seitenlänge, Kopfband über den neun Nadeln + Dreieck auf der Schiene (x aus `printer_head`).
  Raster und Blattbreite kommen aus `printer_geometry`/`printer_sheet_size`, nichts davon ist in Python festgelegt.
* Bedienfeld aus `Typbeschreibung` (Tasten, Lampen; ein K6316 braucht nur eine neue Beschreibung): ONLINE/Kopf/6-8 lpi/Papier rastend, PV1/PV2/TEST als Taster,
  Lampen Netz/ONLINE/FEHLER, Netzschalter (Aus: Takt steht, Lampen dunkel, Blatt bleibt; Ein: `/RESET`).  **TEST-Hinweis** (Merkposten Nr. 10): dauernde Zeile
  unter dem Bedienfeld und eine Warnzeile mit Knopf „Neustart jetzt“, sobald ONLINE-Schalter und -Lampe auseinanderlaufen.
* Einstellungen: Druckertyp (SD 1157/264 wählbar; /267, K6316 IFSS/V.24, K6313, SD 1152/257 ausgegraut mit Begründung), ROM-Fassung repariert/Original-Abzug mit
  Erklärung (Nr. 17), Schnittstelle IFSS/V.24, Tempo, Seitenlänge; Reiter *Schnittstellen* = `SerialWidget`.  Statuszeile: Typ, online/offline/Netz aus/FEHLER,
  Schnittstelle, Server/Verbindungen, Kopfposition (Zoll, Spalte bei 10 cpi).
* Ausgabe: `blatt_zeichnen` für **PDF** (`QPdfWriter`, 600 dpi, Seite 15 Zoll × Seitenlänge) und **System-Druck** (`QPrinter`/`QPrintDialog`, auf die Papierbreite
  eingepasst, nie vergrößert).  Punkte = gefüllte Kreise; Folgen gesetzter Pixel werden in Punkte zu je 2 Pixeln zerlegt (Darstellung, kein Mechanikmaß `[?]`).
* Wächter `py_k1520printer_gui` (18 Fälle, Label `fach_drucker`): Selbsttest über die Tasten = Blatt im Kern bytegleich, Kopf fährt, Blatt wächst und schiebt nach
  oben, Lampen/Netz aus/ein, TEST-Hinweis, Typsperre, Neuanlage nach Rückfrage (ROM-Byte 0D34H geprüft), Konfigurationsrundlauf, **PDF zurück in ein Bild gerechnet
  (jeder Punkt an seiner Stelle, sonst weiß)**, Seitenumbruch, Druckdialog mit Ersatz, Telnet-Rechner druckt Text mit DC1/DC3, Kürzelvertrag, Handbuch, `--help/--paths`.

**[Überholt durch §5e/Merkposten Nr. 27 — Formularlänge und Schalterblöcke sind seither über die ABI erreichbar.]  Nicht über die C-ABI erreichbar (damals):** Formularlängen und Schalterblock-Einzelschalter (BCH.2–7, PIO 3 A) — nur `schnittstelle`/`rom` gehen
beim Anlegen als Konfigurationstext; die Baudrate bestimmt der Gast (CTC 0).  Die Seitenlänge für PDF/Druck ist deshalb eine Oberflächeneinstellung; der Formularvorschub
der Firmware fährt 12 Zoll (Vorgabe des Kerns).  Zweite Bahn und Doppelbahn im Druck: entfallen (§10); K6316: WP2a (§5e).

### 5e  Stand WP2a / WP6a — K6316 (2026-11-06)

**Gebaut.**  `PhasenMotor` (4-Phasen-Schrittmotor, Feld → Halbschritte), `SchrittWagen` (Phasen /SW1–4 rein, /G1 raus, harte Anschläge, Kopflage zwischen den Schritten), `Papierweg::phasen`
(Papier-/Vorsteckmotor), `K6316Steuerung` (`core/cards/printer/`: ROM je Modul, RAM 3800H, 2 CTC, 3 PIO, SIO, Kette CTC D5 → D10 → PIO D12 → SIO, Diodenmatrix mit 16 DIL-Schaltern, Tasten S1–S5,
Drehschalter, MHA/PE/FSO/FSU, Messer), die gemeinsame Schnittstelle `DruckerSteuerung` (`PrinterMachine` kennt beide Geräte), Blattgrenzen (`Blattmodell::schnitt`), C-ABI (Merkposten Nr. 27),
Debugger `--machine k6316[-v24]`, Oberfläche (Typen K6316 IFSS/V.24 mit Bedienfeld aus `typen.py`: Tasten, Lampen H1–H5, Drehschalter, DIL-Gruppe, Schnittkante), Wächter `K6316*` (19 Fälle), `py_k1520printer_gui` (+3).

**Ergebnis der Abnahme.**  Das Original-ROM beider Module läuft bis in die Grundstellung (~8 s), meldet DC1 (V.24 mit S17 und Gegenstelle), druckt den Selbsttest (S5) und Text über die SIO.  Das Druckbild
stimmt **Punkt für Punkt mit dem Zeichengenerator im ROM** überein (Orakel `tests/support/k6316_orakel.h`; Kennungszeile und je 26 Zeichen der Testzeilen, vor- und rückwärts).  Die offenen Punkte
aus `hardware.md` §8 sind beantwortet (§9 dort), einschließlich zweier **Berichtigungen** (D12 A.0/.2 = Lichtschranken FSO/FSU; H4 hängt an /SPAUS, nicht an AM4).

**Annahmen `[?]`.**  Position der Einheit des Messers (3,5 mm unter Nadel 7) und `schnitt_pos` (1600; die Firmware fährt bis 1621), mechanischer Nullpunkt links (Textbeginn bei ~1,4″), rechter Anschlag
(1800), Punktmaß (3 × 2), Flugzeit/Nacheilung ±2 Pixel, Herkunft /MPV, Reihenschaltung Haube/Kopf auf MHA, Funktion S11/S12/S15/S16/S18–S23.

**Nachtrag 2026-11-08:** Breit-/Schrägschrift, Unterstrich und Unterlängen (Nadel 9) sind aus dem ROM abgeleitet (`hardware.md` §10) und im Orakel geprüft (Merkposten Nr. 22).

Hexdump (S2 beim Einschalten, 12,5 Z/Z, 7-Bit-SIO) geprüft.

Lichtschranken- und Vorstecktest (S1/S3 beim Einschalten) geprüft.

**Vorsteckbeleg (2026-11-08):** `Vorsteckbeleg` (Kern) + Blatt-Bahn 1 der C-ABI (`k1520_printer_beleg_*`); Firmware-Messungen in `hardware.md` §11.8, Merkposten Nr. 30.

**Offen.**  Bedienung des Belegs in der Oberfläche (Einlegen/Entnehmen, Belegblatt anzeigen), V.24-Satz am Beleg nachmessen, D/P einzeln (Foto/Maße des Schachts), K6313 (kein Abzug).

### 5f  Stand WP7 — Paket und Werkzeugmenü (2026-11-06)

**Paket.**  `k1520printer` ist wie Kermit/DiskTool ein eigener Starter in derselben Installation, aber ohne eigene Bibliothek (er nutzt `libk1520core`, `K1520_MACHINE_PRINTER = 7`) und ohne
Kommandozeile: Vorlagen `printer_launcher.sh/.cmd` (nicht `launcher.*`: das wählt ein Maschinenprofil), `k1520printer.desktop.in`, Eintrag in `WERKZEUGE` (`install.sh`), Startmenü-Eintrag im `.iss`,
`default_config_printer.yaml` nach `share/k1520emu/`, `app/printertool/` samt Handbuch.  `slim.py` behält nun auch das Qt-Plugin-Verzeichnis **`printsupport/`** (CUPS bzw. Windows-Druckdienst) —
ohne das hätte `QPrintDialog` im Paket keinen Drucker.  Rauchtests (install.sh, .iss, beide `release.yml`-Jobs): `k1520_create(7)`; install.sh/.iss bauen zusätzlich das `PrinterWindow`
(Konfiguration im Wegwerfordner).  **Werkzeugmenü:** Emulatoren und DiskTool haben *K1520 Printer starten* (ohne Kürzel, `programme.PRINTER`); K1520 Kermit und K1520 Printer selbst haben
kein Werkzeugmenü (das Muster sieht es dort nicht vor).  Handbücher: Emulator („Einen Drucker anschließen“: Schnittstelle als Telnet-Client auf `127.0.0.1:5000`), DiskTool.
Wächter: `test_der_drucker_ist_in_allen_paketwegen` u. a. in `py_packaging`/`py_programme`.

## 6  Tests und Wächter

* Registrierung über `k1520_add_test()`; neues Fachlabel `fach_drucker` in `tools/test_auswahl.py` (Wächter `py_testauswahl`).
* Unit: `tests/unit/peripherals/druckwerk`, `tests/unit/cards/printer` (Adressen, Bedienmatrix, Sensoren).
* Integration: Original-Firmware bis Grundstellung, Selbsttest, Text über SIO (`runSmallUntil()`, 100 000er Stapel).  Druckbild-Wächter
  = Prüfsumme des Punktmusters eines definierten Ausschnitts, dazu ein ASCII-Abzug im Fehlerfall.
* Python: `py_c_api` (C-ABI ↔ ctypes mechanisch), `py_k1520printer_gui` (Blatt wächst, Kopfmarker, Taster/LEDs, Typauswahl, PDF-Punkte
  zählen statt „Datei existiert", QPrinter mit Ersatz).  Umlautfreiheit im Code (`py_keine_umlaute`).
* EPROM-Abzüge sind Eingaben, keine Fixtures zum Ändern; Tests lesen sie aus `doc/EPROMS/` bzw. eingebettet als `*_data.h`
  (`tools/eprom_to_h.py`, wie bei den Rechnern).

## 7  Umsetzung mit Agenten

`test-runner`, `code-explorer`, `log-trace-analyzer` ohne Rückfrage; `cpp-coder`, `boot-disasm-analyst` nach Freigabe.

| AP | Agent | Warum |
|---|---|---|
| WP0a | Opus selbst, Vorarbeit `general-purpose` (Blätter zuschneiden/vergrößern) | Schaltplanlesen ist Urteil |
| WP0b | `boot-disasm-analyst` (Übersetzen/Vergleich/E/A-Tabelle) | mechanisch, Quelle vorhanden |
| WP1, WP2a/b, WP3, WP4 | `cpp-coder` (Sonnet; WP3 bei Bedarf Opus) | Muster Mc80/Pc1715, K8025 |
| WP6a/b | `boot-disasm-analyst` + `log-trace-analyzer` | Firmware-Verhalten, Traces |
| WP5 | `general-purpose` (Sonnet), Review Opus | GUI-Muster vorhanden |
| alle | `test-runner` am AP-Ende, **sequenziell** (gemeinsames `build/`) | |

Parallel gefahrlos: WP0a ‖ WP0b (nur Lesen/Doku).  Bauende Agenten nacheinander oder mit `isolation: "worktree"`.

## 8  Risiken

* **SD-1157-Hardware ohne Schaltplan**: Taktfrequenz, Interruptprioritäten, Sensorpegel sind erschlossen.  Läuft die Firmware nicht
  an, ist zuerst die Rekonstruktion verdächtig, nicht die Firmware.
* **SD-1157-Firmware ist eine Anpassung** (JDE), kein Werksoriginal — für die Emulation unerheblich, im Handbuch kenntlich machen.
  Das ROM blockiert je Zeichen ≈ 8,5 ms mit DI (Schiebepuffer) und sendet DC1 nur auf V.24: **gastseitiges Verhalten, nicht „reparieren"**.
* **K6316-Spezialmechanik** (Vorsteck, Schneiden, Leitbleche) verlangt Sensorrückmeldungen, die die Firmware vielleicht prüft;
  fehlende Rückmeldung = Fehlerzustand → als Erstes im Selbsttest sichtbar.
* **Mechanikgenauigkeit** (Nadelversatz, Punktgröße) aus Handbuchwerten; Versatz zwischen Vor-/Rückwärtsdruck wird an der Firmware
  gemessen, nicht in der Anzeige kaschiert.
* **System-Druck/PDF** nur über Qt-Print-Support (Linux/Windows).
* **Speicher**: 210 Spalten × lange Rolle → Blattmodell in Kacheln/Zeilenblöcken, nicht als ein großes Bild.

## 9  Offene Fragen an den Anwender

Entschieden 2026-11-05: **SD 1157 zuerst** lauffähig, K6316 danach; K6316 mit **beiden Firmwaresätzen** (IFSS und V.24) als Typen;
**SD 1157/264 zuerst**, 267 mit zwei Bahnen in WP6b — **überholt 2026-11-06: 267 entfällt (§10).**

* **F4**  Summer/Geräusche: nur Anzeige oder Ton?  Empfehlung: abschaltbar, Standard aus.
* **F6**  SD 1157: Kennung 4/5/6 aus EBD/BDA; 4 = 264 und 6 = 267 plausibel, 5 ungeklärt.  Offene Punkte mit Prüfkriterium: `sd1157/hardware.md` §14.
* **F5**  ~~K6313-Kassette: wird ausgelesen oder angefragt?~~  Erledigt 2026-11-13: Abzüge liegen vor (§11).

## 10  Nicht im Umfang

Centronics/IFSP-Module (auch K6313/K6311, Festlegung 2026-11-15, §11.6), Farbdruck, SD 1157/269 (Grafik), Netzwerkdruckprotokolle (LPD/IPP), virtueller CUPS-Drucker des Wirtssystems.

**SD 1157/267 mit zweiter Papierbahn (Festlegung des Anwenders, 2026-11-06):** wird nicht umgesetzt — sehr selten verwendetes Merkmal.  Der Typ bleibt in der Oberfläche sichtbar gesperrt
(`typen.GESPERRT`, „nicht vorgesehen"), PV2 ohne Wirkung.  Was der Kern dafür schon trägt (Papierweg mit Bahnindex, Formularlänge 2, `SW_FORMULAR2`) bleibt, wird aber nicht weiterverfolgt
und nicht ausgebaut; insbesondere schreibt `Nadeldruckwerk::feuere` weiter nur auf Bahn 0.

## 11  K6311/K6313 und ROM-Wahl (Festlegung des Anwenders 2026-11-13, WP8)

### 11.1  Befund

* **Quelle der Abzüge:** `https://robotron.webhop.net/docs/drucker/` (Excel-Export, Liste in `index-Dateien/sheet001.html`, Dateien unter `mikroprogramme/`),
  52 `.bin` insgesamt.  Geholt 2026-11-13: **K6311 (5)** und **K6313 (14)** nach `doc/EPROMS/K6311/`, `doc/EPROMS/K6313/` (MD5 in den README, WP8a).
  Nicht geholt (nicht im Auftrag): K6314 (21), K6316 (5, andere PGM-Nummern als unsere tiffe-Sätze), K6319, Präsident 6313C/6320.
* **Größen:** 10 KiB (5 × 2 KiB, alte Platine), 16 KiB, 32 KiB (neue Platine/IBM; im K6313 nur `5c53a001`), K6311 8 oder 10 KiB.
* **Gleiche Familie wie K6316:** jeder Abzug beginnt mit `DD 21 24 38 31 …` (`LD IX,3824H`, `LD SP,…`), also dieselbe Firmware-Wurzel.  Stack: K6311 alt/K6316 `3C00H`, K6313-ISO (`30835007`) `F000H` →
  **die 16-KiB-Fassungen laufen auf anderem Speicherplan** (Hypothese [?], WP8a klärt).
* **Zwei Quellen für das Verständnis der Kassette** (Forum robotrontechnik.de, Thread 6421, 24 Beiträge): Die Interfacekassette trägt EPROM **und** RAM (HY6116); das Interface führt nur A0–A14 (32 KiB);
  eine LS00-Logik schaltet oberhalb der ROM-Endadresse auf das RAM um (Signal B8, **Quelle widersprüchlich [?]**).  Kassette und Hauptplatine müssen zusammenpassen (alt/neu).
  Etikett `x.nn-3-ZS/PS`: x = 3/5 K6313 alt/neu, 4/6 K6314, `/PS` = Programmstand; Kürzel der Zeichensätze/Schnittstellen (EPS, IBM, SCH, ISO, KYR, HCD, CL, INT.) sind im Thread nicht erklärt —
  maßgeblich ist `Drucker_K6313_K6314_Dipschalter_usw.pdf` (WP8a).
* **Etiketten ≠ Inhalt:** der Thread nennt einen umgebauten Modul mit Etikett 3.08-3-52/07 und Inhalt 3.65-3-92/11; die Liste führt `33538511` mit defektem 2. ROM, `30835207` nur 10k beschrieben.
  → **Der Katalog trägt eine Güte** (`ok` / `unvollstaendig` / `defekt` / `unbekannt`), sichtbar im Dialog.
* **K6311:** zu ihm liegt **keine Unterlage** im Projekt (kein Schaltplan, kein Handbuch).  Er teilt die Firmware-Wurzel; seine Hardware wird aus dem ROM (E/A-Adressen) und dem Vergleich mit K6316/K6313
  erschlossen — **Annahmen als [?] kennzeichnen, nie als Befund**.  Der Anwender fragt dort ausdrücklich nach Abzügen (Thread 022), also ist Prüfen am Original später möglich.

### 11.2  ROM-Wahl (Entwurf)

* **Ein Katalog `data/drucker_roms.yaml`** (Installation: `share/k1520emu/`, aufgelöst in `app/paths.py`, **keine** Repo-Pfade in der Oberfläche): je Eintrag `schluessel`, `typ` (`sd1157|k6311|k6313|k6316-ifss|k6316-v24`),
  `titel`, `pgm` (Etikett), `datei` (relativ zu `doc/EPROMS`/`share/…/eproms`), `groesse`, `md5`, `schnittstellen`, `zeichensatz`, `guete`, `hinweis`, `vorgabe: true` (genau einer je Typ), `aliases`.
  SD 1157 (`repariert`/`original`) und K6316 (I/V-Sätze) ziehen in den Katalog um; die bisherigen Schlüssel leben als Aliase weiter (Konfigurationen, `rom=repariert`).
* **Kern:** neuer Konfigurationsschlüssel `romdatei=<Pfad>` (Satz aus einer Datei, Länge/Aufteilung je Typ geprüft; Fehler = klare Meldung), der bestehende `rom=` bleibt als Kurzform.  Der Kern kennt den Katalog **nicht** (wie bei Greaseweazle: Auflösung gehört in `app/`).
* **Oberfläche:** Auswahlfeld „EPROM" im Bedienfeld/Typdialog, nur bei Typen mit mehr als einem Eintrag; Vorgabe markiert; Güte-/Hinweiszeile; Wahl wird je Typ in `k1520printer.yaml` gemerkt (`printer.rom.<typ>`).  Ein Wechsel startet den Drucker neu (wie Typwechsel heute).
* **Paket:** die Abzüge gehören ins Paket (`build_payload.sh`, `.iss`), Wächter `py_packaging`; Katalog-Wächter prüft Schema, MD5 gegen die Dateien, genau eine Vorgabe je Typ, Aliase (Muster `FormatKatalogSchema.*`).

### 11.3  Arbeitspakete

| AP | Inhalt | Agent | Erledigt, wenn |
|---|---|---|---|
| **WP8a** | Befund: (1) Katalogdaten aus der webhop-Tabelle + `Drucker_K6313_K6314_Dipschalter_usw.pdf` in `doc/EPROMS/K6311|K6313/README.md` (MD5, Güte, Etikettenschlüssel); (2) K6313-Hardware aus `K6313_14_Serviceschaltplaene.pdf`/`ServiceHB` nach `doc/printer/k6313/hardware.md` (Unterschiede zu K6316: Speicherplan, Dekoder, Kassette ROM/RAM-Umschaltung, E/A, Bedienmatrix, Sensoren, Motoren, DIL); (3) E/A-Diff K6311↔K6313↔K6316 per `tools/z80_disasm2.py` | `boot-disasm-analyst` (Sonnet) | jede E/A-Adresse beider Familien erklärt oder als [?] benannt; Entscheidung, welche Abzüge der Kern fährt |
| **WP8b** | ROM-Wahl-Infrastruktur: Katalog + Schema-Wächter, `romdatei=` im Kern (SD 1157/K6316 zuerst, ohne Verhaltensänderung), `app/paths.py`, Auswahlfeld, Konfiguration, Paket | `cpp-coder` (Kern/Wächter), `general-purpose` Sonnet (GUI) | bisherige Tests grün; Vorgabe-Wahl = bisheriges Verhalten; neuer Wächter `DruckerRomKatalog.*`, `py_k1520printer_gui` |
| **WP8c** | `K6313Steuerung` auf K6316-Basis nach WP8a (`core/cards/printer/…`), Typ `k6313` in `typen.py` (aus `GESPERRT` heraus), `boot_trace`/`k1520dbg --machine k6313`, C-ABI additiv | `cpp-coder` | Grundstellung erreicht, H-LEDs/Selbsttest-Muster |
| **WP8d** | K6313 druckt: Selbsttest, Text über V.24/IFSS/Centronics-frei, Orakel gegen Zeichengenerator im ROM, je ein Wächter je Fassungsfamilie (10 KiB, 16 KiB) | `boot-disasm-analyst` + `cpp-coder` | Wächter wie WP6a |
| **WP8e** | K6311 nach den Annahmen aus WP8a, kenntlich als „Hardware erschlossen" im Handbuch | `cpp-coder` | läuft an; Befund in Merkposten, offene [?] gelistet |

**Stand WP8b (2026-11-14): fertig** — `data/drucker_roms.yaml` (SD 1157 repariert/original, K6316 IFSS/V.24), `romdatei=` im Kern, `app/paths.py`/`app/printertool/rom_katalog.py`, Feld „EPROM“ (nur bei > 1 Eintrag), `printer.rom.<typ>`, Paket, Wächter `DruckerRomKatalog.*`; Vorgaben unverändert, K6311/K6313 noch nicht im Katalog; Merkposten Nr. 31.

**Stand WP8c (2026-11-14): Grundstellung erreicht, Selbsttest hängt** — `K6313Ccb` (`core/cards/printer/k6313_ccb.*`), Typ `k6313` (C-ABI Typ 3, `typen.py`, `boot_trace`/`k1520dbg --machine k6313`), Katalog (33538013 Vorgabe + 4), Merkposten Nr. 32.  Firmware 33538013 synchronisiert den Wagen, ON LINE an, DTR/Papierende über SIO-Kanal B; Tastenmatrix aus dem ROM abgelesen.  Die Einschalt-Testfunktionen laufen nicht an: CTC D101 Kanal 1/2 (Zählbetrieb) bekommen keine CLK/TRG-Impulse (Verdrahtung unbelegt), es wird keine Nadel geschrieben — Aufgabe für WP8d.

**Stand WP8d (2026-11-14): K6313 druckt (Fassung kurz, V.24)** — Selbsttest (LF vor: Kennungszeile + Zeichenumlauf, LF zurück: nur Umlauf) und Text über V.24 stimmen Punkt für Punkt mit dem Zeichengenerator im ROM überein (Orakel `tests/support/k6313_orakel.h`, Wächter `K6313Modul.*`, `K6313Ccb.HcpVon…`, Fachlabel `fach_drucker`).  Ursachen der WP8c-Hänger: **(1)** D102 ZC/TO0 (HCP, Draht 57) speist C/TRG1 und C/TRG2 von D101 laut Plan — der WP8c-Versuch scheiterte nur daran, dass `clkTrg` die Interruptkette nicht neu bewertete; **(2)** das Druckbild brauchte zweites RAM-Fenster 3000H–33FFH (Punktdaten) und Papier/Raster-Geometrie; **(3)** Phasenregister D115 muss im Zählbetrieb mit der CMT-Flanke takten, sonst wandert die Interruptlatenz in die Wagenlage.  Geometrie aus der Firmware gemessen: 160 Einheiten je Zoll, 12 Spalten (1/120″) je Pica-Zeichen, gerade Nadeln 4 Spalten verzögert.  **Offen:** 16-KiB-Fassung (Speicherplan lang, braucht für `37239503` die Centronics-PIO — nicht eingebaut), ESC-Folgen (Elite/Breit/Fett: kein Orakel, Spaltenraster ungeklärt), Nadelpolarität/`/PC`-Freigabe, Summer, IFSS/Centronics.  Merkposten Nr. 33.

**Stand WP8e (2026-11-14): K6311 läuft an, druckt nicht — Hardware erschlossen [?]** — Typ `k6311` (C-ABI Typ 4, `K6313Ccb::Config::k6311`, `PrinterMachine::Config::Typ::K6311_V24`, `boot_trace`/`k1520dbg --machine k6311`, `typen.py`, Katalog mit 12128062 (Vorgabe) und 12128062_1, Handbuch „K6311: Hardware erschlossen [?]“, Wächter `K6311Modul.*`, Merkposten Nr. 34).  Es gibt **keinen Schaltplan**; die E/A-Landkarte 7CH–FBH ist die des K6313 (Adressen aus dem ROM bestätigt), aber **Bit-Belegungen weichen ab**: das ROM liest BCH/BDH als Baud-/Formatschalter (Eingangs-PIO, Quelle der Bits [?]), schreibt den Wagen als Zwei-Bit-Code nach D104 A4/A5 und die Formularmuster 93H/1EH/38H/F0H nach F9H, DDH nur beim Einrichten.  Nachgebildet nur das Belegte (BCH/BDH, Anlauf bis zur Leerlaufschleife, Texteingang); **Wagen, Formularmotor, Nadeln, Tasten, Lampen ON LINE/ERR/PE nicht** — es wird nichts gedruckt, Bedienfeld nur POW.  Nicht im Katalog: 12228062 (Centronics), 13120012/13325212 (8 KiB; der Kern verlangt 10 240 B).

Reihenfolge: **WP8a ‖ WP8b** (a nur lesend/Doku, b baut) → WP8c → WP8d → WP8e.  Je AP Doku + Test + Commit, **nicht pushen**; Bau-Agenten nacheinander (gemeinsames `build/`).
Gesperrt bleibt: SD 1152/257, K6314 (Abzüge liegen vor, aber nicht im Auftrag), 32-KiB-Fassungen, solange WP8a keinen Speicherplan dafür belegt.

### 11.4  Risiken

* K6311 ohne Schaltplan: Hypothesen statt Befunde; Falschmeldungen schlimmer als Lücken.
* 16-KiB-/32-KiB-Fassungen brauchen evtl. eine zweite Platinenvariante (neue Elektronik) — dann zwei Speicherpläne im Kern, nicht einen aufgeweichten.
* Defekte/unvollständige Abzüge (`33538511`, `30835207`) dürfen nicht Vorgabe werden und laufen u. U. nicht an — kein Emulatorfehler suchen, bevor die Güte geprüft ist.

### 11.5  Stand WP8a (2026-11-14) — Befund, Entscheidungen für WP8c–e

Volltext: `doc/printer/k6313/hardware.md`, `firmware_diff.md`, `doc/EPROMS/K6311|K6313/README.md`.  Festgelegt:

* **K6311 und K6313 teilen eine E/A-Karte (7CH–FBH), der K6316 hat eine andere (80H–AFH)** — keine gemeinsame Adresse.  Die CCB hat **keinen Dekoder**, jeder Baustein hängt mit /CE an einer Leitung A2…A7
  (SIO 7C, PIO BC, PIO DC, CTC EC, CTC F4, PIO F8).  → **Eigene Karte `K6313Ccb`, die K6316-Karte wird nicht aufgeweicht** (Revision des Satzes „trägt den K6313 fast vollständig" aus §2.1: Druckwerk, Taktierung
  und Firmware-Wurzel ja, E/A-Karte und Kassettenlogik nein).
* **Zwei Speicherpläne** (kein dritter): *kurz* (10 KiB; K6311 8 KiB) ROM 0000–27FFH, RAM 3800H, Stack 3C00H; *lang* (16 KiB) ROM 0000–3FFFH, RAM F000–F7FFH, Stack F800H.  `5c53a001` (Etikett 32k) läuft auf *lang*.
* **Güte-Änderungen gegenüber dem ersten Entwurf:** `30835207` = `unbekannt` (nicht „nur 10k": Daten bis 3FFFH); `36539211` trägt innen die Kennung 4.65-3-92/10 (K6314) und eine Nullfläche; `3b834000` mit 1126 Byte Nullen; `33538511` defekt (0800–0FFF = FF).
  Nie Vorgabe: diese vier und `5c53a001`.  **Die Kennung `F ZS x nn PS` bei 27F8H/3FF8H prüft der Katalog-Wächter automatisch gegen den Dateinamen.**
* **Reihenfolge der Abzüge:** `33538013` (Vorgabe K6313, kurz), dann `33538513`, `33538028`, `33830803`, `53538500`; lang: `37239503` (nur Centronics, kein SIO), dann `30835007`; K6311 zuletzt.
* **Zeichengenerator K6313:** 9-Byte-Zellen (9×9-Raster) bei ≈2380H–26BFH (kurz), Ländertabellen 16 B bei 2130H (K6311: 2220H); lang nicht ermittelt → eigenes Orakel (WP8d), kein K6316-Modell mit 8 Byte.
* **Offen [?]:** Pläne der V.24/IFSS/IFSP-Kassette und des 10-KiB-Speichermoduls fehlen (SIO-Adressen nur aus der Firmware; Kassettenkennung `PE1` = D104 B4: Centronics 1, V.24 0); Tastenmatrix-Kreuzungspunkte; Centronics-Tor B;
  Haubenkontakt; Interruptketten-Reihenfolge (Drahtlesung).  K6311: alles aus dem ROM erschlossen (`firmware_diff.md` §6), `BC–BF` dort Baudwahl-Eingang [?].
* Der Forum-Hinweis „Pin B8/A15" ist im Plan des 16-KiB-Moduls nicht belegbar; die Umschaltung ROM/RAM ist die Viertel-Dekodierung über A13/A14.

**WP8c-Zuschnitt:** Typ `k6313` (Schlüssel `k6313`, ROM `33538013`, Schnittstelle V.24 über SIO 7CH; IFSS/Centronics-Kassette erst nach WP8d), Speicherplan *kurz*, `K6313Ccb`-Karte, Druckwerk/Papierweg wiederverwendet.

### 11.6  Festlegung des Anwenders 2026-11-15: Umfang und Ziel

* **Centronics wird nicht umgesetzt** (der Anwender hat die Schnittstelle nicht, nichts zum Prüfen) — in §10 aufgenommen: Abzüge, die nur Centronics bedienen (`37239503`, `12228062`), bleiben außerhalb des Katalogs.
  Maßgeblich sind **V.24 und IFSS**.
* **Ziel: alle gelisteten Drucker drucken tatsächlich voll funktionsfähig** (Selbsttest, Text, Schriftarten/ESC-Folgen, Papierweg, Bedienfeld) — „läuft an" genügt nicht; was ohne Beleg nicht erreichbar ist, wird als [?] ausgewiesen statt geraten.
* **Neue Arbeitspakete:**

| AP | Inhalt | Agent |
|---|---|---|
| **WP8f** | K6311 druckt: Wagen-Zwei-Bit-Code (D104 A4/A5) auf `SchrittWagen`, Formularmuster (F9H), Nadelports (DDH/DEH/DFH), Rückmeldungen, Tasten/Lampen aus den ROM-Auswertungen; Orakel gegen den ROM-Zeichengenerator; `K6311Modul.GrenzeDie…` ersetzen | `cpp-coder` (Sonnet) |
| **WP8g** | IFSS am K6313/K6311 (SIO-Kanal/Kassettenkennung laut hardware.md/ROM), Wächter über den Wandler wie K6316 | `cpp-coder` |
| **WP8h** | K6313-Vollständigkeit: ESC-Folgen (Elite, Breit, Fett, Unterstrich…) mit Orakeln, Nadelpolarität, Summer; 16-KiB-Fassung (Speicherplan *lang*) für Abzüge mit V.24/IFSS (`30835007` zuerst, Prüfung der Kassettenkennung) | `cpp-coder` |

Reihenfolge WP8f → WP8g → WP8h (gemeinsames `build/`), je AP Doku + Test + Commit; `test-merge` am Ende.

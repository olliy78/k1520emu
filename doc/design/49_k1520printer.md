# Entwurf 49 — Druckeremulator `k1520printer` (Ticket #28)

Stand 2026-11-05, **WP0a/WP0b, WP1, WP3 und WP2b erledigt (siehe §5a, §5b)**, Rest Planung.  Zweig `feature/issue-28-k1520printer`.
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
mit 6°-Schrittmotor und Taktscheibe, Interfacemodul-Kassetten).  **Was am K6316 entsteht, trägt den K6313 fast vollständig** —
Druckwerk, Taktierung, Bedienmatrix-Prinzip, Interfacemodul.

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

### 2.4  K6313 — kein Abzug

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
  Blatt bleibt (Rückfrage).
* **Fenster**: eigenständiges `app/ui/printer_window.py`, importiert nur Bausteine (Aktionen, Statuszeile, Schnittstellen-Reiter,
  Handbuch, Mehrinstanz) — **`main_window.py` wird nicht zerlegt** (berührt sonst alle sechs Programme).
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
| **WP6b** | SD 1157 druckt: Selbsttest (TEST-Taste), Text über SIO (Testfall ohne Netz), Escape-Folgen, 2 Bahnen (267) | WP2b, WP4 | Selbsttestbild + Textprobe als Wächter (Punktmuster-Prüfsumme) |
| **WP2a** | `K6316Steuerung` nach WP0a, Vorsteckmotor/Schneiden/Leitbleche, Betriebsartschalter, Haube/Kopfhebel | WP1, WP3 | Synchronisation läuft, H-LEDs wie Manual |
| **WP6a** | K6316 druckt: Selbsttest (Foto als Abnahme), Lichtschrankentest, Text über IFSS und V.24, Schnitt | WP2a, WP4 | Wächter wie WP6b |
| **WP4** | SIO beider Geräte an `SerialHub` (IFSS/V.24), Loopback-Test (Port 0) | WP2a oder b | Telnet → Firmware → Punkte (Integrationstest) |
| **WP5** | `k1520printer`: Profil, `PrinterWindow`, `BlattWidget`, Bedienfelder, Typauswahl, PDF, System-Druck, Handbuch, Bindung | WP3 (Rest parallel) | `py_k1520printer_gui` offscreen; PDF enthält die Punkte |
| **WP7** | Paket (Starter, `MASCHINEN`, Inno), Vorgabekonfiguration, `CLAUDE.md`-Abschnitt, Merkposten, `test-merge --alles` | alles | `py_packaging`, `py_programme`, Windows-Gegenprobe |
| WP8 | (später) K6313 auf K6316-Basis, sobald der Abzug da ist; SD 1152/257 Typenrad als zweite `Druckwerk`-Implementierung | Abzug | — |

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

**Offen.**  Zweite Bahn im Druck (Punkte gehen immer auf Bahn 0, `Nadeldruckwerk::feuere`), `SchrittWagen`, Schnitt/Blattgrenzen, Textprobe über die SIO (WP4/WP6b), die offenen `[?]` aus
hardware.md §14 (Takte je Zoll, Nadelteilung, Punktmaß, Flugzeit absolut).  Die Online-Lampe und das Flag `off` der Firmware folgen der Taste ONLINE nur beim Anlauf (Merkposten).

### 5c  Stand WP4 / WP6b Text (2026-11-05)

Gebaut: `Sd1157Steuerung::Anschluss` je SIO-Kanal (IFSS = B, V.24 = A), `PrinterMachine` mit `SerialHub` (Index 0 IFSS, 1 V.24), alter Unterbau, Baudfilter für CTC 0 (Druck-Zeitbasis TC = 23 zählt nicht als Baud),
gemeinsames Druckbild-Orakel `tests/support/sd1157_orakel.h`.  **ROM:** der Abzug leert den Eingabepuffer nie (Byte 0D34H, Hypothese Bitkipp 0BH → 2BH); `doc/EPROMS/SD1157/1157_repariert.rom` ist Vorgabe
(`Config::rom_repariert`, `rom=original|repariert`), der Abzug bleibt unverändert (Merkposten 17).  **Wächter:** `Sd1157Text.*` (Orakel: kurzer Text, mehrere Zeilen in beiden Richtungen, TAB, FF, > 132 Spalten,
Breit/Schräg/Normal, 8 lpi, ungültige ESC, DA/DSR, Papierende, offline im Auftrag), `Sd1157Fluss.*` (DC1/DC3 am IFSS und V.24), `Sd1157Telnet.*` (Loopback, Port 0), `Sd1157Seriell.*`, `Sd1157Online.*`,
`Sd1157Selbsttest.FgdiFolgt…` (Flugzeit: Emulator richtungssymmetrisch, Merkposten 4).  **Offen / [?]:** Doppelbahn im Druck (WP6b, `Nadeldruckwerk::feuere` nur Bahn 0), HPA/VPA-Spaltenzählung, Zeichen 80H–FFH, ROM-Bestätigung 0D34H am Original-EPROM.

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
**SD 1157/264 zuerst**, 267 mit zwei Bahnen in WP6b.

* **F4**  Summer/Geräusche: nur Anzeige oder Ton?  Empfehlung: abschaltbar, Standard aus.
* **F6**  SD 1157: Kennung 4/5/6 aus EBD/BDA; 4 = 264 und 6 = 267 plausibel, 5 ungeklärt.  Offene Punkte mit Prüfkriterium: `sd1157/hardware.md` §14.
* **F5**  K6313-Kassette: wird ausgelesen oder angefragt?  (Bestimmt, wann WP8 beginnt.)

## 10  Nicht im Umfang

Centronics/IFSP-Module, Farbdruck, SD 1157/269 (Grafik), Netzwerkdruckprotokolle (LPD/IPP), virtueller CUPS-Drucker des Wirtssystems.

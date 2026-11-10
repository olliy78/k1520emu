# 50 — GIDE / GUN: IDE-Festplatte, RTC, USB und Netzwerk (A5120, K8915, PRG 710/-1, PC 1715)

Stand: 2026-11-10, **GIDE + RTC umgesetzt** (Zweig `gide`): G0–G5c und G7 erledigt (Karte, ATA-Platte, RTC-Hook, `installGide` in allen
K1520-Maschinen, C-ABI/Python, `--gide`, Systemtests mit echten Fremdtreibern).  **Offen/zurückgestellt:** G6 (Oberfläche, wartet auf die
Basisentscheidung), USB (U0–U3), Netz (N0–N3).  Merkposten: `doc/merkposten/gun.md`.

## 1. Ziel

Vier steckbare **Zusatzfunktionen** nach Vorbild der Gemeinschaftsprojekte GIDE und GUN
(beide **nie von Robotron gefertigt**) als Option in allen vier K1520-nahen Programmen
(`a5120emu`, `k8915emu`, `prg710emu`, `pc1715emu`), gesteckt wie RAF/K6022/K5020
(`K1520Machine::installXxx` nach dem Anlegen, vor dem ersten Lauf), Vorgabe überall **aus**:

| Funktion | Bausteine der Karte | Ports | Quellenlage |
|----------|--------------------|-------|-------------|
| **1. GIDE** (IDE-Platte/DOM/CF) | 2 GAL (16V8, 20V8), 2×74LS646, Jumper A4–A7 | Basis+4…+F (Basis = Jumper) | **vollständig** (§3) |
| **2. RTC** | Epson **RTC 72421** + Batterie | GIDE-Basis+5 | **gut** (§4), Datenblatt noch zu beschaffen |
| **3. USB** | FTDI **VDIP1** (Vinculum) an Z80-PIO, Mario Leubner | 84H–87H | Hardware bekannt, **Gastprotokoll nur teilweise** (§5) |
| **4. Netzwerk** | ATmega162 + WIZnet **WIZ812MJ** (W5100) an Z80-PIO, „KC-Net" von Ralf Kästner | 88H–8BH | Hardware bekannt, **Protokoll unbekannt** (§6) |

Die Funktionen sind **einzeln wählbar** (Heiko Poppe: „lassen sich nach Bedarf einzeln bestücken").
Reihenfolge der Umsetzung = Reihenfolge der Tabelle; 1+2 sind ein Paket (G1–G6), 3 und 4 folgen.

**Annahme zur Anfrage:** „K915" = K8915, „PRG701(-1)" = PRG 710/710-1.

**Abgrenzung / Formfaktor:** Die **GUN** (`robotrontechnik.de/html/eigenbau/gun.htm`, Heiko Poppe,
31.05.2014, EAGLE `PC1715-GIDE-USB-Netz_V4`) ist eine Karte für den **PC 1715** (58-poliger EFS-Stecker).
Für die K1520-Rechner gibt es davon getrennte Nachbauten (Forum: „2x GIDE für K1520", 2020,
Heiko_P; „GIDE-USB-Modul **GUK1520**", Nov. 2025, Kingstener/dragonfly45 — nur Platinenangebot,
ohne Unterlagen).  Der Emulator bildet deshalb **die Funktionsblöcke** ab, nicht eine bestimmte
Platine: gleiches Registermodell, in jeder Maschine steckbar.

## 2. Quellen und Lücken (Recherche 2026-11-10)

| Quelle | Ertrag |
|--------|--------|
| `robotrontechnik.de/html/eigenbau/gun.htm` + `GUN.pdf` | Beschreibung der GUN, Adressen USB/Netz, Bausteintypen |
| `…/downloads/GUN/GUN_Sch.pdf` (EAGLE-Schaltplan, A2, Vektor), `GUN_Stueckliste.pdf` | **Schaltplan gelesen**: Adressdekoder, PIO-Verdrahtung, Glue-Logik, Bausteinliste |
| `autometer.de/gide/` (Tilmann Reh, Spiegel von gaby.de/gide; gaby.de selbst: abgelaufenes Zertifikat) | `GIDEgals.zip` (**GAL-Gleichungen im Klartext**, `gide1.pds`/`gide2.pds`), `GIDEprog.zip` (Z80-Beispiele `gideide.mac`, `gidertc.mac`, `gidebios.mac`, `geom.mac`, `idegen.mac`, `ident.mac`), `GIDEtest09.zip`, `IDE-TCJ.txt` (ATA-Einführung) |
| `retrotechnology.com/herbs_stuff/gide.html` | Geschichte, Verweise (KC-Club, TCJ-Artikel 56–78, P112, ardiehl.de-CPLD-Variante) |
| Forum Threads 17804 / 23589 | nur Flohmarkt/Bestellung; **Dokumentation `GIDE_K1520_Doku.pdf` wurde wegen Urheberrecht entfernt — nicht verwenden**; DOM-Versorgung über IDE-Pin 20 (Jumper HDD PWR) |
| `ac1-info.de/ac1_2010/pio_3…` (AC1-USB mit VDIP1/2 an PIO3, `diskveru.pdf`) | Verwandte USB-Schaltung, Firmware **3.68** im VDIP, Software-Verhalten |

**Nicht beschaffbar / offen:**
- `GIDE.txt` (Rehs Schaltungsbeschreibung) und `GIDEplan.pdf`: Server brach ab (zweimal) — **entbehrlich**,
  da die GAL-Gleichungen das Verhalten vollständig festlegen (§3).
- `RTC7242X-AM.pdf` (Epson-Handbuch): 404 → Registerbelegung aus `gidertc.mac` + Gedächtnis,
  **gegen ein Datenblatt prüfen** (AP G3).
- **susowa.homeftp.net** (KC85-Labor; dort stehen USB- und KCNET-Projekt samt Software): DNS-Fehler,
  Web-Archiv vom Werkzeug gesperrt.  **Blockiert §6 und Teile von §5.**
- FTDI *Vinculum VDIP1 / VNC1L Firmware User Manual* (öffentlich, hier nicht abgerufen).

**Lizenz:** Rehs Dateien tragen „(C) 1995 Tilmann Reh" ohne Weitergabebedingung.  **Nicht ins Repo
einchecken**; der Emulator wird aus der *Verhaltensbeschreibung* (§3) neu geschrieben, Gasttests mit
eigenem Quelltext.  Gleiches für die Forums-Doku.

## 2a. Vorhandene Gastsoftware (Recherche 2026-11-10, zweite Runde)

**Es gibt reichlich Software; sie läuft auf K1520-Rechnern.**  Quellen: Forum 16206 („GIDE – CP/A für A5120/K8924", Klaus, 2018/19),
11026 („GIDE für 5120/8924", 2014), 21872 (RTC/NTIME), 10769 (GUN am PC 1715), hc-ddr.hucki.net (Wiki *z9001/cpm/gide*).

| Software | Autor | Was | Stand |
|----------|-------|-----|-------|
| `GIDEHPxx.COM` (xx = Basisadresse, 16 Stück, `gide-all_02.zip`), `eradir.com` | Heiko Poppe | **nachladbarer** CP/M-2.2-Treiber V0.2 für CP/A-Systeme (A5120, K8924, PC 1715, KC87); bis **8 Laufwerke à 8 MB** (C:…J:), 1024 Verzeichniseinträge | **lokal vorhanden** |
| `gidn@50h/D0h/F0h.com` V0.4 (`gidedrv_v04.zip`), Quelltext `gidn5004.mac` | Kingstener (DL) | nachladbarer Treiber, Laufwerk N: (früher O:), Schnellformat `gidn /F`; läuft laut Forum auch am **K8915 (SCPX)** | **lokal vorhanden** |
| `guk1520.pdf` (Treiber 0.6 mit Quelltext) | Kingstener | für GU-K1520 | lokal vorhanden |
| in **CP/A-BIOS eingebaut** (`ANZGIDE`; `@OS.COM`, Urlader `K2526UL4.BIN`, bootet vom ersten GIDE-Laufwerk) | Klaus (A5120/K8924), Heiko Poppe (PC 1715) | **nur per E-Mail**, nicht abrufbar | — |
| `HDFORMAT.COM`, `SYSWRCPA.COM` | Heiko Poppe | Formatieren / Systemspuren schreiben | nicht abrufbar |
| `TIME.COM` (RTC stellen), `SNTIME11.COM`/`NTIME11.COM` (NTP → RTC), `CLK.COM`; CP/A zeigt die RTC-Zeit in der Statuszeile | susowa, Klaus, Heiko | **RTC-Software**; SNTIME schreibt über Rehs Routinen (Basis+5) | nur per E-Mail / Quelle unerreichbar |
| `GIDEIDE.MAC`/`GIDERTC.MAC`/`GIDE09` | Tilmann Reh | Beispielcode | lokal vorhanden (Quelle `autometer.de`) |

**Folgerungen für den Entwurf:**
1. **Übliche GIDE-Basis ist D0H** (A5120/K8924, PC 1715 mit GUN, K8915-Gerippe „GIDE 0D0h, RTC 0D5h", Netz-PIO 088H);
   MPC4 B0H, BIC E0H, KC87 50H/F0H.  → Vorgabe der **Maschinen-Verdrahtung** D0H (nicht 90H); der Kern lässt die Basis frei
   (Config).  Folge: **D0H kollidiert mit der Kassette K5020** (K8915, PC 1715; am A5120 liegt sie bei 30H) und am PRG 710
   mit dem EPROMmer D0–D4 — dort bleibt eine abweichende Basis nötig.  (§7.1 entsprechend gelesen.)
2. Der Poppe-Treiber benutzt genau das Registermodell dieses Entwurfs (Basis+6…+F, 91h Init, ECh Identify, 20h/30h, `inir`/`otir`
   über +8, Status-Polling) — **die echten Treiber sind der beste Systemtest** (AP G5c): CP/A-Diskette booten, `gidehpd0`,
   `eradir c:`, Dateien kopieren, `dir`, zurücklesen.
3. **Plattenformate sind untereinander unverträglich** (Poppe ≠ DL ≠ im CP/A-BIOS eingebaut): falscher Treiber = Datenverlust.
   Der Emulator ist davon nicht betroffen (Abbild ist nur ein Plattenabbild); ein späteres DiskTool-Dateisystem muss die
   Variante kennen.  Das Layout des Poppe-Treibers (8 MB je Laufwerk, 500 Spuren × 128 Rec/Spur, 2 Systemspuren, Verzeichnis 1024
   Einträge) ist aus der Treiberausgabe belegt; DPB/Blockgröße ist aus dem Treiberquelltext zu entnehmen (G5c).
4. **SCPX/SCP:** der nachladbare DL-Treiber läuft am K8915; SCP 3.0 (CP/M 3, PC 1715W) wird von den bekannten GIDE-Tools **nicht**
   unterstützt; SCP verträgt laut Forum nur 128-Byte-Sektoren (Blocken im Treiber).
5. **RTC-Software** ist für den Emulator nur in Rehs Routinen (lokal) greifbar; `SNTIME`/`TIME.COM` sind nicht beschaffbar.
   Hinweis aus dem Forum: stellte man „GUN-RTC Stundenzehner falsch" fest (Hardwarefehler, nicht Chipmodell).
6. Fremdsoftware liegt **nur lokal** in `~/Documents/K1520emu/GIDE_Software/` (Lizenz ungeklärt, nie ins Repo); Tests suchen dort
   (`K1520_GIDE_SW` überschreibt) und werden **übersprungen**, wenn sie fehlt.

**Ergebnis G5c (2026-11-10, Systemtest mit den echten Treibern, `GideSystem.*`, ohne Modelländerung grün):**
- **A5120/CP/A + Poppe `GIDEHPD0.COM`:** Treiber meldet `GIDE-Modul auf Adresse D0H`, erkennt die Platte (Identify), `ERADIR D:`,
  `PIP D:=A:...`, `DIR D:`, Rückkopie **bitgleich**.  Am CP/A-Fixture belegt das BIOS A: und C:, der Treiber vergibt **D:…K:**.
- **K8915/SCPX + Kingstener `gidn /f`** (Laufwerk **N:**): Formatieren, Kopieren, Rückkopie bitgleich.
- **Anforderungen an das Plattenmodell (aus den Treiberquellen):** Poppe sendet **nie 91h**, adressiert **CHS ohne LBA-Bit** mit
  Kopf = Laufwerk (SDH = A0H+n, 8 Laufwerke), 500 Zylinder, 16 Köpfe, 63 Sektoren → die Platte braucht mindestens
  **504 000 Sektoren (≈ 258 MB, als Rohabbild sparse anlegen)**; DPB SPT 128, BSH 6, BLM 63, EXM 3, DSM 3E7H, DRM 3FFH.
  Kingstener sendet 91h (SC=40h, SDH=A0H|3 → 4 Köpfe × 64 Sektoren) und braucht ≥ 256 Zylinder; DPB SPT 64, BSH 5, DSM 7FDH.
  Folge für G6: Vorgabegröße „Neue Platte" **256 MB (sparse)**; kleinere Abbilder gehen nur mit dem Kingstener-Treiber.
- Treiberbefund: nach `ERADIR D:` findet der erste Befehl auf A: nichts, bis `DIR D:` und `DIR A:` aufgerufen wurden (Treiber-/BDOS-
  Verhalten: `hdisk` bleibt gesetzt; Diskette unversehrt).  Nicht abgedeckt: PRG 710, PC 1715 (kein passendes Treiber-/Boot-Paar geprüft).

## 3. GIDE — Befund (aus `gide1.pds`/`gide2.pds`, Stand 26.09.1995)

### 3.1 Adressdekodierung (GAL2, 20V8)
Jumper `ja4..ja7` vergleichen A4–A7 mit der **Basis** (Hochnibble); die Karte belegt die Ports
**Basis+4 … Basis+F** (`selq = sel·iorq·(A3+A2)`).  Alles nur mit /IORQ, A8–A15 unbeachtet
(außer RTC, §4).

| Port | Signal | Bedeutung |
|------|--------|-----------|
| +4 | `exp` | nur im 22V10-Aufbau („EXP", an IDE-Pin 20 herausgeführt) — im GAL-Aufbau ohne Wirkung |
| +5 | `rtc` | Chip-Select der RTC 72421 (§4) |
| +6, +7 | `cs1` | IDE /CS1: **+6 = Alternate Status (lesen) / Device Control = „DOR" (schreiben)**, +7 Drive Address |
| +8 | `cs0` + Daten | IDE-**Datenregister** (16 Bit, siehe 3.2) |
| +9…+F | `cs0` + A0–A2 | Task-File: +9 Error/Features, +A Sector Count, +B Sector Number, +C Cyl Low, +D Cyl High, +E Drive/Head (SDH), +F Status/Command |

IDE-Adressleitungen HA0–HA2 = A0–A2 gepuffert.  Schreiben auf +6: Bit 2 = SRST (`out (IdeDOR),6` setzt,
`2` löscht den Software-Reset; Bit 1 = nIEN).

### 3.2 Das 16-Bit-Datenregister auf dem 8-Bit-Bus (GAL1, 16V8 + 2×74LS646)
Ein **Wechsel-Flipflop `LH`** (Low/High-Byte-Folge) macht aus *einem* Port 8 einen Bytestrom:
- `LH` kippt bei **jedem Zugriff** auf Port +8 oder +9…+F (`/TOG`), wird aber bei einem **Task-File-Zugriff
  (+9…+F) auf 0 gesetzt** (`LH := /LH · /TFRADR`); Zugriffe auf +6/+7 (`cs1`) ändern ihn nicht.
  Der Takt wirkt am Zugriffsende (wegen `LH` als Registerausgang erst für den **nächsten** Zugriff gültig).
- **Lesen +8:** 1. Zugriff (`LH=0`): `IORD` zur Platte, das ganze 16-Bit-Wort wird in das 646-Register
  getaktet (`RCLK`), das **Low-Byte** geht auf den Bus.  2. Zugriff (`LH=1`): **kein** `IORD`,
  das gemerkte **High-Byte** geht auf den Bus.
- **Schreiben +8:** 1. Zugriff (`LH=0`): Low-Byte wird nur im Register gemerkt (kein `IOWR`).
  2. Zugriff (`LH=1`): `IOWR` mit Low=Register, High=Busbyte.
- Folge: `inir` ×2 (2×256 Byte) liest einen 512-B-Sektor, `otir` ×2 schreibt ihn
  (`gideide.mac`).  Der Gast muss **gerade** Byteanzahl je Sektor in einem Zug holen; ein Task-File-Zugriff
  dazwischen setzt die Folge zurück (Wächter dafür, §G2).
- Reihenfolge: **Low-Byte zuerst** (Wort = Low + 256·High, wie ATA).
- Kein Interrupt, kein /WAIT, kein IORDY (der Gast fragt Status BSY/DRQ ab).  IDE-IRQ ist nicht
  am Bus angeschlossen (`gideide.mac`/GUN-Schaltplan).

### 3.3 Gastzugriffsmuster (Rehs Beispiel `gideide.mac`)
`HDInit`: DOR←06h, kurz warten, DOR←02h, Status.BSY(Bit 7) abwarten; Sector Count/Cyl/Head setzen;
**Command 91h** (Initialize Drive Parameters).  Sektor lesen: Task-File setzen (SDH = A0h|Kopf),
Command **20h**, auf **DRQ (Status Bit 3)** warten, 2× `inir`, auf BSY=0 warten, Status maskiert mit 89h
prüfen.  Schreiben: Command **30h**, `otir` 2×.
In Rehs Quellen belegt sind nur **20h, 30h, 91h** und **ECh Identify** (`ident.mac`).  Die übrigen
ATA-Befehle in G1 (90h, 10h, 70h, 40h/41h, C4h–C6h, EFh, Power) stammen aus der allgemeinen
ATA-Befehlsliste (`IDE-TCJ.txt`) und werden **tolerant** behandelt (Erfolg ohne Wirkung bzw. ABRT).

### 3.4 Geräteseite: was ein DOM/CF-Modul können muss
LBA **und** CHS (SDH-Bit 6), 512-B-Sektoren, Identify-Daten mit Geometrie/Kapazität; das Gastsystem
liest daraus (`geom.mac`).  Modell: ein einzelnes Master-Gerät (Slave nicht belegt: Bit 4 von SDH = 1 →
„kein Gerät", Status 0).  Zeitverhalten: Befehlsende nach wenigen µs–ms Maschinenzeit; BSY-Fenster
**nicht** wirklichkeitsnah nötig, aber **sichtbar** (≥ 1 Statusabfrage), damit Gastschleifen laufen.

## 4. RTC 72421 (am GIDE-Port +5)

- 16 Register, adressiert über **A8–A11 des Busses** (`ld bc,0F00h+RTC / out (c),a`: B = Registernummer!).
  → Die Karte braucht `K1520Bus::ioAddress()` (wie RAF/K6022), in jeder Maschine zu prüfen (G0).
- Belegung nach `gidertc.mac`: 0 S1, 1 S10, 2 MI1, 3 MI10, 4 H1, 5 H10, 6 D1, 7 D10, 8 MO1, 9 MO10, 0Ah Y1,
  0Bh Y10, 0Ch Wochentag, **0Dh CD** (Bit 0 HOLD, Bit 1 BUSY, Bit 2 IRQ-FLAG, Bit 3 30-s-Adjust),
  **0Eh CE** (Takt/Intervall/Maske), **0Fh CF** (Bit 0 RESET, Bit 1 STOP, Bit 2 **24h**, Bit 3 TEST).
  Inbetriebnahme des Gastes: CF←5, CF←4, CD←0.  Lesen: HOLD setzen, BUSY=0 abwarten, Ziffern lesen,
  HOLD löschen.  [? bitweise Einzelheiten gegen das Epson-Datenblatt prüfen — AP G3]
- Batteriegepuffert (CR2032): Zeit läuft weiter, auch wenn der Emulator aus ist → **gleiche Zeitpolitik wie die
  P8000-Uhr** (`doc/design/38_p8000_rtc.md`, Merkposten p8000 Nr. 59): zählt in **Maschinenzeit**,
  springt beim Start nie zurück (`max(Stand+Abwesenheit, Wirtszeit)`), Oberfläche speist **UTC**.
  Der P8000-Baustein `core/cards/p8000/rtc.*` enthält bereits ein **RTC-72421-Modell** (Epson, Nachbau pofo.de):
  **Chipmodell herauslösen und wiederverwenden** statt neu schreiben (AP G3).
- Kein Interrupt angeschlossen (kein INT im Schaltplan); Gastsoftware fragt ab.

## 5. USB (VDIP1, Ports 84H–87H)

Aus dem GUN-Schaltplan (gelesen):
- **Z80-PIO** („USB", IC3), Chip-Enable `IO84H`; **A0 = Port-Auswahl (A/B), A1 = Daten/Steuer** →
  **84H Daten A · 85H Daten B · 86H Steuer A · 87H Steuer B**.  Dekoder 74LS138: IC4 (A5–A7 = 100 → 80H-Block)
  gibt IC1 frei, IC1 (A2–A4) liefert `IO84H` (Y1) und `IO88H` (Y2), zusätzlich /M1 und /IORQ.
- **Port A = AD0…AD7** des VDIP1 (Betriebsart **2, bidirektional** mit `ARDY/ASTB`).
- **Port B**: B0…B5 = AC0…AC5 (FIFO-Steuerleitungen), **B6 = RES**, **B7 = PRG**
  (Vinculum-Reset/Programmiermodus).
- Glue (74LS00 IC5): `ASTB = NAND(B2,B3)` (Strobe bei RD# **oder** WR# aktiv), `BSTB = B2 · /B0`
  (Lese-Strobe nur wenn Daten bereit; B0 = /RXF).  Die genaue Zuordnung der AC-Leitungen
  (RXF#, TXE#, RD#, WR#, …) **gegen das Handbuch prüfen** [?].
- **Gastprotokoll:** VDIP1 mit Firmware **3.68 (VDAP, Monitor-Befehlssatz)**, Textbefehle mit CR
  (`E`/`e` Sync-Echo, `DIR`, `CD`, `OPR/OPW`, `RDF/WRF`, `CLF`, `SEK`, `DLF`, `MKD`, `FS`, `IDD` …,
  Kurz- oder erweiterter Befehlssatz `SCS`/`ECS`, Prompt `D:\>`).  **Alles [?] aus dem Gedächtnis —
  gegen *Vinculum Firmware User Manual* prüfen (AP U0).**
- Vorbild-Software: `DiskVerU` (AC1, FAT16/32-Sticks, Firmware 3.68) zeigt das Anwenderverhalten;
  Quelltext/Binärdatei nicht ausgewertet.  Die **KC85/GUN-Treiber** der Gemeinschaft (susowa) wären die
  ideale Gegenprobe (Adressen 84H–87H werden dort „unterstützt") — **nicht erreichbar** (§2).

**Modellidee:** `VdipGeraet` = Vinculum-Firmware-Zustandsmaschine (Befehlszeilen-Parser, Dateihandle,
Verzeichniszustand) über einem **Wirtsverzeichnis als „USB-Stick"** (Datei-API statt FAT-Image;
Option „FAT-Abbild" später über den vorhandenen `FatFileSystem` aus dem DiskTool).  An die PIO
über Rückruf-Schnittstelle (Port A Mode 2 + externe Strobes), **die Glue-Gatter werden nicht Gatter für
Gatter nachgebaut**, sondern als Verhalten (RXF/TXE-Pegel → BRDY/ARDY).  Zu klären in U1: bietet
`Z80PIO` Mode 2 samt externem `/ASTB`/`/BSTB` über seine Schnittstelle an (Kommentar in `z80_pio.cpp`
sagt: Mode 2 ist implementiert; ob die Strobes von außen gesetzt werden können, ist zu prüfen).

## 5a. Protokoll nach UTools-Quellen (AP U1, 2026-11-10)

Quelle: UTools V1.4 (Leubner/Haensel; lokal `~/Documents/K1520emu/GIDE_Software/utools14/`, nicht im Repo), Zeilen
beziehen sich auf `USBINC.Z80` (I), `UDIR.Z80` (D), `UGET.Z80` (G), `UPUT.Z80` (P), `USB.Z80` (U).  Das Handbuch war
nicht beschaffbar: **Was nur aus dem Gedaechtnis stammt, ist `[?]`.**  Zwei Befunde gegen die Vermutung aus §5:
(1) **SCS = Kurzbefehlssatz (Einzelbyte-Opcodes, Prompt `>` CR, Fehler als 2 Buchstaben), ECS = erweiterter Satz
(Textbefehle, Prompt `D:\>` CR, Fehler im Klartext)**; die Opcodes werden auch im ECS-Modus erkannt (UDIR schickt `10h`
im Vorgabemodus, I:279 ff.).  (2) **Der Prompt IST der Status**: Erfolg = Prompt + CR, Fehler = Fehlertext + CR
*ohne* folgenden Prompt (G:487-490, P:305-340: nach `CF`/`FI` laeuft das naechste EXEC sofort weiter und erwartet `>`).
Ohne Stick lautet der Status `No Disk` (ECS) bzw. `ND` [?] (SCS) (D:256).  `E`/`e` antworten ohne Prompt (I:279-303).

| Befehl | Syntax (Bytes) | erwartete Antwort | Quelle |
|--------|----------------|-------------------|--------|
| Sync | `CR` · `E CR` · `e CR` | beliebige Zeile bis CR · `E CR` · `e CR`; vorher alles Anstehende wird abgeholt | I:272-303 |
| Start | (Reset) | `Ver03.68VDAPF On-Line:` CR `Device Detected P2` CR `No Upgrade` CR `D:\>` CR (wortwoertlich, Wiki hc-ddr); ohne Stick `No Disk` statt Detected/Prompt [?] | Wiki |
| leere Zeile | `CR` | Status (`>` CR / `D:\>` CR / Fehler) — dient als „Stick vorhanden?“ | D:120, G:130 |
| SCS | `10h CR` | `>` CR, ab dann Kurzsatz | D:110-113 |
| ECS | `11h CR` | `D:\>` CR (UTools ueberlesen alles bis `>`) | U:94-105 |
| IPH / IPA | `91h CR` / `90h`[?] | Status; IPH = Zahlen binaer (MSB zuerst als Parameter), IPA = ASCII-dezimal [?] | D:115 |
| DIR | `01h CR` bzw. `DIR CR` | CR, je Eintrag `NAME.EXT` CR / `NAME DIR` CR (Datei ohne Typ ohne Punkt; in Unterverzeichnissen `.`/`..` als `. DIR`), dann Status | D:127-160, G:738-762 |
| DIR name | `01h SP NAME.EXT CR` | CR, `NAME.EXT SP` + 4 Byte Groesse **LSB zuerst** + CR, Status; fehlt: CR, `CF` CR | G:212-245, D:529-562 |
| DIRT name | `2Fh SP NAME.EXT CR` | CR, `NAME.EXT SP` + 10 Byte + CR, Status; 10 Byte = erstellt (Zeit-Wort, Datum-Wort, LE) · Zugriffsdatum (Wort) · geaendert (Zeit, Datum, LE); je 4 Byte = MS-DOS-Dword LE (aus `m2utim`-Aufrufen bei +0/+2/+6 abgeleitet) | G:269-302, D:596-629 |
| OPR | `0Eh SP name CR` | Status; fehlt: `CF` (3.66+) bzw. `FI` (3.64) CR; schon offen: `FO` | G:487, P:305-340 |
| OPW | `09h SP name [SP d3 d2 d1 d0] CR` | Status; Datei wird angelegt, sonst angehaengt [?]; Stempel = DOS-Dword **MSB zuerst** (Datum vor Zeit), rohe Bytes in jedem Modus | P:517-535 |
| RDF | `0Bh SP 0 0 0 n CR` | genau n Rohbytes (nichts davor), dann Status | G:535-557 |
| WRF | `08h SP 0 0 0 n CR` + n Rohbytes | Status nach den Daten | P:578-600 |
| CLF | `0Ah SP name CR` | Status (Name der offenen Datei) | G:588, P:342, 625 |
| DLF | `07h SP name CR` | Status; schreibgeschuetzt `RO`; fehlt `CF` | P:460-495 |
| CD / DLD / MKD / REN / RD / FS / IDD / FWV | Textform `CD name` usw. (U:53-69, UTools14.TXT); `CD /` Wurzel, `CD ..` hoch; `MKD name [SP d3..d0]` (mit Datum auf DOS/ZSDOS-Systemen) | Status; Daten von RD (Rohbytes), FS/IDD/FWV (Textzeilen, Format [?]) | U:92-141 |
| Namen | CP/M-Name wird zu `NAME.EXT` ohne Leerzeichen; `\ / " | +` werden zu `_`; **leere Endung ergibt `NAME.`** (Punkt wird gesendet) | I:439-461 |
| Fehlertexte (ECS) | `Bad Command` `Command Failed` `Disk Full` `Invalid` `Read Only` `File Open` `Dir Not Empty` `Filename Invalid` `No Disk` | SCS-Kuerzel gesichert nur `CF` `FI` `RO` `FO`; `BC` `DF` `NE` `FN` `ND` [?] | U:245-258 |
| Port-Pegel | PIO-B Bit0 = RXF# (1 = nichts abzuholen), Bit1 = TXE# (1 = nicht schreibbar); Ausgaenge: B2 = RD# (low = Lesen, `C0h`), B3 = WR (high = Schreiben, `CCh`), Ruhe `C4h`; B6 = RES# und B7 = PRG# im Ruhezustand 1; Eingangsmaske `33h` | I:209-225, 369-426 |

`[?]`-Punkte im Modell: Opcodes ausser den in den Quellen benutzten (`CD 02h`, `RD 04h`, `DLD 05h`, `MKD 06h`, `REN 0Ch`,
`IDD 0Fh`, `FS 12h`, `FWV 13h`, `SEK 28h`, `IPA 90h`), IPA-Zahlenform, Textformat von FS/IDD/FWV, OPW „anhaengen“, Kurzcodes
`BC DF NE FN ND`, Verhalten von `RDF` ueber das Dateiende (Modell liefert nur den Rest).

## 6. Netzwerk (88H–8BH, KC-Net-Bauart)

Aus dem Schaltplan: **zweite Z80-PIO** (IC2, `IO88H`: **88H Daten A · 89H Daten B · 8AH Steuer A · 8BH
Steuer B**) ↔ **ATmega162** (8 MHz) als *Brücke*: PIO-Port A = 8-Bit-Daten (Bit-Parallelbus), Port B =
Handshake/Steuerung (`PB0…PB7` ↔ `PD…`), ATmega ↔ **WIZ812MJ (W5100)** über 74HCT573-Adresslatch,
Adressen A0–A14, `/CS /RD /WR /INT /RESET`; zusätzlich LED-Ausgänge.  Der ATmega bringt
**Firmware von Ralf Kästner** („KC-Net"), die das Protokoll Rechner↔W5100 festlegt.
**Das Rechner-seitige Protokoll (Kommandobytes, Register- vs. Blockzugriff) ist nirgends erreichbar**
(susowa-Seite).  Ohne diese Angabe lässt sich keine Gastsoftware betreiben.

Konsequenz für die Planung: **§6 ist in zwei Stufen geschnitten**:
- **N0 (Voraussetzung, vom Anwender):** Protokollbeschreibung/Firmware-Quelltext/Hex der KC-Net-Brücke
  und möglichst ein lauffähiger Gasttreiber/Beispielprogramm beschaffen (Quellen: Anwender, KC85-Foren,
  Mitglieder der Gemeinschaft).  Erst danach die APs N1–N3 freigeben.
- **N1:** Modell des **W5100** (Register `MR/GAR/SUBR/SHAR/SIPR`, Sockets 0–3, TX/RX-Puffer 8+8 KB
  Vorgabe, Sn_MR/CR/SR, **TCP-Client/Server und UDP** über die vorhandene Socket-Schicht
  `core/serial/net/` auf den Wirtsrechner) — protokollunabhängig, testbar ohne Gast.
- **N2:** ATmega-Brücke als **Verhaltensmodell** (nicht den AVR emulieren, solange das Protokoll bekannt ist;
  *Alternative bei fehlender Dokumentation:* die Original-Firmware im AVR-Emulator laufen lassen — großer
  Zusatzaufwand, nur mit Hex-Datei möglich, nicht empfohlen).

Kein Interrupt zum Bus (INT geht nur zur PIO, die PIO-Interruptkette steht in der Karte; ob die GUN die
PIOs in die Bus-IEI/IEO-Kette hängt, ist aus dem Schaltplan abzulesen: IEI/IEO beider PIOs sind
beschaltet — **in G0/N3 klären**, Rangfolge in den vier Maschinen beachten).

## 7. Einbindung in die Maschinen

### 7.1 Portbelegung — **Konflikte**
- GUN-Festlage **84H–8BH**: **kollidiert mit der RAF (88H/89H, fest)** → USB-Optionen und RAF sind je
  Maschine nur teilweise kombinierbar; **Netz und RAF schließen einander aus** (`registerIO` wirft →
  `installXxx` meldet den Grund wie bei der RAF).  Der Oberflächenfilter soll die Kombination vorab sperren.
- GIDE-Basis ist frei einstellbar (4 Jumper, Basis = n·10H).  **Die Basis „80H" der Rehs Beispiele
  verbietet sich auf der GUN** (GIDE belegt +4…+F = 84H–8FH und läge über USB/Netz).  Welche Basis Heiko Poppe
  für die GUN wählt, steht nicht in den Unterlagen [?].  **Vorschlag: Vorgabe 90H** (Reihe 90H–9FH,
  in K1520-Rechnern ungenutzt) — in G0 je Maschine gegen die tatsächliche Portkarte prüfen; über
  *Einstellungen* wählbar (Konfliktprüfung wie bei der RAF).
- Bekannte belegte Bereiche (Auswahl): K5020 30H–37H bzw. D0H–D7H, K6022 E0H–E7H, RAF 88H/89H, K8915
  A8H (Speicherumschaltung), PC 1715: 00H–07H K5122, … — vollständige Karte erstellt G0.

### 7.1a Befund G0 (2026-11-10, Bestandsaufnahme am Code)

- **`ioAddress()` (A8–A15) stimmt in allen vier Maschinen**: Z80-Kern legt bei `IN A,(n)` A auf A8–A15, bei
  `IN/OUT r,(C)`, `INIR`, `OTIR` B (`z80.cpp`); `K1520Bus::ioRead/ioWrite` setzen `io_addr_`.  Kein Umbau nötig.
  Randfall: A5120-ZVE1 mit gesperrtem E/A (`eaUnerlaubt`) ruft den Bus nicht auf (für GIDE/RAF belanglos).
  `ioReadDebug(uint8_t)` setzt `ioAddress()` nicht [?] — für den Debugger-Befehl `gide` beachten.
- **Alle vier Maschinen haben `K1520Bus bus_`**, `systemBus()`, `zusatzkartenSteckbar()` (nur P8000: false).
  Vorbild `installK5020`/`installK6022` (prüfen `bus.ioOwner()` vor `registerIO`) — **dieses Muster nehmen**,
  nicht den Wurf von `registerIO` wie `installRaf`.  Reset-/PowerOn-Verdrahtung je Maschine an den Stellen von
  `rafPowerOn()`; `bestueckungAbschliessen()` (a5120.cpp, k8915.cpp, prg710.cpp, pc1715.cpp).
- **Portkarte (frei = kein Chip der Maschine, mit allen Optionen):**
  90H–9FH, A0H–A7H, F0H–FFH (außer PRG-ATP F4/F5) frei **auf allen**; 8AH–8FH frei auf A5120/PC 1715.
  80H–83H: K8915 V3 (ZRE-CTC), 80H–87H: K8915 Gen2 und PRG 710 (K2521-CTC/PIO).  88H/89H: RAF.
  A8H–AFH: EM (A5120) bzw. ZRE-Register (K8915).  B0H–B7H: EM/AUP (A5120).  C0H–CFH: PRG 710 (ATP/ASS).
  D0H–D7H: K5020 (K8915, PC 1715) bzw. EPROMmer (PRG 710).  E0H–E7H: K6022, E8H–EBH PRG-Seitenregister.
  **Folge:** USB/Netz der GUN (84H–8BH) sind am **K8915 und PRG 710 nicht steckbar** (CTC/PIO der ZRE),
  am A5120 und PC 1715 schon (Netz ohne RAF).  GIDE-Basis **90H passt überall**.  `installGunXxx` meldet
  Portkonflikte wie `installK5020`.
- **Z80PIO Mode 2** ist vorhanden (`setStbA/setStbB`, `setARdyCallback/setBRdyCallback`, `setExternA/B`,
  `pinsA/B`; Test `test_pio_handshake.cpp`), aber noch in **keiner Karte** benutzt; Port B wird im Mode 2
  erzwungen Mode 3 (passt zur GUN-Verdrahtung).
- **Interruptkette:** Zusatzkarten werden hinten angehängt (`appendInterruptChain`); **PRG 710** baut die Kette
  in `interruptKetteSetzen()` neu — dort muss eine PIO eingetragen werden.
- **RTC:** `P8000Rtc72421` (`core/cards/p8000/rtc.h`) hat keine P8000-Abhängigkeit, ist aber über die
  P8000-Portadresse (`FDE1`, Register `(port>>1)&0xF`) angesprochen und liefert `0x30|Nibble`.  Für G3: reine
  Registerschnittstelle `lese(n)`/`schreibe(n,d)` herauslösen (Bestandstests `test_p8000_rtc*.cpp`, 20+ Fälle,
  bleiben Wächter), Takt = Maschinentakt, Batteriedatei `K1520-P8000-RTC 2` mit eigenem Kopf für GIDE.

**Entscheidung des Anwenders (2026-11-10):** Jumperstellungen der echten Karten sind unbekannt.  Im Kern sind
**alle Jumper konfigurierbar** (GIDE-Basis, Funktionsblöcke einzeln), wie an der Hardware; ob die Konfiguration bis in den
Einstellungsdialog durchgereicht wird oder die Maschinen eine feste Verdrahtung vorgeben, wird **später** entschieden
(G6 zurückgestellt).  Umfang jetzt: GIDE + RTC (G1–G5a).

**Berichtigung AP U2 (2026-11-10):** am **K8915 V3** belegt die ZRE nur 80H..83H, 84H..87H sind **frei** — die GUN-USB
(Basis 84H) ist dort steckbar; nicht steckbar bleibt sie am K8915 Gen 2 und am PRG 710/-1 (K2521: 80H..87H).  Die Karte
(`core/cards/gun_usb/`) hat eine einstellbare Basis (4er-Schritte); die UTools-CP/M-Fassung (DCh..DFh) laeuft ohne Patch
gegen `basis = 0xDC` (Systemtest `GunUsbSystem.UToolsA5120`: USB FWV, UDIR, UPUT, UGET bitgleich).

### 7.2 Gemeinsames Muster (wie RAF/K6022/K5020)
`K1520Machine::installGide(Config)` / `installGunUsb()` / `installGunNet()` im Basisklassen-Kopf
(`core/machines/machine.h`), Steckung vor dem ersten `run()`/`reset()`, C-ABI `k1520_gide_*`,
`k1520_usb_*`, `k1520_net_*` (additiv, `test_c_api.py` prüft die Signaturen mechanisch),
Python `K1520Emulator(gide=…, gide_image=…, rtc=True, usb=…, net=…)`, `boot_trace`/`k1520dbg
--gide/--usb/--net` (+ Debugbefehl `gide`: Register, letzter Befehl, Sektorprotokoll),
Systemweiter `/RESET` setzt Karten zurück (Plattenabbild bleibt).
Nicht im Save-State (wie RAF/K6022/K5020), vorerst festgelegt.

### 7.3 Plattenabbild
Rohabbild 512-B-Sektoren (`.img`/`.hdf`; Geometrie aus Identify-Konfiguration oder Dateigröße),
**optional gepackt** wie P8000-Platten (`.img.gz`, `core/util/gzip_datei.h`, Schreibfaden mit Schreibpause —
Muster aus `core/peripherals/winchester/` wiederverwenden).  „Neue Platte…" legt **leeres** Abbild an
(Größenwahl 8/16/32/64/128 MB; Obergrenze für CP/M-Gäste 8 MB je Laufwerk, das Gastsystem
partitioniert).  Schreiben geht **verzögert** in die Datei (0,5 s Schreibpause, wie `DiskImage`);
Sperre `<abbild>.lock` gegen Mehrfachöffnen (Muster P8000).

### 7.4 Wirt-/Gastsoftware
- Für den **Anwender** nützlich wird die Platte erst mit einem **Gasttreiber** (ein CP/A-/SCPX-/SCP-BIOS
  kennt keine IDE).  Vorbild: das **nachladbare** `RAFCPM.COM`.  Plan: `GIDEDRV.COM` (eigenes Z80-Programm,
  Quelle + eingecheckte `.com` unter `tools/gide/`, Bau über `tools/cpm_bau.py`, Beigabe auf den
  Bootdisketten über `tools/disketten_beigaben.py`) — **eigener Entwurf nötig** (DPB/Blockgröße je
  Betriebssystem; `CpaDpbRule`; BIOS-Sprungleiste hängen).  Wird als **G5b zurückgestellt**, bis der Anwender
  entschieden hat (Frage F3).
- Für G1–G4-Tests genügt ein **Prüfprogramm `GIDETST.COM`** (Identify, Sektor schreiben/lesen/vergleichen,
  RTC lesen/setzen).
- DiskTool-Anbindung (CP/M-Partitionen in IDE-Abbildern lesen) ist **Ausblick**, nicht Teil dieses Entwurfs.

## 8. Arbeitspakete (für Sonnet-5.5-Agenten)

Regeln (aus `CLAUDE.md`): Bau/Test **nur über `tools/dev.sh`**; build-berührende Agenten **nacheinander**
oder je Worktree; je AP Doku + Commit in den Feature-Zweig (nicht pushen); **keine Umlaute im Code**;
Kommentare deutsch; neue langsame Tests brauchen ein `fach_*`-Label — **keine langsamen Tests anlegen**.
`cpp-coder` für Kern, `test-runner` für Läufe, `code-explorer` für Suchen.  Jedes AP trägt seine **Abnahme**.

| AP [Status] | Inhalt | hängt an | Ergebnis / Abnahme |
|----|--------|----------|--------------------|
| **G0** [erledigt] | **Bestandsaufnahme.** (a) Je Maschine (A5120 inkl. .16/A5130/K8924, K8915 V3/G2, PRG 710/-1, PC 1715/1715W) prüfen: wird `K1520Bus::ioAddress()` (A8–A15) durchgereicht? Ist `installRaf` dort möglich (Vorbild)? (b) **Portkarte** aller belegten E/A-Ports je Maschine als Tabelle in dieses Dokument (§7.1) einfügen; Vorgabe-Basis 90H bestätigen oder ändern; (c) Kann `Z80PIO` Mode 2 mit externen `/ASTB`/`/BSTB` treiben (Schnittstelle benennen); (d) Interruptketten-Platz von PIO(+CTC) der GUN je Maschine. Nur Lesen. | — | §7.1 mit Tabelle, Befundliste (Kurzbericht), keine Codeänderung |
| **G1** [erledigt] | **ATA-Gerät** `core/peripherals/ata/ata_geraet.{h,cpp}`: Task-File-Register, Status/BSY/DRQ/ERR, Befehle 20h/30h, 21h/31h ohne Retry, 40h/41h Verify, **91h**, 90h, 10h, 70h, **ECh**, C4h–C6h (Multiple), EFh (Features, tolerant), E0–E7h (Power, tolerant), unbekannt → ABRT; CHS+LBA; Sektorpuffer 512 B; Geräte-Reset über Device Control (SRST); Slave nicht vorhanden. Abbildspeicher: Rohabbild + Gzip-Variante + verzögertes Zurückschreiben + Sperrdatei (Muster `winchester`). Reine C++-Klasse ohne Bus. | G0 | GoogleTest `tests/unit/peripherals/test_ata_geraet.cpp`: Identify-Wörter (Geometrie, Kapazität, Modellname), Lesen/Schreiben je CHS und LBA, Grenzfälle (letzter Sektor, jenseits → ERR/IDNF), Reset-Folge, `.gz`-Rundlauf, Sperre; Label via `k1520_add_test()` |
| **G2** [erledigt] | **GIDE-Karte** `core/cards/gide/gide.{h,cpp}` (BusDevice): Port-Dekodierung nach §3.1 (Basis konfigurierbar, nur Hochnibble), `LH`-Flipflop nach §3.2 (inkl. Reset durch Task-File-Zugriff, `cs1` ohne Wirkung), Anbindung an `AtaGeraet`, `attachToBus` wirft bei Portkonflikt, `reset()`, Zustand für Debugger. RTC-Hook als Zeiger (nullptr = keine RTC). | G1 | `tests/unit/cards/test_gide.cpp`: **Rehs Routinen als Z80-Testprogramm** (HDInit, Sektor lesen `inir`×2, schreiben `otir`×2, Rundlauf byteweise gleich), `LH`-Wächter (Zugriff auf +9…+F mitten im Sektor setzt Folge zurück), ungerade Länge, Konfliktfall, Basis 90H/A0H |
| **G3** [erledigt] | **RTC 72421**: Chipmodell aus `core/cards/p8000/rtc.*` herauslösen (gemeinsamer Baustein `core/primitives/rtc72421.{h,cpp}`, P8000-Karte nutzt ihn weiter — **P8000-Wächter `Rtc*` müssen grün bleiben**), Zugriff über A8–A11, Zeitpolitik nach §4 (Maschinenzeit, Batteriedatei `gide_rtc.dat` in der Benutzerkonfiguration, UTC-Eingabe), Einhängen in `Gide` an Basis+5. Datenblatt (Epson RTC-72421 Application Manual) beschaffen und Registerbitbelegung **gegen** §4 prüfen, Abweichungen in diesem Dokument nachtragen. | G2 | Tests: Rehs `gidertc.mac` als Z80-Programm (Init, Hold/Busy, Lesen, Setzen), Zeit läuft in Maschinenzeit, kein Zurückspringen nach Neustart, 24h-/12h-Modus, P8000-Wächter grün |
| **G4** [erledigt] | **Maschinenintegration** (Muster `installRaf`): `installGide(Config{basis, abbild, rtc})` in `machine.h`, Rückgabe + `gideFehler()`, `/RESET`-Verdrahtung, in **allen vier** Maschinen lauffähig (`zusatzkartenSteckbar()`); C-ABI `k1520_gide_*` (anlegen/Abbild einlegen/auswerfen/Status/Zugriffsprotokoll/`k1520_gide_create_image`), Python-Bindung (`app/core_binding/k1520.py`), `test_c_api.py` erweitern, `boot_trace`/`k1520dbg --gide`, Debugbefehl `gide`. | G2, G3 | `tests/integration/test_gide_maschinen.cpp`: je Maschine Karte stecken, vom Gast-Z80-Code Sektor schreiben/lesen; Doppelstecken/Portkonflikt/zu spät gesteckt → Fehlertext; `py_c_api`, `py_binding` grün |
| **G5a** [entfaellt (durch G5c ueberholt)] | **Prüfprogramm `GIDETST.COM`** (`tools/gide/`, Z80-Quelle + eingechecktes `.com`, Bau `tools/gide/build.py` über `tools/cpm_bau.py`): Portbasis als Parameter, Identify anzeigen, Schreib-/Lese-/Vergleichslauf, RTC-Anzeige/Stellen; auf die Bootdisketten (`tools/disketten_beigaben.py`, Wächter `cli_beigaben_auf_den_disketten` nachziehen). | G4 | Integrationstests je Maschine: CP/A (A5120), SCPX (K8915, PRG 710-1), SCP/CP/A 1715 starten, `GIDETST` läuft, Meldung „OK" über `vramText()`/`laufeBisText`; Tastatur-Batchgrößen beachten (5 000 Zyklen) |
| **G5c** [erledigt] | **Systemtest mit echten Treibern** (Fremdsoftware aus `~/Documents/K1520emu/GIDE_Software/`, übersprungen wenn sie fehlt): A5120 mit CP/A-Bootdiskette, `GIDEHPD0.COM` (Basis D0H), `ERADIR C:`, Datei auf C: kopieren/lesen/vergleichen; K8915 mit SCPX und `gidn@D0h.com`; Abbild vorab leer. Ablauf über die Testhilfen (`TempDisk`, `typeString`, `runSmallUntil`). Befunde (Treiberverhalten, das das Modell verletzt) in den Entwurf. | G4 | Tests `GideSystem.*` (Label wie die anderen langsamen Systemtests nur nach Absprache — sonst `fast` halten) |
| **G5b** [entfaellt (F3: vorhandene Treiber genuegen)] | **Gasttreiber `GIDEDRV.COM`** (nachladbar wie `RAFCPM.COM`; Laufwerk z. B. `H:`) — **Entwurf zuerst** (DPB je OS, Partitionstabelle, CP/M-Grenzen) — **erst nach Anwenderentscheidung F3**. | G5a | Entwurfsabschnitt + Folgeplan |
| **G6** [zurueckgestellt (Basisfrage)] | **Oberfläche**: *Einstellungen ▸ Allgemein* Gruppe „IDE/GUN": Kästchen GIDE, Basis (Auswahl 80H…F0H mit Konfliktanzeige), Abbildwahl + „Neue Platte…" (Größe), RTC-Kästchen + Zeitquelle; Statuszeilen-Leuchte „IDE" (folgt dem **Zugriff**, wie die Laufwerksleuchte), Abbilddatei-Name; Konfigschlüssel `general.gide.*` in den vier `default_config_*.yaml` (Vorgabe **aus**), `Programmprofil`-Feld (alle vier Profile erlauben es), Handbuch-Abschnitt (`app/help/handbuch.md`, **Kürzeltabelle bleibt unberührt, kein Tastenkürzel**), Hilfe-Wächter `py_help`. | G4 | Python-Tests (offscreen) `tests/python/test_gide_gui.py`: Stecken per Konfiguration, Konfliktsperre RAF↔Netz, Zurücksetzen, Konfig-Rundlauf |
| **G7** [erledigt (2026-11-10; kein `fach_gide`: alle Gide-Tests schnell, Pfade ohne Fachlabel)] | **Paketierung/Merkposten**: `doc/merkposten/gun.md` (+ Eintrag in `doc/merkposten/README.md`), Kurzabsatz in `CLAUDE.md`, `tools/test_auswahl.py` (Pfade `core/cards/gide`, `peripherals/ata` → Fachlabel `fach_gide`), `doc/ci_pipeline.md` falls nötig, Paketlisten (Beigaben). | G6 | `tools/dev.sh test-merge --trocken` zeigt die richtige Auswahl; `py_testauswahl`, `py_packaging` grün |
| **U0** [offen] | **USB-Recherche** (nur Lesen/Schreiben von Notizen): *Vinculum Firmware User Manual* (VDAP-Monitor, Kurz-/Erweiterter Befehlssatz, Antwortformate, Statuszeichen, Sync `E`/`e`, Dateihandle-Modell) + *VDIP1 Datenblatt* (Pins, Parallel-FIFO-Zeitverhalten RXF#/TXE#/RD#/WR#) beschaffen; `pio3_usb_sch.pdf`/`diskveru.pdf` (AC1) per `pdftotext`; **AC-Zuordnung und Glue aus §5 bestätigen**. Ergebnis als §5-Nachtrag. | — | Aktualisierter §5 ohne `[?]` oder mit benannten Restlücken |
| **U1** [erledigt 2026-11-10; Protokoll §5a, Tests `VdipGeraet.*`/`VdipStick.*`/`VdipPfadflucht.*`] | `core/peripherals/vdip/vdip_geraet.{h,cpp}`: Firmware-Zustandsmaschine (Befehlszeile, `E`/`e`, `DIR`, `CD`, `OPR/OPW/CLF`, `RDF/WRF/SEK`, `DLF`, `MKD`, `FS`, `IDD`, Fehlertexte `Command Failed`/`Bad Command`), über einer **Wirtsverzeichnis-Abstraktion** (Pfad-Sandbox: nie aus dem Ordner herausführen, `..` abweisen, 8.3-Namen); Ein-/Ausgabe als Bytefolge mit FIFO-Pegeln. | U0 | Unit-Tests: Befehlsablauf, Pfadflucht-Tests (`..`, absolute Pfade, Symlinks), Datei anlegen/lesen/anhängen/löschen, große Datei |
| **U2** [erledigt 2026-11-10] | **Karte** `core/cards/gun_usb/`: Z80PIO (Mode 2) auf 84H–87H, Verhaltensglue VDIP↔PIO (B-Bits nach §5), Reset/PRG-Leitung, `installGunUsb(Config{ordner})` + C-ABI/Python/`--usb` wie G4. | G0, U1 | Gastprogramm-Test (Z80-Code im Test): Sync, `DIR`, Datei schreiben/lesen; Rundlauf mit Wirtsdatei |
| **U3** [offen] | GUI (Kasten „USB": Ordnerwahl, „Stick einlegen/entfernen"), Statuslampe, `GIDETST`/`USBTST.COM`, Handbuch, Konfig. | U2, G6 | wie G6 |
| **N0** [offen (Anwender)] | **Quellenbeschaffung KC-Net** (Anwender-Aufgabe, siehe F2).  Ohne Ergebnis: N1 darf nur das Modell **ohne** Karte liefern. | Anwender | Dokument/Quelltext/Hex im Ordner `doc/gun/` (**Lizenzstatus klären** vor Einchecken) |
| **N1** [offen] | `core/peripherals/w5100/`: W5100-Registermodell (Gemeinsam: Mode, IP/Gateway/Subnet/MAC, Socket-Register, Puffer 8+8 KB, TCP-Client/-Server, UDP) über `core/serial/net/socket.*`; **kein Netz im Emulationsfaden** (Regel aus `serielle_schnittstellen.md`: Hub/Wandler-Faden, Rückstau, Loopback-Tests auf Port 0). | — (nur Netzschicht) | Unit-Tests gegen Loopback: TCP-Echo, UDP, Verbindungsabbau, Puffervoll |
| **N2** [offen] | `core/cards/gun_net/`: PIO (88H–8BH) + ATmega-**Verhaltensmodell** gemäß N0-Protokoll, `installGunNet`, C-ABI/Python/`--net`, Konflikt zu RAF. | N0, N1, G0 | Gast-Z80-Test: Socket öffnen, Daten senden/empfangen gegen Test-Server auf Loopback |
| **N3** [offen] | GUI (Kasten „Netzwerk": Wirtsschnittstelle/Weiterleitung, Statuslampe), Handbuch, Konfig; Sicherheitshinweis: der Gast öffnet Verbindungen **nach außen** — Standard „nur Loopback/NAT-Ziel nach Rückfrage", **keine Rückfrage-Umgehung**. | N2, G6 | wie G6 |

Reihenfolge: **G0 → G1 → G2 → G3 → G4 → G5a → G6 → G7** (Paket 1+2), danach **U0 → U1 → U2 → U3**,
parallel/danach **N1** (unabhängig), **N2/N3** erst nach **N0**.
Parallelisierbar (verschiedene Dateien, aber **nicht** gleichzeitig bauen — je Worktree): G1 ‖ U0, N1 ‖ U1.

## 9. Fragen an den Anwender (Stand 2026-11-10: beantwortet)

**Antworten:** F1 — Jumperstellung unbekannt: im Kern ist die Basis frei, die Maschinenvorgabe wird **später** entschieden (G6 zurückgestellt;
D0H bleibt die übliche Praxisbasis, §2a).  F3 — die vorhandenen Poppe-/Kingstener-Treiber genügen, **G5b entfällt**.  F5 — RTC **wie P8000**
(UTC, Maschinenzeit).  F2 (KC-Net/USB-Software) und F4 (Netz + RAF) bleiben offen und betreffen nur USB/Netz.  Ursprüngliche Fragen:

1. **F1 GIDE-Basis:** Hat jemand die tatsächliche Jumperstellung der GUN/GIDE-K1520 (Heiko Poppe)?
   Vorschlag 90H (§7.1) — ist das für deine Hardware passend?
2. **F2 KC-Net-/USB-Software:** Liegen dir die Projektseiten „USB"/„KCNET" von susowa (oder die
   ATmega-Firmware/Software) vor?  Ohne sie bleibt §6 bei N1 stehen.  Hast du eine GUN/GIDE-Karte zum
   Nachmessen (z. B. Datenregister-Verhalten, RTC-Zeitraster)?
3. **F3 Gasttreiber:** Soll `GIDEDRV.COM` (nachladbarer Plattentreiber für CP/A/SCPX/SCP) mit in den
   Umfang, oder reichen vorerst das Prüfprogramm und das Registermodell (es gibt dann noch keinen
   „Laufwerksbuchstaben")?  Empfehlung: **ja, als eigenes Paket nach G5a**, weil die Platte sonst
   nur Programmierern dient.
4. **F4 Zusatzbezug:** USB und Netz kollidieren auf 88H/89H mit der RAF (Netz ganz, USB nicht).
   Akzeptabel, dass die Oberfläche „Netz + RAF" sperrt?
5. **F5 RTC-Zeitquelle:** wie P8000 (UTC, Maschinenzeit-Politik)?  Empfehlung ja.

## 10. Risiken

- **Quellen:** GIDE/RTC sicher; USB mittel (Handbuch beschaffbar); **Netz unbekannt** (Anwender).
- **Z80PIO Mode 2** mit externen Strobes (U2) — ggf. Primitive erweitern (Chip-Wächter, „Chips werden
  vollständig getestet").
- **Portkonflikte** je Maschine (G0) — vor allem K8915/PC 1715.
- **Zeitverhalten** der Gastsoftware (BSY-Schleifen): modelliert wird minimal sichtbares BSY.
- **Urheberrecht** der Originalquellen: nicht einchecken (§2).

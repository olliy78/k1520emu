<!-- Angelegt 2026-11-10 (AP G7, Entwurf 50).  Gilt WIE CLAUDE.md, sobald an der GIDE (IDE-Platte + RTC 72421)
     gearbeitet wird.  Begruendung: doc/merkposten/README.md -->

# GIDE / GUN (IDE-Platte + RTC + USB) — Merkposten

**GIDE** (Tilmann Reh, nie von Robotron gefertigt) als **steckbare Option** in allen K1520-nahen Maschinen
(A5120 auch .16, K8915 V3/Gen2, PRG 710/-1, PC 1715/1715W; MC80 mechanisch), Vorgabe überall **aus**.
Karte `core/cards/gide/` (Klasse `Gide`), Platte `core/peripherals/ata/` (`AtaGeraet`), RTC 72421 = das
Chipmodell der P8000-Uhr (`core/cards/p8000/rtc.*`, `P8000Rtc72421`).  Bestückung
`K1520Machine::installGide(Gide::Config, const RtcConfig*)` (`core/machines/machine.h`), C-ABI `k1520_gide_*`,
Python `K1520Emulator(gide=…)`, Werkzeuge `boot_trace`/`k1520dbg --gide` + Debugger-Befehl `gide`.
Entwurf, Quellenlage, Treiberbefunde: **`doc/design/50_gun_gide.md`** (§3 Registermodell, §4 RTC, §2a Fremdsoftware).
Die **GUN** (PC-1715-Platine mit GIDE + USB + Netz) ist Vorbild; abgebildet werden die Funktionsblöcke, nicht die Platine.

## Festlegungen, die man nicht aufweichen darf

| # | Festlegung | Wächter |
|---|------------|---------|
| 1 | **Portdekodierung:** Basis = Hochnibble (Jumper A4–A7), die Karte belegt **Basis+4 … Basis+F** (12 Tore), alles andere antwortet nicht.  +4 (EXP) ohne Wirkung, +5 = RTC, +6/+7 = IDE /CS1 (Alternate Status/Device Control, Drive Address), +8 = 16-Bit-Daten, +9…+F = Task-File.  Basis mit gesetztem Tiefnibble → `std::invalid_argument`.  /IORQ allein, A8–A15 werden **nur für die RTC** ausgewertet. | `Gide.NurBasisPlus4BisFAntworten`, `Gide.ExpPortOhneWirkung`, `Gide.BasisA0`, `Gide.PortkonfliktWirftUndBasisIstVielfachesVon16` |
| 2 | **LH-Wechsel-Flipflop** (aus GAL1, `gide1.pds`): kippt bei JEDEM Zugriff auf +8 und +9…+F, wird aber bei Task-File-Zugriff (+9…+F) auf **0** gesetzt; +6/+7 ändern ihn nicht.  Lesen +8: 1. Zugriff holt das Wort von der Platte (`IORD`) und liefert das **Low-Byte**, 2. Zugriff das gemerkte High-Byte ohne `IORD`.  Schreiben: 1. Zugriff merkt Low, 2. schreibt (`IOWR`).  **Low-Byte zuerst.**  `INIR`/`OTIR` je 2 × 256 B = 1 Sektor; ein Task-File-Zugriff mitten im Sektor setzt die Folge zurück — so nachbilden, nicht „entschärfen“. | `Gide.LowByteZuerst`, `Gide.TaskFileZugriffMittenImSektorSetztFolgeZurueck`, `Gide.ZugriffAufCs1AendertLhNicht`, `Gide.UngeradeZugriffszahlLiestUndSchreibtNichtVorzeitig`, `Gide.ResetSetztLhZurueckMediumBleibt` |
| 3 | **Gast-Rundlauf nach Rehs Routinen** (`HDInit`: DOR 06h/02h, 91h; Lesen 20h + DRQ + 2×`inir`; Schreiben 30h + 2×`otir`) mit echtem Z80 im Test.  Kein Interrupt, kein /WAIT, kein IORDY — der Gast fragt BSY/DRQ ab; BSY ist **mindestens eine Statusabfrage lang sichtbar**. | `Gide.RundlaufHdInitSchreibenLesenByteweiseGleich`, `AtaGeraet.BsyIstMindestensEineAbfrageSichtbarDannDrq`, `AtaGeraet.BsyEndetAuchOhneAbfrageNachTakten` |
| 4 | **Basis = Jumper.**  Die Jumperstellungen der echten Karten sind unbekannt (Anwenderentscheid 2026-11-10): im **Kern ist die Basis frei** (`Gide::Config::basis`, Vielfaches von 10H); die **Maschinenvorgabe** (feste Verdrahtung oder Wahl in der Oberfläche) ist **offen, später** (G6).  Die Tests stecken ausdrücklich Basis (A0H/90H/B0H/D0H) — nicht auf eine Vorgabe verlassen. | `GideMaschine.A5120_AbweichendeBasisB0`, `py_gide_api` (`test_abweichende_basis_und_ohne_rtc`) |
| 5 | **Übliche Basis in der Praxis ist D0H** (Poppe-Treiber `GIDEHPD0`, Kingstener `gidn@D0h`, GUN am PC 1715, K8915-Gerippe).  **D0H kollidiert** mit der Kassette K5020 (K8915, PC 1715; am A5120 30H) und mit dem EPROMmer des PRG 710 (D0–D4).  Frei auf allen Maschinen: 90H–9FH, A0H–A7H, F0H–FFH (außer PRG-ATP F4/F5).  Kollision → `installGide` meldet den Grund (`gideFehler()`), **kein Absturz**; die Tore werden **vor** dem Anmelden geprüft (`belegtGrund`, Vorbild `installK5020`, nicht der Wurf von `registerIO`). | `GideMaschine.Bestueckung_*` (Konfliktfälle), `cli_dbg_gide_konflikt` |
| 6 | **Gesteckt wird nach dem Anlegen, vor dem ersten `run()`/`reset()`**; danach, doppelt oder bei belegtem Tor `false` + `gideFehler()` (C-ABI `k1520_gide_error`, Python `ValueError` mit Grund).  P8000: nicht steckbar (kein K1520-Bus).  Der Maschinentakt kommt über `gideTakt(absolute Takte)` aus der Laufschleife — **`takt()` muss laufen**, sonst weder verzögertes Zurückschreiben noch BSY-Ende noch RTC. | `GideMaschine.Bestueckung_*`, `py_gide_api` (`test_zu_spaet_gesteckt_scheitert`, `test_p8000_hat_keinen_steckplatz`) |
| 7 | **`/RESET` setzt LH und die Platte zurück, Abbild und RTC bleiben; Netz-Ein (`powerOn`) löscht Befehlsspur und Zähler.**  Nicht im Save-State (wie RAF/K6022/K5020; neue Teile bräuchten eine neue Fassungsnummer). | `Gide.ResetSetztLhZurueckMediumBleibt` |
| 8 | **RTC 72421 an Basis+5:** Registernummer = **A8–A11 des Zyklus** (`K1520Bus::ioAddress()`, Gast: `LD B,reg / OUT (C),A`), Lesen liefert den **reinen Nibble** (ohne die P8000-Kennung `30H`).  Zeit läuft in **Maschinenzeit** (steht ohne Takte), springt beim Start **nie zurück** (Batteriedatei, `max(Stand+Abwesenheit, Wirtszeit)`), Oberfläche speist **UTC** — wie die P8000-Uhr (Merkposten p8000 Nr. 59).  **`schreiben_unter_hold = true` NUR für die GIDE**: Rehs `RTCset` stellt unter HOLD (nie STOP); im P8000-Modell bleibt es aus.  Batteriedatei hat den **eigenen Kopf `K1520-GIDE-RTC 2`** (P8000: `K1520-P8000-RTC 2`) — die beiden Dateien nie verwechseln.  RTC wählbar (`rtc_vorhanden`); nicht bestückt → FFH, Schreiben verpufft.  Kein Interrupt. | `GideRtc.*`, `Gide.RtcHookRegisterNummerAusA8bisA11`, `Gide.RtcNichtBestueckt_IgnoriertDenHook`, **P8000-Wächter `P8000Rtc72421.*` müssen grün bleiben** (Chipmodell geteilt) |
| 9 | **Plattenabbild:** Rohabbild mit 512-B-Sektoren (`.img`/`.hdf`) oder **`.img.gz`** (Art folgt den Magic Bytes); liegt ganz im Speicher.  Änderungen gehen **verzögert** zurück (Schreibpause in Maschinentakten, bei gzip zusätzlich 5 s Wirtszeit Abstand, atomar); Auswerfen schreibt sofort.  **Sperrdatei `<abbild>.lock`** (Prozessnummer; Sperre eines toten Prozesses wird übernommen), zweites Öffnen scheitert.  „Neue Platte“ legt ein **leeres (sparse) Abbild** an, `.gz` = gepackt, eine vorhandene Datei wird nie überschrieben. | `AtaGeraet.VerzoegertesZurueckschreibenNachSchreibpause`, `AtaGeraet.AuswerfenSchreibtSofortZurueck`, `AtaGeraet.GzipRundlauf`, `AtaGeraet.ZweitesOeffnenDesselbenAbbildsScheitertSperrdateiWirdGeloest`, `AtaGeraet.VerwaisteSperreEinesToterProzessesWirdUebernommen`, `AtaGeraet.DatenBleibenNachFlushUndNeuOeffnenErhalten` |
| 10 | **ATA-Geräteregeln:** CHS **und** LBA (SDH Bit 6), 91h ändert die Übersetzung, ECh Identify (Geometrie, Kapazität, Modell), 20h/30h/21h/31h, 40h/41h, 90h/10h/70h, C4h–C6h; EFh/Power **tolerant**, unbekannt → ABRT; Sektor jenseits des Endes → IDNF; **Slave nicht vorhanden** (SDH Bit 4); Datenregister ohne DRQ liefert 0 und schreibt nichts; ohne Medium ABRT, Identify geht.  In Rehs Quellen belegt sind nur 20h/30h/91h/ECh. | `AtaGeraet.*` |
| 11 | **Zugriff nur über 8 Bit** (LH-Folge, Festlegung 2); die Kartenklasse bietet **keinen** 16-Bit-Weg am Bus.  Eine Maschine, die E/A-Adressen auf 8 Bit abschnitte, wäre wie bei der RAF zu berichtigen (`ioAddress()` stimmt in allen vier, Entwurf 50 §7.1a). | `GideMaschine.*_Z80SchreibtLiestUndLiestRtc` |
| 12 | **Oberfläche der GIDE (2026-11-11):** Einstellungen = nur FLACHE Zeilen im Reiter Allgemein (Haken+Basis, Uhr: Haken+Zeitversatz, Uhrzeit Gast/PC, „Nach PC-Uhr stellen”); **keine Gruppenrahmen, keine eigenen Kästen** (Anwenderentscheid).  Das **Plattenabbild** steht im Reiter *Laufwerke* (`app/ui/gide_platte_widget.py`, sichtbar nur bei gesteckter Karte, wie der P8000-Plattenkasten) und ist **nur gepackt** (`.img.gz`, Endung erzwungen; ein rohes `.img` wird mit Hinweistext abgelehnt; der Kern liest Roh weiter).  `AtaGeraet::abbildAnlegen(.gz)` schreibt als Strom (`gzip::nullenSpeichern`, 256 MB ≈ 260 KB, kein Rohpuffer).  Uhr: `general.gide.rtc_versatz` (`'0'`/`'-28'`/`'-40'`, Vorgabe `-40` wie P8000) → `k1520_gide_install_ex`; Netz-Aus/Ein wirkt auch an der GIDE-Uhr (`Gide::powerOn` → `netzEin`, `k1520_gide_rtc_netz_aus` beim Ausschalten/Beenden der GUI).  Die Einstellungsseiten sind senkrecht rollbar (Mindesthöhe unabhängig von der Zeilenzahl). | `py_gide_gui`, `py_gide_api`, `GideRtc.*`, `GzipDatei.NullenSpeichern*` |

## Fremdsoftware: nur lokal, Tests überspringen

Die Gast-Treiber (Poppe `GIDEHPxx.COM`, Kingstener `gidn@D0h.com`, `ERADIR.COM`, Rehs `GIDEIDE/GIDERTC.MAC`) tragen
**ungeklärte Lizenz** und liegen **nie im Repo**, nur lokal in `~/Documents/K1520emu/GIDE_Software/`
(`K1520_GIDE_SW` überschreibt den Ort).  `GideSystem.*` **überspringt** sich, wenn sie fehlt — ein Skip ist kein Rot,
aber auch kein Beleg; auf dem Entwicklungsrechner muss er einmal grün gelaufen sein.  Die Forumsdoku
`GIDE_K1520_Doku.pdf` wurde wegen Urheberrecht entfernt — nicht verwenden.  Der Emulator ist aus der
*Verhaltensbeschreibung* (GAL-Gleichungen) neu geschrieben.

## Treiberanforderungen (aus den echten Treibern, Entwurf 50 §2a „Ergebnis G5c“)

- **Poppe `GIDEHPD0`** (CP/A, A5120): sendet **nie 91h**, adressiert **CHS ohne LBA-Bit**, Kopf = Laufwerk (SDH = A0H+n, bis 8 Laufwerke),
  500 Zylinder × 16 Köpfe × 63 Sektoren → **mindestens 504 000 Sektoren (≈ 258 MB)**, als Rohabbild **sparse** anlegen.
  Am CP/A-Fixture belegt das BIOS A:/C:, der Treiber vergibt D:…K:.  Nach `ERADIR D:` findet der erste Befehl auf A: nichts,
  bis `DIR D:`/`DIR A:` gelaufen sind (Treiber-/BDOS-Verhalten, Diskette unversehrt).
- **Kingstener `gidn`** (SCPX, K8915, Laufwerk N:): sendet **91h** (SC=40h, SDH=A0H|3 → 4 Köpfe × 64 Sektoren), braucht **≥ 256 Zylinder**.
- **Plattenformate sind untereinander unverträglich** (Poppe ≠ DL/Kingstener ≠ im CP/A-BIOS eingebaut): falscher Treiber = Datenverlust.
  Der Emulator ist davon nicht betroffen (nur Plattenabbild); ein späteres DiskTool-Dateisystem müsste die Variante kennen.
- Vorgabegröße für „Neue Platte“ in der Oberfläche (G6): **256 MB sparse**; kleinere Abbilder gehen nur mit dem Kingstener-Treiber.
- SCP 3.0 (PC 1715W) wird von den bekannten Treibern nicht unterstützt.

## Wächter-Übersicht

`AtaGeraet.*` (`tests/unit/peripherals/test_ata_geraet.cpp`), `Gide.*` (`tests/unit/cards/test_gide.cpp`),
`GideRtc.*` (`test_gide_rtc.cpp`), `GideMaschine.*` (`tests/integration/test_gide_maschinen.cpp`: Stecken je Maschine, zu spät,
doppelt, Tor belegt, Z80-Gastcode Sektor + RTC), `GideSystem.*` (`test_gide_system.cpp`: **echte Treiber**, lokal), `py_gide_api`
(C-ABI/Bindung; `py_c_api` vergleicht Header/Bibliothek/ctypes mechanisch), `cli_bt_gide`, `cli_bt_gide_k8915`, `cli_dbg_gide`,
`cli_dbg_gide_keine`, `cli_dbg_gide_konflikt`.  Alle sind **schnell**; es gibt kein Fachlabel `fach_gide` (siehe Entwurf 50 §8, G7).

## Ausdrücklich NICHT gemacht

- **G6 Oberfläche** (Einstellungen, Statuslampe „IDE“, Konfigschlüssel `general.gide.*`, Handbuch) — zurückgestellt, bis die Basisfrage
  (Festlegung 4) entschieden ist.  Es gibt keinen Programmprofil-/Konfigurationsweg; `gide=` gilt nur in Python/CLI/Werkzeugen.
- **Netz (W5100/KC-Net, 88H–8BH)** — Entwurf 50 §6, APs N*; kollidiert mit der RAF, am K8915/PRG 710 nicht steckbar (CTC/PIO der ZRE).
  In Planung (N1 laeuft), das KC-Net-Protokoll fehlt noch; keine Zusagen.  USB siehe unten (U1/U2 fertig, GUI U3 zurueckgestellt).
- **G5b Gasttreiber `GIDEDRV.COM`** — entfällt: die vorhandenen Poppe-/Kingstener-Treiber genügen (Anwenderantwort F3).
  Auch kein eigenes Prüfprogramm `GIDETST.COM` (G5a ist durch die echten Treiber, G5c, überholt).
- **Save-State**, **P8000** (kein K1520-Bus), **DiskTool-Dateisystem für IDE-Abbilder**.
- **PRG 710 und PC 1715 am Gastsystem:** dort nur Z80-Prüfcode (`GideMaschine.*_Z80…`), kein passendes Treiber-/Boot-Paar
  geprüft; MC80 nicht geprüft.

## USB (GunUsb + VdipGeraet) — AP U1/U2

USB-Karte der GUN: **Z80-PIO + VDIP1** (Vinculum, Firmware VDAP 3.68) als steckbare Option, Vorgabe aus.  Karte `core/cards/gun_usb/`
(`GunUsb`), Firmware als Byte-Zustandsmaschine `core/peripherals/vdip/` (`VdipGeraet`, `VdipStick`).  Bestueckung
`K1520Machine::installGunUsb(GunUsb::Config)` **nach dem Anlegen, vor dem ersten `run()`/`reset()`**; C-ABI `k1520_usb_*`
(`install`/`installed`/`error`/`stick_insert`/`stick_remove`/`status`), Python `K1520Emulator(usb=…)`, `--usb[=basis]`/`--usb-dir <ordner>`
in `boot_trace`/`k1520dbg`.  Entwurf: `doc/design/50_gun_gide.md` §5 (Karte), §5a (Protokoll), §7.1a (Portlage).

| # | Festlegung | Waechter |
|---|------------|----------|
| U1 | **Vier Tore ab Basis:** +0 Daten A (AD0..7, Mode 2), +1 Daten B, +2 Steuer A, +3 Steuer B (die Karte vertauscht gegenueber der PIO-Zaehlung 0=DA 1=SA 2=DB 3=SB: A0 waehlt Port A/B, A1 Daten/Steuer).  **Basis = Vielfaches von 4** (GUN fest **84H**; UTools-CP/M-Fassung/Z9001-Variante **DCh..DFh**), sonst `std::invalid_argument`.  **Kein INT genutzt** (die PIO haengt nur in der Interruptkette, die UTools brauchen ihn nicht). | `GunUsb.BusAnmeldungUndKonflikt`, `GunUsb/GunUsbBasis.InitSyncDirUndDateienRundlauf` (parametrisiert ueber Basen), `GunUsbMaschine.A5120_BasisDC` |
| U2 | **Port-B-Belegung:** B0 RXF# (E), B1 TXE# (E), B2 RD# (A, aktiv low), B3 WR (A, Impuls aktiv high), B4/B5 Eingaenge ohne Funktion, B6 RES# (A), B7 PRG# (A).  **PRG# wird nicht ausgewertet** (Programmiermodus des Chips nicht nachgebildet).  RXF#/TXE# werden bei jedem Zugriff aus dem `VdipGeraet` auf B0/B1 gelegt; TXE# ist immer bereit (**kein Zeitmodell**). | `GunUsb.PegelRxfTxe`, `GunUsb.LesenOhneDatumAendertNichts`, `GunUsb.SyncWieUsbinc` |
| U3 | **Verhaltensmodell statt Gatter [?]:** die 74LS00-Gatter des Schaltplans werden nicht einzeln nachgebaut.  **Fallende RD#-Flanke mit RXF# = 0 → naechstes Byte sofort ins Eingaberegister von Port A** (der Gast `USBINC.Z80` liest Port A VOR dem Deaktivieren von RD#; die echte PIO luede erst bei der steigenden BSTB-Flanke).  **Fallende WR-Flanke (B3) → Byte an das VDIP1** (TXE# = 0 vorausgesetzt), ASTB = NAND(B2,B3).  **Abgesichert nur durch das Verhalten der echten UTools-Binaerdateien**, nicht durch den Schaltplan — nicht „der Schaltung zuliebe" umbauen, ohne `GunUsbSystem.UToolsA5120` gruen zu sehen. | `GunUsb.*`, `GunUsbSystem.UToolsA5120` |
| U4 | **RES#-Impuls (B6 low → high) startet die Firmware neu** (`VdipGeraet::reset`, Startmeldung in der Ausgabe); `/RESET` der Karte setzt PIO und VDIP1 zurueck, **der Stick bleibt gesteckt**; Stick-Wechsel zeigt sich ohne Zwischenzugriff (`Device Detected/Removed P2`); schreibgeschuetzter Stick lehnt Schreiben ab. | `GunUsb.ResImpulsStartetFirmwareNeu`, `GunUsb.ResetDerKarteLaesstStickGesteckt`, `GunUsb.StickWechselZeigtSichOhneZwischenzugriff`, `GunUsb.SchreibgeschuetzterStickLehntSchreibenAb`, `VdipGeraet.ResetSchliesstDateiUndLiefertBanner` |
| U5 | **SCS/ECS (Vertauschung nicht „berichtigen"):** **ECS** = Vorgabe nach Reset, Textbefehle (`DIR`, `OPW` …), Prompt `D:\>` CR, Klartext-Fehler; **SCS** (per `10h`) = Kurzsatz mit Opcodes, Prompt `>` CR, Kurzfehler.  Opcodes und Mnemonics werden in beiden Modi erkannt.  **Der Prompt ist zugleich der Status:** Erfolg = Prompt CR, Fehler = Fehlertext CR **ohne** Prompt.  `E`/`e` (Echo) ohne Prompt. | `VdipGeraet.SyncWieUtools`, `VdipGeraet.EchoMitHalbfertigerZeileUndUnbekannt`, `VdipGeraet.StartmeldungMitStick`, `VdipGeraet.StartmeldungOhneStick` |
| U6 | **Stick = Wirtsverzeichnis mit PFAD-SANDBOX — nie aufweichen:** nur strikte **8.3-Namen** (eingeschraenkter Zeichensatz, keine Geraetenamen, GROSS normalisiert), **nie ausserhalb der Wurzel** (kein `..`/Pfadtrenner/absoluter Pfad ausser dem Befehl `CD ..`/`CD /`), **Symlinks und alles ausser regulaeren Dateien/Verzeichnissen sind unsichtbar**; jede Pfadkomponente wird bei jedem Zugriff erneut geprueft (auch nach `CD`, auch die Wurzel selbst als Symlink).  Fehler kommen als VDIP-Fehlertext, nie als Ausnahme.  Ein Gast mit freier Textzeile darf den Wirt nie lesen oder beschreiben koennen. | `VdipPfadflucht.KeinZugriffAusserhalbDesSticks`, `VdipPfadflucht.CdPunktPunktUndWurzel`, `VdipPfadflucht.SymlinksNachAussen`, `VdipPfadflucht.VerzeichnisWirdNachCdDurchSymlinkErsetzt`, `VdipPfadflucht.WurzelSelbstAlsSymlinkWirdAufgeloest`, `VdipPfadflucht.TestKannRotWerden`, `VdipStick.Namen83` |
| U7 | **Portlage je Maschine** (Entwurf 50 §7.1a, Befund U2): **84H..87H frei** am A5120 (auch .16), **PC 1715** und **K8915 V3** (die ZRE-CTC belegt nur 80H..83H).  **Belegt** am **K8915 Gen 2** (K2521: CTC/PIO auf 80H..87H) und am **PRG 710/-1** → dort **freie Basis wie DCh**; Konflikt → `installGunUsb` meldet den Grund (`k1520_usb_error`, Text `--usb: E/A-Tor 84H belegt`), **kein Absturz**, Pruefung der Tore **vor** dem Anmelden (`belegtGrund`). | `GunUsbMaschine.Bestueckung_A5120`/`_A5120_16`/`_K8915`/`_K8915_Gen2`/`_Prg710`/`_Prg710_1`/`_Pc1715`, `cli_dbg_usb_konflikt`, `py_usb_api` (`test_zre_belegt_84h_aber_freie_basis_geht`) |
| U8 | **Gast-Rundlauf mit echtem Z80** an allen Maschinen: Startmeldung holen; ausserdem Sync, `DIR`, Schreiben/Lesen (`WRF`/`RDF`, `OPW` mit Anhaengen/Stempel), `SEK`, `MKD`/`CD`/`DLD`, `DLF`, `REN`, `FS`/`IDD`, Fehlerfaelle (Datei fehlt, schon offen, ohne Stick, schreibgeschuetzt) mit Gegenpruefung gegen die Wirtsdatei. | `GunUsbMaschine.*_Z80HoltStartmeldung`, `VdipGeraet.SchreibenWieUput_UndPruefungGegenWirtsdatei`, `VdipGeraet.LesenWieUget_UndPruefungGegenWirtsdatei`, `VdipGeraet.DirtDatumUndZeit`, `VdipGeraet.ZeitUmrechnung`, `VdipGeraet.OpwHaengtAnVorhandeneDateiAn`, `VdipGeraet.SchonOffen`, `VdipGeraet.OhneStick`, `VdipGeraet.SchreibgeschuetzterStick` |

### Fremdsoftware (UTools): nur lokal, Tests ueberspringen

Quellen und Binaerdateien der **UTools** (`USB.COM`, `UDIR.COM`, `UPUT.COM`, `UGET.COM`, `USBINC.Z80` …) liegen nur lokal in
`~/Documents/K1520emu/GIDE_Software/utools14/` (`K1520_GIDE_SW` ueberschreibt den Ort), **Lizenz ungeklaert, nie ins Repo**.
`GunUsbSystem.UToolsA5120` (CP/A am A5120, Karte auf DCh: `USB FWV`, `UDIR`, `UPUT`, `UGET`) **ueberspringt sich ohne sie** — ein Skip ist
kein Rot, aber auch kein Beleg; auf dem Entwicklungsrechner muss er einmal gruen gelaufen sein.  Das Protokoll (Entwurf 50 §5a) ist aus
den UTools-Quellen und dem Verhalten der Binaerdateien abgeleitet, **nicht** aus einem Firmware-Handbuch.

### Nicht nachgebildet / offen

- **PRG#-Programmiermodus** (B7), **Zeitmodell** (TXE#/RXF# immer sofort), **Interrupt** der PIO in der Praxis.
- Textformat von **`FS`/`IDD`/`FWV`** [?] (nur so weit, wie die UTools es auswerten), **Opcodes ausser den von UTools benutzten** [?] (aus der
  Erinnerung), **IPA-Zahlenform** [?], **Reihenfolge der `DIR`-Ausgabe** (Wirtsverzeichnisreihenfolge, am echten Stick FAT-Reihenfolge).
- **GUI** (U3/N3, 2026-11-11): Gruppen `USB-Stick (GUN/VDIP1)` und `Netzwerk (KCNet/W5100)` (`app/usb.py`, `app/netz.py`, `app/ui/karten_gruppe.py`), `general.usb`/`general.net`; jede Karte faellt im Neuaufbau fuer sich zurueck (`MainWindow._karten_bauen`); Stick stecken/entfernen, Stick-Ordner, Schreibschutz und Netzkabel wirken ohne Neuaufbau, Loopback nur beim Stecken; Netz-Vorgabe `nur_loopback` AUS (Anwenderentscheid). Waechter `py_usb_netz_gui`.
- Nicht im Save-State, P8000 nicht steckbar (kein K1520-Bus).
- Wachter-Uebersicht: `GunUsb.*` (`tests/unit/cards/test_gun_usb.cpp`), `VdipGeraet.*`/`VdipStick.*`/`VdipPfadflucht.*`
  (`tests/unit/peripherals/test_vdip_geraet.cpp`), `GunUsbMaschine.*` (`tests/integration/test_gun_usb_maschinen.cpp`), `GunUsbSystem.*`
  (`test_gun_usb_system.cpp`, lokal), `py_usb_api`, `cli_bt_usb`, `cli_bt_usb_k8915`, `cli_dbg_usb`, `cli_dbg_usb_keine`, `cli_dbg_usb_konflikt`.
  Alle schnell.

## KCNet-Netzkarte (AP N2, Stand 2026-11-10)

`core/cards/gun_net/` (`k1520::GunNet`: Z80-PIO + Verhaltensmodell der ATmega-Firmware vor `k1520::w5100::W5100`), gesteckt ueber `K1520Machine::installGunNet`
**nach dem Anlegen, vor dem ersten `run()`/`reset()`** (C-ABI `k1520_net_*`, Python `K1520Emulator(net=…)`, `--net[=basis]` in `boot_trace`/`k1520dbg`, Befehl `net`).
Protokoll: `doc/design/50a_kcnet_protokoll.md` (aus den Gast-Binaerdateien rekonstruiert, seit 2026-11-10 abgeglichen mit der OFFIZIELLEN Beschreibung "KCNET Protokoll Version 1.2" von R. Kaestner, kc85.info; dort §10 massgeblich).
Die ATmega-Firmware ist Closed Source (nur auf persoenliche Anfrage) -> das Verhaltensmodell ist der gewollte Weg.  Festlegungen mit Waechtern:

| Nr. | Festlegung | Waechter |
|---|---|---|
| N1 | **Handshake:** nach `OUT` (Daten A) steht PB0 = 1 bis zur naechsten Statusabfrage (Lesen von Daten B), PB7 = 1 nur bei einem Antwortbyte, nach `IN (Daten A)` sofort 0; das naechste Antwortbyte kommt erst bei der naechsten Statusabfrage.  Ohne Antwort ist PB7 = 0 (das IST die Erkennung "Karte da"). | `GunNet.HandshakePb0NachOutEinsPb7NachInNull`, `GunNet.ErkennungKarteDaUndVersion` |
| N2 | **Neuinitialisierung der PIO (Betriebsartwort an Steuer A) wirft angefangene Kommandos und unabgeholte Antworten weg** — Modellentscheidung, der echte ATmega sieht das nicht; sonst scheitert die Erkennung nach einem abgebrochenen Programm. | `GunNet.NeuinitialisierungWirftOffeneAntwortenWeg` |
| N3 | **Bit 15 der Gast-Adresse wird maskiert** (`W5100` rechnet mit 15 Bit): Gast `8000H|adr` -> `adr`, Tx `C000H..` -> `4000H..`, Rx `E000H..` -> `6000H..`.  Der Zeiger (Kommando 02) zaehlt im Block weiter und ueber Kommandos hinweg. | `GunNet.BlockSchreibenUndLesenUeberTxPufferUndRegister`, `GunNet.EinzelbyteZugriffMitBit15AufW5100` |
| N4 | **Kommando 03 zaehlt in MASCHINENZEIT** (`takt(total_cycles_)` je Befehl, Takt = `k6022TaktHz()` der Maschine), Umlauf 60000; 08 zaehlt ab 49152; 0D zaehlt ungueltige Kommandobytes (16 Bit). | `GunNet.TimerLaeuftInMaschinenzeitMitUmlauf60000`, `GunNet.DynamischerPortZaehltAb49152`, `GunNet.UngueltigesKommandoWirdGezaehltUndVerworfen` |
| N5 | **/RESET der Maschine setzt den W5100 zurueck** (Sockets zu, Register leer, MAC `02:4B:43:4E:45:54` = Vorgabe `Config::mac`); die Richtlinie bleibt.  Netz: Wirtsnetz, Vorgabe **frei** (Entscheidung des Anwenders); die TESTS bleiben auf Loopback/Port 0. | `GunNet.ResetSetztW5100ZurueckRichtlinieBleibt` |
| N6 | **Portlage:** Vorgabe 88H (GUN); 88H/89H gehoeren der RAF; PRG 710/-1: ATP-Zusatz belegt C0H..C3H, ZRE 80H..87H; MC80.3x: VIS 2a C0H..CFH, ASP 88H..93H (28H frei); MC80.2x, K8915 (V3/Gen 2), A5120, PC 1715: 88H und C0H frei.  Konflikt -> `installGunNet` meldet den Grund, kein Absturz. | `GunNetMaschine.*`, `cli_dbg_net_konflikt`, `py_net_api` |
| N7 | **Echte Gastprogramme** (nur lokal in `~/Documents/K1520emu/GIDE_Software/kcnet/sw/cpmnet87/`, `K1520_GIDE_SW`): TFTP1287/CPMN1587 erkennen die Karte (Basis C0H), Firmware 1.2, Hardware 1.0, online; CPMNET-TCP-Server mit Echo gegen einen Loopback-Client; `TFTP GET` gegen einen Loopback-Server auf UDP **69** (privilegiert: ohne Recht uebersprungen — lokal pruefbar mit `unshare -rn sh -c 'ip link set lo up && tools/dev.sh test -R GunNetSystem'`). | `GunNetSystem.*` |

| N8 | **Byte-Reihenfolgen nach der offiziellen Beschreibung:** Zaehler/Adresse/Timer/Fehlerzaehler niederwertig zuerst; IP-Adressen und die dynamische Portnummer (Kdo 8) in **Net-Order = hoeherwertiges Byte zuerst** (CPMNET/TFTP schreiben das erste Byte nach Sn_PORT 8404H; mit Lo zuerst scheiterte die Bindung des Gast-UDP-Sockets (Port 192), jetzt laeuft dessen DNS-Rueckfrage ins Leere — der Test wartet seither `kLangBudget`); Versionen Unter-, dann Hauptversion; Kennung hoechstens 128 Byte inkl. 0. | `GunNet.DynamischerPortZaehltAb49152InNetOrder`, `GunNet.KennungHoechstens128ByteMitNull`, `GunNetSystem.CpmnetTcpServerEchoMitLoopbackClient` |
| N9 | **Schreibzugriffe nur 8000H..FFFFH und nie in die MAC (8009H..800EH)** (Kdo 0 und 4; der Zeiger zaehlt trotzdem weiter); die MAC liest der Gast nur (`Config::mac`). | `GunNet.SchreibenNurAb8000HUndNichtInDieMac` |
| N10 | **524-ms-Regel (Maschinenzeit):** angefangenes Kommando oder nicht abgeholte Antwort ohne Byteverkehr werden nach 524 ms verworfen (Zeiger und Fehlerzaehler bleiben); im Ruhezustand keine Frist. | `GunNet.Frist524ms*`, `GunNet.FristLaeuftNichtWennKeinKommandoOffenIst` |
| N11 | **IP-Nummern 0..7** alle schreib-/lesbar; Nummer > 7 = Kommandofehler (Annahme [?]).  **Port B:** PB0 = ARDY, PB7 = invertiertes BRDY, PB1..PB6 = 0 (an Masse). | `GunNet.IpNummern0Bis7SindAlleSchreibUndLesbar`, `GunNet.PortBPb1BisPb6SindMasse` |

Nicht nachgebildet/offen: Kommando 0C (ID-String) hat keinen Gast-Beleg; DHCP/DNS/Ping-Gegenstellen (Broadcast/ICMP gibt es im W5100-Modell nicht — `H`/`D`/`P` in CPMNET sind ohne Wirkung);
kein Save-State; GUI (N3) offen; PRG 710 mit ATP-Zusatz nur mit freier Basis.

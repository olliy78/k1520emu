<!-- Angelegt 2026-11-10 (AP G7, Entwurf 50).  Gilt WIE CLAUDE.md, sobald an der GIDE (IDE-Platte + RTC 72421)
     gearbeitet wird.  Begruendung: doc/merkposten/README.md -->

# GIDE / GUN (IDE-Platte + RTC) — Merkposten

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
- **USB (VDIP1, 84H–87H) und Netz (W5100/KC-Net, 88H–8BH)** — Entwurf 50 §5/§6, APs U*/N*; Netz kollidiert mit der RAF, am K8915/PRG 710
  nicht steckbar (CTC/PIO der ZRE).  Das Netzprotokoll ist unbekannt (N0, Anwenderaufgabe).
- **G5b Gasttreiber `GIDEDRV.COM`** — entfällt: die vorhandenen Poppe-/Kingstener-Treiber genügen (Anwenderantwort F3).
  Auch kein eigenes Prüfprogramm `GIDETST.COM` (G5a ist durch die echten Treiber, G5c, überholt).
- **Save-State**, **P8000** (kein K1520-Bus), **DiskTool-Dateisystem für IDE-Abbilder**.
- **PRG 710 und PC 1715 am Gastsystem:** dort nur Z80-Prüfcode (`GideMaschine.*_Z80…`), kein passendes Treiber-/Boot-Paar
  geprüft; MC80 nicht geprüft.

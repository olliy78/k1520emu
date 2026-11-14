# 51 — SOUND-Karte K1520 (AY-3-8910/8912 + CTC) mit Tonausgabe

Stand: 2026-11-14, **Entwurf, nichts umgesetzt; Fragen F1–F4 beantwortet, Gastsoftware im Repo (`tools/sound/fremd/`).**  Recherche: Forum robotrontechnik.de Thread 14846
(„SOUND-Karte für K1520", 2017/18), Beschreibung `SOUND-K1520V2-Beschreibung.pdf` (Stand 13.02.2018) und
Gastsoftware `SKTEST4_BC/` aus `~/projects/robotron/rowikla/`.
**Grundsatz (Anwenderentscheid 2026-11-14): keine Fremdsoftware.**  Der Chip wird nach Datenblatt
selbst in C++ nachgebaut, die Tonausgabe über eigene, schlanke Ausgaben an die Systemschnittstellen
(ALSA bzw. `waveOut`) — kein ayumi, kein MAME-Code, kein QtMultimedia, kein miniaudio.

## 1. Ziel

Eine steckbare **SOUND-Karte K1520 V2** (Daniel Auerbach, Klaus Wilfling, Rolf Weidlich; nie von Robotron
gefertigt) als Option in allen Maschinen mit K1520-Steckplatz, gesteckt wie GIDE/K5020/RAF
(`K1520Machine::installSound` nach dem Anlegen, vor dem ersten `run()`/`reset()`), Vorgabe überall **aus**.
Dazu als neue Querschnittsfunktion: **Ton aus dem Emulator** — eine Tonquelle im Kern, die in
Maschinenzeit Abtastwerte erzeugt, eine Ausgabe an die Soundkarte des Wirts und eine WAV-Mitschrift.

Die Tonausgabe ist bewusst allgemein geschnitten: der Summer des K8915 (`bellCount`) oder ein späterer
Lautsprecher anderer Maschinen kann dieselbe Strecke benutzen (nicht Teil dieses Entwurfs).

## 2. Quellen

| Quelle | Ertrag |
|--------|--------|
| Forum Thread 14846 (Beiträge 000–214 wörtlich, Rest nur zusammengefasst) | Entwicklungsgeschichte, GAL K.12-Gleichungen (Beitrag 150), Störungen ohne Bustreiber, Stromaufnahme, Demoprogramme |
| `rowikla/Dokus/SOUND-K1520V2-Beschreibung.pdf` | **maßgeblich**: Adressbelegung, Jumper JP1–JP6, Takt, Kanalzuordnung, Stiftleiste S7 |
| `rowikla/Dokus/Interrupt_CPA_SCPX.txt` | Interruptsäule unter CP/A und SCPX: I = F7H, Karten-CTC Vektor **D8H** (F7D8H…F7DEH) |
| `rowikla/Dokus/K8924_Portadressen_KW2.txt` | 38H…3FH = SOUND-Karte (im K8924 von K. Wilfling eingebaut) |
| `rowikla/SKTEST4_BC/` | **Testprogramm `SKTEST4.COM`** (CP/A, SCPX; Quelle `SKTEST4.ASM`, Player `AYPBC.INC` von Rolf Weidlich, Lied `MUSIC.STC`) |
| AY-3-8910/8912-Datenblatt (General Instrument) | Registermodell, Zähler, Hüllkurvenformen, Pegelstufen |
| Schaltplan V2 (Google Drive, Beitrag 084/150) | noch nicht ausgewertet — nur nötig, falls §3 Widersprüche zeigt (Frage F3) |

| Google-Drive „Software" der Kartenseite (`k1520.8i8.de` → `sites.google.com/view/robotron/sound`) | Demos und Player je Rechner, **im Repo unter `tools/sound/fremd/`** (Herkunftstabelle dort) |

### 2a. Gastsoftware (Download 2026-11-14, Weitergabe freigegeben)

| Programm | Was | Nutzen hier |
|----------|-----|-------------|
| `TGCPM.COM` | 1-kHz-Ton auf A/B/C, Geräusch; ohne CTC | kleinster Rauchtest (Zeitschleife) |
| `SKTEST4.COM` | Komponententest: AY, AY-Ports, CTC kaskadiert, CTC-Interrupt + Lied | **Systemtest S7** |
| `STCTEST1.COM` | eingebautes Lied im Hintergrund (CTC-Interrupt), Prompt bleibt bedienbar | **Demo 1 auf der Diskette**; dazu `STCTEST1.mp3` = Aufnahme der echten Karte → Klangvergleich |
| `PLAYSTC.COM` + `SONG.STC` | spielt eine STC-Datei von Diskette (`PLAYSTC B:SONG.STC`) | **Demo 2 auf der Diskette** |
| `CPMSTCPL`/`CPMPT3PL` 1.0.5 + `SNDINIT2` | Player für STC/PT3 **vom USB-Stick** (GIDE/USB bzw. GUN), Einstellungen in `SOUND.INI` | AP S10: mit der emulierten GUN-USB spielt er Lieder aus einem Wirtsordner |

Die CP/A-/SCPX-Fassungen setzen I = F7H und Vektor D8H voraus; **PC-1715-Fassungen** benutzen Karte an
**E0H** und Vektor F7F0H (eigenes ZIP).  K8915 (Interruptsäule FFB0H) nur über die USB-Player mit
`SOUND.INI` (`SNDINIT2`-Tabelle: K8915 `38 2.46 b0ff d0`, A5120 `38 2.46 d8f7 d0`, PC 1715 CP/A
`E0 2.46 f0f7 d0`, PC 1715 SCP `E0 2.46 20f7 d0`).

## 3. Befund Hardware

### 3.1 Adressen (JP1, Komparator LS688/DL8121, 8er-Raster)

| Tor | Lesen | Schreiben |
|-----|-------|-----------|
| Basis+0 | AY: Inhalt des gewählten Registers | AY: Registeradresse setzen (Latch) |
| Basis+1 | — (Bus offen, FFH) | AY: gewähltes Register schreiben |
| Basis+2, +3 | frei | frei |
| Basis+4…+7 | CTC Kanal 0…3 | CTC Kanal 0…3 |

Basis = Vielfaches von 8 (A3…A7 über Jumper), Vorgabe der Unterlagen und aller Gastsoftware **38H**.
Aus der GAL K.12: `/CS_AY = /A1·/A2`, `BC1 = /A0·/A1·/A2`, `BDIR = /A1·/A2·/WR`, `/CE_CTC = A2`.
Das Lesen an Basis+0 ist belegt benutzt: `AYPBC.INC` liest R7 zurück (`OUT (38H),7 / IN A,(38H)`), um
die Portrichtungsbits zu erhalten; `SKTEST4` liest die Ports A/B.  Die GAL enthält `/M1=H`
(kein Ansprechen im Interrupt-Quittungszyklus).

### 3.2 Takt des AY (JP2)

| JP2 | Takt | Bemerkung |
|-----|------|-----------|
| Q1 | Quarzoszillator, Stückliste **3,579545 MHz** | über der Datenblattgrenze 2 MHz, funktioniert aber |
| Q1/2 | **1,7897725 MHz** | **empfohlen** (Tonhöhen der ZX-Spectrum-STC-Lieder) → Vorgabe |
| TAKT | Systemtakt der Maschine (A5120: 2,4576 MHz) | |

`SKTEST4` fragt den Takt ab (1,75 / 1,79 / 2,00 MHz) und stellt damit 1 kHz ein (Periode 6DH/6FH/7DH).
Der Quarzwert wird frei einstellbar (Bestückung), Vorgabe 3 579 545 Hz.

### 3.3 CTC der Karte

Eigene Z80-CTC an Basis+4…+7, Takt = Systemtakt des Busses, in der Interrupt-Prioritätenkette (JP3/JP6
regeln nur die Polarität der Kette — im Emulator ohne Bedeutung).  Stiftleiste S7 führt TO0, TR1, TO1,
TR2, TO2, TR3; über Jumper kaskadierbar (TO0→TR1, TO1→TR2, TO2→TR3).  Gastgebrauch: Kanal 0 als
Zeitgeber, 50 Hz (`AYPBC.INC` Variante 2: Steuerwort A7H, Zeitkonstante C0H → 2,4576 MHz/256/192 = 50 Hz),
Vektor D8H, I = F7H unter CP/A/SCPX.

### 3.4 Analogteil

Die Karte gibt **echtes Stereo** aus (links/rechts getrennt); der AY hat aber drei Stimmen, eine davon
liegt deshalb in der Mitte (auf beiden Seiten).  Welche, legt die Kanalzuordnung fest — Vorgabe im
Emulator **Stereo ABC** (Anwenderentscheid 2026-11-14), wählbar ACB und Mono.
Kanalzuordnung über steckbare Widerstände + Schiebeschalter S8: **ABC** (A links, B Mitte, C rechts) oder
**ACB** (A links, B rechts, C Mitte); Seitenkanal 10 kΩ, Mittelkanal 22 kΩ auf beide Seiten
(Gewicht Mitte ≈ 10/22 ≈ 0,45 je Seite).  JP4 = Mono.  C16/C17 (1 nF, optional) = Tiefpass.  Verstärker
PAM8403, Line-Out — beides ohne Einfluss auf das Modell außer der Lautstärke.

### 3.5 AY-3-8910 — was nachgebaut wird (Datenblatt)

| Reg. | Inhalt | lesbare Bits |
|------|--------|--------------|
| R0/R1, R2/R3, R4/R5 | Tonperiode A/B/C, 12 Bit (fein/grob) | R1/R3/R5: 4 Bit |
| R6 | Rauschperiode | 5 Bit |
| R7 | Mischer: Bit 0–2 Ton aus (A/B/C), Bit 3–5 Rauschen aus, Bit 6/7 Port A/B Ausgang | 8 Bit |
| R8–R10 | Pegel A/B/C: Bit 0–3 Stufe, Bit 4 = Hüllkurve | 5 Bit |
| R11/R12 | Hüllkurvenperiode, 16 Bit | 8 Bit |
| R13 | Hüllkurvenform (CONT/ATT/ALT/HOLD); **Schreiben startet die Hüllkurve neu** | 4 Bit |
| R14/R15 | E/A-Port A/B | Eingang: Pins (offen = FFH, interne Pull-ups); Ausgang: Registerwert |

- Ton: Zähler zählt mit Takt/16; Periode 0 wirkt wie 1.  Rauschen: 17-Bit-LFSR, Takt/16 geteilt durch
  die 5-Bit-Periode (Periode 0 = 1).  Hüllkurve: Takt/256 geteilt durch R11/R12, 16 Stufen, 16 Formen
  (10 verschiedene).  Kanalsignal = `(Ton | Ton_aus) & (Rauschen | Rauschen_aus)` × Pegel.
- Pegel: logarithmisch, 16 Stufen, Stufe 0 = still; Tabelle als **Messwerte** (Datenblatt-Kennlinie,
  veröffentlichte Messungen, je Stufe ≈ 3 dB) — Zahlen, kein fremder Code.
- Adresse ≥ 16 im Latch: Chip abgewählt (Lesen FFH, Schreiben wirkungslos).
- `/RESET` (systemweit): alle Register 0 → stumm, Ports Eingang.
- **AY-3-8912**: nur Port A herausgeführt; R15 bleibt als Register lesbar/schreibbar.

## 4. Aufbau im Emulator

```
core/primitives/ay38910.{h,cpp}    Chip: Register, Ports, Zähler, Hüllkurve, Pegel  → Abtastwerte
core/audio/                         Tonstrecke (neu, ohne Kartenwissen):
   tonquelle.h                        Schnittstelle "liefert Stereo-Abtastwerte in Maschinenzeit"
   ringpuffer.h                       einseitig sperrfrei (ein Erzeuger, ein Verbraucher)
   wav_schreiber.{h,cpp}              WAV-Mitschrift (16 Bit Stereo, 48 kHz Maschinenzeit)
   ausgabe.{h,cpp}                    Ausgabe an den Wirt: Auswahl der Ausgaben
   ausgabe_alsa.cpp                   Linux: libasound.so.2 per dlopen (keine Bauabhängigkeit)
   ausgabe_waveout.cpp                Windows: waveOut (winmm)
   ausgabe_stumm.cpp                  Tests/CI/kein Gerät
core/cards/sound_k1520/             Karte: Dekodierung, AY + CTC, Mischer ABC/ACB/Mono, Tiefpass
```

### 4.1 Festlegungen

1. **Maschinenzeit.** Die Karte bekommt wie die GIDE die absolute Taktzahl (`soundTakt`) und erzeugt
   daraus Abtastwerte mit 48 kHz *Maschinenzeit*.  Registerschreibzugriffe wirken taktgenau: vor jedem
   Schreiben rechnet der Chip bis zum aktuellen Takt nach („lazy catch-up"), dann ändert sich das Register.
2. **Abtastratenwandlung**: der Chip läuft intern in Schritten von Takt/8 (Hüllkurve, Ton, Rauschen auf
   diesem Raster), je Ausgabewert wird über alle Schritte **gemittelt** (Kastenfilter), danach ein
   einpoliger Tiefpass (≈ 8–10 kHz; C16/C17 bestückt = tiefer) und ein Gleichanteilfilter.  Kein
   Herauspicken einzelner Werte (Aliasing).
3. **Der Kern rechnet, die Ausgabe holt.**  Der Emulationsfaden schreibt in den Ringpuffer, der
   Ausgabefaden (im Kern, C++) liest daraus — **ohne Kernsperre** und ohne Python.  Die Oberfläche
   schaltet nur ein/aus, Lautstärke, stumm.
4. **Nur bei Echtzeit hörbar.**  Bei Faktor ≠ 1,0 (schneller/unbegrenzt) ist die Wirtsausgabe stumm und
   der Ringpuffer verwirft (kein Rückstau, kein Nachhall).  Die WAV-Mitschrift läuft in jedem Tempo
   (Maschinenzeit, deterministisch).
5. **Gleichlauf** bei Echtzeit: Füllstand-Regelung (Ziel ≈ 60 ms); über Ziel+X ein Wert verworfen, unter
   Ziel−X einer verdoppelt; Unterlauf = Stille, nie Blockieren des Emulationsfadens.
6. **Ohne Gerät läuft alles.**  Fehlt `libasound.so.2` / kein Ausgabegerät: Ausgabe „stumm", ein Satz im
   Grund (`k1520_audio_fehler`), keine Ausnahme, kein Meldungsfenster.
7. **Basis im Kern frei** (Vielfaches von 8, sonst Ablehnung), Vorauswahl der Oberfläche je Maschine
   38H, sofern frei (§5); Tor-Konflikte meldet der Bus wie bei der GIDE (`belegtGrund`).
8. **Nicht im Save-State** (wie GIDE/K5020) — Zustand nach Laden: Chip zurückgesetzt.
9. **Kein Tastenkürzel** für Ton an/aus (Kürzeltabelle des Handbuchs ist Vertrag).

### 4.2 Schnittstellen

- `K1520Machine::installSound(const SoundK1520::Config&)`, `soundK1520()`, `soundFehler()`,
  `soundEinketten()` (CTC hinten in die Interruptkette, wie `gunUsbEinketten`), `soundTakt`/`soundReset`.
- `SoundK1520::Config`: `basis` (38H), `chip` (8910|8912), `takt` (Quarz|QuarzHalbe|System),
  `quarz_hz` (3 579 545), `kanal` (ABC|ACB|Mono), `tiefpass` (bool), `kaskade` (Bits TO0→TR1 …).
- C-ABI (additiv): `k1520_sound_install(h, basis, chip, takt, quarz_hz, kanal, tiefpass, kaskade)`,
  `k1520_sound_fehler`, `k1520_sound_regs(h, uint8_t[16])`;
  `k1520_audio_start(h)/_stop`, `k1520_audio_lautstaerke(h, float)`, `k1520_audio_stumm(h, int)`,
  `k1520_audio_fehler`, `k1520_audio_echtzeit(h, int)` (setzt die Oberfläche aus dem Faktor des Emulationstakts, §4.1 Nr. 4),
  `k1520_audio_wav_start(h, pfad)/_stop`.  Dazu öffentlich `K1520Machine::nenntaktHz()` (heute nur protected `k6022TaktHz()`, Befund S0).
- Python: `K1520Emulator(sound={…})`, `emu.audio_*`; `test_c_api.py` gleicht die Deklarationen ab.
- Werkzeuge: `boot_trace`/`k1520dbg` `--sound[=basis]`, `--wav datei`; Befehl `ay` im Debugger
  (Register, Perioden → Hz, Mischer entschlüsselt, Hüllkurvenform, Portrichtung).

## 5. Maschinen

Alle mit `zusatzkartenSteckbar()` (A5120 inkl. .16, A5130, K8924, K8915 V3/V2, PRG 710/-1, PC 1715
mechanisch wie GIDE, MC80 prüfen); P8000 nicht.  Belegung je Maschine in S0 am Bus prüfen
(`ioBesitzer`); bekannt frei am A5120/K8924 (Unterlagen).  Interruptsäule ist Gastsache (CP/A: F7D8H).
**Vorauswahl der Oberfläche:** 38H, am **PC 1715 und PC 1715W E0H** (PC 1715: so die Gastsoftware; 1715W: 38H–3FH belegt, Befund S0), MC80.2x 38H (E0H dort belegt) — dort kollidiert
sie mit der Lochstreifenkarte K6022 (E0H–E7H fest); beide zugleich meldet der Kern als Konflikt, wie
GIDE/K5020 (gewollt, Basis von Hand ändern).

### 5a. Befund S0 (2026-11-14)

Methode: reine Code-Lektüre (nichts gebaut, nichts gelaufen).  Die Tabelle 1 ist aus den `registerIO`-Aufrufen
abgeleitet, nicht am laufenden Bus gemessen — **S4 prüft sie mit `ioBesitzer(38H..3FH)` je Maschine nach**
(Erweiterung von `KartenVorgaben.*`, `tests/integration/test_karten_vorgaben.cpp`; dort fehlen heute MC80).

**1. Tore.**  `zusatzkartenSteckbar()` ist `true` (`machine.h:162`) und wird nur vom P8000 (`p8000.h:199`) und vom
P8000-Terminal (`p8000_terminal_machine.h:87`) überschrieben → **alle MC80-Varianten sind steckbar**, P8000 lehnt ab.

| Maschine | belegt im Bereich 38H–3FH | belegt im Bereich E0H–E7H | Vorauswahl | Konflikt |
|----------|---------------------------|---------------------------|-----------|----------|
| A5120 (.16), A5130, K8924 | — (K2526 00–0F `k2526.cpp:140`, K5122 10–18 `a5120.cpp:209`, K8025 50–5F `a5120.cpp:206`, AUP 40–43/B0–B7, EM A8–B0, RAF 88/89, K5020 30–37 daneben) | nur Option K6022 | **38H** | keiner |
| K8915 V3 / Gen 2 | — (K5122 10–18, K7028 40–5F + 60–67 `k7028.cpp:157`, ZRE 80–83/A8–AB `zre8762.cpp:30`; Gen 2: K2521 80–87, K3528 A8–AB) | nur Option K6022 (K5020 liegt auf D0–D7, `k8915.h:243`) | **38H** | keiner |
| PRG 710 / 710-1 | — (K5122 10–18, K8025 50–5F, K2521 80–87, ASS 590069 C4–C7/CC–CF, ATP C0–C9, EPROMmer D0–D4, F4/F5; K5020 30–37) | nur Option K6022; **E8–EB = Seitenregister** (`prg710_speicher.h:42`, grenzt an) | **38H** | keiner |
| PC 1715 (K7222/K7221) | — (K5122 00–07 + 20–23, CTC 08–0B, SIO 0C–0F, CRT 18–1B, 24–33, BWS 34–37 `pc1715_zre.cpp:21–28,209–218`; K5020 D0–D7) | nur Option K6022 | **E0H** (38H wäre ebenfalls frei) | nur mit K6022 (gewollt) |
| PC 1715W | **38–3F belegt** durch `PC1715W-E/A` (Senke für /SR 38–3B, /RST 3C–3F, `pc1715.cpp:75`, Kommentar `:55–57`) | nur Option K6022 | **E0H** (38H geht nicht) | 38H → `belegtGrund` „PC1715W-E/A" |
| MC80.30 / .31 | — (ZVE 80–87/8C/8D `zve3x.cpp:34–36`, SPE B0–B3 `spe.cpp:58`, VIS C0–CF, ASP D0–DB; K5020 fest 30–37 `mc80.h:163`) | nur Option K6022 | **38H** | keiner |
| MC80.21 / .22 | — (Senke 00–0F `mc80_2x.h:191`, K2521 80–87, DPL F0–F7, MBI EF/F8–FF) | **E0–E8 = PPE-Senke** (`mc80_2x.h:192`) → auch K6022 nicht steckbar | **38H** | E0H wäre belegt, nicht wählen |
| P8000 (alle) | — | — | — | `installSound` lehnt ab (Grund wie bei GIDE) |

Folgen: (a) Die Vorauswahl der Oberfläche ist 38H, **am PC 1715 nur beim Modell PC 1715 E0H, am 1715W ebenfalls E0H**
(Profil `app/profil.py` Zeile ~716 hat zwei Modelle in einem Profil; für Sound ist E0H auf beiden richtig).  Gastsoftware gibt es nur für die
CP/A-Fassung des PC 1715, nicht für SCP 1715 und nicht für den 1715W.  (b) Der Kern meldet 38H am 1715W korrekt als belegt, weil die Senke
sich als echtes `BusDevice` anmeldet.  (c) Alle 8 Tore der Karte anmelden (8er-Raster, die Komparatorlogik
besetzt den ganzen Block; Basis+2/+3 lesen FFH), nicht nur 0/1/4–7 — `registerIO` kennt nur zusammenhängende Bereiche.

**Registrierung.**  `K1520Bus::registerIO(dev, basis, n)` (`k1520_bus.cpp:6`) wirft `std::runtime_error` bei Überlappung; `ioOwner(port)`
(`k1520_bus.h:298`) → `K1520Machine::ioBesitzer` (`machine.h:384`) → C-ABI `k1520_io_owner` (`k1520_api.h:1164`).  Üblich ist
`Karte::belegtGrund(bus)` vor dem Anmelden (Text „E/A-Tor %02XH belegt (%s)", Muster `gun_net.cpp:40`).  **Abweichung vom Header-Kommentar:**
der Bus reicht den **absoluten** Port an `ioRead/ioWrite` durch (`k1520_bus.cpp:160,177`), nicht den relativen — die Karte rechnet `port - basis`
selbst.  Eine Karte mit zwei Funktionsblöcken nimmt je Block ein eigenes `BusDevice` (Muster K5020: `tor_d_`/`tor_s_`, `k5020.cpp:26–27`).
Die Oberfläche hat keine Tortabelle: `app/kartenwahl.py::KartenArt` (Felder `schritt`, `tore_ab`, `tore_zahl`, `eigener_name`) und
`Programmprofil.karten_vorauswahl` — für Sound `schritt=8`, `tore_zahl=8`, `eigener_name` = `deviceName()` der Karte; die Vorauswahl braucht
die Modellabbildung (`*_basis_modell`) nur für den PC 1715 (alle Modelle E0H → eine Vorgabe je Profil genügt).

**2. Einbau-Muster und Laufschleifen.**  Vorbilder: GIDE/GunNet (`machine.h:334,448`: `installX` → Steckbarkeit, `bestueckbar_`, Doppelt-Prüfung, `belegtGrund`,
`attachToBus`, `powerOn`, ggf. `…Einketten`) plus K5020 für die CTC (`k5020Takt(int)` liefert `bool dirty`, `machine.h:529`).  Der C-ABI-Fehler
läuft über einen statischen String je Karte (`k1520_gide_install_ex`, `k1520_api.cpp:1700`; Handle = `K1520Machine*`, `toMachine`).
Zu ergänzen in `machine.h` (analog): `installSound(cfg)`, `soundK1520()`, `soundFehler()`, geschützt `virtual soundEinketten(…)` (Vorgabe
`appendInterruptChain(&k.ctc())`), `bool soundTakt(int used, uint64_t total)` (CTC `clockTick(used)` + absolute Takte für den AY → `true` = Interrupt
evtl. geändert), `soundReset()`, `soundPowerOn()`, Member `std::unique_ptr<…> sound_` **hinter** den anderen Karten (Zerstörungsreihenfolge) und `sound_fehler_`.
Aufrufstellen (Zeilen vor dem Eintrag der Karte; alle Maschinen außer P8000):

| Maschine | `…Takt` in `run()` | `…Reset` in `resetHardware()` | `…PowerOn` | Kette |
|----------|--------------------|-------------------------------|------------|-------|
| A5120 | `a5120.cpp:1032–1036` (Hauptpfad; **der ZVE2-DMA-Pfad `:933` tickt keine Karte**, ZRE-CTC auch nicht → kein neues Problem) | `:372–377` | `:408–410` | Basisvorgabe |
| K8915 | `k8915.cpp:229–232` | `:107–112` | `:134–136` | Basisvorgabe |
| PRG 710 | `prg710.cpp:276–279` | `:169–173` | `:191–193` | **`interruptKetteSetzen()` (`prg710.cpp:126–146`) um `sound_`-CTC ergänzen + `soundEinketten`-Override `prg710.h:199–202`** (die Kette wird dort bei jedem Stecken neu gebaut) |
| PC 1715 / 1715W | **zwei** Schleifen: `pc1715.cpp:450–453` (PC 1715) und `:519–522` (1715W) | `:267–270` | `:286–288` | Basisvorgabe (Kette `{afs, zre}` bzw. `{dma, ctc2, zre}`, Karte hinten) |
| MC80.3x | `mc80.cpp:161–164` | `:123–126` | `:99–101` | Basisvorgabe |
| MC80.2x | `mc80_2x.cpp:157–160` | `:125–128` | `:98–100` | Basisvorgabe |

Zeitstand: `gideTakt(total_cycles_)` wird **nach** dem Befehl gerufen, ein `OUT` sieht also den Stand des Vorgängerbefehls (Versatz ≤ ~23 Takte ≈ 9 µs,
für das „lazy catch-up" des AY unhörbar).  Ein Zeiger auf `total_cycles_` in der Config (wie `AUP::Config::uhr`, `aup.h:108`) bringt dieselbe Genauigkeit —
es bleibt beim GIDE-Muster (`soundTakt(used, total_cycles_)`).  Keine `AutoFlush`-Stelle nötig.

**3. Z80CTC (`core/primitives/z80_ctc.{h,cpp}`).**  Ein Objekt = 4 Kanäle an 4 aufeinanderfolgenden Toren, **relativer** Port 0–3 in `ioRead/ioWrite`
(die Karte reicht `port - basis - 4` durch).  Takt: `clockTick(int takte)` mit den verbrauchten Systemtakten (aufgeschoben/gebatcht, `z80_ctc.h:119`,
Rückgabe `true` = ZC/TO-Flanke möglich → `bus.markIntDirty()`); jeder `ioRead/ioWrite/clkTrg` rechnet vorher ab.  ZC/TO-Ausgänge: Rückruf
`setZCTOCallback(cb(kanal, pegel), kanalmaske)`; er kommt mit `false` bei der Flanke (`fireZCTO`, `z80_ctc.cpp:487`) und mit `true` im Folgetakt.  **Kaskade
TO0→TR1** wie am A5120 (`a5120.cpp:257–265`): im Rückruf `ctc.clkTrg(kanal+1, pegel)`; Zähler brauchen Steuerwort-Bit D6 = 1 und die gewünschte
Flanke (D4).  **Die Kanalmaske nur auf die verdrahteten Jumper setzen** (Bit n = TOn hat Abnehmer; ohne Jumper gar keinen Rückruf) — jeder maskierte
Kanal verlässt den schnellen arithmetischen Pfad (`zcto_beobachtet`, `z80_ctc.h`).  TR-Eingänge der Stiftleiste S7 (von außen) sind unbelegt
(kein Abnehmer; Kaskadenjumper reichen).  Interruptkette: `Z80CTC` ist selbst `InterruptSlave` (`setIEI/getIEO/hasInterrupt/getVector/onRETI/inService`) →
`bus.appendInterruptChain(&ctc)`; die Vektorbasis kommt vom Gast (Steuerwort D0 = 0 an Kanal 0, Gast: D8H).  Besonderheiten: `getIEO` = IEI ∧ ¬Anforderung ∧ ¬IUS
(`z80_ctc.cpp:586`, Fund SIOS 2026-10-30), RETI löscht IUS nur beim ersten Baustein in Bedienung (Bus `signalRETI`); `setIntAusVerwirftAnforderung`
bleibt **aus** (nur ASP des MC80 braucht es; ein Sound-Gast `AYPBC.INC` schaltet INT nicht im Betrieb ab); `reset()` bei `/RESET` (verwirft Verdrahtung nicht),
`setResetLaedtZeitkonstante` nur P8000.  Taktquelle ist **immer der Systemtakt** (kein eigener Quarz an der Karten-CTC, §3.3) — also kein `setzeEingangsTakt`.

**4. Takt, Nenntakt, Ausgabe.**  Der Kern kennt den Nenntakt nur geschützt und unter dem historischen Namen `k6022TaktHz()` (`machine.h:500`, Vorgabe 2 457 600;
PC 1715 `cpuHz()` = 2 458 000, 1715W 3 993 600; K8915/PRG/MC80 `CPU_HZ` = 2 457 600).  Für die Tonquelle **eine öffentliche Funktion `nenntaktHz()`**
(oder `soundTaktHz`) auf Basis dieses Werts anlegen — `installGide/GunNet` benutzen ihn schon.  Die absolute Taktzahl ist je Maschine ein
privates `total_cycles_`; die Karte bekommt sie aus `soundTakt`.  **Der Echtzeit-Faktor kennt nur Python**: `app/emulationstakt.py` (`set_tempo(hz, faktor)`,
`_lauf`, Scheiben von 2 ms Wanduhr ≈ 4900 Takte bei 1×, Schlaf bis 20 ms) ruft `k1520_run`; der Kern sieht nur `run(max_cycles)`.  Für §4.1 Nr. 4 braucht
es deshalb einen **neuen C-Aufruf** `k1520_audio_echtzeit(h, int)` (setzt die Oberfläche bei jedem `set_tempo`) — in §4.2 fehlt er.  Fäden im Kern gibt es als
Vorbild: `SerialHub::sicherstellenFaden` (`hub.cpp:362–368`, `join` im Destruktor `:99–106`), Schreibfaden der ATA-Platte (`ata_geraet.cpp:255`), Winchester
(`platte.cpp:134`), W5100 (`w5100.cpp:24`; deshalb `gun_net_` als **letztes** Member zerstört, `machine.h:570`).  `dlopen` kommt im Kern noch nicht vor
(nur `dladdr`, `${CMAKE_DL_LIBS}` ist an `k1520_floppy2` gebunden, `CMakeLists.txt:215`; `Threads::Threads` an `k1520_serial/ata/w5100/winchester`).
Natürlicher Ort der Tonquelle: ein Member in `K1520Machine` neben den Karten (`machine.h`, Handle = `K1520Machine*`, also auch die C-ABI ohne Zusatzobjekt),
**Ringpuffer und Ausgabefaden gehören nicht der Karte**, sondern einem eigenen Objekt, das vor der Karte stirbt (Faden zuerst anhalten).

**5. Beigaben (Vorlage `tools/gide/karten.py`).**  `RECHNER` (Schlüssel = Systemdiskette → `Rechner(titel, …)`), `dateien(schlüssel)` → `{Diskettenname: Bytes}`,
`cpm_text()` (LF→CRLF, ^Z, 128-B-Auffüllung), Textbausteine je Rechner, **`DISKETTEN`** (Schlüssel → Dateiname der Lieferdiskette) und `pruefen/main --check`
(liest jede Diskette mit `k1520disktool get <abbild> * --to <tmp>` und vergleicht bytegleich; Ausgabe `aktuell|FEHLT|OHNE|ALT`).  Bauskripte rufen
`karten.dateien(...)`: `tools/cpa_a5120/build.py:118`, `tools/cpa_pc1715/build.py:75`, `tools/scpx_a5120/build.py:40` (über `tools/diskbau.py::karten`, `:79`);
Pfade `tools/disketten_beigaben.py::diskpfad` + `ORDNER`.  Wächter: ctest `cli_karten_beigaben` (`tests/cli/CMakeLists.txt:132`, Labels `cli;fast`),
`KartenBeigaben.*` (`tests/integration/test_karten_beigaben.cpp`, bootet die Lieferdisketten mit den Karten), `py_karten_software` (`tests/python/test_karten_software.py`),
`py_karten_vorauswahl` (Tabelle gegen Profil); `tools/test_auswahl.py` hat für `core/cards/gide/**` einen Blatteintrag ohne `fach_`-Label.  Heutige `DISKETTEN`:
`a5120_cpa` = `a5120_cpa_k5601_system.hfe`, `a5120_scpx` = `a5120_scpx17_k5601_system.hfe`, `k8915_scpx`, `prg710_scpx`, `prg710_1_scpx`,
**`pc1715_cpa` = `disks/pc1715_cpa1715/pc1715_cpa1715_system.hfe` (die PC-1715-CP/A-Diskette)**, `pc1715_scp6/7`, `pc1715w_scp3`.  Für Sound gehören
rein: `a5120_cpa`, `a5120_scpx` (38H), `pc1715_cpa` (E0H, PC-1715-Fassungen); K8915/PRG nur über den USB-Player (S10).  Gesamtgröße je Satz ≈ 12 KB
(SKTEST4 5250 + STCTEST1 2270 + PLAYSTC 1678 + SONG.STC 1007 + TGCPM 2034 B); **freier Platz auf `pc1715_cpa1715_system.hfe` ist nicht geprüft** (der 1715W hat
nur 38 KB frei) — in S9a mit `k1520disktool` messen, bevor man die PC-1715-Fassungen einplant.

**Abweichungen/Lücken gegenüber dem Entwurf (Stand S0).**
1. §5: „MC80 prüfen" → **steckbar** (Vorgabe `true`); MC80.2x belegt E0–E8 durch die PPE-Senke, 38H ist frei.
2. §5: PC 1715**W** belegt 38–3F → dort **E0H**, nicht 38H (der Entwurf nennt nur „PC 1715 E0H"; 1715W ohne Gastsoftware, nur Basis wählbar).
3. §4.2 fehlt `k1520_audio_echtzeit` (nötig für §4.1 Nr. 4); der Nenntakt ist kein öffentlicher Kernwert (`nenntaktHz()` nachrüsten, A5120 `k6022TaktHz()` = 2 457 600,
   aber GUI-Profil `nenntakt_hz` = 2 450 000, `app/profil.py:581`, `app/takt.py:23` — 0,3 % Abweichung zwischen Erzeugung (Maschinenzeit) und Verbrauch
   (Wanduhr): von der Füllstandregelung (§4.1 Nr. 5) abgefangen, Tonhöhe ≈ 5 Cent daneben; besser `nenntakt_hz` des A5120-Profils auf 2 457 600 setzen oder die
   Regelung nach `nenntaktHz()` des Kerns richten).
4. §6 S6: das Debugger-Dispatch-Skript heißt `tests/cli/scripts/all_commands_smoke.dbg` (+ je Maschine `tests/cli/cases/dbg_*_all_commands_smoke.cli`), nicht `tests/dbg/…`.
5. Der Bus reicht absolute Ports durch (Header-Kommentar falsch), siehe oben — S3 muss `port - basis` rechnen.
6. PRG 710: die Interruptkette wird in `interruptKetteSetzen()` neu aufgebaut — ein bloßes `appendInterruptChain` im Basis-`soundEinketten` genügt dort **nicht**.
7. `KartenVorgaben.*` deckt nur A5120/K8915/PC 1715/PRG 710 ab (kein MC80, kein 1715W-Sonderfall) — S4 muss die Soundkarte dort und je einen Test für MC80.3x/.2x und 1715W (38H belegt) ergänzen.

## 6. Arbeitspakete (für Sonnet-5.5-Agenten)

Jedes AP: Agent `cpp-coder` bzw. allgemein mit Modell **sonnet**, liest vorher diesen Entwurf und die
genannten Merkposten; baut und testet nur über `tools/dev.sh`; Code ohne Umlaute; nach Abschluss
die Zeile „Stand:" oben in diesem Entwurf nachführen + Commit in den Zweig `sound` durch den Orchestrator (Agenten committen nicht selbst).  **Bauende APs nacheinander**
(gemeinsames `build/`), oder mit `isolation: "worktree"`.

| AP | Inhalt | hängt ab von | Abnahme / Wächter |
|----|--------|--------------|-------------------|
| **S0** | Bestandsaufnahme: Tor 38H…3FH je Maschine frei? (`ioBesitzer` in einem kleinen Test über alle Maschinen); wo `gideTakt`/`gideReset` in den Laufschleifen sitzen (Vorbild für `soundTakt`); CTC-API (`Z80CTC`: Takt, TRG-Eingänge, TO-Ausgänge, Kaskade); Ergebnis als §5a hier eintragen. Keine Kernänderung. | — | Tabelle Maschine → Basis-Vorauswahl |
| **S1** | **Chip `Ay38910`** (`core/primitives/ay38910.*`): Register/Latch/Lesemasken, Abwahl ≥ 16, Ports + Pull-ups + 8912, Ton/Rauschen/Hüllkurve auf dem Takt/8-Raster, Pegeltabelle, `rechneBis(takt)`, `abtastwerte(...)` mit Kastenfilter (§4.1 Nr. 2) – kanalweise Mono-Ausgänge A/B/C, Mischung erst in der Karte. | — | `tests/unit/primitives/test_ay38910.cpp`: Lesemasken aller 16 Register; Frequenz aus Nulldurchgängen (Periode 1, 0x6F, 0xFFF bei 1,79 MHz, ±0,5 %); Rauschfolge wiederholt sich nach 2¹⁷−1; alle 16 Hüllkurvenformen Stufe für Stufe gegen eine Tabelle aus dem Datenblatt; R13-Schreiben startet neu; Periode 0 = 1; Reset = still; Aliasing-Probe (hoher Ton → kein Spiegelton über −40 dB, einfache DFT im Test) |
| **S2** | **Tonstrecke** `core/audio/`: Ringpuffer (SPSC, sperrfrei), WAV-Schreiber, Ausgabe-Schnittstelle + `stumm` + `alsa` (dlopen `libasound.so.2`, 48 kHz S16 Stereo, eigener Faden, Füllstand-Regelung §4.1 Nr. 5) + `waveout` (Windows, nur bei `_WIN32` gebaut). Echtzeit-Schalter (§4.1 Nr. 4). | — (parallel zu S1 möglich, wenn eigenes Worktree) | Unit-Tests Ringpuffer (Über-/Unterlauf, Faden-Test), WAV-Kopf byteweise; `stumm` ohne Gerät; ALSA-Fehlerfall ohne Bibliothek (dlopen auf falschen Namen umgebogen) liefert Grund statt Absturz; `tools/dev.sh win` baut `waveout` |
| **S3** | **Karte `SoundK1520`** (`core/cards/sound_k1520/`): Dekodierung §3.1, AY + `Z80CTC` (Kaskaden-Jumper, CTC-Takt = Systemtakt), Mischer ABC/ACB/Mono mit Gewichten §3.4, Tiefpass, `takt()`, `reset()`, `belegtGrund`. | S1, S2 | `test_sound_k1520.cpp`: Tore +0…+7, +1 lesen = FFH, +2/+3 nicht belegt; Basis ≠ 8er → Ablehnung; R7-Rücklesen wie `AYPBC.INC`; CTC-Kanal 0 mit A7H/C0H gibt 50 Hz Interrupts (Takt 2,4576 MHz); Kaskade TO0→TR1 zählt; ABC vs. ACB vs. Mono: Ton nur auf A → nur links / beide |
| **S4** | **Einbau in die Maschinen**: `installSound` in `K1520Machine` (Muster GIDE: nur vor erstem Lauf, doppelt/Konflikt/Basis → Grund), `soundEinketten`, `soundTakt`/`soundReset` in allen Laufschleifen, Tonquelle an die Ausgabe der Maschine. | S3, S0 | `KartenVorgaben.*` um die Soundkarte erweitern (alle Karten zugleich, Konflikte je Maschine); Install-Fehlerfälle; P8000 lehnt ab |
| **S5** | **C-ABI + Python** (§4.2), `app/core_binding/k1520.py`, `K1520Emulator(sound=…)`. | S4 | `py_c_api` (mechanischer Abgleich), `py_binding`: Register schreiben/lesen über `io_write`, WAV-Mitschrift einer Sekunde 1-kHz-Ton → Frequenz per `wave` + Nulldurchgängen |
| **S6** | **Werkzeuge**: `--sound[=basis]`, `--wav` in `boot_trace` und `k1520dbg`, Befehl `ay`, `help`-Text, `tools/k1520dbg.md`/`tools/boot_trace.md`. | S4 | Dispatch-Smoke `tests/cli/scripts/all_commands_smoke.dbg` (+ `tests/cli/cases/dbg_*_all_commands_smoke.cli`) um `ay`; `cli_*`-Test: `boot_trace --sound --wav` erzeugt gültige WAV |
| **S7** | **Systemtest mit Gastsoftware** (Originale aus `tools/sound/fremd/original/`, entpackt in `TempDisk`-Kopien): A5120 + CP/A, `SKTEST4` Eingaben `B` (1,79 MHz), `1`, `A` → WAV ≈ 1 kHz nur links; Menü `4` (CTC-Interrupt) → 2 s nicht still, Interrupts ≈ 50/s; `STCTEST1` → Lied läuft, danach `DIR` am Prompt bedienbar; `PLAYSTC SONG.STC`; dasselbe unter SCPX; PC 1715 + CP/A mit den PC-1715-Fassungen an E0H. Label `fach_sound`, langsamer Lauf. | S5 | `SoundSystem.*` (ohne Skip — die Originale liegen im Repo) |
| **S8** | **Oberfläche**: *Einstellungen ▸ Allgemein ▸ Soundkarte* (aus / AY-3-8910 / AY-3-8912, Basis mit Konfliktanzeige wie GIDE, AY-Takt, Quarz, Kanal ABC/ACB/Mono, Tiefpass, Kaskade), Vorauswahl je Programmprofil (`app/profil.py`), *Maschine ▸ Ton* (an/stumm) + Lautstärke ohne Kürzel, Statuszeile „Ton: aus/stumm/hörbar/kein Gerät", `general.sound` in Konfiguration + Vorgabedateien (aus), Handbuch-Abschnitt; `k1520_audio_echtzeit` aus dem Faktor des Emulationstakts; **A5120-Nenntakt im Profil (`app/profil.py`, `app/takt.py`: 2 450 000) auf den Kernwert 2 457 600 angleichen** (sonst Tonhöhe ≈ 5 Cent daneben, Befund S0). | S5 | `py_sound_gui` (offscreen, Ausgabe `stumm`); Kürzel-Wächter bleiben grün; `py_keine_umlaute` |
| **S9a** | **Beigaben auf die Systemdisketten** nach dem Muster `tools/gide/karten.py` (eigenes `tools/sound/karten.py`, Wächter wie `KartenBeigaben.*`/`cli_karten_beigaben`): A5120/K8924 CP/A und SCPX bekommen `SKTEST4.COM` (neuerer Bau aus `rowikla_SKTEST4_BC/`), `STCTEST1.COM`, `PLAYSTC.COM`, `SONG.STC`, `TGCPM.COM` (CP/A-/SCPX-Fassung, 38H); PC-1715-CP/A-Diskette die PC-1715-Fassungen (E0H); Diskettenbau neu, `tools/sound/fremd/README.md` nachführen. | S7 | Wächter prüft je Diskette die Dateien bytegleich zu den Originalen |
| **S10** | **USB-Player** `CPMSTCPL`/`CPMPT3PL` 1.0.5 + `SNDINIT2` mit passender `SOUND.INI` je Rechner (A5120 `38 C0 D8F7 <USB-Basis>`, K8915 `38 C0 B0FF …`, PC 1715 `E0 C0 F0F7 …`; USB-Basis = Vorauswahl der GUN-USB im Profil) auf die Disketten; Systemtest: Stickordner mit `SONG.STC`, Player spielt ihn ab. Optional, wenn S0–S9a stehen. | S9a | `SoundUsbPlayer.*` |
| **S9** | **Abschluss**: Merkposten `doc/merkposten/sound.md` (Festlegungen §4.1 mit Wächtern), CLAUDE.md-Absatz, `tools/test_auswahl.py` Label `fach_sound` für die neuen Pfade, Paket prüfen (keine neue Python-Abhängigkeit; ALSA-Ausfall im Paket dokumentieren), `tools/dev.sh test-merge`, `tools/dev.sh win`. | alle | grüner `test-merge` + `win` |

Reihenfolge: S0 ∥ S1 ∥ S2 (S1/S2 nur mit Worktree parallel) → S3 → S4 → S5 → S6 ∥ S8 → S7 → S9a → (S10) → S9.
Der Orchestrator (Opus) prüft nach S1 und S2 die Abnahmen selbst (Klang hören am Entwicklungsrechner:
`SKTEST4` Ton A/B/C und Lied), bevor S3 beginnt; nach S7 vergleicht **der Anwender** `STCTEST1` im Emulator mit der Aufnahme
`STCTEST1.mp3` der echten Karte (Tempo, Tonhöhe, Lautstärkeverhältnisse); dafür eine WAV-Mitschrift
`out/sound/stctest1_emu.wav` bereitstellen.

## 7. Fragen an den Anwender (Stand 2026-11-14: beantwortet)

| Nr. | Frage | Antwort |
|-----|-------|---------|
| F1 | Gastsoftware ins Repo und auf die Systemdisketten? | **ja** (2026-11-14) → `tools/sound/fremd/`, AP S9a |
| F2 | Welche Demos? | `STCTEST1` (Hintergrundlied) und `PLAYSTC` + `SONG.STC`, dazu `SKTEST4` und `TGCPM` als Prüfprogramme; USB-Player als S10 |
| F3 | Schaltplan V2 auswerten? | nur bei Unklarheiten.  Bisher keine: das Rücklesen an Basis+0 ist durch die Gastsoftware belegt, der Rest steht in der Beschreibung V2 |
| F4 | Kanalzuordnung? | **Stereo ABC** als Vorgabe (echtes Stereo, B in der Mitte), ACB/Mono wählbar |

## 8. Risiken

- **ALSA unter PipeWire/Pulse**: hängt am ALSA-Kompatibilitätsplugin des Wirts (Debian 13: Standard).  Fehlt
  es, bleibt der Ton stumm mit Grund.  Rückfallweg nur auf Anwenderentscheid: miniaudio (Einzeldatei wie
  miniz) — widerspricht dem Grundsatz „keine Fremdsoftware", deshalb nicht geplant.
- **Latenz/Knacken** bei schwankendem Emulationsfaden (Scheiben von 20 ms): Füllstand-Regelung und Ziel
  60 ms; zu prüfen am langsamsten Profil (P8000 nicht betroffen, keine Karte).
- **Pegeltabelle**: Messungen verschiedener Quellen weichen ab; Klang kann von der echten Karte leicht
  abweichen — Vergleich mit `STCTEST1.mp3` (Aufnahme der echten Karte, im Download enthalten).
- **Kosten im Emulationsfaden**: Takt/8-Raster bei 1,79 MHz = 224 000 Schritte/s; bei 10× bzw.
  unbegrenztem Tempo ohne Hörer und ohne WAV keine Abtastwerte erzeugen (nur Register nachführen).

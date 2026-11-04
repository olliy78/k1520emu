# Entwurf 46 — 5,25″-HD-Laufwerk am A5120 (TEAC FD-55GFR an der K5122)

Stand 2026-11-04, **vorgemerkt, nicht begonnen**.  Anlass: der Anwender besitzt ein TEAC FD-55GFR-618 und möchte
es am A5120 als HD-Laufwerk (1,2 MB, 500 kbit/s, 360 U/min) betreiben — im Emulator und am echten Gerät.

## 1  Befund

* **Die K5122 kann 500 kbit/s.**  Steuer-PIO Tor B Bit 2 = `/HF`: 0 = 1 MHz Schreibtakt (8″-MFM, MF 6400),
  1 = halbiert (8″-FM, 5,25″-MFM).  `/HF` geht **nicht** auf den Laufwerksstecker X3 (Handbuch §5.8), es wirkt nur
  auf Schreibtakt und PLL der Karte.
* **Physik:** 5,25″-HD (500 kbit/s, 360 U/min, 10 416 B je Spur) ist für die Karte dasselbe wie 8″-MFM am MF 6400.
  Ein HD-Laufwerk ist aus ihrer Sicht ein MF 6400 mit 80 Zylindern, zweiseitig, 96 tpi.
* **Emulator, Stand main `9f39f7ce`:** die Byteperiode folgt `/HF` (`K5122::currentBytePeriod`, 39 Takte bei
  `/HF = 0`, `07_k5122_afs.md`).  HD-Abbilder gibt es in allen Containern (Entwurf 45).  `FloppyDriveV2::mount`
  weist ein 500-kbit/s-Medium ab, außer Laufwerk **und** Medium laufen mit 360 U/min (`floppy_drive2.cpp:77`).
  Kein `DriveProfile` ist HD-fähig; `rpm` ist fest je Profil.
* **Gast:**
  * Lade-ROM setzt `/HF = 1` fest (`OUT (12H),7FH`, 00DD) → **Booten nur von DD**.
  * CP/A-BIOS (`biosdskt.mac`): der Frequenzcode `dfrcod` kommt aus der Tabelle `mdmfm8` (00H) / `mdmfm5` (04H) /
    `mdfm8` (04H), gewählt nach `ft.kom` Bit 3 (MFM) und Bit 4 (5″).  Diese Bits stammen aus dem **DPB-Typbyte**
    (`biosdsk.mac` ~1005: `dpbt80`, `dpbt5z`, `dpbtdd`).  Ein DPB „8″-Takt, MFM, 80 Spuren, zweiseitig" ergäbe also
    `/HF = 0` — ob das BIOS diese Kombination (8″-Zweig + 80 Zylinder + Rückseite) durchgängig trägt, ist **ungeprüft**.
  * `mdmfm8` schaltet die Vorkompensation nie ein (`dpretr = 255`), `mdmfm5` ab Spur 25.  Ein 5,25″-HD-Laufwerk
    braucht sie innen (üblich ab Zyl. 43 von 80).
  * FORMAT.COM kennt im MF6400-Menü nur 77 Zylinder einseitig.  Formatiert werden kann über das DiskTool
    (Emulator) bzw. den Greaseweazle (Gerät).
  * Baukette für das BIOS: `~/projects/CPA/CPAA5120` (Makefile, Kconfig, zxcc).

## 2  Festlegungen

* **Ein neues Laufwerksprofil `FD-55GFR`**, kein Umbau eines bestehenden: 5,25″, 80 × 2, 96 tpi, MFM (+FM),
  **zwei Betriebsarten**: DD = 300 U/min / 250 kbit/s, HD = 360 U/min / 500 kbit/s.
* **Die Betriebsart folgt `/HF`** — Nachbildung der Verdrahtung „Density Select (Pin 2) ← `/HF`" (Vorschlag 2 aus der
  Besprechung vom 2026-11-04).  Offener Pin bzw. RESET = 1 = DD.  Am Profil ein Schalter
  `densel_an_hf` (Vorgabe an); aus = Laufwerk fest per Brücke auf HD gestellt.
* **Unveränderte Systeme laufen im DD-Betrieb wie mit einem K5601** — `/HF = 1` kommt vom ROM und von jedem BIOS,
  das 5,25″-MFM fährt.  Das ist der wichtigste Wächter.
* **Die Drehzahl bestimmt die Index-Periode, die Rate die Byteperiode — beides aus EINER Stelle**
  (`FloppyDriveV2` liefert die wirksame Drehzahl, `K5122::currentBytePeriod` die Rate).  Das Umschalten der Drehzahl
  bekommt eine Einschwingzeit als Profilparameter (Vorgabe 0 = sofort, bis gemessen).
* **Mischdisketten (DD-Bootspuren + HD-Datenbereich) sind Etappe 2** und nur nach einem Gerätetest der Medien
  (§4, HL6) — DD-Aufzeichnung mit DD-Schreibstrom auf HD-Medium ist erfahrungsgemäß schwach.
* Der 250-kbit/s-Pfad und die Boot-Invarianten bleiben bitgleich.  `/WAIT`-Maschinen (K8915, PRG 710, PC 1715)
  bleiben außen vor.

## 3  Arbeitspakete Etappe 1 — HD als Datenlaufwerk

| AP | Inhalt | Wächter / Abnahme |
|---|---|---|
| **HL1** Kern: Profil | `DriveProfile` um `rpm_hd`, `hd_faehig`, `densel_an_hf`, `umschaltzeit_ms` erweitern; Profil `FD-55GFR` in `drive_profile.cpp` + `knownDriveProfileNames()`; Klassen-Doku-Tabelle nachziehen | `DriveProfile.FD55GFR_*`; `knownDriveProfileNames` im Formatkatalog-Test |
| **HL2** Kern: Betriebsart | K5122 meldet `/HF` an das selektierte Laufwerk (`FloppyDriveV2::setHochdichte(bool)`, beim Schreiben von Port 12H/13H und beim Selektieren).  Wirksame `rpm` → Index-Periode; Einschwingzeit; Save-State trägt die Betriebsart (folgt aus dem PIO-Zustand, beim Laden neu ableiten) | `K5122Test.HdLaufwerk_DrehzahlFolgtHF` (Index-Periode 490 000 ↔ 408 333 Takte), `…_ResetIstDD` |
| **HL3** Kern: Einlegen | Mount-Regel: 500-kbit/s-Medium an `hd_faehig` erlauben; DD-Medium bleibt erlaubt.  Liest das Laufwerk in falscher Betriebsart, kommt — wie am Gerät — nichts Lesbares (kein Sonderfehler).  `notices()`: „HD-Diskette: lesbar nur bei /HF = 0" | `FloppyDriveV2.HdMediumImHdLaufwerk`, bestehende Abweisung für K5601 bleibt (`…HdImK5601Abgewiesen`) |
| **HL4** Katalog + Oberfläche | `data/formats.yaml`: Laufwerk `FD-55GFR` bei den DD-Formaten des K5601 und bei `dos1200`; neues CP/A-Format **`cpa_hd`** (80 × 2 × 8 × 1024 MFM, 500 kbit/s/360, Systemspuren wie `cpa800`, DPB mit 8″-Typbit, Kapazität ≈ 1,25 MB) samt Dateisystemprofil; Laufwerkswahl in `a5120emu` (Formatdialog `fuer_emulator` zeigt HD nur für HD-fähige Laufwerke); C-ABI unverändert (Profil per Name) | `py_c_api`, Formatdialog-Test, `DiskVolume` öffnet `cpa_hd` |
| **HL5** Gast: CP/A-BIOS | Analyse: trägt der `mdmfm8`-Zweig 80 Zylinder + Rückseite + Vorkompensation?  Dann BIOS-Variante (Kconfig-Option „LW-Typ 5,25″ HD") in `~/projects/CPA/CPAA5120` mit DPB `cpa_hd` und `dpretr = 43`; Combo-Bootdiskette `disks/a5120_cpa/…_hd.hfe` (A: K5601, B: FD-55GFR) | System-Test: Kaltstart von A: (DD), `DIR B:`, `PIP` 1 MB auf B: und zurück, Prüfsumme; danach `DIR A:` → `/HF` wieder 1, A: lesbar |
| **HL6** Doku | `07_k5122_afs.md` §10.6 abhaken, `09_floppy_drive.md` (Betriebsart), Handbuch (Laufwerkswahl, „HD nur als Datenlaufwerk"), Merkposten | — |

Reihenfolge HL1 → HL2 → HL3 → HL4 → HL5 → HL6; HL5 kann nach HL3 parallel analysiert werden.
Vor dem Merge `tools/dev.sh test-merge` (berührt `core/cards/k5122` und `floppy_drive` → Boot- und Formatspuren).

## 4  Etappe 2 — Mischdiskette (DD-Systemspuren, HD-Datenbereich), nur nach Gerätetest

| AP | Inhalt |
|---|---|
| **HL7** Gerätetest [Anwender] | Am FD-55GFR mit Greaseweazle: HD-Medium, Zyl. 0–1 in DD (300 U/min, 250 kbit/s) schreiben, Rest HD; mehrfach zurücklesen (Lesereserve), auch in einem K5601.  Ergebnis entscheidet, ob Etappe 2 weitergeht. |
| **HL8** Medium | Spurweise Rate **und** Drehzahl im `DiskMedium` (`TrackImage` je Spur `rate_kbps`/`rpm`).  Container: HFE v1 und DMK tragen das nicht (feste Zellenzahl je Datei bzw. feste Spurlänge) → eigener Weg (HFE v3 mit Spurrate prüfen, sonst nur im Save-State/DiskTool-Projekt). |
| **HL9** Gast | Bootsystem und BIOS schalten spurabhängig: Systemspuren `/HF = 1`, Datenbereich `/HF = 0`, Wartezeit nach dem Drehzahlwechsel.  Alle Systemspuren müssen DD sein, weil der Zweitlader und das Bootsystem davon lesen und `@OS.COM` schon aus dem Datenbereich kommt. |

## 5  Offene Fragen [Anwender]

1. **Brücken des FD-55GFR-618:** gibt es den Zwei-Drehzahl-Betrieb (DD 300 / HD 360 über Density Select)?  Ohne ihn
   liefen DD-Disketten mit 300 kbit/s ein — die K5122 kann nur 250/500 (10-MHz-Quarz, nur halbiert) → dann nur
   Betrieb „fest HD" (`densel_an_hf` aus).
2. **Polarität von Pin 2** (Brücke) — muss „/HF = 0 ⇒ HD" ergeben.
3. **Verdrahtung:** PIO-Pin B2 (A1.2) über einen Open-Collector-Treiber mit Pull-up auf eine freie Ader, **nur zu
   diesem Laufwerk** (nicht auf den Bus — Pin 2 anderer Laufwerke ist unbelegt oder anders belegt).
4. **Umschaltzeit der Drehzahl** laut Datenblatt (Parameter `umschaltzeit_ms`).
5. **Vorkompensation:** reicht die 8″-Schaltung der K5122 (ab welcher Spur) für 96 tpi HD?  Nur am Gerät messbar.

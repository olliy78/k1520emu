# Entwurf 45 — HD-Disketten (500 kbit/s) im DiskTool

Stand 2026-11-03, Zweig `DiskTool_HD` (von `DiskTool_DCP_FAT12`).  Anlass: DOS- und CP/A-Software liegt oft auf
HD-Disketten (5,25″ 1,2M, 3,5″ 1,44M); EC 1834 und A7150 wurden häufig auf HD-Laufwerke umgebaut.  Ziel: HD-Abbilder in
**allen Containern** (`.hfe`, `.dmk`, `.imd`, `.td0`, `.cqm`, `.img`) und am **echten Laufwerk** (Greaseweazle) lesen und
schreiben.  **ED (2,88M, 1 Mbit/s) ist ausdrücklich nicht Ziel**; `dos2880` bleibt `.img`-only.

## 1  Befund: der Stack kann längere Umdrehungen schon

Das Modell rechnet in Byte-Zellen ohne Zeitachse; die Datenrate steckt nur in der **Länge einer Umdrehung**.  8″ DD (MF6400,
MFM 500 kbit/s, 360 U/min) läuft schon heute: `TrackCodec::nominalTrackBytes` kennt 10 416 Byte, `HfeCodec::save` schreibt
166 667 Zellen, `DiskMedium::nominalRpm() == 360`.  Physikalisch sind die beiden HD-Formate:

| Format | Rate | U/min | Byte je Umdrehung (MFM) | Zellen (HFE) | gleiche Physik wie |
|---|---|---|---|---|---|
| 5,25″ HD 1,2M (15×512) | 500 | 360 | 10 416 | 166 667 | 8″ DD (MF6400) |
| 3,5″ HD 1,44M (18×512) | 500 | 300 | **12 500** | **200 000** | — neu |

Was fehlt:
1. **Rate als Angabe** am Format und am Medium (heute nur implizit aus `rpm` und „8″ = 500").  `HfeCodec::save` schreibt
   immer `bitrate 250` in den Kopf (`gw write` streckt nach der Zellenzahl, darum ging 8″ trotzdem — ein Gotek/HxC liest
   so eine Datei aber falsch).
2. **12 500 Byte** als Umdrehung (3,5″ HD) in `nominalTrackBytes`; heute fällt 18×512 auf 10 416 zurück und läuft über
   den Index.
3. **HFE lesen:** `bitrate ≥ 375` gilt heute als **Überabtastung** einer DD-Diskette (Faktor 2).  Eine echte HD-Datei trägt
   ebenfalls 500.  Entschieden wird je Spur am Inhalt (gültige CRCs) — Faktor 1 muss bei HD gewinnen.  Ausgerechnet hier
   hängen reale Mitschnitte (falsche Kopfangaben, `doc/merkposten/disktool.md` SCP1700): **nicht vereinfachen**.
4. **Containerköpfe:** IMD-Modus/TD0-Rate/CQM-Dichte aus der Rate des Mediums statt fest 250.
5. **Greaseweazle:** Rate/Drehzahl folgen dem gewählten Format (heute `--rate`/`--rpm` von Hand in `physical_cli`),
   HD-Laufwerk (Dichteumschaltung `densel`).
6. **Emulator:** kein Robotron-Laufwerk kann HD.  Ein HD-Medium wird beim Einlegen mit Klartext abgewiesen (oder gar nicht
   angeboten), statt still mit falscher Zeitbasis zu laufen.

## 2  Festlegungen

* **Rate und Drehzahl sind Eigenschaften des Formats** (`data/formats.yaml`: `rate: 500`, `rpm: 300|360` auf Formatebene;
  Vorgabe 250/300 bzw. aus den Laufwerken wie heute).  `DiskMedium` führt `nominalRateKbps()` neben `nominalRpm()`;
  `TrackImage::cell_factor` bleibt ein Teiler relativ zur **Grundrate des Mediums** (keine Bruchfaktoren).
* **Der 250-kbit/s-Pfad bleibt bitgleich.**  Jede bestehende `.hfe`/`.dmk` liest und schreibt Byte für Byte wie vorher;
  die Boot-Invarianten (CLAUDE.md), Lücke 2 = 22×4E, „Zellenzahl = Umdrehung" bleiben.  Wächter: die vorhandenen Tests +
  `test-format` + `test-matrix`.
* **8″ DD bleibt, wie es ist** (rpm 360, Rate 500 implizit), bekommt aber im HFE-Kopf nun die wahre Rate 500 —
  **nur wenn** das nachweislich am bestehenden Wächter `HfeCodec.*`/8″-Boot nichts ändert; sonst unverändert lassen und
  im Merkposten vermerken.
* Neue Formate: `pc_hd_15x512` (rate 500, rpm 360), `pc_hd_18x512` (rate 500, rpm 300); CP/A auf HD nur, wenn ein
  Format belegt ist (offen, s. §4).

**Umsetzung H1 (2026-11-03):** Abweichungen/Präzisierungen gegenüber §2:
* **8″ behält Rate 250 im Medium** (HFE-Kopf, Zellen 166 667 wie bisher, byteweise gleich).  Rate 500 am Medium haben nur HD-PC-Formate und
  eine geladene HFE mit Kopf ~500 + Faktor 1 + Umdrehung ≥ 140 000 Zellen (das kann auch eine externe 8″-Aufnahme sein).
* **Emulator-Abweisung** in `FloppyDriveV2::mount`: Rate 500 wird abgewiesen, außer Laufwerk UND Medium laufen mit 360 U/min (MF6400).  Ein HD-15×512
  (360 U/min) wäre damit im 8″-Laufwerk zulässig (physikalisch dieselbe Zeitbasis); 1,44M (300 U/min) nie.
* **IMD/TD0:** Rate-500-Spuren mit 15/18 × 512 MFM machen das Medium zu HD (`SectorTrackIo::toMedium`); sonst (8″) unverändert.  CQM: Dichte-Byte 1 + 15/18 × 512.
  DMK führt keine Rate; Erkennung am Inhalt.  `.img`: aus dem Format.
* Neue Hilfen: `DiskMedium::nominalRateKbps/detectHighDensityFromContent`, `nominalRpmForFormat`, `TrackCodec::buildTrack(.., rate)`.

## 3  Arbeitspakete

| AP | Inhalt | Wächter |
|---|---|---|
| **H1** Kern | Rate/rpm im Katalog + `DiskMedium`; `nominalTrackBytes` 12 500; HFE schreiben (Kopf-Bitrate/rpm, Zellen aus Rate×Zeit) und lesen (echtes HD vs. überabgetastetes DD); DMK-Spurlänge; IMD/TD0/CQM-Rate; `nur_img` für dos1200/dos1440 aufheben; Emulator weist HD-Medium ab | neue `HfeCodec.Hd*`, `DmkCodec.Hd*`, `ImdCodec.Hd*`, `FatDiskVolume.Hd*`; alle bestehenden grün |
| **H2** Greaseweazle | Rate/rpm aus dem Format an `TrackSync`/`app/gw`; HD-Laufwerk/`densel`; `physical_cli` ohne Handangaben; Oberfläche (physisch öffnen) | Python-Tests ohne Hardware; `tests/hardware/` Entwurf |
| **H3** Gerät | Anwender liest/schreibt eine echte HD-Diskette (1,44M und/oder 1,2M) | Messprotokoll |

## 4  Offen

* Welche CP/A-HD-Formate es gab (Katalogeinträge erst mit Beleg).
* Welches HD-Laufwerk der Anwender hat (3,5″ 1,44M oder 5,25″ 1,2M) — bestimmt H3.

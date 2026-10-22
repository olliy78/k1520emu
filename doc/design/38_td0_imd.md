# Entwurf 38 — TeleDisk (`.td0`) und ImageDisk (`.imd`) als Container (#9)

Stand 2026-10-22, Zweig `feature/issue-9-td0-imd`.  Ticket GitHub #9: „DiskTool **und** Emulator sollen
zusätzlich `.cqm`, `.td0` und `.imd` unterstützen (lesend und schreibend)“.
Vorlagen: `doc/design/35_cqm.md` (Container-Codec mit Schreiben), `doc/design/09_floppy_drive.md` §4,
`doc/merkposten/cqm.md`.

## 0. Ausgangslage

* **`.cqm` ist erledigt** (Entwurf 35: Lesen, Schreiben, DiskTool, Emulator, C-ABI).  Offen sind nur
  `.td0` und `.imd`.
* Container sind `ImageCodec`-Fassaden vor dem `DiskMedium` (`image_codec.{h,cpp}`); je Format ein
  `*_codec.{h,cpp}`, eine `SourceInfo` für Kopfdaten, Verdrahtung in `DiskImage`, `FloppyDriveV2`,
  `Laufwerke`, `DiskVolume`, `FsProfile::allowsContainer`, C-ABI (`k1520_*`, `k1520d_*`),
  `k1520disktool`, Python (`app/core_binding`, DiskTool-/Emulator-Dialoge, `profil.py`).
* Lokal liegen **67 echte `.td0`** (`~/projects/robotron/{A5120,PC1715,cm1910}`, `~/projects/A5120.16/docs/tiffe/MUTOS`,
  darunter 1× `td` = Advanced Compression, 1- und 2-seitig, FM-Spuren), aber **kein einziges `.imd`**.

## 1. Recherche — reichen die Unterlagen?

| Format | Quellen (gelesen) | Urteil |
|--------|-------------------|--------|
| **IMD** | MAME `imd_dsk.cpp` (BSD-3), SIMH `sim_imd.h`, Dunfield-Beschreibung (Server beim Recherchieren nicht erreichbar, Inhalt über die Sekundärquellen belegt) | **Ausreichend.** Das Format ist klein und vollständig beschrieben (§2.1). Offen nur Kleinigkeiten (§2.1 „Zu prüfen“). |
| **TD0** | MAME `td0_dsk.cpp` (BSD-3, enthält den LZSS-+-adaptive-Huffman-Dekoder nach Okumura/Yoshizaki), 67 Echtdateien als Prüfstand | **Ausreichend für Lesen; für Schreiben mit Vorbehalt.** Kopf, Spur, Sektor, Methoden 0/1/2 und die „td“-Kompression sind beschrieben; MAME ist an einer Stelle selbst widersprüchlich (Methode 2, §2.2) und prüft keine CRC. Die Lücken schließt der **Korpus**: Dekodieren muss für alle 67 Dateien lückenlos auf `128<<N` Bytes je Sektor aufgehen, und die CRC-Felder (Kopf, Spur, Sektor-Low-Byte) müssen stimmen. |

Die TD0-Spezifikation von Sydex war nie offiziell; alles ist rückentwickelt (Wayne Warthen/Dunfield-Notizen,
MAME, libdsk, SAMdisk).  **Wir übernehmen keinen Fremdcode**, sondern schreiben nach der Formatbeschreibung
neu; der Huffman-/LZSS-Dekoder wird nach dem (freien) Okumura-LZHUF-Algorithmus geschrieben, MAME dient nur als
Gegenprobe.  Quellen werden im Merkposten genannt.

## 2. Formate (Arbeitsstand, im Korpus zu belegen)

### 2.1 IMD

```
"IMD v.vv: DD/MM/YYYY HH:MM:SS"  Kommentar (frei, CR LF)  0x1A
dann je Spur bis Dateiende:
  u8 Mode   0..2 = FM 500/300/250 kbit/s, 3..5 = MFM 500/300/250
  u8 Zylinder
  u8 Kopf   Bits 0..5 Kopf, 0x40 = Kopf-Map folgt, 0x80 = Zylinder-Map folgt
  u8 Sektoren je Spur (0 = leere Spur)
  u8 Sektorgrößen-Code N (128<<N; 0xFF = Größen-Tabelle? -> zu prüfen, 6/7 = 8192/16384)
  u8[nsec] Sektor-Numerierungs-Map (Reihenfolge = Reihenfolge auf der Spur; Interleave steckt hier drin)
  u8[nsec] Zylinder-Map (nur bei 0x80)      u8[nsec] Kopf-Map (nur bei 0x40)
  je Sektor: u8 Typ, dann Daten
    0 = keine Daten   1 = normal (N Bytes)   2 = komprimiert (1 Füllbyte)
    3 = gelöscht (DAM F8)   4 = gelöscht+komprimiert
    5 = Datenfehler (CRC)   6 = Fehler+komprimiert   7 = gelöscht+Fehler   8 = gelöscht+Fehler+komprimiert
```

Zu prüfen (AP-I1): Größen-Code 0xFF/Mischgrößen je Spur (IMD kann das nicht — Spuren mit gemischten
Sektorgrößen müssen beim Schreiben abgelehnt werden, wie MAME), Verhalten bei `nsec = 0`, Mode-Bytes ≥ 6.

### 2.2 TD0

```
Kopf 12 B: 'TD'|'td'(Advanced Compression)  [2] Volume-Folge  [3] Prüffolge  [4] Version (21 = 2.1)
           [5] Datenrate (Bit 0..6: 0=250,1=300,2=500 kbit/s; Bit 7 = FM)  [6] Laufwerkstyp
           [7] Bit 7 = Kommentarblock, Bit 0..6 Schrittweite  [8] DOS-Allocation-Flag  [9] Seiten
           [10..11] CRC-16 über Byte 0..9 (Poly 0xA097, Start 0)
ab hier bei 'td' LZSS+adaptives Huffman (N=4096, F=60, THRESHOLD=2, MSB-first), Kopf bleibt roh
Kommentarblock (wenn [7]&0x80): u16 CRC, u16 Länge, Jahr-1900,Mon,Tag,Std,Min,Sek, dann Text (NUL-getrennte Zeilen)
je Spur: u8 nsec (0xFF = Ende), u8 Zyl, u8 Kopf (Bit0 Kopf, Bit7 FM), u8 CRC-Low-Byte über die 3 Bytes
  je Sektor: u8 C, u8 H, u8 R, u8 N, u8 Flags, u8 CRC-Low-Byte über die ENTPACKTEN Daten
    Flags: 0x01 doppelt, 0x02 CRC-Fehler im Datenfeld, 0x04 gelöschte DAM, 0x10 von DOS übersprungen, 0x20 nur ID (keine Daten)
    (0x10|0x20 gesetzt: kein Datenblock)  sonst: u16 Länge, u8 Methode, Nutzlast
    Methode 0 roh | 1 u16 Zähler + 2-Byte-Muster | 2 RLE: Paare (k, n): k=0 → n Rohbytes,
            sonst Muster der Länge 2^k?/2k? (MAME unterscheidet in zwei Funktionen!) n-mal
```

**Belegpflicht AP-T1:** die Länge des Methode-2-Musters (`1<<k` oder `2*k`) wird am Korpus entschieden
(nur eine Lesart geht für alle Sektoren lückenlos auf `128<<N` Bytes auf); ebenso, ob das Sektor-CRC-Byte
über die entpackten Daten läuft.  Das Ergebnis kommt in den Merkposten.

## 3. Sinnvoll? Passt es ins Konzept?

**Ja, beides.**

* Beide sind **sektorbasierte Container** wie `.img`/`.cqm` — Geometrie steht in der Datei, die Spur wird über
  `TrackCodec::buildTrack` synthetisiert.  Das passt unverändert zur Schicht `ImageCodec` ⇄ `DiskMedium`.
* **Mehr als `.img`/`.cqm`:** TD0/IMD tragen **gemischte Geometrie** (z. B. CP/A-Systemspuren 128 B + Datenspuren),
  **FM- und MFM-Spuren**, **gelöschte Datenmarken**, **CRC-Fehler** und **fehlende Sektoren**.  Damit lassen
  sich Disketten mit Kopierschutz-/Fehlerstellen ablegen, was `.img` nie kann.  Das ist der eigentliche Gewinn.
* **Grenzen sind dieselben wie bei `.img`:** was hinter der Daten-CRC steht (UDOS-Sektorkontrollblock), geht in
  keinem der beiden Formate mit — `rawCompatible()` bleibt die Schranke fürs Schreiben;
  `FsProfile::allowsContainer("td0"|"imd")` = `allow_img`.
* **TD0 ist für uns vor allem ein Lese-Format** (die Sammlung der Anwender besteht aus 67 TD0, MUTOS/SCP/CP/A),
  IMD ist das gängige Austauschformat der Gegenrichtung.  Das Ticket verlangt beides lesend **und** schreibend; wir
  liefern es, mit einer bewussten Einschränkung:
  **TD0 wird unkomprimiert (`TD`) geschrieben.**  Teledisk liest beide, Advanced Compression bringt uns nur Dateigröße,
  und ein LZHUF-*Kodierer* ist die zusätzliche Fehlerquelle ohne Gegenstelle zum Prüfen (Teledisk läuft nicht unter Linux).
  Eine `td`-Datei wird gelesen und beim Speichern als `TD` zurückgeschrieben (inhaltsgleich, größer).  Optionaler
  Kodierer: AP-T4, ausdrücklich **nicht** empfohlen.
* **Risiko:** keine `.imd` und kein Teledisk zum Gegenprüfen.  Abfederung: (1) TD0-Korpus als Echtdaten-Prüfstand,
  (2) unabhängig geschriebener Python-Leser/-Schreiber in `tests/python/` (wie `cqm_bau.py`), (3) Rundlauf
  TD0 → Medium → IMD → Medium über alle 67 Abbilder, Sektordaten und Marken gleich, (4) Ablehnung statt Raten.

## 4. Festlegungen (nicht neu aufrollen)

1. **Eine gemeinsame Zwischenschicht** `sector_track_io.{h,cpp}` (nur Kern, keine Dateiformate): Liste
   `SectorRecord` je Spur (C, H, R, N, Flags *gelöscht / Datenfehler / ID-Fehler / ohne Daten*, Daten) und Kodierung FM/MFM +
   Datenrate ⇄ `DiskMedium`.  IMD und TD0 sind dünne Parser/Schreiber darüber.  `TrackCodec::buildTrack` bekommt dafür die
   Fähigkeiten, die es nicht hat: **gelöschte DAM (F8), absichtlich falsche Daten-/ID-CRC, Sektor ohne Datenfeld**
   (Erweiterung des `LogicalSector` ist schon da — `deleted`, `*_crc_ok`; `buildTrack` wertet sie noch nicht aus).
2. **Streng beim Lesen, ablehnend beim Schreiben**, wie CQM: falsche Kennung, abgeschnittene Datei, Prüfsummenfehler in
   Kopf/Spur (TD0) → Laden scheitert mit Grund.  Ein *Sektor*-CRC-Byte, das nicht passt, ist nur ein Hinweis
   (`SourceInfo::warnings`), das Abbild bleibt benutzbar.  Beim Schreiben wird jedes nicht darstellbare Medium mit Begründung
   abgelehnt (Tail hinter der CRC, gemischte Sektorgröße in einer IMD-Spur, unformatierte Lücken sind erlaubt = Spur fehlt).
3. **Schreiben wie CQM:** zuerst die ganze Datei im Speicher aufbauen, **durch den eigenen Leser gegenprüfen**, in
   `<datei>.k1520tmp` schreiben, `rename`.  Nie ein Teilzustand.
4. **Kopffelder bleiben** (`SourceInfo::raw_header`, Kommentar, Datum; bei IMD der Kopfzeilentext, bei TD0 auch
   Version/Laufwerkstyp/Schrittweite), solange die Geometrie unverändert ist.  Neue Dateien: Kommentar
   `"Image file created by k1520emu"`.
5. **Interleave bleibt erhalten:** die Reihenfolge der Sektoren auf der Spur kommt aus der Datei (IMD-Numerierungs-Map,
   TD0-Sektorfolge) und wird beim Schreiben aus der `parseTrack`-Reihenfolge zurückgegeben — anders als bei `.img`/`.cqm`.
6. **Datenrate** wird je Spur geführt und beim Schreiben wieder ausgegeben; fehlt sie (Medium kam aus `.img`/`.hfe`),
   wird sie aus Encoding + Laufwerks-/Sektorgeometrie abgeleitet (8″ FM → 500 FM/IMD-Mode 0; 5,25″/3,5″ DD → 250 MFM;
   HD → 500 MFM) und das im Merkposten dokumentiert.
7. **Container-Kennungen** in der C-ABI wachsen additiv: `td0`, `imd` (Reihenfolge bestehender Zahlen bleibt).  Erkennung
   an der Kennung (`TD`/`td`+Kopf-CRC, `IMD `), nicht an der Endung; Endung nur als Zweitwahl.
8. **Keine Umlaute im Code**, Kommentare Deutsch, Dateien `td0_codec.*`, `imd_codec.*`, `sector_track_io.*`.
9. **Kein Tastenkürzel**, kein Meldungsfenster für das Einlegen (Hinweis im Laufwerkskasten, `profil.py`-Hinweisliste).

## 5. Arbeitspakete

Reihenfolge streng sequenziell (teilen `build/` und `image_codec.cpp`); jedes AP endet grün und mit eigenem Commit.
Modell der Agenten: **Sonnet** (`cpp-coder` für Kern, `general-purpose`/Sonnet für Python-Oberfläche).

| AP | Inhalt | Ergebnis / Test |
|----|--------|-----------------|
| **A0** Fundament | `ContainerType::Td0/Imd` samt `name/extension/selfDescribing/fromExtension/detect`-Stub; `sector_track_io` (Datensatz, Medium ⇄ Sektorlisten); `TrackCodec::buildTrack` wertet `deleted`, `data_crc_ok`, `id_crc_ok`, „ohne Datenfeld“ aus (FM und MFM, mit `parseTrack`-Rundlauf); Prüfung, was `rawCompatible()` über CRC-Fehler/gelöschte Marken sagt (Befund festhalten). | `test_sector_track_io`, Erweiterung `test_track_codec`; bestehende Tests unverändert grün |
| **I1** IMD lesen+schreiben | `imd_codec.*` nach §2.1, `SourceInfo`, Maps (Zyl/Kopf), alle 9 Typcodes, Mode→Rate/Encoding, Mischgrößen-Ablehnung, Atomic-Write mit Gegenprobe; unabhängiger Python-Bau `tests/python/imd_bau.py`. | `test_imd_codec` (synthetisch: FM/MFM, Interleave, gelöscht, Fehler, komprimiert, Maps, fehlende Sektoren, beschädigte Dateien) |
| **T1** TD0 lesen | `td0_codec.*`: Kopf+CRC, Kommentar, `TD` und `td` (LZHUF-Dekoder), Methoden 0/1/2, Flags, Sektor-CRC-Hinweis, Spur-CRC; Methode-2-Frage am Korpus entscheiden; Echtdaten-Test mit `K1520_TD0_DIR` (sonst übersprungen, wie CQM): alle 67 laden, Größen stimmen, Dateisystemerkennung im DiskTool wo möglich. | `test_td0_codec`, Befundtabelle in den Merkposten |
| **T2** TD0 schreiben | `encode/save` (unkomprimiert), Kopf-/Spur-/Sektor-CRC, Kommentar+Datum, Vorlage; Rundlauf Datei→Medium→Datei→Medium über den Korpus (Sektordaten, Spurbytes+Marken, Kopffelder gleich); Kreuzprobe TD0→IMD→Medium. | `Td0Echtdaten.SpeichernUndWiederLadenIstVerlustfrei`, `Kreuz.Td0ZuImd…` |
| **K1** Kernverdrahtung | `DiskImage` (Binden, `speichere`, Autoflush, `saveAs`, `create`, `lastError`, Info-Zugriff), `FloppyDriveV2::mount`, `Laufwerke`, `DiskVolume` (`allowsContainer`, Schreibschutz-Politik wie CQM), `fs_profile`, C-ABI additiv (`k1520_*`/`k1520d_*`, `container_info` für Kommentar/Datum/Version/Rate/Warnungen), `k1520disktool` (`info`, `save-as`, `create`, `export` für beide), `tests/python/test_c_api.py`-Abgleich der drei Deklarationen. | `test_disk_image`, `test_disk_volume`, CLI-Fälle `tests/cli/cases/`, `py_c_api` |
| **G1** Oberfläche | DiskTool + Emulator: Dateidialog-Filter, Anlegen/Speichern unter, Ziehen&Ablegen, `drive_widget`, `disk_info_dialog` (Kommentar, Datenrate, Warnungen), `profil.py`-Hinweise (`td0_hinweise`/`imd_hinweise`: kein UDOS, Fehler-/Löschmarken bleiben), Handbuch `app/help/handbuch.md` (Kürzeltabelle unverändert!), Wächter `py_*` anpassen. | `test_td0_imd_emulator_gui.py`, `test_td0_imd_disktool_gui.py`, `py_help`, `py_keine_umlaute` |
| **Z1** Abnahme | Merkposten `doc/merkposten/td0_imd.md` + Zeile in `README.md` der Merkposten, Kurzabsatz in `CLAUDE.md`, Eintrag in diesem Entwurf („Stand“), Volllauf `tools/dev.sh test` (Hintergrund), `test-format`, `win`; Ticket #9 kommentieren erst nach Zustimmung des Anwenders. | grüne Lanes; Abnahmeprotokoll am Ende dieses Entwurfs |
| *(T4)* | *Optional, nicht geplant:* LZHUF-Kodierer für `td`. | — |

## 6. Offen / Entscheidung beim Anwender

* IMD-/Teledisk-Gegenprobe an echter Software (ImageDisk, Teledisk, SAMdisk, `hxcfe`): sobald der Anwender ein
  `.imd` oder ein erzeugtes `.td0` dort öffnet, Ergebnis im Merkposten festhalten.
* Soll beim Speichern einer `td`-Datei wieder `td` entstehen (T4)?  Empfehlung: nein.

# Merkposten — TeleDisk (`.td0`) und ImageDisk (`.imd`) (#9)

Kern: `core/peripherals/floppy_drive/{td0_codec,imd_codec,sector_track_io}.{h,cpp}`; Entwurf `doc/design/38_td0_imd.md`,
Formatbeschreibung `doc/design/38a_td0_imd_formatbeschreibung.md`.  Beide sind Container wie `.cqm` (lesen UND schreiben, keine
erzwungene Schreibsperre); die Oberflaechen behandeln sie wie `.cqm`.

1. **Clean-Room, MIT.**  Die Formate sind aus HxC (GPL) *beobachtet*, in eigenen Worten beschrieben (38a) und neu implementiert;
   Code, Namen, Kommentare des HxC-Quelltexts sind NICHT uebernommen.  greaseweazle (`image/imd.py`, `td0.py`, Unlicense) durfte frei gelesen werden.
   **Wer hier weiterarbeitet, liest den HxC-Quelltext nicht**, sondern 38a.
2. **`.td0` wird IMMER unkomprimiert (`TD`) geschrieben**; `td` (Advanced Compression) wird gelesen (eigener LZHUF-Dekoder) und als `TD`
   zurueckgeschrieben.  Ein Kodierer ist bewusst nicht gebaut (keine Gegenstelle zum Pruefen).
3. **Streng beim Lesen, ablehnend beim Schreiben.**  Kopf-/Spur-CRC (TD0) sind harte Fehler, Kommentar-CRC und Sektor-CRC8 nur Warnung.
   Abgelehnt beim Schreiben: unformatiertes Medium, Nachspann hinter der Daten-CRC (UDOS), ID-CRC-Fehler, IMD mit gemischten
   Sektorgroessen je Spur.  Eine **leere Diskette** (unformatiert) gibt es deshalb nicht als `.td0`/`.imd`.
4. **Vorlage muss bei jedem Speichern mitgegeben werden** (`DiskImage` haelt `SourceInfo`): sonst gehen Kommentar, Datum, Kopfbytes und
   TD0-**Pseudosektoren** (Flag 0x40, ohne ID-Feld, liegen nicht im Medium, nur in `SourceInfo::pseudo`) verloren.
5. **Sektor-ID immer aus dem Sektorkopf**, nie aus dem Spurkopf (Korpus: ID-Zylinder 89/90 auf Spur 81/82).  Sektorgroessen-Codes 0..6
   (bis 8192 B); `parseTrack` maskiert Codes > 6 weiter.  Interleave bleibt erhalten (anders als `.img`/`.cqm`).
6. **Offen / ungeprueft:** TD0-Methode 2 mit Musterlaenge `2*c` fuer c > 1 ([Q], im Korpus nur c = 1); IMD komplett nur gegen
   gw und eigene Python-Leser/-Schreiber, kein echtes `.imd` und keine echte Teledisk-/ImageDisk-Probe; Laufwerkstyp im TD0-Kopf neuer
   Dateien ist geraten; `guessRate` raet nur 250/500 kbit/s (300 nur ueber die Vorlage).
7. **Pruefstand:** `Td0Echtdaten.*` (`K1520_TD0_DIR=$HOME/projects`, sonst uebersprungen; Rundlauf 100 Dateien, 134 007 Sektoren),
   `tests/python/test_td0_gegenprobe.py` / `test_imd_gegenprobe.py` (gw optional, uebersprungen ohne), unabhaengige Python-Leser
   `td0_pack.py`, `imd_bau.py`.  Die Python-Ebene bleibt ohne greaseweazle gruen.

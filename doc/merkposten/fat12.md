<!-- Merkposten zu Entwurf 44 (MS-DOS FAT12 im DiskTool).  Gilt WIE CLAUDE.md, sobald an FAT gearbeitet wird. -->

# FAT12 (MS-DOS, DCP, PC-DOS) — Merkposten

Entwurf: `doc/design/44_fat12.md`.  Code: `core/filesystem/fat/fat_fs.{h,cpp}`, Anschluss in `core/filesystem/disk_volume.cpp`
(Stichwort `FsType::Fat`, `istFat()`, `hatBaum()`, `dosSystemUebertragen`).  Wächter: `FatFs.*`, `FatDiskVolume.*`,
`FatLieferdisketten.*` (`tests/unit/filesystem/test_fat_fs.cpp`), `tests/python/test_disktool_fat.py`.

1. **Die Lage steht im BPB, nicht im Katalog.**  Die elf Profile `dos160 … dos2880` benennen nur eine Geometrie.  Erkennung =
   BPB plausibel UND (Sprungbefehl ODER FAT-Kopf `Medienkennung FF FF`); `detect_rank: -20`.  Eine Diskette voller `0xE5` ist nie FAT.
2. **`.img` hat keine Geometrie.**  Gleiche Dateigröße, verschiedene Geometrie (360K 40×2 ↔ 80×1; 320K; 640K; 800K 10×512 ↔ 5×1024).
   Zwei Dinge halten das: die **Kopfzahl des BPB** in `looksLikeFat` und `mediumFuer` in `DiskVolume::open` (Medium je Kandidat im
   Zuschnitt des Kandidaten, am Ende im Zuschnitt des Siegers).  Wer die Probe wieder auf dem Medium des ersten Kandidaten laufen lässt,
   bricht `FatDiskVolume.ZweiGeometrienGleicherGroesseUnterscheidetDerBpb`.
3. **Der Bootsektor gehört zur Diskette UND zum Lader.**  `bootsektorMitBpb` setzt die BPB-Felder VOR dem Programm (Sprungziel) von der
   Diskette, behält den Code und alles dazwischen, was nicht BPB ist (DCP führt dort Variablen).  Für gleiche Größe ist das Ergebnis der
   Originalsektor byteweise.  **Nicht** „den ganzen Sektor übernehmen": ein 720K-Lader auf 360K bootete mit falscher FAT-Lage.
4. **`sys` und die Lader:** die Systemdateien müssen die ERSTEN Verzeichnisplätze und den Anfang des Datenbereichs belegen und zusammenhängend
   liegen.  Deshalb: Ziel leer, Reihenfolge der Quelle, Attribute/Zeit mit, Kurznamen (`fat_lfn = 0`), Warnung bei nicht zusammenhängender Lage.
   Der Zuteiler vergibt immer die niedrigsten freien Cluster — NICHT auf „nächster freier nach dem letzten" umstellen.
5. **Lange Namen: Automatik nach OEM-Kennung** (`langeNamenUeblich`).  Neu angelegte Disketten tragen `MSWIN4.1` (= ja); eine Diskette mit
   DCP-Lader (`DCP  3.3`) bekommt Kurznamen.  Grund: ein DOS 3.3 zeigt die Einträge langer Namen als versteckte Systemdateien und
   verbraucht Wurzelplätze (112 bei 720K!).  Lange Namen werden nie gekürzt; `~N`-Kurznamen werden je Verzeichnis eindeutig vergeben.
6. **Zeit ohne Zeitzone.**  `FileEntry::dos_mtime` = „Uhr wie sie zeigte" als Sekunden; Umrechnung nur über `utcZuOrtszeit`/`ortszeitZuUtc`
   (Ortszeit des Rechners).  Beim Auspacken setzt `extractMit` die Dateizeit, beim Einfügen kommt sie von der Datei.
7. **HD nur `.img`** (`containers: [img]` → `FsProfile::nur_img`, sperrt auch cqm/td0/imd): der Stack kennt nur 250 kbit/s; ein HFE/DMK/IMD mit
   falscher Datenrate sähe heil aus und wäre es nicht.
8. **Jeder Schreibvorgang prüft Platz VORHER** (`wouldFit`) und lässt bei „kein Platz"/„Wurzel voll" die Diskette byteweise unverändert
   (`FatFs.EineVolleDisketteIstEineTransaktion`, `WurzelIstEndlich…`).  Einträge langer Namen verbrauchen Wurzelplätze mit.
9. **Querverweis-Reparatur kopiert** (`fat.doppelt.trennen`), sie schneidet nicht ab: ohne Datenverlust, braucht freie Cluster.  Eine Kreuzung
   heißt auch, wenn zwei Einträge denselben Startcluster haben (ein früherer Ausnahmefall dafür war falsch).
10. **Nicht FAT:** `A7150_TEXT40/DCPTXT1/2/Text_Asstech*/SCP1700_30` sind CP/M-86 (`scp1700`), `A7150_BOS1810_LACS` BOS 1810.  Die FAT-Disketten der
    Sammlung: 11× A7150, 6× PC 1834 (`dos720`); `A7150_FOR1/MASM` tragen je einen echten verlorenen Cluster, `A7150_FOR2` zwei unlesbare Sektoren.
11. **Offen:** `recover` (Wiederherstellen gelöschter Dateien) für FAT; Umbenennen; Start einer erzeugten DCP-Systemdiskette am Gerät.

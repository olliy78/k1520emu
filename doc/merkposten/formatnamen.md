# Merkposten — Format- und Profilnamen (Entwurf 47, Zweig `formatnamen`, 2026-11-04)

Entwurf: `doc/design/47_formatnamen.md` (Schema §1/§2, Aliase §4, Anzeige §3, Tabellen alt→neu §10/§11).

1. **Schlüssel und Anzeige sind getrennt.**  Der Katalogname ist technisch und verrät die Geometrie; der Anwender wählt über
   `Kapazität · Rechner · Titel` (`title`/`capacity`/`machines`, `app/katalog_anzeige.py`).  Der Schlüssel steht nur im
   Tooltip und als `itemData`.  Nie einen Schlüssel als einzigen Text in eine Auswahlliste setzen (Wächter `py_katalog_anzeige`).
2. **Format = reine Geometrie** `[fm_]<ss|ds><Zyl>_<Zonen>[_dstep][_idN][_hd|_ed]`, Systemzone `<n>t<S>x<B>[fm]-` davor.
   **Der Name wird nicht erzeugt, sondern geprüft** (`DiskFormat::schemaName()`, Wächter `FormatKatalogSchema.*`):
   ein Generator würde einen Fehler in der Spurliste zu einem „richtigen" Namen machen.  Gleiche Spurliste = EIN Eintrag.
3. **Profil = OS + Layout** `<os>[_<rechner>]_<format ohne Systemzone>[_<rolle>]`, Rolle aus `{boot, daten, arbeit, syl,
   kombi, leer}` (Wächter `FsKatalogSchema.*`).  Kapazität und Rechner-Langname nie im Namen.  `udos1715` blieb Familienname
   (kein `ndos_…`) — offene Anwenderfrage.
4. **Alte Namen bleiben für immer als `aliases:` gültig** (Konfigurationen und Skripte der Anwender); jede Ausgabe schreibt
   nur den neuen Namen.  Auflösung an EINER Stelle (`find`, `forFormat`, C-ABI `k1520d_fs_resolve`/`k1520_format_resolve`);
   wer einen Namen vergleicht, löst vorher auf.  Wächter `KatalogAltnamen.JederAlteNameLoestAuf` (eingefrorene Liste
   `tests/fixtures/formatnamen_alt.txt`), `FormatKatalogAlias.*`, `FsKatalogAlias.*`.  Gespeicherte Formatnamen
   kanonisiert `app/config_io.py` beim Laden OHNE die Formatversion zu erhöhen (sonst sperrt es ältere Programme aus).
5. **Falle: Profilname ≠ Formatname.**  Beim Umbenennen hat eine Ersetzung Profilnamen in Format-Argumente geschrieben
   (`mount_path(…, "cpa_ds80_…_boot")` → „Unbekanntes Format").  Mount/`createDisk`/`FormatDialog` nehmen FORMATE, `create --fs`/
   `open(filesystem=…)` nehmen PROFILE.
6. **`machines: []` heißt „unbekannt", nicht „alle"** und wird nur gefüllt, wo es belegt ist; leere Profile erscheinen im
   Filter „Für Rechner" nur unter „alle".  Rechnerwortschatz geschlossen (Kopfkommentar `data/formats.yaml`, Wächter
   `KatalogAnzeige.AusgelieferterKatalogIstVollstaendig`).  Unsicher/zu prüfen: CP/A und SCPX nur `A5120` (A5130/K8924?),
   UDOS 4.x mit PRG 710/710-1 aus „UDOS 4.3" abgeleitet, alle FAT-Profile außer `dos_ds80_9x512` ohne Rechner.
7. **Erkennung:** `DiskVolume::oeffnenMit` lässt innerhalb EINES Formats Profile anderer Art mit schlechterem `detect_rank`
   fallen (wie schon zwischen Formaten) — nötig, seit `cpa640 ≡ k5601_16x256` ein Eintrag ist.  Profile gleicher Art bleiben
   Alternativen (Gegenprobe `FsCheckGegenprobe.*`).
8. **Alte Dokumente** (`doc/format.md`, `doc/*_diskettenformat.md`, `doc/disketten_bestand.md`, Merkposten außerhalb dieser Datei)
   nennen noch die Namen vor 2026-11-04; die Zuordnung steht in Entwurf 47 §10/§11 und in `tests/fixtures/formatnamen_neu.txt` /
   `profilnamen_neu.txt`.  Beim Anfassen eines solchen Dokuments die Namen nachziehen, nicht vorher.

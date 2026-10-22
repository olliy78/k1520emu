<!-- Merkposten wie doc/merkposten/disktool.md: gilt WIE CLAUDE.md, sobald an diesem
     Teilsystem gearbeitet wird.  Entwurf: doc/design/37_tar_satz_und_platte.md -->

# tar-Archive/-Sätze bearbeiten und P8000-Plattenabbilder (Entwurf 37) — Merkposten

Tickets #10 (tar-Satz) und #11 (Platte), Zweig `tar_satz_platte`.  Kern: `core/filesystem/tar/`
(`tar_archiv.*`, `tar_fs.*`), `DiskVolume::tar*`, `core/filesystem/wega/platten_volume.*`,
`core/filesystem/fileinfo.*`; Python: `app/disktool/ui/main_window.py` (Satz), `archive.py`.
Ältere Regeln zu tar (Lesen, Export, `tar720_leer`): `doc/merkposten/cqm.md` Nr. 13.

**tar-Satz**

1. **Jede Änderung baut den V7-Strom NEU** (`TarArchiv`: Liste Kopf+Daten), nie In-place-Patchen.
   Dabei geht verloren, was V7 nicht kennt: ustar-Präfix, pax-/GNU-Langnamen und -Felder,
   uname/gname, Gerätenummern; Namen > 100 Zeichen werden abgelehnt (nicht gekürzt), Verzeichnisse
   stehen als Typ `5`, Linknamen nur bei Typ 1/2.  Das Ergebnis ist das, was der P8000-/WEGA-`tar`
   schreiben würde.  Wächter `TarWrite.ModellBautenV7Strom`, `ZuLangerNameWirdAbgelehnt`,
   `SystemTarArchivBearbeitenUndGegenprobe` (System-`tar` liest das Ergebnis).
2. **Schreibbar ist nur ein ABGESCHLOSSENES Archiv** (Kopf bei Sektor 0 UND Endmarke, oder leer).
   Folgeteil und abgeschnittenes Archiv bleiben lesbar (Hinweis, Angebot „tar-Satz öffnen…") —
   ein Umbau würde die Fortsetzung zerstören.  Wächter `TarWrite.UnabgeschlosseneArchiveBleibenLesbar`,
   `TarSatz.OffenerSatzIstGeschuetztUndNurMitFolgeteilenBearbeitbar`,
   `test_a_follow_up_part_stays_readonly_with_the_reason`.
3. **Der Satz ist EIN Strom im Speicher**, geschrieben wird nur beim Speichern; `flush()` ist im
   Satzmodus leer, `dirty()` folgt der Strom-Revision.  Verteilt wird auf Abbilder GLEICHEN Formats
   (nur einheitliche Katalogformate), das letzte mit Nullen aufgefüllt.  Wächter
   `TarSatz.VerteiltAufNAbbilderMitNummerVorDerEndung`, `LetztesAbbildMitNullenAufgefuellt`,
   `GemischtesFormatIstKeinSatzformat`, `GegenprobeSystemTarUeberDieAbbildDatenbereiche`.
4. **Nummer VOR der Endung** (`name1.hfe`, `name2.hfe`; die Endung bestimmt den Container
   img/hfe/dmk/cqm).  Ein mit `tarDisketteNeu` angelegtes EINZELabbild behält seinen Namen ohne
   Nummer, solange es auf ein Abbild passt.  Wächter `TarSatz.BasisnameOhneZiffernUndMitZiffern`,
   `TarNeu.WaechstDerInhaltUeberEineDisketteWirdBeimSpeichernEinSatz`.
5. **Nie stillschweigend löschen**: werden es weniger Abbilder, bleiben die höheren Nummern liegen
   und kommen als Hinweis (Plan-Dialog nennt sie vorab).  Geschrieben wird alles-oder-nichts
   (Hilfsnamen `~satzN_…`, dann umbenennen); Vorhandenes wird einmal je Sitzung als `<name>~`
   gesichert.  Wächter `TarSatz.SchrumpfenLoeschtAlteNummernNieStillschweigend`,
   `test_save_dialog_shows_plan_overwrites_and_leftovers`, `test_leftover_numbers_are_reported_after_saving`.
6. **Greaseweazle: ein Satz geht nicht auf EINE echte Diskette** (`physischSchreibenGrund`, Aktion
   gesperrt mit Klartext und Weg: Satz speichern → Einzelabbilder öffnen → je Abbild schreiben).
   Wächter `TarSatz.PhysischSchreibenIstFuerSaetzeGesperrtMitBegruendung`,
   `test_a_set_cannot_be_written_to_a_physical_disk`.
7. **`.tar` ablegen — Popup nur auf einem LEEREN Archiv** eines Satzes („Als Datei im Container“
   / „Ist ein fertiger tar-Container“); auf Archiv mit Inhalt, bei mehreren abgelegten Dateien und
   bei Folgeteil/unbrauchbarer Datei immer „als Datei“.  Ein fertiger Container über Diskettengröße
   wird ein Satz.  Wächter `TarNeu.TarAufLeereDisketteBietetBeideWegeAufNichtLeereNurDatei`,
   `ContainerWirdDasArchivEinesSatzes`, `ContainerUeberDisketteGroesseWirdEinSatz`,
   `test_tar_on_an_empty_archive_*`, `test_tar_on_a_nonempty_archive_is_always_a_file_without_a_popup`.
8. **Altregel `tar720_leer` bleibt**: die LEERE Einzeldiskette nimmt genau EINE `.tar` als Archiv
   (`tar-put`/„tar-Datei auf Diskette schreiben…“, ohne Popup), danach ist sie ein normales
   abgeschlossenes Archiv.  Wächter `test_empty_single_disk_keeps_the_one_tar_rule`,
   `py_disktool_tar_schreiben`, `TarWrite.TarPutBleibtEinmalwurfUndDanachBearbeitbar`.
9. **`.img`-Geometrie wird beim Öffnen eines Satzes GERATEN** (die Dateigröße ist bei kleinen
   Formaten mehrdeutig); wer es weiß, übergibt das Format: `openTarSatz(…, format)` /
   `DiskTool.open_tar_satz_format`.  Neu angelegt wird immer mit gewähltem Format
   (`tarFormate`, `tarDisketteNeu`, `tarSatzNeu`; `create --fs tar720_leer --format`).
   Wächter `TarNeu.OeffnenMitVorgegebenemFormatUndUnbekanntesFormat`,
   `FormatlisteHatNurEinheitlicheFormateUndJedesLaesstSichAnlegen`.
10. **Metadaten**: `.fileinfo` mit `fs=tar|wega` (mode/uid/gid/mtime) — Herunterladen schreibt sie,
    Hinzufügen liest sie, sonst 0644/0755, uid/gid 0, Zeit jetzt.  Kern und Python-Schreiber
    (`app/disktool/fileinfo.py`) meinen DASSELBE Format.  Wächter `TarWrite.FileinfoRundlaufUndVorgaben`,
    `test_python_fileinfo_writer_knows_tar_and_wega`, `test_python_written_tar_fileinfo_is_read_by_the_core`.
11. **Archiv eines Satzes (T5)**: `abbilder/` (die gespeicherten Abbilddateien UNVERÄNDERT, in
    Stromreihenfolge), `dateien/` (alle Dateien + `.fileinfo`), Abschnitt `tar_satz:` der
    `diskarchive.yaml` (`basename`, `format`, `container`, `images[index,path,size,sha256]`; kein
    `image:`), Satz im Inhaltsverzeichnis.  **Archiviert wird der Stand der DATEIEN**: ein Satz mit
    ungespeicherten Änderungen wird nach Rückfrage erst gespeichert (Nein = nichts Halbes);
    `create_archive(satz_abbilder=…)` weist einen `dirty` Satz mit `ValueError` ab.  Die Dateiliste
    führt `DiskTool.tar_satz_dateien` (gesetzt beim Öffnen/Speichern; leer bei `dirty`), weil der Kern
    nur den Basisnamen kennt und Abbilder beim Öffnen beliebig heißen dürfen.  Zurück:
    `archive.satz_entpacken` / `--satz-entpacken ORDNER` prüft sha256 und Namen (nur Dateiname,
    nichts ausserhalb des Ordners), schreibt erst nach der Prüfung ALLER Abbilder, überschreibt nur mit
    `--force`.  `ohne_abbild=True` ohne `satz_abbilder` bleibt das Sammlungsarchiv (nur Dateien).
    Wächter `py_disktool_archiv_tar_satz`.

**Platte (#11)**

12. **Eine Fassade `PlattenVolume`** (Partitionen `md0…md4` als oberste Ordner einer virtuellen Wurzel);
    die `.fileinfo`-Logik liegt in EINER Einheit (`core/filesystem/fileinfo.*`) und wird von
    `DiskVolume` (WEGA) und `PlattenVolume` gemeinsam benutzt — Format unverändert (`fs=wega`).
    Wächter `PlattenVolume.RundlaufExtractInsertBehaeltAngaben`, `EinfuegenOhneFileinfoNimmtVorgaben`.
13. **C-ABI `k1520d_platte_*`** (additiv) und `PlatteTool` mit der Oberfläche des `DiskTool`
    (duck-typing, `ist_unix=True`, `tar_satz=False`); `test_c_api` gleicht Header ↔ Bibliothek ↔
    Bindung mechanisch ab.  Wächter `py_disktool_platte` (`test_every_platte_function_is_in_header_and_binding`).
14. **Schreibschutz ist die Vorgabe, vor dem ersten Schreiben entsteht `<abbild>~`; zurückgeschrieben
    wird in der Art der Quelle** (`.img` bleibt `.img`, `.img.gz` bleibt gepackt).  Wächter
    `PlattenVolume.SchreibschutzVerweigertSchreibenUndAendern`, `GepacktesAbbildBleibtGepacktUndHatSicherung`,
    `test_backup_entsteht_vor_dem_ersten_zurueckschreiben`.
15. **Gesperrte Funktionen mit Begründung** für Platten (Diskeditor, Boot, GW, Formatwahl, Archivieren,
    Prüfen/Wiederherstellen): `app/disktool/ui/schreibsperre.py::PLATTE_GESPERRT`.  Eine Platte hat
    keine Spuren und kein Diskettenformat; ihren Inhalt holt „Alles extrahieren“.  Wächter
    `py_disktool_platte_gui`.

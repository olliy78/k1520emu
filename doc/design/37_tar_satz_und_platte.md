# Entwurf 37 — tar-Diskettensätze bearbeiten (#10) und P8000-Plattenabbilder im DiskTool (#11)

Stand 2026-10-20.  Zweig `tar_satz_platte`.  Tickets: GitHub #10, #11 (Label `in progress`).
Vorlagen: `doc/design/35_cqm.md` §4a (tar lesen/exportieren), `doc/design/27_wega_dateisystem.md`
(WEGA, Platte nur CLI), `doc/merkposten/cqm.md` Nr. 13, `doc/merkposten/disktool.md`.

## 0. Ausgangslage (erkundet)

- tar: `TarFileSystem` (`core/filesystem/tar/`) ist **nur lesbar** (`write/erase/mkfs` = „nur lesbar“,
  `DiskVolume::readOnlyForced`).  Schreiben geht nur über `tarSchreiben` (ein fertiges `.tar` auf die
  leere `tar720_leer`).  `DiskVolume::openTarSatz` verkettet Abbilder zu EINEM Strom, schreibgeschützt;
  `tarExport` lehnt Sätze ab.  `.fileinfo` mit `fs=tar|wega` wird beim Extrahieren schon geschrieben und
  beim Einfügen (WEGA-Zweig) gelesen; der Python-Schreiber `app/disktool/fileinfo.py` kennt nur UDOS/CP/M.
- Platte: `WegaPlatte` (`core/filesystem/wega/wega_platte.*`) liest/schreibt, **nur über die CLI**
  (`tools/k1520disktool.cpp platte()`).  Keine C-ABI, `DiskVolume` kennt keine Partitionen,
  GUI/`DiskTool` (Python) kennen sie nicht.  `WegaFileSystem` ist direkt als `FileSystem` nutzbar.

## 1. Festlegungen (Entscheidungen, nicht neu aufrollen)

### tar (#10)
1. **Ein editierbares Archivmodell im Speicher** (`TarArchiv`: Liste Kopf+Daten).  Jede Änderung
   (Hinzufügen, Löschen, Ersetzen, Attribute) baut den Strom neu: V7-Köpfe, Daten auf 512 aufgefüllt,
   Endmarke (2 Nullblöcke).  Kein In-place-Patchen.  Der WEGA-/P8000-`tar` ist V7: keine ustar-/pax-
   Felder, Namen ≤ 100, Verzeichniseinträge (Typ `5`) werden mitgeschrieben.
2. **Schreibbar ist, was ein abgeschlossenes Archiv ist**: Kopf bei Sektor 0 UND Endmarke (oder leere
   Diskette/Satz).  Folgeteil (kein Kopf am Anfang) und abgeschnittenes Archiv bleiben lesbar +
   Hinweis; sie lassen sich nur als Satz öffnen.
3. **Satzmodell**: `DiskVolume` im Satzmodus hält Geometrie (Katalogformat, einheitlich), Basisname
   und die Strom-Bytes.  Speichern verteilt den Strom auf Abbilder gleichen Formats, Nummer ans Ende
   des Basisnamens **vor** der Endung (`name1.hfe`, `name2.hfe`, …; wie die Sammlung `w11data1`…);
   letztes Abbild mit Nullen aufgefüllt.  Einzeldiskette, deren Inhalt die Kapazität übersteigt,
   wird beim Speichern automatisch ein Satz (Rückfrage mit Namensvorschlag).  Alte, nicht mehr
   gebrauchte Nummern werden nie stillschweigend gelöscht (Hinweis).
4. **Neu anlegen**: leere tar-Diskette bzw. leerer Satz mit gewähltem Format (alle einheitlichen
   Katalogformate, nicht nur 720K).  Das Format ist für den ganzen Satz gleich; beim Öffnen wird es
   erkannt.
5. **`.tar` auf eine leere tar-Diskette ziehen**: Popup „Als Datei im Container ablegen“ /
   „Ist ein fertiger tar-Container“.  Auf nicht leerem Archiv immer „als Datei“.  Ein fertiger
   Container über Diskettengröße wird als Satz angelegt.
6. **Hinweis bei Folgediskette** (Anfang mitten in Daten oder keine Endmarke) mit Angebot
   „Datei ▸ tar-Satz öffnen…“.
7. **Greaseweazle**: Sätze lassen sich nicht direkt auf physische Disketten schreiben (Aktion
   gesperrt, Begründung im Klartext); Weg: Satz speichern → Einzelabbilder öffnen → je Abbild GW.
8. **Metadaten**: Herunterladen schreibt `.fileinfo` (`fs=tar`: mode/uid/gid/mtime…); Hinzufügen
   liest sie, sonst Vorgaben (0644/0755, uid/gid 0, mtime jetzt).  Kern und Python-Schreiber
   (`fileinfo.py`) müssen dasselbe Format meinen.
9. **Archiv eines tar-Satzes** enthält alle Abbilder + Dateien + Metadaten (`archive.py`,
   bisher `ohne_abbild`).

### Platte (#11)
10. **Eine Plattenfassade im Kern** (`PlattenVolume`, neben `WegaPlatte`), die die Partitionen
    `md0…md4` als oberste Ordner einer virtuellen Wurzel zeigt und für Lesen, Schreiben, Löschen,
    `mkdir`, Attribute, Extrahieren/Einfügen mit `.fileinfo` dieselbe Logik benutzt wie `DiskVolume`
    für WEGA.  Die `.fileinfo`-Lese-/Schreibcode wird dazu aus `DiskVolume` in eine gemeinsame
    Einheit gezogen (kein Kopieren; Format unverändert, `fs=wega`).
11. **Eigene C-ABI `k1520d_platte_*`** (additiv) und Python-Klasse `PlatteTool` mit der Oberfläche des
    `DiskTool`, soweit die GUI sie braucht (duck-typing); `ist_unix=True`.
12. **Schreibschutz Vorgabe** (wie bei physischen Datenträgern); Aufheben im GUI; vor dem ersten
    Schreiben `<abbild>~` (existiert in `WegaPlatte`).  `.img`/`.img.gz` beides; zurückgeschrieben wird
    in der Art der Quelle.
13. **GUI**: Datei ▸ Öffnen und Drag&Drop erkennen eine Platte (`WegaPlatte::istPlatte`); kein
    Diskettenformat-Dialog; Anzeige wie bei WEGA-Diskette (Baum, Rechte, uid/gid, Zeit), Eigenschaften-
    dialog zum Ändern; Funktionen, die für Platten keinen Sinn haben (Diskeditor, Boot, GW,
    Formatwahl), sind gesperrt mit Begründung.

## 2. Arbeitspakete (Reihenfolge fest; je AP: Agent Sonnet 5.5 → Prüfung durch mich → Doku/Commit)

Die Ketten sind unabhängig, laufen aber **nacheinander** (gemeinsamer `build/`, gleiche Dateien
`disk_volume.cpp`, `main_window.py`).

| AP | Inhalt | Wächter (neu) | Status |
|----|--------|---------------|--------|
| **P1** | Kern: `.fileinfo`-Code aus `DiskVolume` herauslösen (`core/filesystem/fileinfo.*`), `PlattenVolume` (list/read/write/erase/mkdir/setAttributes/extract/insert, flush, Backup), CLI `platte()` darauf umstellen | `PlattenVolume.*` (C++) | fertig 2026-10-21 |
| **P2** | C-ABI `k1520d_platte_*` + ctypes `PlatteTool` + `test_c_api`-Abgleich | `py_disktool_platte` | fertig 2026-10-22 |
| **P3** | GUI: Öffnen/Drop, Ordneransicht md0…, Schreibschutz, Eigenschaften, gesperrte Aktionen, Handbuch | `py_disktool_platte_gui` | fertig 2026-10-22 |
| **T1** | Kern: `TarArchiv` + schreibbares `TarFileSystem` (abgeschlossene Archive), Attribute, `.fileinfo` | `TarWrite.*` | umgesetzt 2026-10-22 (Abnahme offen) |
| **T2** | Kern: Satzmodus (Verteilen auf N Abbilder, Nummerierung, Überlauf Einzeldiskette → Satz), `tar-satz` schreibend; CLI `tar-satz-save`/`tar-satz-neu` | `TarSatz.*`, `dt_tar_satz_*.cli` | offen |
| **T3** | Neu-Anlegen mit Format (leere Diskette/Satz), `.tar` als Datei vs. fertiger Container, C-ABI + `DiskTool` | `TarNeu.*`, `py_disktool_tar_satz` | offen |
| **T4** | GUI: Satz bearbeiten (Drag&Drop, Löschen), Speichern → mehrere Abbilder, Popup `.tar`, Hinweis Folgediskette, GW-Sperre, Python-`fileinfo.py` | `py_disktool_tar_satz_gui` | offen |
| **T5** | Archiv eines Satzes (Abbilder + Dateien + Meta), Handbuch, Merkposten, `CLAUDE.md`-Absatz | `py_disktool_archiv_tar_satz` | offen |

Reihenfolge der Ausführung: P1 → P2 → P3 → T1 → T2 → T3 → T4 → T5 (Platte zuerst: kleineres Risiko,
der `.fileinfo`-Umbau in P1 nützt dem tar-Weg T1).

## 3. Abnahme je AP

- `tools/dev.sh test` grün (Hintergrundlauf), Neutests laufen einzeln mit `-R`; keine Tests entfernt
  oder abgeschwächt.
- Keine Umlaute im Code (Konvention), deutsche Kommentare, `dev.sh` statt Binärdateien aus `build*/`.
- Echte Gegenprobe: tar mit System-`tar` (`tar -tvf`) bzw. WEGA-Gegenprobe `tools/p8000/wega_s3fs.py`.
- Nach jedem AP: Status in §2 + „Verlauf“ (§4) aktualisieren, Commit auf `tar_satz_platte`
  (nur eigene Dateien, nie `git add -A`), Kommentar im Ticket.

## 4. Verlauf

- 2026-10-20: Plan geschrieben, Tickets auf `in progress`.
- 2026-10-21: P1 abgenommen (7 PlattenVolume-Tests, Volllauf 3697/3697 gruen).
- 2026-10-22: P2 abgenommen (43 `k1520d_platte_*`, `PlatteTool`, 14 Faelle `py_disktool_platte`). Bekannt rot, vorbestehend (auch ohne P2): `py_kermit_gui` SEGFAULT unter Last (`kermittool/binding.py abort`).
- 2026-10-22: P3 umgesetzt (GUI: Oeffnen/Drop `.img.gz`, Plattenbaum md0..md4 mit Rechten/uid/gid/Zeit, Eigenschaften, Holen/Einfuegen mit `.fileinfo`, Ordner anlegen, Loeschen, Schreibschutz-Vorgabe, gesperrte Aktionen mit Begruendung in `schreibsperre.PLATTE_GESPERRT`, Handbuch-Abschnitt; 18 Faelle `py_disktool_platte_gui`).
- 2026-10-22: `py_kermit_gui`-SEGFAULT behoben (Use-after-free Kermit.close()/abort*(), Sperre, 2474dc8); Python-Ebene 78/78 gruen.
- 2026-10-22: T1 umgesetzt (`core/filesystem/tar/tar_archiv.*`, schreibbares `TarFileSystem`, `DiskVolume` ohne Dauer-Schreibsperre fuer abgeschlossene Archive, 10 Faelle `TarWrite.*`; alte Regel 'tar720 nur lesbar' in `TarFs.NurLesbar`, `CqmDiskVolume.*`, `TarLeer.*`, `py_disktool_tar_schreiben`, `py_cqm_disktool_gui` gezielt auf 'ohne Endmarke' bzw. 'schreibbar' umgestellt).

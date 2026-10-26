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
3. **Der Satz ist EIN Strom im Speicher**, geschrieben wird nur beim Speichern; `flush()` ist bei einem
   echten Satz (mehrere Abbilder) leer, bei einer tar-Diskette (Nr. 16) schreibt es ins eigene Abbild;
   `dirty()` folgt der Strom-Revision.  Verteilt wird auf Abbilder GLEICHEN Formats
   (nur einheitliche Katalogformate), das letzte mit Nullen aufgefüllt.  Wächter
   `TarSatz.VerteiltAufNAbbilderMitNummerVorDerEndung`, `LetztesAbbildMitNullenAufgefuellt`,
   `GemischtesFormatIstKeinSatzformat`, `GegenprobeSystemTarUeberDieAbbildDatenbereiche`.
4. **Nummer VOR der Endung** (`name1.hfe`, `name2.hfe`; die Endung bestimmt den Container
   img/hfe/dmk/cqm).  Eine tar-Diskette (Nr. 16) behält ihren Namen ohne Nummer, solange der Inhalt auf ein
   Abbild passt; beim Nummerieren entfallen abschließende Ziffern des Namens.  Wächter `TarSatz.BasisnameOhneZiffernUndMitZiffern`,
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
8. **Die Altregel „`tar720_leer` nimmt genau EINE `.tar`“ ist ENTFALLEN** (2026-10-22, Vereinheitlichung, Nr. 16):
   `tarLeer/tarPruefen/tarSchreiben`, `k1520d_tar_put*`, `DiskTool.tar_leer/tar_put*`, `act_tar_schreiben`
   gibt es nicht mehr.  Die leere Diskette ist ein leeres Archiv: eine einzelne `.tar` darauf fragt wie in Nr. 7
   (Datei/Container), danach ist sie NICHT schreibgeschützt.  `tar-put` bleibt als CLI-Hülle (Container auf LEERES
   Archiv + Speichern).  Wächter `TarLeer.ContainerRundlaufUndNurAufLeeremArchiv`, `TarLeer.NurInEinLeeresTarArchiv`,
   `TarWrite.ContainerAufLeereDisketteDanachBearbeitbar`, `dt_tar_put_nur_auf_leerem_archiv.cli`,
   `test_tar_on_an_empty_single_disk_offers_both_ways`.
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
16. **Vereinheitlichung (2026-10-22): eine tar-Diskette IST ein Satz mit N >= 1 Abbildern.**  Jede aus einer Datei
    geöffnete oder angelegte *schreibbare* tar-Diskette (auch `tar720_leer`) arbeitet im Satzmodus
    (`DiskVolume::einzelSatzMachen`, `tarSatzEinzel()`): Strom im Speicher, `flush()`/Strg+S schreibt ihn in
    DASSELBE Abbild zurück (`satzInsAbbild`: Container/Format/CQM-Kopffelder bleiben, `<abbild>~` einmal je Sitzung,
    nicht bei ganz leerem Archiv), solange er auf EIN Abbild passt; sonst meldet `flush()` den Weg und
    `tarSatzSpeichern()` nummeriert (danach echter Satz, `tarSatzEinzel()` false).  Folgeteil/abgeschnitten/ohne
    Endmarke werden NICHT umgestellt (lesbar + Hinweis).  Es gibt keine Platzgrenze beim Einfügen mehr.
    Das Profil `tar720_leer` bleibt (Erkennung ganz Null, sonst „CP/A Füllbyte 0x00“).  `tar-export` einer tar-Diskette
    liefert den Strom einschließlich ungespeicherter Änderungen; einzelne Folgeteile lassen sich nicht mehr roh auf
    leere Disketten schreiben — Weg ist der Container-Satz.  Greaseweazle: erlaubt, wenn gespeichert und EIN Abbild.
    Wächter `TarNeu.GeoeffneteTarDisketteSchreibtInsSelbeAbbildZurueck`, `…WaechstZumSatz`, `TarSatz.Einzeldiskette…`,
    `TarWrite.ZuGrossFuerEinAbbild…`, `py_disktool_tar_satz_gui` (`…_without_a_question`, `…_overflow_…`).

17. **tar im MUTOS-8000-Layout (2026-10-25, Entwurf 39 Teil A „A5b“, #22).**  Das V7-`tar` des MUTOS schreibt seinen Strom auf
    das **Rohgeraet** `/dev/rrkN`, nicht linear ab Zylinder 0: 512-B-Bloecke des Treibers ab Zylinder 1, Verschraenkung 2,
    Spurversatz 6 (`Mutos8kDev`), **494 Bloecke = 252 928 B je Diskette**, Zylinder 0 bleibt unberuehrt.  Mehrdisketten gibt es dort
    nur ueber `dd` (`tar cf - … | dd of=/dev/rrk0 bs=512 count=494`, Folgediskette `skip=494`).  Dafuer vier Profile in
    `data/formats.yaml` (`type: tar`, `unix_layout: mutos8k`, `data_start` Zyl. 1): `tar_mutos8k_26x128_mfm` (Format `udos_ss77`, K5601),
    `tar_mutos8k_26x128_fm` (`mf3200_26x128`, 8″) und je eine `…_leer`.  Festlegungen: (Namensschema `mutos8k_<Sektoren>x<Bytes>_<fm|mfm>`, 2026-10-26.  Die alten Namen (`mutos8k_fm`, `mutos8k_mfm`, `mutos77`, `mutos80`, `tar_mutos8k_{fm,mfm}[_leer]`) gelten als Alias (`aliases:` in `data/formats.yaml`, `FsCatalog::find`; Wächter `FsCatalog.AlteProfilnamenLoesenAlsAliasAufDasNeueProfilAuf`), stehen aber in keiner Liste und nie als Erkennungsergebnis.)
    * **Ein Strom-Zugriff, ein Ort**: `TarSpaceQuelle(SectorSpace&, const FsProfile*)` rechnet bei `mutos8k_layout` ueber `Mutos8kDev`,
      sonst linear; **jeder** Zugriff des `DiskVolume` auf den Datenstrom (Erkennung, Mounten, `linearBytes`, Kapazitaet, Satz speichern,
      `satzInsAbbild`, Nullfuellung) geht ueber diese Klasse mit `profile_` — nie direkt `space->read/write/size()`.
    * **Als „Format“ kann das Profil genannt werden**: `tarSatzNeu`/`tarDisketteNeu`/`openTarSatz(…, format)` nehmen ein Katalogformat
      (linear, wie bisher) ODER den Namen eines tar-Profils mit Layout (`tar-satz-neu --format tar_mutos8k_26x128_mfm`); `tarFormate(formats, fs_cat)`
      nennt sie mit der Kapazitaet des Rohgeraets (C-ABI/GUI-Liste „Neue tar-Diskette/Satz“ ohne Aenderung).  Linear bleibt `--format udos_ss77`.
    * **Erkennung VOR den Dateisystemen** der Geometrie (`disk_volume.cpp`, Vorprobe hinter der Schleife): eine MUTOS-tar-Diskette ist fuer UDOS eine
      leere Diskette (Zyl. 0 = E5).  Probiert wird `looksLikeTar` bei Block 0 (Pruefsumme, Name, Groesse) bzw. „Datenbereich ganz Null“ (`_leer`); nur ein
      `wega_mutos`-Dateisystem derselben Geometrie geht vor.  Wächter: eine UDOS-/MUTOS-Diskette bleibt, was sie ist.  **Das Layout bestimmt das ERSTE
      Abbild eines Satzes**; Folgedisketten beginnen mitten in einer Datei und sind nicht erkennbar (`openTarSatz` reicht es durch).
    * **Neue Disketten tragen im Datenbereich Nullbloecke, Zyl. 0 bleibt wie formatiert (E5)**: MUTOS-`tar tvf` auf einer nie beschriebenen
      (E5-)Diskette endet nicht sauber, eine Null-Diskette ist ein leeres Archiv.  Der Rest der letzten Diskette eines Satzes ist Null (wie linear).
    * **`.img` kennt kein Aufzeichnungsverfahren**: 77 × 26 × 128 als `.img` wird ohne Angabe immer MFM (K5601); FM-`.img` nur mit `--fs`/vorgegebenem Profil.
    * **Gast-Verhalten, kein Fehler** (Befund 2026-10-25): eine Diskette eines Satzes ist KEIN abgeschlossenes Archiv — `tar` liest zuletzt einen
      Datensatz zu 8 Bloecken ueber die 494 hinaus und meldet `Tar: tape read error` (`err on dev 13/0 bn=488 er=0,305`); der Inhalt davor ist vollstaendig.
      Ein Archiv ueber 494 Bloecke haengt MUTOS-`tar` ohne Meldung (kein Mediumende).  `tar` braucht `pwd` in `/bin`, `/usr/bin` oder `/mnt/bin`.
    * **B1-Nebenbefund geklaert (kein DiskTool-/Emulatorfehler):** das offline mit `rawpack.py` (`sector_write`) gepackte Abbild las MUTOS an 16 Sektoren falsch,
      weil es **vor dem Lesen von MUTOS selbst veraendert worden war**: MUTOS schreibt ueber das Blockgeraet `/dev/rk0` **verzoegert** (Pufferspeicher, `sync` noetig);
      wurde die Diskette im Debugger (`mount A …`) getauscht, bevor die Puffer der vorigen Diskette geschrieben waren, landeten diese Bloecke auf der NEUEN Diskette
      (nachgestellt: `dd of=/dev/rk0 seek=38`, Tausch, `sync` → Bloecke 38–41 der neuen Diskette ueberschrieben).  Ein frisch mit `rawpack` gepacktes Abbild liest MUTOS
      fehlerfrei (zwei Laeufe, auch nach Tausch).  **Regel: vor jedem `mount A` im Debugger `sync` und warten; Rohgeraet `/dev/rrkN` ist ungepuffert.**
    Wächter: `TarMutos.*` (`test_tar_mutos.cpp`: Abbildung mit EIGENER Formel, MUTOS-Fixtures `mutos8000_tar_*.dmk`, Satz-Rundlauf in hfe/dmk/img und FM,
    System-`tar`-Gegenprobe, Zyl. 0 unberuehrt, Erkennung, Formatliste), `FsCatalog.ProfilnamenSindEinStabilerVertrag`, `py_disktool_tar_mutos`,
    `dt_tar_mutos_*.cli`, und die **Abnahme unter MUTOS im Emulator** `cli_mutos8000_tar_disktool_geschrieben` (`tar tvf`/`tar xvf` einer DiskTool-Einzeldiskette +
    `cmp` gegen die Originale, DiskTool-Satz aus zwei Disketten ueber `dd | tar`, Grenzdatei `cmp`-gleich).

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

# Merkposten — BS600 im DiskTool: Prüfung, Rettung, Oberfläche (Entwurf 48, AP-B4/B5, 2026-11-05)

Gehört zu `doc/merkposten/bs600.md` (legt AP-B2 an, Zusammenführen später).  Format: `doc/bs600_diskettenformat.md`; Dateisystemklasse
`core/filesystem/bs600/bs600_fs.*`; Prüfung `core/filesystem/check/bs600_check.cpp`; Rettung `bs600_recover.cpp`.

1. **Die Befundkennungen `bs600.*` sind ein Vertrag** (Tabelle im Kopf von `bs600_check.cpp`, Wächter `Bs600Check*`,
   `FsCheckKeineFalschmeldungen.JedeUnversehrteFixturePrueftOhneBefund` mit beiden Originalen).  Beide Originalabzüge prüfen
   **ohne jeden Befund**; nur die PRG-700-Diskette trägt den Hinweis `bs600.urlader.prg700`.  Was das Formatdokument nicht belegt (Typ `L`,
   Rest[10], Sektor 4–6 der Spur 0), wird nicht beurteilt.  Alles liegt auf der Ebene *Verwaltung* (Spur 0): die Schnellprüfung beim Öffnen
   liest keine Datenspur (`spuren_gesamt == 1`); nur `bs600.datei.unlesbar` (Medium) braucht die Vollprüfung.
2. **`tabelle.fehlt` ist Gefahr, `tabelle.ueberzaehlig` nur Warnung.**  Das Original-BS600 allokiert allein aus der Belegungstabelle; fehlt ein Bit,
   überschreibt die nächste Datei die vorhandene.  `Bs600FileSystem::write` vereinigt dagegen Tabelle und Einträge und ist von dem Fehler nicht
   betroffen — die Gefahr gilt dem Gerät, nicht unserer Klasse.  Reparatur *nachtragen* (empfohlen, ohne Datenverlust) setzt nur Bits; *freigeben*
   ist **nicht** vorausgewählt (dort könnten Daten einer Datei ohne Eintrag liegen: erst `recover`) und **gesperrt**, solange ein unbrauchbarer
   Eintrag existiert (sein Platzbedarf ist unbekannt).  *Eintrag löschen* (`bs600.eintrag.loeschen`) verwirft einen Verweis: `datenverlust`, nie
   vorausgewählt.  *Reorganisieren* als Reparatur (`bs600.luecke`) ist ebenfalls nicht vorausgewählt.  Rang in `applyRepairs`: Einträge (1) → reorganisieren (4)
   / Tabelle (3); Wächter `Bs600CheckReparatur.*`.
3. **Ein Fehler aus B3 ist hier aufgefallen und behoben:** `reorganize()` schrieb ein zu großes Ende in den Eintrag verschobener Dateien (`sektoren()`
   rechnet aus Anfang und Ende und wurde NACH dem Umsetzen des Anfangs gerufen).  Daten und Lesen waren unberührt (gelesen wird bis zur Endadresse),
   die Prüfung (`tabelle.fehlt`, `eintrag.ueberlappt`, `adresse.laenge`) zeigte es.  Wächter `Bs600Fs.ReorganizeSchliesstLueckenOhneDatenverlust` (prüft das Ende)
   und `Bs600Check.AngelegteDisketteIstSauber`.  Abzüge, die mit einem B3-Stand reorganisiert wurden, tragen den Fehler — die Prüfung findet ihn.
4. **Rettung = Rohbereich, nie ein Eintrag.**  Ein Eintrag trägt Name, Typ, Lage, Adressen, Datum, Signum; `erase` überschreibt alle 42 Byte mit Blanks (so auch angenommen
   fürs Original — B0 fand den Löschvorgang nicht [?]).  Zurück bleibt der Inhalt ohne Namen, Ladeadresse und Länge; die Adresse ist aus den Bytes nicht abzuleiten.
   `recoverScan`: Stufe *Verzeichnis* = Tabelle belegt ohne Eintrag (Güte *wahrscheinlich*), *Oberfläche* zusätzlich freie Sektoren ohne Füllmuster (*Bruchstück*, Grenzen geraten).
   `wiederherstellbar` ist immer `false`, `recoverRestore` lehnt mit Begründung ab.  Weg zurück: `recover --to` (Rohbytes ganzer Sektoren), dann `put --load ADR --entry ADR`.
   Wächter `Bs600Recover.*`.
5. **Urlader und Zielgerät** (`Bs600FileSystem::urladerKennung`/`urladerGegenWunsch`): `SYL FOR BS620` = PRG 710-1 (belegt: Abzug läuft am 710-1 bis zum Selbsttest),
   `BOOTSTRAP FOR BS600` = **PRG 700** (3-KB-ROM; B0: bootet am 710/710-1 nicht), `SYL FOR BS610` = PRG 710 ist **Annahme** (nur Webseite, kein Abzug).  PRG 700 steht
   NICHT im Rechnerwortschatz des Katalogs (Merkposten formatnamen Nr. 6) → das Gerät erscheint nur als Text: `info`/Diskettenangaben/`create --boot`/`boot-put`/
   Prüfbericht (`bs600.urlader.prg700`, Hinweis).  `--prg 700|710|710-1` prüft den Urlader VOR dem Schreiben (CLI; Wächter `Bs600Urlader.*`,
   `test_cli_prg_flag_binds_the_loader_to_its_device`).  Ein Urlader ist kein Mangel, wenn er fehlt (Datendiskette) → kein Befund.
6. **Katalog / Gegenprobe:** Format `ss40_1t26x128-15x256` und Profil `bs600_ss40_15x256_boot` bestehen `FormatKatalogSchema.*`/`FsKatalogSchema.*`; `machines: ["PRG 710","PRG 710-1"]`
   (Anzeige „150K · PRG 710 / PRG 710-1 · BS600 Systemdiskette”).  Beide Originale sind in `FsCheckGegenprobe.DerAusgelieferteKatalogIstEindeutig` aufgenommen; die Geometrie
   (Spur 0 26×128 + 15×256) gehört keinem anderen Profil, UDOS/SCPX/CPA bleiben unverdrängt (alle bisherigen Fixtures in derselben Liste).
7. **`put` ohne Angaben** (B5e, B3-Voreinstellung unverändert): Typ `P`, Ladeadresse `0000`, keine Startadresse, Datum heute, kein Signum — ein Programm ohne Ladeadresse ist **nicht lauffähig**.
   Angaben kommen aus `.fileinfo` (`fs=bs600`, schreibt `get` immer) oder den Schaltern; die Oberfläche fragt nicht (kein `INSERT_ANGABEN_FEHLEN` wie bei UDOS) — Eigenschaften nachtragen.
   `DiskVolume::insert` streicht aus dem Namen die Endung (`prog.bin` → `PROG`), damit auch Ziehen/Einfügen in der Oberfläche geht (das Dateisystem verbietet `.`).
8. **C-ABI (additiv):** `k1520d_set_bs600_attrs`, `k1520d_bs600_reorganize`, `k1520d_rename`, `k1520d_entry_extent`, `k1520d_bs600_urlader`; Python `DiskTool.set_bs600_attrs/bs600_reorganize/rename/bs600_urlader/ist_bs600`,
   `Entry.extent`.  Die Eintragsfelder gehen über die allgemeinen Getter (Typ = `entry_type`, Signum = `entry_created`, Lade/Ende/Start = `low_addr/high_addr/start`).  Wächter `test_disktool_bs600.py`.
9. **Oberfläche:** eigene Spalten (Name, Typ, Größe, Lage, Laden, Ende, Start, Datum, Signum); Eigenschaften-Dialog mit Name (= Umbenennen), Typ, Datum, Signum, Lade-/Startadresse
   (`ist_bs600` MUSS vor dem UDOS-Zweig stehen: `Entry.type` ist hier `P`, kein UDOS-Typ); Aktion *Diskette ▸ BS600: Dateien reorganisieren…* **ohne Tastenkürzel** (Kürzeltabelle des
   Handbuchs ist Vertrag), nur an beschreibbarer BS600-Diskette, mit Rückfrage; Neuanlage über `data/bootsektor.yaml` (zwei Urlader: BS620/PRG 710-1 und BOOTSTRAP/PRG 700 — nur Urlader, die Dateien
   kommen per *Alles einfügen*; **am Emulator nicht auf Start geprüft**, das ist B2).  Diskeditor öffnet die gemischte Geometrie (26×128 / 15×256) ohne Besonderheit.
10. **Offen:** BS610-Abzug (PRG 710) fehlt → Zuordnung `BS610 = 710` ungeprüft; Original-Löschvorgang und Reorganisationsprogramm unbekannt (Annahmen 3/4 oben); `L`-Typ ohne Beleg; keine Beispieldiskette im
    Paket (`disks/`) — Freigabe liegt vor (Entwurf 48 §2.5), Auslieferung ist AP-B6; Rechner „PRG 700” im Wortschatz erst, wenn ein Emulator dafür existiert.

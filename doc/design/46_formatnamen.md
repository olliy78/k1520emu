# Entwurf 46 — Einheitliche Format- und Profilnamen, Anzeige für Anwender

Stand 2026-11-04.  Zweig `formatnamen`.  Anlass: die Namen in `data/formats.yaml` folgen drei bis fünf verschiedenen
Schemata.  Manche verraten die Geometrie (`k5601_ds40_9x512`), andere nicht (`cpa780`, `cpa800`, `mf6400_600k`);
`cpa800` ist zudem irreführend, weil SCPX, MicroDOS und andere dieselbe Geometrie benutzen.  Format und Dateisystem
tragen teils denselben Namen (`cpa780`, `cpa800`, `scpx798`, `udos_ds77`, `udos_ss77`).

**Grundentscheidung (Anwender, 2026-11-04):** *Schlüssel* und *Anzeige* werden getrennt.  Der Schlüssel (Katalogname) ist
technisch, einheitlich und verrät die Geometrie.  Die Oberfläche zeigt dem Anwender Kapazität, Rechner und einen
Klartexttitel — er wählt nie anhand des Schlüssels.  Ein Name wie `scpx_ds80_16x256_boot` ist für Profis gedacht
(CLI, Skripte, Tests); „800K · PC 1715 · SCP-Systemdiskette" ist, was ein Anwender sucht, der eine Diskette für seinen
PC 1715 anlegen will.

## 1  Namensschema der Formate (Geometrie)

```
[fm_]<Seiten><Zylinder>_<Zonen>[_dstep][_id<N>][_hd|_ed]
```

| Teil | Bedeutung |
|---|---|
| `fm_` | ganze Diskette FM (nur 8″-Einfachdichte, `mf3200`); MFM ist der Normalfall und bleibt ungenannt |
| `ss` / `ds` + Zahl | Seiten (ein-/doppelseitig) + Zylinderzahl: `ss40`, `ds80`, `ds77` |
| Zone Daten | `<Sektoren>x<Bytes>`: `5x1024` |
| Zone Systembereich davor | `<n>t<Sektoren>x<Bytes>`, `n` = Zahl der **Spurseiten**, Zylinder-major gezählt; Zonen mit `-` getrennt: `3t26x128-5x1024` |
| Zonen-Kodierung | `fm` direkt hinter der Zone, wenn nur diese FM ist (`2t26x128fm-8x1024`) |
| `_dstep` | Doppelschritt (`step: 2`) |
| `_id<N>` | erste Sektor-ID, wenn ≠ 1 |
| `_hd` / `_ed` | Hochdichte 500 kbit/s bzw. extra-hoch |

Nie im Formatnamen: Laufwerk (`k5601_…`, das steht in `drives:`), OS, Kapazität.  Zeichenvorrat `[a-z0-9_-]`.

Beispiele: `cpa800` → `ds80_5x1024`; `cpa780` → `ds80_3t26x128-5x1024`; `scpx780` → `ds80_6t26x128-5x1024`;
`scp1700_640` → `ds80_1t16x128fm-16x256`; `mf3200` → `fm_ss77_4x1024`; `mf6400_600k` → `ss77_2t26x128fm-8x1024`;
`k5601_ds40_9x512_dstep` → `ds40_9x512_dstep`; `pc_hd_18x512` → `ds80_18x512_hd`.

**Folgen für den Bestand:** `scpx780` und `scpx780_b` haben identische Spurlisten → ein Eintrag; `cpa640` ≡ `k5601_16x256`
→ ein Eintrag (`ds80_16x256`).  Namen, die nach dem Schema zusammenfallen, müssen auch **gleiche Spurlisten** haben
(Wächter W2); der Katalog erzeugt den Namen nicht, er prüft ihn.

## 2  Namensschema der Dateisystemprofile (OS + Layout)

Ein Profil ist OS plus Layout, nicht Geometrie: dieselbe Geometrie trägt viele Profile (`cpa800` trägt sieben), sie
unterscheiden sich in Bootkopf, Verzeichnisgröße und Systemspuren.

```
<os>[_<rechner>]_<format ohne Systemzone>[_<rolle>]
```

- `<format ohne Systemzone>` kommt mechanisch aus dem Formatnamen; den Systembereich legt das OS fest.
- `<rechner>` nur, wenn der Rechner das Layout ändert (`ndos_p8000_…`, `scp_1715_…`).
- `<rolle>` ∈ `boot` (Systemspuren/Lader), `daten`, `leer`, `syl`, `kombi`.
- Die Kapazität steht nie im Namen.

| alt | neu |
|---|---|
| `cpa780` / `cpa800` | `cpa_ds80_5x1024_boot` / `cpa_ds80_5x1024_daten` |
| `scpx640` / `scpx798` | `scpx_ds80_16x256_boot` / `scpx_ds80_5x1024_boot` |
| `scp1715` | `scp_1715_ds80_5x1024_boot` |
| `microdos800` / `microdos800_daten` | `microdos_ds80_5x1024_boot` / `…_daten` |
| `udos_ds77` | `udos_ds77_26x128` |
| `udos1715` / `udos_p8000` / `udos50` | `ndos_ds80_16x256` / `ndos_p8000_ds80_16x256` / `ndos_a5120_ds80_16x256` |
| `wega720` | `wega_ds80_9x512` |
| `mutos8k_8x512_mfm` | `mutos8k_ss77_8x512` |
| `tar720` / `tar720_leer` | `tar_ds80_9x512` / `tar_ds80_9x512_leer` |
| `dos360` / `dos1440` | `dos_ds40_9x512` / `dos_ds80_18x512_hd` |

**Offen (Anwender entscheiden):** `udos1715` → `ndos_…` ändert den Familiennamen (NDOS ist, was der Gast fährt;
`type:` heißt weiter `udos1715`).  Alternative: `udos1715` bleibt Familienname.  Die vollständige Zuordnung alt → neu
steht in `data/formats_alias.yaml` (Schritt 1) und ist dort maßgeblich, nicht diese Tabelle.

**Vorab prüfen (Schritt 0):** `format:` ist bei `scp1715`, `scpx8915` und `microdos1715` `cpa800` (kein Systembereich),
die Beschreibung sagt „Systemzylinder 0-1".  Vermutlich gehört dort die `scpx780`-Geometrie hin.  Messen
(`k1520disktool measure` an den Fixtures), nicht raten; die Namen der betroffenen Profile hängen am Ergebnis.

## 3  Anzeige: drei neue Katalogfelder

| Feld | Ebene | Bedeutung |
|---|---|---|
| `title` | Format + Profil | Klartextname in Alltagssprache („SCP 1715 Systemdiskette") |
| `capacity` | Format + Profil | Marktbezeichnung als Anzeigewert („800K", „1,44M") — **nicht** die Bytezahl; die Bezeichnung weicht davon ab (780K/798K/800K) |
| `machines` | **nur Profil** | Rechner, für die die Diskette gedacht ist (`[PC 1715, PC 1715W]`) |

- Bei **Formaten** gibt es keine Rechnerliste: dieselbe Geometrie passt zu mehreren Rechnern, die Aussage hat erst auf
  Dateisystemebene Gewicht.  Formate zeigen Kapazität + Geometrie.
- **Leeres `machines:` heißt „unbekannt"**, nicht „alle".  Wo die Zuordnung nicht aus dem Bestand belegt ist (z. B. welche
  DOS-Formate zu A7150/PC 1834 gehören), bleibt das Feld leer und wird im Entwurf als offen geführt; **nicht raten**.
- `description` bleibt als Langtext erhalten; `title` ersetzt ihn in Listen.
- Rechnernamen sind ein **geschlossener Wortschatz** (`app/profil.py` kennt die Programme; Liste in `data/formats.yaml`
  oben dokumentiert), damit „PC 1715" nicht in drei Schreibweisen auftaucht.

### Oberfläche

- Auswahlzeilen: **`Kapazität · Rechner · Titel`**; der Schlüssel nur als Tooltip bzw. Detailzeile.
- **„Neue Diskette für …"**: zuerst der Rechner, dann nur dessen Disketten.  Der Emulator kennt seinen Rechner
  (`Programmprofil`) und wählt vor.
- Sortieren/Filtern nach Kapazität und Rechner.
- Betroffen: Emulator (`app/ui/format_dialog.py`, `app/ui/drive_widget.py`) und DiskTool (Neu-Dialog, `create --fs`,
  `fs`-/`formats`-Listen der CLI).  CLI zeigt dieselben Spalten, nimmt aber weiterhin den Schlüssel.

## 4  Aliase (Rückwärtsverträglichkeit)

Die alten Namen stehen in über 350 Fundstellen (Tests, C-ABI-Aufrufe, gespeicherte Konfigurationen `a5120emu.yaml`,
Skripte, Doku, Lieferdisketten-Bau).  Deshalb:

- Katalogfeld **`aliases: [alt1, alt2]`** an Format und Profil; Auflösung an EINER Stelle (`FormatCatalog::find`,
  `FsCatalog::find`), jeder Treffer über einen Alias liefert den **neuen** Namen zurück (`name()`).
- Ausgaben (CLI, GUI, Protokoll, gespeicherte Konfiguration) schreiben ab dem Umbau **nur noch den neuen Namen**;
  eine alte Konfiguration wird beim Laden stillschweigend umgeschrieben (Migrationskette `config_io._MIGRATIONEN`).
- Aliase sind **dauerhaft**, nicht „bis zum nächsten Release": Konfigurationen und Skripte von Anwendern sind nicht
  unter unserer Kontrolle.
- Ein Alias darf nie einen echten Katalognamen verdecken (Wächter W3).

## 5  Schritte

| Schritt | Inhalt | Prüft |
|---|---|---|
| **0** | Die Profile `scp1715`/`scpx8915`/`microdos1715` (und was sonst auffällt) nachmessen; Ergebnis hier eintragen | Messung an Fixtures |
| **1** | Katalog: `aliases:` lesen (Format + Profil), Auflösung in `find`, Wächter W3; **noch keine Umbenennung** | `FormatKatalogAlias.*` |
| **2** | Formate umbenennen (Schema §1), Duplikate zusammenlegen, alte Namen als Aliase; Test-/Skriptfundstellen mechanisch nachziehen (`sd`, je Treffer prüfen) | W1, W2, volle `test`-Runde |
| **3** | Dateisystemprofile umbenennen (Schema §2), `format:` nachziehen, Aliase | W1, W4 |
| **4** | Felder `title`/`capacity`/`machines` im Katalog lesen, durch C-ABI reichen (additiv: `k1520_format_title/_capacity`, `k1520d_fs_title/_capacity/_machines`), Python-Bindung, `test_c_api.py` | W5, `py_c_api` |
| **5** | Katalog füllen: alle Titel + Kapazitäten; Rechner nur, wo belegt | W5, W6 |
| **6** | Oberfläche: Auswahlzeilen, „Neue Diskette für …", Filter (Emulator + DiskTool), CLI-Spalten; Handbuchkapitel anpassen | `py_*_gui`, Handbuch-Wächter |
| **7** | Doku: `README`-Hinweise, `doc/format.md`, Merkposten `doc/merkposten/formatnamen.md`, CLAUDE.md (Abschnitt Katalog) | `py_keine_umlaute` etc. |
| **8** | Gesamtlauf `tools/dev.sh test-merge --alles` (+ `win`: Katalogpfade, Umlaute) | Gesamt |

Je Schritt ein Commit; nach 2 und 3 ist der Baum für sich lauffähig (Aliase fangen den Rest).  Schritte 4–6 hängen
nicht an 2/3 und lassen sich auch vorher ausführen, falls die Umbenennung wartet.

## 6  Wächter

| # | Wächter | Hält fest |
|---|---|---|
| W1 | `FormatKatalogSchema.*` (unit) | **Jeder** Formatname entspricht dem Schema aus §1 und beschreibt seine eigene Spurliste: Name wird aus `tracks`/`encoding`/`step`/`rate`/`first_sector` neu berechnet und muss stimmen.  Ein Name, der lügt, fällt hier auf |
| W2 | `FormatKatalogSchema.KeineDoppeltenSpurlisten` | zwei Formate mit identischer Spurliste + Optionen sind ein Fehler (nach dem Umbenennen fallen `scpx780_b` und `cpa640` sonst unbemerkt wieder an) |
| W3 | `FormatKatalogAlias.*`, `FsKatalogAlias.*` | Alias löst auf den neuen Namen; kein Alias verdeckt einen Katalognamen; kein Alias doppelt; **jeder alte Name von heute löst weiterhin auf** (eingefrorene Liste `tests/fixtures/formatnamen_alt.txt`) |
| W4 | `FsKatalogSchema.*` | Profilname = `<os>[_<rechner>]_<Formatname ohne Systemzone>[_<rolle>]`, `format:` zeigt auf ein existierendes Format, Rolle aus dem geschlossenen Wortschatz |
| W5 | `KatalogAnzeige.*` | jedes Format und Profil hat `title` und `capacity`; kein `capacity` ohne Einheit; `machines` nur aus dem geschlossenen Wortschatz; leeres `machines` ist erlaubt und bedeutet „unbekannt" |
| W6 | `py_katalog_anzeige` | Auswahllisten der GUI zeigen Kapazität + Rechner + Titel, nie den Schlüssel allein; „Neue Diskette für <Rechner>" liefert nur Profile dieses Rechners; der Schlüssel steht im Tooltip |
| W7 | `py_c_api` (erweitert) | neue C-ABI-Funktionen in Header ↔ Bibliothek ↔ ctypes deckungsgleich |
| W8 | `py_config_io` (erweitert) | eine Konfiguration mit alten Formatnamen wird geladen und mit neuen zurückgeschrieben |

Jeder neue langsame Test braucht ein `fach_*`-Label (Wächter `py_testauswahl`); `tools/test_auswahl.py` kennt
`data/formats.yaml` bereits als gemeinsames Fach — gegebenenfalls einen Eintrag für `data/formats_alias.yaml` ergänzen.

## 7  Nicht-Ziele / Risiken

- **Kein Neuformatieren oder Umbenennen vorhandener Disketten**: die Namen sind Katalogschlüssel, in Abbildern steht
  kein Formatname (Ausnahme: ein im `.hfe` mitgeführter Katalogname — in Schritt 2 prüfen, ob die Container so etwas
  schreiben; wenn ja, Aliasauflösung beim Lesen zwingend).
- **Der Katalog erzeugt keine Namen**, er prüft sie (W1).  Ein Generator würde Fehler in der Spurliste unsichtbar
  zu einem „richtigen" Namen machen.
- **`k1520_format_description` und `k1520d_fs_description` bleiben** (additive C-ABI), die GUI wechselt auf `title`.
- **`machines:` ist Information, keine Einschränkung**: eine Diskette bleibt auf jedem Rechner mountbar, dessen
  Laufwerk sie darstellen kann (`drives:` entscheidet das).
- Bei den Dateisystemprofilen `microdos*`/`bcu880*` ist die Rollenbezeichnung (`syl`/`kombi`) Eigenbau-Wortschatz; vor
  Schritt 3 mit den Merkposten `microdos.md` und der BCU-880-Dokumentation abgleichen.

## 8  Messergebnis Schritt 0

Gemessen mit `k1520disktool measure`/`info` an Kopien der Fixtures (2026-11-04).

| Profil | Fixture | gemessene Spuren | trifft Katalogformat | `format:` im Profil | stimmt? |
|---|---|---|---|---|---|
| `scp1715` | `pc1715_scp1715_v0006_boot.hfe` | c0-79 h0-1, alle 5×1024 MFM | `cpa800` | `cpa800` | ja |
| `scp1715` (SCP 3.0) | `pc1715w_scp30_system.hfe` | c0-79 5×1024, c80-81 unformatiert | `cpa800` (4 unformatierte Spuren) | `cpa800` | ja |
| `scpx8915` | `k8915scpx_boot1.hfe` | c0-79 5×1024 (1 Sektor mit CRC-Fehler) | `cpa800` | `cpa800` | ja |
| `cpa1715` | `pc1715_cpa1715_boot_4lw.hfe` | c0-79 5×1024 | `cpa800` | `cpa800` | ja |
| `microdos1715` | `pc1715_microdos_k5601_system.hfe` | c0-79 5×1024 | `cpa800` | `cpa800` | ja |
| `microdos800` | `a5120_microdos26_k5601_seriell.hfe` | c0-79 5×1024 | `cpa800` | `cpa800` | ja |

Keine der Disketten hat 128-B-Systemspuren: auch die Systemzylinder 0-1 von SCP 1715, SCP 3.0 und SCPX 8915 sind
durchgehend 5×1024 MFM; die `scpx780`-Geometrie (Spur 0-2 mit 26×128) trifft **keine** davon.  `format: cpa800` ist
also richtig; falsch ist nur die Beschreibung „Systemzylinder 0-1" im Sinne einer eigenen Geometrie.  Die
Systemzylinder sind ein Merkmal des Dateisystemprofils (`data_start: cyl 2`), nicht des Formats.  Vorschlag: `format:`
bei allen sechs unverändert lassen, keine Umhängung auf `scpx780`; die Profilnamen dürfen daher das Format
`ds80_5x1024` tragen (`scp_1715_ds80_5x1024_boot`, `scpx_8915_ds80_5x1024_boot`).  Die 1715W-Fixture hat zwei
zusätzliche unformatierte Zylinder (80-81), die das Format nicht ändern.

## 9  Zusatzaufgaben (vom Anwender, 2026-11-04)

Unabhängig vom Namensumbau (§1–§8); können vor, nach oder neben den Schritten 2–8 laufen.  Neue Aufgaben des
Anwenders werden hier fortlaufend angehängt (Z1, Z2, …).

### Z1  Statuszeile des DiskTools zeigt die Kapazität

Heute (`app/disktool/ui/main_window.py::_inhalt_anzeigen`) steht dort `N Dateien · X KB frei` (je Volume, bei
zweiseitigen UDOS-Disketten mit Seitenpräfix).  Gewünscht: **„7 Dateien - 64 KB von 640 KB frei"**.

- **Einzeldiskette:** frei von gesamt, aus `FsInfo::free_bytes`/`total_bytes` (`core/filesystem/file_system.h`, liegt
  schon vor; die Python-Bindung reicht heute nur `free` durch — `total` bzw. `used` ergänzen, additiv in der C-ABI
  `k1520d_*`, `test_c_api.py` mitziehen).
- **Mehrseitige Disketten (UDOS ds77):** je Seite „A: 64 KB von 320 KB frei"; die Gesamtkapazität ist die Summe, die
  Anzeige bleibt je Seite (jede Seite ist ein eigenes Dateisystem).
- **tar-Satz über mehrere Disketten:** die Anzeige bezieht sich auf das **Medium**, nicht auf ein Abbild.  Kapazität =
  `TarSatzPlan::kapazitaet` (Nutzbytes EINES Abbilds) × Zahl der Abbilder, Belegung = Länge des tar-Stroms
  (`TarSatzPlan::strom`).  Zusätzlich die Zahl der Disketten: „12 Dateien - 1,2 MB von 2,1 MB belegt (3 Disketten)".
  Bei einem *abgeschlossenen* Archiv gibt es keinen freien Platz im Sinn eines Dateisystems (V7-Neuaufbau, Entwurf 37):
  Anzeige dann „belegt von Kapazität", nicht „frei".  Leeres tar-Archiv: „0 Dateien - 0 KB von 720 KB".
- **P8000-Plattenabbilder / Dateisysteme ohne Kapazitätsangabe** (`total_bytes == 0`): wie bisher nur „X KB frei",
  nie „von 0 KB".
- **Einheiten:** KB bis 1023 KB, darüber MB mit einer Nachkommastelle (`1,2 MB`; deutsche Schreibweise, Komma).  Die Zahl
  in der Statuszeile ist **Nutzkapazität des Dateisystems**, nicht die Marktkapazität (`capacity:` aus §3, „800K") —
  beide dürfen auseinanderfallen (Tooltip nennt die Marktbezeichnung, sobald §3 steht).
- **Nicht vollständig geladene physische Diskette:** wie heute erst anzeigen, wenn alles beisammen ist
  (`_inhalt_anzeigen` wird erst am Ende aufgerufen); bis dahin keine Teilzahl.
- **Wächter Z1:** `py_disktool_statuszeile` — Einzeldiskette (CP/M `cpa_…_daten`: Format „N Dateien - X KB von Y KB frei",
  X ≤ Y), zweiseitige UDOS (je Seite), tar-Satz mit 2 Abbildern (Summe, Zahl der Disketten), Dateisystem ohne
  Kapazität (kein „von 0 KB"), roh geöffnet (Text unverändert).  Dazu ein Formatierer-Test für Einheiten/Komma
  (`1023 KB`, `1,0 MB`, `0 KB`).  Der Formatierer gehört in eine eigene reine Funktion (kein Qt), damit er ohne GUI
  prüfbar ist.
- **Zuständig:** einfache Umsetzung, Sonnet-Agent; der tar-Satz-Fall braucht vorher einen Blick auf
  `DiskVolume::tarSatzPlan` bzw. das offene Satzobjekt, wie die Kapazität des *geöffneten* Satzes (nicht des Plans)
  zu holen ist.

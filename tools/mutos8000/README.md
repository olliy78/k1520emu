# MUTOS 8000: Auswertung von M8KROOT und Nachbau der Wurzeldiskette

Werkzeuge zu `doc/design/39_mutos8000.md` (§4.6.3 Blockabbildung, §4.7.6 Befund M8KROOT, §4.8 rekonstruierte Wurzel).
Nur Python-Standardbibliothek, lauffähig mit `python3 -I`; das DiskTool wird nur über `tools/dev.sh tool k1520disktool` gerufen.

| Datei | Zweck |
|-------|-------|
| `m8k.py` | gemeinsame Bausteine: TD0-/IMD-Leser, `Mutos8kDev` (Verschränkung 2, Spurversatz 6), Rückschieben um 1 Bit, Inode-/Verzeichnisleser |
| `m8kroot_lesen.py` | Befund der beschädigten `M8KROOT` (Superblock, Inodes, Baum, lesbare Bytes; `--inodes`, `--vergleich ORDNER`) |
| `wurzel_quellbaum.py` | Quellbaum (Dateien, `.fileinfo`, `wega-dateiangaben.txt`) aus `M8KROOT` + Auszug von DISK-ROOT; Handangaben (Namen, `.profile`, Geräte) stehen als Daten im Skript |
| `wurzel_pruefen.py` | unabhängiger Leser: Baum einer Wurzel (`.td0`/`.imd`) byteweise gegen einen Quellbaum |
| `bau_wurzel.py` | ganzer Nachbau: DISK-ROOT holen, Quellbaum, `create --fs mutos8k_fm --inodes 104 --blocks 250` (Swap-Bereich ab Block 250, §4.8), Zylinder 0 per `boot-get` von M8KCMD + `boot-put`, `put`, `check --full`, Prüfung |
| `bau_aus_archiv.py` | baut die Wurzeldiskette aus einem **Archiv** (`.zip` oder ausgepackter Ordner): `bauangaben.yaml`, `zylinder0.bin`, `dateien/`; `check --full` und unabhängige Prüfung (Fehler bei Block >= `s_fsize`) |
| `archiv_erzeugen.py` | erzeugt die eingecheckten Quell-Archive aus den Disketten (`archive --ohne-abbild`): `--art wurzel` (Vorgabe) → `…_m8k_root.zip`, `--art ladediskette` → `…_boot_udos.zip` aus der Original-Ladediskette `…_boot_udos.dmk` |
| `bauangaben.yaml` | Vorlage der Bauangaben (fs, `fs_mfm`, Inodes, Blöcke = Swap-Grenze, Ladecode-Datei) |
| `bau_ladediskette.py` | baut die **UDOS-Ladediskette** aus dem Archiv `a5120_mutos8000_boot_udos.zip` (oder einem ausgepackten Ordner) je Laufwerksvariante; s. u. |
| `bauangaben_ladediskette.yaml` | Vorlage der Bauangaben der Ladediskette (Profile, Muster der Laufwerkstyp-Konstante, Varianten); `archiv_erzeugen.py --art ladediskette` legt sie ins Archiv |

```sh
python3 -I tools/mutos8000/m8kroot_lesen.py                  # Befund nach §4.7.6
python3 -I tools/mutos8000/bau_wurzel.py --vergleich         # Nachbau nach out/mutos8000/, Vergleich mit der eingecheckten .hfe
```

`bau_wurzel.py` arbeitet an Kopien in einem Temp-Ordner und überschreibt die eingecheckte Diskette nie ohne `--ueberschreiben`.
Die Programme aus DISK-ROOT werden nicht eingecheckt; sie entstehen beim Bau aus `disks/a5120_mutos8000/…_root.td0`.

**Gemessen** (aus `M8KROOT`, rückgeschoben): Baum, Modi, uid/gid, Gerätenummern, voll lesbare Dateien, Zeiten der Inodes am Sektoranfang.
**Abgeleitet**: Namen, die letzten 28 Byte von `.profile`, Bit 0200 der Modi, Zeiten der Inodes am Sektorende (Zeit des Nachbarn), `etc/mount`/`umount` (Ersatz aus DISK-ROOT).
Einzelheiten: Entwurf 39 §4.7.6 und §4.8.

Nachbau gegen `…_m8k_root_rekonstruiert.hfe`: Dateibaum, Inhalte und `ls -l` (Modus, uid/gid, Größe, Zeit) sind gleich; Prüfer: 43 Einträge, 0 Abweichungen.
Wächter: `tests/python/test_mutos8000_werkzeuge.py` (Befund-Leser, Archiv → Bau → Vergleich, getauschte Datei).

## Ablauf: Datei ändern → neue Wurzel (Ticket #22)

Die Wurzel wird aus dem eingecheckten Archiv `disks/a5120_mutos8000/a5120_mutos8000_m8k_root.zip` gebaut.
Es trägt **kein Abbild** (`--ohne-abbild`): ein angepasstes Archiv soll nie neben einer veralteten Diskette stehen; die Diskette entsteht stets neu.

1. Auspacken: `unzip disks/a5120_mutos8000/a5120_mutos8000_m8k_root.zip -d meine_wurzel`
   (`bauangaben.yaml`, `zylinder0.bin`, `dateien/` samt `.fileinfo` je Datei und `dateien/wega-dateiangaben.txt` für Geräteknoten, FIFOs und die Wurzel).
2. Datei unter `meine_wurzel/dateien/` tauschen (z. B. `etc/passwd`, `etc/ttys`, `.profile`). Rechte, Eigentümer und Zeit stehen im danebenliegenden `.fileinfo`; eine neue Datei braucht ein eigenes `.fileinfo` (sonst Modus 0644/0755, Besitzer 0/0, Zeit jetzt).
3. Bauen: `python3 -I tools/mutos8000/bau_aus_archiv.py meine_wurzel meine_wurzel.hfe` (statt des Ordners geht auch ein `.zip`). Die eingecheckte Diskette ist nur mit `--ueberschreiben` als Ziel erlaubt, ebenso ein vorhandenes Ziel.
4. Ausprobieren: `meine_wurzel.hfe` im Emulator als Laufwerk C: mit dem Format `udos_ss77` einlegen (Entwurf 39 §2/§3a).
5. Optional wieder als Archiv ablegen: `python3 -m app.disktool.archive meine_wurzel.hfe neu.zip --ohne-abbild --bezeichnung m8k_root --beilage meine_wurzel/bauangaben.yaml --beilage-binaer meine_wurzel/zylinder0.bin`.

`bloecke: 250` ist keine Platzfrage: ab Block 250 liegt der Auslagerungsbereich des Kernels (rk2, Entwurf 39 §4.8). Der Bau bricht ab, wenn ein Block >= `s_fsize` belegt wäre.
Das Archiv entsteht aus der eingecheckten Diskette mit `python3 -I tools/mutos8000/archiv_erzeugen.py --force` (nur nötig, wenn die Diskette selbst geändert wurde).

**Bitgleichheit:** zwei Bauten aus demselben Archiv sind bis auf die Anlagezeit (`s_time` im Superblock, Zeiten der Inode 1) gleich. Voraussetzung ist, dass `put` die Verzeichnisse sortiert anlegt; zuvor bestimmte die Reihenfolge des Dateisystems, welches Verzeichnis welchen Block bekam.

### Wurzel für MFM-Laufwerke (K5601)

`python3 -I tools/mutos8000/bau_aus_archiv.py --laufwerke k5601 <archiv> <ziel.hfe>` baut dieselbe Wurzel in **MFM** statt FM
(Profil `mutos8k_mfm` aus `fs_mfm` der Bauangaben; Vorgabe `--laufwerke mf3200` = FM, `mutos8k_fm`). Blockabbildung (Verschränkung 2,
Spurversatz 6) und Inhalt sind gleich, nur das Aufzeichnungsverfahren der 77 × 26 × 128 Sektoren ist ein anderes (Entwurf 39 §4.6.3).
Eingecheckt: `…_m8k_root_rekonstruiert_mfm.hfe`. Die Anlagezeit (`s_time`) ist das Einzige, was zwei Bauten unterscheidet.

## Ladedisketten aus dem Archiv bauen (Ticket #22)

Die UDOS-Ladediskette (Lade-Menü, `BOOT MUTOS`, `FORM.*`, `COPY.*`, Kernel `mutos_1216`, Treiber `mutos_io`) wird wie die Wurzel
**aus Dateien und Metaangaben** gebaut, nicht sektorweise kopiert. Quelle ist `disks/a5120_mutos8000/a5120_mutos8000_boot_udos.zip`
(`archive --ohne-abbild` der Original-Ladediskette `…_boot_udos.dmk`):

* `dateien/`: 18 UDOS-Dateien mit `.fileinfo` je Datei und dem Beiblatt `udos-dateiangaben.txt` (Typ, Eigenschaften, Einsprung, Satzlänge,
  Segmente, Speicheranforderung, Erstell-/Änderungsvermerk). Das 19. Objekt, `DIRECTORY`, entsteht mit der Diskette.
  Leere Datumsvermerke stehen als `______` (nicht `-`, das hieße bei `put` „heute“ und machte den Bau tagesabhängig).
* `systemspuren.bin`: Spur 0–2 mit dem SYL-Ladekopf und die Bootspur (`boot-get`, 104 Sektoren zu 132 B mit Sektorkontrollblock).
* `bauangaben.yaml`: Profile je Aufzeichnung (`fs_fm: udos_ss77_fm`, `fs_mfm: udos_ss77`), Muster der Laufwerkstyp-Konstante, Varianten.

**Die Laufwerkstyp-Konstante** (Entwurf 39 §9.13) steht in den Systemspuren: `LD A,2AH / AND D / LD (0FCAH),A`, Zylinder 2, Sektor-ID 1,
Offset 120 (`3EH`) / 121 (Wert). 2 Bit je Laufwerk: 2 = 77 Spuren FM, 3 = 77 Spuren MFM. Der Bau sucht `3E ?? A2 32 CA 0F` in `systemspuren.bin`
(**genau eine** Fundstelle, sonst Abbruch), prüft den Wert gegen `typ_erwartet` (`2A`) und ersetzt ihn erst dann.

| `--laufwerke` | Aufzeichnung | Profil | Konstante | Laufwerke A: / B: / C: |
|---|---|---|---|---|
| `mf3200` | FM | `udos_ss77_fm` | `2AH` | alle 8″ MF3200 (Auslieferung) |
| `k5601` | MFM | `udos_ss77` | `3FH` | alle 5¼″ K5601 |
| `mf3200_k5601` | FM | `udos_ss77_fm` | `3EH` | A: MF3200 (Diskette liegt in A:), B:/C: K5601 (Wurzel dort MFM) |

```sh
python3 -I tools/mutos8000/bau_ladediskette.py disks/a5120_mutos8000/a5120_mutos8000_boot_udos.zip meine_lade.hfe --laufwerke k5601
```

Ablauf: `create` im Profil, `boot-put` (gepatchte Systemspuren), `put dateien/`, `check --full` (ohne Befund), Gegenprobe (Aufzeichnung laut `info`,
Konstante zurückgelesen, jede Datei byteweise gleich dem Archiv). Ziel nur `.hfe` (der DMK-Schreiber macht bei FM falsche CRCs); ein vorhandenes
Ziel — auch eine eingecheckte Diskette — nur mit `--ueberschreiben`. Es gibt **keine festen Orte**: OS, ZDOS, OS.INIT usw. liegen auf den gebauten
Disketten an anderen Sektoren als auf dem Original (der Urlader findet sie über das Verzeichnis), und die Disketten booten trotzdem
(`cli_mutos8000_boot_ladediskette_*`). Zwei Bauten aus demselben Archiv sind **bitgleich** (kein Zeitstempel).

**Datei ändern → neue Diskette:** Archiv auspacken (`unzip … -d meine_lade`), Datei unter `meine_lade/dateien/` tauschen (z. B. `OS.INIT`, das Lade-Menü; Angaben
stehen im `.fileinfo` daneben), `bau_ladediskette.py meine_lade neu.hfe --laufwerke …`. Danach im Emulator in A: einlegen, Laufwerke passend
(`k1520dbg --em em256 --laufwerke K5601,K5601,K5601 neu.hfe`), `BOOT MUTOS`, Wurzel erst NACH der Aufforderung in C: einlegen (`eject A`, `mount C … udos_ss77`, `r`).
Das Archiv entsteht aus der Original-Ladediskette mit `python3 -I tools/mutos8000/archiv_erzeugen.py --art ladediskette --force`.
Eingecheckt sind die drei Varianten `…_boot_udos_mf3200.hfe`, `…_boot_udos_k5601.hfe`, `…_boot_udos_mf3200_k5601.hfe`.

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
Wächter: `tests/python/test_mutos8000_werkzeuge.py` (Befund-Leser, ohne DiskTool).

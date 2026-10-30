# tools/sios_a5120 — SIOS-1526-Disketten des A5120

| Datei | Zweck |
|-------|-------|
| `original/` | unveränderte Originalabbilder (Herkunft und Prüfsummen: `original/README.md`) |
| `build.py` | leitet daraus die Beispieldisketten `disks/a5120_sios1526/` ab (Python-Standardbibliothek + `k1520disktool`) |
| `sylp_bereinigen.py` | 40-Spur-Arbeitsabbild aus dem 80×2 ausgelesenen `SYLp` (von `build.py` benutzt) |
| `dump.py` | Sektordump und Etikettenleser (`bild`, `labels`, `get`) |
| `gen_prn.py` | Listing-Erzeugung für die Analyse |

## build.py

```sh
python3 tools/sios_a5120/build.py --tool build/k1520disktool            # bauen (idempotent, überschreibt)
python3 tools/sios_a5120/build.py --tool build/k1520disktool --check    # nur vergleichen (Wächter cli_sios_a5120)
python3 tools/sios_a5120/build.py --tool build/k1520disktool --out /tmp/x   # statt disks/
```

Erzeugt nach `disks/a5120_sios1526/` (Namensschema `a5120_sios<version>_<anzahl>x<laufwerk>_<tastatur>.hfe`):

| Ergebnis | Quelle | Umwandlung |
|----------|--------|------------|
| `a5120_sios11_3xk5601_seriell.hfe` | `SIOS1526_SYLs.hfe` | bytegleich |
| `a5120_sios11_3xk5601_seriell_sl1.hfe` | `SIOS1526_SL1_1.img` | `k1520disktool save-as` (rohes Abbild → `.hfe`; F8-Marken fehlen) |
| `a5120_sios09_3xk5601_parallel.hfe` | `SIOS1526_SYLp.hfe` | bytegleich |
| `a5120_sios09_3xk5600_10_parallel.hfe` | `SIOS1526_SYLp.hfe` | `sylp_bereinigen.py` (Seite 0, Zylinder 0..39) |
| `a5120_sios_programme1.hfe` / `_programme2` / `_generierung` | `DP1` / `DP2` / `SG1` | bytegleich |

Es werden **keine Bytes des SIOS-Codes verändert**.  Wer eine neue Bestückung belegt hat, trägt sie in `DISKETTEN` ein, den Ordner in
`tools/disketten_beigaben.py` (`ORDNER`) und einen Wächter in `tests/integration/test_sios_boot.cpp` (`SiosBeispiel.*`).  Die Matrix der geprüften und
der nicht laufenden Bestückungen steht in `doc/sios/bedienung.md` §7.

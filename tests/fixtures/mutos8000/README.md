# Fixtures MUTOS 8000 (eingefroren)

Herkunft: Projekt `~/projects/mutos8k` (Import-Commit 104536c); dort liegen Quellen, Forschung und die
Werkzeuge, die aus diesen Originalen die Disketten in `disks/a5120_16_MUTOS/` bauen.  Hier nur Kopien,
die Emulator-Tests als Eingabe brauchen -- nicht pflegen, bei Bedarf aus mutos8k neu kopieren.

| Datei | Verwendet von |
|-------|---------------|
| `mutos_1216` (Kernel, 36352 B), `kernel_karte.txt` | `tests/python/test_z8kdis_kernel.py` |
| `mutos_io` (U880-Treiber), `mutosio_quelle/karte.txt`, `mutosio_quelle/src/mutosio.mac`, `mutosio_quelle/mutosio.orig` | `test_z80_disasm.py`, `MacListing.AssemblesMutosIoBytegleichZumOriginal` |
| `a5120_mutos8000_boot_udos.dmk` | `cli_mutos8000_boot_rekonstruierte_wurzel` |
| `a5120_mutos8000_{cc1,cc2,root}.td0` | `cli_mutos8000_boot_ladediskette_k5601_8x512`, `cli_dt_mutos_8x512_erkennung*` |

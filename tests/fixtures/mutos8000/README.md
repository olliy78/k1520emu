# Fixtures MUTOS 8000 (eingefroren)

Herkunft: Projekt `~/projects/mutos8k` (Import-Commit 104536c); dort liegen Quellen und Forschung.  Der
Diskettenbau aus den Originalen (`disks/a5120_mutos8000/`) liegt hier (`tools/mutos8000/bau_alle.py`).  Hier nur
Kopien, die Emulator-Tests als Eingabe brauchen -- nicht pflegen, bei Bedarf aus mutos8k neu kopieren.
Die Disketten-Originale (boot_udos, cc1, cc2, root) liegen NICHT hier, sondern in `disks/a5120_mutos8000/`.

| Datei | Verwendet von |
|-------|---------------|
| `mutos_1216` (Kernel, 36352 B), `kernel_karte.txt` | `tests/python/test_z8kdis_kernel.py` |
| `mutos_io` (U880-Treiber), `mutosio_quelle/karte.txt`, `mutosio_quelle/src/mutosio.mac`, `mutosio_quelle/mutosio.orig` | `test_z80_disasm.py`, `MacListing.AssemblesMutosIoBytegleichZumOriginal` |

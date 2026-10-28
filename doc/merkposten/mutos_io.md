# Merkposten mutos_io (Entwurf 40)

Vor Arbeiten an `tools/mutos8000/mutos_io/` lesen.  Weg und Werkzeuge: `tools/mutos8000/mutos_io/README.md`; Semantik: `mutos_io_analyse.md`.

1. **`src/mutosio.mac` ist der Master**, nicht die Karte und nicht die Datei auf der Diskette.  Die Karte (`karte.txt`) belegt nur den Ausgangsstand.
   Wächter: `cli_mutosio_bin_passt_zur_quelle` (eingechecktes `mutosio.bin` == Bau), `cli_mutosio_gleicht_dem_archiv` (Erstbau == Archivdatei und -`.fileinfo`,
   Variante `K8X512` == `mutos_patch io-8x512`), `py_mutos_io_pipeline` (Änderung -> Diskette -> Bildschirm).
2. **Segmentlängen stehen nur in der `.fileinfo`.**  Die Datei ist Segment für Segment auf 256 Byte mit FFH gefüllt (`SEGnnnnE`-Marken).  Wer Code einfügt, ändert `segs=`,
   `segment=`, `zusatz=` - `ladediskette.py` rechnet sie aus dem M80-Listing; HIGH in `mem=` (CB68) darf nicht unter das Segmentende fallen (Abbruch).
3. **Bei jeder Änderung `mutosio.bin` neu einchecken**, sonst `build.py --check` rot (gewollt).  Das Archiv-Original bleibt unverändert; ausgelieferte Disketten (`disks/a5120_16_MUTOS/`)
   erst mit `bau_alle.py` neu bauen, wenn die Änderung ausgeliefert werden soll.
4. **Kein Byte ohne Nachweis ändern, das ein Patch (`mutos_patch.py`) kennt:** die Patchlisten sind die Beschreibung der bisher gemessenen Änderungen; neue Varianten als `IF`-Zweig +
   Gleichheitsbeweis gegen den Patch (Muster `K8X512`).
5. **Tote Bytes gehören zur Bytegleichheit:** `SEKTOR_BERECHNEN` trägt in der 8 x 512-Fassung nach `JP SB_FERTIG` ein `DB 99H` und den toten Rest der alten Anweisung - nur so ist die Fassung
   bytegleich zum Patch.  Aufräumen wäre ein Fachentscheid (ändert die Datei), kein Schönheitsfehler.
6. **Befunde, die nicht der Erwartung entsprechen** (alle in `mutos_io_analyse.md` §1): Fertigmeldungen schreiben ACH nicht (nur den Vektor); SIO sendet nur über Kanal A, auch für tty2/tty3; Roh-Floppy (Klasse 3,
   Port 1CH, Block 01B5H im Nukleus) wird im Betrieb nie benutzt und zerstört beim Anstoßen 8 Byte Nukleus.
7. **Werkzeuge:** ein Disassembler (`tools/z80_disasm2.py`, `--format mac --karte`); Debugger-Ausbau für diese Arbeit siehe `tools/k1520dbg.md` (Abdeckung `cover`, Z80-only-Trace, `em msg`).

# k1520tape — Kassettenbänder (`.k52`)

Kommandozeile für das Bandformat aus `core/peripherals/kassette/k52_codec.h`. Nur Rohblöcke und
Bootband; FEX-Dateiformate sind nicht belegbar (`doc/kassette/treiber.md` §6.2).

```
k1520tape create   <band.k52>
k1520tape ls       <band.k52> [--side A|B]
k1520tape put      <band.k52> <datei> [--side A|B] [--block N] [--no-mark] [--format F]
k1520tape get      <band.k52> <datei> [--side A|B] [--file N] [--as F --addr A [--start A] [--name N] [--type C]]
k1520tape bootband <binaer> <band.k52> [A|B] [--format F]
```

**JKCEMU-Programmdateien (`--format F`, `--as F`):** F = `auto|roh|headersave|kcc|kctap|hex`.
`put`/`bootband` streifen den Kopf ab (Auto erkennt Headersave, KC-TAP, Intel-HEX am Inhalt, KCC an
der Endung `.kcc/.kcb/.kcm/.jtc`) und legen die Nutzbytes aufs Band; `get --as` setzt den Kopf davor
(`--addr` Pflicht). `bootband` verlangt bei Formaten mit Adresse **0400H**. Beschreibung und Grenzen:
`doc/kassette/jkcemu_formate.md`.

- `put`: Datei als Blöcke zu je N Datenbytes (Vorgabe 128, 1..256; letzter mit E5H aufgefüllt), danach
  eine Bandmarke (`--no-mark` lässt sie weg). Bei vollem Band bleibt das Band unverändert.
- `get`: Blockfolge zwischen Bandmarken (Datei Nr. `--file`, ab 0) zurückschreiben; der Auffüllrest
  bleibt erhalten, ein Block mit falscher CRC bricht ab.
- `bootband`: neues Band mit 4 × 128 B (Datei auf 512 B aufgefüllt), wie das Lade-ROM es erwartet
  (`treiber.md` §6.1): Ladeadresse 0400H, `SYL` am Anfang (wird geprüft), Einsprung bei Versatz 34H.
- Lücken: erste = Initiallücke der Geometrie, weitere 10 mm [?] (nicht belegt).
- Exit: 0 ok, 1 Fehler.

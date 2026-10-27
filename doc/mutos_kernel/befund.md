# MUTOS-Kernel: Befund K0 (2026-10-27)

Gemessen an den Disketten-Archiven (`disks/a5120_mutos8000/…_best_{usr,cc1,cc2,bin}.zip`), der WEGA-3.1-Platte und `OlliL/P8000`.

## Werkzeuge auf den MUTOS-Disketten

* `bin/cc as ld`, `lib/c0 c1 c2 cpp`, `crt0.o`, `libc.a`: alle `a.out` mit Magic `E807` (`A_MAGIC1`), Kopf 8 Worte
  (`a_magic a_text a_data a_bss a_syms a_entry a_unused a_flag`, 16 B), `a_flag = 1` (gestrippt, keine Symbole).
  Symbole: `struct nlist` mit 8-Byte-Namen, Typ `N_UNDF…N_BSS`, `N_EXT = 0x20` (`include/a.out.h`).
* **Alle Werkzeuge laufen nichtsegmentiert** (Z8002-Modus).  `c1` erzeugt nur `$NONSEGMENTED`; `as` und `ld` kennen die Direktiven
  `$NONSEG[MENTED]` und `SEG`, `ld` schreibt `$SECTION %s_d` (Zilog-Modulformat).
* Es gibt auf den Disketten **kein `scc`, `sld`, `cas`, `scrt0.o`, `slibc.a`**.

## Der Kernel laeuft NICHTSEGMENTIERT (Berichtigung, gleicher Tag)

Erste Fassung dieses Befunds hielt ihn wegen der 4-Wort-Vektortabelle fuer segmentiert -- falsch.  Das Z8001 liest seine PSA immer im 4-Wort-Format;
entscheidend ist das FCW-Bit 15 (SEG) des Eintrags.  Gemessen an `mutos_1216`:
* Eintrag 0 (Reset): FCW `C000` (System + SEG) -> `027E`.  Alle Trap-Eintraege (`0008`..`0030`): FCW `4004`, `4008`, `4018`, `4024`, `4028`, `400C` = SEG **geloescht**,
  alle auf dieselbe Routine `030E` (die Low-Bits sind vermutlich eine Trap-Kennung -- noch zu belegen).
* Alle 996 `CALL`-Befehle (`5F00`) im Code haben die Kurzform ohne Segmentwort; Speicherbedarf endet bei `CEFF` = ein 64-KB-Raum.
* Der Rechner hat keine MMU; das Z8001 laeuft ohne MMU auch segmentiert (Segmentbits direkt auf dem Bus) -- die fehlende MMU entscheidet nichts.

**Folge:** die MUTOS-Werkzeuge (`cc`, `as`, `ld`, nichtsegmentiert) sind voraussichtlich die richtige Kette; WEGA `scc/cas/sld` ist nur Vergleichsreferenz.
Offen: segmentierter Start-Stub (`027E`..) und wie der Kernel die 192 KB (`mem = 196608`) ohne MMU erreicht (EM-Speichermodell, Entwurf 17).
WEGA 3.1 hat `scc/sld/cas` (segmentiert) und `cc/ld/as` (nichtsegmentiert) -- beide auf `md2` der Platte, falls ein Vergleich noetig wird.

## Offen / zu messen
* Ob `scc`/`cas`/`sld` aus WEGA 3.1 den MUTOS-Kernel bytegleich erzeugen (gleiche Familie?) — Test: WEGA-Modul bauen, mit Funktionen in `mutos_1216` vergleichen.
* Ob `mutos_1216` aus WEGA-ähnlichen Quellen stammt (Abgleich der Funktionen, K6 §5.3).

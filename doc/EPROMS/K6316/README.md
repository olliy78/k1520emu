# K6316 — EPROM-Abzüge (Schalterdrucker, Speichermodul)

Quelle: [tiffe.de …/Drucker/K6316/erprom v24-für 6316/Eprom-k6316/](https://www.tiffe.de/Robotron/Drucker/K6316/erprom%20v24-f%fcr%206316/Eprom-k6316/),
geholt 2026-11-04.  Zwei Sätze zu je 5 × 2 KiB (U 2716 / K573RF2), einer je Schnittstellenmodul; die Sätze unterscheiden sich in
fast jedem Chip (1888/2013/1987/2006/1057 Byte Unterschied) — es ist also eine Firmware **je Interfacevariante**, nicht ein
Tabellenunterschied.  Laut robotrontechnik.de wurde der K6316 praktisch nur mit IFSS ausgeliefert.

| Datei | MD5 |
|---|---|
| `v24/V1.BIN` | b32f78aeac3317db477d492f826f46a4 |
| `v24/V2.BIN` | 0b8ee7e504ada62b317c9fba74f35b24 |
| `v24/V3.BIN` | 1e869490cf51a8eb89be6b81abccf947 |
| `v24/V4.BIN` | def00d52b3d29b3858896ad791424b67 |
| `v24/V5.BIN` | 44d136eb0bae2646e48d06e0d0219053 |
| `ifss/I1.BIN` | f56b9a9f3c3b9214242f4fe460823441 |
| `ifss/I2.BIN` | c8475d52814182ca335f7f400b0710d2 |
| `ifss/I3.BIN` | 464504d58b19d225d877ecc0a95f6b9a |
| `ifss/I4.BIN` | d2c88745923f1e097fad40aeb8a32d55 |
| `ifss/I5.BIN` | 6ed36b349cc42d485461d27daf8164ba |

Erster Befund (noch ohne Schaltplanabgleich, [?]):

* `V1`/`I1` beginnen mit `DD 21 24 38` / `31 00 3C` (`LD IX,3824H`, `LD SP,3C00H`) → Chip 1 liegt ab 0000H, die fünf Chips
  vermutlich lückenlos 0000H–27FFH; RAM um 3800H–3BFFH.
* Häufige E/A-Adressen (`OUT (n)`/`IN (n)`): 80H–8DH, A6H–A9H.  Die Zuordnung zu PIO/CTC/SIO und der Dekoder kommen aus den
  Serviceschaltplänen (`doc/printer/k6316/original/`), Entwurf 49 WP0.
* Bestückung laut Servicehandbuch: U 880 D, U 855 D (PIO), U 856 D (SIO), U 857 D (CTC), U 2716 / K573RF2.
* Die Reihenfolge V1…V5 = Steckplatz/Adressbereich ist **Annahme** — beim Abgleich mit dem Bestückungsplan prüfen.

**Zusammengesetzte Sätze (WP8b, Katalog `data/drucker_roms.yaml`):** `k6316_ifss.rom` = `cat ifss/I1.BIN … I5.BIN` (MD5 90307f3dd64f237e25c9baafeb867d34),
`k6316_v24.rom` = `cat v24/V1.BIN … V5.BIN` (MD5 61114a06ccb249a6a0e6cd34fa6dc135); je 10 240 B, die Datei, die der Kern über `romdatei=` nimmt.

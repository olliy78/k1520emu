# K6313 — EPROM-Abzüge (Hard-Copy-Drucker K6313/K6314-Familie)

Quelle: [robotron.webhop.net/docs/drucker/](https://robotron.webhop.net/docs/drucker/) (Liste `index-Dateien/sheet001.html`, Dateien
unter `mikroprogramme/`), geholt 2026-11-13; Forum [robotrontechnik.de, Thread 6421](https://www.robotrontechnik.de/html/forum/thwb/showtopic.php?threadid=6421)
(Etikettenschlüssel, Beitrag 21 ist ein Bild, nicht ausgewertet).  Güte durch eigene Messung (WP8a, 2026-11-14).

## Etikett und Kennung

`x.nn-F-ZS/PS`, Dateiname = Etikett ohne Trennzeichen (`33538013` = `3.35-3-80/13`).

* `x`: 3 = K6313 alte Platine, 5 = K6313 neue Platine, 4/6 = K6314 alt/neu (Forum; hier liegen nur 3 und 5 vor).
* `nn`: Programmzweig (35 = Standard, 08 = ISO/IFSP, 38 = HCD 80, 65/72 = 16-KiB-Umbau für DOS-PC, b8 = ISO/IFSP, c5 = IBM-Universal; nur aus den Etiketten der Liste gelesen `[?]`).
* `F`: 3 (K6313/14), `ZS`: Zeichensatz-/Schnittstellencode (80 = EPS INT., 85 = EPS INT./1715, 50/52 = ISO LAT/KYR bzw. GERMAN, 92 = EPS/IBM/SCH,
  95 = ATARI/THOM, 40 = ISO US ASCII, a0 = EPS/IBM 1834) — die Kürzel stehen nur in der Liste und sind im Forum unerklärt `[?]`.
* `PS`: Programmstand.
* **Die Firmware trägt dieselbe Kennung als ASCII-Text am ROM-Ende, in der Reihenfolge `F ZS x nn PS`** (`38033513` bei `27F8H` für `3.35-3-80/13`;
  bei 16 KiB bei `3FF8H`) `[E]` — damit lässt sich jedes Etikett prüfen.  **Befund:** `36539211` (Etikett 3.65-3-92/11) trägt die Kennung
  `39246510` = **4.65-3-92/10** (K6314-Programm); der Abzug ist also nicht das, was Datei und Liste sagen.  Alle übrigen Abzüge stimmen mit dem Dateinamen überein.

Platinen laut Liste: `05-260-6111-0 B/L0a` bzw. `BIMAR 6312` = 10-KiB-Platine (5 × 2716), `05-260-6311-2 B/L0` / `SDM 80-5` = 16-KiB-Speichermodul (27128),
`1.92.606240.3 B/L0a` = neue Platine (32 KiB, 27256).

| Datei | Größe | MD5 | PGM / Etikett | Schnittstellen / Zeichensatz | Güte | Hinweis |
|---|---|---|---|---|---|---|
| `30835007.bin` | 16384 | `2a1b75629c6852e9482d1d0afcbc014e` | 3.08-3-50/07 `V24/CL/IFSP ISO LAT/KYR PO-STAND.`; 05-260-6311-2 B/L0, 1×16k ROM + 4×0,5k RAM | V.24, Stromschleife, IFSP; ISO, Latein/Kyrillisch | ok | „inkl. Prüfprogramme" (Code ab `2800H`, Stapel `F400H`, Einsprünge `2C0EH…2C9AH`).  Stapel/RAM `F000…F7FFH`. |
| `30835207.bin` | 16384 | `6de9fe5b5353e19f514406e9cbe7dd02` | 3.08-3-52/07 `V24/CL/IFSP ISO GERMAN DE/US ASCII`; 05-260-6311-2 B/L0; 25.02.1988; eBay | wie oben, Deutsch + US ASCII | unbekannt | Liste: „nur 10k beschrieben".  **Messung widerspricht:** Daten bis `3FFFH`, Kennung `35230807` bei `3FF8H` passt; 0000–27FFH bis auf 15 Byte gleich `30835007`, 1283 Byte Unterschied nur in 2800–3FFFH (Prüfprogramm-Teil).  Ob der Chip wirklich nur 10 KiB trug und der Rest Altlast ist, ist nicht entscheidbar `[?]`. |
| `33538012.bin` | 10240 | `64e82d79a19ffea54e8b61080635b059` | 3.35-3-80/12 `V24/CL/CEN EPS INT.` (Dank René N.) | V.24, CL, Centronics; EPSON, international | ok | Etikett aus der Liste ohne Platinenangabe.  1011 Byte Unterschied zu `/13`. |
| `33538013.bin` | 10240 | `860295a3999aa5fbec45a33419b52970` | 3.35-3-80/13 `V24/CL/CEN EPS INT.`; 05-260-6111-0 B/L0a, 5×2k ROM + 4×0,5k RAM; drei Stücke (25.5.88, 10.10.88, n. l.) | wie oben | ok | **Vorgabe-Kandidat 10 KiB** (häufigstes Etikett, Liste „√").  Kennung `38033513`. |
| `33538028.bin` | 10240 | `4e78d5980b1a2d0122e9b098495abea8` | 3.35-3-80/28 `HCD 80 V24/CEN INT.`; BIMAR 6312, 5×2k ROM + 1×2k RAM | V.24, Centronics; HCD 80 | ok | 7009 Byte Unterschied zu `/13` (anderer Programmzweig). |
| `33538511.bin` | 10240 | `92935e08f0559d889d0f67acc639f4bd` | 3.35-3-85/11 `HCD 80 V24/CEN INT./1715`; 18.12.86 | V.24, Centronics; INT./1715 | **defekt** | **2. ROM (0800–0FFFH) ist zu 100 % FF** (gemessen, 2048 von 2048 Byte); Liste „2-ter ROM defekt".  Nicht als Vorgabe, läuft nicht an. |
| `33538513.bin` | 10240 | `67539a9786298e136f8e800f2ed1b0e3` | 3.35-3-85/13 `V24/CL/CEN EPS INT./1715`; 05-260-6111-0 B/L0a (Dank Holger B., Matthias K.) | V.24, CL, Centronics; EPSON, INT./1715 | ok | Nur **113 Byte** Unterschied zu `33538013` (80 → 85 = Zusatz „1715", wohl Zeichen-/Tabellenteil) `[?]`. |
| `33538514.bin` | 10240 | `2cbbf6119f58e9f4fb2d95b5ab839dc5` | 3.35-3-85/14 `V24/CL/CEN EPS INT./1715`; aus Silentrix MP108 | wie `…513` | ok | 6910 Byte Unterschied zu `…513`. |
| `33830803.bin` | 10240 | `7996f20e27902c770d0a3ef7a0b0bc6e` | 3.38-3-08/03 `HCD 80 V24/CL/IFSP US ASCII/KYR 0`; aus CM6329.01 M bzw. K6311 M | V.24, CL, IFSP; US ASCII/Kyrillisch | ok | Etikett laut Liste „Thanks to Oleksiy". |
| `36539211.bin` | 16384 | `ebd69941839e800aadbd680a162c034c` | 3.65-3-92/11 `V24/CL/CEN EPS/IBM/SCH`; SDM 80-5, 1×16k ROM, 2×8k + 2×2k = 20k RAM; „Nachwendeumbau (vermtl. für DOS-PC)" | V.24, CL, Centronics; EPSON/IBM/Schweden | unbekannt | **Kennung im Abzug `39246510` = 4.65-3-92/10**, nicht 3.65-3-92/11.  Nullfläche `25ACH…2A7DH` (1234 B), darauf zeigen `LD HL,2604H/2884H/2A5EH` — Tabelle oder Lücke `[?]`. |
| `37239503.bin` | 16384 | `6ce4515f912d139fc439d45584675866` | 3.72-3-95/03 `CEN EPS/IBM/SCH ATARI/THOM`; SDM 80-5, 1×16k ROM + 1×2k RAM; 07.01.1987 | nur Centronics; EPSON/IBM, ATARI/Thomson | ok | Kein SIO-Zugriff (nur Centronics, gemessen).  Kennung `39537203`. |
| `3b834000.bin` | 10240 | `8247a212373517485a59c8018b608760` | 3.b8-3-40/00 (Etikett `3.B8-3-4 1 /00`, Tippfehler) `V24/CL/IFSP ISO LAT/KYR`; 04.Mai.1988 | V.24, CL, IFSP; ISO Latein/Kyrillisch | unbekannt | **1126 Byte Nullen `1A91H…1EF6H`** (Block 1800–1FFF zu 55 % `00`, bei allen anderen 10-KiB-Abzügen < 3 %); `LD HL,1D77H` (`1355H`) zeigt hinein.  Verdacht auf beschädigten Abzug oder Nulltabelle `[?]`. |
| `53538500.bin` | 10240 | `76f34de73992e88a7267fa95480754a1` | **5**.35-3-85/00 `V24/CL/CEN EPS INT./1715` (**neue Platine**); 24.02.90 | wie `…513` | ok | Neue Platine, trotzdem 10-KiB-Zuschnitt, Stapel `3C00H`.  8080 Byte Unterschied zu `…513`. |
| `5c53a001.bin` | 16384 | `75e0ea39ecd487ea6952884b0a316a2b` | **5**.c5-3-a0/01 `V24/CL/CEN EPS/IBM IBM 1834 BWK` (neue Platine 1.92.606240.3 B/L0a), 1×32k ROM + 1×8k RAM | V.24, CL, Centronics; EPSON/IBM | ok (16 von 32 KiB) | Liste „nur 16k beschrieben".  Das Etikett nennt 32 KiB: gemessen Daten nur bis `3FFFH` (Dateigröße 16 KiB), Kennung `3A05C501` bei `3FF8H`.  Stapel `F800H` wie die 16-KiB-Familie. |

## Gemessen: FF-/00-Anteil je 2-KiB-Block (Prozent `FF/00`)

| Datei | Blöcke 0000, 0800, 1000, … |
|---|---|
| `33538012` | 1/1 · 0/2 · 0/3 · 0/2 · 41/1 |
| `33538013` | 1/1 · 0/2 · 0/3 · 0/2 · 41/1 |
| `33538028` | 1/1 · 0/2 · 0/3 · 0/2 · 41/0 |
| `33538511` | 1/1 · **100/0** · 0/3 · 0/2 · 38/1 |
| `33538513` | 1/1 · 0/2 · 0/3 · 0/2 · 38/1 |
| `33538514` | 1/1 · 0/2 · 0/3 · 0/2 · 38/0 |
| `33830803` | 1/1 · 0/2 · 0/3 · 0/2 · 37/1 |
| `3b834000` | 1/1 · 0/1 · 0/2 · **8/55** · 41/0 |
| `53538500` | 0/1 · 0/2 · 0/3 · 0/2 · 38/1 |
| `30835007` | 1/1 · 0/1 · 0/2 · 3/2 · 0/2 · 13/13 · 51/0 · 29/0 |
| `30835207` | 1/1 · 0/1 · 0/2 · 3/2 · 0/2 · 13/13 · 51/0 · 31/0 |
| `36539211` | 1/1 · 0/2 · 0/3 · 1/2 · 0/30 · 27/31 · 52/1 · 56/1 |
| `37239503` | 1/1 · 0/2 · 0/3 · 1/2 · 0/2 · 43/0 · 52/2 · 58/0 |
| `5c53a001` | 0/1 · 0/2 · 0/3 · 1/2 · 0/2 · 48/0 · 54/1 · 61/0 |

Alle 14 Abzüge beginnen mit `DD 21 24 38 31 …` (10 KiB, `LD IX,3824H`, `LD SP,3C00H`) bzw. `DD 21 24 F0 31 00 F8` (16 KiB, `LD IX,F024H`, `LD SP,F800H`) —
**zwei Speicherpläne**, siehe `doc/printer/k6313/firmware_diff.md`.

## Offen

* `43530809` (Liste: nicht hier) und die K6314-/K6316-/K6319-/Präsident-Abzüge sind nicht geholt.
* Bedeutung der Kürzel EPS, IBM, SCH, ISO, KYR, HCD, CL, INT. ist nur aus Liste/Handbuch erschlossen (EPS = EPSON-Steuercodes, CL = Current Loop `[?]`).

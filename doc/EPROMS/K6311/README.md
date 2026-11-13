# K6311 — EPROM-Abzüge (Hard-Copy-Drucker, Vorläufer des K6313)

Quelle: [robotron.webhop.net/docs/drucker/](https://robotron.webhop.net/docs/drucker/) (Excel-Export `index-Dateien/sheet001.html`,
Dateien unter `mikroprogramme/`), geholt 2026-11-13.  Etikettdaten aus dieser Liste; Güte durch eigene Messung (WP8a, 2026-11-14,
Anteil FF/00 je 2-KiB-Block, Binärvergleich, ROM-Kennung am Ende).  Zum K6311 liegt im Projekt **keine** Unterlage (kein Schaltplan,
kein Handbuch); alles zur Hardware steht in `doc/printer/k6313/firmware_diff.md` §K6311 und ist dort als `[?]` gekennzeichnet.

**Dateiname = Etikett ohne Trennzeichen**: `x.nn-F-ZS/PS` → `xnnFZSPS` (`12128062` = `1.21-2-80/62`).  Die Firmware trägt dieselbe
Kennung als ASCII-Text am ROM-Ende, aber in der Reihenfolge `F ZS x nn PS` (`1.21-2-80/62` → `28012162` bei `27F8H`) `[E]`.
Das Etikett des Moduls ist **nicht** maßgeblich (Umprogrammierungen, Tippfehler — Forum 6421, Beitrag 16, und die Liste selbst);
maßgeblich ist die Kennung im Abzug.

| Datei | Größe | MD5 | PGM / Etikett | Schnittstelle / Zeichensatz | Güte | Hinweis |
|---|---|---|---|---|---|---|
| `12128062.bin` | 10240 | `8674d87842c7409864a420141be86cc0` | 1.21-2-80**2**/62, `HCD 80 F ISO-G V24/RS232C INT./GERMAN`; Platine 05-260-6111-0 B/L0a, 5×2k ROM + 2×0,5k RAM; Stempel 19.05.84 | V.24; Vorgabe-Zeichensatz Deutsch `[?]` | ok | Kennung `28012162`.  Zuordnung zur Liste (…802 = GERMAN) aus Byte `27F7H` = `20H` geschlossen `[?]`.  Die Liste vermerkt „Typo im Label?"; das Byte `27F7H` wird beim Start nach `(3836H)` kopiert (Zeichensatz-Vorgabe `[?]`). |
| `12128062_1.bin` | 10240 | `dacf64d41476fb71bd66f6c0dff325de` | 1.21-2-80**3**/62, `… V24/RS232C INT./UK ASCII`; BIMAR 6312, 5×2k ROM + 1×2k RAM; Stempel 11.VII.1985 | V.24; UK ASCII `[?]` | ok | **Genau 1 Byte Unterschied** zu `12128062.bin` (`27F7H`: `20H` → `30H`); Rest bitgleich.  Zuordnung `_1` = …803 = UK ASCII aus dieser Differenz `[?]`. |
| `12228062.bin` | 10240 | `5eb95fcf21c95c0895353f246366aa6a` | 1.22-2-80**0**/62, `HCD 80 F ISO-G CENTRONICS INT./US ASCII`; BIMAR 6312, 5×2k ROM + 1×2k RAM; eBay-Fund | **Centronics**, US ASCII | ok | Kennung `28012262`.  8099 Byte Unterschied zu `12128062` (andere Schnittstellenroutinen: kein SIO-Zugriff, `BC–BF` als Datenpfad `[E]`).  Liste: „Typo im Label?". |
| `13120012.bin` | 8192 | `23d980fecd696f483c27fef486bab035` | 1.31-2-00/12, `HCD 80 T ISO V24/RS232C GERMAN`; BIMAR 6312, 4×2k ROM + 1×2k RAM | V.24, Deutsch | ok (nur 8 KiB) | Nur 4 × 2 KiB bestückt (0000–1FFFH) laut Liste; gemessen: Block 3 (1800–1FFF) enthält Code, letztes Nicht-FF-Byte `1FFFH`, Kennung `20013112` bei `1FF8H` → **vollständig im Sinne des Chips**, kein 10-KiB-Abzug. |
| `13325212.bin` | 8192 | `026295fbcff3fca009b4972fc6b6696d` | 1.33-2-52/12 (Dateiname) bzw. Etikett `2.33-2-00/12 HCD 132 T ISO IFSS GERMAN`, teils gestrichen, „vermutl. neu programmiert"; 05-260-6111-0 B/L0a, 4×2k ROM + 1×2k RAM; 11.Aug.1986 | IFSS, 132 Zeichen `[?]` | ok (nur 8 KiB) | Kennung im Abzug `25213312` = **1.33-2-52/12**, nicht das Etikett (2.33-2-00/12).  Block 3 mit 22 % FF (Datenbereich), sonst wie `13120012`. |

## Gemessen

FF-/00-Anteil je 2-KiB-Block in Prozent (`FF/00`):

| Datei | 0000 | 0800 | 1000 | 1800 | 2000 | Bemerkung |
|---|---|---|---|---|---|---|
| `12128062` / `_1` | 0/2 | 0/1 | 0/1 | 0/3 | 49/0 | Block 4 (2000–27FF) zur Hälfte FF: Zeichengenerator + Tabellen; FF-Läufe ab `2083H` (381 B), `2678H` (376 B) |
| `12228062` | 0/2 | 0/1 | 0/2 | 16/3 | 56/0 | FF-Lauf ab `1EBAH` (838 B) |
| `13120012` | 0/2 | 0/1 | 0/2 | 14/2 | – | 8 KiB |
| `13325212` | 1/2 | 0/1 | 0/2 | 22/1 | – | 8 KiB, FF-Lauf ab `1BFFH` |

Kein Block leer, keine Nullflächen ≥ 48 Byte, kein Block ganz FF → **kein Hinweis auf einen defekten Chip**.  Alle fünf beginnen mit
`DD 21 24 38` (`LD IX,3824H`), Stapel `3BFEH` (zwei Abzüge V.24 1.21) bzw. `3C00H`.

## Offen

* Schnittstellenmodul-Zuordnung (V.24 / IFSS / Centronics) der 10-KiB-Platinen ist nur aus dem Firmwareverhalten erschlossen.
* Die Liste führt kein 16-KiB-K6311; die Abzüge `13xxxxx` (8 KiB) haben kein zweites Gegenstück.

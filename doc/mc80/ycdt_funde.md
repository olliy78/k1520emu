# MC80.3x — Funde von ycdt.net/mc80.3x (gelesen 2026-10-26)

Quelle: `http://www.ycdt.net/mc80.3x/` (nur per HTTP erreichbar; Spiegel k1520.com).  Alle 22 dort
angebotenen `.bin`-Abzüge liegen schon in `doc/EPROMS/MC80/` (gleiche Namen) — **keine neuen ROMs**.
Die Lücken bleiben: 31E ohne 2000H/2800H/2C00H/3C00H, Segment 20H „e“, `mc80.33` ohne 1800H–1FFFH.

## MC80.31E (Kundenfassung mit zwei RAM- und zwei EPROM-Segmenten)

Speicherbild: 0000–27FF Betriebssystem · 2800–2FFF RAM-Modus · 3000–3BFF Texteditor (Extrataste
„T“) **oder** EPROG (Stellung „E“) · 3C00–3FFF Druckprogramm DRU · 4000–BFFF RAM-Segment 01H/02H bzw.
EPROM-Segment 10H/20H (Umschaltung ZVE-PIO A) · C000–E57F freies dRAM · E580–EAFF Druckertreiber ·
EB00–F003 Kennung „%04“.  INIT-Kennung in CATA „%03“ bei 0B13H, 1FF3H, 2740H.  EPROM-Programmierung
fest auf 3000H.

**Sondertasten-Vorbelegung** (Tabelle bei 23EAH, 3 Byte je Taste, `INIT` kopiert 21 Byte nach FED0H):

| Taste | Tabelle | Sprung | Wirkung |
|---|---|---|---|
| FT1 | 23EAH | `C3 2770H` | Spiel Pharao [?] |
| FT2 | 23EDH | `C3 2778H` | Quelltexteditor „Q2“ ins 2. Segment laden |
| FT3 | 23F0H | `C3 2780H` | GRAPHIC-BASIC „BAS“ ins 2. Segment laden |
| FT4 | 23F3H | `C3 27B0H` | Druckertreiber „LP“ (LP11) nach E580H laden |
| FT5 | 23F6H | `C3 27C8H` | Blockassembler „ASM“ ins 1. Segment laden |
| FT6 | 23F9H | `C3 3C00H` | „DRU“: Bildschirm (Zeilen 1–23) auf Drucker 2 |
| FT7 | 23FCH | `C3 27E0H` | „CT“ [?] |

Zuordnung zu unseren Tasten [?]: Taste n der Nachbildung (Maske 40H>>n) entspricht dem Eintrag
FED0H+3n, also FT(n+1); Taste 7 (Maske 00H, FEE5H) ist FT0 (auf der Seite ohne Angabe).  Unser
`mc80.3s-2000.bin` trägt nur FT4 (`C3 0B51H`) und FT7 (`C3 225FH`) — eine andere Fassung, gleiches Layout.
Die Sprungziele 2770H–27E0H liegen im 2400H-Abzug der **31E-Fassung** (Listing
`mc80.31e_2000-27ff.pdf`, nicht im Repo); unser `mc80.31e-2400-d0d7.bin` ist die CCD-Fassung und
hat dort andere Inhalte — daher bleibt `Mc80Segmente.SondertastenLadenEditorUndBasic` nicht umsetzbar.

## Spiele (`mc80.3x_spiele.htm`)

`RKET 01,4000,<Ladeadresse>` + `Z` für Pharao (6400H) und Wall (8000H); Tetris (4000H) mit `TE`,
Hasi (4000H) mit `HASI`.  Damit ist **der Befehl** belegt; dass `RKET` dabei ein leeres Kettenglied
`01 <adr-4003H> FF` anlegt, bleibt Annahme.

## Nur als eingescanntes Listing (keine Textebene, kein OCR im Haus)

31E: 2000–27FF (21 S.), 2300/2700 (Fassungen), 3940–3BFF, E580–EAFF (Druckertreiber); .33: 0000–00AF,
0380–04FF, 0B00–0CFF, 2000–2FFF; EPROG V3.0/3.1 (Quelltext, 7305H/9305H), QEDIT/ED, Tetris-Listing;
Hardware: SPE2/SPE4, ZVE/ZVE-LA, DAW 4.12, DAG, QFA, PK-3 (5214).

**Stichprobe `mc80.33_2000-2fff.pdf` (Seite 1, 2, 12):** lesbarer Z80-Re-Assembler-Ausdruck
(Adresse, Bytes, Mnemonik) eines **Segmentprogramms bei A000H–AFFFH** („EPROM 2000H“): Sprungtabelle
bei A019H, Segmentumschaltung `OUT 85H` mit Merkzelle FBFEH, Aufrufe in 0BF7H/0BEBH/0BF4H/0BDCH und
2446H.  Es enthält **nicht** die fehlende PK-3-Hälfte 1800H–1FFFH; Abtippen (≈ 4 KB) lohnt für das
Entsperren der .33 nicht.  Dafür müsste ein Listing von 1800H–1FFFH auftauchen.

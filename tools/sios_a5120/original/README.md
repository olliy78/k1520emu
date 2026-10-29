# SIOS 1526 — Originalabbilder

Herkunft: Diskettensammlung DL, `~/projects/robotron/Diskettensammlung_von_DL/Diskettenarbeit/Disketten_Images_RT/`
(Dateidatum 2023-06-19), unverändert übernommen am 2026-10-29.  **Nie direkt mounten** (der Emulator öffnet r/w) —
`boot_trace`/`k1520dbg` mounten per Vorgabe Copy-on-Write.

| Datei | Medium (gemessen) | Inhalt (IBM-Etiketten, EBCDIC) | SHA-256 |
|-------|-------------------|--------------------------------|---------|
| `SIOS1526_SYLs.hfe` | 80 Zyl. × 1 Seite, 26 × 128 B MFM | VOL1 `ROBINI`, HDR1 `SYSARE11` (Spur 1–12) — **SIOS 1526/1.1**, SYL-Lader in Spur 0 S. 1–4 | `ee832629…` |
| `SIOS1526_SYLp.hfe` | 80 Zyl. × 2 Seiten, Seite 0 = 26 × 128 B MFM bis Zyl. 39 und ab 69, Seite 1 leer (ein Streusektor c15h1) | VOL1 `ROBINI`, HDR1 `SYSARE09` (Spur 1–10) — **SIOS 1526/0.9** | `4c3a5e09…` |
| `SIOS1526_DP1.hfe` | 40 Zyl. × 1 Seite, 26 × 128 B MFM | VOL1 `MWD051`, HDR1 `PHASELB` (Spur 1–37): CATM CODK COCA COPD DINH DIMO DIKA DMPM EDIT FGEN INIT DIDI KORP LBSV CODP | `bca0b4af…` |
| `SIOS1526_DP2.hfe` | wie DP1 | VOL1 `MWD051`, HDR1 `PHASELB`: LINK MBAS PRMF PRDI PHAK RAMT SUS0 SGEN SOPH SYSD TEXT MTES SORD | `cca40745…` |
| `SIOS1526_SG1.hfe` | wie DP1 | VOL1 `MWD002`, HDR1 `SYSORG09` (Satzlänge 1024, Spur 1–28: Generiermodule GRUN, 636, 637, BAB, ARI, DEZ, …) + HDR1 `PHASELB` (Spur 29–34: SGEN SOPH SYSD); `DDR1`-Etiketten = gelöschte Einträge DATA10…DATA26 | `74bb0cc0…` |

Befund und Plan: `doc/design/43_sios1526.md`.

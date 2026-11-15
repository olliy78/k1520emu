# Gastsoftware zur SOUND-Karte K1520 (Fremdsoftware)

Weitergabe im Repo und auf den Systemdisketten vom Anwender freigegeben (2026-11-14).
Entwurf: `doc/design/51_soundkarte_k1520.md`.  Die Dateien unter `original/` sind **unverändert**
(Stand der Downloads 2026-11-14) — nicht flicken; Anpassungen entstehen beim Diskettenbau.

| Datei | Herkunft | Inhalt |
|-------|----------|--------|
| `original/K1520_SOUND_BC.zip` | Google-Drive-Ordner „Software ▸ A5120 + K8924" der Kartenseite `k1520.8i8.de` (→ `sites.google.com/view/robotron/sound`) | TGCPM, SKTEST4, STCTEST1 (mit MP3-Aufnahme `STCTEST1.mp3` der echten Karte), PLAYSTC + SONG.STC, Original AYPLAY.INC 1.2 (WeRo); CP/A + SCPX, 38H, Vektor F7D8H, Stand 16.12.2017 |
| `original/K1520_SOUND_CPM-PT3-STC-USB-Player_1.0.3.zip` | dto. (auch im Ordner K8915, bytegleich) | USB-Player CPMSTCPL/CPMPT3PL 1.0.3 + SNDINIT2 (SOUND.INI-Erzeuger) |
| `original/PC1715_K1520-SOUND.zip` | Ordner „PC1715" | USB-Player **1.0.5** (Stand 21.05.2025) für den PC 1715 + SOUND.INI, Adapter-Unterlagen |
| `original/K1520_SOUND_PC1715_CPA.zip` | Ordner „PC1715" | TGCPM/SKTEST4/STCTEST1/PLAYSTC für den PC 1715 unter CP/A (Karte an **E0H**, Vektor F7F0H) |
| `original/Universal_AYPLAYINC.zip` | Wurzel des Software-Ordners | AYPLAY.INC universal + Doku (WeRo) |
| `original/CPM_USB_PT3_STC-Player_1.0.5.pdf` | Wurzel des Software-Ordners | Anleitung USB-Player 1.0.5 |
| `original/rowikla_SKTEST4_BC/` | `~/projects/robotron/rowikla/SKTEST4_BC/` (vom Anwender) | SKTEST4 in einem neueren Bau (5250 B statt 5142 B im ZIP) |
| `original/SOUND-K1520V2-Beschreibung.pdf`, `Interrupt_CPA_SCPX.txt` | `~/projects/robotron/rowikla/Dokus/` | Hardwarebeschreibung V2, Interruptsäule CP/A/SCPX |

Autoren: Daniel Auerbach (Karte, SNDINIT2), Rolf Weidlich „WeRo" (AYPLAY.INC, Player), Klaus Wilfling „KW"
(CP/A-/SCPX-Anpassungen), Heiko Poppe „HP", J. Markwardt (PC-1715-Interrupttabelle).

## Auf den Systemdisketten (AP S9a)

Entpackt und unverändert (Länge wie im Archiv) liegen die Programme in `a5120/` und `pc1715/` (einzige Quelle
für Diskettenbau **und** `SoundSystem.*`-Tests; `sound_karten.py` vergleicht sie mit den Archiven).  Gebaut wird
über die Bauskripte der Disketten (`tools/gide/karten.py::dateien` mischt `tools/sound/sound_karten.py` ein);
auf der Diskette sind die Dateien wie bei CP/M üblich mit 00 auf 128-B-Sätze aufgefüllt.  Wächter `cli_sound_beigaben`.

| Satz | auf welchen Disketten |
|------|-----------------------|
| `a5120/` TGCPM, SKTEST4 (rowikla-Bau), STCTEST1, PLAYSTC, SONG.STC | `disks/a5120_cpa/a5120_cpa_k5601_system.hfe`, `disks/a5120_scpx17/a5120_scpx17_k5601_system.hfe` |
| `pc1715/` dieselben fünf Namen, PC-1715-Fassungen | `disks/pc1715_cpa1715/pc1715_cpa1715_system.hfe` |

Nicht auf Disketten: K8915, PRG 710/-1, MC80, PC 1715 SCP/1715W (keine Gastsoftware dafür bekannt bzw. nicht verlangt);
USB-Player (`CPM*PL`, SOUND.INI) folgen mit S9b.

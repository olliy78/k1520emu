# JKCEMU-Dateiformate und das Kassettenband (Stand 2026-10-26)

Quelle: `~/projects/jkcemu` (0.9.9.1), `help/fileformats.htm`, `file/FileSaver.java`, `file/FileInfo.java`.

## Befund

JKCEMU hat **keine digitale Kassettenkarte** (keine K5020, kein A5120/K8915/PRG/PC1715/MC80).  Es
emuliert nur Audio-Anschluesse (Z1013, KC85, Z9001, AC1, LLC2, ZX Spectrum, KC compact).  Seine
Formate zerfallen in zwei Gruppen:

| Gruppe | Formate | Inhalt | Von uns |
|--------|---------|--------|---------|
| Speicherabbild | Headersave (`.z80`), KCC/KCB/KCM/JTC, KC-TAP (auch Multi), Intel-HEX, roh | Adresse(n) + Bytes | **ja** (`programmdatei.*`) |
| Tonsignal/Tape | WAV/AU/AIFF, CSW, TZX/CDT, ZX-TAP | Pulse bzw. rechnerspezifische Bloecke | nein |
| KC-BASIC | `.sss`, KCB-Relozierung | BASIC-Tokens je KC-Rechner | nein |

Das `.k52`-Band traegt dagegen die **rohen Blockbytes der Gastsoftware** (`AA | Daten | CRC | AA`,
Merkposten Nr. 10) — das Blockformat ist OS-eigen (BCU, SCPX/KMBG, EGOS), kein Hardwarerahmen.
Ein JKCEMU-Format kann also nur eine **Huelle um ein Speicherabbild** sein, nie ein fertiges Band.

## Bytelagen (alles little-endian)

- **Headersave:** 32 B Kopf: 0 Anfang, 2 Ende (inklusiv), 4 Start (nur Typ `C`), 12 Typ (ASCII),
  13..15 = `D3 D3 D3`, 16..31 Name (16, Leerzeichen); danach Daten.
- **KCC:** 128 B Kopf: 0 Name(8) 8 Typ(3) 16 Zahl der Adressen (2|3) 17 Anfang 19 Ende 21 Start, Rest 0.
  Ende ist bei KC85/HC900 **Ende+1**, bei Z9001/KC87/JTC das reale Ende (`SchreibOptionen::kc_ende_exklusiv`).
  Lesen laedt bis einschliesslich der eingetragenen Adresse, hoechstens soviel die Datei hergibt
  (wie JKCEMU: nie ein Byte zu wenig, ggf. eines zu viel).
- **KC-TAP:** Kennung `C3 "KC-TAPE by AF. "` (16 B), dann 129-B-Bloecke (Nr. + 128 B); Block 0 = KCC-Kopf,
  Datenbloecke 1..n, der letzte immer `FF`, aufgefuellt mit 0.  Multi-KC-TAP = Kennungen hintereinander;
  gelesen wird nur die erste Teildatei (Hinweis auf stderr).
- **Intel-HEX:** Typ 00/01 (+02/04 nur, wenn die Adresse im 16-Bit-Raum bleibt), Pruefsumme streng;
  Luecken werden mit 00H gefuellt; geschrieben wie JKCEMU: 32 B/Zeile, CRLF, keine Startadresse.

## Bedienung

`k1520tape put|bootband … --format F` und `get … --as F --addr A` (tools/k1520tape.md).  `bootband`
verlangt bei Formaten mit Adresse 0400H (Ladeadresse des Lade-ROMs) statt zu raten.  Die Oberflaeche
(Kasten „Kassette“) kennt die Formate nicht — sie arbeitet weiter mit `.k52`.

## Offen

- Ein Bootband aus Headersave hat nur dann Sinn, wenn die Datei `SYL` an 0400H traegt (wird geprueft).
- Audio (`.wav` nach ISO 3407, K11) bleibt zurueckgestellt; ein JKCEMU koennte das Signal ohnehin
  nicht decodieren, weil es unser Blockformat nicht kennt.

Wächter: `Programmdatei.*` (test_programmdatei), `cli_k1520tape_jkcemu_formate`, `…bootband_hex`, `…falsche_adresse`.

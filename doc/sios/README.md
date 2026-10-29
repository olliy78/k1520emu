# SIOS 1526 - Arbeitsgrundlage (AP S1)

Stand 2026-10-29.  Entwurf: `doc/design/43_sios1526.md`, Handbuchauszug: `handbuch_auszug.md`.
Markierungen: **[Lauf]** am Emulator belegt (`k1520dbg`), **[Code]** aus dem Code gelesen,
**[Doku]** aus `doc/trascripted/Zentrale Recheneinheit_K2526_K2527.md`, **[?]** Annahme.

## Dateien

| Datei | Inhalt |
|-------|--------|
| `tools/sios_a5120/dump.py` | `bild` (lineares Abbild 26*128 B je Spur), `labels` (VOL1/HDR1/DDR1, DKOI ~ cp500), `get` (HDR1-Datei herausholen). Nutzt `app/core_binding/k1520disk.py` wie `tools/k8915_sysload.py` |
| `tools/sios_a5120/gen_prn.py` | erzeugt beide Listings aus `original/SIOS1526_SYLs.hfe`; Namen/Kommentare stehen im Skript (`NAMEN`, `KOMMENTAR`, `MODUL_INFO`) |
| `doc/sios/sysare11.prn` | SIOS 1526/1.1, 0000H-8FFFH, fuer `k1520dbg -l` / `boot_trace -l` (`lst doc/sios/sysare11.prn`) |
| `doc/sios/syl_lader.prn` | SYL-Lader 0400H-05FFH |

Neu bauen: `python3 tools/sios_a5120/gen_prn.py`.  Das Listing ist **statisch** (rekursiver Abstieg, 28 KB von 34 KB als Code;
58 Einstiege aus Luecken tentativ zusammengesetzt und mit `[vermutet]` markiert).  Alle zur Laufzeit bis 20,9 Mio. Takte
ausgefuehrten Befehle (693 Adressen) sind im Listing enthalten und fallen auf Befehlsanfaenge.

## Ladeablauf (SYLs, SIOS 1.1) [Lauf]

1. **Lade-ROM** (A26) liest Spur 0 Sektor 1-4 nach 0400H, prueft `SYL` bei 0400H und ruft `CALL 0437H` (ROM 0174H).
2. **SYL-Lader** (`syl_lader.prn`): CRC-CCITT ueber 0410H..05E9H gegen 05FEH/05FFH.  Kopf: 0403H = 04 (OUT 12H),
   0404H/0405H = **8800H Byte** (SYLp: 6C00H), 0406H = 00 -> Ladeadresse **0800H** (sonst 4000H + OUT EEH).
   Sektorzahl = 8800H/128 = **272**, gelesen **ab Spur 2** (Datei-Sektor 26; Spur 1 der Datei SYSARE11 wird nicht geladen -
   dort steht eine zweite `SYL`-Kopie) nach **0800H..8FFFH**, 26 Sektoren je Spur, Spur 12 nur 12 Sektoren.  Je Sektor ZVE2-Lesen
   nach 0700H, CRC pruefen, 7 Wiederholungen.  Ende (045CH): **AAWA (004CH) = 9000H**, (0069H) = AAWA-0400H, `RET` ins ROM.
3. **Lade-ROM** (0177H): `LD HL,(03FAH)` (= 0800H) -> (0462H); ROM aus (Port 0AH Bit0); fuellt 00BAH..03FFH; `JP 0074H`:
   `LD HL,(0462H) / LD L,9DH / JP (HL)` -> **Systemstart 089DH**.
4. **SYSTART** (089DH, Cycle 14,87 Mio): EI, I=02, 0400H-07FFH loeschen; BS-PIO/CTC einrichten; Seite 0 aus der Vorlage
   0840H..088BH aufbauen (RST-Sprungziele +0800H; NMI 0066H = JP 0A8FH); IM-2-Tabelle ab 0274H aus Vorlage 088CH; Suche `INDEX`
   (AAWO, 006CH); dann **Blockpruefung**: 34 Bloecke zu 1 KB mit CRC-CCITT (Kopf 2 Byte, 1022 Byte, CRC 2 Byte; ~5,9 Mio. Takte);
   Blocks mit Folge-Nummer 0 haben einen Moduleinsprung bei Block+2 (`CALL 005DH`, [Lauf] 10 Aufrufe: 0800, 2000, 3000, 4000,
   4400, 4800, 5000, 6000, 7000, 8400).
5. 0A3F: `LD DE,0075H / CALL 003BH` = relativer Aufruf von **0ABAH** (Speicher in 64-B-Schritten lesen), danach **0AFFH** (s. u.).

## Besonderheiten des Codes

- **Positionsunabhaengig durch Systemroutinen auf Seite 0**: `CALL 002BH` (DE=n: Sprung nach Folgeadresse+n), `CALL 003BH`
  (Aufruf Folgeadresse+n), `CALL 0033H/0036H` (DE/HL = Folgeadresse), `CALL 0042H` mit L=2i+1 (Sprung ueber Wortetabelle 0800H,
  Eintrag+0800H; z. B. L=19H/1BH = CRC_INIT/CRC_BLOCK 0B08H/0B0BH, L=2BH = Fehleranzeige 0B32H, L=1FH 0D17H).
- `RST 08H` (SVC) hat **ein Kennbyte** hinter dem Befehl; `OUT 00H` loest /INT-BS (BS-PIO B1, ISR 1E48H) aus der Makroebene aus.
- Fehlerkennung als ASCII in BC/DE: `LD BC,3032H / LD DE,5953H` = "02"/"SY" (`ERR SY 02`).
- Seite 0 nach SYSTART (aus Lauf): 0008 RST08 -> 1BA5, 0010 -> 0E7F (Monitor), 0018 -> 1AC8, 0028 -> 14D2, 0066 -> 0A8F;
  0060 ERAM (FF7FH), 0062 AGM = 0800H, 004C AAWA = 9000H, 006C AAWO, 0300..0315 Geraetetabelle, IM-2-Tabelle 0274..027F:
  BS-PIO A 0D4B, BS-PIO B 1E48, CTC2 0A8C (-> 27AB gepatcht), CTC3 0D6A.
- Module (Ports): 5000-5FFF K8025 (58H, 5EH/5FH), 6000-6FFF K5122 (10H-17H), 8400-8FFF SIO A33/DFUE (50H/51H, 56H, 0CH),
  3000-3FFF Port 40H/0AH [?]; 2000-2FFF viele `OUT 00H`.  Port 40H wird bei 301EH und 36CBH beschrieben (nicht im Lauf erreicht).

## Befund fuer S2 (warum keine Systemzeile)

**Ursache gefunden: Polaritaet/Modell von BS-PIO Port B Bit 2 (/SPS-ESR) im K2526-Kern.**

- Nach der Blockpruefung bei 0AFFH (Cycle 20 924 616): `IN A,(0AH) / RES 2,A / NOP / OUT (0AH),A / EI / RET` [Lauf].
- Der Kern (`k2526.cpp` onBSPIOPortBOut, `K2526(...)` readByte/writeByte) deutet **B2 = 0 als Schutztabellen-Schreibmodus**
  und liefert dann fuer jeden ZVE1-**Lesezugriff - auch den Opcode-Abruf -** `spa_.readEntry()` (= 0): ab 0B04H holt die
  ZVE1 nur noch NOPs, der PC laeuft 0B05H..9000H durch, dort `RST 38H`-Flut.  Das ist die beobachtete Absturzkette
  (die Kette 0A42 CALL 003B -> 0ABA -> 0AFF ist der Normalweg, `RET` bei 0B07H wird nie als solches geholt).
- **Hardware laut [Doku] 6.11/B2 (Zeilen 891, 932):** B2 = **high** (Einschaltzustand) = Schutz-RAM zum **Beschreiben** frei,
  "Schutzfunktionen unwirksam"; erst **B2 = low schaltet Speicher- und E/A-Schutz scharf**.  Programmiert wird das 1-KB-RAM A19
  (1 Bit je 64-Byte-Segment, 1 = BS/geschuetzt) **ueber AB0 des Adressbus** (nicht ueber ein Datenbyte), d. h. durch normale
  Speicherzugriffe.  Genau das tut SIOS: 0ABAH liest `LD A,(HL)` in 64-B-Schritten, ab HL=0001H (AB0=1) ueber den ganzen
  64-KB-Raum = alles BS, dann ab 0340H (AB0=0, 3 Segmente) und AAWA..ERAM (004CH ist 64-B-ausgerichtet, AB0=0) = Anwenderbereich,
  zuletzt AAWO-Bereich; danach 0AFFH `RES 2` = Schutz ein.  Schutzregel: Befehle aus einem BS-Segment duerfen BS und Anwender
  beschreiben, Befehle aus einem Anwender-Segment nur Anwender; Verletzung -> /MEMDI, SPS-Indikator (PIO A3, Interrupt) bzw. NMI
  bei E/A.  Der Kern modelliert dagegen ein WP/RP-Byte je Segment aus dem **Datenbyte**, Polaritaet vertauscht.
- Gegenprobe [Lauf]: `b 0x0B04` / `set A 0x16` (B2 bleibt 1) -> SIOS laeuft weiter, aber die Schutztabelle bleibt unprogrammiert;
  danach BS-PIO-A-Interrupt (SPS-Indikator A3=0 steht an, Maske F7H, ISR 0D4B trampolint per RETI nach 0D56 und wird vor dem
  ersten Befehl erneut angenommen), SP faellt von 0400H auf 0272H, Spinschleife 0C7DH `BIT 7,(HL)` HL=00CBH, Bildschirm leer
  (nach 320 Mio. Takten).  Das ist ein Folgeeffekt der fehlenden Schutz-Emulation, nicht die eigentliche Ursache.
- S3-Vorschlag: `MemIOProtect` nach [Doku] 6.11 nachbilden (B2=1: Zugriff schreibt `tab[A>>6] = A & 1`, Speicher normal;
  B2=0: Pruefung nach Bereich des M1-Befehls; Lesen/Opcode-Abruf nie umleiten; /RES-SPA Port 02H loescht den Indikator) mit
  Einheitstest am Baustein und Wächter `SiosBoot.V11_ZeigtSystemzeile`.  CP/A, UDOS, SCPX beruehren B2 nicht (Lade-ROM laesst B2=1).

## SIOS 0.9 (SYLp)

Gleiche Ladeadresse 0800H, gleicher Einsprung 089DH; der SYL-Lader unterscheidet sich in **einem Byte** (0405H: 6CH statt 88H,
also 6C00H Byte = 216 Sektoren statt 272).  SYSARE09 hat 27 Systembloecke (0800H..73FFH, 33 280 B Datei) mit anderer Modulfolge
(Kennungen 00-05, 10-12, 70-73, 40/41, 30-35, 30-33; Kopfflags 92/82/91/81 statt 90/94/84), Systemzeile `MONITOR ===== SIOS 1526/0.9 (M)`
bei 0EECH.  Block 0 (0800H-0BFFH) und Blocks mit gleicher CRC wie bei 1.1 (z. B. 0ffa, 001b, 49a8, 07c8) sind bitgleich.
Listing fuer 0.9 nicht erzeugt (`gen_prn.py` ist auf SYLs ausgelegt).

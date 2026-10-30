# SIOS-INIT V1.4: Schreib- und Kontrollleseprotokoll gegen den K5122 -- Befund

Stand 2026-10-30, Zweig SIOS1526 (809479d6).  Reine Analyse, **kein Eingriff unter `core/` im Hauptbaum**; die
Vorschlaege wurden in einer Kopie erprobt (`/tmp/sp/fix`, Patch `/tmp/sp/k5122_v1_v2.patch`, Test
`/tmp/sp/test_sios_init.patch`).  Belege: **[Lauf]** am Emulator, **[Code]** gelesen, **[Doku]**
`doc/trascripted/Floppy Anschlußsteuerung K 5122.md`.  Fortsetzung von `befund_init.md` (dort §4: "OUT 14H =
Byte-Slot" ist nach Doku nicht gestuetzt -- hier steht die tragfaehige Erklaerung).  Annotierter ZVE2-Abzug:
`/tmp/sp/init_zve2.asm` (RAM-Abzug A200..A6FF); ZVE1 9A00..A1FF: `/tmp/sp/w2/z1.lst`.

## 0. Ergebnis

1. **(a) ist kein OUT-Effekt, sondern ein Altbyte-Fehler des Modells.**  Nach einem MK/MK1/MR-Impuls beginnt der
   Lesestrom im Modell immer mit **4 x A1** (1 "Altbyte" + 3 Sync).  Auf der Hardware steht das Altbyte nur im
   Daten-PIO B, wenn vorher *gelesen* wurde.  Kommt der Impuls unmittelbar nach dem *Schreiben* (INIT-Pfad `A4AB`),
   gibt es keins: das erste IN liefert das erste von **3 x A1**.  INIT zaehlt genau so (3 IN vor dem INIR im Pfad
   `A4AB`, 4 IN im Pfad `A4FF`).
2. **(b) hat zwei Ursachen**: (i) Aufbau -- `--laufwerke K5600.20,K5600.20` mit der **40-Spur**-Datei
   `sios1526_dp2.hfe`: INITs Menue "1" steppt einfach, Spur 01 = Halbspur, `FloppyDriveV2` verwirft das Schreiben
   (DoubleStep) -> INIT geht in seine Wiederholungen; (ii) Modellfehler -- ein `/STR=1` von ZVE1 beendet einen
   laufenden Schreibtransfer nicht, `/BUSRQ` bleibt stehen, der neu gestartete ZVE2 laeuft *vor* dem Start-Strobe
   los -> Verklemmung (ZVE1 `9E89`, ZVE2 `A3AC`).  Mit `K5600.10` fuer B: tritt (ii) nicht auf.
3. Mit zwei kleinen Aenderungen (V1, V2, §6) laeuft INIT **ohne Gastpatch** komplett (40 Spuren, "INITIAL DISK IS
   FINISHED"), und der Hang (b) ist weg.  Volle Regression der Kopie: 4184/4184 gruen (ohne `python`,
   `format_matrix`, `wega_install`; **mit** `format_integration`).
4. Es gibt keinen Hardwarevorgang, den der Emulator nur im Lesetransfer fortschreibt: der Indexpuls laeuft in
   `K5122::update()` immer (Motor auf Drehzahl), auch im Schreibmodus.  Die Schleife `A3AC..A3B2` hat /WE=**0**
   (Spurvorlauf-Schreiben), nicht /WE=1.

## 1. Protokoll von INIT in Phasen (5,25", MFM, Menue "1": 128 B, 26 Sektoren)

Steuerport `10H`: b7 /ST, b6 /HL, b5 MR/SD, b4 MK1, b3 /STR, b2 /FR, b1 MK, b0 /WE.  Gesehene Worte:
`AD` /STR=1,/WE=1 (aus) · `A1` /STR=0,/WE=1 (scharf) · `B9` /STR=1 · `B4` /STR=0,/WE=0,MK=0 (Daten schreiben) ·
`B6` = B4 mit MK=1 (Sync-A1 schreiben) · `B5` /STR=0,/WE=1,MK1=1,MR=1 · `85` /STR=0,/WE=1,MK1=0,MR=0,MK=0
(MFM lesen) · `87` dasselbe FM.  ZVE2-Einstieg `[0001H]`: `A390` (5,25", `[9C93]` bit5=1) oder `A253` (8").

| Phase | Wer / Ort | Vorgang | Beleg |
|---|---|---|---|
| P0 | ZVE1 `9E55..9E83` | `AD`; `[9C92]`=4 (Index-Zaehler), Ctrl-PIO A `97/FF` (Index-Interrupt), `OUT 04H` (ZVE2-Reset frei), Daten-PIO B Mode 3 -> `7F` (Mode 1), `IN 16H`, `OUT 14H` | [Code] |
| P1 | ZVE1 `9E87` | `A1`: /STR 1->0, /WE=1.  Modell: `READ D1 C=n ...`, `assertBUSRQ` | [Lauf] c231240855 |
| P2 | ZVE2 `A390..A3A3` | Register, `B9` (/STR=1): ZVE2 gibt den Bus ab (+11 Takte) | [Lauf] |
| P3 | ZVE1 `9E89..9EA0` | laeuft 96 Takte, `B4` (/STR=0, **/WE=0**): Schreibbeginn.  Modell: `write_mode_=true`, `assertBUSRQ` | [Lauf] c231240962 |
| P4 | ZVE2 `A3A5..A3B2` | `OUT 14H`,4E im Wechsel mit `BIT 7,[9C93]`: Luecke schreiben **bis zum naechsten Index** (ISR `A19C` auf ZVE1 kippt bit7) | [Code] |
| P5 | ZVE2 `A3EA..A4A9` | je Sektor: Luecke, 12 x 00, `B6`, 3 x A1, `B4`, FE, c h s n crc crc, Luecke 2 (22 x 4E), 12 x 00, `B6`, 3 x A1, `B4`, FB, 128 Daten, crc, Luecke 3; jedes Byte ein `OUT 14H`/`OUT (C),r` = 1 Byte-Takt | [Lauf] 26 x B6/B4 |
| P6 | ZVE2 `A4C4..A4D9` | Luecke weiter bis zum naechsten Index: **Schreiben von Index zu Index**, 600 174 Takte = 1,22 Umdrehungen ab `B4` | [Lauf] |
| P7 | ZVE2 `A4AB..A4C2` | `B5` (/WE=1), `85` (MFM lesen, MR=0), `OUT 14H` (wirkungslos) -> `A517` | [Lauf] |
| P8 | ZVE2 `A517..A52B` | **3 IN** (A1 A1 A1; Summe der letzten zwei = 42H), **INIR 7** = FE c h s n crc crc, Vergleich mit `[9C7E..]` | [Code] |
| P9 | ZVE2 `A52D..A5BB` | Datenfeld: 5 IN (FB), 128 vergleichen, crc; naechster Sektor ueber `A4FF` | [Code] |
| P10 | ZVE2 `A4FF..A517` | Folgesektor: `B5` dann `85` unmittelbar, danach **4 IN** + INIR | [Code] |
| P11 | ZVE2 `A5E7`/`A5F6` | Ende: `[9C94]` bit0 (gut) bzw. bit2 (defekt), `AD`: ZVE2 verliert den Bus; ZVE1 wartet in `9ED3` auf `[9C94]!=0` | [Lauf] |

Hardware-Lesart [Doku §5.6.1, §4.3]: ZVE2 laeuft nur, solange /BUSRQ ansteht; /BUSRQ entsteht aus der Bereitschaft
des Daten-PIO und wird durch /STR=1 unterdrueckt.  Das Mikroprogramm ist so gebaut, dass **je Byteperiode genau ein
Datenport-Zugriff** stattfindet (`OUT 14H` beim Schreiben, `IN 16H` beim Lesen).  Beim Lesen liefert das Marken-FF
**als erstes Byte das Datenbyte der Marke** [Doku §5.3 "...als erstes Byte zur CPU uebertragen wird"; §5.5].

### 1.1 Zaehlprobe: wie viele IN zwischen Impuls und FE?

| Pfad | Lage vor dem Impuls | Impuls | IN bis INIR | Folgerung |
|---|---|---|---|---|
| FM `A305` (nach Schreiben) | Schreiben | `87` | 1 (`A30C`, `CP FC`) | erstes Byte = Marke FC: **kein Altbyte** |
| FM `A31C` (mitten im Lesen) | Lesen | `A5`,`87` | 1 (`A320`), dann INIR ab FE | **Altbyte**, Rest ab FE |
| MFM `A4AB` (nach Schreiben) | Schreiben | `B5`..`85` | **3** | A1 A1 A1, INIR = FE: **kein Altbyte** |
| MFM `A4FF` (mitten im Lesen) | Lesen | `B5`,`85` | **4** | **Altbyte** + A1 A1 A1, INIR = FE |
| Boot-ROM / SYL | Lesen | MK/MK1 | 1 verwerfen + 3 | **Altbyte** + 3 A1, FE bei buf[4] |

Eine Regel erklaert alle Zeilen: nach einem Impuls mitten im Lesen steht ein Byte im Daten-PIO B (Altbyte), nach
einem Impuls aus dem Schreiben nicht.  Das Modell kennt nur den ersten Fall (`romReadResyncTarget`: Backoff 4 MFM /
1 FM = Altbyte + Sync).

## 2. Modell neben der Hardware (`core/cards/k5122/k5122.cpp`)

| Phase | Modell | Abweichung |
|---|---|---|
| P1 `A1` | `handleCtrlPortAWrite` Z.713, ZVE1-Kontext, /WE=1: `startReadTransfer()` (Z.935), `byte_ready_`, `assertBUSRQ` | Hardware: kein Byte ohne Marke; hier Lesetransfer (folgenlos, ZVE2 gibt sofort ab) |
| P2 `B9` | Z.704 "/STR=1 von ZVE2": `releaseBUSRQ` | ok -- aber nur bei `transferring_ && !write_mode_` |
| P3 `B4` | Z.774: Schreib-DMA, `write_mode_=true`, `write_buf_.clear()`, `assertBUSRQ` | ok |
| P4-P6 | `advanceByteClock` (k5122.h:489) fordert je Byteperiode (78 Takte MFM) an; `handleDataPortAWrite` (Z.831) sammelt, `consumeByteSlot`; auch der Vorlauf bis zum Index landet in `write_buf_` | `/WE` 0->1 (`B5`) ist **nicht** das Ende: `write_mode_` lebt bis `85`, Leerlauf (20 000 Takte) oder Lese-Strobe |
| P7 `85` | Z.659: `read_enc_=MFM`; `!streaming_ok` -> `commitFormatTrack()`, `write_mode_=false`, `startReadTransfer()` -> `head_pos_` = markPos-4 | **D1**: Altbyte nach Schreiben |
| P8 | `ioRead(0x16)` Z.74 liefert `head_pos_`, `consumeByteSlot`; `OUT 14H` bei /WE=1: `handleDataPortAWrite` kehrt zurueck | stimmt zur Doku (§5.5: /ASTB beim Lesen gesperrt) |
| P10 | Z.809 MK1-Anstieg -> `resyncToNextMark` (Z.1066): naechste Marke -4 | ok (Altbyte + 3 A1) |
| Index | `update()` Z.587 laeuft immer; `commitFormatTrack` setzt die Phase 0 | ok |
| P11 `AD` | von ZVE2 im Lesen: Z.704.  Von ZVE1 oder im Schreibmodus: **kein Zweig** | **D2** |

## 3. Symptom (a)

Der zusaetzliche A1 entsteht in `handleCtrlPortAWrite` beim `85` aus P7: das Modell startet den Lesetransfer auf
`markPos-4` des ersten Sektors; `A517/A51B/A51D` verbrauchen drei A1, das vierte steht als erstes INIR-Byte im Puffer
(`[A24B] = A1 FE 00 00 01 00 EA` [Lauf, `befund_init.md` §1]).  `OUT 14H` ist unschuldig.  Der Patch `A4C0: DB 16`
hat das fehlende Altbyte aus dem Gast heraus "verbraucht" -- derselbe Effekt wie V1, aber im falschen Ort.

## 4. Symptom (b)

**Aufbau [Lauf]:** B: = `K5600.20` mit 40-Spur-Abbild -> `DoubleStep`; INIT Menue "1" steppt einfach: Spur 01 = Position 1
= keine Spur: `Vollspur-Schreiben verworfen: Kopfposition 1/Kopf 0 liegt auf dieser Diskette nicht`, Kontrolllesen
`UNFORMATIERT -> 6250 B Gap-Flux`, ZVE2 meldet Spur defekt (`A603`), ZVE1 wiederholt (`[9C98]`=5), steppt (`8D`/`CD`) und
startet neu.  Mit `K5600.10` fuer B: laufen alle 40 Spuren (40 x `FORMAT-WRITE ... OK`).  **Frage an den Anwender:**
welches Laufwerk gehoert zu `sios1526_dp2.hfe` -- oder Menuepunkt "2 DOUBLE TRACKS" mit 80-Spur-Medium.

**Ereignisfolge der Verklemmung [Lauf, `iow 0x10`]:**
```
c240544382 OUT 10H=A1  ZVE1 9E89      Start (ZVE2 neu gestartet)
c240544393 OUT 10H=B9  ZVE2 A3A5
c240544489 OUT 10H=B4  ZVE1 9EA0      Schreibbeginn -> write_mode_=true, BUSRQ
c240544849 OUT 10H=AD  ZVE1 9E59      360 Takte spaeter: ZVE1 bricht ab (Wiederholung, Altflag in [9C94])
c240545174 OUT 10H=B9  ZVE2 A3A5      ZVE2 (OUT 04H) laeuft VOR dem naechsten A1 los
c240545214 OUT 10H=A1  ZVE1 9E89      -> ZVE1 steht, ZVE2 in A3AC, K5122 READING, /BUSRQ ansteht
```
Ursache [Code]: `AD` (/STR 0->1) von ZVE1 im Schreibmodus wird nicht behandelt (Z.704 gilt nur fuer
`transferring_ && !write_mode_` und ZVE2).  `write_mode_` und `/BUSRQ` bleiben stehen; ZVE2 bekommt nach `OUT 04H`
sofort den Bus, schreibt 15 Gap-Bytes in `write_buf_` und `B9`.  Der folgende `A1` von ZVE1 sieht kein Schreiben mehr
(`commitFormatTrack`: "keine Sektoren im Strom (15 Bytes)"), `startReadTransfer`, `assertBUSRQ`: ZVE2, schon *hinter*
`B9`, laeuft in `A3A8..A3B2`, wo `OUT 14H` im Lesetransfer ignoriert wird (`handleDataPortAWrite` Z.834, kein
`consumeByteSlot`) -> `/BUSRQ` faellt nie, ZVE1 kann die Index-ISR (`A19C`, laeuft auf ZVE1) nicht ausfuehren.
Hardware: /STR=1 unterdrueckt /BUSRQ sofort [Doku §4.3, §5.6.1].

## 5. Abweichungsliste Modell <-> Hardware

| # | Abweichung | Hardware | Modell | Folge |
|---|---|---|---|---|
| D1 | Lesestrom nach dem Schreiben beginnt mit Altbyte (4 x A1) | §5.3/§5.5; Zaehlprobe §1.1 | `85`-Zweig Z.673-684 | ERR 9B/9C -- **in Kopie behoben (V1)** |
| D2 | /STR 0->1 beendet den Schreibtransfer nicht | §4.3, §5.6.1 | Z.704 nur Lesen+ZVE2 | stehender /BUSRQ, Verklemmung (b) -- **in Kopie behoben (V2)** |
| D3 | /WE 0->1 im Schreibmodus ohne Wirkung | §5.5 "sofortiges Umschalten" | `write_mode_` bis `85`/Leerlauf/Strobe | ohne Folge fuer INIT; nicht geaendert |
| D4 | Kontrolllesen teleportiert ueber Luecken (`resyncToNextMark`), Verify dauert 213 000 statt >= 400 000 Takte | Lesen in Echtzeit | Resync ohne Rotationszeit | nur Zeitbild |
| D5 | 40-Spur-Abbild im 80-Spur-Laufwerk, INIT steppt einfach | 96-tpi-Laufwerk traefe Halbspuren | `DoubleStep`, Schreiben verworfen | Aufbaufrage |
| D6 | FM (8"): erster IN nach `87` erwartet FC (`A30E`); der Faithful-Strom hat keine Indexmarke und beginnt mit Altbyte | §1.1 | kein IAM im Lesestrom | 8"-Pfad von INIT, **ungeprueft**, hier nicht behandelt |

## 6. Aenderungsvorschlag (K5122)

**V1 (D1)** -- `K5122::handleCtrlPortAWrite`, `(data & 0xF9) == 0x81`-Zweig, Fall `!streaming_ok`: vor dem Zuruecksetzen
`const bool vom_schreiben = write_mode_;`; nach `startReadTransfer()` bei `vom_schreiben && transferring_ &&
stream_has_marks_ && effReadEnc()==Encoding::MFM`: `head_pos_ = (head_pos_ + 1) % cur_track_->size()` (= markPos-3, drei
A1).  `resyncToNextMark` und der /STR-Lese-Strobe bleiben bei Altbyte + 3 A1.  Sauberer: `romReadResyncTarget(...,
bool mit_altbyte)`.

**V2 (D2)** -- `handleCtrlPortAWrite`, vor dem Block "/STR fallende Flanke": bei `write_mode_ && !(prev_ctrl_a_ & 0x08)
&& (data & 0x08)` (jeder Kontext): `commitFormatTrack()` falls `write_buf_` nicht leer, `write_mode_ = transferring_ =
byte_ready_ = dma_pending_ = false`, `byte_acc_=0`, `bus_.releaseBUSRQ()`.

Waechter vorschlagen: `K5122Test.WriteDann85Lesen_DreiA1` (Schreibstrom -> `0x85` -> die ersten 4 IN = A1 A1 A1 FE),
`K5122Test.StrAnstiegImSchreibmodus_GibtBusFrei`, `SiosInit.InitialisiertDiskette` (B: = `K5600.10`; nach `DISKETTE`
**25 Mio. Takte warten**, sonst geht das `N` verloren und INIT formatiert als 8"-FM, vgl. `befund_init.md` §4).

## 7. Erprobung in der Kopie [Lauf]

* nur V1: `SiosInit.InitialisiertDiskette` gruen (kein ERR); 4110/4110 Faelle gruen (ohne `python`, `format_matrix`,
  `format_integration`, `wega_install`).
* V1+V2: 4184/4184 gruen (jetzt **mit** `format_integration`: FORMAT.COM-Ketten, SCPX-INIT, UDOS-Format).
  `--laufwerke K5600.20,K5600.20` + 40-Spur-dp2 (der Hangaufbau): kein Hang mehr, ZVE1 kehrt ins Betriebssystem zurueck.
* nicht gelaufen: `format_matrix` (88 FORMAT-Menues), Python-Ebene, `wega_install`, 8"-INIT (D6).

## 8. Risiko

* **Boot-ROM / SYL / CP/A-BIOS-`dio`**: nehmen den `0x81`-Zweig nicht aus `write_mode_` (Lese-Strobe aus ZVE1 bzw.
  `streaming_ok`); V1 wirkungslos, die 4 x A1 (Invariante 4) bleiben.
* **CP/A FORMAT.COM**: beendet das Schreiben mit dem Lese-Strobe aus ZVE1-Kontext (Z.764) -- V1 unberuehrt.  V2 greift
  nur bei /STR=1 im Schreibmodus; die Hardware bricht dann ebenfalls ab.  `test-format` gruen.
* **SCPX INIT.COM**: nimmt den `0x81`-Zweig aus `write_mode_` (Kommentar Z.664-672); sein Leseweg "ueberspringt nur A1,
  erwartet dann FE" -> 3 statt 4 A1 unkritisch; `ScpxInit.*` gruen.
* **UDOS**: `beginWriteField`/`commitWriteField` (`we_writing_`), nicht beruehrt.
* Vor dem Merge `tools/dev.sh test-merge --alles` (enthaelt `format_matrix`).

## 9. Naechste Schritte

1. Anwender: welches Laufwerk gehoert bei SIOS 1526 zu B: / `dp2` (`K5600.10` oder 80-Spur + Menue "2")?
2. V1+V2 im Hauptbaum umsetzen, Waechter anlegen, `test-merge --alles`.
3. 8"-/FM-Pfad (D6) erst bei Bedarf.

# BS600 V02 (K7609) und BS620 (K7672): der Tastaturteil im Vergleich (Entwurf 48, AP-B8 Schritt 2)

Grundlage: die Datei `BS600` der PRG-700-Diskette (`tests/fixtures/bs600/bs600_prg700.hfe`, Version 02, `220385 HOF`) und die Datei
`BS620` (`tests/fixtures/bs600/bs620_prg710-1.hfe`, Version 04, `201288 KRU`), beide mit `tools/z80_disasm2.py --linear` je Segment
disassembliert (Ladeplan wie `selbsttest.md` §1: Datei 0000–42FF → 6D00, 4300–4EFF → 0000 Monitor, 4F00–67FF → 1000).  Beide Dateien sind
26 624 B lang und weichen in 1767 Bytes voneinander ab.

Gewissheit: **[Disk]** unmittelbar aus dem Code, **[Lauf]** im Emulator, **[?]** Deutung/offen.

## 1. Schnittstelle (gleich in beiden Fassungen) [Disk]

| Eintrag | Adresse | Wirkung | Rueckgabe |
|---|---|---|---|
| `RST 08` | 0008 | `(0E4AH) := FFH` (Tastenpuffer leer), dann Geraete-Init (§2) | – |
| `RST 30` | 0030 | **nicht wartend**: liegt ein Zeichen an, wird es geholt, umgesetzt und nach `(0E4AH)` geschrieben; danach (`003F`) `A := (0E4AH)` | `A` = letzter Code (bleibt bis zum naechsten `RST 08` stehen, `FFH` = keiner); **Z bei 82H**, sonst Flags von `CP 81H` (Z auch bei 81H) |
| `CALL 0B94` → `0115` | 0115 | **wartend**: bis ein Zeichen kommt, umsetzen, nach `(0E4AH)` | `A` = interner Code |
| `CALL 0B97` | 012A bzw. 0201 | nicht wartend, Rohzugriff | BS600: Z = nichts, sonst `A` = **roher 8279-Code**; BS620: umgesetzter Code (Unterschied, s. §2) |

Einzige RAM-Zelle der Tastatur: **`0E4AH`** (Puffer, ein Byte, kein Ring).  Kein Interrupt: beide Fassungen fragen ab (Polling) [Disk].
Benutzt wird die Tastatur nur ueber diese Einstiege (`RST 30` an vielen Stellen in Monitor/Dateidiensten/Ueberlagerung, `CALL 0B94` an 5
Stellen, `0E4AH` wird zusaetzlich an `0A35`, `0A44`, `23D9`, `70FD`, `7582`, `7F49` geschrieben — in beiden Fassungen bytegleich) [Disk].
Kein Aufrufer von `0B97` in den Dateien beider Disketten (Bytesuche `CD 97 0B`) [Disk].

### Interne Codes [Disk]

| Code | Bedeutung (Handbuch/Menuebedienung) | BS620 (K7672, Zeichen ueber SIO) | BS600 V02 (K7609, 8279-Code ueber Tabelle 02B3) |
|---|---|---|---|
| 80H | Funktionstaste | `ESC [ u` (auch `ESC O`/`ESC ?`) | 08H/48H (BS ?) |
| 81H | Funktionstaste | `ESC [ v` | 10H/50H (Sondertaste) |
| 82H | Funktionstaste (Fehlerquittung) | `ESC [ w` | 18H/58H |
| 83H | **CL / ESC** (Abbruch, Fehlerquittung) | `ESC` allein (kein Folgezeichen in ~256 Abfragen) | 28H/68H (CL) |
| 84H | **ENTER** | `0DH` | **38H (ET2)** |
| 87H / 88H | Funktionstasten | `ESC [ s` / `ESC [ t` | 09H (`$`) / 11H (`%`) |
| 89H / 8AH / 8BH / 8CH | Kursor rechts / links / hoch / runter | `ESC C` / `D` / `A` / `B` (auch hinter `[`) | 39H / 3AH / 19H / 3BH |
| 1EH | (ET1, ROM-Starttaste) | `ESC Z` | 37H (ET1) |
| 09H | Tabulator | – | 3CH |
| sonst | Zeichen | ASCII, `a`–`z` → gross | Tabelle (Ziffern, Buchstaben, Satzzeichen; Bit 6 = Umschaltebene) |

Die Tabelle `02B3–0329` (119 Bytes, Index = 8279-Code 00H–76H) steht **in beiden Dateien bytegleich**; BS620 benutzt sie nicht mehr [Disk].
8279-Codes 77H–7FH laegen hinter der Tabelle [?] (die K7609 liefert sie nach `doc/prg710/k7609_codes.csv` nicht).

## 2. Lage der Tastaturstellen in beiden Fassungen [Disk]

| Teil | BS600 V02 (K7609 an 8279 C8H/C9H) | BS620 (K7672 an K8025 SIO B 5EH/5FH) |
|---|---|---|
| `RST 08`-Sprung | `000D: JP 004D` | `000D: JP 0048` |
| `RST 08`-Init | `004D`: `LD HL,7421H / CALL 749C` (Portpaar-Tabelle, s. u.) | `0048–005C`: `OUTI`×2 an 58H (K8025-CTC K0: 07H, 01H) + `OTIR` 8 Bytes an 5FH (SIO B: 00 18 04 4C 03 C1 05 68 = Reset, ×16 2 Stopp, Rx 8 Bit, Tx 8 Bit, 9600 Bd) aus `011D` |
| `RST 30` | `0030: IN A,(C9H) / AND 07H / JR NZ,0048 / JR 003F`; `0048: CALL 0115 / JR 003F` | `0030: CALL 0201 / JR 003F` |
| Warten + Umsetzen | `0115–0129`: `CALL 012A` bis NZ, `A` = Tabelle `02B3`[Code], nach `0E4AH` | `0115–011C`: `CALL 0201` bis NZ (Tabelle der SIO-Init `011D–0126`) |
| Holen | `012A–0131`: `IN A,(C9H) / AND 07H / RET Z / IN A,(C8H)` (wie ROM `TASTE_HOLEN`) | `0201–0284` ESC-Folgen-Umsetzer, `0285–028C` `IN A,(5FH) / BIT 0,A / RET Z / IN A,(5EH)` |
| Sprungleiste | `0B97: JP 012A` | `0B97: JP 0201` |
| Init im Hauptprogramm (`2561` → `74A8`) | `74A8: LD HL,7429H / CALL 749C` = 8279: `C9H := 02H, C1H` (Betriebsart, Loeschen), `CAH := 00H` [?] | `74A8: RST 08 / NOP×5` |
| Selbsttest | kein Tastaturtest; Fehlerwarten (`097F`) benutzt `RST 08`/`RST 30` — Stufe 1 in beiden bytegleich | gleich |

Luecken: BS600 hat `0201–02B2` (178 B) und `005A–0065` mit FFH; BS620 hat `0127–0131` und `028D–02B2` FFH [Disk].

**Auffaellig:** `RST 08` von BS600 V02 zeigt auf `7421H`; dort steht keine Portpaar-Tabelle, sondern das Ende einer Menuetabelle
(`7410…`).  `749C` gibt deshalb 7 Bytes (`35 0B 0A 3D 0B 00 C9`) an **Port 71H** und 2 Bytes (`02 C1`) an **Port 72H** aus.  Die
8279-Tabelle beginnt 8 Bytes spaeter (`7429H`, so in `74A8` benutzt).  Deutung: Ueberbleibsel einer verschobenen Tabelle [?]; ob an
70H–73H im PRG 700/710 etwas sitzt, ist unbelegt [?] (im Emulator unbelegt; 710-ROM und UDOS benutzen die Ports nicht).

Beleg fuer die Abstammung: in der BS620-Diskette traegt die Datei `SYSINT` (Ladeende 32CFH) im ungenutzten Rest ihres letzten Sektors
(32DF–32FB) genau die Bytes `0115–0131` der BS600-Fassung (Warten + Umsetzen ueber `02B3` + `IN (C9H)/IN (C8H)`) — Speicherrest beim
Schreiben; BS620 entstand also aus einer K7609-Fassung [Disk; Herkunft des Rests [?]].  Die Datei `TEST` der PRG-700-Diskette
(`170890 RIE`, Adressen 0000) enthaelt ebenfalls `IN (C9H)`/`CALL 0115` an `0030`/`0048` (Monitorbruchstueck) [Disk].

## 3. Was sonst geraetespezifisch ist [Disk]

Die uebrigen Unterschiede (ausser Tastatur) in der Reihenfolge ihrer Bedeutung:

1. **Diskettentreiber** (`1000–1240`, `12FF–170C`, `1DC0–1DD6`, `190D–191C`): BS620 setzt in allen Strobe-Bytes an Port 10H **Bit 2** (`A9`→`AD`,
   `B1`→`B5`, `E9`→`ED`, `C9`→`CD`; `OUT (C),D/E` ueber `1DC7`/`1DC0` mit `SET 2`), und schrittet **doppelt** (`14AF: CALL 150B` zweimal).
   BS600 V02 schrittet einfach (48 tpi, K5600.10 — `merkposten/bs600.md` Nr. 11).  Polaritaet des Marken-FF: in beiden gleich (low).
   `190D`: BS620 waehlt nach dem Zugriff alle Laufwerke ab (`OUT (18H),FFH`).
2. **Lochband K6022**: `0447–0550` (BS600 laesst den Leser bis zum Zeichen 1EH vorlaufen, BS620 ruft statt dessen die Leser-Init `0BD0`),
   `9000–90F7` (Ueberlagerung, um 3 Bytes verschoben) [Disk; Zweck [?]].  Keine Tastaturstelle.
3. **Hauptmenue**: BS600 V02 hat 5 Eintraege (`PROG NC-WERKSTUECKE`), BS620 4 (`2515`, `2559`).  Version `AFFE` 02/04.
4. **Pruefsumme `2400`**: BS600 aktiv, BS620 `RET`.  Sollbytes `AEC0–AF00` entsprechend verschieden.
5. **Gleich in beiden** (kein Geraeteunterschied): Bildschirm (VRAM F800H, `E8H[F] = FFH`), alle `E8H`-Werte (Stufe 1 bytegleich, `25E0`),
   Fernschreiber/SIO C4H–C7H, ATP-SIO/CTC C0H–CFH, Anzeige F4H/F5H, EPROMmer D0H–D1H, ZRE-PIO 84H–87H.  Aufrufe ins ROM gibt es nur im
   Fehlerpfad des RAM-Tests A (`04BC`, `RST 38` = 3-KB-ROM des PRG 700), in beiden gleich; im 1-KB-ROM des 710/710-1 liegt dort nichts [?].
   Alle uebrigen Aufrufe `0Bxx` gehen an die eigene Sprungleiste des Monitors im RAM.

Ueberlagerungen der Diskette (`SYSCN6`, `SYSCNH` der BS620-Diskette, 7000–AFFF) tragen die BS620-Fassung von `74A8` (`RST 08`); sie
benutzen die Tastatur nur ueber `RST 30`/`0B94` [Disk].

## 4. Folgerung fuer die BS610-Rekonstruktion (Schritt 3)

Der Tastaturteil ist sauber abgegrenzt: **ersetzen genuegt** `000E`, `0030–0037`, `0048–005C`, `0115–0131`, `0B98–0B99` (Monitor) und
`74A8–74AD` (Hauptprogramm) durch die Bytes von BS600 V02.  Kein Platzproblem (alles an Ort und Stelle), `0201–028C` (SIO-Umsetzer) kann
als toter Code stehen bleiben, die Tabelle `02B3` ist schon da, die Pruefsumme ist in BS620 stillgelegt.

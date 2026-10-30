# SIOS 1526: Module T-K7633 und KUT (SYSORG09) - Befund

Stand 2026-10-30.  Quelle: `tools/sios_a5120/original/SIOS1526_SG1.hfe`, Datei `SYSORG09`; Werkzeug
`tools/sios_a5120/modul.py` (`liste`, `get`, `pruefen`, `prn`); Listings `doc/sios/t_k7633.prn`, `doc/sios/kut.prn`
(Namen/Kommentare in `tools/sios_a5120/modul_text.py`).  Stärke: **[Code]** aus dem Listing gelesen (statisch, nicht am
laufenden SIOS), **[Schluss]** gefolgert, **[?]** offen.  Laufbelege (ab 2026-10-30, nach der /IEO-Korrektur) stehen in
Abschn. 8; Abschn. 1-7 sind die statische Analyse.

## 1. Modulformat [Code, mit Gegenprobe]

- Modul = n Sätze zu 1024 B ab Position `p` (Satz 0 = Verzeichnis).  Je Satz: Byte 0 = Gruppe<<4 | Laufnummer, Byte 1 =
  80H | 10H (weiterer Satz folgt) | Variante; Byte 1022/1023 = **CRC-CCITT (Start FFFFH, big-endian) über Byte 0..1021**
  (alle Sätze von T-K7633, KUT, T-K7637, T-K7636 geprüft: stimmen).  Ladeadresse/Relokation stehen **nicht** im Modul.
- **Gegenprobe:** T-K7637-Sätze 0, 1, 3 sind **bitgleich** zu den SYSARE11-Blöcken 6, 7, 9 (2000H, 2400H, 2C00H); Satz 2 weicht nur
  in 6 Byte ab (Offset 37EH-381H: 0 im Modul, `B0 02 B4 02` im System = von SGEN eingesetzte Zeiger, und die CRC).  T-K7636 = SYSARE09
  Blöcke 6-8 bitgleich, BAB = SYSARE11 10-13 = SYSARE09 9-12.  Folge: SGEN **kopiert die Sätze 1:1 in 1-KB-Blöcke**, patcht nur wenige
  Zeiger und rechnet die CRC neu.  Der Code ist lageunabhängig (Seitenbasis in D beim Aufruf von Block+2, relative `CALL 002BH`/`003BH`),
  deshalb braucht es keine Relokationstabelle.  Das Listing nimmt 2000H (Block wie T-K7637).
- Variantenziffer (Byte 1, Low-Nibble): T-K7637 4, T-K7606 0, T-K7636 2, **T-K7633 3**, **KUT 1**.
- Aufbau T-K7633 (3 Sätze = 3072 B, Satz 0: 2002H INIT, +004FH Geräteroutine, +04AAH Befehlsroutine, +07A4H ISR; Satz 1-2: Fortsetzung);
  T-K7633 ist zu **~75 % bitgleich in Teilstücken mit T-K7637/T-K7636** (gleiche Befehls-/Eingabeverwaltung); neu ist die AUP-Anbindung.

## 2. T-K7633: Ports und Protokoll [Code]

Benutzte E/A (vollständig): **IN 41H** (ISR 27BFH), **IN 42H** (2033H Typ, 28C7H Terminal, 2B2CH Kunde), **OUT 43H** (202FH Reset,
208AH Lampen), **OUT 00H** (INT-BS, Makroebene).  **Nichts** sonst: kein 40H, keine PIO/CTC-Programmierung.

| Vorgang | Befund |
|---|---|
| Init 2002H | Zeiger (0110H)=+004FH, (010EH)=+04AAH; Gerätetabelle `(0301H)=0EH` (Gerät 1 = Dialog, Eintrag 010EH); IM-2-Slot **027CH** (CTC-Kanal 2) := ISR +07A4H |
| Reset | `OUT (43H),FFH` (202FH) |
| Typ | **ohne Gültigkeitstest** `IN A,(42H)` bis High-Nibble == **80H**; Zählschleife 65535 Lesungen (~0,5 s), sonst Fehler (BC=2032H DE=424BH); Low-Nibble -> (00B1H), von der Gerätefrage (Code FFH) zurückgemeldet |
| Abtastung | ISR 07A4H im **Zeittakt (CTC-Kanal 2, ~1,3 ms)**, `IN A,(41H) AND 03H`; **kein AUP-Interrupt** |
| Bit 1 von IN 41H | **Terminaltastatur K7633**: `IN 42H` -> Taste (28C7H) |
| Bit 0 (allein) von IN 41H | **Kundentastatur**: Code wird **nur dann** gelesen, wenn eine Kundeneingabe aktiv ist ((01BAH) Bit 0); sonst bleibt er anstehen |
| Beide Bits | Bit 1 zuerst (Bit 0 im nächsten Takt) |
| Lampen/Hupe | `OUT (43H)` nur bei **Änderung** gegen Abbild (076AH); Byte = FCH-Basis, Bit 0 = SEL, Bit 1 = ALPHA (beide aus (00CAH) Bit 0/1, 0 = an), Bit 4 = GET (0 = an, solange Kundeneingabe aktiv), Bit 5 = Fehlerlampe (0 = an, wenn (00CBH) Bit 7), Bit 6 = Hupe (0 = an, **einmaliger** Anstoß, ausgelöst durch Befehl 01H -> (004EH) Bit 2), Bit 7 = 1 (2060H-208CH) |

**Tastencodes im Treiber (28C7H-2B5FH):** Code = IN 42H.  High-Nibble **A0H**: `A0H-A7H` kippt Bit n in (00CAH) (A0H = F10 = **SEL**, A1H = F05 =
**ALPHA**: die Umschaltung steckt im SIOS, nicht in der Tastatur - die Lampe folgt dem Zustand), `A8H-AFH` (2AFEH): AFH löscht (00CBH) Bit 7
(F12 = Fehler-/Sperre quittieren), A8H kippt Bit 0, sonst `(Code AND 3)<<1 XOR (00CBH)`: **A9H (SS2) kippt Bit 1, AAH (SS1) kippt Bit 2**.
Codes **>= C0H** = Funktions-/Ende-Text-Tasten (Weiterleitung als Ereignis, 2A53H); **30H-39H, 2DH** = Ziffern/Minus (2974H, Puffer ab 009FH
abwärts); 11H/12H/10H/19H/09H Steuercodes; Codes < 20H ohne Eingabebereich Fehler.  Es gibt **keine Codetabelle im Treiber**: die
Umsetzung Taste -> Code liegt vollständig in der AUP-EPROM-Tabelle (aup_analyse §1.5).  Buchstaben kommen als ASCII (41H-5AH) direkt;
das Modul prüft sie nicht gegen eine Tabelle -> die AUP muss **mit** Alpha-Tabelle liefern, sobald ALPHA aktiv ist.

**Schlüsselschalter:** SS1/SS2 senden AAH/A9H; (00CBH) Bit 2/Bit 1 **kippen bei jedem Code** (Toggle).  Der Rest der Bediensicherung
(Ebene-Wert, Zelle 076BH bei der K7637) ist hier **anders**: kein 076BH, kein Stopfen-Wert, nur zwei Bits in (00CBH).  Das passt zu
zwei Schlüsseln (Anwender/Administrator); welches Bit welche Ebene ist, entscheiden die Verbraucher (siehe `tastatur_bediensicherung.md`
§3: Bit 1 = Chefebene, K7637-Wert 1).  [Schluss] SS2 (A9H, Bit 1) = Administrator/Chef, SS1 (AAH, Bit 2) = Anwender.

## 3. KUT (Satz 25) [Code]

**Kundentastatur-Gerätetreiber** (Name = Kundentastatur), Gerät **8** (`(0308H)=A8H` = Eintrag 01A8H), Befehls-/Ereignisroutine +0090H/+001BH.
Kein Eingabeport: die Codes liest `KUT_CODE` im **Modul T-K7633** (2B1DH).  Einziger E/A: `OUT (43H)` mit gesetztem Bit 4 (GET-LED aus) am
Ende der Eingabe.  Funktion: **PIN-Eingabe fester Länge**: Befehl 2DH mit Länge (0051H)/Puffer (0052H) startet (SVC auf Gerät 8, Befehl 01H);
`(01BAH)` Bit 0 = aktiv -> T-K7633 schaltet die GET-LED ein und liest die Codes; **19H = Korrektur** (Puffer nullen, von vorn), sonst Zeichen
ablegen; nach der vorgegebenen Länge fertig (`(01ACH)=FFH`, `OUT 00H`) - **kein Ende-Taste**.  Passt zur Tabelle (Kundentastatur 0-9, zwei Mal
19H).  Abbruch durch (01B1H)=FFH.  Länge/Wertebereich der Ziffern prüft der Treiber nicht (Fehler 06 nur bei falschem Pufferbereich).

## 4. Annahmen im Modell (core/cards/aup) gegen den Treiber

| Annahme | Beleg | Urteil |
|---|---|---|
| **A1** Gültigkeit Bit 0 = K7633, Bit 1 = Kunde | ISR 27BFH: **Bit 1 = Terminal**, Bit 0 allein = Kunde | **widerlegt (vertauscht)** [Code].  `Config::bit_terminal=2`, `bit_kunde=1` |
| Rest der Bits 0, aktiv high | `AND 03H` maskiert, `Z` = nichts | bestätigt (Rest egal) |
| A2 `OUT 40H-42H` wirkungslos, `IN 40H`/`43H` = FFH | Treiber liest nie 40H/43H.  **BAB** (Bildschirm, 3000H) schreibt `OUT (40H),03H` bei Init (301EH) und `OUT (40H),00H` (36CBH) **unbedingt** | bestätigt: 40H-Schreiben **muss** folgenlos bleiben, sonst stört der BAB (K7025-Register!) die AUP |
| A3 Tabelle „mit Alpha“ solange ALPHA-LED an | ALPHA-Zustand = (00CAH) Bit 1 (Taste A1H = F05), SIOS setzt nur die LED; Buchstaben ohne Treibertabelle | **gestützt [Schluss]**: AUP wählt nach Kommandobit 1 (Bit 1 = 0) oder nach der F05-Taste selbst.  Auflösung der Mehrdeutigkeit: **F05 sendet A1H in beiden Tabellen** (`A1H` in beiden Spalten) - SIOS führt den Zustand nach, deshalb braucht die AUP den Zustand **aus dem Kommando** (3 Bits: SEL, ALPHA) |
| A4 Entprellung 16 ms, Wiederholung | Treiber ohne Wiederholung/Entprellung; Abtastung 1,3 ms | offen [?] (Firmware) - unkritisch |
| A5 jedes Kommando mit Bit 6 = 0 ist ein Hupsignal | Treiber sendet das Kommando mit Bit 6 = 0 **einmal** je Anstoß (Befehl 01H), nächstes Kommando setzt Bit 6 wieder | bestätigt |
| A6 Kommando ohne Bit 7 verwerfen, FFH = Reset | Treiber setzt Bit 7 immer (OR FCH), Reset `FFH` bei Init | bestätigt |
| A7 Schlüssel **nur beim Einschalten** Code, Ausschalten nichts | Treiber **kippt** Bit bei jedem AAH/A9H; ohne Code beim Abziehen bliebe die Ebene für immer gesetzt | **widerlegt [Schluss]**: **beide Flanken** senden den Code (wie K7637-SL) |
| A7 Code 00H liefert nichts | Treiber: Codes < 20H ohne Eingabebereich = Fehler; 00H nie erwartet | bestätigt (nicht senden) |
| A8 Schlange, nichts geht verloren | Treiber holt ein Code je 1,3-ms-Takt | unkritisch |
| Kein Interrupt | ISR im CTC-Kanal-2-Takt, kein AUP-Vektor | bestätigt |
| *neu* **TYP nach Reset** | Init liest `IN 42H` ohne Gültigkeitsbit bis High-Nibble 80H (0,5 s) | **fehlt im Modell**: nach `OUT 43H,FFH` muss `IN 42H` **80H-8FH** liefern (Low-Nibble = Variante, K7633 [?] - K7637 hat 0), und die Gültigkeit darf nicht Voraussetzung sein.  Heute: A6 = 0 -> SIOS meldet Fehler `BK`-Zeitablauf |
| *neu* GET-LED / Fehlerlampe / SEL / ALPHA | Lampenbyte-Bits wie aup_analyse §1.3 (Bit 4 GET, 5 Fehler, 0 SEL, 1 ALPHA) | bestätigt; Zuordnung Bit 0 = SEL, Bit 1 = ALPHA (Handbuch: SEL = Selektortaste) |

## 5. Konkret für den K8924 (Modell nachziehen)

1. **TYP nach Reset** liefern: nach `FFH` (und nach /RESET) `a6 = 0x80 | typ`, `IN 42H` ohne Gültigkeitslöschung-Zwang (Wert bleibt stehen; der Treiber liest bis zu 65535 Mal).  Variante: `Config::typ` (Vorgabe 0x80).  Danach A6 = 0.
2. **Gültigkeitsbits tauschen** (A1): Terminal = Bit 1 (02H), Kunde = Bit 0 (01H).  Tests `AUP.*`, `py_aup`, `AupMaschine.*` und `tools/auptest` anpassen.
3. **Schlüssel auf beiden Flanken** senden (A7); Reset/Einschalten mit gesteckten Schlüsseln: Code einmal je Schlüssel (SIOS startet bei (00CBH) = 0).
4. 40H-Schreiben bleibt wirkungslos (Konflikt mit BAB nur bei Karten gleicher Adresse - Hinweis in `aup.h`).

## 6. SLE / HLE / SIM / Sparkassenhardware

Diese Module (Satz 98-105, nur auf Diskette 2) **liegen nicht auf SG1**.  Aus SG1 folgt nur: Verzeichnisnamen `LBE`(1), `MB`(1), `SLE`(4), `HLE 2`(1),
`HLE 4`(1), `SIM`(2) und dieselben Namen in `PHASELB` (`SYSD` kennt sie; keine Ports/Texte).  [Schluss] **HLE 2/4** = Handleseeinheit mit 2 bzw. 4
Lesern (AHL K1003 = „Adapter für Handlese-Einheit für max. 4 Magnetkartenlesegeräte“, Entwurf 33 §5), **SIM** = SIMO-Sicherheitsmodul (an der AUP-
Sonderschnittstelle SIO/CTC B0-B7H); SLE [?] unbekannt.  Treiber/Ports sind **nur aus Diskette 2 (SG2) zu belegen**.  T-K7633 und KUT nennen weder
SLE/HLE/SIM noch Sparkassentexte; auf die Sonderschnittstelle (B0H-B7H) greift keines der beiden zu.

## 7. Offen / nächste Schritte

1. Typ-Low-Nibble der K7633 und ob die AUP den Schlüsselzustand **beim Anlauf** meldet (Bezug: SIOS-Anlauf bei (00CBH) = 0).
2. Alpha-Tabellenwahl: AUP-EPROM fehlt; bis Beleg `aup_alpha=led` beibehalten.
3. Verbraucher von (00CBH) Bit 1/Bit 2 im Systemcode (Programmiermodus MEM/POU/…) suchen.
4. Diskette 2 (SG2) für SLE/HLE/SIM/V24-Module beschaffen.
5. Gegenprobe am laufenden SIOS, sobald NFND gelöst: Haltepunkt `27BF`, IN 41 = 02/01.

## 8. Gegenprobe am laufenden SIOS (2026-10-30, Zweig sios-aup) [Lauf]

Wächter `SiosSgen.GeneriertEinSystemMitK7633UndBootetAmAup` (`tests/integration/test_sios_sgen.cpp`): SGEN (SG1, Version 09) mit
GRUND + **T-K7633** + KUT + BAB + PH-FD640 + LOG FD2 + LOG FD1 (statt T-K7637) läuft bis `SGEN FINISHED`; die erzeugte 40-Spur-Diskette
bootet in einem **K5600.10** an einem A5120 mit AUP bis `MONITOR ===== SIOS 1526/0.9 (M)_` (das System ist 0.9: SYSORG09; im K5600.20
bootet die 40-Spur-Diskette nicht, wie bei SYLp).  Belegt damit am laufenden System:

- **TYP**: `IN 42H` = 80H an PC 2035H (Init-Schleife), ohne Gültigkeitsbit; ohne das Typzeichen käme der Fehler nach ~0,5 s.
- **Gültigkeitsbit 1 = Terminal**: Tasten über die K7633 (ALPHA = F05, D, A, T, ET1 = Position 40H) ergeben `DAT     00.00.00`,
  ET2 (41H) bringt `(M)_` zurück.
- **Schlüsselschalter SS2 -> Bit 1 von (00CBH)**: 0 vor, 1 nach dem Einschalten, wieder 0 nach dem Ausschalten (kippt je Drehung).
- **ALPHA-Lampe** folgt dem Kommando (Bit 1 = 0 nach F05); die Tabelle „mit Alpha“ kommt aus dem Kommando.
- **NEU: `OUT 43H,FFH` ist auch das Lampenbyte „alles aus“** (PC 2C7CH im laufenden System, lange nach der Initialisierung).  Ein Modell, das
  FFH als Reset behandelt UND danach das Typzeichen/den Schlüsselanlauf erneut meldet oder Tastenwartende verwirft, stört das System.
  Das Modell: FFH löscht Gültigkeit und Schlange und legt das Typzeichen nach A6, meldet aber keinen Anlauf.
- Offen: Typ-Low-Nibble der K7633 (Vorgabe 0), ob die AUP-Firmware nach FFH wirklich die Schlange verwirft, Anlauf der Schlüsselschalter nach /RESET.

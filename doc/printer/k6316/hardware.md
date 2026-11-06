# K6316 — Hardwarebefund (Entwurf 49, WP0a)

Stand 2026-11-06 (WP2a): Logik vollständig gelesen und gegen das ROM **und den laufenden Emulator** geprüft; die offenen Punkte der
ersten Fassung sind in §9 beantwortet, **die Tabellen in §3 und §5 sind dort berichtigt** (Korrekturen sind mit „**WP2a:**“ markiert).  Marken: `[S:n]` Schaltplan-Scan `original/schaltplan_scans/n.jpg`, `[P:n]` Seite von `original/K6316_Plaene.pdf`,
`[H]` Servicehandbuch, `[F:adr]` IFSS-Firmware (Adresse im Gesamtabbild I1…I5), `[?]` Annahme.

Die Signalzuordnung ist aus Drahtnummern handschriftlicher Pläne gelesen.  **Unabhängige Gegenprobe:** die Ein-/Ausgabemasken, mit
denen die Firmware die drei PIOs einrichtet (§3), treffen alle Eingänge und Ausgänge, die der Plan zeigt.  Das ist der Beleg, dass Decoder
und Drahtlesung stimmen.

## 1  Bausteine

Steckeinheit Logik `[S:03]` (56-262-7100-8): ZVE U880 D6, CTC U857 D5 + D10, PIO U855 D11 + D12 + D14, Decoder DS8205 D8, Bustreiber
DS8216 D4/D7/D9, LED-Treiber D204 D13, Quarz **9830,4 kHz** mit Teiler D2 → **φ = 2,4576 MHz** `[S:03]`, bestätigt durch den Baudtakt (§4).

Speichermodul A3 SPM (5 × U 2716, RAM U 214 = 2114) `[H]`: **kein Stromlaufplan in den Unterlagen**.  Schnittstellenmodul: IFSS `[P:11]`
(U856 D1, Optokoppler MB104/MB101, Sende-/Empfangsschleife, Varianten aktiv/passiv per Brücken), V.24 `[P:10, P:16, P:18]`, IFSP `[S:19]`.

## 2  Speicher und E/A-Karte

| Bereich | Inhalt | Beleg |
|---|---|---|
| 0000H–27FFH | 5 × 2716, Reihenfolge I1…I5 / V1…V5 | `[F]`: Code läuft über die Chipgrenzen, Sprungziele passen; Bestückungsplan nicht vorhanden |
| 3800H–3BFFH | RAM 1 KiB (2 × 2114), `SP` = 3C00H, `I` = 38H | `[F:0000]`; LDIR löscht bis 3C23H und überschreibt danach die Vektoren — **WP2a:** keine Spiegelung nötig; Lesen ab 2800H (außerhalb ROM/RAM) liefert FFH, Schreiben ab 3C00H geht ins Leere, die Firmware läuft damit `[E]` |

Decoder D8 `[S:03]`: `A` = A2, `B` = A3, `C` = A5, freigegeben durch A7 = 1, A6 = 0, A4 = 0 (nur mit /IORQ über die Bausteine selbst):

| E/A | CS | Baustein | Registerfolge |
|---|---|---|---|
| 80H–83H | O0 | PIO D12 | A-Daten, B-Daten, A-Steuer, B-Steuer |
| 84H–87H | O1 | CTC D5 | Kanal 0–3 |
| 88H–8BH | O2 | CTC D10 | Kanal 0–3 |
| 8CH–8FH | O3 | PIO D14 | wie oben |
| A0H–A3H | O4 = /CS4 | Schnittstellenmodul (X502), IFSS frei | |
| A4H–A7H | O5 = /CS5 | SIO am Modul: A-Daten, B-Daten, A-Steuer, B-Steuer | `[P:11]` B/A = A0, C/D = A1 |
| A8H–ABH | O6 | PIO D11 | wie oben |
| ACH–AFH | O7 | nicht benutzt | |

Die Firmware benutzt genau diese Adressen (IFSS: 80,81,82,83,84,87,88,8A,8B,8C,8D,A5,A7,A8,A9 + 8E,8F,AA,AB über `OUT (C)`; V.24
zusätzlich A4, A6).  Im IFSS-Satz läuft die SIO über **Kanal B**, im V.24-Satz über **Kanal A**.

## 3  PIO-Belegung

Einrichtung: `[F:0055–0072]` Mode 3 mit Masken `8EH←8C/8FH←00`, `AAH←07/ABH←C4`, `82H←07/83H←45`.

| Tor | Bit | Signal | Richt. | Bedeutung |
|---|---|---|---|---|
| D12 A (80H) | 0 | FSO | ein | **WP2a:** Lichtschranke oben im Vorsteckschacht, 1 = Papier davor `[P:8 Manual 5.6.3, F:0524]` (war „Spannungsüberwachung [?]“) |
| | 1 | /ITON | ein | Taste ON/OFF-LINE |
| | 2 | FSU | ein | **WP2a:** Lichtschranke unten, 1 = Papier davor (`EA6H`) |
| | 3 | SFH | aus | Papiermotor Haltestrom |
| | 4–7 | /SF1–/SF4 | aus | Papiermotor Phasen (`OUT (80H)` aus Tabelle, `[F:0E81]`) |
| D12 B (81H) | 0 | /G1 | ein | **Taktscheibe Druckwagen** (Interrupt, Vektor 22H `[F:0554]`) |
| | 1,3,5,7 | /SW1, /SW2, /SW3, /SW4 | aus | Wagenmotor Phasen; Fortschalten durch Rotieren um 2 Bit `[F:0824/0831]` |
| | 2 | MHA | ein | **WP2a:** 1 = Haube offen oder Kopf abgeschwenkt; 0 = in Ordnung `[F:083A,0AA2]` `[H Manual 5.2.1/5.5.2]` |
| | 4 | /SWH | aus | Wagenmotor Haltestrom (`OR 10H`) |
| | 6 | PE | ein | Papierende, **1 = Ende** `[F:0D29]`; treibt über D13 zugleich die Lampe H3 `[S:03]` |
| D14 A (8CH) | 0 | /DN9 | aus | Nadel 9 (0 = schlägt an, wenn /DTFG = 0) |
| | 1 | /DTFG | aus | Drucktaktfreigabe: **0 = Endstufen frei**, 1 setzt die Timer zurück `[H Service 1.2.4.2, S:08]` |
| | 2 | TPVV | ein | Taste Papiervorschub vor (+ Matrixspalte) |
| | 3 | TPVR | ein | Taste Papiervorschub rück (+ Matrixspalte) |
| | 4 | Draht 73 → D13 → /LON | aus | H5 ON-LINE, **1 = an** (D13 invertiert) |
| | 5 | Draht 74 → D13 → /LTEST | aus | H1 TEST, 1 = an |
| | 6 | Draht 75 → D13 → /LTF | aus | H2 FEHLER, 1 = an (blinkt als Bedienfehler) |
| | 7 | /MELO | ein | Drehschalter-Kontakt Endlos (0 = Stellung Endlos), s. §9 Nr. 2 |
| D14 B (8DH) | 0–7 | /DN8…/DN1 | aus | Nadeln 8…1 (Bit 0 = /DN8) |
| D11 A (A8H) | 0 | TSCH | ein | Taste Schneiden (+ Matrixspalte) |
| | 1 | TBAW | ein | Taste Beleg/Auswurf [?] (+ Matrixspalte) |
| | 2 | DIL | ein | Matrixspalte DIL-Schalter |
| | 3 | SVH | aus | Vorsteckmotor Haltestrom |
| | 4–7 | /SV1–/SV4 | aus | Vorsteckmotor Phasen `[F:0E74]` |
| D11 B (A9H) | 0 | /YABU | aus | Leitblechmagnet unten (ULA), 0 = Magnet an `[S:08]` |
| | 1 | AM4 | aus | Matrixzeile 4 (**WP2a:** nicht die Lampe H4 — die hängt an /SPAUS, s. §9 Nr. 1) |
| | 2 | /MAUF | ein | Drehschalter-Kontakt Aufwickeln (0 = Stellung Aufwickeln) `[F:0503]` |
| | 3 | /YABO | aus | Leitblechmagnet oben (OLA), 0 = an |
| | 4 | AM1 | aus | Matrixzeile 1 |
| | 5 | AM3 | aus | Matrixzeile 3 |
| | 6 | /MPV | ein | Meldekontakt am Vorsteck (Adapterplatte X3), im Ruhezustand 1; Herkunft [?] (Kandidat: Mikroschalter Einzugshilfe, Service 4.2.18.2) |
| | 7 | AM2 | **ein/aus** | Matrixzeile 2; bei der Einrichtung Eingang — Umschaltung in der Firmware prüfen |

`[F:04F7]`: Bit 7 von 8CH (/MELO) und Bit 2 von A9H (/MAUF) ergeben die Betriebsart 37H/38H/39H — das sind die **drei Stellungen des
Drehschalters** Aufwickeln/Schneiden/Endlos, also zwei Kontakte, keine Melder.  Die Beschriftung oben ist insoweit vorläufig.

`[F:00C9…]` beim Einschalten: TPVV → Selbsttest, TPVR → Testfunktion, /ITON → Sonderzweig 0524H, TSCH → 01ABH (vgl. Manual §5.2).

## 4  Zeitgeber und Interrupts

`IM 2`, `I` = 38H `[F:0009, 0053]`:

| Vektor | Quelle | Beleg |
|---|---|---|
| 00H–06H | CTC D5, Kanal 0–3 | `OUT (84H),00` `[F:0562]`; Kanäle 1/2 als Zeitgeber mit Interrupt (`B7H`, Teiler 256) `[F:0814]` |
| 08H–0EH | CTC D10, Kanal 0–3 | `OUT (88H),08` `[F:0085]` |
| 10H–1EH | SIO | `WR2 = 10H` `[F:00BE]` |
| 20H / 22H | PIO D12 A / B | `[F:0550–0556]` |

* **Baudtakt:** CTC D10 Kanal 2 = `17H`, Zeitkonstante 1 `[F:008D]` → φ/16 = **153,6 kHz** = 16 × 9600 Bd, geführt als RTC zum Modul `[S:03, P:11]`.
* IFSS-SIO: `WR4 = 45H` (×16, 1 Stopp, ungerade Parität), `WR3 = 61H` (7 Bit), `WR5 = AAH`, `WR1 = 15H` `[F:00A7]`.
* Kette: D5.IEO → D10.IEI (Draht 119), D10.IEO → D12.IEI (Draht 28), D12.IEO → Modul /JEO (Draht 29) `[S:03]`.  Die Bausteine ohne
  Vektor (D11, D14) können keinen Interrupt auslösen; ihre IEI-Beschaltung ist deshalb unerheblich.
* **WP2a:** alle CTC-Kanäle beider Firmwaresätze laufen im **Zeitgeberbetrieb** (Steuerwörter 33H/97H/B7H/17H am Lauf gesammelt): die
  CLK/TRG-Eingänge werden nie gebraucht `[F]`; ZC/TO-Ausgänge werden nirgends verdrahtet gebraucht.  CTC D10 Kanal 2 = Baudtakt;
  **im V.24-Satz wählen die DIL-Schalter S24–S26 die Zeitkonstante** (§9 Nr. 6).

## 5  Bedienfeld und Schalter

Adapterplatte `[S:10]`: **16 DIL-Schalter S11–S26** als Diodenmatrix.  Zeilen: AM1 (Draht 28, nur S14), AM2 (50), AM3 (51), AM4 (52).
Spalten: TBAW (42), TSCH (43), TPVR (44), TPVV (45), DIL (53).  Schaltet eine Zeile auf low und liest die Firmware eine Spalte, sieht sie den Schalter
in Reihe zur Diode.  Die Bedientasten liegen parallel auf denselben Spalten.

| Zeile (Draht) | TBAW (42) | TSCH (43) | TPVR (44) | TPVV (45) | DIL (53) |
|---|---|---|---|---|---|
| AM1 (28) | | | | | S14 |
| AM2 (50) | S24 | S22 | S26 | S25 | S23 |
| AM3 (51) | S19 | S21 | S17 | S18 | S20 |
| AM4 (52) | S11 | S13 | S16 | S15 | S12 |

Funktion der einzelnen Schalter: §9 Nr. 6.  S14 hängt an Diode V1 und (Brücke XV) an einer zweiten Leitung zum Draht 55 `[S:10]` — ob das
einen Unterschied macht, ist `[?]`; das Modell behandelt S14 wie die übrigen (Zeile AM1, Spalte DIL).

LEDs `[S:03, S:10]`: /LTEST (H1), /LTF (H2), /LPE (H3), LSPAUS (H4), /LON (H5) und die Netzlampe im Schalter — Zuordnung `[M:9]`, s. §9 Nr. 1.
Summer: im K6316 nicht vorhanden `[S:03/10, P:Bedienfeld]`.

## 6  Leistungsseite (für die Emulation nur als Signal)

Wagenmotor 6°, 60 Vollschritte je Umdrehung, Taktscheibe 30 Impulse je Umdrehung (`/G1`) `[H]`; Nadelfrequenz 500 Hz, 9 Nadeln in versetzten
Spalten, Ansteuerung je Richtung versetzt `[H]`.  Papiermotor und Vorsteckmotor: je 4 Phasen + Halten.  Magnete /YABU, /YABO.
Schneiden: **kein Signal — rein mechanisch**, s. §9 Nr. 4.

## 7  Kennung der Firmwaresätze

Der Selbsttest liest sie aus dem ROM: IFSS `540` (24F8H) / `19461306` (27F8H), V.24 `501` / `15263105`.  Damit lässt sich jedes Abbild prüfen.

## 8  Offene Punkte der ersten Fassung

Alle vier Punkte sind in WP2a beantwortet (§9).  Dieser Abschnitt bleibt als Verweis.

## 9  Befunde WP2a (2026-11-06)

Marken zusätzlich: `[M:n]` Seite n von `K6316_Manual.pdf`, `[V:n]` Seite n von `K6316_Service.pdf` (OCR), `[E]` am laufenden Emulator
gemessen (Wächter in `tests/integration/test_k6316.cpp`, Merkposten Nr. 20 ff.).

1. **Bedienfeld `[M:9, Abb. 4]`, `[S:03/10]`.**  Tasten (Diodenmatrix-Spalten, Taste = Spalte über Diode auf 5 V, Spalten über 33 kΩ nach 0 V, also
   **gedrückt = 1**, unabhängig von den Zeilen): S1 ON/OFF-LINE (/ITON, Pull-up, **gedrückt = 0**, löst einen PIO-Interrupt aus), S2 LINE FEED ▲
   (TPVV), S3 LINE FEED ▼ (TPVR), S4 Belegauswurf „BA“ (TBAW), S5 Schneiden (TSCH).  Lampen: **H1 TEST** gelb (D14 A.5), **H2 FEHLER** rot (D14 A.6; dauernd =
   technischer Fehler, blinkt = Bedienfehler `[M:8]`), **H3 PAPIER** gelb (= PE, D13), **H4 BEREIT** grün, **H5 ON-LINE** grün (D14 A.4).  D13 (D204) sind
   Inverter: Ausgangsbit 1 = Lampe an.  **Berichtigung:** H4 hängt **nicht** an AM4, sondern an /SPAUS der Spannungsüberwachung (`ISPAUS` am Stecker X528, Blatt 03
   unten, invertiert über den fünften D13-Inverter): sie leuchtet, solange die Betriebsspannungen stimmen, ohne CPU-Zutun.  Beim Einschalten gelten
   (`[M:6/7, 9–10]`, Firmware `00C9H`): S5 gehalten bis der Ausdruck beginnt = Selbsttest mit Druck; S5+S4 = Selbsttest ohne Druck (Wagenmotor, Papier); S1 =
   Lichtschrankentest (H2/H5 = FSO/FSU ohne Papier, Firmware `0524H`); S2 = Hexadezimaldarstellung; S3 = Test der Vorsteckeinrichtung.  Lampen je Zustand
   `[M:7]`: ON-LINE H4+H5; Selbsttest H1+H4.  **Der Emulator zeigt H1 im Selbsttest, H5 aus** — passt zur Tabelle (ON-LINE-Lampe erlischt, weil der Test
   OFF-LINE läuft [E]).
2. **Drehschalter** `[F:04F7H, 0EB0H]`, `[M:7, 24]`: zwei Kontakte.  /MELO = 0: Stellung **Endlos** (Code 37H); /MELO = 1 und /MAUF = 0: **Aufwickeln** (38H); beide 1:
   **Schneiden** (39H).  Die Firmware antwortet auf „Gerätekennung“ mit `ESC [ <37|38|39> c`, Schneiden nur bei 39H (`CP 39H`).  Bit 0 des Codes (37/39 = 1, 38 = 0)
   gibt Papier rückwärts frei: in Aufwickeln gesperrt `[M:7]`.  Gelesen wird beim Netz-Ein.
3. **MHA, Kopf, Haube** `[F:083A/0AA2]`, `[M:7 Abb. 3, 5.2.1, 5.5.2]`, `[V:55 Einstellung 4.2.18]`: solange MHA = 1 blinkt die Firmware H2 (CTC D5 Kanal 3 als Taktgeber,
   Rücknahme `0AA2H`) und wartet; erst bei MHA = 0 beginnt die Synchronisation.  Servicehandbuch nennt getrennte Mikroschalter für vordere Haube und
   Druckkopfverstellung; dass beide **in Reihe auf einer Leitung** liegen, ist die einfachste Lesart `[?]` (der Schaltplan zeigt nur die eine Leitung MHA an Adapterplatte X2).
   /MPV: Ruhelage 1 (`029DH`), mit 0 reagiert die Firmware wie auf einen Beleg am Vorsteck; **Herkunft [?]**.  FSO/FSU: Lichtschrankenplatte `[S:11]`
   (zwei Gabellichtschranken, Schmitt-Trigger je Platte), Pegel 1 = Papier.  **/YABU, /YABO** sind die Leitblechmagnete unten/oben (`ULA`/`OLA`, Blatt 08);
   `0ECDH` zieht je nach Papierrichtung einen davon kurz, DIL-S14 Aus lässt Papiertransport nur am Rand zu `[M:11]` — im Modell ohne Wirkung, die
   Pegel sind lesbar (`K6316Steuerung::magnetUnten/Oben`).
4. **Schneiden `[V:56, 4.2.19]`**: **kein Signal**.  Die Schneideinrichtung wird mechanisch von der Wagenlage betätigt: die rechte Anschlagschraube löst in der
   *maximalen Endstellung* des Wagens aus, die linke setzt am linken Anschlag zurück.  Die Firmware (Betriebsart 39H, S5+S4 offline) fährt den Wagen dazu auf
   Position **1621 Halbschritte** [E] (≈ 10,8″, hinter dem Druckbereich), danach schiebt sie den Beleg 225 Schritte (≈ 53 mm) aus.  Modell: ab
   `Config::schnitt_pos` = 1600 schneidet das Messer einmal (gespannt wird bei ≤ 120), die Blattgrenze liegt `schnitt_abstand_y` = 30 Einheiten (3,5 mm,
   kleinster Fußabstand `[M:4, 2.6]`) unter der Zeile der Nadel 7.  **Die Firmware schneidet unmittelbar nach der letzten Zeile, ohne vorher vorzuschieben [E].**
5. **Wagenmotor `[V:6–10]`**: 6°-Motor, 60 Vollschritte je Umdrehung, Taktscheibe 30 Zähne ⇒ /G1 wechselt je Vollschritt.  **Ein Vollschritt = 1/75″**
   (3 Motorschritte = 4 Drucktakte zu 1/100″ = 0,04″; bei 100 Z/s und 10 Z/Zoll 750 Hz `[V:8, Tab. 2]`).  Phasen /SW1–/SW4 (D12 B.1/3/5/7), **steigende
   Phasennummer = Wagen nach links** (Tabelle 1, Synchronisation: /SW2+/SW3, /SW3+/SW4, /SW4+/SW1 …).  /SWH = Halten (1 Phase) / Laufen (2 Phasen).
   Synchronisation `[V:10]`: 2,5 Hochlaufschritte, Fahrt gegen den **linken Anschlag**, der Stillstand von /G1 wird an der Zeit erkannt (Havarie bei
   Blockierung `[V:11]`), 80 ms Beruhigung, 3,5 Schritte zurück, 80 ms, fertig.  Im Modell (`SchrittWagen`/`PhasenMotor`): Position in **Halbschritten**
   (1/150″, 2 Pixel = 300 dpi), harte Anschläge (`pos_min` 0, `pos_max` 1800), keine Trägheit; die **Kopflage zwischen den Schritten** läuft gleichmäßig
   auf die neue Stellung zu (`positionPixel`) — die Firmware feuert zwischen den Schritten (1/10″: 4 Drucktakte je 3 Schritte), ohne das lägen alle Punkte auf
   Schrittpositionen.  Papier-/Vorsteckmotor: 2-Phasenmotoren mit je 4 TTL-Signalen (/SF1–4, /SV1–4), **1 Schritt = 1/108″** `[V:14, Bild 1]`; Haltestrom über SFH/SVH
   (im Modell ohne Wirkung).  Der Synchronisationslauf dauert mit dem Vorsteckmotor ~8 s Gerätezeit bis zum ersten DC1 [E] (Handbuch: 30 s Wartezeit zwischen Aus und Ein).
6. **DIL-Schalter** (S11–S26, 16 Stück, **im Auslieferungszustand alle Aus** `[M:11]`: „Aus entspricht Nichtbestückung“): Zeilen AM1–AM4 (Ausgabe, **high** = Zeile aktiv)
   × Spalten TSCH/TBAW/DIL/TPVV/TPVR; Belegung wie §5.  Bekannt: **S14** (Ein: Endlospapiertransport in jeder Druckposition; Aus: nur am Rand) `[M:11]`;
   **S13** Zeichengeneratorumschaltung (nur V.24/IFSP, nur bei bestücktem Block S11–S14) `[M:11]`, im ROM bei `2204H` (AM4 × TSCH) gelesen.  **V.24-Satz** liest
   (ROM `0093H–0140H`, `197CH`): **S24/S25/S26 = Bit 2/1/0 des Baudcodes** (Zeitkonstante 2^Code, Code 7 = 200 Bd), **S17 = XON/XOFF-Protokoll** (ohne S17 läuft das
   DTR-Protokoll und es geht **kein DC1** hinaus), S18–S23 weitere Format-/Protokollwahl `[?]` (gelesen, Bedeutung nicht belegt).  Im IFSS-Satz liest die Firmware
   die Matrix nur beim Einschalten für die Tasten (S14 nach `[M]`).  Die Adapterplatte kennt Bestückungsvarianten (`-2`: nur S14, `-1`: S15/16/23–26 …) `[S:09/10]`.
7. **Punktraster** `[M:3, 2.1]`, `[V:5]`: Zeichen 9 × 7 (Halbschrittmatrix), Zeichenabstand 1/10, 1/12,5, 1/15″, Grundteilung horizontal 50 Punkte/Zoll, vertikal
   72 Punkte/Zoll, Zeichenbreite 1,9 mm, -höhe 2,5 mm (7 Punkte), Nadeln 1…9 untereinander im Abstand 1/72″ **in zwei Spalten** (ungerade/gerade Nadeln,
   Verteilung Blatt 08): die geraden Nadeln sitzen **3 Motorschritte = 4 Drucktakte** vor den ungeraden (die Firmware löst sie entsprechend später aus, am
   Selbsttest gemessen [E]), Drucknadelfrequenz 500 Hz, höchstens jeder 2. Rasterpunkt.  **Zeichengenerator im ROM `[F:1303H]`:** 2400H + 8 × Code, 8 Spaltenbytes, Bit 7 − k
   = Nadel k+1, 0 = Nadel schlägt an (das Byte geht unverändert auf 8DH); Spalte 0 von `, ; @ _ g j p q y` trägt Steuerdaten (Unterlängen).  Die Zeilen 0…7
   ( = Nadel 1…8) sind das Zeichen; Nadel 9 ist Unterlänge/Unterstrich.  Breitschrift: jede Spalte 6 statt 3 Pixel Abstand und doppelt gesetzt [E].
8. **Nadelanschlag `[S:08]`, `[V:11–13]`**: Je Nadel ein Timer (B555), getriggert vom Pegel /DNx, zurückgesetzt von /DTFG = 1; der Stromfluss beginnt, sobald
   /DNx = 0 **und** /DTFG = 0, den Rest regelt die Endstufe.  Modell: Anschlag = Flanke „aktiv“ (DN = 0 bei DTFG = 0) je Nadel.  Zwischen zwei Anschlägen
   steht /DTFG wieder auf 1 (Firmware `1877H`).
9. **Widersprüche Schaltplan/Firmware:** (a) hardware.md Fassung 1 nannte D12 A.0/.2 „Spannungsüberwachung“ — die Firmware (`0524H`) und das Handbuch
   (Lichtschrankentest) sagen FSO/FSU; (b) H4 an AM4 war eine Fehllesung; (c) die Namen /MELO und /MAUF in Fassung 1 („Melder“) sind die zwei Kontakte des
   Drehschalters.  Nicht auflösbar: Herkunft von /MPV, Reihenschaltung Haube/Kopf auf MHA, Funktion S11/S12/S15/S16/S18–S23 `[?]`.

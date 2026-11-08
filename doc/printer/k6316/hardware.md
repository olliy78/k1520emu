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

## 10  Druckregeln aus dem ROM (Orakel) — Stand 2026-11-08

Zweck: Formeln, aus denen `tests/support/k6316_orakel.h` die erwarteten Punkte **ohne** Blick auf den Emulator berechnet.  Alles aus den
ROM-Abzügen `doc/EPROMS/K6316/{ifss,v24}` (je fünf Chips lückenlos ab 0000H) abgeleitet.  Marken: `[E]` = aus dem ROM erschlossen (statische Lesung
**und** Gegenprobe: die Zellenbau-Routinen wurden in einem Wegwerf-Z80-Interpreter ausgeführt und bitgleich mit den Formeln unten verglichen —
95 Codes × 6 Modi × beide Firmwaresätze = 1140 Fälle, 0 Abweichungen; die Füllroutine der Druck-ISR zusätzlich an 8 Texten/Modi), `[?]` = unsicher.
Die Skripte sind nicht eingecheckt (Scratchpad).  Adressen **IFSS**; V.24-Entsprechung in Klammern (gleicher Code, anders verschoben).

### 10.1  Datenweg Zeichen → Nadel `[E]`

1. **Zeichenzelle.**  `sub_1303` (V.24: `13ADH`) baut je Zeichencode eine **12-Byte-Zelle** im Ringpuffer `3A1FH…3BABH` (Zeiger `(385EH)`, Weiterschalten
   `sub_12CC` / V.24 `1376H`, Spiegelzelle `3BABH` wird nach `3A1FH` kopiert).  Modus `(3836H)`: Bits 0–2 = Schriftart (0 Normal, 1 Breit, 2 Schräg; +3 = mit
   Unterstrich), Bit 3 = Zweitzeichensatz (SO, Basis `2100H` statt `2400H`).  Glyphe `g[0..7]` = ROM `[2400H + 8·(Code & 7FH)]`.  Dispatch `1312H…1335H`:
   Modus 0/3 → `1418H`, 1/4 → `13EDH` (`sub_0865`), 2/5 → `133DH` (Scherung).
2. **Zelle → Takte.**  Die Füllroutine der Druck-ISR (`17C2H…186DH`, V.24 Entsprechung ab `187CH`) erzeugt aus einer Zelle `N[0..11]` zehn „Wörter“, eines je
   Drucktakt `k = 0…9`: Wort = (Byte 0: Nadeln 1…8, Byte 1 Bit 0: Nadel 9).
   ```
   Daten(k)  = N[2+k]                      (k = 0..9, N[10], N[11] sind FF)
   Nadel9(k) = (N[0] >> k) & 1             (k = 0..7)   -- Rotation `RRC (HL)` je Takt, 17D9H
             = N[1] & 1                    (k = 8)      -- 17F3H
             = (N[1] >> 1) & 1             (k = 9)      -- 1800H/17FFH (RRA)
   ```
   Alle Bytes sind **roh = invertiert**: Bit 0 = Nadel schlägt an.  Nadel j (1…8) = Bit (8−j) des Datenbytes; Nadel 9 = Bit 0 des zweiten Wortbytes `[1737H]`.
3. **Ausgabe.**  ISR `1715H…174DH` (V.24 gleich): Port `8DH` = `(Strom A | 55H) & (Strom B | AAH)` → Strom A liefert Nadeln 1,3,5,7 (Bits 7,5,3,1) **und Nadel 9**
   (Bit 0 des Folgebytes → Port `8CH` Bit 0, `1737H…1747H`), Strom B Nadeln 2,4,6,8.  Die Leseabstände im 7-Wörter-Ring `3874H…3880H` sind 4/5/6 Wörter (`sub_153B`,
   `1542H/1552H/155FH`, je nach Zeichenabstand `(3828H)` = 0/1/2): **gerade Nadeln drucken dieselbe Spalte 4/5/6 Takte später** — das ist die feste Nadelversetzung 0,04″
   (3 Motorschritte, §9 Nr. 7).  Für das Orakel entfällt sie: im Blatt (Kern: `nadel_x`) stehen alle Nadeln einer Spalte übereinander.
4. **Takt = 1/(10·Zeichen/Zoll)″.**  Jedes Zeichen belegt **10 Takte** je Zelle: 10 Z/Z → Takt 1/100″ (3 Pixel), 12,5 Z/Z → 1/125″ (2,4 Pixel), 15 Z/Z → 1/150″ (2 Pixel);
   Umschaltung `(3868H)` über `sub_1BB6/1B99/1BD8` (ESC-Folgen, im Selbsttest s. 10.7).  Die Formeln unten gelten in **Takten**; der Orakel-Test fährt 10 Z/Z (Takt = 3 Pixel).

### 10.2  Normalschrift, Spalte 0, Nadel 9, „Unterlänge“ `[E]` (Modus 0; `1418H…1432H`)

Zelle: `N[0]=g[0]`, `N[1]=FF`, `N[2..8]=g[1..7]`, `N[9..11]=FF`.  Folge:

* **Spalte 0 ist keine Druckspalte, sondern die Nadel-9-Bitmaske:** Bit k von `g[0]` (0 = Treffer) = Nadel 9 schlägt im Takt k an (k = 0…6 ↔ Datenspalte `g[k+1]`;
  Bit 7 ↔ Takt 7, dort ohne Spaltendaten).  Nur `, ; @ _ g j p q y` haben `g[0] ≠ FF` (V.24: ohne `@`).
* **Es gibt kein Verschieben der Matrix nach unten.**  Die Unterlänge steckt in den Glyphen selbst: Kleinbuchstaben nutzen Nadel 3…7, `g j p q y` zusätzlich 8 und über
  `g[0]` die Nadel 9 (`p`: Schaft Nadeln 3…9 in Spalte 2; `g`: Haken Nadel 8 + Nadel 9 in den Spalten 2/4/6; `_`: nur Nadel 9, `g[0]=AAH` → Takte 0,2,4,6 = punktierte Linie).  Großbuchstaben: Nadeln 1…7.
* **Spaltenlage:** Datenspalte `g[c]` (c = 1…7) im Takt `c − 1`; Takte 7…9 bleiben leer (Zeichenabstand 10).  (`k6316_orakel.h` führt `g[c]` bei Takt `c` und `g[0]` als Spalte 0:
  das ist um eine Spalte verschoben, bleibt aber wegen des gefitteten Ursprungs richtig, weil `g[0] = FF` für alle ausser den neun Sonderzeichen.)

```
treffer_normal(code):                          # Punkte (takt, nadel) relativ zum Zeichenanfang
  g = ROM[2400H + 8*code .. +8]
  for c in 1..7: for j in 1..8: if bit(g[c], 8-j) == 0: (c-1, j)
  for k in 0..7:               if bit(g[0], k)   == 0: (k, 9)
```

### 10.3  Breitschrift `[E]` (Modus 1; `sub_0865`, `0865H…08F2H`)

Auslösung: `ESC [ 1 m` (SGR 1; `1C7FH → 1D49H → 1D83H`), im Selbsttest (Zeile 2, 13 Zeichen, mit Unterstrich = Modus 4).  **Zwei Zellen je Zeichen = 20 Takte (1/5″)**, Spaltenabstand **2 Takte**:

* die acht Spalten stehen im Takt 0, 2, 4 … 14; Spalte m (m = 0…7) = **ROH-UND zweier Nachbarspalten** `g[m] & g[m+1]` mit `g[0] = g[8] = FF` (roh-UND = **Vereinigung der Treffer**:
  jede Quellspalte kommt in zwei aufeinanderfolgenden Breitspalten vor = „doppelt gesetzt“, Beleg `08BFH…08DCH`);
* Nadel 9: Bit j (j = 0…6) von `g[0]` = Treffer ⇒ Nadel 9 in den Takten `2j` **und** `2j+2` (`AND FAH/EBH/AFH/BFH` in `0873H…08B6H` je 2 Takte, Rest in `N[1]` bzw. der 2. Zelle);
* Takte 15…19: leer.

```
treffer_breit(code):
  gg = [FF] + g[1..7] + [FF]
  for m in 0..7: w = gg[m] & gg[m+1]; for j in 1..8: if bit(w, 8-j)==0: (2*m, j)
  for j in 0..6: if bit(g[0], j)==0: (2*j, 9) und (2*j+2, 9)
  Zeichenbreite = 20 Takte
```

### 10.4  Schrägschrift `[E]` (Modus 2; `133DH…13E9H`, Schleife `1390H…13CAH`)

Auslösung: `ESC [ 3 m` (`1D7FH`; Modus wird 2), im Selbsttest Zeile 3 (25 Zeichen).  **Scherung nach rechts: Nadel j wird um `9 − j` Takte verzögert** (Nadel 9 gar nicht, Nadel 8 um 1, Nadel 1 um 8
Takte; Steigung 1 Takt je Nadelabstand).  Die Schleife legt Spaltenbit b (Bit b = Nadel 8−b) der Datenspalte c in Zellbyte `c + 2 + b` (jenseits Byte 11 um 2 weiter — die zwei
Kopfbytes der Folgezelle werden übersprungen), also in den Takt `c + b`; die Spalte `g[0]` wird **ungeschert** nach `N[0]` kopiert (`1386H…1388H`).  Teile über Takt 9 hinaus
fallen in die **Folgezelle** (roh-UND, `FF`-Vorbelegung der Folgezelle `1436H`) und vereinigen sich dort mit dem nächsten Zeichen.  Zeichenbreite weiter 10 Takte.

```
treffer_schraeg(code):
  for c in 1..7: for b in 0..7: if bit(g[c], b)==0: (c + b, 8 - b)       # = (c-1) + (9-j), j = 8-b
  for k in 0..7:               if bit(g[0], k)==0: (k, 9)
  # Zeichen n: alle Takte + 10*n; Ergebnis aller Zeichen vereinigen (Überlauf bis Takt 14)
```

### 10.5  Unterstrich (Modus ≥ 3) `[E]`

`ESC [ 4 m` addiert 3 zum Modus (`1D5D`H; nur wenn Modus < 3), `ESC [ 0/1/3 m` löscht ihn.  `sub_1441` (`1441H…1459H`) setzt in jeder Zelle, deren `N[0]` **und** `N[1]` beide `FF` sind (kein eigenes
Nadel-9-Bild), `N[0]=AAH`, `N[1]=FEH` ⇒ **Nadel 9 in den Takten 0,2,4,6,8 der Zelle**.  Bei Breit (Modus 4) wird jede der zwei Zellen einzeln geprüft (`13F9H…13FFH`); bei Schräg (5) und Normal (3) die eine.
Zeichen mit eigenem Nadel-9-Bild (`g j p q y , ; @ _`) bekommen **keinen** zusätzlichen Strich.  Leerzeichen laufen im Modus ≥ 3 über `1DE7H` (Strich), sonst nur Vorschub (`1B21H…1B4EH`).

### 10.6  Zweitzeichensatz `[E]`
`SO` (0EH, `1E8BH`) / `SI` (0FH, `1E96H`) setzen/löschen Bit 3 von `(IX+18)` und damit des Modus: Glyphen aus `2100H + 8·Code`, sonst identische Regeln.  Die Tabellen der Sätze unterscheiden sich (`'A'` liegt im IFSS-Satz bei
Code 61H, im V.24-Satz bei 41H) — für das Orakel Normaltabelle `2400H` nehmen, sonst die Tabelle des jeweiligen ROM lesen.

### 10.7  Selbsttest-Zeile (Druckverlauf `00FBH…0196H`) `[E]`
Ablauf je Zeile (`C` = Zeilenzähler 20H…; Zeichen laufen zyklisch 21H…7EH, 20H, 21H …):
`26 Zeichen Normal` → Leerzeichen (Modus 0, 10 Takte) → `ESC[1m ESC[4m` (= Modus 4) → `13 Zeichen Breit+Unterstrich` (13·20 = 260 Takte) → `ESC[3m` (Modus 2, löscht Unterstrich) → Leerzeichen (Modus 2) → `25 Zeichen Schräg` (64 Zeichen je Zeile, dann LF+CR).
Segmentanfänge in Takten ab Zeilenanfang: Normal 0, Breit 270, Schräg 540.  Nach Zeile `k` (C = 20H+k): `C=35H` (k=21) → Zeichenabstand 12,5 Z/Z (`1BB6H`), `3FH` (k=31) → SO, `4AH` (k=42) → 15 Z/Z (`1B99H`), `5FH` (k=63) → SI, 10 Z/Z, Neubeginn (`0188H`).  Die Zeilennummern sind aus dem Zählerstand gelesen, nicht am laufenden Gerät gemessen `[?]`.

### 10.8  Hexdump (S2 beim Einschalten) `[E]` (`0303H…0368H`; V.24: `03A9H…03F2H`, Code gleich)
Gesetzt in `00C9H…00D3H` (Eingang `8CH` Bit 2 beim Einschalten, nach Handbuch S2 = TPVV; Zuordnung `[?]`).  Jedes empfangene Byte B wird als **drei Zeichen** gedruckt:
`'0'+hi` — **ohne Korrektur: hi ≥ A druckt `:` `;` `<` `=` `>` `?` (30H|hi)** —, dann `lo`: `'0'…'9'` bzw. `'A'…'F'` (`RLD`-Folge, Vergleich `90H` gegen `lo<<4`), dann Leerzeichen.  **32 Bytes je Zeile**
(`(IX+103)`: LF+CR vor dem ersten Byte und nach je 32 Bytes, Zähler 20H), Zeilenlänge 96 Zeichen.  Alles läuft über die normale Zeichenausgabe `1A14H` (Normalschrift, Pitch wie eingestellt).  Ob das Hi-Nibble-Verhalten
Absicht oder Abzug-Fehler ist, bleibt `[?]` (die Glyphen 3AH…3FH sind gewöhnliche Satzzeichen, keine Hexziffern).

### 10.9  Orakel-Pseudocode für `k6316_orakel.h`
```
BREITE  = {0,3: 10, 1,4: 20, 2,5: 10}[modus]        # Takte je Zeichen
x_px    = ursprung_x + (taktsumme_vorheriger_Zeichen + takt) * DRUCKTAKT     # 3 px bei 10 Z/Z
y_px    = ursprung_y + (nadel - 1) * NADEL                                   # nadel 1..9, 9 = y 24
punkte(text, modus) = Vereinigung ueber Zeichen n von treffer_<modus>(code) verschoben um taktsumme(n)
Unterstrich (modus >= 3): zusaetzlich (0,9),(2,9),(4,9),(6,9),(8,9) je Zelle ohne eigenes Nadel-9-Bild (10.5)
```
Hinweise: (a) Nadel 9 gehört jetzt dazu (`vergleiche` schneidet bisher `y ≥ 8·NADEL` ab); (b) die Sonderzeichenliste `hatSteuerSpalte0` besser aus `g[0] != FF` ableiten (V.24 kennt `@` nicht als Sonderzeichen);
(c) Schräg-Überläufe (Takte 10…14) gehören dem Folgezeichen/Zeilenende — am Zeilenende das Fenster um 4 Takte nach rechts erweitern; (d) Rückwärtszeilen: die Füllroutine liest die Zellen rückwärts (`1825H…186DH`); dass dasselbe Bild entsteht, ist hier nicht nachgerechnet `[?]` (der Emulator-Wächter in Nr. 21 deckt es ab).

### 10.10  V.24-Satz `[E]`
Gleiche Regeln: Zeichengenerator ebenfalls `2400H + 8·Code` (Zweittabelle `2100H`), Zellenbau `13ADH` statt `1303H`, Weiterschalten `1376H`, Füllroutine ab `187CH`, Hexdump `03A9H` — für alle Codes 20H…7EH und Modi 0…5 **zellengleich** zum IFSS-Satz.  Unterschiede nur in den
Glyphen der Codes **24H, 26H, 40H** (IFSS: andere Formen, `@` mit eigenem `g[0]=BBH`; V.24: `@` mit `g[0]=FF`) und in der Zweittabelle.

### 10.11  Offen `[?]`
Zeilennummern des Pitch-/SO-Wechsels im Selbsttest (aus Zählerstand, nicht gemessen); Zuordnung der Einschalt-Tasten zu `8CH`-Bits (Hexdump); ~~SGR-Parameter~~ am Emulator bestätigt 2026-11-08 (`ESC[1m`/`[3m`/`[4m`/`[0m` über die SIO, Wächter `K6316Modul.SchriftartenUeberEscFolgen…`); Selbsttestzeilen 1–3 (Segmente 0/270/540) ebenso;
Verhalten der Zweitzeichensatz-Glyphen im Orakel (nicht getestet); Anfang einer Schrägzeile (erste Zelle ohne `FF`-Vorbelegung nur beim allerersten Zeichen nach Zeilenwechsel, dort `sub_1FDCH` — nicht geprüft).

# K6316 — Hardwarebefund (Entwurf 49, WP0a)

Stand 2026-11-05, **Zwischenstand**: Logik vollständig gelesen und gegen das ROM geprüft, Leistungsseite und Bedienfeld nur soweit
für die Firmware nötig.  Marken: `[S:n]` Schaltplan-Scan `original/schaltplan_scans/n.jpg`, `[P:n]` Seite von `original/K6316_Plaene.pdf`,
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
| 3800H–3BFFH | RAM 1 KiB (2 × 2114), `SP` = 3C00H, `I` = 38H | `[F:0000]`; LDIR löscht bis 3C23H und überschreibt danach die Vektoren — **Spiegelung 3C00H → 3800H ist nötig oder harmlos** `[?]` |

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
| D12 A (80H) | 0 | Draht 53 → X821 F5O | ein | Spannungsüberwachung [?] |
| | 1 | /ITON | ein | Taste ON/OFF-LINE |
| | 2 | Draht 55 → X821 F5U | ein | Spannungsüberwachung [?] |
| | 3 | SFH | aus | Papiermotor Haltestrom |
| | 4–7 | /SF1–/SF4 | aus | Papiermotor Phasen (`OUT (80H)` aus Tabelle, `[F:0E81]`) |
| D12 B (81H) | 0 | /G1 | ein | **Taktscheibe Druckwagen** (Interrupt, Vektor 22H `[F:0554]`) |
| | 1,3,5,7 | /SW1, /SW2, /SW3, /SW4 | aus | Wagenmotor Phasen; Fortschalten durch Rotieren um 2 Bit `[F:0824/0831]` |
| | 2 | MHA | ein | Lichtschranke/Kopfhebel [?], `[F:083A]` |
| | 4 | /SWH | aus | Wagenmotor Haltestrom (`OR 10H`) |
| | 6 | PE | ein | Papierende (treibt zugleich LED /LPE direkt) |
| D14 A (8CH) | 0 | /DN9 | aus | Nadel 9 |
| | 1 | /DTFG | aus | Nadelfreigabe (Halbspalte) `[H]` |
| | 2 | TPVV | ein | Taste Papiervorschub vor (+ Matrixspalte) |
| | 3 | TPVR | ein | Taste Papiervorschub rück (+ Matrixspalte) |
| | 4 | Draht 73 → D13 → /LON | aus | LED ON-LINE |
| | 5 | Draht 74 → D13 → /LTEST | aus | LED Test |
| | 6 | Draht 75 → D13 → /LTF | aus | LED Fehler [?] |
| | 7 | /MELO | ein | Melder unten [?] |
| D14 B (8DH) | 0–7 | /DN8…/DN1 | aus | Nadeln 8…1 (Bit 0 = /DN8) |
| D11 A (A8H) | 0 | TSCH | ein | Taste Schneiden (+ Matrixspalte) |
| | 1 | TBAW | ein | Taste Beleg/Auswurf [?] (+ Matrixspalte) |
| | 2 | DIL | ein | Matrixspalte DIL-Schalter |
| | 3 | SVH | aus | Vorsteckmotor Haltestrom |
| | 4–7 | /SV1–/SV4 | aus | Vorsteckmotor Phasen `[F:0E74]` |
| D11 B (A9H) | 0 | /YABU | aus | Leitblech-/Abschlagmagnet unten [?] |
| | 1 | AM4 | aus | Matrixzeile 4 + LSPAUS-LED (über D13) |
| | 2 | /MAUF | ein | Melder Aufwickeln [?] `[F:0503]` |
| | 3 | /YABO | aus | Magnet oben [?] |
| | 4 | AM1 | aus | Matrixzeile 1 |
| | 5 | AM3 | aus | Matrixzeile 3 |
| | 6 | /MPV | ein | Melder Papier vorn [?] |
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
* **Offen `[?]`:** Takteingänge CLK/TRG der CTCs (im Plan nur kurze Stummel), Zeitkonstante von D10 Kanal 0.

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

Funktion der einzelnen Schalter steht im Manual (S13/S14 Interface, S18 Baudrate/Duplex) und ist in WP2a zu übertragen.  S14 hängt an
Diode V1 und an der Brücke XV zum Draht 55 — Sonderfall, in WP2a prüfen.

LEDs `[S:03, S:10]`: /LON, /LTEST, /LTF, /LPE, LSPAUS (= H1–H5 des Manuals; Zuordnung im Bedienfeldplan `[P:?]`) und Netzlampe.
Summer: /ITON ist Taste, ein Summer kommt im K6316 nicht vor `[?]`.

## 6  Leistungsseite (für die Emulation nur als Signal)

Wagenmotor 6°, 60 Vollschritte je Umdrehung, Taktscheibe 30 Impulse je Umdrehung (`/G1`) `[H]`; Nadelfrequenz 500 Hz, 9 Nadeln in versetzten
Spalten, Ansteuerung je Richtung versetzt `[H]`.  Papiermotor und Vorsteckmotor: je 4 Phasen + Halten.  Magnete /YABU, /YABO.
Schneiden: kein eigenes Signal im Stecker gefunden `[?]` (Kandidaten: /YABU, /YABO oder Vorsteckmotor) — in WP2a aus Firmware und Handbuch klären.

## 7  Kennung der Firmwaresätze

Der Selbsttest liest sie aus dem ROM: IFSS `540` (24F8H) / `19461306` (27F8H), V.24 `501` / `15263105`.  Damit lässt sich jedes Abbild prüfen.

## 8  Noch offen für WP2a

1. Bedienfeldplan lesen (`K6316_Plaene.pdf`, Zuordnung H1–H5, Tasten) und V.24-Modul-Blatt.
2. MHA, /MELO, /MAUF, /MPV, /YABU, /YABO: Bedeutung aus Lichtschrankenplan `[S:11]` und Handbuch.
3. CTC-Takteingänge und Zweck der Zeitgeber aus der Firmware.
4. Punktraster: Schrittweite Wagen je Spalte, Nadelversatz, Zeilenraster (aus Datenblatt Manual §2 und Firmware).

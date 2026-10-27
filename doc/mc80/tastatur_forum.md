# MC80.3x — Tastatur: Erkenntnisse aus dem Forum (Stand 2026-10-25)

Quelle: robotrontechnik.de, Thread „Tastatur am MC80.3x“ (`threadid=7918`, 40 Beiträge,
2012–2014; Hauptautor AE, am Gerät getestet mit dem Minimalsystem `mc80la`).  Ausgelöst
durch die Meldung eines Testers, dass Kursor/CL fehlen.

## Protokoll Tastatur → Rechner (9600 Bd, 7 Bit, ungerade Parität)

Je Tastendruck zwei Worte: **Präfix** (Statuswort), dann Code.

| Präfix | Bedeutung | Umsetzung im Kern |
|---|---|---|
| 1CH | Taste im großen Tastenfeld | `Mc80Tastatur::zeichen` |
| 1DH | Taste im Ziffernblock (ohne CTRL/Umschaltung) | `ziffernblock` |
| 1EH | Sondertaste, Code = Bitmaske (40H >> n, Taste 7 = 00H) | `sondertaste` |
| 1FH | CTRL war vorher gehalten; EGOS setzt Bit 7 des Codes | `funktionstaste` |

Codes der Nicht-Zeichentasten: **↑ 0BH, ↓ 0AH, ← 08H, → 0CH, CL 7FH, CR/LF 0DH, Tab 09H,
ENTER 11H, OFF 13H**; die unbeschriftete Ziffernblocktaste liefert **20H**, „00“ sendet 1DH + 30H
zweimal.  CR/LF: beim ersten Erkennen 1CH, dann 0DH.  Es gibt keine Rückschritt-Taste
(wir bilden den Wirts-Rückschritt als ← ab).

Offen [?]: welches Zeichen unter 1FH gesendet wird (Kappe oder Steuercode — wir senden die
Kappe in Großbuchstaben); der Code von `÷` im Ziffernblock.

## Rechner → Tastatur

| Wort | Wirkung | Antwort |
|---|---|---|
| 07H | kurzer Piep, Dauerton aus | — |
| 10H | Dauerton ein | — |
| 0EH / 0FH | CTRL-/Links-/Rechts-/Fixumschaltung sperren / freigeben | — |
| 04H | Matrix sperren, Bereit-LED aus | 15H (NAK) |
| 05H | Matrix freigeben, LED ein | 06H (ACK) |
| 13H / 14H | Autorepeat aus/ein — wegen Firmwarefehlers wirkungslos | — |
| sonstige | ohne Funktion | 18H (CAN) |

Nach dem Einschalten sendet die Tastatur 06H; die SIO ist dann noch nicht initialisiert, das
Zeichen geht verloren (nicht nachgebildet).  **Achtung:** EGOS sendet 12H/14H, wenn FB7FH Bit 6
wechselt; 12H ist nicht definiert und wird mit CAN beantwortet, das EGOS als Zeichen empfängt.

## Weitere Befunde

- Die EPROMs bei 0000H unterscheiden sich in einem Bit (CRC 14EBH / 4506H), das die Tastatur-
  Baudrate über die CTC-Zählkonstante (4 bzw. 5) bestimmt; 0800H/2000H sind in allen Varianten
  gleich, 1000H/1800H hängen am Kassettenlaufwerk, 2400H nur an der INIT-Ausschrift.
- TST-Treiber (00BCDH): Code vorhanden → A = Code, Cy = 1, Z = 0, sonst A = 0.  Funktionstasten
  sind darüber nicht unterscheidbar.
- Tastatur-Hardware: U881/U883, ROM U2716 (CRC ÄZ1 0F688H, ÄZ3 0BEC0H), ÄZ3 mit 9,832-MHz-
  Oszillator; 12 Zeilen × 8 Tasten.
- Funktionstasten-Sprungtabelle FED0H + 3n; nach INIT ist FECDH…FEE7H mit FFH belegt (alle liefern 0C3H).

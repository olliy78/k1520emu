# mutos_io — Semantik der U880-Seite, am Emulator belegt (2026-10-27)

Gegenstand: `tools/mutos8000/mutos_io/src/mutosio.mac` (bytegleiche Quelle, Zeilenkommentar `; ADR`), das UDOS-Programm bei 9000H, das als
E/A-Prozessor des Z8000-Kernels `mutos_1216` dient (A5120.16 mit EM256).  Bezug: Entwurf 39 §2, §11.1, §11.2; Entwurf 17 §7; Entwurf 40.

**Kennzeichnung wie Entwurf 39 §11:** *gemessen* = im Lauf beobachtet (Debugger, Aufruf steht dabei) · *abgeleitet* = aus Quelle/Kernel gelesen,
nicht im Lauf geprüft · *offen* = weder noch.  Alle Läufe: Ladediskette `a5120_mutos8000_boot_udos_k5601.hfe`, Wurzel `…_root_mfm.hfe`,
`--em em256 --laufwerke K5601,K5601,K5601` (COW, nichts am Repo verändert).  Kurzform des Aufrufs (`SKRIPT` = Datei unter `tools/mutos8000/versuche/`):

```
tools/dev.sh tool k1520dbg --em em256 --laufwerke K5601,K5601,K5601 \
    disks/a5120_16_MUTOS/a5120_mutos8000_boot_udos_k5601.hfe -x tools/mutos8000/versuche/SKRIPT
```

| Skript | Zweck |
|--------|-------|
| `mutos_io_abdeckung.dbg` | Haltepunkt (`b` + `bi … 2000000000`, hält nie) auf JEDE der 1343 Befehlsadressen der Quelle; `bl` am Ende liefert die Trefferzahl je Adresse |
| `mutos_io_nachricht.dbg` | Logpoints am Nachrichtenweg (94E1, 9558, 90FC, 9160, 9078, 90A3) |
| `mutos_io_ebenen.dbg` | Logpoints an den IN-A8H-Stellen und den Interrupt-Auslösungen (Ebenen, Zeitgeber, Tastatur) |
| `mutos_io_tty_drucker.dbg` | `cat /etc/motd > /dev/lp` und `> /dev/tty1` (Klasse 1 bzw. Klasse 2 Einheit 1) |
| `mutos_io_rohfloppy.dbg` | Klasse 3 von Hand angestoßen (Anforderungsbit + DCB-Felder gepokt) |
| `mutos_io_nukleus.dbg` | die UDOS-Nukleus-Einsprünge, die mutos_io ruft |

## 1. Kernbefunde

1. **Gemessen: im Betrieb (Boot bis Login, `ls`, `cat`) laufen nur vier Wege** — Platte (Klasse 0, Handler 90EB, Einheit = Minor 2), Konsolenausgabe
   (Klasse 2, 9208), Tastatur (Klasse 4, 90BB) und der 50-Hz-Zeitgeber (947E).  Drucker (91A8) und tty1 (9D57 → 9E84) laufen nur, wenn man sie anspricht
   (`/dev/lp`, `/dev/tty1`).  Nie gelaufen: Roh-Floppy (Klasse 3), SIO-Empfang, tty2/tty3, Paritäts-ISR, alle Fehlerrückrufe (§9).
2. **Die Vermutung stimmt im Gerüst** (Beschreiber je Klasse mit Anforderungs-/Fertigmaske/Handlertabelle; DCB; Handlertabelle 8 × [DCB, Handler]; ISR 94C1
   über PIO A32 = INT-16; Vektor 0AH für Platte), **weicht aber in diesen Punkten ab:**
   - **ACH (Status-8) wird bei Fertigmeldungen NICHT geschrieben.**  Die Fertigmeldung einer Platten-/Konsolen-/Druckeranforderung ist nur `OUT (ADH),Vektor`
     (90A3); der Status steht im **Nachrichtenfeld +3 im EM**.  ACH trägt ein Datum nur bei den *spontanen* Interrupts: Tastenzeichen (Vektor 22H, 90D0/90D4)
     und Zeitgeber (Vektor 02H, Zahl der aufgelaufenen Ticks, 94A6/94AA).
   - **Das Nachrichtenfeld +0 ist 2 × Klasse** (0, 2, 4, 6), nicht die Klassennummer; die Klassen sind: 0 Platte, 1 Drucker, 2 Konsole/tty, 3 Roh-Floppy, 4 Eingabe.
     (Kernel-Seite: `PUSH #0/#2/#4/#6` vor `CALL %5CAE` — Drucker 694A = 2, Konsole = 4, Platte = 0, Floppy = 6.)  Der Beschreiber liegt bei `A405 + 4 × Klasse`.
   - **Port 1CH ist am A5120 von KEINER Karte belegt**, und das Kommandoblock-Fenster **01B5H liegt im UDOS-Nukleus (Code)**.  Klasse 3 ist hier funktionslos
     und zerstört beim Anstoßen 8 Byte Nukleus (§8).
   - **Kanal B der SIO wird nie gesendet** (`OUT (52H)` kommt in der Quelle nicht vor); tty1/2/3 senden alle über Kanal A (50H/51H).  tty2/tty3 sind unter MUTOS
     ohnehin nicht ansprechbar (`/dev` kennt nur `console`, `tty1`; `mknod` fehlt).
   - **Bit 7 im Befehlsfeld +2 heißt „ohne Interrupt“** (Z8000 fragt das Statusbyte ab): die Konsole sendet beides, `01H` (mit Vektor 24H, 59 ×) und `81H` (ohne, 13 ×).
   - **Die Ebene IX+1 ist eine Schwelle gegen A33 Bit 0–2** (§4), nicht eine eigene Priorität im Beschreiber.
3. **Der Zeitgeber ist 50,000 Hz** (CTC 0DH: `A7H`, Zeitkonstante `C0H` = 192, Vorteiler 256 → 2 457 600/256/192).  Gemessen: Abstand der Vektor-02H-Meldungen
   ≈ 49 160 Maschinentakte (`mutos_io_ebenen.dbg`).  Die Tastatur wird **im Zeitgeber-ISR abgefragt** (kein eigener Interrupt): 947E → 970C.

## 2. Hauptschleife, Beschreiber, DCB (gemessen/abgeleitet)

```
9000: LD SP,CB68; CALL A000 (Bedienmaske, wartet auf 'r'); CALL 9400 (Hardware-Init); OUT (A9),2C; (A400)=0; CALL 960A
9014: IX=(A401)=Beschreiber der laufenden Klasse
        (IX+0)=Anforderungsmaske != 0 -> C=niedrigstes gesetztes Bit (961E) -> IX=DCB, HL=Handler aus Tabelle (9625) -> JP (HL)
9025: naechste Klasse (9600: (A400)+=2, bei 0AH zurueck auf 0; 960A: (A401)=A405+2*(A400))
        (IX+1)=Fertigmaske != 0 -> IX=DCB -> 903D (Fertigmeldung); sonst JR 9014
```

**Beschreiber** (4 Byte, `A405 + 4k`; abgeleitet aus dem Datenbereich, gemessen über die Treffer der Klassen): `+0` Anforderungsmaske (Bit n = Einheit n),
`+1` Fertigmaske (Bit n = Einheit n fertig), `+2/+3` Zeiger auf die Handlertabelle (8 × 4 Byte `[DCB-Zeiger, Handler]`, Index = Bit/Einheit).
Bits setzen/löschen: 963E = setzen (A = Einheit, HL = Byte), 964B = löschen.

| k | Nachricht +0 | Name | Beschreiber | Tabelle | Einheit 0 | weitere Einheiten |
|---|------|------|-------------|---------|-----------|-------------------|
| 0 | 00 | Platte | A405 | A419 | DCB0 A4B9 / 90EB | 1–7: `967B` (Leerhandler, nie gelaufen) |
| 1 | 02 | Drucker | A409 | A439 | DCB1 A4D0 / 91A8 | — |
| 2 | 04 | Konsole/tty | A40D | A479 | DCB3 A4FE / 9208 | 1: DCB4 A515 / 9D57 · 2: DCB5 A52C / 9D60 · 3: DCB6 A543 / 9D69 |
| 3 | 06 | Roh-Floppy | A411 | A459 | DCB2 A4E7 / 9A00 | — |
| 4 | (08, nur intern) | Eingabe | A415 | A499 | AUF0 / 90BB (Tastatur) | 1: 9D00 · 2: 9D09 · 3: 9D12 (SIO-Empfang) |

Beleg: Nachricht `+0 = 04` → 9614 (`SLA A`, `+ A405`) → Beschreiber A40D → Zeiger A479 → Handler 9208 (gemessen, `mutos_io_nachricht.dbg`: IX = A4FE bei `+0 = 4`).
Die Anordnung im Datenbereich (`A405…A418`) ist daher *Platte, Drucker, Konsole, Floppy, Eingabe*; die Namen DCB0…DCB6 sind nach der Reihenfolge im Datenbereich vergeben
(DCB2 = Floppy liegt vor DCB3 = Konsole), nicht nach der Klasse.

**DCB** (23 Byte, A4B9 + 23·i; Vorbesetzung im Datenbereich, Rest zur Laufzeit):

| Feld | Bedeutung | Beleg |
|------|-----------|-------|
| +0 | Vektor an den Z8000 (Platte 0AH, Drucker 12H, Floppy 1AH, Konsole 24H, tty1/2/3 28H/2CH/30H) | gemessen: 90A3 gibt 0AH/24H aus |
| +1 | **Ebene** (Schwelle, §4): Platte 5, Drucker 4, Floppy 5, Konsole/tty 4 | gemessen: Vergleich 9055/9057 |
| +2/+3 | Z80-Puffer des Geräts (Platte A8B6 = 4 KB, Drucker A7B6, Floppy B8B6, Konsole A5B6, tty1 A636, tty2 A6B6, tty3 A736 — je 128 B außer Platte) | abgeleitet; Platte: Obergrenze 1000H in 9816/981B |
| +4/+5 | EM-Adresse der Nachricht (Little-Endian, nach Vertauschen: D26A Konsole, D532 Platte) | gemessen: 94E1 zeigt HL = D26A/D532 |
| +6…+19 | **Kopie der 14-Byte-Nachricht** (+0 → IX+6 … +13 → IX+19) | gemessen: 9536 LDIR, 9558 |
| +20/+21 | ursprünglich angeforderte Zählung (nur gespeichert, nie gelesen) | abgeleitet |
| +22 Bit 0 | „Auftrag läuft“: gesetzt 9538, gelöscht 90B3; ist es beim Eintreffen schon gesetzt, gilt nur Konsole/Einheit 0/Bit 7 gesetzt (951C…9534), sonst `RST 38H` | abgeleitet; Zweig **nie gelaufen** |

`IX+7` (= Nachricht +1) ist zugleich Einheitsnummer = Bitnummer in den Masken.  `IX+8` (= Nachricht +2) Befehl: Bit 7 = **ohne Interrupt**; `&7F`: 0 = lesen
(Gerät → EM), 1 = schreiben (EM → Gerät) — für die Platte wird daraus 0BH/0FH des Nukleus (9121–912B), für die Roh-Floppy bleibt der Wert als Controller-Befehl (§8).
`IX+9` (= Nachricht +3) **Status**: 0 = läuft, 1 = fertig/gut, ≥ 40H (Bit 6) = Fehlercode des Nukleus (C1…C6, 44H/45H) bzw. 0FFH bei tty-Eingang (9DC7/9DC9).

## 3. Nachrichtenformat (14 Byte im EM, gemessen an zwei Quellen)

Gegenprobe Kernel-Seite (`z8kasm -d` an `mutos_1216`, Konsole 63D2–63EC, Platte 6B76–6BB4, Floppy 81D4–81E8) und Messung (`mutos_io_nachricht.dbg`, Punkt 9558):

| Byte | Bedeutung | Kernel schreibt | Beispiel Platte (gemessen) | Beispiel Konsole |
|------|-----------|-----------------|----------------------------|------------------|
| 0 | 2 × Klasse | 00 / 02 / 04 / 06 | 00 | 04 |
| 1 | Einheit (Minor) | Platte: Minor; Konsole: 0 | 02 (rk2) | 00 |
| 2 | Befehl | Platte: 0 = lesen, 1 = schreiben; Konsole 01H oder 81H | 00 / 01 | 01 / 81 |
| 3 | **Status** (vom U880 zurück) | 0 vorbelegt, Kernel wartet bis ≠ 0 | 01 | 01 |
| 4/5 | Segment des Puffers (Wort; nur Bits 0–1 zählen: Port-B-Segmentwahl) | 0 | 0000 | 0000 |
| 6/7 | Offset des Puffers im Segment (Big-Endian) | Puffer | A628 | D2A2 |
| 8/9 | Zählung in Byte (Big-Endian); **zurück: übertragene Byte** | 0200 | 0200 | 0001 |
| 10…13 | Platte: 32-bit-Blocknummer (nur 12/13 benutzt: 9140/913A → `IY+12/+11`) | Blocknr. | 00000001 | — |

Rückgabe schreibt der U880 an +3 (Status, 9092) und +8/+9 (Zählung, 9097/9099); Quellen: 9078–9099.  *Auffälligkeit (abgeleitet):* 9173 übernimmt die obere Zählbyte nur,
wenn die übertragene Zahl nicht kleiner ist (`CP (IX+14); JR C`) — bei einer Kurzübertragung bliebe also das obere Byte der Anforderung stehen.  Nie ausgelöst (kein Fehlerlauf).

**Wie die Nachricht zum U880 kommt (gemessen):** der Kernel legt den Zeiger bei **EM-Offset 4006H** ab (94C1…94D5: Attribut-Seite 4 auf `0CH`, Bus anfordern `OUT (A9),48H`,
`HL=(4006)`, danach `(4006)=0`), setzt A33 Bit 4 (INT-16) → PIO A32 Port A Bit 4 → ISR 94C1.  Die ISR holt die 14 Byte nach `A5A8` (zweite Kopie, 94E4…94EE), wählt
mit Byte 0 den Beschreiber (9614), mit Byte 1 den DCB (9625), kopiert in DCB+6, setzt IX+22 Bit 0, merkt `IX+4/5`, und bei Befehl `&7F = 1` (Schreiben) holt sie die Daten
sofort EM → Z80-Puffer (954C–9552); zuletzt Anforderungsbit setzen (956A) und `RETI`.  Das EM-Fenster: 9690 bildet eine 16-Bit-EM-Adresse auf das Z80-Fenster 4000H–7FFFH ab
(zwei benachbarte 4-KB-Seiten über `OUT (C),A` an AFH; A14/A15 der EM-Adresse kommen aus Bit 2/3 des Attributs).
*Init* (9400…9410): alle 16 Seiten auf 0FH (= gesperrt, Attribute sind negiert), PIO A32: Vektor EAH (Port A) / ECH (Port B).

## 4. Interrupt-Zustellung an den Z8000 und die Ebenen (gemessen)

`IN A,(A8H)`: Bit 0–2 = A33 Bit 0–2 (**aktuelle Ebene des Z8000**, 0 = alles erlaubt), Bit 3 = /VI (1 = kein Vektor steht an), Bit 6 = 8/16 (**wird 1, sobald der U880 den Bus
hat**: 96D8 wartet nach `OUT (A9),48H+n` auf Bit 6), Bit 7 = TREN.  Gemessen (`mutos_io_ebenen.dbg`): Normalwerte 88H (= Ebene 0, kein VI), 80H (VI steht noch an), 8DH/8FH/87H (Ebene 5/7)…

Eine Fertigmeldung (903D…90B7) wird **nur zugestellt, wenn** (a) Bit 7 des Befehls gesetzt ist (dann *keine* Prüfung, kein Vektor), oder (b) `/VI = 1` **und** `Ebene_Z8000 < IX+1`.
Sonst bleibt der Auftrag stehen (`JR NC,90B7`) und die Schleife versucht es beim nächsten Umlauf erneut.  Fest einkodierte Schwellen bei den spontanen Quellen:
**Zeitgeber 6** (94A1), **Tastatur 4** (90C4), **SIO-Empfang 4** (9D22).

Zeitgeber (947E, gemessen): Zähler `C966` zählt Ticks; ist die Ebene < 6 und /VI = 1, geht `OUT (ACH),Zähler; OUT (ADH),02H` hinaus und der Zähler wird 0.  Messwerte des Zählers
bei der Zustellung: 1 (5937 ×), 2 (56 ×), 3/5/6 (selten) — der Z8000 war also gelegentlich ≥ Ebene 6 und holte mehrere Ticks nach.
Tastatur (90BB): Zeichen aus dem Ring `C8B6` → `OUT (ACH),Zeichen; OUT (ADH),22H`.  **Paritätsfehler** (PIO-B-Bit 7 → Vektor ECH → 94B4): `OUT (A9),6CH` dann `2CH` (PR-Impuls); **nie gelaufen**.

Port-B-Werte an A9H (abgeleitet aus Entwurf 17 §7.2): `2CH` = Grundzustand (/RAMEN, /STOP, /TRQ8 inaktiv), `6CH` = + PR-Impuls (Paritäts-FF löschen), `48H + n` = /TRQ8 aktiv (Bus
anfordern), n = Segment 0…3.

## 5. Plattenanforderung (gemessen, `mutos_io_nachricht.dbg`)

```
Z8000: Nachricht D532 {00, 02(Minor), 00(lesen), 00, 0000, A628, 0200, 00000001}; Zeiger -> EM:4006; A33.4
94C1 : Nachricht -> A5A8 -> DCB0(A4B9)+6; Anforderungsbit 2 in A405+0
90EB : (C967)=0 sonst Wiedervorlage 90F2; (C967)=1; IY=AUF0(A55A): IY+0 = A12A[Minor] (Laufwerk), IY+1 = 0BH/0FH,
       IY+2/3 = Puffer, IY+11/12 = Blocknr. (lo/hi), IY+6..9 = Rueckruf 9163 (fertig) = 9163 (Fehler); CALL 9800; JP 966D (Anforderungsbit loeschen)
9800 : Block -> (Zylinder, Sektor), Auftrag fuer den Nukleus (CALL 0BFDH -> JP 0700H), je 128 Byte Rueckruf 9893; (C967) schuetzt die Platte
9163 : Fertig: IX=DCB0; IX+14/15 = uebertragene Zahl; IX+9 = 01 (oder Fehlercode); Fertigbit setzen; (C967)=0
903D : Fertigmeldung (s. §4), Lesen: Z80-Puffer -> EM (9063–9072), Status/Zaehlung in die Nachricht (9078–9099), OUT (ADH),0AH
```

Messwerte: 127 Lesen + 8 Schreiben im Boot bis Login (alle Minor 2 → Laufwerk 2), Zählung stets 0200 (512 B); jede Anforderung endet mit Status 01 und Vektor 0AH (135 ×).
`A12A[4]` = Minor → Laufwerk, hier `0,1,2,3` (Bedienmaske, A1F1).

## 6. Konsole, Tastatur, Bedienmaske

- **KONSOLE_AUS 9208 (gemessen):** je Zeichen `CALL 0BEBH`, das ist laut `u 0xBE8` `JP 0513H` (UDOS-Zeichenausgabe, `mutos_io_nukleus.dbg`); `0BE8` = Tastenabfrage (`JP 05CBH`),
  `0BF1` = Textausgabe (`JP 044EH`), `0BFD` = Platte (`JP 0700H`).  Nach der Zählung `IX+9 = 01`, dann 9658 (Anforderungsbit löschen, Fertigbit setzen).
  Gemessen 158 ×; 13 ×  mit Befehl `81H` (kein Vektor), 59 × mit `01H` (Vektor 24H).
- **Tastatur (gemessen):** Zeitgeber-ISR 947E ruft 970C: `IN (5DH)` Bit 0 = Zeichen da, `IN (5CH)` holt es (FFH → CR), Tabelle 9755 für Codes < 20H, Präfix 9xH/BxH (9730…9744,
  **nie gelaufen**), Byte 80H… verworfen.  Ablage in den Ring `C8B6` (Aufbau: `+0` Schreib-, `+1` Leseindex, Nutzdaten ab +4 bis 2BH, Umlauf auf 4; Init 9701 = `04 04 00 00`), dann Bit 0 der
  Klasse-4-Anforderung (947E…9492).  Der Handler 90BB gibt die Zeichen einzeln aus (§4).
- **Hardware-Init Tastatur (abgeleitet aus Quelle, Zeitkonstanten):** CTC 58H Kanal 0 (`05`,`01` = Zeitgeber, Konstante 1 → 9600 Baud × 16) · SIO 5CH/5DH polled (WR1 = 0, 8 Bit, kein Paritätsbit)
  · danach `OUT (5CH),00H` und `07H` (944F…9467; auch am Ende der Maske A2A8 in umgekehrter Reihenfolge) — **was diese Bytes der Tastatur sagen, ist offen** (vermutlich Signalton/Reset).
- **Bedienmaske A000–A2AF (gemessen):** `keys` der Ziffern 0–3 je Spalte, Eingabe `r` = weiter.  `7` → „error : number (0…3)“ (A19C), doppelte Zuordnung → „rk - duplikate“ (A1B8).
  Ungelaufen: Eingabe `x` (A17D, „not assign rk“; die Taste kam im Lauf nicht an — erneut versuchen), Übernahme einer *geänderten* Zuordnung (A1D4–A1EE), CR-Vorgabe (A171–A174).

## 7. Drucker, tty1, SIO (gemessen mit `mutos_io_tty_drucker.dbg`)

- `cat /etc/motd > /dev/lp`: 91A8 (Klasse 1, DCB1 A4D0, Befehl 01) → `CALL 0EB00H` (UDOS-Druckertreiber; `u 0xEB00` zeigt `JR EB14`) mit IY = AUF1 (A567); Zählung 35H (53 B), dann 2 B; Rückruf 91E1.
- `cat /etc/motd > /dev/tty1`: 9D57 (Klasse 2/4-Einheit 1, DCB4 A515) → 9D70 → 9E84 → 9EA0.  **Gemessen: alle 44 Zeichen über `OUT (50H),A`** (9EBE), jedes weitere nach dem
  Tx-leer-Interrupt 9EFB (44 ×), Ende 9F09 (`OUT (51H),28H` = WR0 „Tx-Interrupt zurücksetzen“), Rückruf 9DA2 (`RETI` mit auf den Stapel gelegtem Rückrufziel).
- **Kanal A ist der einzige Sendeweg** (abgeleitet: 9EA0 liest nur `IN (51H)`, schreibt nur `OUT (50H)`; 9D57/9D60/9D69 laufen alle in 9D70 → 9E84).  Empfang: Kanal A → Ring `C8E2` (9E3E, Einheit 1,
  Vektor 26H), Kanal B → Ring `C90E` (9E61, Einheit 2, Vektor 2AH), Ring `C93A` (Einheit 3, Vektor 2EH) hat keine Quelle in der Quelle.  **Nie gelaufen (kein Zeichen von außen).**
- SIO-Init (9DE3, abgeleitet): beide Kanäle `18H` (Reset), WR4 `45H`, WR5 `28H`, WR3 `41H`, WR1 `1EH` = **7 Bit, ungerade Parität, 1 Stopp, ×16**, Sender/Empfänger an, Empfang wird mit `AND 7FH` maskiert;
  CTC 5AH Kanal 2 Konstante 1 = 9600 Baud; IM-2-Tabelle 0F30…0F3E (B: 9F18 / 9E61×3, A: 9EFB / 9E3E×3).

## 8. Roh-Floppy (Klasse 3, 9A00 / 9B00): gemessen angestoßen, im Betrieb nie benutzt

- **Auslöser (abgeleitet):** der Kernel hat einen Floppy-Treiber (Strategie 7F94, Start 7FE4, Sender 81D4–81F2 mit `PUSH #6`; Nachrichtenfelder je Einheit in `D558 + 14·n`),
  aber **die Wurzeldiskette hat keinen Geräteknoten dafür** (`ls -l /dev`: nur console, kmem, lp, mem, null, rk0–3, rlp, rrk0–3, swap, tty, tty1).  Im Lauf nie erreicht.
- **Ablauf (gemessen, `mutos_io_rohfloppy.dbg`; Anforderungsbit `A411 = 01`, DCB2 mit Einheit 0, Befehl 0AH gepokt):** 9A00 (Wächter `C967`: 0 → 2) → 9B46 → Tabelle `9B16` (8 × 4 Byte
  `[Befehl, Maske lo, Maske hi, Controllerbefehl]`; Befehl wird mit `RES 0` ohne Asynchron-Bit gesucht) → Kommandoblock **01B5H** (8 Byte gelöscht, `(01B5)` = Controllerbefehl, `01B6/7` = Puffer,
  `01B8/9` = Zählung) → **`OUT (1CH),0`** → Warten (128 Durchläufe) auf **01BDH Bit 1** → Zeitablauf → Status **C2H** (9C00).  Beim zweiten Aufruf (Wächter = 2) wertet 9B02/9BA6 das
  Statusbyte `01BDH` mit der Fehlertabelle `9B36` (Bits 80/40/10/08/20/04/08/20 → Fehler F1…F8) aus und meldet 01.
- **Befund:** am A5120 antwortet nichts auf Port 1CH; der Block 01B5…01BC liegt im UDOS-Nukleus und wird dabei überschrieben (gemessen: Inhalt danach `07 B6 B8 FF FF 00 00 00 00`, vorher `E7 03 7E CD F7 03 CD 36 04`).
  Die Statusbits der Fehlertabelle gleichen einem WD179x (Nicht bereit / Schreibschutz / Satz nicht gefunden / CRC / Datenverlust) — **abgeleitet, offen** welche Hardware gemeint ist.
  Für den Emulator: Klasse 3 braucht keine Nachbildung, solange der Kernel sie nicht auslöst.

## 9. Abdeckung: was im Lauf NIE ausgeführt wurde

Lauf `mutos_io_abdeckung.dbg` (Boot, Bedienmaske mit Fehleingaben, Login, `ls`, `cat`, `/dev/lp`, `/dev/tty1`): von 1343 Befehlsadressen **347 ungelaufen**.  Nach Routinen:

| Adressen | Routine | Grund / Auslöser |
|----------|---------|------------------|
| 90F2–90F3 | Platte: Wiedervorlage, wenn `C967 ≠ 0` | zwei Plattenaufträge gleichzeitig — kommt nicht vor |
| 91F0–91F2 | Drucker-Fertig: Fehlerstatus (`FFH`) | Druckerfehler |
| 94B4–94BF | Paritätsfehler-ISR | kein Paritätsfehler |
| 951C–9534 | zweite Nachricht, solange `IX+22` Bit 0 gesetzt | nie überlappende Aufträge |
| 967B–968D | Leerhandler (Einheiten ohne Gerät) | nie angefordert |
| 9730–9744 | Tastatur-Präfix 9xH/BxH | Sondertasten |
| 981B–981C | Plattenauftrag > 1000H (`RST 38H`) | Prüfung greift nie |
| 9904–991A | Platten-Fehlerrückruf | kein Fehler |
| 9A00–9C29 | Roh-Floppy (Klasse 3) | s. §8 (nur von Hand gelaufen) |
| 9D00–9D3C | SIO-Eingabe-Handler 9D00/9D09/9D12 | kein Empfang |
| 9D60–9D6D, 9DAF–9DCD | tty2/tty3-Aufträge, Fehlerzweig `FFH` der tty-Fertigmeldung | nicht ansprechbar |
| 9E3E–9E82 | SIO-Empfangs-ISR (A: 9E3E, B: 9E61) | kein Zeichen von außen |
| 9ECD–9ED5 | Senden: Tx-Puffer nicht leer | nie voll |
| 9F18–9F24 | Tx-leer-ISR Kanal B | B sendet nie |
| A171–A183 | Maske: CR-Vorgabe, `x` | s. §6 |
| A1D4–A1EE | Maske: Übernahme geänderter Zuordnung | nur Vorgabe `r` |

Die Zahlen der Hauptwege im selben Lauf: 94C1 347 · 90EB 186 · 9208 158 · 90BB 359 · 947E 13 248 · 9893 744 · 91A8 2 · 9D57 1 · 9EBE 44.
**Die Abdeckung umfasst keine Variante `k5601_8x512`/`nur_a`** und keinen Lauf mit zweitem Terminal (Entwurf 39 A5a) — dort könnten 9D00 ff. laufen.

## 10. Ports (Übersicht)

| Port | Richtung | Bedeutung | Stand |
|------|----------|-----------|-------|
| 0DH | OUT | CTC (ZRE) Kanal 1: `A7H`, `C0H` → 50 Hz, Vektor 0FE2 → 947E | gemessen |
| 50H–53H | I/O | K8025-SIO: A Daten/Steuer, B Daten/Steuer; Senden nur 50H/51H | gemessen/abgeleitet |
| 58H, 5AH | OUT | CTC Kanal 0 (Tastatur-Takt), Kanal 2 (SIO-Takt), je Konstante 1 | abgeleitet |
| 5CH/5DH | I/O | Tastatur-SIO (abgefragt, keine Interrupts) | gemessen (Zeichen kommen darüber) |
| A8H | IN | Bit 0–2 Ebene, 3 /VI, 6 8/16 (Bus erhalten), 7 TREN | gemessen |
| A9H | OUT | PIO-A32-Port B: `2CH` Grund, `6CH` PR, `48H+n` Bus anfordern | abgeleitet (Entwurf 17), 2CH/48H+n gemessen |
| AAH/ABH | OUT | PIO A32 Steuerwörter (Vektor EAH/ECH, Mode 3, Maske `EF`/`7F`: INT-16 bzw. /PE) | abgeleitet aus 9474/9479 |
| ACH | OUT | Status-8: nur Zeichen (Tastatur), Tickzähler (Zeitgeber), SIO-Zeichen | gemessen |
| ADH | OUT | Vektor → VI an den Z8000 | gemessen |
| AFH | OUT (C),A | Attributspeicher, B = Seite (Hochnibble) | gemessen |
| 1CH | OUT | Roh-Floppy-Start, am A5120 unbelegt | gemessen (§8) |

## 11. Fehlende oder umständliche Fähigkeiten des Debuggers (Belege)

1. **`trace <datei> lo hi` filtert nur Z80-Zeilen; die U8001-Zeilen (`T3`) werden immer geschrieben** (Doku `tools/k1520dbg.md` §12 nennt es).  Mit `--em` füllt der U8001 den 2-Mio.-Deckel
   in Sekunden (gemessen: alle drei Dateien `cov_*.txt` hatten 2 000 000 Zeilen, davon fast alle `T3`; die gesuchten Z80-Fenster waren leer).  Gewünscht: `trace … z80`/`trace … u8000 off`
   oder ein Deckel je CPU.
2. **Keine Abdeckungsfunktion.**  Behelf: `b ADR` + `bi ADR 2000000000` auf jede Adresse (1343 × 2 Zeilen, aus der Quelle erzeugt) und `bl`.  Es geht (ca. 20 % Mehrzeit), aber `bl` druckt auch alle
   Treffer-0-Zeilen und kann nicht „nur ungetroffene“ oder in eine Datei.  Gewünscht: `cov on [lo hi]` / `cov report [datei] [unhit]` (Befehlsadressen automatisch, nicht nur Haltepunkte).
3. **`lp` kann keinen Speicherblock ausgeben**: eine 14-Byte-Nachricht braucht 14 Ausdrücke `[IX+6] … [IX+19]` in der Zeile (Zeilen ≈ 600 Zeichen).  Gewünscht: `lp ADR dump AUSDRUCK N`.
4. **`lp` hat keinen Zähler/keine Begrenzung** (13 000 Zeitgeber-Treffer überfluten die Ausgabe; ich habe mit `grep`/`awk` nachgefiltert).  Gewünscht: `lp ADR … count N` und `lp ADR … if BEDINGUNG`.
5. **Tastenfolgen laufen schneller als die Bedienmaske von mutos_io sie annimmt**: einzelne Tasten ohne `g 5000000` dazwischen gehen verloren (`x` kam nicht an).  Ein `keys … wait`-Schalter,
   der erst weitertippt, wenn das nächste Zeichen abgeholt ist, würde das beheben.
6. **`-l mutosio.mac` geht** (Namen aus der Quelle erscheinen in `u`), meldet aber je eine „?“-Zeile für `MACRO`/`ENDM`/`REPT`/`.PHASE`.  Nicht ausgewertet, ob die Adressen bei den vier
   `.PHASE`-Segmenten stimmen (der Versuch lief ohne geladenes mutos_io).
7. **Anstoßen von Hand ist umständlich**: Klasse 3 ließ sich nur durch Poken von Anforderungsbit und DCB-Feldern auslösen (`e 0xA411 01`, `e 0xA4EE …`).  Ein Befehl `em msg KLASSE EINHEIT BEFEHL …`,
   der eine Nachricht wie der Z8000 in den EM legt und INT-16 setzt, würde jeden Handler ohne Kernel-Treiber prüfbar machen.

## 12. Offen

- Was meldet `OUT (5CH),00H/07H` der Tastatur (Reihenfolge 00,07 beim Init, 07,00 nach der Maske)?  Vermutung Signalton; Beleg: `K7637`-Modell lesen.
- Welcher Controller steckt hinter Port 1CH / Block 01B5H (Klasse 3)?  Der Kernel-Treiber `fd` (7F94 ff.) ist vorhanden; ein Geräteknoten fehlt auf der Wurzeldiskette.
- Wann wird `IX+9` bei der tty-Eingabe `0FFH` (9DC7/9DCB)?  Nur lesbar aus der Quelle; ohne Empfang nicht lauffähig — mit einer Gegenstelle am Serienport (Entwurf 19) gezielt nachholen.
- Die Anforderungen der Nachricht Byte 10/11 (obere Blocknummer) werden nie benutzt: Plattengröße > 65 535 Blöcke nicht vorgesehen — abgeleitet.

# SIOS-INIT: warum Spur 00 als defekt gilt (ERR 9B/9C) -- Befund

Stand 2026-10-30, Zweig SIOS1526.  Reine Analyse, kein Kern-Code geaendert.  Werkzeuge: `k1520dbg`
(`b2`, `s2`, `e`, `wb`, `loadstate`), `tools/z80_disasm2.py`.  Belege: **[Lauf]** am Emulator, **[Code]** gelesen.

## 0. Aufbau des Laufs (reproduzierbar)

A5120, K5600.20 x2, K7637, Sicherheitsstecker 1, SYLs in A: booten (40 Mio. Takte), dann zur Laufzeit
`mount A sios1526_dp1.hfe k5601_ss40_26x128` (Programmlaufwerk 04 = A:, kein ASN noetig) und
`mount B sios1526_dp2.hfe k5601_ss40_26x128` (Ziel = Laufwerk 05).  Tasten: `CAL\xFF`, `INIT\xFF`, `N\xFF`
(5,25"), dann 5 x `\xFF` (Menue 1, Satzlaenge, Folge, 2 x Defektspur), am Punkt 5 `N\xFF` (nur Spur 00).
Alles COW (Vorgabe).  Das Ziel wird ohnehin komplett ueberschrieben; eine 40-Spur-Leerdiskette war nicht noetig.

## 1. Ursache 1 (ERR 9B/9C): ZVE2-Kontrolllesen sieht ein A1 zu viel -- Lesestrom 4xA1

**Kein Marken-/Index-Problem.**  INIT schreibt bei dieser Belegung gar keine Indexmarke: ZVE1 setzt in `[9C93]` (Steuerbyte)
Bit 6 = 0 (Wert A4/A6), ZVE2 ueberspringt dann `A3BC..A3E3` (Schreiben von C2 C2 C2 FC).  Der Haltepunkt `b2 A30E`
(FM-Indexmarke FC) ist deshalb nie erreichbar; das war der falsche Zweig (Hypothese aus bedienung.md ist **widerlegt**).

Was geschieht [Lauf]:
- ZVE2 schreibt Spur 00 (`A3AC..A4C4`), dann Kontrolllesen.  Weil Bit 6 = 0, geht es ueber `A4C4 -> A4AB -> A517`.
- ZVE2 liest je Sektor die Kennung: `A517 IN A / A51B IN A / A51D IN D,(C)` (Summe der letzten zwei = 42H = A1+A1, passt),
  dann `A520 INIR` (B=7) nach `A24BH`.  Erwartet wird, dass **INIR mit FE beginnt**: FE 00 00 01 00 EA 2D.
- Erwartung der ZVE1 steht in `[9C7EH..9C84H]` = `FE 00 00 01 00 EA 2D`.  ZVE1 vergleicht danach `A24B..A251` mit `9C7E..`
  (`9F5B..9F6C`, bei Fehler `JP NZ,9EF8` = Wiederholung, nach 5 Fehlschlaegen Abbruch -> ERR 9B).
- Gelesen wurde `[A24BH] = A1 FE 00 00 01 00 EA`: **um ein Byte verschoben** (viertes A1 vorn, CRC-Low fehlt).  Der Lesestrom des
  BusRq-Weges liefert 4xA1 + FE (`TrackCodec::buildFaithfulReadTrack`, `kReadA1 = 4`), der Resync-Kopf steht auf der ersten A1
  (`romReadResyncTarget`, markPos-4).  A517/A51B/A51D verbrauchen drei A1; das vierte steht dann als erstes INIR-Byte im Puffer.
- Nach dem Vergleichsfehler beendet ZVE1 das ZVE2-Lauf (ZVE2 wird bei Takt ~219 000 mitten in `A54D` zurueckgesetzt, `PC=0000`).

**Warum die 4xA1 sonst richtig sind:** INIT liest die Kennung an zwei Stellen mit unterschiedlicher Vorlaufzahl.
Pfad `A4FF` (alle Folgesektoren) hat vor A517 noch `A515 IN A,(16H)` -> vier Bytes vor FE (x A1 A1 A1 | oder A1 A1 A1 A1) --
das passt zu 4xA1.  Pfad `A4AB` (erste Kennung nach dem Schreiben, also der hier genommene) hat an der Stelle des Lesens
stattdessen `A4C0 OUT (14H),A` (nur ein Schreibzugriff auf Datenport A) -- drei Bytes vor FE, das passt zu 3xA1.
Der Datenteil (`A565..A5CC`: 5 INs, FB als fuenftes) passt ebenfalls zu 4xA1.  Im Emulator verbraucht ein `OUT (14H)` im
Lesemodus **kein** Stromsymbol (`K5122::handleDataPortAWrite`: "Lese-Echo -- ignorieren"; `ioRead(0x16)` zaehlt strikt je IN).
Auf der Hardware beansprucht dieser OUT vermutlich einen Byte-Slot (Byte-/BUSRQ-Handshake gilt fuer IN 16H und OUT 14H).
Das ist **Hypothese** -- der Handbuchtext zum Marken-FF sagt nur "das erste IN danach liefert das Byte der Marke".

**Gegenprobe [Lauf]** (kein Kern-Eingriff): `e 0xA4C0 DB 16` (aus `OUT (14H),A` wird `IN A,(16H)`, gleiche Laenge) nach `loadstate`
und vor `keys N`.  Ergebnis: `A24B = FE 00 00 1A 00 35 A4` am Ende, ZVE2 erreicht `A5E7` (Erfolgsende nach **allen 26 Sektoren**
inkl. Datenvergleich, ZVE2-Takt 361 574) statt der Fehlersprunge `A5F6/A5F9`.  ERR 9B/9C verschwinden.

**Korrekturvorschlag:** `core/cards/k5122/k5122.cpp`, `K5122::handleDataPortAWrite` (Z. 831) bzw. `ioWrite(0x14)`: im Lese-
Transfer (`transferring_ && !write_mode_ && !we_writing_`) einen `OUT (14H)` **ohne unmittelbar vorangegangenes IN (16H)**
als Byte-Slot behandeln (Kopf um eins vorrucken, wie ein IN).  Echo-Paare `IN A,(16H); OUT (14H),A` (3rd-Stage-Loader,
Kommentar dort) bleiben unberuehrt: Merkflag `echo_frei_` = true nach jedem IN 16H, im OUT gelesen und geloescht; ein
Resync (`resyncToNextMark`) setzt es auf false.  Alternative, ohne Hypothese ueber den Slot: Resync-Backoff fuer die A4AB-Lage
um eins verkuerzen -- nicht empfohlen, es gibt dort keinen unterscheidbaren Zustand.
**Betroffene Boot-Invarianten/Waechter:** Nr. 4 (treuer Lesestrom 4xA1, MK1-Resync), `K5122Test.*`, `BootIntegration*`,
`BootIntegrationLuecke2.*`, SCPX-INIT/FORMAT-Laeufe (`format_integration`/`format_matrix`), `Udos*`.  Vor dem Merge `test-merge --alles`.
Offen: ob irgendein anderer Gast einen OUT 14H im Lesemodus ohne vorheriges IN schickt (gleiche Verschiebung dann auch dort).

## 2. Ursache 2 (nach der Gegenprobe: ERR 96): geloeschte Datenmarke F8 geht in der BusRq-Schreib-/Lesekette verloren

Nach bestandenem Kontrolllesen schreibt INIT die Etiketten ueber den SIOS-Geraetetreiber (SVC `RST 08`, Parameter ab `0104H`):
Sektoren 1-3 mit Operation `20H` (A983 = 00, ok), dann ab Sektor 9 mit Operation `28H` (**DDR1, DAM F8**,
`A7D3..A7F9`).  Der erste `28H`-Auftrag (Sektor 9) liefert Status **90H** (Watchpunkt `wb A983 == 90`, PC 65FC im Treibermodul 6000H);
INIT zeigt daraufhin `ERR 96`.  `6573..65C9` im Treiber: stimmt die Datenmarke nicht mit der erwarteten (`040AH`, FB/F8 umgeschaltet)
ueberein, wird mit der anderen Marke wiederholt und bei Treffer `B = 90H` gemeldet ("Marke weicht ab").
Die Spur 00 im Ziel nach dem Lauf: **26 x FB** (Python `parseTrack` via `DiskTool`), das Original `sios1526_dp2.hfe` hat 9-26 = F8.
INIT selbst formatiert alle 26 Sektoren mit FB (`[9C85]` = FB bei jedem Sektor, Haltepunkt `b2 A495`), die F8 entstehen erst
durch den Treiber-Schreibzugriff `28H`.

Fehlstellen im BusRq-Weg [Code] (alle drei lassen `LogicalSector::deleted` / Marke F8 unter den Tisch fallen):
1. `K5122::commitWriteField` (`k5122.cpp` ~Z. 1450): uebernimmt Daten/Nachspann, setzt aber `ziel->deleted` **nicht** aus
   `write_buf_[data_start-1]` (0xF8/0xFB); `buildTrack(sektoren, ...)` schreibt dann FB.
2. `TrackCodec::buildFaithfulReadTrack` (`track_codec.cpp` ~Z. 905-915): DAM fest `pushMark(0xFB, ...)` und CRC ueber `{A1,A1,A1,FB,...}`
   statt `sec.deleted ? 0xF8 : 0xFB` -- der Treiber liest also nie F8, auch von einer Originaldiskette (`dp2` Sektor 9-26).
3. `K5122::parseFormatStream` (`handleMark`, ~Z. 1222): `cur.deleted` bleibt false, ein von FORMAT/INIT geschriebenes F8 geht
   verloren.  (Fuer INIT folgenlos, fuer andere Gaeste nicht.)
**Gegenprobe steht noch aus** (braucht Kern-Aenderung): Punkte 1+2 setzen und `INIT` erneut fahren; Erwartung: Status 00 bei Sektor 9-26,
Ende ohne ERR.  Den Kern habe ich auftragsgemaess nicht angefasst.  Wächter: ein `K5122Test` "WriteField_F8_BleibtGeloescht" (BusRq:
Datenfeld mit F8 schreiben, Lesestrom muss F8 + gueltige CRC liefern) bzw. `SiosMonitor`-Test mit INIT bis zum Ende.

## 3. Nebenbeobachtungen
- Das Menue von INIT V1.4 kennt `2  DOUBLE DENSITY, DOUBLE TRACKS` (80 Spuren); bei Auswahl `1` fuer 5,25" laufen die Spuren 00..39.
- Der erste ZVE1-Wartezweig `9ED3` (Abfrage `[9C94]`, 16-Bit-Zaehler BC) lief im Erstversuch einmal in den Zeitablauf (`9EDE`),
  das ist die Wiederholungsschleife (`[9C98]` = 5 Versuche), kein Emulatorfehler.
- bedienung.md Abschnitt 4 ("Verdacht Indexmarke") bitte nach diesem Befund berichtigen (hier nicht geaendert).

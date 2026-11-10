# KCNet (K1520-Netzwerkkarte, Z80-PIO <-> ATmega162 <-> W5100): rechnerseitiges Protokoll

Stand 2026-11-10. Ergänzung zu `doc/design/50_gun_gide.md` (Netz N1/N2). Rekonstruiert **allein aus den Z80-Binärdateien**
(Quelltext und ATmega-Firmware nicht beschaffbar). Eingaben, nur disassembliert (`tools/z80_disasm2.py --org 0x100 [--range]`), nichts ausgeführt, nichts ins Repo kopiert:
`~/Documents/K1520emu/GIDE_Software/kcnet/sw/cpmnet87/{CPMN1587.COM (CPMNET 1.5), TFTP1287.COM (TFTP 1.2)}`, `sw/eeprom/eepload.COM`, `sw/bic/BICBOOT.COM`,
`roboboot.bin`/`RossBOOT.bin` (enthalten keinen KCNet-Zugriff), `gal/K15ETU6.EQN`, `K15ETU11.EQN`. Adressen unten = Ladeadresse 0100H (COM).
Belegt = im Code gesehen; **[?]** = Annahme/Deutung.

## 0. Wichtigster Befund

Beide Programme enthalten **byte-identisch denselben Treiberblock** (TFTP `0109–028E`, CPMN gleiche Adressen, `cmp` der Bereiche 014C–0290: gleich).
Er besteht aus (a) Initialisierung/Erkennung `sub_0109`, (b) Byte-Primitiven `014C` (senden) und `0154` (empfangen),
(c) einer **Sprungtabelle `015C` mit 14 Kommandos**. Beim Start kopiert das Programm die Tabelle (`LDIR`, 42 B) in seine eigene Vektortabelle
(TFTP `0D28`, CPMN `1452`; vor dem Kopieren zeigt jeder Eintrag auf ein `RET`/`JP L1484`). Alles darüber (W5100-Registerlogik, Socket-Ring-Puffer, TCP/UDP/DHCP/DNS)
läuft **im Z80-Programm**; die Karte ist ein dünnes, „sehr generisches" Fenster auf den W5100 (Adressraum 1:1).
Die Menütexte in `CPMN1587` (`4773`: „INTERFACE MENU") benennen die Kommandos ausdrücklich und bestätigen die Zuordnung (Abschnitt 4).

## 1. Adressdekodierung (GAL, bestätigt)

`K15ETU6.EQN`: `/CS_ETH = IO28 + IOC0`; `IOC0 = /A2 /A3 /A4 /A5 A6 A7 J6 J5` -> **C0H–C3H** (A0/A1 nicht dekodiert, laufen an die PIO), `IO28 = /A2 A3 /A4 A5 /A6 /A7 J6 /J5` -> **28H–2BH**. Jumper J6/J5 wählen.
Code bestätigt A0 = Kanal (A/B), A1 = Daten/Steuer: **C0 Daten A, C1 Daten B, C2 Steuer A, C3 Steuer B** (`OTIR` nach C2 bzw. C3, `IN (C0)/(C1)` als Daten).
Der Treiber hat die Basis **C0H fest** einkompiliert (keine 28H-Variante in diesen Binaries). `K15ETU11.EQN` = Boot-/EEPROM-Logik (BTB, KCLTS, MEMDI), mit dem Netzprotokoll unbeteiligt.

## 2. PIO-Initialisierung (belegt, `sub_0109`, `0109–0118`)

```
0109 DI
010A LD C,C2H ; LD B,2 ; LD HL,0103H     ; Datenbytes bei 0103: 8F 07 CF FF 17 FF
0111 OTIR                                ; -> Port A Steuer: 8FH, 07H
0113 INC C   ; LD B,4 ; OTIR             ; -> Port B Steuer: CFH, FFH, 17H, FFH
0118 EI
```
- Port A: `8F` = **Betriebsart 2** (bidirektional, Bits 7:6 = 10, Low-Nibble 1111); `07` = Interrupt-Steuerwort, Bit 7 = 0 -> **Interrupt gesperrt**. Kein Vektor.
- Port B: `CF` = **Betriebsart 3** (Bit-Modus), `FF` = E/A-Register: **alle acht Bits Eingang**; `17` = Interrupt-Steuerwort (Bit 7 = 0 gesperrt, Bit 4 = Maske folgt), `FF` = alle Bits maskiert. Port B ist hier reiner Statuseingang (das ist die für Modus 2 an Port A geforderte B-Betriebsart).
- Das DI/EI umklammert nur die Programmierung; Interrupts werden von der Software nie benutzt.
- Die Software schreibt **nie** ein Ausgangsmuster nach Port B (Daten B `C1` wird nur gelesen).

## 3. Handshake pro Byte (belegt)

```
sub_014C  Byte senden (A):          sub_0154  Byte holen (-> A):
  OUT (C0),A                          IN A,(C1) ; RLCA ; JR NC,sub_0154    ; warte bis PB7 = 1
 L:IN A,(C1) ; RRCA ; JR C,L          IN A,(C0) ; RET
  RET                                 
```
- **Senden:** Byte nach Daten A (`C0`); danach warten, bis **PB0 = 0**. PB0 = 1 bedeutet „letztes Byte noch nicht von der Gegenseite übernommen".
- **Empfangen:** warten, bis **PB7 = 1** („Byte da"), dann Daten A (`C0`) lesen.
- **Keine Zeitüberwachung** in diesen Schleifen (Endlosabfrage). Nur das Gesamtprogramm hat Timeouts (Ticker, s. u.).
- Deutung [?]: PB0 = ARDY-Spiegel (Ausgaberegister voll bis die ATmega-Seite /ASTB gibt); PB7 = „Eingaberegister voll" (ATmega hat /BSTB gegeben, Z80 hat noch nicht gelesen), eventuell invertiertes BRDY. Wichtig fürs Modell: **PB0 muss unmittelbar nach dem `OUT` bereits 1 sein** (rein hardwaresynchron), sonst überholt `IN (C1)` die Warteschleife und das nächste Byte überschreibt das vorige; ebenso muss **PB7 nach dem Lesen von `C0` sofort 0 sein**.
- **Block** (`01DD–020C`, Sendeschleife `OUTI` / Leseschleife `INI`): Kommando + Länge, dann Bytes im Strom, in Stücken von höchstens 256 (`B=C`, 0 = 256); nach **jedem** `OUTI` wird PB0 = 0 abgewartet, vor **jedem** `INI` PB7 = 1. Zwischen den Stücken wird nichts neu gesendet.
- Widerspruch zur Schaltplanlesung: dort „B0 = BTB (EEPROM-Freigabe)". Im Code ist B0 eindeutig das Übernahme-Handshake-Bit der Netzseite. `eepload.COM` (`02D2`, `02DB`) programmiert Port B anders (`FF`, dann `9D` = Eingänge B0,B2,B3,B4,B7; **Ausgänge B1,B5,B6**; Schreibwert `(0D9B<<5)|(0D9A<<1)` nach C1 = B1 / B5,B6 als EEPROM-Segmentbits). Beide Belegungen sind mit demselben Bauteil nur über verschiedene Bestückungsvarianten oder gar nicht vereinbar -> **[?] Schaltplan an der Stelle B0/B1/B4/B5/B6 prüfen**; für die Netzseite zählt nur: B0 Eingang (Handshake), B7 Eingang (Daten da).

## 4. Rahmenformat und Kommandoliste (belegt, `015C–028A`)

Kein Rahmenkopf, keine Prüfsumme, kein Status-/Quittungsbyte. Jedes Kommando = **Kommandobyte** + feste Parameter (alle 16-Bit **Low zuerst**), Antwort (falls vorhanden) sofort danach, jedes Byte mit dem Handshake aus Abschnitt 3.
Die Tabelle `015C` und die Menütexte (CPMN `4773`) ergeben:

| Cmd | Tabelle | Senden nach dem Kommandobyte | Antwort | Name im CPMN-Menü | Gast-Nutzung |
|---|---|---|---|---|---|
| 00 | `01BA` | Adr.Lo Adr.Hi **nur über Kommando 02 vorweg** (der Treiber sendet erst `02 L H`, dann `00 CntLo CntHi`, dann die Daten) | — | WRBYTES | Block Z80 -> W5100-Adresse |
| 01 | `01C3` | (wie 00: `02 L H`, dann `01 CntLo CntHi`) | CNT Bytes | RDBYTES | Block W5100 -> Z80 |
| 02 | `01AA` | L H | — | POINTER ADR | Zeiger setzen (nur intern; Auto-Inkrement im Block, s. u.) |
| 03 | `0275` | — | 2 B (Lo, Hi) | TIMER | 1-ms-Zähler 0..59999 (Umlauf bei 60000: Z80 rechnet `SBC; ADC EA60H`; CPMN-Text „TIMER = n ms") |
| 04 | `0186` | L H Wert | — | WRBYTE ADR BYTE | ein Byte schreiben |
| 05 | `0199` | L H | 1 B | RDBYTE ADR | ein Byte lesen |
| 06 | `021B` | Idx, 4 Bytes | — | SERVER IP-ADDRESSES (Setzen) | vom Gast nirgends benutzt |
| 07 | `0233` | Idx | 4 B | SERVER IP-ADDRESSES (Lesen) | **Idx 0 = DNS-Server** (TFTP `0BB2`: 4 Bytes, danach hängt der Gast Port `00 35` = 53 an) |
| 08 | `0285` | — | 2 B (**Net-Order, hoeherwertiges Byte zuerst**, s. §10) | DYNAMIC PORT | nächster freier lokaler Port (TFTP `0603`: wird nach Sn_PORT `8404/05` geschrieben, hi,lo) |
| 09 | `025F` | — | 2 B (Lo, Hi) | — | **Firmware-Version** H.L (`sub_0D43`; Prüfung `H>=1 und L>=2`, sonst Abbruch „requires at least Firmware-Version 1.2") |
| 0A | `0264` | — | 2 B | — | Hardware-Version H.L (CPMN-Statusseite „Hardware") |
| 0B | `0269` | — | 1 B | — | Link/online (E≠0: online; E=0: „network-cable not connected !", TFTP `1A1D`) |
| 0C | `024B` | — | nullterminierte Zeichenkette | INTERFACE-ID | vom Gast nirgends aufgerufen (nur im CPMN-Menüpunkt 6) |
| 0D | `028A` | — | 2 B | COMMAND ERRORS | Zähler ungültiger Kommandos (**Schluss: unbekannte Kommandobytes werden gezählt und verworfen**; CPMN-Menüpunkt 0) |

Beispiel Block schreiben (TFTP `01BA`): `02 L H` (W5100-Adresse) – `00 CntLo CntHi` – Cnt Datenbytes. Block lesen analog mit Kommando 01.
Die Adresse wird **bei jedem Blockbefehl neu gesetzt**; ob der ATmega den Zeiger über Kommandos hinweg weiterzählt, ist nicht erforderlich und nicht belegt.
Zugriffe jenseits des Kommandosatzes (Länge 0) werden gar nicht erst gesendet (`01BA`: `BC=0` -> Rücksprung).

### Fehlercodes
Es gibt keine. Ungültige Kommandos werden nur gezählt (Kommando 0D). Der Gast prüft nach der Initialisierung nur: PB7 (Karte da), Version (Kommando 09 >= 1.2), Link (0B) und Netzkonfiguration (IP-Register ≠ 0, TFTP `1A46`: „network not configured !").

## 5. Erkennung und Reset der Karte (belegt)

`sub_0109` nach der PIO-Programmierung (`0119–0124`):
```
IN A,(C0)        ; Dummy-Lesen (leert ein evtl. gefülltes Eingaberegister)
IN A,(C1) ; RLCA ; JR C,nicht_gefunden      ; PB7 = 1 -> "KCNET-Interface not found !"
LD HL,015CH ; OR A ; RET                    ; Erfolg: HL = Zeiger auf Kommandotabelle, CY = 0
```
- „Gefunden" heißt: **PB7 = 0 im Ruhezustand nach einem Dummy-Lesen von `C0`.** Fehlt die Karte, liest `IN` am offenen Bus/Pull-up 1.
- Sofort danach (`271A…2725`) kopiert der Gast die Tabelle und fragt **Kommando 09** ab (Version >= 1.2). Kein Warten, kein Wiederholen, kein Zeitlimit, kein Reset-Kommando auf ATmega-Ebene.
- Der Gast verlangt außerdem CP/M 2.x (`BDOS 0CH`, `AND F0H`, `CP 20H`).
- Es gibt **kein** Reset-/Kaltstartkommando der ATmega-Firmware. „Clear Network Configuration / RESET successful" in CPMN löst den **W5100-Software-Reset** aus: MR (W5100 `0000`, im Gast `8000`) <- `80H`, danach lange Zählschleife (`0295–029C`, ≈ 256 × 4 Befehle) und Test `MR & 80H` = 0 -> Erfolg.

## 6. Abbildung auf den W5100 (aus dem Gastcode ableitbar)

Der Gast adressiert den W5100 **1:1 mit gesetztem Bit 15** (`8000H | W5100-Adresse`); alle Adressen im Programm liegen bei `8000H…FFFFH`. [?] ob der ATmega Bit 15 ignoriert/maskiert (sehr wahrscheinlich: W5100 hat 15 Adressbit, `0x8000` ist vermutlich Auswahl des W5100-Fensters, ggf. gibt es einen zweiten Adressraum bei Bit 15 = 0 für ATmega-Interna/EEPROM-Konfiguration — nicht belegt).
Beleg der Registerbelegung (W5100-Datenblatt):
- Gemeinsam: `8000` MR, `8001` GAR (4), `8005` SUBR (4), `8009` SHAR (6, im Menü „RO"), `800F` SIPR (4), `8015` IR, `8016` IMR, `8017` RTR (2, Menü „Retry Time ×100 µs"), `8019` RCR, `802A/802E` UIPR/UPORT (TFTP `0315`/`031B`).
- Socket n: Basis `8400+n*100H`: `+0` Sn_MR (Protokoll 1=TCP, 2=UDP, 3=IP-RAW, 4=MAC-RAW), `+1` CR, `+2` IR, `+3` SR, `+4` PORT (2), `+6` DHAR (6), `+0CH` DIPR, `+10H` DPORT, `+12H` MSSR, `+14H` PROTO, `+15H` TOS, `+16H` TTL, `+20H` TX_FSR, `+24H` TX_WR, `+26H` RX_RSR, `+28H` RX_RD.
- Tx-/Rx-Puffer: Tx `C000H + n*800H`, Rx `E000H + n*800H` (`sub_03D9/03DE`), Größe je Socket **2 KB** (Maske `07FFH`, `sub_03D1/03D5`; also RMSR=TMSR=55H). **Ringpuffer-Umbruch und Zeiger (TX_WR/RX_RD) verwaltet der Gast selbst**: bei Überlauf zerlegt `sub_03ED` in zwei Blockbefehle (`0D28`/`0D2B`).
- Beispielsequenzen (TFTP):
  - Socket öffnen (UDP): Sn_PORT <- dynamischer Port (Kommando 08), Sn_CR (`8401`) <- `01` (OPEN), dann Sn_SR (`8403`) lesen und gegen `22H` (UDP) bzw. `13H` (TCP INIT) prüfen (`0617–063C`).
  - TCP verbinden: `391` setzt DIPR (`840C`), `377` DPORT (`8410`), CR <- `04` (CONNECT), Schleife auf SR = `17H` (ESTABLISHED) oder `00` (CLOSED) (`066F–0684`).
  - UDP senden: Zeiger `TX_WR` (`8424`) lesen (2 Einzelbytezugriffe, hi vor lo), Daten per Block ab `C000+n*800+(wr&7FF)`, `TX_WR` neu schreiben, CR <- `20H` (SEND); Empfang: `RX_RSR` (`8426`) bis zweimal gleich lesen (`04F3/0523`), Daten ab `E000…`, `RX_RD` (`8428`) zurückschreiben, CR <- `40H` (RECV).
  - IP/Netzmaske lesen: `sub_02E5`/`sub_02F9` = Blocklesung `800F`/`8005` je 4 Bytes. MAC wird nur gelesen (Menü: „SHAR (RO)").
  - DHCP/DNS/ICMP laufen vollständig im Gast über UDP/IP-RAW-Sockets.
- Abgeleitet für die Firmware: Kommando 00/01/04/05 sind ein **W5100-Speicherfenster** (vermutlich über SPI oder Adress-/Datenbus des ATmega, Auto-Inkrement innerhalb eines Blockbefehls); die Kommandos 03, 06–0D sind ATmega-eigene Dienste (Ticker, Konfig-IPs im EEPROM, Zähler).

## 7. Offene Punkte (nur mit echter Hardware klärbar)

1. Genaue Bedeutung/Polarität von PB0 und PB7 und deren zeitliche Lage (ARDY/BRDY direkt oder ATmega-gesteuert), insbesondere die Reaktionszeit der ATmega-Seite je Byte (Durchsatz, Blockschreiben mit `OUTI` 21 T-Zustände + Abfrage pro Byte).
2. Bit 15 der Adresse (Maske? zweiter Adressraum?). Adressen mit Bit 15 = 0 kommen im Gast nie vor.
3. Auto-Inkrement des Zeigers über Blockende und über Kommandos hinweg; Verhalten bei Länge > Puffer, Länge 0 (der Gast sendet es nie).
4. Was bewirken Kommando 06 (Setzen) und 0C (ID-String) und welche Indizes ≠ 0 kennt Kommando 06/07? Wert der ID-Zeichenkette, Hardware-Version (0A), echte Firmware-Versionen.
5. Reaktion auf ein unbekanntes/abgebrochenes Kommando (Zähler 0D), insbesondere ob der ATmega nach Abbruch mitten im Block wieder synchronisiert.
6. Zeitbasis von Kommando 03 (1 ms, Umlauf 60000) und von Kommando 08 (Erzeugung des dynamischen Ports, Zufall oder Zähler).
7. Konflikt Port-B-Belegung Netz vs. EEPROM-Auswahl (B0/B1/B5/B6, Abschnitt 3) und ob die PIO-Ausgänge für EEPROM-Segmente bei gestecktem Netzbetrieb (Direction FF) hochohmig sind.
8. Wer konfiguriert den W5100 beim Einschalten (MAC aus EEPROM, IP?), Link-Statusquelle (PHY-Register) und ob Kommando 0B Link oder „Netz konfiguriert" meldet.
9. Wirkung des PIO-Interrupts (vorbereitet, ungenutzt) – unrelevant für die Gastprogramme.
10. `eepdisp/eepdel.COM`, `vsolraf.COM` und die Boot-Images wurden nicht auf KCNet-Zugriffe hin zerlegt (eepload/BICBOOT/roboboot/RossBOOT: kein Netzzugriff).

## 8. Bewertung

**Reicht für ein Verhaltensmodell der ATmega-Firmware aus**, das `TFTP1287.COM` und `CPMN1587.COM` unverändert bedient:
- Das Protokoll ist vollständig und klein (14 Kommandos, byteweise Handshake, keine Rahmen/Prüfsummen/Status), alle Kommandobytes und Längen sind belegt.
- Fehlt nur: (a) die elektrische Handshake-Hülle (PB0/PB7 als direkt synchrone Folge von PIO-Betriebsart 2; im Emulator als Zustandsbits des Kartenmodells gleichwertig, Anforderung: PB0 = 1 sofort nach `OUT`, PB7 sofort nach `IN` = 0), (b) Werte der Identitätsdaten (Version >= 1.2, Hardware-Version, Link = online, DNS-Server), (c) ein plausibles Verhalten für Zeitgeber/Ephemeralport.
- Modell: W5100-Registermodell aus N1 (`core/…/w5100`) hinter einem Byte-Zustandsautomaten für die Kommandos 00–0D; Bit 15 der Adresse maskieren; Tx/Rx-Puffer 2 KB × 4 Sockets wie der Gast sie benutzt.
- Nicht zu klären ohne Hardware: Feinheiten aus Abschnitt 7, Nr. 1–5; für die Gastprogramme folgenlos, solange PB0/PB7-Semantik und Kommandoumfang stimmen.

## 9. Bestaetigung und Korrekturen durch das Verhaltensmodell (AP N2, 2026-11-10)

`core/cards/gun_net/` bedient `TFTP1287.COM` und `CPMN1587.COM` **unveraendert** (Basis C0H; Tests `GunNetSystem.*`).  Gegen den Befund oben:
- **Bestaetigt:** Handshake wie Abschnitt 3 (PB0 nach `OUT` = 1 bis zur naechsten Abfrage, PB7 nach `IN` = 0), Erkennung ueber PB7 = 0 nach dem Dummy-Lesen, 14 Kommandos
  mit den Parameterlaengen der Tabelle, Antworten **Lo, Hi** (03, 0D; Kommando 08 dagegen **Hi, Lo** — berichtigt in §10), Bit 15 maskiert (Tx `C000H` -> `4000H`, Rx `E000H` -> `6000H`), 2-KB-Puffer je Socket (RMSR = TMSR = 55H) mit
  Zeigerverwaltung im Gast.  Der Zeiger aus Kommando 02 zaehlt im Block weiter; ob ueber Kommandos hinweg, braucht der Gast nicht (Modell: ja).
- **Bildschirm CPMNET `I`:** `Software 1.2      Hardware 1.0   online` (Kommandos 09/0A/0B), `0` -> `COMMAND ERRORS = 0`.  TFTP ohne IP: `network not configured !`;
  Kabel ab (0B = 0) **und** IP gesetzt: `network-cable not connected !` (die IP-Pruefung kommt zuerst).
- **Neu:** Der Gast zeigt `MAC-Address` aus SHAR — die Firmware muss die MAC nach dem Einschalten setzen (Modell: `02:4B:43:4E:45:54`, `Config::mac`), sonst `00:00:..`.
  "Network: configured" erscheint erst mit IP **und** Netzmaske (nur IP gesetzt, Maske 0: `not configured`).  TFTP sendet den Modus **`OCTET`** (Grossbuchstaben), antwortet DATA-Bloecke aus fremder TID (UDP-Socket nimmt
  jeden Absender an) und laedt damit eine Datei bitgleich.  CPMNET `S` (TCP-Server) benutzt Socket 3 und schliesst bei lokalem Port 0 sofort (`TCP SOCKET CLOSED`).
- **Modellentscheidung (nicht belegt):** Betriebsartwort an Steuer A wirft Rest-Kommandos/Antworten weg (Abschnitt 7 Nr. 5); /RESET setzt den W5100 per MR-Reset zurueck.
- **Weiter offen:** Nr. 1 (genaue Zeitlage PB0/PB7), 4 (06/0C), 6 (Zeitbasis 03/08 ausser Maschinenzeit), 7, 8, 9.

## 10. Abgleich mit der offiziellen Beschreibung (2026-11-10)

Quelle: Ralf Kaestner, **"KCNET Protokoll Version 1.2"** (kcnet_v1.2_prg_de.pdf, kc85.info, 1 Seite, 2008-12-14) und der Artikel "KCNET 1.2" (kc85.info, 2008-07-15);
lokal in `~/Documents/K1520emu/GIDE_Software/kcnet/doku/` (nicht im Repo).  Die Beschreibung ist **massgeblich** vor den Deutungen der Abschnitte 1-9.
Die Firmware selbst ist Closed Source (nur auf persoenliche Anfrage); das Verhaltensmodell `core/cards/gun_net/` ist deshalb der gewollte Weg.

| Kdo | Name | Parameter (Reihenfolge) | Antwort (Reihenfolge) |
|---|---|---|---|
| 0 | Schreibe Bytes nach Zeiger+ (8000H+) | Zaehler NWB, HWB, dann Zaehler Bytes | — |
| 1 | Lese Bytes von Zeiger+ | Zaehler NWB, HWB | Zaehler Bytes |
| 2 | Schreibe Adress-Zeiger fuer Kdo 0/1 | Adresse NWB, HWB | — |
| 3 | Lese Timer (0..59999 ms) | — | Timer NWB, HWB |
| 4 | Schreibe Byte nach Adresse (8000H+) | Adresse NWB, HWB, Byte | — |
| 5 | Lese Byte von Adresse | Adresse NWB, HWB | Byte |
| 6 | Schreibe IP-Adresse | Nummer 0..7, 4 Byte **Net-Order** (b4,b3,b2,b1 = a.b.c.d) | — |
| 7 | Lese IP-Adresse | Nummer 0..7 | 4 Byte Net-Order |
| 8 | Lese dynamische Portnummer | — | 2 Byte **Net-Order (b2,b1)**, hoeherwertiges Byte zuerst |
| 9 | Lese Software-Version | — | Unterversion, Hauptversion (1.2 -> 2, 1) |
| 10 | Lese Hardware-Version | — | Unterversion, Hauptversion (1.0 -> 0, 1) |
| 11 | Lese Link-Status | — | 1 Byte |
| 12 | Lese Kennung | — | Zeichenkette + 0, **hoechstens 128 Byte** |
| 13 | Lese Anzahl Kommando-Fehler | — | Anzahl NWB, HWB |

NWB/HWB = nieder-/hoeherwertiges Byte.  Die Gast-Befunde der Abschnitte 4/9 (Zeigerfuehrung, 1:1-Fenster mit Bit 15) bleiben gueltig.

**Abweichungen des Modells vor dem Abgleich und ihre Korrektur** (Wächter in `tests/unit/cards/test_gun_net.cpp`):

| # | Abweichung | Beleg | Korrektur / Test |
|---|---|---|---|
| 1 | Kdo 8 lieferte Lo,Hi (49152 = `00 C0`) | PDF "Net-Order (b2, b1)"; Gast `TFTP 0603`: erstes Byte -> Sn_PORT 8404H (= Hi) | Hi,Lo; `DynamischerPortZaehltAb49152InNetOrder` |
| 2 | PB1..PB6 lasen 1 (Pull-ups) | Artikel: "PIOB1 bis PIOB6 gehen an Masse ... bei Eingaben immer 0" | PB1..PB6 = 0; `PortBPb1BisPb6SindMasse` |
| 3 | Schreiben unterhalb 8000H wirkte (Bit 15 maskiert) | PDF "nur auf Adressen des TCP/IP-Stack Schreibzugriff (8000H...FFFFH)" | wirkungslos (Kdo 0 und 4; der Zeiger zaehlt trotzdem); `SchreibenNurAb8000HUndNichtInDieMac` |
| 4 | MAC 8009H..800EH war ueber die Karte beschreibbar | PDF "kann die Ethernet-MAC-Adresse ... nicht beschreiben" | wirkungslos (Kdo 0 und 4); gleicher Test.  Die MAC kommt aus `Config::mac` (Firmware laedt sie nach /RESET) |
| 5 | Kein Fristabbruch | PDF "blockierende Zustaende ... nach 524 ms abgebrochen und die komplette Befehlsverarbeitung zurueckgesetzt" | s. unten; `Frist524msSetztAngefangenesKommandoZurueck`, `Frist524msVerwirftNichtAbgeholteAntwort`, `FristLaeuftNichtWennKeinKommandoOffenIst` |
| 6 | Kdo 6/7 nahmen Nummer `& 7` | PDF "Nummer (0...7)" | alle 8 Plaetze; Nummer > 7 = Kommandofehler (Zaehler 0D, keine Antwort, nichts gespeichert) **[?]** (Beschreibung sagt nichts); `IpNummern0Bis7SindAlleSchreibUndLesbar` |
| 7 | Kdo 12 ohne Laengengrenze | PDF "Zeichenkette + 0 (max. 128 Byte)" | hoechstens 127 Zeichen + 0; `KennungHoechstens128ByteMitNull` |

Bestaetigt ohne Aenderung: Zaehler/Adresse/Timer/Fehlerzaehler niederwertig zuerst, IP Net-Order, Versionen (Unter-, Hauptversion), Timer 0..59999 ms (Maschinenzeit), dyn. Port ab 49152 aufsteigend
(Artikel: bis 65535, "16383 Aufrufe"; das Modell laeuft nach 65535 auf 49152 um), Link 1 Byte, Fehlerzaehler 16 Bit.

**524-ms-Regel.**  Die Firmware wartet bei der PIO-Ausgabe (Antwortbyte nicht abgeholt) und der PIO-Eingabe (Kommando/Daten unvollstaendig) hoechstens 524 ms, dann wird die gesamte Befehlsverarbeitung
zurueckgesetzt.  Modell: 524 ms **Maschinenzeit** (`GunNet::takt`, `Config::takt_hz`) ohne ein Byte in eine der beiden Richtungen, waehrend ein Kommando angefangen oder eine Antwort offen ist, werfen Kommando,
Parameter, Restdaten und Antworten weg (`Zaehler::fristabbrueche`); Zeiger (Kdo 2) und Fehlerzaehler bleiben **[?]** (die Beschreibung nennt nur die Befehlsverarbeitung).  Geprueft wird bei jedem
E/A-Zugriff und in `takt()`.  Im Ruhezustand (nichts offen) laeuft keine Frist.  Ersetzt die bisherige "Modellentscheidung" nicht, ergaenzt sie: das Betriebsartwort verwirft weiter sofort (Modell, der ATmega sieht es nicht).
Der Gast (CPMNET/TFTP) bleibt davon unberuehrt; die Treiber warten ohne eigene Frist (Abschnitt 3).

**Schreibschutz.**  Die PIO beschreibt nur den TCP/IP-Stack (8000H-FFFFH), nicht die MAC (8009H-800EH).  Lesen ist nicht eingeschraenkt; Lesen unterhalb 8000H ist unbelegt (Modell: Bit 15 maskiert, wie bisher) **[?]**.

**Handshake / Port B (Aufloesung von Abschnitt 3 und 7 Nr. 1, 7).**  Hardware 1.2: **ARDY der PIO geht direkt auf PB0, BRDY invertiert auf PB7, PB1..PB6 liegen an Masse**.  ARDY = 1, solange das
Ausgaberegister von Port A gefuellt ist und die Gegenseite es nicht abgeholt hat -> "Senden: warte bis PB0 = 0".  BRDY = 1, solange das Eingaberegister leer ist; invertiert: PB7 = 1 <=> Antwortbyte da ->
"Empfangen: warte bis PB7 = 1", nach dem Lesen von Daten A sofort wieder 0.  Das stimmt mit dem Gastcode (Abschnitt 3) und dem Modell (PB0/PB7 als Zustandsbits) ueberein.  Der "Widerspruch zum Schaltplan"
(B0 = BTB, B1/B5/B6 als Ausgaenge fuer die EEPROM-Segmentwahl in `eepload.COM`) betrifft die **K1520-Platine** mit zusaetzlicher EEPROM-/Boot-Logik (`K15ETU11.EQN`), nicht die Netzseite: ohne EEPROM-Bestueckung
liegen B1..B6 an Masse; fuer die Netzfunktion zaehlen nur B0 und B7.  Ob die K1520-Platine B1/B5/B6 zusaetzlich ausgibt, bleibt **[?]** (Plan, nicht Netzmodell).  Damit sind Abschnitt 7 Nr. 1 (Polaritaet), 4 (Kdo 6/12 definiert), 6 (Zeitbasis Kdo 3/8
= 1-ms-Zaehler 0..59999 bzw. aufsteigend ab 49152) und 7 geklaert; offen bleiben Nr. 2 (Lesen < 8000H), 3 (Zeiger ueber Blockende), 5 (Synchronisation nach Abbruch: nun durch die 524-ms-Regel beschrieben), 8, 9.

**RS232-Kommandos** (zweite Tabelle der PDF: Terminalmenue `k c s h l w r p x y i o` ueber MAX232 am ATmega, ASCII) sind eine Debug-Schnittstelle der Firmware, nicht der Z80-Seite; nicht nachgebildet (Entscheidung: nicht relevant,
hoechstens spaeter als Debugger-Befehl `net`).  `m052net.dok` beschreibt dagegen die KC85-Modul-Software (CAOS-EEPROM-Segmente, NCFG-Menue), kein RS232-Menue.

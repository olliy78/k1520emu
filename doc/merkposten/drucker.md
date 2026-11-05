# Merkposten — Drucker (`k1520printer`, Entwurf 49, Ticket #28)

Gilt für `core/peripherals/druckwerk/`, `core/cards/printer/`, `core/machines/printer/`, `k1520_printer_*` in der C-ABI und den Debugger-Befehl `blatt`.
Entwurf und Stand: `doc/design/49_k1520printer.md` (§5a/§5b); Hardware des SD 1157 aus der Firmware: `doc/printer/sd1157/hardware.md`.
Stand 2026-11-05 (WP3/WP2b, WP4 Anschluss).

1. **Das Druckwerk kennt kein Gerät, kein PDF, kein Qt.**  `Blattmodell` und `Nadelkopf` wissen nichts vom SD 1157; die Steuerung gibt Pegel rein (`Wagen::eingang`,
   `Papierweg::ausgaenge`) und löst den Anschlag aus.  Alles in Maschinentakten.  Wächter `Blattmodell.*`, `Nadelkopf.*`, `Papierweg.*`, `GeregelterWagen.*`, `Nadeldruckwerk.*`
   (`tests/unit/peripherals/druckwerk`), Label `fach_drucker`.
2. **Der Wagen-Schnittstelle fehlt nichts für den K6316**: `WagenEingang::phasen` rein, `WagenRueckmeldung::g1` raus.  `SchrittWagen` ist trotzdem NICHT gebaut (WP2a).
3. **Raster des Blatts:** x = 2 Pixel je Takt der Taktscheibe (1/200″), y = 1/144″ (Papierschritt 1/48″ = 3, Nadelteilung 1/72″ = 2, 6/8 lpi = 18/24 Zeilen).  Takte je Zoll (100),
   Nadelteilung, Punktmaß sind `[?]`-Annahmen im `NadeldruckParameter`; die Firmware sieht sie nie.  Das Blatt wächst mit der Papierlage (Rand oben `rand_oben_y` = 36,
   damit Rückzüge über Lage 0 hinaus nicht ins Leere gehen).
4. **Flugzeit des Kopfes (am Verhalten der Firmware gemessen).**  Die Firmware löst rückwärts einen Takt später aus als vorwärts.  Mit idealem Kopf (Punkt = Ausloseposition) liegt jede
   rückwärts gedruckte Zeile 1 Takt rechts der vorwärts gedruckten.  Beide Richtungen decken sich nur mit **0,5 Takt Flugzeit je Richtung** (`versatz_vor = +1`, `versatz_rueck = -1` Pixel
   bei 2 Pixeln je Takt, Punktbreite 2).  Nicht in der Anzeige kaschieren, nicht auf 0 stellen.  Wächter `Sd1157Selbsttest.OhneFlugzeitLiegtDieRueckwaertsZeileEinenTaktRechts`,
   `Nadelkopf.VorUndRueckwaertsLandenMitFlugzeitAufDenselbenPunkten`, `Nadeldruckwerk.RueckwaertsfahrtGleichtDieFlugzeitAus`.
5. **PIO-Interrupts im Modus 3 sind am SD 1157 EREIGNISSE** (`Z80PIO::setModus3Flanke(true)` auf allen drei PIOs, U855 §6.4: Übergang Nichterfüllung → Erfüllung).  Im Pegelmodus
   (Vorgabe des Bausteins) fordert jede Auffrischung der Eingangspegel (`eingaengeSetzen`) bei /LR = 0 den Randinterrupt neu an, auch durch einen **fremden** Pinwechsel; die ISR setzt
   Spaltenzähler (kpz = 7, h' = 6) mitten im Auslauf zurück, die Firmware hält sich dann für 70 Takte zu weit rechts und druckt die erste Zeile rückwärts ins Leere.
   **Verdacht gegen das Primitive (nicht geändert, wie vorgegeben):** der Pegelmodus von `Z80PIO::checkInterrupt` legt bei jedem Aufruf mit erfüllter Bedingung neu an, nicht nur
   bei einer Änderung der überwachten Bits.  Die Pegelauffassung gilt für HARDY (Kommentar in `writeCtrl`); ob sie am echten Baustein stimmt, ist offen.
6. **/VK deckt den /LR-Bereich ab (`vk_bis >= lr_bis`, Vorgabe 100).**  WP0b nahm „/VK = 0 nur ≤ 10“ an: damit blieb die Grundstellung aus Randlagen am Anschlag hängen.  Herleitung aus
   Firmware (§10.1/§11, `sy23..sy30`): beim Einschalten liest die Firmware /VK; 0 → vorwärts, 1 → rückwärts, in beiden Fällen eine **LFG-Spitze** (Mikrosekunden), nach der der Wagen
   den Bremsweg (70 Takte bei VTAB = 1) ausläuft; danach wird /VK **erneut** gelesen, und bei 1 fährt die Firmware dauernd rückwärts bis zur /LR-Flanke.  Das funktioniert nur, wenn
   /VK = 1 bedeutet, dass /LR schon 1 ist.  Mit `vk_bis = 10` hängen die Startlagen −50, 0, 3 (Spitze vorwärts 70 → /VK = 1, /LR = 0) und 100, 101 (Spitze rückwärts → Position 30/31: /VK = 1, aber /LR = 0)
   am Anschlag −200 (am Lauf gemessen).  Kein Sonderfall im Wagen: das Verhalten folgt aus Position und Sensorlage.  Wächter `Sd1157Rom.GrundstellungLaeuftAusAnderenStartlagenDurch`
   (20 Startlagen von −200 bis 2399), `GeregelterWagen.VorwarnkontaktDecktDenLrBereichAb`.
7. **Bremsweg ~ v²:** 20 Takte bei Druckgeschwindigkeit, 70 bei VTAB = 1 — die Firmware bremst selbst 2 bzw. 7 Spalten vor dem Ziel (§10.1).  Wächter `GeregelterWagen.AuslaufNach…`.
   Nach /LR bleibt der Wagen links bei Position ~3 stehen; **/RESET versetzt Wagen und Papier nicht** (`halt()`), der Neustart läuft dann aus dem Randbereich (~8,6 Mio. Takte:
   erst 30 Spalten vor, dann zurück).  Wächter `Sd1157Rom.ResetStartetNeu…` (10 Mio. Takte Frist).
8. **Anfangsvorschub = 56 Schritte** (nicht 48): 24 vor, Rückzug 24 + 4 (`add a,4` in m24dd, Spielausgleich) zurück, 4 vor; Nettolage 0, Extremwerte +24/−4.  Wächter
   `Sd1157Papier.AnfangsvorschubIst56SchritteMitNettolageNull`, `Sd1157Rom.GrundstellungBisZurLeerlaufschleife`.
9. **Der Selbsttest wird mit einem Orakel geprüft, nicht mit einer Prüfsumme** (`test_sd1157_druck.cpp`): die erwarteten Punkte entstehen aus `1157zg.rom` (Glyphen, Teststrom ab 0300H) und den
   Regeln: Normal 10 Takte/Zelle ab Takt 80; Breit 20 Takte, Spalte k bei +2k aus Spalte[k] UND Spalte[k−1] (Vorspalte je Zeichen FFH), Spalte 6 noch einmal bei +14 (m1d95/m1de2);
   Schräg: Nadel j um 8−j Takte nach rechts, Nadel 9 nicht (Ring der Quelle m1e1b..; der absolute Versatz ist am Lauf gewonnen, aber an allen 20 Zeichen bestätigt).  Passt das Bild nicht,
   ist das ein Befund — **das Orakel nicht dem Emulator anpassen**.  Teststart: Taste ONLINE offline **vom Anlauf an** (`Config::online = false`), TEST halten, loslassen bei PC `0FA6` (hpt3 =
   Ende des Teststroms; sonst wiederholt die Firmware den Zyklus), dann 40 Mio. Takte (Zeilen, FF, Neustart).  Wächter `Sd1157Selbsttest.*`.
10. **Was die Firmware von den Bedienelementen nur beim Anlauf liest:** Taste 6/8 lpi (BCH.1, `sy34` -> `za61`) und der Zustand `off`/Online-Lampe (`sy40`).  **Online/Offline zur Laufzeit
    (am ROM gemessen, Wächter `Sd1157Online.*`; Quelle 760-910):** `off` (IX+5 Bit 7) setzen nur `sy40` (Anlauf offline) und `m05b3`; `m05b3` ist der EINZIGE Ort, der TEST auswertet, und nur über
    `m05ee` (`off` = 1) erreichbar.  Wird ONLINE im Betrieb auf offline gelegt (`off` = 0), gilt: **PV1/PV2 wirken** (Zweig `m0646`/`ta00`, ein LF = 8 Schritte), **TEST nicht** (kein Selbsttest in
    20 Mio. Takten), **die Online-Lampe (2A.1) bleibt an**; wieder online: PV1 wirkt nicht mehr.  Ein kurzer Offline-Impuls im Leerlauf (Taster) ändert nichts.  Erst ein **/RESET** bei offline
    liest ONLINE neu (`sy40`): Lampe aus, TEST läuft.  Daten: im Empfang schaltet `seri8` offline sofort auf DC3 (Quelle; am Lauf nicht geprüft, s. Nr. 16).  **[?]** ob ONLINE am Gerät rastend
    oder Taster ist: die Quelle unterscheidet das nicht (liest nur den Pegel); für die Oberfläche ist „rastend + Reset-Hinweis" die einzige Bedienung, bei der TEST erreichbar ist.  Zur Laufzeit
    gewechselt ist das **Gastverhalten, kein Emulatorfehler**.  PV1 kurz = ein LF (8 bzw. 6 Schritte), PV1 ≥ ~0,85 s = Formularvorschub bis zur Formularlänge (BCH.5..4: 192/288/384/576).
    Wächter `Sd1157Tasten.*`.
11. **Papierende:** `/PE` fällt bei Papierlage ≥ `Papierweg::Parameter::laenge`; beim Vorschub setzt die Firmware das Statusbit `zmb pe` (IX+0AH Bit 2, RAM 2C5AH) mit dem ersten Schritt und
    `ape` (IX+7 Bit 5, 2C57H) 82 Schritte später (Lage 100 → 181); der Vorschub läuft aber bis zum Ende des Auftrags weiter.  Beim Anlauf ohne Papier steht `zmb pe` gleich im Register und
    löscht sich mit dem Papier.  Online-Lampe und Fehlerlampe ändert das nicht (`hpv2` setzt im seriellen Betrieb nur Ausgänge zurück).  Wächter `Sd1157Papierende.*`.
12. **NMI** (`PrinterMachine::nmi`, Farbbandhavarie) setzt die Fehlerlampe (/TF = 0).  Wächter `Sd1157Tasten.OnlineLampeFolgtDemEinschaltzustandFehlerLampeDerHavarie`.
13. **C-ABI `k1520_printer_*`** (additiv, `core/api/k1520_api.h`): Blatt je Zeilenbereich (`_sheet_rows`, Bit 7 = linkester Punkt), `_sheet_size`, `_sheet_version`, `_sheet_clear`, `_head`,
    `_geometry`, `_key`, `_switch`/`_switch_get`, `_lamp`; Konstanten `K1520_PRINTER_KEY_*`, `_SW_*`, `_LAMP_*`; an jeder anderen Maschine 0 bzw. −1.  Wächter `py_c_api`
    (`test_printer_sheet_keys_lamps_and_switches_through_the_c_api`).
14. **Debugger:** `k1520dbg --machine sd1157`, Befehl `blatt [info | x0 y0 [breite hoehe [sx sy]]]` zeigt einen Ausschnitt als ASCII (`#` = Punkt, Pixel; `sx`/`sy` Pixel je Zeichen) —
    dieselbe Hilfsfunktion `blattAscii` liefert den Abzug in der Fehlermeldung der Druckertests.
15. **Offen:** zweite Bahn im Druck (`Nadeldruckwerk::feuere` schreibt nur auf Bahn 0; WP6b), `SchrittWagen`/Schnitt/Blattgrenzen (WP2a, K6316), Text-Druck über die SIO (Nr. 16), absolute `[?]` (Takte je
    Zoll, Nadelteilung, Punktmaß, Flugzeit absolut), Papierende-Lampe (es gibt keinen Ausgang).
16. **SIO-Anschluss (WP4):** `Sd1157Steuerung::anschluss()` liefert je Kanal einen `SerialAnschluss` (B = „IFSS", A = „V.24"; Hub-Index 0 = IFSS, 1 = V.24; `PrinterMachine::serialHub()`,
    C-ABI `k1520_serial_*`, alter Unterbau DFU = V.24, PRINTER = IFSS).  Ohne Hub (Karte allein) leert eine Senke den Sender.  **Am V.24 gibt die Karte das DC1 nach dem Einschalten erst frei, wenn ein Transport
    anbindet** (CTS/DCD kommen vom Wandler, Auto Enables halten das Zeichen; wie K8025) — der Host sieht es dann.  **Baud:** CTC 0 ist Baudgenerator und wird im Takt-Interrupt (`m1a0d`, `17H,17H`, TC = 23) als
    Zeitbasis des Drucks neu gestartet; `Sd1157Steuerung::baudTakte()` lässt als Baud nur Perioden der Baudtabelle gelten (16 x 2^n, n = 0..7) und hält sonst die zuletzt programmierte — die Firmware
    sieht die CTC unverändert (Wächter `Sd1157Seriell.DieBaudrateBleibtBeiDerDruckZeitbasisDerCtc0Stabil`, belegt, dass TC = 23 im Selbsttest tatsächlich auftritt).  **[?] Empfangspfad:** Zeichen kommen
    verlustfrei im Puffer an (Wächter `Sd1157Empfang.*`), aber der Schiebepuffer des ROM (0D20H) zieht `ipzl` um 2 und `ipzf` um 1 ab, nachdem `ipzl` um 1 erhöht wurde: der Abstand `ipzf - ipzl` bleibt
    konstant, der Puffer erscheint nie leer, die Firmware holt endlos NUL und kommt kaum zum Druck (3 Testzeilen „AAA", „BBB", „CCC" drucken nichts).  Ob das der ROM-Abzug ist oder eine Annahme der Karte
    (die Quelle ohne Patch leert korrekt), ist **nicht geklärt**; DC3-Fluss (ab `ipzf` >= 3BF5H) und IFSS-Freigabe sind deshalb nicht prüfbar.  Nicht umgehen, erst klären.
17. **Noch nicht untersucht (WP4-Rest):** Flugzeit-Erklärung über Takt-Interrupt/FGDI-Zeitpunkt (Nr. 4 bleibt Anpassung, nicht Beleg); Steuerfolgen/TAB/FF/Papierende im Auftrag; Loopback-Telnet-Rundlauf.

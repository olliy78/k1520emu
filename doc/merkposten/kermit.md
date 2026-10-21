<!-- Angelegt 2026-10-18 (AP K7, Entwurf 36).  Diese Datei gilt WIE CLAUDE.md, sobald an der
     Kermit-Bibliothek (kermit/), k1520kermit, app/kermittool oder Kermit auf WEGA gearbeitet wird —
     sie ist nur nicht in jeder Anfrage geladen.  Begruendung: doc/merkposten/README.md -->

# Kermit (Bibliothek `kermit/`, WEGA-Gegenstelle) — Merkposten

Eigene Kermit-Bibliothek `libk1520kermit` (C++ `k1520::kermit`, C-ABI `k1520k_*`), Kommandozeile
`k1520kermit`, Oberfläche `app/kermittool/`; Gegenstelle im Emulator ist C-Kermit 4.2(030) auf der
WEGA-3.1-Platte (`disks/p8000_wega31/`).  Entwurf und Stand je AP: **`doc/design/36_kermit.md`**
(§6.5 = Gegenprobe K7), WEGA-Seite: **`doc/p8000/kermit_wega.md`**, Bedienung: `tools/k1520kermit.md`.
C-Kermit-Quellen (zum Nachlesen der Eigenheiten): Kermit-Diskette `~/projects/robotron/P8000/discs/
kermit.cqm` (tar720, `ck*.c`).

## Festlegungen, die man nicht aufweichen darf

| # | Festlegung | Wächter |
|---|------------|---------|
| 1 | **QBIN-Vorgabe `&`, nicht `Y`.** C-Kermit 4.2 schaltet das 8-Bit-Präfix nur ein, wenn die Gegenseite ein Zeichen nennt (`spar()`: `Y` ohne eigene Parität ⇒ aus).  Die WEGA-Leitung tty0 **löscht Bit 7 auf dem Weg PC → WEGA** (gemessen, getty-Einstellung bleibt nach `login` stehen); WEGA → PC ist 8-Bit-sauber.  Mit `Y` scheitert also jedes Senden binärer Daten an WEGA. | `P8000WegaKermit.PruefsummenUnd8Bit`, `…PcClientGegenWegaServer` |
| 2 | **Blockprüfung 3 bleibt Vorgabe.**  C-Kermit 4.2 spiegelt 1/2/3 (`spar()`: `bctr = chklen`) — die Annahme aus K1 „kann nur 1" war falsch.  Typ 2 rechnet dort über `char`; Typ 1 und 3 sind davon rechnerisch unberührt (Nachbildung `KermitPaket.Ck42VorzeichenbehaftetesCharTrifftNurTyp2`).  Mit QBIN `&` stehen ohnehin nur 7-Bit-Zeichen im Paket. | `KermitPaket.Ck42…`, `P8000WegaKermit.PruefsummenUnd8Bit` |
| 3 | **Fristen sind adaptiv, nie ein Festwert in Wirtszeit.**  `Einstellungen::frist_ms` gilt in der Zeit der Gegenstelle; wirksam ist `frist_ms / min(Tempo, 1)`, mindestens 4 × geglättete Umlaufzeit, dieser Zuschlag höchstens bis TIME der Gegenstelle (in ihrer Zeit, also / Tempo), Grenzen `frist_min_ms`/`frist_max_ms`; **jedes empfangene Byte verlängert die laufende Frist**.  Tempo: Einstellung (`tempo`, `k1520k_set_tempo`, `--tempo`) oder Automatik aus der Zeichenrate (der Hub taktet die Gast-Baud in Maschinenzeit: Zeichenrate / (Baud/10) = Tempo). | `KermitTempo.*` |
| 4 | **Eine schnelle Gegenstelle bekommt keine kürzere Frist.**  Sie heilt mit ihrer eigenen (TIME in ihrer Zeit, also kurz in Wirtszeit); gleich lange Fristen auf beiden Seiten kreuzen ihre Wiederholungen (gemessen mit der virtuellen Uhr: 122 statt 0 Wiederholungen). | `KermitTempo.SchnelleGegenstelleHeiltSelbst` |
| 5 | **Login-Fristen sind Fristen der STILLE** (jedes Zeichen setzt sie neu), RETURN wird nur bei stiller Leitung wiederholt, nach `login:` wird die Stille abgewartet (sonst läse ein gestautes zweites `login:` als Ablehnung des Namens).  Ein ausdrückliches Tempo teilt alle Login-Zeiten.  Das Login misst die Zeichenrate und gibt sie als Startwert an die Sitzung (`LoginErgebnis::tempo` → `setzeTempoSchaetzung`; CLI und C-ABI tun das). | `KermitTempo.Login*` |
| 6 | **Wiederholungsregeln des Senders** (alle am echten C-Kermit gefunden): (a) eine Quittung für das VORIGE Paket heißt „deins kam kaputt an" — C-Kermit 4.2 schickt sie statt eines NAK → sofort wiederholen; (b) ein wiederholtes Paket der Gegenstelle (ihr R/G) heißt „mein S kam nicht an" → wiederholen; (c) **N(0) auf I/S/G/R wird NICHT beantwortet** — das ist der Leerlauf-NAK des C-Kermit-Servers (`tinit()` → `nack()` bei jedem Wiedereintritt), eine Wiederholung erzeugte ein doppeltes Y(0), das als Antwort auf das nächste Paket galt (G D lieferte die Init-Parameter); (d) N(n+1) ist für S/I KEINE Quittung (die Quittung trägt die Parameter). | `KermitSitzung.QuittungDesVorigenPaketsLoestWiederholungAus`, `…SenderWiederholtAufWiederholtesPaketDerGegenstelle`, `…LeerlaufNakDesServersWiederholtDasInitNicht`, `…NakAufSendInitIstKeineQuittung` |
| 6a | **Nachlauf:** hat der Client zuletzt ein B quittiert, wartet er vor dem nächsten I/R/G/S auf das Leerlauf-N(0) (höchstens `nachlauf_ms` = 2 s Zeit der Gegenstelle).  C-Kermit 4.2 leert nach jedem guten Paket seinen Eingang (`rpack` → `ttflui`) und verschluckte das sofort hinterhergeschickte Paket (gemessen: S erst nach 30 s Leerlauf-NAK wiederholt, 95 statt 350 B/s).  Gegen den eigenen Server (schickt kein N) kostet das 2 s je Befehl nach einem Empfang. | `P8000WegaKermit.PruefsummenUnd8Bit` (Wdh 0), `…PcClientGegenWegaServer` |
| 6b | **Alte Transaktion nach einer Trennung:** NAKt die Gegenstelle auf unser I ein Paket ≠ 0 (gemessen N25), steckt sie in der abgebrochenen Transaktion (ohne Eingriff > 120 s Maschinenzeit).  Abbruch mit einem E-Paket **in der Prüfungslänge ihres NAK** (ein E mit Prüfung 1 verwirft sie als beschädigt), dann I bei ihrem N(0). | `KermitSitzung.AlteTransaktionDerGegenstelleWirdAbgebrochen`, `P8000WegaKermit.VerbindungsabbruchUndWiederaufnahme` |
| 6c | **Der Nachlauf (6a) gilt nur gegen eine Gegenstelle mit 9 Init-Feldern** (`nachlaufNoetig_ = dein.felder <= 9`): CAPAS ⇒ kein C-Kermit 4.2 ⇒ kein Leerlauf-N, kein Warten.  Nicht auf „nie“/„immer“ vereinfachen. | `KermitSitzung.Nachlauf*` |
| 7 | **Der Server wiederholt die Schlussquittung**, wenn das B der eben beendeten Transaktion erneut kommt (auch mit Prüfung 2/3 — B wird wie N über seine Länge gelesen). | `KermitSitzung.ServerWiederholtQuittungDesB` |
| 8 | **`G C` ohne Pfad trägt ein leeres Feld** (`"C "`), nie gar keins: C-Kermit 4.2 `cwd()` liest die Feldlänge ungeprüft (`vdir[unchar(*vdir)+1] = 0` — ohne Feld ein Schreibzugriff vor den Puffer); mit Länge 0 wechselt es nach `$HOME`. | `KermitSitzung.LeererVerzeichniswechselTraegtLeeresFeld` |
| 9 | **WEGA-Benutzer `kermit` hat die Shell `/bin/sh`** und `/usr/kermit/.profile` = `exec /usr/bin/kermit`; `.kermrc` ohne `set file names literal`.  Grund: C-Kermit führt `G D/E/T/U/W` über die **Login-Shell aus `/etc/passwd`** aus (`zxcmd`: `execl(pw_shell, "-c", "ls -l …")`) — mit Shell `/usr/bin/kermit` kam als Verzeichnis die Gebrauchsanweisung von kermit; und `sfile()` kopiert bei literalen Namen ohne Sendenamen den **uninitialisierten** Paketnamen in den Dateinamen (`strcpy(filnam,pktnam)` statt umgekehrt) — jedes R endete mit „Can't open file". | `P8000WegaKermit.PcClientGegenWegaServer` (G D, R), `P8000WegaKermit.ServerHinterGetty` |

## Gemessen am echten C-Kermit 4.2 (K7) — und was nur angenommen ist

Siehe die Tabelle in Entwurf 36 §6.5.  **Gemessen** (Emulator, P8000 Vollgerät, tty0 als Telnet-Server
des Hubs bzw. tty3 als Telnet-Client): alles in den Wächtern oben.  **Nur aus den Quellen gelesen**
(nicht im Emulator ausgelöst): `G C` ohne Feld (Speicherfehler, nicht provoziert), C-Kermit-Verhalten
bei Parität ≠ none, Langpakete/Attribute (4.2 kennt sie nicht, nur 9 Init-Felder).

## Kermit-80 (Beigabe `KERMIT.COM` auf den Disketten) — Entwurf 36 §6.8

- **Nur Client.**  Es gibt kein CP/M-Kermit-Gegenstück zum Server der Bibliothek; Kermit-80 spricht gegen
  `Sitzung::serve` (PC-Server).
- **Eine Überlagerung je Rechner** (`tools/kermit80/kermit_{a5120,k8915,pc1715,pc1715w,prg710,p8000}.com`, PRG 710 und
  710-1 teilen sich eine; der 1715W zählt mit dem CTC im Zählerbetrieb 57H/ZK 13, der P8000 mit CTC1 K0 ZK 4).  Anschluss und Anfangsfolge stehen in `bau.py` (`RECHNER`) und kommen aus `tools/sertest/src/sertest.mac`
  — **ändert sich dort etwas, neu bauen** (`python3 tools/kermit80/bau.py <rechner> --out tools/kermit80/kermit_<rechner>.com`,
  `CPA_TOOLS` nötig) und die Disketten nachziehen.  Wächter `Kermit80Bau.EingechecktesComIstDasErgebnisVonBauPy`.
- **Der Zerleger erkennt Parität selbst** (81H als Paketbeginn oder Bit 7 in LEN/SEQ/TYPE) und streift danach Bit 7
  ab.  Nicht auf „immer abstreifen“ vereinfachen: ohne Parität sind rohe 8-Bit-Daten gültig.  `KermitParitaet.*`.
- **Kermit-80 hat keinen Anmeldeablauf.**  Gegen die WEGA meldet man sich im Terminalmodus (`connect`) von Hand an;
  zurück zum Prompt mit dem **Fluchtzeichen Ctrl-\** (nicht Ctrl-]) und `c`.  Die einfache Datensichtstation lässt den Cursor
  nach `connect` über den Bildrand laufen: der Prompt steht dann nicht mehr in der letzten Zeile und mit Resten des alten Bildes
  dahinter.  `Kermit80Wega` löscht deshalb das Bild vor jedem Befehl und wartet auf einen nackten, ruhigen Prompt.
- **Binärdateien über die WEGA-Leitung (tty0 löscht Bit 7) brauchen `set parity space|mark|even|odd`** an Kermit-80: nur dann
  nennt es das 8-Bit-Präfix, das C-Kermit 4.2 einschaltet.  Mit `parity none` kommt alles ≥ 80H verstümmelt an (`Kermit80Wega`).
- **Im Test läuft Kermit-80 als `SERTEST.COM`** (ausser am P8000: die ausgelieferte OS/M-Diskette `disks/p8000_osm30/` trägt `KERMIT.COM`, der Test mountet eine Temp-Kopie und startet `kermit`) (die Hilfen aus `sertest_hilfen.h` legen immer diesen Namen an) und
  wird mit `sertest` gestartet — nur eine Abkürzung; auf den ausgelieferten Disketten heißt es `KERMIT.COM`.
- Ein Lauf mit Parität ≠ none und QBIN `&` setzt voraus, dass der Server das Präfix annimmt (Kermit-80 schlägt bei Parität `Y` vor).

## Tests schreiben

- Gegenprobe im Emulator: `tests/system/test_p8000_wega_kermit_austausch.cpp` (Label `wega_install`,
  `tools/dev.sh test-wega`).  Startet aus einem **eigenen eingefrorenen Zustand** `kermit31_k7_vN`
  in `~/.cache/k1520emu/p8000_wega` (Plattenkopie + Testdateien per DiskTool in `/usr/kermit` +
  Kaltstart bis `login:`), den es beim ersten Lauf selbst baut (~60 s Rechenzeit); ein Stempel mit
  Größe/Zeit der ausgelieferten Platte erzwingt den Neubau.  Testdateien ändern ⇒ `kStand` hochzählen.
- **Die Maschine muss laufen, solange die Bibliothek spricht** (Hub taktet in Maschinenzeit): Fall 1
  Maschine im `Motor`-Faden, Bibliothek im Testfaden; Fall 2 umgekehrt (PC-Server im Faden, Konsole
  im Testfaden).  `Motor::start(tempo)` drosselt auf ein Tempo (1x = Echtzeit).
- Paketprotokoll sehen: `K1520_KERMIT_PAKETE=1` (Rückruf `Rueckrufe::paket`), Werkzeug
  `P8000WegaKermit.DISABLED_SendenEinzeln` (`--gtest_also_run_disabled_tests`).
- Unit-Tests mit virtueller Uhr: `SpeicherLeitungPaar::setzeZeichenzeit` bildet eine serielle Leitung
  nach (Bytes nacheinander); **ein Server-Faden muss sein Ende nach `serve()` schließen**, sonst steht
  die virtuelle Uhr (das Ende nimmt nicht mehr teil) und der Test wartet 30 s Echtzeit.

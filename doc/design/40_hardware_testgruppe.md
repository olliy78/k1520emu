# Entwurf 40 — Testgruppe „Echte Diskette am Greaseweazle"

Stand 2026-10-27.  Ergänzt `14_physische_diskette.md` §14 (dort: Testbarkeit *ohne* Hardware).
Dieser Entwurf regelt die Tests, die **nur mit angeschlossenem Greaseweazle, K5601 und einer
überschreibbaren Diskette** laufen.

## 1. Anlass

Die Fehler der Vergangenheit an der physischen Diskette (Index-Naht, Fremdsync,
`Flux Underflow`, „No Index" nach Pause, verwaister FORMAT-Strom, Leseausrutscher) hatten eines
gemeinsam: der Ersatzfaden über einer `.hfe` (`gw_fake.py`) liefert *perfekte* Bitzellen und
nimmt jedes Schreiben an.  Was die echte Hardware ausmacht — Drehzahl, Jitter, Motoranlauf,
Index, Schreibnaht, Prüf-Lesen gegen eine echte Scheibe — kommt dort nicht vor.  Die vorhandene
`test_gw_hardware.py` prüft Einzelschritte (Spur lesen, eine Datei schreiben), aber keine
**Anwenderketten**.

## 2. Ziel

Eine **eigene Testgruppe `tests/hardware/`**, die vollständige Anwenderketten gegen das echte
Laufwerk fährt, mit **einem Aufruf** automatisch durchläuft und **nie** Teil der normalen
Regression ist.

Nicht-Ziele: Hardware-Tests ersetzen die Ersatzlaufwerk-Tests nicht; sie prüfen nur, was diese
prinzipiell nicht sehen können.  Kein Test verlangt Handgriffe während des Laufs (Diskette
wechseln, Schreibschutz) — Fälle, die das bräuchten, stehen in §9 als manuell.

## 3. Aufbau und Abgrenzung

```
tests/hardware/
  conftest.py        Vorprüfung, Schutzriegel, Sicherung/Wiederherstellung, Fixtures
  hw_helfer.py       Maschinenfahrer (Tasten, Warten auf Text), Sitzungs-Fabrik, CLI-Aufruf
  test_h0_*.py … test_h6_*.py
  README.md          Bedienung
```

* **Nicht in ctest registriert.**  `tests/python/CMakeLists.txt` globbt nur `tests/python/test_*.py`;
  die Gruppe liegt daneben und wird dort nie eingesammelt.  Das alte `test_gw_hardware.py` bleibt
  vorerst (lesende Einzelschritte), seine Fälle werden in H1/H2 aufgenommen; danach wird es
  gelöscht.
* **Ein Aufruf:** `tools/dev.sh test-hardware [--schreiben] [-k AUSDRUCK] [--sichern DATEI]`.
  `dev.sh` baut zuerst (wie überall: nie ein veraltetes Binary), dann `pytest tests/hardware`.
  Ohne `--schreiben` laufen nur die lesenden Stufen (H0, H1).
* Marke `hw_lesen` / `hw_schreiben` je Test; die Stufe bestimmt, ob der Fall ohne `--schreiben`
  überhaupt gesammelt wird (er wird dann *abgewählt*, nicht übersprungen, damit „grün" nicht
  vortäuscht, es sei geschrieben worden).
* Steuerung über Umgebung wie bisher (`K1520_GW_DRIVE`, `_CYLS`, `_HEADS`, `_RATE`), vorbelegt für
  K5601: Laufwerk `a`, 80 Zylinder, 2 Köpfe, 250 kbit/s.

## 4. Schutz der Diskette

Die Diskette im Laufwerk **wird überschrieben** — aber nur die, die der Anwender dafür
freigegeben hat.

1. **Freigabe ist ausdrücklich:** Schreibstufen laufen nur mit `--schreiben`
   (`K1520_GW_WRITE=1`).  Ohne Freigabe wird *vor dem ersten Motoranlauf* abgelehnt.
2. **Sicherung vorab** (Vorgabe an, `--keine-sicherung` schaltet ab): die eingelegte Diskette
   wird mit `gw read` in `out/hw_sicherung/<zeit>.hfe` gesichert (~2,5 min).
3. **Wiederherstellung am Ende** (`--zurueckspielen`, Vorgabe aus): die Sicherung wird mit
   `gw write` zurückgeschrieben.  Ob der Test eine Opferdiskette hinterlässt, ist damit eine
   Entscheidung des Aufrufers, nicht des Tests.
4. **Alle Dateioperationen nur in `out/hw_*` und im Scratchpad.**  Nie in `disks/`.
   Boot-Quellen sind Kopien (`TempDisk`-Regel, CLAUDE.md).

## 5. Vorprüfung (Stufe H0, läuft immer)

Schlägt sie fehl, **überspringt** die Gruppe den Rest mit dem Grund (nicht rot — ein fehlender
Adapter ist kein Defekt des Emulators):

| Prüfung | Maßstab |
|---|---|
| Paket `greaseweazle` importierbar, Adapter gefunden | `finde_adapter()` |
| Laufwerk antwortet, Motor läuft, Index kommt | `read_track(0,0)` liefert Zellen |
| Drehzahl | Umlaufzeit 200 ms ± 3 % (300 U/min) |
| Zellzahl je Spur | ≈ 100 000 Zellen bei 250 kbit/s DD |
| Diskette liegt ein | Spur liefert Flux; sonst „keine Diskette" |
| Schreibschutz | nur bei `--schreiben`: Probeschreiben einer Spur und Rücklesen; sonst Abbruch „Schreibschutzkerbe prüfen" |

## 6. Testfälle

Jede Stufe setzt auf der vorigen auf; schlägt eine Grundstufe fehl, entfallen die davon
abhängigen (`pytest.skip` mit Verweis), damit ein Defekt in H2 nicht fünf Folgefehler erzeugt.

### H1 — Lesepfad (lesend)

* H1.1 Dieselbe Spur dreimal lesen → Zellzahlen innerhalb 1 ‰.
* H1.2 Ganze Diskette über `Sync` einlesen; Zeit, Zahl `read_crc_bad` (Soll 0 auf heiler
  Diskette), `tracks_failed == 0`.
* H1.3 Zweimal einlesen → je Sektor gleiche Nutzdaten (Jitter darf nicht in Inhalte wandern).

### H2 — Schreibpfad (schreibend, Gerätenähe)

* H2.1 Eine Spur schreiben und zurücklesen (`Flux Underflow`-Wächter: Drehzahlstreckung).
* H2.2 Alle 160 Spuren mit Prüfmuster beschreiben (Sektorinhalt = Funktion von Spur/Sektor),
  `verifies_done == writes_done`, `tracks_defect == 0`; frisch zurücklesen, Muster bitgleich.
* H2.3 Schreiben nach langer Pause (Motor aus, 10 s warten) → kein „No Index" (Issue #13).
* H2.4 Index-Naht: Schnitt der geschriebenen Spur liegt vor einem Sektorkopf
  (`naht_vor_sektorkopf`), kein zerhackter Sektor.

### H3 — Emulator bootet (A5120)

* H3.1 **Boot vom Image** (Bezug): CP/A-Fixture als Datei in A: → `A>`.  Zeigt, dass der
  Treiber der Maschine heil ist, und liefert Zeitmaß.
* H3.2 **Image → echte Diskette → Boot physisch:** dieselbe Fixture mit `gw write` auf die
  Scheibe, Emulator mit `PhysicalSession(for_emulator=True)` als A: → `A>`, dabei weniger als die
  halbe Diskette gelesen (wie `PhysicalBoot.*`).
* H3.3 Dasselbe für UDOS 4.3 (Fixture `a5120_udos43`) — anderer Lesepfad (Fremdsync/Zeigerkette).

### H4 — Formatieren und Systeminstallation auf der echten Diskette

* H4.1 Leere Diskette (alle Spuren gelöscht, `gw erase`) → Emulator mit CP/A-Bootdiskette als
  A: und echter Diskette als B: (schreibend) → `FORMAT` mit Verify, alle Spuren → keine Fehlermeldung
  des Gastes.
* H4.2 Nach dem Abmelden (`flush`) **Vollmessung der Scheibe**: alle Sektoren beider ID- und
  Daten-CRCs heil (das Verify des Gastes sieht die Scheibe *nicht*, vgl. Merkposten).
* H4.3 `CPABCGEN B:` → Systemspuren auf die echte Diskette → `DIR B:`.
* H4.4 **Kaltstart von der selbst formatierten Diskette** in A: (neue Sitzung, frisch gelesen)
  → `A>`.  Das ist die Anwenderkette der Fehlergeschichte.
* H4.5 Wiederholung mit UDOS (`FORMAT`/Systemgenerierung nach Matrix `UdosLaufwerkstypen`) —
  erst nach grünem H4.1–4.4, weil UDOS die Gap-Verkettung braucht.

### H5 — DiskTool live (nicht als Image)

* H5.1 Diskette der Kette H4.4 mit `k1520disktool --physical` öffnen: `info` erkennt Format und
  Dateisystem, `check` (fsck) ohne Befund.
* H5.2 Ändern: `put` mehrere Dateien (klein, 1 Extent, > 16 KiB), `rm`, Umbenennen, überschreiben.
  Danach `--write`-Sitzung ordentlich schließen.
* H5.3 **Frisch neu einhängen** (neuer Prozess, nichts im Speicher): `ls` stimmt, `get` liefert
  die Dateien byteweise, `check` ohne Befund.
* H5.4 Genau die geänderten Spuren sind auf der Scheibe geändert (Vollmessung vorher/nachher,
  wie Merkposten §15.1) — kein Nebenschaden.
* H5.5 Dieselbe Kette über die Oberflächen-API (`DiskTool.open_physical`) statt CLI.

### H6 — Rundlauf Emulator ↔ DiskTool

* H6.1 DiskTool schreibt Datei → Emulator (physisch gemountet) liest sie im Gast (`TYPE`/`DIR`)
  und prüft Inhalt.
* H6.2 Gast schreibt Datei (`PIP`/`SAVE`) → DiskTool liest sie frisch.
* H6.3 Wechselspiel wiederholt, danach `check` ohne Befund.

### H7 — Betriebsarten (einseitig, Doppelschritt)

Siehe §12.

### H8 — Tempo des Gastsystems

Gast und Adapter sind durch das Speicherabbild entkoppelt, aber an zwei Stellen nicht:

* **Eine unbekannte Spur blockiert die Maschine** (`TrackSync::ensureLoaded`), bis der Adapter
  sie gelesen hat; die Maschinenzeit steht dabei.  Eine geänderte (`Dirty`) Spur blockiert
  **nicht** — auch nicht, solange ihr Prüf-Lesen aussteht; der Gast liest sie aus dem Abbild.
  Beim FORMAT mit Vergleichs-Lesen blockiert deshalb nicht das Verify, sondern das Vorlesen der
  Kopf-1-Spur vor jedem Zylinder (Entwurf 14 §14.1).
* **Geänderte Spuren laufen auf**, wenn der Gast schneller formatiert, als der Adapter
  schreiben und prüf-lesen kann (≈ 1 s je Spur).  Gemessen unbegrenzt: am Ende von FORMAT
  noch 79–105 Spuren ausstehend.

Der Fahrer drosselt wie `app/emulationstakt.py` (Nenntakt × Faktor gegen die Wanduhr,
Rückstand höchstens 0,1 s nachgeholt).

* H8.1 FORMAT mit Verify bis Spur 19 in **1×, 10×, unbegrenzt**: Gast meldet keinen Fehler,
  `errors == 0` (u. a. keine Zeitüberschreitung einer blockierenden Anforderung), kein
  misslungener Vergleich, Scheibe je Spur wie formatiert.
* H8.2 **Auswerfen wie die Oberfläche** nach FORMAT im unbegrenzten Tempo:
  `PhysicalSession.close()` ohne vorheriges `flush` — die ganze Scheibe muss danach formatiert
  sein.  Anlass: `close()` wartete höchstens 30 s und verwarf den Rest still (§11).

### H9 — k1520DiskTool live an der echten Diskette: CP/A, UDOS, WEGA

Geprüft wird das **DiskTool**, nicht der Emulator.  Je Dateisystem wird ein Abbild aufgespielt
(`cpa_cpa780_k5601_clock.hfe`, `disks/a5120_udos43/…entwickler.hfe` = `udos_ds77` mit
Verkettung im Gap hinter der Daten-CRC, `disks/p8000_wega30/…doc1.dmk` = `wega720`), danach
arbeitet das DiskTool **live** (`open_physical`).

**Orakel ist das DiskTool an einer Dateikopie desselben Abbilds**: dieselben Schritte
(`tests/hardware/disktool_szenario.py`) an Datei und Laufwerk müssen dasselbe ergeben —
Format, Dateisystem, Verzeichnis (Größe/Typ/Eigenschaften/Nutzer), alle Dateibytes, der Befund
der vollen Prüfung und **jeder Sektor samt Anhang hinter der Daten-CRC**.

| Fall | Inhalt |
|---|---|
| H9.1 | live geöffnet == Datei (vor der Änderung) |
| H9.2 | ändern: einfügen klein/groß (CP/A > 16 KiB = 3 Extents, WEGA > 10 Blöcke = indirekt, UDOS 20 KiB Kette), überschreiben mit anderer Länge, löschen, Eigenschaften (`set_cpm/udos/unix_attrs`), bei WEGA Verzeichnisse; Rückführung vollständig, jede Spur geprüft, keine Schadstelle, **genau die geänderten Spuren** geschrieben |
| H9.3 | frisch neu eingehängt (neue Sitzung, nichts im Speicher) == Datei-Orakel danach |
| H9.4 | unabhängige Gegenprobe: `gw` dekodiert je Spur dieselben Sektoren wie im Orakel-Abbild |
| H9.5 | schreibgeschützt geöffnet: Änderung abgewiesen, keine Spur geschrieben |
| H9.6 | zu große Datei (1 MiB): abgewiesen, keine Spur geschrieben (Transaktion) |
| H9.7 | Kommandozeile `--physical`: `put`/`ls`/`get`/`rm`/`check` |

**Am Gerät (2026-10-28): 18/18 grün** in 34 min.  Geschrieben wurden genau die Spuren, die
sich laut Orakel ändern — CP/A 11, UDOS 11, WEGA 24 —, jede geprüft, kein misslungener
Vergleich, keine Lesewiederholung.

Dieselben Szenarien laufen **ohne Hardware** in der Regression
(`py_disktool_physisch_gegenprobe`, Ersatzlaufwerk über einer Kopie, 9 Fälle, ~3 s).

Zwei Festlegungen dafür:
* **Feste Uhr:** WEGA stempelt Superblock und Verzeichnis-Inodes mit „jetzt“; ohne festen Wert
  unterscheiden sich Orakel und Laufwerk um die Sekunden zwischen den Läufen (gemessen: 4
  Sektoren).  `K1520_FS_UHR=<Unix-Sekunden>` setzt die Uhr des WEGA-Dateisystems fest (nur wenn
  keine eingesetzt ist); das Szenario setzt sie, Quelldateien bekommen dieselbe mtime.
* **Nicht `udos_boot_scp.hfe`:** dem Mitschnitt fehlt Sektor 13 auf Spur 51/0, und genau dorthin
  legt der Belegungsplan die erste neue Datei — das DiskTool verweigert dann zu Recht die ganze
  Operation (Transaktion), schon an der Datei.

### Weitere Maschinen (Ausbau)

Die Fahrer sind maschinenneutral (Profil: Bootdiskette, Tastenfolge, Prompt); als nächste
Matrixzeilen vorgesehen: A5130 (Profil), K8915 (`/WAIT`-Zweig — nur wenn das Laufwerk dort
passt), PRG 710, PC 1715.  Jede Zeile = ein Profileintrag, kein neuer Testcode.

## 7. Maschinenfahrer

Die Python-Bindung genügt, kein Qt: `K1520Emulator` + `PhysicalSession` + `mount_physical`
(Muster `tests/python/test_k8915_physical.py`).  Tasten über `key_press(ord(c))` mit
300 000-Takt-Abstand (wie `serial_gast.py`), Warten über `run_until_text`.  Der Fahrer
schreibt nach jedem Schritt den Bildschirm in `out/hw_log/<test>.txt` — bei einem Fehlschlag
ist das die einzige Spur, und die Hardware lässt sich nicht zurückspulen.

## 8. Auswertung und Aufbewahrung

* Jeder Lauf schreibt `out/hw_log/<zeit>/bericht.txt`: Vorprüfung, Zeiten je Stufe,
  `Stats` der Sitzungen (writes/verifies/defect/crc_bad).
* Vor und nach jedem schreibenden Fall wird (sofern teuer genug) eine Vollmessung gemacht;
  Messgröße: Zahl der Sektoren mit falscher ID-/Daten-CRC.  Soll: 0.
* Laufzeitbudget: H0–H1 ≈ 5 min, H2 ≈ 15 min, H3–H4 ≈ 20 min, H5–H6 ≈ 15 min.  Vollständig
  im Hintergrund fahren (`run_in_background`), nie pollen.

## 9. Bewusst nicht automatisch

* Diskette entnehmen / wechseln während der Sitzung (Fehlermeldungen „keine Diskette").
* Schreibschutzkerbe (Verhalten bei geschütztem Medium).
* Mehr als ein Laufwerk am Kabel (§16 von Entwurf 14).

## 10. Offene Punkte

* Ob `gw erase` vor H4.1 nötig ist oder FORMAT über den vorhandenen Inhalt ausreicht, wird am
  Gerät gemessen (§11 hält Messungen fest).
* Zeitgrenzen (Spur lesen < 2 s) sind an einem F1/K5601 gemessen; andere Adapter brauchen
  ggf. `K1520_TEST_ZEIT_FAKTOR`.

## 11. Messungen am Gerät (Stand der Umsetzung)

Gemessen 2026-10-27 an Greaseweazle F1 (Firmware 1.6) + K5601, Laufwerk `a`:

* Umlauf 200 ms, ≈ 100 000 Zellen je Spur; Spur lesen 0,5–0,6 s (erster Zugriff 2 s: Motoranlauf).
* **`gw write` ist als Aufspieler untauglich:** schreibt die HFE-Spur länger als eine Umdrehung
  (220 ms) über die Index-Naht; Spur 0 der CP/A-Fixture hatte danach 23 von 26 Sektoren, und der
  Emulator blieb im Bootloader hängen (1 Sektor mit falscher CRC). `Device.write_track` liefert
  26/26. Die Gruppe spielt Abbilder deshalb mit dem Projekt-Schreibpfad auf (`schreibe_abbild`,
  66 s für 160 Spuren).
* **Sporadischer Schreibausrutscher:** beim ersten Komplettaufspielen war genau 1 von 160 Spuren
  (Zylinder 51) fehlerhaft (gw: 3/5 Sektoren, unser Dekoder: 1 CRC-Fehler); nach erneutem Schreiben
  5/5. Der Produktpfad fängt das per Prüf-Lesen ab; `schreibe_abbild` macht deshalb eine
  Nachkontrolle mit dem `gw`-Dekoder (bis zu 2 Wiederholungen).
* **Unabhängiges Orakel:** `gw read --format ibm.scan` zählt Sektoren je Spur
  (`T<c>.<h>: IBM MFM (g/e sectors)`), `gw convert` dasselbe für Abbilder — damit sind Abbild- und
  Plattenbefund vergleichbar.
* Kaltstart CP/A von der aufgespielten Diskette bis `A>`: 10 von 160 Spuren gelesen, 0 CRC-Fehler.
* Format-Tastenfolge (`format_und_system`) ist am Abbild für 5 Stichproben (cpa780, 16×256,
  S/U/V) bestätigt; 47 K5601-Zeilen kommen aus `format_all.format_matrix()`.

* **Firmware gibt das Laufwerk nach ≈ 10 s Ruhe frei** („Seek: No drive unit selected"); `Device`
  hielt sich weiter für gewählt, der erste Auftrag nach einer Pause des Gastes scheiterte.
  Behoben in `app/gw/device.py` (`_mit_indexwiederholung` wählt neu), Wächter
  `test_gw_motor.py::test_vom_adapter_freigegebene_auswahl_wird_neu_eingestellt`.
* **CP/A-FORMAT blieb auf einer gelöschten echten Diskette stehen** (Spur 44, 44, 89, 87, 80 in
  verschiedenen Läufen; Maschine lief weiter, kein Fortschritt).  **Ursache im Kern, behoben:**
  - Eine gelöschte Spur liefert am echten Laufwerk *Rauschen*.  Im Medium ist sie damit eine Spur
    **mit Bytes, aber ohne Sektor** — eine Leerdiskette aus einer Datei hat dagegen *leere* Spuren.
  - `K5122::startReadTransfer` erkannte „unformatiert" nur an `empty()`.  Die Rauschspur lief in
    den Zweig für formatierte Spuren: Lesestrom aus null Sektoren, aber `stream_has_marks_ = true`.
    Die ZVE2 blieb in ihrer Kopfsuche (`E4C7–E4DD`) und hielt den Bus; der Index-Timeout lebt in
    der Timer-ISR der ZVE1 (`E600`), die nie mehr drankam.  PC-Histogramm im Stillstand: ZVE1
    400/400 bei `E60B`, ZVE2 gleichverteilt über die fünf Befehle der Schleife.
  - Behebung: unformatiert = **kein Sektor**, nicht erst „keine Bytes"; Wächter
    `K5122Test.RauschspurOhneSektorGibtDenBusFrei`.
  - Dazu `TrackSync`: eine gelesene Spur **ohne einen einzigen heilen Sektorkopf** ist Rauschen —
    der Dekoder fand darin Scheinsektoren (FM, Zyl. 123 / Kopf 157, ID-CRC falsch), die jede
    gelöschte Spur dreimal lesen ließen (`read_retries` 14–26 je FORMAT).  Marken werden
    verworfen, nicht wiederholt; Wächter `TrackSync.EineSpurOhneHeilenSektorkopfGiltAlsUnformatiert`.
  - **Nachweis ohne Gerät:** drei Rauschsätze der gelöschten Diskette aufgenommen
    (`Device.read_track`, 3 × 160 Spuren) und über ein Wiedergabe-Laufwerk eingespielt: vorher
    hingen 2 von 3 (bzw. nach der TrackSync-Regel allein immer noch 2 von 3), nach der
    K5122-Behebung 3 von 3 durchformatiert.
  - **Mit dem Tempo hat es nichts zu tun** — die Wiedergabe lief ohne Verzögerung und ohne
    Rückstand.  Unbegrenzt traf es nur häufiger, weil dort mehr Läufe stattfanden.
  - Werkzeug dafür: Stillstandserkennung im Fahrer (400 M Takte ohne neue Spur) mit
    PC-Histogramm beider CPUs und RAM-Abzug — über die neue, additive C-ABI
    `k1520_debug_cpu` (PC/SP von ZVE1/ZVE2; ein Save-State geht am A5120 über die ABI nicht).
* **Orakel je Spur, nicht als Summe:** `gw` erwartet auf einer unformatierten Spur keine Sektoren —
  eine ganz verlorene Spur fiele in „gefunden == erwartet" nicht auf.  Verglichen wird deshalb je
  Spur gegen ein Soll (`spurbefund`).
* **Rückstand beim Abmelden — Datenverlust, behoben:** `PhysicalSession.close(timeout=30)`
  ignorierte ein gescheitertes `flush` und beendete den Arbeitsfaden.  Nach FORMAT im
  unbegrenzten Tempo stehen 79–105 Spuren aus (≈ 1 s je Spur).  Weg der Oberfläche:
  Laufwerkskasten ▸ Auswerfen (`_stop_physical`), ebenso DiskTool und `--physical`.  Der Wächter
  `py_gw_physical::test_abmelden_wartet_den_ganzen_rueckstand_ab` zeigt mit dem alten Code
  **19 von 160** zurückgeschriebenen Spuren.  Jetzt ist die Frist eine Frist *ohne Fortschritt*,
  `close()` liefert `False` samt `nicht_zurueckgeschrieben`, und alle drei Bedienwege melden einen
  Rest (Schadstellen weiter über ihren eigenen Weg).
* **Sporadisch: c0h0 mit 37/42 Sektoren** nach FORMAT unbegrenzt (H8.1), in 2 Wiederholungen
  nicht mehr.  Offen; der Test fängt es, wenn es wiederkommt.
* **CP/M füllt auf 128-Byte-Sätze auf** — ein Bytevergleich nach `get` braucht Größen als
  Vielfache von 128 (Testfehler, kein Produktfehler).

* **Stand der Läufe (2026-10-27/28, nach den Behebungen):** Format-Matrix H4m **47/47** (24 min;
  alle K5601-Menüs + Geometrien S/T/U/V/W, Smoke bis Spur 2, Sektorzahl je Spur auf der Scheibe
  gleich der des Abbilds); H2–H8 grün, H4-Kette dreimal hintereinander grün, H8.1 in 1×/10×/
  unbegrenzt grün (Rückstand am FORMAT-Ende 0 / 9 / 15 Spuren), H8.2 Auswerfen mit 119 Spuren
  Rückstand: 119 s, nichts verloren.  H1 wird übersprungen, wenn die Diskette nicht durchgehend
  formatiert ist (nach einem Schreiblauf der Regelfall) — für H1 eine formatierte Diskette
  einlegen oder H2 vorweg fahren.

## 12. Betriebsarten K5600.10/.20-Kompatibilität

Das K5601 bedient 40-Spur-Disketten im Doppelschritt und einseitig. Abgedeckt auf zwei Wegen:
(a) der Gast schaltet die Geometrie (Matrix S/T/U/V/W in H4m), (b) die Sitzung (`double_step`,
H7). Offen: Sitzung mit 40 Zylindern + `double_step` im Emulator/DiskTool durchgängig (Boot einer
40-Spur-Diskette physisch).

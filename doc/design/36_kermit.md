# 36 — Kermit: Datenaustausch Host-PC ↔ WEGA (P8000)

Stand 2026-10-20 (AP K9), Zweig `kermit` (von `P8000`): **K0–K9 fertig** (K9: Paket lokal belegt, Inno/MSVC nur
in der CI), offen nur das optionale K2b; offene Einzelpunkte in §7.  Zwei Teile: **(A)** Kermit auf der WEGA-3.1-Platte
einrichten (Server hinter `getty`, Client in der Shell), **(B)** ein eigenes Kermit für den PC
(C++-Bibliothek + Kommandozeile + Python/Qt-Oberfläche), Client **und** Server.
Merkposten (seit AP K7): `doc/merkposten/kermit.md`.

## 1. Befund (2026-10-18, gemessen)

- **Kermit-Diskette** (`~/projects/robotron/P8000/discs/kermit.cqm`, tar720; Archiv `…/archive/kermit.zip`):
  C-Kermit **4.2(030)** — Quellen (`ck*.c`, `makefile`, Handbuch `ckermi.doc/.man/.nr`) **und ein fertiges
  Z8000-Binary `kermit`** (78 560 B, Stand 1988-02-04, setuid root, a.out-Kennung `E711` = 0411, `/bin/ls` hat
  `E707`).  `kermit4.cqm` enthält nur `src.030.tar.z`/`src.070.tar.z` (Quellen 4.2(030)/(070), compress-gepackt).
- **Auf der Platte** `disks/p8000_wega31/p8000_wega31_platte.avr.img.gz` ist Kermit **nicht** installiert
  (`md0` = `/usr`, 537 KB frei; `md2` = `/`, 379 KB frei).  `cu` und `uucp` sind vorhanden.
- **Anmeldeweg:** `/etc/inittab` startet `/etc/GETTY` auf tty0, tty2, tty4–7 (Zustand 2); `/etc/passwd` kennt
  schon einen Benutzer mit Programm statt Shell: `uucp:…:/usr/lib/uucp/uucico`.  Dasselbe Muster trägt einen
  Benutzer `kermit` mit Shell `/usr/bin/kermit`.  Ein Anmeldeprogramm kann `kermit` keine Argumente mitgeben;
  C-Kermit liest aber beim Start `~/.kermrc` (HOME aus `passwd`) — dort steht `server` (danach `exit`).
  **Das ist zu prüfen** (§3, K1); Rückfall: ein winziges C-Programm `kermitd`
  (`execl("/usr/bin/kermit","kermit","-x",0)`), das auf WEGA mit `cc` gebaut wird.
- **Leitungen:** tty0/tty2 hängen über die Koppelsoftware am `SerialHub` (Telnet/RFC 2217), tty3 ist am
  P8000 die Druckerleitung (kein getty), tty1 = Konsole am Originalterminal.  9600 Bd; getty-Typ 2.
  Die Terminal-Firmware löscht Bit 7 beim Empfang — **8-Bit-Durchgang ist nicht zugesichert**, also muss
  die Protokollseite 8-Bit-Präfix (QBIN) beherrschen (Kermit-Standard, C-Kermit 4.2 tut es).
- **Wiederverwendbar:** `core/serial/telnet_codec.*`, `rfc2217_codec.*` (reine Codecs, kein Socket),
  `core/serial/net/{socket,adresse}.*` (Sockets POSIX/Winsock, Portsuche).  **Nicht** wiederverwendbar:
  `Wandler`/`SerialHub` (zeitlich an Maschinentakte gebunden, Gastseite); die Kermit-Bibliothek braucht
  eigene blockierende Leitungsklassen darüber.
- **Referenz-Gegenstelle:** ein Host-C-Kermit ist nicht installiert (`which kermit` leer).  Die Gegenprobe ist
  das **C-Kermit 4.2 im Emulator** — beide Richtungen.

## 2. Zielbild

```
 Host-PC                                              Emulator (p8000emu)
 ┌──────────────────────────────┐                     ┌──────────────────────┐
 │ GUI (PySide6)   CLI          │   Telnet / RFC 2217 │ SerialHub: tty0/2/3  │
 │   └── libk1520kermit ────────┼─────────────────────┼─► Koppelsoftware     │
 │       Protokoll · Leitung    │   (Client ODER      │      └► WEGA: getty    │
 │       Client · Server        │    Server)          │          login kermit  │
 └──────────────────────────────┘                     │          kermit -x     │
                                                      └──────────────────────┘
 Fall 1: WEGA = Server hinter getty   → PC = Client, meldet sich als „kermit" an (Login-Skript)
 Fall 2: WEGA = Client (Shell: kermit) → PC = Server, gibt einen Ordner frei (tty3 → Telnet-Client)
```

## 3. Teil A — Kermit auf WEGA 3.1

| Punkt | Entscheidung |
|---|---|
| Binary | zuerst das mitgelieferte (`kermit`) — Probe im Emulator (startet es, `?`-Hilfe, `show`). Läuft es nicht: aus den Quellen mit `make sys3` auf WEGA bauen (`cc` vorhanden), sonst Quellenstand 070 versuchen. |
| Ort | `/usr/bin/kermit` (md0, PATH enthält `/usr/bin`), Rechte `4755 root` wie auf der Diskette (Sperrdateien); Handbuch `ckermi.man` → `/usr/man/man1/kermit.1`; Quellen nicht auf die Platte (Platz). |
| Server-Benutzer | `kermit` in `/etc/passwd`: uid ≠ 0 bevorzugt (Prüfen: Kermit braucht root nur für Sperrdateien; uid 7 `kermit`, gid 1), Kennwort **`kermit`** (Stand K1: ein leeres Kennwort geht nicht — System-III-`login` verlangt dann „Choose one“; das Login-Skript sendet Name und Kennwort; getty meldet sich nach der Sitzung mit gerader Parität, Bit 7 vor dem Vergleich löschen), Home `/usr/kermit`, Shell **`/bin/sh`** mit `.profile` = `exec /usr/bin/kermit` (K7: C-Kermit führt `G D/E/T` über die Login-Shell aus — mit Shell `/usr/bin/kermit` kam die Gebrauchsanweisung statt `ls -l`). |
| Start | `/usr/kermit/.kermrc`: `set file type binary`, `server`, `exit` (K7: **kein** `set file names literal` — damit gibt C-Kermit 4.2 keine Datei her, §6.5). |
| Client am WEGA | Anmelden wie gewohnt (Konsole/tty0/tty2), `kermit`, `set line /dev/tty3`, `set speed 9600`, `set file type binary`, `send`/`get`/`remote dir` gegen den PC-Server — als Text in `README`/Handbuch `Kermit` beschreiben, kein Gastcode. |
| Platte | `disks/p8000_wega31/p8000_wega31_platte.avr.img.gz` **einmal** neu packen, nachdem K1 grün ist (jede Fassung ≈ 6 MB in der Git-Historie); `disks/p8000_wega31/README.md` und `doc/p8000/` ergänzen.  Das Paket zieht die Datei automatisch mit. |
| Einspielen | über das DiskTool (`k1520disktool put <abbild> --as md0/bin/kermit`, `attr` für Rechte/Besitzer; Platte ist schreibgeschützt bis `--force`/Freigabe, vorher `<abbild>~`-Sicherung), **nicht** über einen Emulatorlauf — reproduzierbar und billig. Anschließend Probe am laufenden WEGA (Test, s. K1/K7). |

## 4. Teil B — PC-Seite

### 4.1 Aufbau

```
kermit/                       eigene Bibliothek (NICHT core/, NICHT libk1520disk)
  include/k1520kermit.h       C-ABI  k1520k_*   (K1520K_API-Exportmakro, MSVC-tauglich)
  src/
    paket.{h,cpp}             Paketschicht: Kodierung, Blockprüfung 1/2/3, Wiederholungs-/Steuerzeichen-/8-Bit-Präfix,
                              Lang­pakete (Extended), Attribute-Pakete, (später Schiebefenster)
    protokoll.{h,cpp}         Zustandsautomaten: Senden (S/F/A/D/Z/B), Empfangen (R/F/A/D/Z), Server-Schleife, Fehler/Abbruch
    server.{h,cpp}            Serverbefehle (I, S, R, G-Familie: C/D/E/F/L/H/T, K), Freigabewurzel
    dateien.{h,cpp}           Ordnerfreigabe: Wurzel, Pfadprüfung (kein Ausbrechen), Text/Binär, Namensumsetzung
    leitung.{h,cpp}           abstrakte `Leitung` (lesen mit Frist, schreiben, schliessen, Abbruch)
    leitung_netz.{h,cpp}      TelnetLeitung/Rfc2217Leitung, je Client (verbinden) und Server (lauschen) —
                              über k1520_serial_codec + k1520_serial_net
    loginskript.{h,cpp}       erwarten/senden („login:" → kermit) vor dem Protokollstart
    api.cpp                   C-ABI
kermit/cli/k1520kermit.cpp    Kommandozeile
app/kermittool/               PySide6-Oberfläche (an app/disktool/ ausgerichtet) + Bindung app/kermittool/binding.py
```

Abhängigkeiten der Bibliothek: nur `k1520_serial_codec`, `k1520_serial_net` (statisch) und die Standardbibliothek —
**kein** Z80, keine Karten, kein `k1520_fs`.  Das Gegenstück zum Muster `libk1520disk`: **`libk1520kermit.so`/`.dll`**.
Statt der Linux-Kern-Bibliothek einen eigenen Namensraum `k1520::kermit`.

### 4.2 Protokollumfang (Kermit Protocol Manual, 6. Fassung)

Pflicht (Stufe 1): Grundprotokoll mit Blockprüfung **1, 2 (12 Bit), 3 (CRC-16)**, Aushandlung in S/Y,
Steuerzeichen-Quotierung, **8-Bit-Präfix (QBIN)** mit Aushandlung `Y`/`N`/`&`, **Wiederholungszähler (REPT, `~`)**,
**Langpakete** (CAPAS/MAXL), **Attribute-Pakete** (Dateigröße, Datum, Typ), Fehlerpakete, `Z`/`B`-Abbruch
(Datei/Gruppe), Zeitüberschreitung + Wiederholung, Text-/Binärmodus (Text: Zeilenende LF ↔ CR LF auf der Leitung,
`^Z`-Behandlung aus), Dateinamen **literal** oder **konvertiert** (Großschrift/Punkt, wie C-Kermit).
Server (Stufe 1): `I`-Init, `S`-Empfang, `R`-Senden, `G C` (cwd), `G D` (Verzeichnis), `G E` (löschen), `G F`/`G L`
(Finish/Logout), `G H` (Hilfe), `K`; **Host-Befehl (`C`) wird abgewiesen** — der Server ist eine Ordnerfreigabe.
Stufe 2 (nach Stufe 1 grün, optional): **Schiebefenster**, `G T` (Typ), `G U` (Speicher), Wiederaufnahme.
Nicht geplant: `SET LINE` auf echte serielle Ports (Telnet/RFC 2217 deckt den Emulator ab; ein späteres `device`
wäre eine weitere `Leitung`).

### 4.3 Leitung und Transport

- `Leitung` ist **blockierend mit Frist** (`lesen(buf, n, ms)` → Anzahl / 0 Frist / −1 getrennt) und hat einen
  Abbruchschalter (für die Oberfläche).  Kein Netz in der Protokollschicht — sie bekommt eine `Leitung`
  (Attrappe im Test: Speicher-Paar mit Verzögerung/Verlust/Verfälschung).
- Netz: **Telnet** und **RFC 2217**, je **Client** (zum Hub-Kanal des Emulators verbinden) und **Server**
  (lauschen; der Emulator verbindet als Client).  Die Codecs sind die des Emulators; die Rolle der Leitung
  (verbinden/lauschen) ist **unabhängig** von der Rolle im Kermit-Protokoll (Client/Server).
- **Telnet ist 8-Bit-binär zu behandeln** (kein CR→CR-NUL, kein Echo, `IAC` verdoppeln) — gegen den Hub des Emulators
  zu prüfen (K3).  RFC 2217: Baud/Format als Wunsch senden; der Gast ist maßgeblich (Merkposten Seriell).
- **Maschinenzeit statt Uhr:** der Emulator-Hub taktet mit der Gast-Baudrate; bei 10× Takt ist er schneller als der
  Echtzeit-Baud — Fristen der Bibliothek deshalb großzügig (Vorgabe 10 s je Paket, einstellbar), gemessen am echten
  Betrieb in K7.
- **Login-Skript** (`loginskript`): vor dem Protokollstart RETURN senden, bis „login:" erscheint (getty „hatte ins
  Leere gesendet"), Name (+ Kennwort) senden, auf das Verstummen/den Server-Beginn warten; Fehlschlag = klare
  Meldung mit dem zuletzt Gelesenen.

### 4.4 C-ABI (Auszug)

`k1520k_open_telnet_client/…_server`, `…_rfc2217_…`, `k1520k_login(h, name, kennwort, frist)`,
`k1520k_set_*` (Prüfsumme, Paketlänge, Fenster, Namen, Textmodus, Fristen), `k1520k_send(h, lokal, entfernt)`,
`k1520k_get(h, entfernt, zielordner)`, `k1520k_remote_dir/cd/delete/finish`, `k1520k_serve(h, wurzel)` (blockierend,
mit Abbruch), Rückrufe `fortschritt(datei, bytes, gesamt)`, `ereignis(text)`; Fehler als Zahl + `k1520k_last_error`.
Stabil und additiv; Wächter gleichwertig `test_c_api.py` (Signaturen C-Header ↔ Bibliothek ↔ ctypes mechanisch).
Fäden: Rückrufe laufen im Arbeitsfaden — die Python-Seite stellt sie über Qt-Signale in den GUI-Faden.

### 4.5 Kommandozeile `k1520kermit`

```
k1520kermit ls     <leitung> [pfad]                 Verzeichnis des Servers (G D)
k1520kermit get    <leitung> <entfernt…> --to ordner
k1520kermit put    <leitung> <lokal…> [--as name]
k1520kermit cd|rm|finish|logout <leitung> …
k1520kermit serve  <leitung> <ordner> [--nur-lesen]  (blockiert bis Ende/Strg+C)
Leitung: --telnet host:port | --rfc2217 host:port | --listen-telnet port | --listen-rfc2217 port
         [--login NAME[:KENNWORT]]  [--baud 9600]
Schalter: --pruefsumme 1|2|3  --paket N  --binaer|--text  --namen literal|konvertiert  --frist s  --json
```

Ausgabe wie beim DiskTool: Nutzlast auf stdout, Hinweise/Fortschritt auf stderr, Rückgabewerte 0/1/2 dokumentiert.

### 4.6 Oberfläche (`app/kermittool/`, `bash run_kermit.sh`)

Aufbau wie das DiskTool (`doc/design/13_k1520disktool.md`, `doc/design/11_python_app.md` §10): **jede Bedienung eine
`QAction`** (Menü + Leiste gleich, kein Kürzel ohne `Strg+Umschalt`), Statuszeile mit Zustand, Handbuch
`app/help/…`, Pfade über `app/paths.py`, Konfiguration `kermit.yaml` im Konfigurationsordner.

- **Client-Modus:** links das **Verzeichnis des Servers** (über `G D`, Navigation über `G C`; `ls -l`-Text wird
  zerlegt: Name/Größe/Datum/Art), rechts ein **lokaler Ordner**; Kopieren per Knopf, Doppelklick und
  **Drag&Drop** in beide Richtungen (wie DiskTool), Fortschrittsdialog mit Abbruch, Protokollfenster (Pakete/Fehler).
- **Server-Modus:** Ordner **freigeben** (Pflicht vor dem Start), Start/Stopp, Verbindungsstand, Liste der
  Übertragungen; optional „nur lesen".
- **Verbindungsdialog:** Telnet/RFC 2217 × verbinden/lauschen, Host/Port, Login-Skript, Baud-Vorschlag; zuletzt
  benutzte Verbindungen merken.
- Übertragungsvorgaben (Text/Binär/Automatik nach Endung, Namen literal/konvertiert, Prüfsumme) im
  Einstellungsdialog.
- Eigenes Programm im Menü „Werkzeuge" der anderen Programme (`app/programme.py`) — Wächter `py_programme`
  wird erweitert, nicht umgangen.

## 5. Festlegungen (Vorgaben, begründet)

1. **Prüfsumme:** Vorschlag 3 (CRC-16); Rückfall durch Aushandlung.  C-Kermit 4.2 beherrscht 1–3 und spiegelt den
   Vorschlag (gemessen K7, §6.5).
2. **8-Bit:** QBIN-Vorschlag **`&`** (seit K7; vorher `Y`).  C-Kermit 4.2 präfixt nur, wenn die Gegenseite ein
   Zeichen nennt, und tty0 des WEGA löscht Bit 7 auf dem Weg PC → WEGA — mit `Y` scheitert jedes binäre Senden
   (gemessen K7, §6.5).
3. **Paketlänge:** 94 (Grundprotokoll) bis 1000 (Langpaket) je nach Gegenstelle; C-Kermit 4.2 kann keine
   Langpakete — erwartet wird 94.
4. **Namen:** Senden literal (`readme` bleibt `readme`), Empfangen **konvertiert** wie C-Kermit (`README` → `readme`),
   WEGA-Namen ≤ 14 Zeichen — längere Zielnamen werden **abgelehnt** (Fehlerpaket mit Grund), nicht stumm gekürzt.
5. **Text/Binär:** Vorgabe **binär**; Automatik nach Endung (`.txt .c .h .mac .asm .doc` = Text) ist Option.
6. **Server-Sicherheit:** alle Pfade werden auf die Freigabewurzel normalisiert (`..`, absolute Pfade, Laufwerks-
   buchstaben, Symlinks, die hinausführen, werden abgewiesen); Windows-reservierte Namen abgefangen.
   Wächter mit Angriffsnamen.
7. **Keine Umlaute im Code**, Kommentare deutsch (CLAUDE.md), Oberflächentexte dürfen Umlaute tragen.
8. **Windows:** `K1520K_API` an jeder C-ABI-Funktion, `ws2_32`, `/utf-8`; MinGW-Vorprüfung `tools/dev.sh win`.

## 6. Arbeitspakete

Je AP ein Agent, danach Doku + Commit in `kermit` (nicht pushen).  Build-Kollision: Agenten, die `build/`
anfassen, laufen nacheinander oder im eigenen Worktree.

| AP | Inhalt | Ergebnis / Wächter | Modell | Status |
|----|--------|--------------------|--------|--------|
| **K0** | Befund bestätigen (Binary-Kennung, Magic, Z8000-Lauffähigkeit) — **in K1 enthalten** | — | — | fertig (in K1) |
| **K1** | **WEGA-Installation:** Binary + `.kermrc` + Benutzer `kermit` + Handbuchseite in die Platte (DiskTool `put/attr`), Probe am laufenden WEGA (Anmeldung als `kermit` auf tty0 über Telnet am Hub: Server meldet sich; Konsole: `kermit` startet), bei Misserfolg Rückfall `kermitd`/Selbstbau; Platte neu packen; README. | Systemtest `P8000WegaKermit.ServerHinterGetty` (aus eingefrorenem Zustand, Label `wega_install`), Platte im Git | sonnet | fertig — `7e40c40`, `4e5c0a5`, `65352e4`; zweite Plattenfassung `cda77bb` (K7) |
| **K2** | **Bibliothek Stufe 1:** Paketschicht, Zustandsautomaten Senden/Empfangen/Server, Ordnerfreigabe, Attrappenleitung, C++-Unit-Tests (Kodierung, CRC, QBIN/REPT-Aushandlung, Fehlerfälle, Verlust/Verfälschung, Pfadfalle). | `tests/unit/kermit/*`, Loopback Client↔Server in-process | opus | fertig — `23980c9` (Merge `2987495`) |
| **K3** | **Transport:** `Leitung`, Telnet/RFC 2217 × Client/Server über die Emulator-Codecs, Login-Skript; Test mit Loopback-Sockets (nie feste Ports); Telnet-Binärverhalten gegen den Hub geprüft. | `tests/integration/test_kermit_netz.cpp` | sonnet | fertig — `0d27d6d` |
| **K4** | **C-ABI + Python-Bindung** (`binding.py`), CMake-Ziel `k1520kermit`, Windows-Export, mechanischer Signaturwächter. | `py_kermit_api` | sonnet | fertig — `3933bac` |
| **K5** | **Kommandozeile** `k1520kermit` + `tools/k1520kermit.md`. | `tests/cli/test_kermit_cli.cpp` | sonnet | fertig — `ee32146` |
| **K6** | **Oberfläche** `app/kermittool/` (beide Modi, Verbindungsdialog, Fortschritt, Drag&Drop, Handbuch, Konfiguration). | `py_kermit_gui` (offscreen), Kürzelwächter | sonnet | fertig — `01786b1` |
| **K7** | **Gegenprobe am echten C-Kermit 4.2:** beide Richtungen im Emulator (Fall 1 und Fall 2), Messung der 8-Bit-Sauberkeit, Durchsatz, Fristen; Befunde in Vorgaben §5 einarbeiten. | `P8000WegaKermit.*` (Label `wega_install`), `tools/dev.sh test-wega` | opus | fertig — `d52bdd2`, `cda77bb` |
| **K8** | **Doku:** Handbuch, `doc/merkposten/kermit.md` (+ README-Eintrag), `CLAUDE.md`-Kurzabsatz, `disks/…/README.md`, Entwurf 36 auf Stand. | — | sonnet | fertig (§6.6) |
| **K9** | *(wenn K1–K8 grün)* **Paket/CI:** Linux-Paket (`build_payload.sh`), Inno-Setup, `windows-ci.yml`, Starter `k1520kermit`. | `py_packaging`, `py_programme` | sonnet | fertig (§6.7) — `0f78cfc` |
| K2b | *(optional)* Schiebefenster, `G T`/`G U`, Wiederaufnahme. | — | — | offen (optional) |

Reihenfolge: **K1 ‖ K2** (K2 im Worktree) → K3 → K4 → K5 ‖ K6 → K7 → K8 → K9.

### 6.1 Stand K2 (2026-10-18)

**Fertig:** statische Bibliothek `k1520kermit_core` (`kermit/CMakeLists.txt`, im Haupt-CMake per
`add_subdirectory(kermit)` vor dem Testsystem; PIC, C++17, nur `Threads` + Standardbibliothek, kein POSIX —
Dateien über `std::filesystem`).  Namensraum `k1520::kermit`, Dateien `kermit/src/`:

- `paket.*` — Blockprüfung 1/2/3 (CRC-16/KERMIT, gegen Katalogwert 0x2189 **und** gegen die Rechenwege
  `chk1/chk2/chk3/spack` aus ckfns2.c geprüft), Rahmen normal/Langpaket (LENX + HCHECK), byteweiser
  `PaketZerleger` (Neusynchronisation an MARK, Steuerzeichen im Datenteil = sofort beschädigt, NAK-Länge
  heuristisch wie C-Kermit), Init-Felder S/I/Y (9 Felder oder mit CAPAS/WINDO/MAXLX), Aushandlung,
  Präfixkodierung (QCTL, QBIN, REPT ab 3, max. 94), Attribute (`1 ! # "`).
- `leitung.*` — abstrakte `Leitung` (lesen mit Frist, schreiben, schliessen, `wecken`, **`jetztMs` als
  Zeitquelle**) und `SpeicherLeitungPaar` mit virtueller Uhr (springt nur, wenn alle beteiligten Enden
  warten), Verzögerung, 7-Bit-Abschneiden, Störfunktion je Block, stumme Gegenstelle.
- `dateien.*` — `Freigabe` (Pfadprüfung, `G C`-Navigation, Muster `*`/`?`, Löschen, Anlegen mit
  Namensgrenze und simuliertem Platz), Quellen/Senken, Namensumsetzung (`zrtol`/`zltor`), `ls -l` im
  System-III-Format erzeugen, `zerlegeLs`/`zerlegeLsListe` (System III inkl. angeklebter Linkzahl und
  `-is`-Vorspalten, V7 ohne Gruppe, Linux inkl. long-/full-iso, SELinux-Punkt, Geräte, Symlinks).
- `protokoll.*` / `server.cpp` — `Sitzung` mit `senden`, `sendenGruppe`, `holen`, `empfangen`,
  `entferntVerzeichnis/Wechseln/Loeschen/Hilfe/Beenden/Abmelden`, `serve(Freigabe&)`;
  `abbrechen()` (E-Paket), `abbrechenDatei()` (X), `abbrechenGruppe()` (Z); Rückrufe Fortschritt/Ereignis;
  `Fehlerart` + `fehlerText`.

**Tests:** `tests/unit/kermit/` — `kermit_paket`, `kermit_dateien`, `kermit_sitzung` (Label `unit;fast`);
Client↔Server in zwei Fäden über die Speicherleitung, keine Echtzeitwartezeit.

**Festlegungen/Abweichungen vom Entwurf, mit Grund:**

1. **Zeitquelle ist die Leitung** (`Leitung::jetztMs`) statt einer eigenen Uhr-Klasse: die Frist hängt ohnehin
   am `lesen` der Leitung, eine zweite Quelle könnte auseinanderlaufen.
2. **Blockprüfung:** der Empfänger **spiegelt** die des Senders (C-Kermit 4.2 übernimmt als Sender, was im Y
   steht); der Sender nimmt bei Uneinigkeit 1.  S/I und ihre Quittungen immer Typ 1; nach B wieder 1, so dass
   Serverbefehle und Kurzantworten jede Gegenstelle lesen kann.
3. **Quittung des F-Pakets bleibt leer:** C-Kermit 4.2 prüft das erste Zeichen JEDER Quittung auf `X`/`Z`
   (`canned()`); ein zurückgemeldeter Name wie „Zeta…" bräche dort ab.
4. **Antwort auf ein S/I ohne CAPAS hat genau 9 Felder** (Befund K1: der WEGA-Server antwortet mit MAXL 94,
   TIME 10, EOL CR, `#`, `Y`, 1, `~`; Folgenummer nach I wieder 0 — so auch hier, Wächter
   `KermitSitzung.Ck42AntwortetMitNeunFeldern`).
5. **Server-Leerlauf ohne NAK:** ein N mitten im I→Y→G-Austausch eines C-Kermit-4.2-Clients würde dort mit
   NAK beantwortet.  Vor I/R/G leert der Client den Eingang (wie `ttflui`), weil der C-Kermit-Server bei jedem
   Wiedereintritt ein N(0) schickt.
6. **`..` innerhalb der Freigabe ist erlaubt** (Navigation `G C ..`); abgewiesen wird `..`, das hinausführt.
   Rückstriche, `X:`, Windows-reservierte Namen werden auf ALLEN Systemen abgewiesen; ein Name mit Punkt/
   Leerzeichen am Ende nur unter Windows.  Ein Symlink mit fehlendem Ziel wird verfolgt (sonst legte das
   Schreiben die Datei außerhalb an).
7. **`K` (Kermit-Befehl) wird wie `C` mit E abgewiesen** — eine Ordnerfreigabe führt keine Befehle aus.
8. **Attribute:** Datenfeld unkodiert (wie C-Kermit 5A); ein `"`-Typ des Senders überstimmt den Textmodus des
   Empfängers; ist `1` größer als der freie (simulierte) Platz, antwortet der Empfänger `N1`.
9. **Textmodus:** lokal LF ↔ CR LF auf der Leitung; ein einzelnes CR bleibt erhalten (Rundweg verlustfrei).
   C-Kermit 4.2 verwirft dagegen beim Empfang im Textmodus JEDES CR.
10. **Langpaketgrenze:** Datenbudget = MAXLX − Prüflänge − 7 (vorsichtig), normal MAXL − 2 − Prüflänge.

**Offen (für K3/K7):** C-Kermit 4.2 rechnet `chk1/chk2` über `char` — auf einem Übersetzer mit
vorzeichenbehaftetem `char` (Z8000-`cc`?) weichen Typ 1/2 bei 8-Bit-Rohdaten ab; Typ 3 ist unberührt.
Deshalb Vorschlag 3 oder QBIN `&` beibehalten und in K7 messen.  Schiebefenster, `G T/U`, Wiederaufnahme
(K2b) nicht umgesetzt.

### 6.2 Stand K3/K4 (2026-10-18)

**K3 — Transport (fertig).**  `kermit/src/leitung_netz.{h,cpp}`: `NetzLeitung` mit den Fabriken
`TelnetLeitung`/`Rfc2217Leitung` `::verbinden(host, port, frist)` und `::lauschen(port)` + `annehmen(frist)`
(Port 0 frei, `port()` fragt ihn ab; der Lauscher schliesst nach EINER Verbindung), blockierend mit Frist
(`lesen`: Anzahl / 0 / −1), `wecken()` klebrig aus jedem Faden (auch während `annehmen`).  Aufgesetzt auf
`k1520_serial_codec` + `k1520_serial_net` (Dual-Stack-Lauscher, `Wecker`), nicht auf Wandler/Hub.
`loginskript.{h,cpp}`: RETURN-Wiederholung bis `login:`, Name, bei `Password:` Kennwort, dann auf
`server starting` warten und `ruhe_ms` Stille abwarten; Bit 7 vor dem Vergleich gelöscht; Fehlschlag mit dem
zuletzt Gelesenen (sichtbar gemacht, `<1B>`), Fristen je Schritt einstellbar, `abbruch`-Schalter.
Tests: `tests/integration/test_kermit_netz.cpp` (Label `integration;fast`, ~2 s): 8-Bit-Binär (alle 256
Werte, IAC-Folgen, CR/NUL, 50 KB) × 4 Kombinationen (Telnet/RFC 2217 × A verbindet/lauscht), Frist/wecken/
Trennung (Reste vor dem −1), voller Kermit-Austausch mit 100 KB binär × 4, Trennung mitten in der Datei
(→ `LeitungGetrennt`), Login gegen eine getty-Attrappe mit gerader Parität (virtuelle Zeit), und die
**Gegenprobe gegen einen echten `SerialHub`-Kanal** (Attrappe `SerialAnschluss`, Telnet-Server Port 0; ein
Kermit-Server als „Gast" im Faden, der die Maschinenzeit selbst taktet) mit 6 KB aller Bytewerte hin und zurück.

Befunde zum Hub-Telnet:
1. **Der Hub verhandelt Binär von sich aus** (Server: `WILL ECHO/SGA/BINARY`, `DO BINARY`); ein Client, der nur
   antwortet, ist nach einer Aushandlung in beide Richtungen binär.  Die Leitung wartet deshalb nach dem
   Verbinden/Annehmen bis `aushandeln_ms` (1,5 s) auf BINARY in beiden Richtungen; geht es nicht, läuft sie
   weiter (CR → CR NUL im Codec) und `hinweis()` sagt es.
2. **Der Telnet-Client bietet BINARY selbst an** (`WILL`/`DO BINARY`), der RFC-2217-Client nicht — `Rfc2217Codec`
   gibt den darunterliegenden Telnet-Automaten nur lesend heraus; dort kommt BINARY vom Server (Hub, ser2net).
3. Als RFC-2217-Server **antwortet** die Leitung auf jede Anfrage des Clients (Baud, Format, Steuerung, Maske,
   Leeren, Signatur) mit ihren eigenen Werten und stellt nichts um; als Client merkt sie nur die Antwort
   (`gegenBaud()`; Test: Wunsch 9600, Gast 19200 → Bytes laufen trotzdem).  Es werden **kein RTS/DTR** gesendet:
   der Hub-Wandler hält erst nach dem ersten gesetzten RTS an („CP/A/SCPX setzen es nie").
4. Der Hub liefert an den Gast in Maschinentakten: ein Gast-Faden, der nicht mehr takten lässt, sendet nichts
   mehr — im Test blieb die Schlussquittung (`G L`) liegen, bis der Gast-Faden nachtaktete (Testartefakt, kein Fehler).

**K4 — C-ABI + Python-Bindung (fertig).**  `kermit/include/k1520kermit.h` + `k1520kermit_export.h`
(`K1520K_API`, `K1520K_BUILD_SHARED`/`K1520K_USE_SHARED`), `kermit/src/api.cpp`, SHARED-Ziel `k1520kermit`
(`libk1520kermit.so`/`k1520kermit.dll`, im Wurzel-`build/` wie `libk1520disk`), 35 Funktionen
(`k1520k_*`).  Python: `app/kermittool/{__init__,binding}.py` (keine Qt-Abhängigkeit, lädt die Bibliothek beim
ersten Aufruf), `app/paths.py::kermit_library()` (`K1520_KERMIT_LIB`), `--paths` zeigt „Kermit-Bibliothek".
Wächter `tests/python/test_kermit_api.py` (`py_kermit_api`, 15 Fälle).

Abweichungen/Festlegungen mit Grund:
1. **Ergebnis = `int` (0 = ok), Fehlertext über `k1520k_last_error`**; die Codes 1–12 sind die `Fehlerart`
   der Sitzung (unverändert durchgereicht), 100/101/102 = Verwendung/Leitung/Anmeldung.  Beim Öffnen gibt es kein
   Handle für den Fehlertext: `k1520k_last_open_error` (je Faden).
2. **Ein Handle = Leitung + Sitzung**, die Sitzung entsteht mit dem Handle (die Leitung kennt sie nur als
   Referenz) — Einstellungen wirken also auch vor `accept`.  Server-Handle: `open_*_server` lauscht,
   `accept` nimmt an; danach `serve` bzw. Client-Operationen (beide Rollen sind unabhängig von der Leitungsrolle).
3. **`k1520k_set_paketlaenge(n)`** ist EIN Wert: ≤ 94 = normale Pakete dieser Länge ohne Langpakete,
   > 94 = Langpakete bis n (bis 9024); `k1520k_set_namen` nimmt Senden/Empfang einzeln.
4. **Text-Ergebnisse** (Verzeichnis, Dateinamen, Login-Verlauf) stehen im Handle (`k1520k_result_string`);
   `k1520k_remote_dir` kann zusätzlich in einen Aufrufer-Puffer kopieren.  `k1520k_send` liefert die gesendeten
   LOKALEN Pfade (nicht den Namen beim Empfänger — den kennt der Aufrufer).
5. **`k1520k_parse_ls` liefert JSON** (gültiges UTF-8 bleibt, sonst Latin-1 als `\u00XX`; WEGA-Namen sind ASCII),
   Puffer je Faden.
6. **Abbruch:** `k1520k_abort` setzt Sitzungs- und Login-Schalter und weckt die Leitung (wirkt auch in
   `accept`/`login`/`serve`); `abort_file`/`abort_group` bilden `X`/`Z` ab.  Ein Abbruch VOR dem Start der
   Operation geht verloren (die Sitzung setzt ihre Schalter beim Start zurück).
7. **Wächter vergleicht Typen**, nicht nur Namen: die Prototypen des Headers werden gelesen und gegen
   `argtypes`/`restype` der geladenen Bibliothek (samt Rückruf-Zeigern) gehalten; `K1520K_API` an jeder Funktion
   ist ein eigener Fall.

Offen: Windows-Bau/CI-Lauf der DLL (`tools/dev.sh win` siehe Stand unten), K5 (Kommandozeile) und K6 (Oberfläche)
benutzen die Bibliothek bzw. die Bindung; `k1520kermit` ist seit K9 im Paket (§6.7).

### 6.3 Stand K5 (2026-10-18)

**K5 — Kommandozeile (fertig).**  `tools/k1520kermit.cpp`, Ziel `k1520kermit_cli` in `kermit/CMakeLists.txt` mit
`OUTPUT_NAME k1520kermit` (der Zielname `k1520kermit` gehört der DLL; die Dateien kollidieren nicht), landet im
Wurzel-`build/`.  Wie `k1520disktool` bindet es die **statische** Bibliothek (`k1520kermit_core`) direkt, nicht die
DLL — kein Ladeweg, MSVC/MinGW wie der Rest.  Bedienung, Ausgabekonvention und Rückgabewerte: `tools/k1520kermit.md`.
Tests: `tests/cli/test_kermit_cli.cpp` (`k1520_test_kermit_cli`, 51 Fälle, `cli;fast`, ~3 s): die CLI als **Prozess**
(`std::system` mit Umleitung; Hintergrundprozess = Faden um `std::system`), `serve` gegen Client-Kommandos über
Loopback in beiden Leitungsarten und beiden Lauscher-Rollen, Port 0 (`Port: N` von stderr gelesen), Binärdatei 100 KB,
Namensfallen (`../x`, absolut), `--nur-lesen`, `--max-namen`, Rückgabewerte, JSON-Gültigkeit (eigener Syntaxprüfer),
Login gegen eine getty-Attrappe, Strg+C (SIGINT, POSIX): wartender Server und **mitten in einer Übertragung** (keine halbe
Datei).

Festlegungen und Abweichungen vom Entwurf:
1. **Quelle in `tools/`** (Auftrag), nicht `kermit/cli/` wie in §4.1 skizziert.
2. **Ein Lauscher nimmt EINE Verbindung an** (Eigenschaft von `NetzLeitung::annehmen`): jedes Kommando und jeder
   `serve` ist ein Verbindungsleben.  Ein lauschender `serve` endet also mit `finish`/`logout` **oder mit der
   Trennung** — das ist für `serve` Rückgabe 0 („Gegenstelle hat die Leitung getrennt"), kein Fehler.  Mehrere
   Kommandos über EINE Verbindung gibt es nicht (nur `put`/`get`/`rm` mit mehreren Namen); die Oberfläche (K6) hält das
   Handle über die C-ABI offen.
3. **Strg+C = Rückgabe 1** auch bei `serve` (Abbruch ist kein „Ende durch die Gegenstelle").  Der Signalbehandler setzt
   nur ein Flag; ein Beobachterfaden ruft `Sitzung::abbrechen()` (alle 0,5 s bis zum Ende, weil die Sitzung ihren
   Schalter beim Start einer Operation zurücksetzt) und `Leitung::wecken()`; vor dem Protokollstart (Annehmen, Login)
   nur Wecken + Login-Schalter.
4. **Anmeldefehler = 2** (Verbindungsebene), Fehlerpaket/Abweisung der Gegenstelle = 1.
5. **`--json` immer gültiges JSON**: Erfolg wie in der Doku, Fehler als `{"ok":false,"fehler":…}` auf stdout (und
   Text auf stderr), auch bei Rückgabe 2.
6. **Zusätze zum Entwurf:** `--warte S` (Annehmen), `--cd PFAD` (vor dem Kommando `G C`), `-v` (Ereignisse eines
   Clients; ein `serve` meldet immer), `--fortschritt`, `-l`.  `--frist` gilt je Paket **und** für den Verbindungsaufbau;
   die Anmeldung hat eigene Vorgaben (60 s je Schritt).
7. **Schalter an beliebiger Stelle**, `--schalter=wert` erlaubt; Aufruf-/Pfadprüfung (`--as` bei mehr als einer Datei,
   lokale Datei fehlt, Ordner fehlt) passiert **vor** dem Verbindungsaufbau (Rückgabe 2, kein halber Austausch).

### 6.4 Stand K6 (2026-10-18)

**K6 — Oberfläche (fertig).**  `app/kermittool/` (PySide6), Starter `run_kermit.sh`, Programmname in der
Oberfläche „K1520 Kermit", Handbuch `app/kermittool/help/handbuch.md` (mit `{{VERSION}}`, Fenster
`app/ui_help.py`).  Aufbau: `main.py` · `arbeiter.py` (Arbeitsfaden) · `konfig.py` (`kermit.yaml`) ·
`ui/{main_window,actions,fern_view,lokal_view,server_view,verbindung_dialog,einstellungen_dialog,fortschritt_dialog,ablage}.py`.
Wiederverwendet statt neu gebaut: `app/ui_help.py`, `app/ui_icons.py` (Symbole `folder/in/out/serial/…`),
`app/disktool/ui/{log_dock,info_bar}.py`, `app/config_io.py` (Laden, Schreiben, Schutz vor neuerer Formatversion).

Tests: `tests/python/test_kermit_gui.py` (`py_kermit_gui`, 26 Fälle, offscreen, ~1 s): Fenster/Aktionen,
Symbolleistenliste, Kürzelwächter, Handbuchtabelle in beiden Richtungen, Handbuchabschnitte (u. a. „Kermit Einrichtung
unter WEGA") und Hilfemenü, Drag&Drop-Auswertung, Rundlauf GUI-Client ↔ Bibliotheks-Server (Verzeichnis, Holen,
Schreiben, Doppelklick, Löschen mit Rückfrage, Finish), Abbruch einer 8-MB-Übertragung (Verbindung bleibt),
Server-Modus (Start ohne Freigabe verweigert, Rundlauf, nur lesen, wieder lauschen, Stopp), Konfiguration ohne Kennwort
(Lesen/Schreiben, neuere Formatversion bleibt unberührt, defekte Datei). `test_programme.py` um `KERMIT` erweitert.

Festlegungen und Abweichungen vom Entwurf, mit Grund:
1. **Ein Arbeitsfaden (`QThread`) mit Warteschlange** besitzt die Sitzung; Rückrufe der Bibliothek werden dort in
   Qt-Signale umgesetzt (Fortschritt höchstens alle 50 ms, sonst kämen auf Loopback tausende je Sekunde), das
   Fenster verbindet mit `Qt.QueuedConnection`.  Der Zähler „beschäftigt" läuft im GUI-Faden und wird im ersten
   Empfänger von `auftrag_fertig` zurückgezählt — so stimmt er in jedem späteren Empfänger.
2. **Abbrechen ist zweistufig:** der Dialog-Knopf löst `abort_group` (geordnet, Verbindung bleibt; am Loopback
   gemessen: Rückgabe `E_ABORTED`, danach weiter benutzbar), der zweite Klick `abort` (hart, Leitung weg).
   `Trennen`/`Stopp` rufen immer `abort`.  Das Handbuch sagt beides.
3. **Server-Modus ist ein einziger langer Auftrag** mit Schleife (lauschen → bedienen → bei „wieder lauschen"
   neu); ein Fehler der Gegenseite (Trennung) beendet ihn nicht, Stopp/Abbruch schon.  `nur lesen` gilt ab dem
   nächsten Start (die Bibliothek nimmt es in `serve`).  Client mit Lauschen und „wieder lauschen" verbindet bei
   `E_DISCONNECTED` neu.
4. **Start ohne Freigabe** ist kein Meldungsfenster, sondern eine Warnung im Streifen (`InfoBar`) + Protokoll; `server_starten()`
   liefert `False`.  Kein modales Fenster außer der Löschrückfrage.
5. **Kennwort nur in `self._kennwort`**; `konfig.schreiben` entfernt rekursiv jeden Schlüssel `kennwort*` (Wächter:
   Datei enthält weder den Wert noch das Wort).  Die WEGA-Voreinstellung trägt das dokumentierte Laborkennwort im
   Programmtext (`konfig.VOREINSTELLUNGEN`), nie in der Datei.
6. **Konfiguration `kermit.yaml`:** Abschnitte `uebertragung`, `client`, `server` (inkl. `ordner`, `nur_lesen`),
   `optionen`, `zuletzt`, `lokal`, `window` (Geometrie wie beim Emulator: `restoreGeometry` + eigenes Feld
   `maximized`, im `showEvent` nachgeholt; Symbolleiste als Namensliste, unbekannte Namen werden übergangen, Umbau per
   `removeAction`, nie `clear()`).  `K1520_KERMIT_CONFIG` biegt den Pfad um (Tests, Mehrfachbetrieb).  `QSettings`
   wird nicht benutzt.
7. **Kürzel nur `Strg+Umschalt+…`**, keine mit F-Tasten (auch kein F1/F5/F8): Handbuchtabelle ist ein Vertrag, beide
   Richtungen geprüft.  Eine **allgemeine** Prüfung über alle Programme gab es nicht (der Wächter
   `test_no_shortcut_steals_a_key_from_the_emulated_machine` gilt nur dem Emulatorfenster; das DiskTool hat
   `Strg+O` u. a. und wäre durchgefallen) — K6 bringt die Prüfung für das eigene Programm
   (`test_no_shortcut_takes_a_plain_key`), nicht für die anderen.
8. **Programm starten:** `programme.KERMIT` (Skript `app/kermittool/main.py`, wie die Nachbarn relativ zu
   `paths.base_dir()`; kein eigener Eintrag in `app/paths.py`, weil dort nur Dateien außerhalb von `app/` stehen);
   Menüpunkt „K1520 Kermit starten" im Menü Werkzeuge des Emulators (alle Profile) und des DiskTool, ohne Kürzel;
   `py_programme` erweitert.
9. **Geschwindigkeit der Gegenstelle** (`k1520k_set_tempo`, additiv durch K7): Eintrag im Einstellungsdialog
   („Automatisch" = 0, 0,1x … 20x), Schlüssel `tempo` in `kermit.yaml`, der Arbeitsfaden setzt ihn vor jedem Auftrag.
10. **Dateien**, nicht Ordner: Ordner lassen sich weder ziehen noch holen/senden (Protokoll kennt keine Rekursion);
    die Anzeige der Server-Dateigröße kommt aus `parse_ls`.
11. **Handbuch** ausführlich (Was ist Kermit, Bedienung, Server-Modus, Kommandozeile, „Kermit Einrichtung unter WEGA"
    a–e, Fehlersuche).  Nicht auf der WEGA selbst geprüft und im Text so gekennzeichnet: `tar x` der Kermit-Diskette unter
    WEGA mit den genannten Dateinamen, die Befehlsform für `chown`/`cat > .kermrc`, `passwd kermit` (geprüft ist der
    Weg über das DiskTool, K1).

**Offen:** keine Pixel-/Drag&Drop-Ereignisprüfung (offscreen nicht möglich; nur Auswertung und Annahmemethoden);
Login-Skript in der GUI nur über die CLI/C++-Tests gegen eine getty-Attrappe belegt, nicht in `test_kermit_gui`;
Paket/Windows (K9); Gegenprobe am echten C-Kermit im Emulator (K7).

### 6.5 Stand K7 (2026-10-19) — Gegenprobe am echten C-Kermit 4.2

**Test:** `tests/system/test_p8000_wega_kermit_austausch.cpp` (Label `wega_install`; `tools/dev.sh test-wega -R Kermit`):
`P8000WegaKermit.PcClientGegenWegaServer` (Fall 1), `.WegaClientGegenPcServer` (Fall 2), `.PruefsummenUnd8Bit`,
`.VerbindungsabbruchUndWiederaufnahme`, `Takt/P8000WegaKermitTempo.AdaptiveFristen/{frei,echtzeit,last}`, Werkzeug
`.DISABLED_SendenEinzeln`.  Start aus einem eigenen eingefrorenen Zustand `kermit31_k7_vN` (Plattenkopie + Testdateien
per DiskTool + Kaltstart; baut sich beim ersten Lauf in ~60 s Rechenzeit, ein Stempel erzwingt den Neubau, wenn sich die
Platte ändert); danach kostet jeder Fall 30–250 s Wirtszeit statt 87 s Boot.  Fall 1: Maschine im eigenen Faden
(`Motor`, frei oder gedrosselt), Bibliothek im Testfaden; Fall 2 umgekehrt.

**Bewiesen (grün):** Fall 1 — Login-Skript gegen getty, `G D` (WEGA-`ls -l`, `zerlegeLsListe` liest alle Testdateien mit
richtiger Größe), `G C`, `R` von 0 B/1 B/94 B/95 B/Text/4 KB (alle 256 Werte)/100 KB zufällig, `S` 100 KB nach `/tmp` und
byte­gleich zurück, `G E`, Name > 14 Zeichen (Bibliothek lehnt ab; ohne Grenze kürzt WEGA stumm auf 14), Namensumsetzung
(Server sendet groß, Empfang konvertiert → klein, literal → groß; gesendete Namen schreibt C-Kermit klein), Abbruch mitten
in der Datei beim Holen (X, keine halbe Datei beim PC) und beim Senden (Z „D", C-Kermit verwirft die Datei), `G F` und
`G L` (getty meldet sich wieder), zweite Anmeldung auf derselben Verbindung.  Fall 2 — `kermit` an der Konsole,
`set line /dev/tty3` (Hub-Kanal als Telnet-**Client** zum `Sitzung::serve` der Bibliothek: geht), `send` Text (kommt mit
CR LF an, C-Kermit-Vorgabe Text) und binär, `get`, Rücksendung bytegleich, `remote directory`, `remote cwd` (fragt nach
einem Kennwort), `finish` beendet den PC-Server.

**Messwerte** (P8000 Vollgerät, tty0 bzw. tty3 am Hub, 9600 Bd, Paketlänge 94 — C-Kermit 4.2 kennt keine Langpakete):

| | Befund |
|---|---|
| (a) 8-Bit | **WEGA → PC sauber**, **PC → WEGA nicht** (tty0 löscht Bit 7 — getty-Einstellung bleibt nach `login`; 4 Wiederholungen, dann „Zu viele Wiederholungen").  Aushandlung real: unser `Y` + C-Kermits `Y` ⇒ kein Präfix (C-Kermit präfixt ohne eigene Parität nur, wenn die Gegenseite ein Zeichen nennt); unser `&` ⇒ C-Kermit antwortet `&`, Präfix aktiv.  tty3 (Fall 2): mit `&` des PC-Servers gefahren, nicht roh geprüft. |
| (b) Prüfsumme | C-Kermit 4.2 spiegelt 1/2/3.  Mit QBIN `&`: 1, 2, 3 in beiden Richtungen grün.  Ohne Präfix WEGA → PC: 1 und 3 grün, **2 scheitert** (5 beschädigte Pakete) — gemessen bestätigt: das WEGA-`cc` hat vorzeichenbehaftetes `char`, `chk2()` summiert `char`; Typ 1 und 3 sind rechnerisch unberührt (Nachbildung `KermitPaket.Ck42VorzeichenbehaftetesCharTrifftNurTyp2`).  Vorgabe der Bibliothek bleibt **3**. |
| (c) Durchsatz | Maschinenzeit: R 100 KB zufällig 269 B/s, S 100 KB 328 B/s, R 4 KB roh 393 B/s, R Text 434 B/s (Grenze 9600 Bd ≈ 960 B/s; der Rest ist Rechenzeit des U8001 je Stop-and-Wait-Paket).  Wirtszeit bei freier Maschine (≈ 5,5×): 1384 / 1564 B/s.  `G D` (fork + `ls -l`): 3,9 s Maschinenzeit. |
| (d) Fristen/Tempo | Tempo-Schätzung aus der Zeichenrate: frei 5,26 (Ist ≈ 5,7), 1× (gedrosselt) 0,87, 0,3× 0,26; Login-Schätzung vorsichtig zu klein (1,0 / 0,38 / 0,13, Rechenpausen des Gastes im Text).  Wirksame Paketfrist 10 s / 11,5 s / 38,5 s Wirtszeit, Umlaufzeit 12 / 65 / 216 ms, **0 Wiederholungen** in allen drei.  Anmeldung ≈ 12 s Maschinenzeit bis zum Serverbeginn.  10× ist im Test nicht einstellbar (die Maschine schafft frei ≈ 5,5×) — 10×/0,1× sind mit der virtuellen Uhr nachgebildet (`KermitTempo.*`). |
| (e) Trennung | Socket mitten in `R` geschlossen: Bibliothek meldet `LeitungGetrennt`, keine halbe Datei.  C-Kermit bleibt in der Transaktion und NAKt das nächste Datenpaket (N25), ohne Eingriff bis MAXTRY × (TIME + Leerpause) > 120 s Maschinenzeit; kein SIGHUP über den Hub.  Neuer Client: bricht sie mit einem E-Paket (in der Prüfung der alten Transaktion) ab, danach sofort bedienbereit, ohne neue Anmeldung. |
| (f) Zeichenverlust | Keiner: 0 beschädigte Pakete über je 100 KB in beide Richtungen bei freier Maschine; alle Wiederholungen ließen sich auf Protokollursachen zurückführen (unten). |

**Gefundene Fehler und Eigenheiten — behoben:**

1. WEGA-Platte (zweite Fassung): Shell des Benutzers `kermit` `/bin/sh` + `.profile` `exec /usr/bin/kermit`, weil
   C-Kermit `G D/E/T/U/W` über die Login-Shell aus `passwd` ausführt (sonst Gebrauchsanweisung statt `ls -l`); `set file
   names literal` aus `.kermrc` gestrichen, weil `sfile()` dann den uninitialisierten Paketnamen in den Dateinamen kopiert
   (jedes `R`: „Can't open file").  `doc/p8000/kermit_wega.md` §6.
2. Bibliothek, Vorgabe **QBIN `&`** statt `Y` (a).
3. Sender wiederholt sofort bei einer Quittung für das VORIGE Paket (C-Kermits Antwort auf ein beschädigtes Paket) und bei
   einem wiederholten Paket der Gegenstelle; N(n+1) ist für S/I keine Quittung.
4. Leerlauf-NAK: das erste N(0) auf I/S/G/R wird nicht beantwortet (sonst doppeltes Y(0), als Antwort auf das nächste
   Paket missdeutet), ab dem zweiten wiederholt.  **Nachlauf:** hat der Client zuletzt ein B quittiert, wartet er vor dem
   nächsten Befehl (auch vor S) auf dieses N(0), höchstens `nachlauf_ms` = 2 s Zeit der Gegenstelle — C-Kermit leert nach
   jedem guten Paket seinen Eingang (`rpack` → `ttflui`) und verschluckte sonst das sofort folgende I/S.
5. Server wiederholt die Schlussquittung auf ein wiederholtes B (auch bei Prüfung 2/3; B wird über die Länge gelesen).
6. `G C` ohne Pfad trägt ein leeres Feld (C-Kermit `cwd()` liest die Feldlänge ungeprüft).
7. Alte Transaktion der Gegenstelle nach Trennung: E-Paket mit ihrer Prüfungslänge (e).

**Zeitverhalten (Anforderung „übertaktete Gegenstellen"):** `Einstellungen::tempo` (0 = Automatik), `baud`,
`frist_min_ms`/`frist_max_ms`, `nachlauf_ms`; `frist_ms` gilt in der Zeit der Gegenstelle.  Wirksam: `frist_ms /
min(Tempo, 1)`, mindestens 4 × geglättete Umlaufzeit (Karn: nur unwiederholte Pakete), dieser Zuschlag höchstens bis TIME
der Gegenstelle / Tempo; jedes empfangene Byte verlängert die laufende Frist.  Eine schnelle Gegenstelle bekommt **keine
kürzere** Frist — sie heilt mit ihrer eigenen; gleich lange Fristen kreuzten die Wiederholungen (virtuelle Uhr: 122 statt
0).  TIME der Gegenstelle ist nur noch Obergrenze (früher direkt die Wirtszeitfrist).  Login: Fristen der Stille, RETURN
nur bei stiller Leitung, Ruhe nach `login:`, ausdrückliches Tempo teilt alle Zeiten, Zeichenrate → `LoginErgebnis::tempo`
→ Startwert der Sitzung.  C-ABI additiv `k1520k_set_tempo`/`k1520k_tempo` (Bindung `set_tempo`/`tempo`), CLI `--tempo
auto|F`, Paketrückruf `Rueckrufe::paket` (Protokollfenster, Fehlersuche `K1520_KERMIT_PAKETE=1`), Zähler `Statistik`.
Unit-Tests `tests/unit/kermit/test_kermit_tempo.cpp` (10× und 0,1× mit `SpeicherLeitungPaar::setzeZeichenzeit`).

**Gemessen vs. angenommen:** gemessen ist alles in der Tabelle und in den Punkten 1–7 (je ein Wächter, Liste in
`doc/merkposten/kermit.md`).  Nur aus den Quellen gelesen, nicht im Emulator ausgelöst: der Speicherfehler bei `G C` ohne
Feld; die `leftover`-Reste in `getpkt()` nach einem abgebrochenen Senden des C-Kermit (einmal beobachtet: Dateiname mit
fremdem Byte davor, nach einem gescheiterten Roh-Transfer; die Messung schaltet ihn mit einem `G D` ab, die Bibliothek
filtert solche Namen nicht); Verhalten bei Parität ≠ none.

**Offen:** echte 10×-Messung (Maschine zu langsam im Test); tty3 ohne Präfix nicht vermessen; Namensfilter gegen
Steuer-/8-Bit-Bytes im empfangenen Dateinamen; Windows-Lauf der neuen Tests.

### 6.6 Stand K8 (2026-10-20) — Dokumentation

**Fertig.**  Handbuch `app/kermittool/help/handbuch.md` sprachlich neu geschrieben (kurze Sätze, unpersönliche
Anrede wie Emulator- und DiskTool-Handbuch, Schritte als nummerierte Listen) und inhaltlich auf den Stand K7
gebracht: Benutzer `kermit` mit Shell `/bin/sh` + `.profile`, `.kermrc` ohne `set file names literal` (beides
mit Begründung), QBIN `&`, C-Kermit 4.2 spiegelt die Prüfsumme (die alte Aussage „nur Typ 1" ist entfernt),
Messwerte zu Tempo und Anmeldung, *Trennen* beendet den WEGA-Server NICHT (nur *Finish*), Fall 2 mit
`set speed` (nicht `set baud`, so im Test getippt).  Neuer Unterabschnitt **Schnellstart** (beide Fälle) vor der
Einrichtung; der DiskTool-Weg ist als Befehlsfolge ausgeschrieben und an einer Plattenkopie nachgefahren
(`get` von `kermit.cqm`, `put`/`mkdir`/`attr`, `put --force` für `/etc/passwd`, `check --full` ohne Befund;
Befund dabei: `put --force` setzt den Modus auf 0644 zurück → danach `attr`).  Als **nicht geprüft**
gekennzeichnet: der ganze Weg unter WEGA selbst (`tar xvf /dev/rfd1 …`, `echo … >> /etc/passwd`,
`passwd kermit`, `chown`/`chgrp`, `cat > .kermrc`), Fall 2 an anderen Leitungen als der Konsole, das
Wiederverbinden ohne Anmeldung nach bloßem *Trennen* in der Oberfläche (nur an der Bibliothek belegt), tty3
ohne 8-Bit-Präfix.  Dazu: `CLAUDE.md`-Kurzabsatz, §6-Statusspalte, §7.  Merkposten (`doc/merkposten/kermit.md`,
README-Eintrag) und `disks/p8000_wega31/README.md` waren seit K7 aktuell.

### 6.7 Stand K9 (2026-10-20) — Paket und CI

**Fertig, lokal belegt:** das Linux-Paket schnürt (`packaging/build_payload.sh`, Bau in `build_dist/`) und enthält
`bin/libk1520kermit.so`, `bin/k1520kermit-cli`, `share/doc/k1520kermit.md`, `app/kermittool/` samt `help/handbuch.md`,
`kermit_launcher.sh` und `k1520kermit.desktop.in`.  Geschnitten **wie das DiskTool**, nicht wie ein Emulator:
`launcher.*` wählt am Namen ein Maschinenprofil (`app/profil.py`), Kermit hat keines — also eigene Vorlage
`kermit_launcher.sh/.cmd`.  `bin/k1520kermit` ist in der Installation der **Starter**, die Kommandozeile heißt dort
`bin/k1520kermit-cli` (im Quellbaum bleibt `build/k1520kermit` die Kommandozeile; `paths.kermit_cli()` fragt erst den
`-cli`-Namen).  `app/paths.py::kermit_library()` löste im Installationslayout schon vorher auf (`--paths` zeigt
`Kermit-Bibliothek` und neu `Kermit (CLI)`); die einzige Änderung dort ist `kermit_cli()`.  Weder Bibliothek noch
Kommandozeile tragen einen Pfad des Baurechners (kein Formatkatalog), geprüft in `py_packaging`
(`test_kermit_aus_dem_paket_laeuft_und_ist_sauber`) und im Rauchtest von `release.yml`.
`install.sh`: Starter, Verweis, `.desktop`, Rauchtest (`k1520k_version`), Schlussmeldung; Deinstallieren über
`WERKZEUGE`/`KONSOLENWERKZEUGE` (Ausweis unverändert — alles liegt unter `bin app share`).  Die WEGA-3.1-Platte mit dem
Benutzer `kermit` (`p8000_wega31/`) kommt über die bestehende `p8000_*`-Regel mit (Wächter
`test_die_wega_platte_mit_kermit_benutzer_liegt_im_paket`: Passwd-Zeile im Abbild, bitgleich zur eingecheckten Platte).
Wächter: `test_kermit_ist_in_allen_paketwegen` (Skripte, `.iss`, beide Workflows), `test_kermit_cli_name_im_installationslayout`,
`test_kermit_findet_seine_bibliothek_im_installationslayout`, dazu die erweiterten Listen (Starter, Desktop-Einträge, ASCII-Starter,
Nutzlast).  Merkposten: `doc/merkposten/paketierung.md`, Entwurf 13 §10c.

**Nur durch die CI prüfbar (Dateien angepasst, Workflows NICHT ausgelöst):** Inno-Setup-Lauf (`k1520emu.iss`: Symbol *K1520 Kermit*,
`bin\k1520kermit.cmd`, Rauchtest-Feld 47 Zeilen), MSVC-Bau und `dumpbin /exports` (`k1520k_*`, Schwelle 20, 37 Funktionen) und
`/dependents` für `k1520kermit.dll`, Paket- und Setup-Prüfung in `windows-ci.yml`, beide Rauchtests in `release.yml`
(Bibliothek lädt, `k1520k_version` = Bauversion, `--help` der Kommandozeile, `--paths` des Einstiegs).  Die Größenschranke der
Setup-Probe (136 MB) wurde nicht angefasst; Kermit bringt ~1 MB mit.

## 7. Offene Aufgaben

Zusammengetragen aus den „Offen"-Listen von §6.1–§6.6, nach Dringlichkeit:

1. ~~**K9 — Paket und CI**~~ erledigt (§6.7); offen bleibt dort nur der erste echte Lauf von `release.yml`/`windows-ci.yml`
   (Paketjob mit `-f paket=true`) — siehe 2.
2. **Windows:** MinGW-Vorpruefung gruen (`tools/dev.sh win -R kermit`, 192/192 unter wine, K9).  Offen: MSVC-Gegenprobe der CI für Bibliothek, CLI, die neuen Tests (Tempo, Austausch, Strg+C nur POSIX
   geprüft) **und das Paket** (Inno-Setup, Kermit-Symbol, Rauchtest) — bisher nicht gelaufen.
3. **Empfangsnamen filtern:** Steuer- und 8-Bit-Bytes im Dateinamen eines F-Pakets (beobachtet nach einem
   abgebrochenen Senden des C-Kermit: `leftover`-Rest in `getpkt()`) werden nicht abgefangen.
4. **Echte 10×-Messung:** im Test schafft die Maschine frei nur ≈ 5,5×; 10× und 0,1× sind nur mit der
   virtuellen Uhr nachgebildet (`KermitTempo.*`).
5. **tty3 ohne 8-Bit-Präfix** nicht vermessen (Fall 2 lief mit `&` des PC-Servers).
6. **Nachlauf 2 s gegen den eigenen Server:** die Wartezeit auf das Leerlauf-N(0) nach einem B kostet gegen
   einen Server, der keins schickt (die eigene Bibliothek), 2 s je Befehl nach einem Empfang — ggf. am
   Init-Austausch erkennen und abschalten.
7. **Oberfläche:** Login-Skript nur über CLI/C++-Tests belegt, nicht in `py_kermit_gui`; Drag&Drop nur über
   die Auswertung der Mime-Daten (offscreen keine echten Ereignisse); Wiederverbinden ohne Anmeldung nach
   bloßem *Trennen* nicht eigens geprüft.
8. **Kennwort:** das Laborkennwort `kermit` steht in der Voreinstellung (`konfig.VOREINSTELLUNGEN`) und auf
   der ausgelieferten Platte; ob eine Abfrage beim ersten Verbinden oder ein anderes Konto für das Paket
   besser ist, entscheidet der Anwender.
9. **Nur aus den Quellen gelesen**, nicht ausgelöst: Speicherfehler bei `G C` ohne Feld, Verhalten bei
   Parität ≠ none.
10. **K2b (optional):** Schiebefenster, `G T`/`G U`, Wiederaufnahme.

## 8. Risiken

- **Binary läuft nicht unter WEGA 3.1** (gebaut 1988 für WEGA 3.0?) → Selbstbau aus den Quellen auf WEGA (langsam im
  Emulator, aber machbar), notfalls Quellen 070.
- **`.kermrc`/`server` hängt nicht sauber hinter `login`** (Terminal-Modi, Abmelden) → `kermitd`-Wrapper.
- **Leitung nicht 8-bit-sauber / Zeichenverlust** (Koppelsoftware, Ringpuffer) → QBIN `&`, kleinere Pakete; Befund in
  `doc/merkposten/kermit.md`.
- **Hub-Takt:** Gast-Baud in Maschinenzeit → Fristen im Protokoll zu knapp bei hoher Emulationsgeschwindigkeit
  (Merkposten Seriell „Maschinenzeit, nicht Uhr").
- **Git-Größe** der neu gepackten Platte (≈ 6 MB je Fassung): nur eine Fassung committen.

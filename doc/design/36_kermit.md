# 36 — Kermit: Datenaustausch Host-PC ↔ WEGA (P8000)

Stand 2026-10-18, Zweig `kermit` (von `P8000`).  Zwei Teile: **(A)** Kermit auf der WEGA-3.1-Platte
einrichten (Server hinter `getty`, Client in der Shell), **(B)** ein eigenes Kermit für den PC
(C++-Bibliothek + Kommandozeile + Python/Qt-Oberfläche), Client **und** Server.
Merkposten (nach AP K8): `doc/merkposten/kermit.md`.

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
| Server-Benutzer | `kermit` in `/etc/passwd`: uid ≠ 0 bevorzugt (Prüfen: Kermit braucht root nur für Sperrdateien; uid 7 `kermit`, gid 1), **leeres Kennwort** (Laborrechner; dokumentieren — ein Kennwort ist Option K1b), Home `/usr/kermit`, Shell `/usr/bin/kermit`. |
| Start | `/usr/kermit/.kermrc`: `set ...` (Baud/Parität/Dateinamen wie in §5) dann `server`, danach `exit`.  Wenn `.kermrc` nicht gelesen wird oder `server` Ende nicht abmeldet: Rückfall `kermitd`. |
| Client am WEGA | Anmelden wie gewohnt (Konsole/tty0/tty2), `kermit`, `set line /dev/tty3`, `set baud 9600`, `send`/`get`/`remote dir` gegen den PC-Server — als Text in `README`/Handbuch `Kermit` beschreiben, kein Gastcode. |
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

1. **Prüfsumme:** Vorschlag 3 (CRC-16); Rückfall durch Aushandlung.  C-Kermit 4.2 beherrscht 1–3.
2. **8-Bit:** QBIN `Y`-Aushandlung; ist die Leitung nicht 8-bit-sauber, einigt man sich auf `&` (Präfix).  Im Emulator
   gemessen (K7), nicht angenommen.
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

| AP | Inhalt | Ergebnis / Wächter | Modell |
|----|--------|--------------------|--------|
| **K0** | Befund bestätigen (Binary-Kennung, Magic, Z8000-Lauffähigkeit) — **in K1 enthalten** | — | — |
| **K1** | **WEGA-Installation:** Binary + `.kermrc` + Benutzer `kermit` + Handbuchseite in die Platte (DiskTool `put/attr`), Probe am laufenden WEGA (Anmeldung als `kermit` auf tty0 über Telnet am Hub: Server meldet sich; Konsole: `kermit` startet), bei Misserfolg Rückfall `kermitd`/Selbstbau; Platte neu packen; README. | Systemtest `P8000WegaKermit.ServerHinterGetty` (aus eingefrorenem Zustand, Label `wega_install`), Platte im Git | sonnet |
| **K2** | **Bibliothek Stufe 1:** Paketschicht, Zustandsautomaten Senden/Empfangen/Server, Ordnerfreigabe, Attrappenleitung, C++-Unit-Tests (Kodierung, CRC, QBIN/REPT-Aushandlung, Fehlerfälle, Verlust/Verfälschung, Pfadfalle). | `tests/unit/kermit/*`, Loopback Client↔Server in-process | opus |
| **K3** | **Transport:** `Leitung`, Telnet/RFC 2217 × Client/Server über die Emulator-Codecs, Login-Skript; Test mit Loopback-Sockets (nie feste Ports); Telnet-Binärverhalten gegen den Hub geprüft. | `tests/integration/test_kermit_netz.cpp` | sonnet |
| **K4** | **C-ABI + Python-Bindung** (`binding.py`), CMake-Ziel `k1520kermit`, Windows-Export, mechanischer Signaturwächter. | `py_kermit_api` | sonnet |
| **K5** | **Kommandozeile** `k1520kermit` + `tools/k1520kermit.md`. | `tests/cli/test_kermit_cli.cpp` | sonnet |
| **K6** | **Oberfläche** `app/kermittool/` (beide Modi, Verbindungsdialog, Fortschritt, Drag&Drop, Handbuch, Konfiguration). | `py_kermit_gui` (offscreen), Kürzelwächter | sonnet |
| **K7** | **Gegenprobe am echten C-Kermit 4.2:** beide Richtungen im Emulator (Fall 1 und Fall 2), Messung der 8-Bit-Sauberkeit, Durchsatz, Fristen; Befunde in Vorgaben §5 einarbeiten. | `P8000WegaKermit.*` (Label `wega_install`), `tools/dev.sh test-wega` | opus |
| **K8** | **Doku:** Handbuch, `doc/merkposten/kermit.md` (+ README-Eintrag), `CLAUDE.md`-Kurzabsatz, `disks/…/README.md`, Entwurf 36 auf Stand. | — | sonnet |
| **K9** | *(wenn K1–K8 grün)* **Paket/CI:** Linux-Paket (`build_payload.sh`), Inno-Setup, `windows-ci.yml`, Starter `k1520kermit`. | `py_packaging`, `py_programme` | sonnet |
| K2b | *(optional)* Schiebefenster, `G T`/`G U`, Wiederaufnahme. | — | — |

Reihenfolge: **K1 ‖ K2** (K2 im Worktree) → K3 → K4 → K5 ‖ K6 → K7 → K8 → K9.

## 7. Risiken

- **Binary läuft nicht unter WEGA 3.1** (gebaut 1988 für WEGA 3.0?) → Selbstbau aus den Quellen auf WEGA (langsam im
  Emulator, aber machbar), notfalls Quellen 070.
- **`.kermrc`/`server` hängt nicht sauber hinter `login`** (Terminal-Modi, Abmelden) → `kermitd`-Wrapper.
- **Leitung nicht 8-bit-sauber / Zeichenverlust** (Koppelsoftware, Ringpuffer) → QBIN `&`, kleinere Pakete; Befund in
  `doc/merkposten/kermit.md`.
- **Hub-Takt:** Gast-Baud in Maschinenzeit → Fristen im Protokoll zu knapp bei hoher Emulationsgeschwindigkeit
  (Merkposten Seriell „Maschinenzeit, nicht Uhr").
- **Git-Größe** der neu gepackten Platte (≈ 6 MB je Fassung): nur eine Fassung committen.

# `k1520kermit` — Kermit-Client und -Server für die Kommandozeile

Überträgt Dateien zwischen dem PC und einem Kermit über **Telnet** oder **RFC 2217** — als Client
(PC holt/sendet bei einem Kermit-Server, z. B. dem C-Kermit der WEGA-3.1-Platte) **und** als Server
(PC gibt einen Ordner frei, z. B. für das `kermit` der WEGA-Shell).  Dieselbe Protokollschicht
(`kermit/`, `k1520kermit_core`) treibt die C-ABI `libk1520kermit` und die Oberfläche; das Werkzeug bindet die
statische Bibliothek wie `k1520disktool` direkt.  Entwurf: `doc/design/36_kermit.md` §4.5.

Aufruf im Projekt immer über `tools/dev.sh tool k1520kermit …` — nie direkt aus `build/`
(CLAUDE.md „Build & test").

## Kommandos

```
k1520kermit ls     <leitung> [pfad] [-l]                Verzeichnis des Servers (G D)
k1520kermit get    <leitung> <entfernt…> [--to ordner]  Dateien holen (Vorgabe: aktueller Ordner)
k1520kermit put    <leitung> <lokal…> [--as name]       Dateien senden (--as nur bei genau einer Datei)
k1520kermit cd     <leitung> <pfad>                     Verzeichnis des Servers wechseln (G C)
k1520kermit rm     <leitung> <name…>                    Dateien auf dem Server löschen (G E)
k1520kermit finish <leitung>                            Server beenden, das Programm bleibt (G F)
k1520kermit logout <leitung>                            Server beenden und abmelden (G L)
k1520kermit serve  <leitung> <ordner> [--nur-lesen]     Ordner freigeben
```

Jedes Kommando baut **eine** Verbindung auf, tut seine Sache und trennt.  Die Schalter dürfen an jeder
Stelle stehen (`--telnet 127.0.0.1:5000 --login kermit:kermit ls` ist dasselbe wie `ls --telnet …`);
`--schalter=wert` geht ebenfalls.

### Leitung (genau eine)

| Schalter | Bedeutung |
|---|---|
| `--telnet HOST:PORT` / `--rfc2217 HOST:PORT` | verbinden (z. B. zum Hub-Kanal des Emulators) |
| `--listen-telnet PORT` / `--listen-rfc2217 PORT` | lauschen, **eine** Verbindung annehmen (der Emulator verbindet als Client) |
| `--login NAME[:KENNWORT]` | Anmeldung an einer getty vor dem Protokollstart (RETURN bis `login:`, Name, Kennwort, Serverbeginn abwarten) |
| `--baud N` | RFC 2217: Wunsch (Vorgabe 9600); der Gast ist maßgeblich |
| `--warte S` | Lauschen: so lange auf die Verbindung warten (Vorgabe 300 s) |

Die Rolle der Leitung (verbinden/lauschen) ist unabhängig von der im Protokoll (Client/Server): auch
`put --listen-telnet 0` und `serve --telnet host:port` sind ein gültiges Paar.
**Port 0 = freier Port.**  Der gebundene Port steht danach auf **stderr** als eigene Zeile `Port: N` (vor dem
Warten auf die Verbindung), damit Skripte ihn lesen können:

```sh
k1520kermit serve --listen-telnet 0 ~/frei 2> serve.err &
until grep -q '^Port: ' serve.err; do sleep 0.1; done
PORT=$(sed -n 's/^Port: //p' serve.err)
```

### Übertragung

`--pruefsumme 1|2|3` (Vorgabe 3, wird ausgehandelt) · `--paket N` (Vorgabe 1000; ≤ 94 = normale Pakete ohne
Langpakete) · `--binaer` (Vorgabe) | `--text` (LF ↔ CR LF) · `--namen literal|konvertiert` (beide Richtungen;
ohne den Schalter: senden literal, empfangen konvertiert wie C-Kermit) · `--max-namen N` (Zielnamen länger als N
werden **abgelehnt**, WEGA: 14) · `--frist S` (je Paket in der Zeit der Gegenstelle und für den
Verbindungsaufbau, Vorgabe 10 s) · `--tempo auto|F` (Tempo der Gegenstelle gegenüber Echtzeit: ein P8000 mit
10x Takt = `10`, unter Last `0.5`; Vorgabe `auto` = aus der Zeichenrate gemessen, Startwert aus dem Login-Text) ·
`--cd PFAD` (vor dem Kommando auf dem Server wechseln) · `--nur-lesen` (nur `serve`: Schreiben und Löschen
werden abgewiesen).

### Ausgabe

`-l` (nur `ls`), `--json`, `--fortschritt`, `-v`, `-h`/`--help`.

## Ausgabe weiterverarbeiten

Die **Standardausgabe ist reine Nutzlast**, alles andere (Hinweise, Fortschritt, Ereignisse, `Port: N`,
Fehlermeldungen) geht nach **stderr**:

- `ls` gibt **ohne `-l` nur die Namen** aus, einen je Zeile; mit `-l` die Zeilen des Servers (`ls -l`-Text
  des Servers, bei WEGA System III), mit `--json` die zerlegten Einträge
  (`name`, `art`, `rechte`, `links`, `besitzer`, `gruppe`, `groesse`, `datum`, `ziel`, `jahr`, `monat`, `tag`,
  `stunde`, `minute`; fehlende Zahlen −1, fehlende Texte `""`).
- `get` gibt die abgelegten Pfade aus (`<ordner>/<name>`), `put` die gesendeten lokalen Pfade, `rm` die
  gelöschten Namen — je Zeile einen.
- `serve` schreibt auf stdout nichts; jede Übertragung meldet es auf stderr (`Empfange: …`, `Empfangen: …`,
  `Gesendet: …`, `Verzeichnis: …`, `Server beendet (FINISH)`).
- **`--json`** gilt für alle Kommandos.  `ls` liefert ein Array; die übrigen
  `{"ok":true,"dateien":[…],"meldung":"…"}`.  **Im Fehlerfall steht immer ein Objekt
  `{"ok":false,"fehler":"…"}`** auf stdout (zusätzlich der Text auf stderr), damit eine Weiterverarbeitung nie
  an leerer Eingabe scheitert.
- **Fortschritt** (`Datei: bytes / gesamt Bytes`) erscheint nur, wenn stderr ein Terminal ist oder
  `--fortschritt` gegeben wurde.

```sh
$ k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit ls 2>/dev/null | wc -l
$ k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit ls --json | jq '.[] | select(.art=="-") | .name'
```

## Rückgabewerte (Teil der Schnittstelle)

| Wert | Bedeutung |
|---|---|
| `0` | in Ordnung (bei `serve`: `finish`/`logout` oder Trennung durch die Gegenstelle) |
| `1` | Fehler der Übertragung oder der Gegenstelle: Fehlerpaket (Datei nicht da, abgewiesen, `--nur-lesen`, Name zu lang, Pfadfalle), Zeitüberschreitung, Leitung mitten in der Übertragung weg, **Abbruch mit Strg+C** |
| `2` | Aufruf-, Verbindungs- oder Anmeldefehler: unbekanntes Kommando/Schalter, keine/zwei Leitungen, lokale Datei oder Ordner fehlt, Verbindung nicht aufgebaut, `Annehmen` ohne Gegenstelle, Login-Skript gescheitert |

## Abbruch mit Strg+C

Strg+C (SIGINT/SIGTERM) bricht **sauber** ab: ein Beobachterfaden setzt den Abbruchschalter der Sitzung
(Fehlerpaket an die Gegenstelle) und weckt die Leitung — auch während des Wartens auf eine Verbindung, der
Anmeldung und einer laufenden Übertragung.  Eine angefangene Empfangsdatei wird **verworfen**, nicht halb
liegengelassen.  Rückgabe `1`.

## Beispiele

**Fall 1 — WEGA ist Server** (Kermit auf der Platte, hinter `getty`; Benutzer `kermit`, Kennwort `kermit`,
`doc/p8000/kermit_wega.md`).  `p8000emu` verbindet tty0 per Telnet an Port 5000:

```sh
k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit ls -l
k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit put brief.txt --as brief
k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit get README --to ~/wega
k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit --max-namen 14 put langerdateiname.c
k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit logout
```

Eine getty meldet sich erst nach Eingabe: das Login-Skript sendet RETURN bis `login:` erscheint (nur bei
stiller Leitung).  Die Anmeldung braucht auf dem Emulator Maschinenzeit (≈ 10 s bis zum Serverbeginn); ihre
Fristen sind Fristen der **Stille** (jedes Zeichen setzt sie neu, Vorgabe 60 s), `--frist` ändert nur die
Paketfrist.

**Zeitverhalten (K7, `doc/merkposten/kermit.md` Nr. 3–5).**  Der Emulator taktet die serielle Leitung in
Maschinenzeit: läuft er 6× schneller als Echtzeit, kommen die Zeichen 6× so schnell wie 9600 Bd; unter Last
langsamer.  Die Paketfrist ist deshalb adaptiv: `frist / min(Tempo, 1)` Wirtszeit (eine langsame Gegenstelle
bekommt mehr Zeit, eine schnelle nicht weniger), mindestens 4 × Umlaufzeit, jedes empfangene Zeichen verlängert
sie.  `--tempo` braucht man nur, wenn die Automatik daneben liegt (z. B. Emulator stark gedrosselt und kaum
Text vor dem ersten Paket).

**Fall 2 — WEGA ist Client** (Shell: `kermit`, `set line /dev/tty3`, `set speed 9600`, `set file type binary`,
`send`/`get`/`remote dir`; `remote cwd` fragt nach einem Kennwort — RETURN): der PC ist Server, der Emulator
verbindet tty3 als Telnet-Client zu ihm.  Ohne `set file type binary` sendet C-Kermit 4.2 Text (CR LF auf der
Leitung, ohne Attribute) und verwirft beim Empfang jedes CR.

```sh
k1520kermit serve --listen-telnet 5003 ~/austausch                 # blockiert bis finish/Trennung/Strg+C
k1520kermit serve --listen-rfc2217 0 ~/austausch --nur-lesen       # freier Port, nur Lesen
```

Wer den Emulator lauschen lässt statt ihn verbinden: `serve --telnet 127.0.0.1:5003 ordner`.

## Tests

`tests/cli/test_kermit_cli.cpp` (`k1520_test_kermit_cli`, Label `cli;fast`): CLI-`serve` und Client-Kommandos
als Prozesse gegeneinander über Loopback (Telnet/RFC 2217 × Lauscher Server/Client, Port 0, nie feste Ports),
Binärdatei 100 KB, Namensfallen, Rückgabewerte, Ausgabekonvention, Login gegen eine getty-Attrappe, Strg+C
(POSIX).  `tools/dev.sh test -R KermitCli`.

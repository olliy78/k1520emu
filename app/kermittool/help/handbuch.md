# K1520 Kermit — Handbuch

Version {{VERSION}}

K1520 Kermit tauscht Dateien zwischen diesem Rechner und einem Kermit auf der Gegenseite aus —
vor allem mit der **WEGA des P8000-Emulators**, aber mit jedem Kermit, der über **Telnet** oder
**RFC 2217** erreichbar ist. Das Programm kann beides: **Client** (es fragt einen Kermit-Server) und
**Server** (es gibt einen Ordner frei, und ein Kermit-Client holt oder bringt Dateien).

Wer nur wissen will, wie es losgeht: *Der übliche Weg*. Wer Kermit unter WEGA erst einrichten muss:
*Kermit Einrichtung unter WEGA*. Was ein Begriff bedeutet: *Was ist Kermit*. Wenn etwas nicht
geht: *Fehlersuche*.

## Was ist Kermit

**Kermit** ist ein Dateiübertragungsprotokoll der Columbia University (New York, 1981 für den
Austausch zwischen unterschiedlichsten Rechnern entworfen). Seine Idee: **nur Zeichen übertragen, die
jede Leitung durchlässt** — druckbare ASCII-Zeichen, ohne Steuerungs- und Hardwareanforderungen. Deshalb
läuft es über eine einfache serielle Leitung, ein Modem oder, wie hier, über eine Telnet-Verbindung zum
Emulator, und deshalb gibt es Kermit für fast jedes System. Der C-Kermit 4.2, der auf der WEGA-Platte
liegt, ist so eine Fassung.

**Das Protokoll in Worten.** Alles geschieht in kleinen **Paketen**. Ein Paket beginnt mit einem
Startzeichen, trägt Länge, laufende Nummer, Typ (z. B. Init, Dateiname, Daten, Ende), die Nutzdaten und
eine **Prüfsumme**. Der Empfänger quittiert jedes Paket (ACK) oder verlangt es erneut (NAK); bleibt
die Antwort aus, sendet der Absender nach einer **Frist** noch einmal. So geht auf einer gestörten Leitung
kein Byte verloren, und eine kaputte Verbindung wird nach den eingestellten Wiederholungen als Fehler
gemeldet.

* **Prüfsumme:** Typ 1 (6 Bit, Summe), 2 (12 Bit) oder 3 (**CRC-16**, die sicherste). Beide Seiten
  handeln beim Start aus, welche gilt; die schwächere Seite bestimmt.
* **Quotierung:** Steuerzeichen und Bytes mit gesetztem achtem Bit werden durch ein Präfixzeichen
  ersetzt (`#` für Steuerzeichen, `&` für das 8-Bit-Präfix), damit sie die Leitung unverändert
  überstehen. Gleiche Bytes hintereinander fasst ein **Wiederholungszähler** (`~`) zusammen.
* **Paketlänge:** normale Pakete haben höchstens 94 Zeichen. **Langpakete** erlauben mehr, aber
  nicht jede Gegenstelle kennt sie — der C-Kermit 4.2 der WEGA nicht.
* **Dateiangaben:** vor den Daten kann ein Attributpaket Größe, Datum und Art (Text/Binär) nennen.
* **Client und Server:** Ein **Server** wartet und tut, was man ihm sagt (Dateien annehmen und liefern,
  das Verzeichnis zeigen, ein Verzeichnis wechseln, Dateien löschen, sich beenden). Ein **Client**
  gibt die Befehle. Beide Rollen kann jeder Kermit spielen; wer Client ist, steht in der Bedienung, nicht
  im Protokoll.

## So arbeitet K1520 Kermit

* **Eigene Bibliothek.** Das Protokoll steckt in einer C++-Bibliothek (`libk1520kermit`), die die
  Oberfläche und die Kommandozeile `k1520kermit` gemeinsam benutzen. Die Oberfläche ruft sie in
  einem **eigenen Arbeitsfaden** auf: das Fenster friert nie ein, eine Übertragung lässt sich jederzeit
  abbrechen, und der Fortschritt kommt als Meldung zurück.
* **Leitung.** Zum Emulator führt eine **Telnet**- oder **RFC-2217**-Verbindung (RFC 2217 ist Telnet
  mit Steuerung von Baudrate und Format einer seriellen Leitung). Die Leitung wird **8-Bit-binär**
  behandelt. Das Programm kann zum Emulator **verbinden** oder **lauschen**, bis der Emulator sich
  meldet; welche Seite verbindet, ist unabhängig davon, wer Client oder Server ist.
* **Rollen.** *Client:* das Programm fragt einen Kermit-Server (Fall 1, die WEGA ist Server).
  *Server:* das Programm gibt einen Ordner frei (Fall 2, die WEGA ist Client).
* **Login-Skript.** Hängt der Server hinter einer Anmeldung (getty/`login`), sendet das Programm RETURN, bis
  `login:` erscheint, dann den Benutzernamen und, wenn eingestellt, das Kennwort, und wartet dann auf den
  Beginn des Servers. Bit 7 wird beim Erkennen der Texte ignoriert, denn die Gegenstelle kann gerade
  Parität senden.
* **Fristen und Tempo.** Der Emulator taktet die Leitung nach der Baudrate des Gastes in
  **Maschinenzeit**, nicht in Wirtszeit. Läuft er schneller oder langsamer als Echtzeit (z. B. mit zehnfachem
  Takt), kommen die Zeichen entsprechend früher oder später an. Die Frist je Paket (Vorgabe 10 s) ist
  deshalb großzügig; die Einstellung *Geschwindigkeit der Gegenstelle* (Vorgabe *Automatisch*) legt fest,
  mit welchem Faktor die Bibliothek ihre Fristen rechnet.
* **Namen und Sicherheit.** Der freigegebene Ordner des Servers ist eine Wurzel: `..`, absolute Pfade,
  Laufwerksbuchstaben und Verweise, die hinausführen, werden abgewiesen.

## Der übliche Weg

1. Im P8000-Emulator (`p8000emu`) den Kanal **tty0** oder **tty2** als *Telnet-Server* freigeben
   (Einstellungen ▸ Schnittstellen) und sich den Port merken.
2. Hier **Verbindung ▸ Als Client verbinden…**, die Voreinstellung *WEGA 3.1 (Benutzer kermit)* wählen,
   den Port eintragen, *Verbinden*.
3. Links erscheint das Verzeichnis des Servers, rechts der lokale Ordner. Dateien per Doppelklick,
   Knopf oder Ziehen von einer Seite auf die andere bringen.
4. Am Ende **Verbindung ▸ Trennen**. Die Anmeldung der WEGA meldet sich danach wieder.

## Das Fenster

Links steht der **Server**, rechts ein **lokaler Ordner** mit Adresszeile und Ordnerknopf (die Rücktaste geht eine
Ebene hinauf). Zwischen beiden stehen die zwei Richtungen als Knöpfe:

* **Holen** (Pfeil nach rechts): ausgewählte Serverdateien in den lokalen Ordner.
* **Schreiben** (Pfeil nach links): ausgewählte lokale Dateien zum Server.
* **Auf dem Server löschen** entfernt Serverdateien, nach einer Rückfrage.

Ein Doppelklick auf einen Ordner (oder `..`) wechselt das Verzeichnis, auf eine Datei überträgt sie. Ziehen
geht in beide Richtungen; Dateien lassen sich auch aus dem Dateimanager auf die linke Seite ziehen. Ordner
überträgt Kermit nicht.

Im **Client-Modus** zeigt die linke Seite Name, Größe, Datum und Rechte (der `ls -l`-Text des Servers, in
Spalten zerlegt). Der Pfad darüber ist relativ zum Startverzeichnis der Sitzung. Im **Server-Modus** steht
dort stattdessen die Liste der **Übertragungen und Ereignisse**: eine Zeile je Datei mit Byte-Zähler und Zustand
(*läuft*, *fertig*), dazu die Ereignisse der Sitzung.

Eine Übertragung zeigt einen **Fortschrittsdialog**. *Abbrechen* beendet sie geordnet (die Verbindung bleibt
benutzbar); reagiert die Gegenstelle nicht, beendet ein zweiter Klick (*Hart abbrechen*) die Sitzung. Alles, was
geschieht, steht im **Protokoll** (Ansicht ▸ Protokoll), auch der Fehlertext der Gegenstelle. Dauerhafte
Meldungen (Fehler, Hinweise) erscheinen in einem Streifen über den beiden Seiten und lassen sich schließen.

Die **Statuszeile** zeigt den Zustand: *Getrennt*, *Verbunden mit …*, *Lauscht auf Port N*,
*Übertragung: Datei*, im Server-Betrieb mit dem Vorsatz *Server:*; rechts die Übertragungsart.

Die **Symbolleiste** ist eine Liste von Aktionsnamen; sie lässt sich über Ansicht ▸ Symbolleiste
ausblenden. Ein unbekannter Name in der Konfiguration wird übergangen.

## Verbindung herstellen (Client)

*Verbindung ▸ Als Client verbinden…* öffnet den Verbindungsdialog:

* **Voreinstellung:** *WEGA 3.1 (Benutzer kermit)* trägt Benutzer `kermit`, Kennwort `kermit` und die
  WEGA-Werte ein (Namen höchstens 14 Zeichen, Paketlänge 94). Darunter stehen die **zuletzt benutzten**
  Verbindungen.
* **Leitung:** *Telnet* oder *RFC 2217*. **Aufbau:** *Verbinden* (zum Emulator) oder *Lauschen* (der
  Emulator verbindet hierher; Port 0 = freien Port nehmen, er steht dann in der Statuszeile).
* **Host** und **Port** (beim Verbinden), **Baud-Vorschlag** (nur RFC 2217; der Gast ist maßgeblich).
* **Anmelden:** Haken, Benutzername und Kennwort für ein Login-Skript.

Das **Kennwort wird nur für diese Sitzung gehalten und nie in die Konfigurationsdatei geschrieben** — auch
nicht in die Liste der zuletzt benutzten Verbindungen.

Ein Lauscher nimmt nur **eine** Verbindung an. Mit **Nach der Trennung wieder lauschen** lauscht ein Client, der
lauscht, nach deren Ende erneut.

*Trennen* schließt die Leitung (oder beendet das Warten). *Server der Gegenstelle beenden (Finish)* beendet
den Kermit-Server der Gegenstelle ordentlich; auf der WEGA meldet sich die Anmeldung danach wieder.

## Server-Modus: einen Ordner freigeben

Der Server gibt einen **Ordner** frei, den die Gegenstelle sieht. Der Weg:

1. **Verbindung ▸ Ordner freigeben…** — **ohne Freigabe startet der Server nicht**.
2. **Verbindung ▸ Als Server einrichten…** — Leitung, Aufbau, Port; Voreinstellung *WEGA als Client (hier
   lauschen)*.
3. **Server starten** (Strg+Umschalt+S); **Server stoppen** beendet ihn und die Leitung.

*Nur lesen* verbietet der Gegenstelle das Schreiben und Löschen; die Einstellung gilt ab dem nächsten Start.
**Nach der Trennung wieder lauschen** hält den Server bereit, auch wenn die Gegenstelle ohne `FINISH` getrennt hat;
sonst endet der Server mit der Sitzung und wird neu gestartet. Ein `host`-Kommando (C) der Gegenstelle weist der
Server ab: eine Ordnerfreigabe führt keine Befehle aus.

## Einstellungen

*Ansicht ▸ Einstellungen…*

* **Übertragungsart:** *Binär* überträgt Byte für Byte, *Text* setzt Zeilenenden um (lokal LF, auf der Leitung CR LF),
  *Automatik* nimmt Text für `.txt .c .h .mac .asm .doc`, sonst binär.
* **Namen** beim Senden und Empfangen: *literal* (der Name bleibt) oder *konvertiert* (Kleinschrift/Punkt
  wie C-Kermit).
* **Prüfsumme** 1, 2 oder 3 (CRC-16). Die Gegenstelle darf herunterstufen.
* **Paketlänge:** bis 94 normale Pakete, darüber Langpakete.
* **Fristen:** Frist und Wiederholungen je Paket, Frist je Schritt der Anmeldung.
* **Geschwindigkeit der Gegenstelle:** *Automatisch* oder ein fester Faktor (1x, 10x …) für einen Emulator,
  der schneller oder langsamer als Echtzeit läuft.
* **Längster Zielname:** auf der WEGA 14 Zeichen. Längere Namen werden **abgelehnt**, nicht gekürzt.
* **Vorhandene Dateien beim Empfang ersetzen.**
* Der Knopf *WEGA-Werte* setzt Paketlänge 94, Namen 14 und Prüfsumme 3.

Die Einstellungen stehen in `kermit.yaml` im Konfigurationsordner des Programms (ohne Kennwörter), samt
Fensterlage, Symbolleiste, freigegebenem Ordner und der Liste der zuletzt benutzten Verbindungen.

## Die Kommandozeile k1520kermit

Dieselbe Bibliothek steckt in der Kommandozeile `k1520kermit` (Aufruf im Quellbaum über
`tools/dev.sh tool k1520kermit …`; Bedienung: `tools/k1520kermit.md`):

    k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit ls -l
    k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit put brief.txt --as brief
    k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit get README --to ~/wega
    k1520kermit serve --listen-telnet 5003 ~/austausch

Kommandos: `ls`, `get`, `put`, `cd`, `rm`, `finish`, `logout`, `serve`. Die Leitung wird mit `--telnet`,
`--rfc2217`, `--listen-telnet` oder `--listen-rfc2217` gewählt; `--login NAME[:KENNWORT]` meldet an.
Die Ausgabe auf stdout ist reine Nutzlast, alles andere geht nach stderr; `--json` liefert maschinenlesbare
Ausgabe. Rückgabewerte: `0` in Ordnung, `1` Fehler der Übertragung oder der Gegenstelle (auch Strg+C),
`2` Aufruf-, Verbindungs- oder Anmeldefehler.

## Fall 1 — die WEGA ist der Server

Auf der fertig eingerichteten WEGA-Platte (`disks/p8000_wega31/`, im Paket als Beispieldiskette) ist Kermit als
Server hinter der Anmeldung eingerichtet: Benutzer **kermit**, Kennwort **kermit**, Shell `/usr/bin/kermit`; die
Datei `.kermrc` startet den Server. Der Weg:

* im Emulator tty0 oder tty2 als Telnet-Server freigeben,
* hier verbinden mit Anmeldung *kermit / kermit* (Voreinstellung *WEGA 3.1*),
* das Login-Skript sendet RETURN, bis `login:` erscheint, dann Name und Kennwort und wartet auf den
  Serverbeginn. Das dauert bei 9600 Baud und der Maschinenzeit des Emulators einige Sekunden bis zu etwa
  zehn Sekunden.

## Fall 2 — die WEGA ist der Client

Hier läuft das Programm als **Server** (Abschnitt *Server-Modus*). Auf der WEGA, angemeldet als `wega`:

    kermit
    C-Kermit>set line /dev/tty3
    C-Kermit>set baud 9600
    C-Kermit>send datei
    C-Kermit>get datei
    C-Kermit>remote directory

`/dev/tty3` ist am P8000 die Druckerleitung (kein getty); im Emulator wird dieser Kanal als **Telnet-Client**
zum PC konfiguriert (Einstellungen ▸ Schnittstellen), das Programm lauscht auf dem eingetragenen Port.

## Kermit Einrichtung unter WEGA

Dieser Abschnitt richtet Kermit auf einer WEGA-3.1-Platte **von Grund auf** ein. Wer das nicht braucht,
nimmt die **fertig eingerichtete Platte** `disks/p8000_wega31/p8000_wega31_platte.avr.img.gz` (im Paket
Ordner `p8000_wega31/`): sie enthält alles Folgende bereits. Quellen der Angaben:
`doc/p8000/kermit_wega.md` und `disks/p8000_wega31/README.md`. Wo ein Schritt auf der WEGA selbst
**nicht geprüft** ist, steht es dabei.

### a) Installation von der Kermit-Diskette

Die Kermit-Diskette `kermit.cqm` (tar720, ein tar-Archiv ohne Dateisystem) trägt C-Kermit **4.2(030)**: Quellen
(`ckermi.nr`, `ckermi.man`, `ck*.c`, `makefile`) und ein **fertiges Z8000-Programm** `kermit/kermit` (78 560 Byte,
Stand 4. 2. 1988, Modus 4755, Besitzer root). Dieses Programm **läuft unter WEGA 3.1 unverändert** (an der Konsole
geprüft: `kermit` startet, `show parameters` antwortet) — ein Selbstbau war nicht nötig.

Auf der Diskette liegt alles unter `kermit/`. Weg über die Diskette im Emulator (die Handgriffe sind die des
Abschnitts *Arbeiten mit Disketten unter WEGA* im Handbuch des `p8000emu`):

1. Die Diskette `kermit.cqm` in Laufwerk **B:** einlegen (die Startdiskette steckt in A:; die Diskettenbefehle
   gehen nur an der Systemkonsole und nur bei einem Start über die WEGA-Startdiskette).
2. An der Konsole als `wega` angemeldet das Archiv vom Raw-Gerät (`/dev/rfd1`) auspacken, hier nur die
   zwei gebrauchten Dateien:

       #1 mkdir /usr/kermit
       #2 cd /usr/kermit
       #3 tar xvf /dev/rfd1 kermit/kermit kermit/ckermi.nr

3. Das Programm an seinen Platz, mit den Rechten wie auf der Diskette (setuid root, wegen der Sperrdateien
   unter `/usr/spool/uucp`), und die Handbuchseite aus der nroff-Quelle:

       #4 cp kermit/kermit /usr/bin/kermit
       #5 chmod 4755 /usr/bin/kermit
       #6 cp kermit/ckermi.nr /usr/man/man1/kermit.1

   Die Platte hat danach auf `/usr` (`md0`) noch Platz: das Programm braucht rund 78 KB, auf der ausgelieferten
   Platte waren dort 537 KB frei.

*Nicht geprüft:* der Weg `tar x` **unter WEGA** mit diesen Dateinamen auf der Platte. Geprüft ist der Weg über das
**DiskTool** (Platte offline, Emulator aus): `k1520disktool put <abbild> … --as md0/bin/kermit`, `mkdir`,
`attr` für Modus und Besitzer; danach `k1520disktool check --full` ohne Befund und der Probelauf am laufenden WEGA.
Alternativ ohne Diskette: die fertig eingerichtete Platte aus dem Paket `p8000_wega31`.

### b) Benutzer kermit einrichten

Der Server läuft als **Benutzer mit Programm statt Shell** — dasselbe Muster, das die Platte schon für `uucp`
kennt (`uucp:…:/usr/lib/uucp/uucico`). Als root in `/etc/passwd` eine Zeile anhängen:

    kermit:<verschlüsseltes Kennwort>:7:1::/usr/kermit:/usr/bin/kermit

* **uid 7 / gid 1** (`other`): uid 7 war auf der Platte frei (belegt: 0–6, 10, 24).
* **Heimatordner** `/usr/kermit`, **Shell** `/usr/bin/kermit`.
* **Kennwort:** ein leeres Kennwort **geht nicht** — der System-III-`login` antwortet dann `You don't have a
  password. Choose one.` und startet `passwd`. Das Kennwort setzt man deshalb mit `passwd kermit` (als root);
  auf der ausgelieferten Platte ist es **`kermit`** (Laborrechner — hängt die Maschine am Netz, mit `passwd`
  ändern, und im Programm im Verbindungsdialog entsprechend eintragen). *Nicht geprüft:* die Eingabe der
  Zeile und `passwd kermit` auf der WEGA selbst; die ausgelieferte Platte trägt die Zeile mit dem
  verschlüsselten Kennwort `kermit` (mit dem DiskTool eingespielt, nicht auf der WEGA getippt).

Heimatordner und Startdatei anlegen:

    #1 mkdir /usr/kermit
    #2 cat > /usr/kermit/.kermrc
    set file type binary
    set file names literal
    server
    exit
    (Strg+D)
    #3 chown kermit /usr/kermit /usr/kermit/.kermrc

*Nicht geprüft:* die genaue Befehlsform (`chown` mit Name oder Zahl, `cat` als Editor) auf der WEGA; maßgeblich sind
Inhalt und Besitzer: `/usr/kermit` und `.kermrc` gehören `kermit` (uid 7, gid 1), `.kermrc` hat Modus 0644.

Was `.kermrc` bewirkt: C-Kermit liest sie beim Start, auch wenn es als Login-Shell läuft. Die beiden `set`-Zeilen
sorgen dafür, dass der Server Dateien unverändert liefert (binär, Namen unverändert); die Vorgaben von C-Kermit
4.2 wären Text und Namen in Großschrift. `server` startet den Server; nach `FINISH` oder `LOGOUT` kehrt der
Server zurück, `exit` beendet die Sitzung, und die Anmeldung (getty) meldet sich wieder.

### c) Das System konfigurieren

* **Anmeldeleitungen:** `/etc/inittab` startet `/etc/GETTY` auf **tty0, tty2 und tty4–7** (Zustand 2). tty0 und
  tty2 hängen über die Koppelsoftware am **Hub des Emulators**; dort gibt man den Kanal als **Telnet-Server**
  frei (Einstellungen ▸ Schnittstellen im `p8000emu`) und trägt den Port hier ein. Die Leitung läuft mit
  **9600 Baud**. An `inittab` ist für Fall 1 nichts zu ändern.
* **Fall 2 (WEGA ist Client):** `tty3` ist die Druckerleitung des P8000 und hat **kein getty**. Dort hinein
  geht `set line /dev/tty3` und `set baud 9600`; im Emulator wird dieser Kanal als **Telnet-Client** zum PC
  eingestellt, das Programm lauscht (Server-Modus).
* **Dateinamen höchstens 14 Zeichen.** Ein längerer Name wird **abgelehnt**, nicht gekürzt (Fehlerpaket mit
  Grund). Das Programm kennt dafür *Längster Zielname* (WEGA-Voreinstellung 14).
* **Binär oder Text:** Vorgabe ist binär. Ein Programm (`a.out`) im Textmodus ist danach kaputt; Textmodus nur
  für Textdateien.
* **Prüfsumme:** der C-Kermit 4.2 bietet im Austausch **Typ 1** und Paketlänge 94 an (er kann weder Typ 2/3 noch
  Langpakete); die Aushandlung stuft deshalb herunter. Das Programm schlägt 3 vor und nimmt, was die Gegenstelle
  nimmt. 8-Bit-Sauberkeit der Leitung und Durchsatz sind *nicht abschließend geprüft*; der Kermit-Standard (`&`-Präfix
  für das achte Bit) deckt eine nicht 8-bit-saubere Leitung ab.

### d) Probe und Selbsttest

1. Emulator mit WEGA starten und warten, bis `WEGA login:` an der Konsole steht (nach dem Kaltstart mehrere Minuten
   Maschinenzeit). tty0 als Telnet-Server freigeben, Port merken.
2. Hier *Als Client verbinden*, *WEGA 3.1 (Benutzer kermit)*, Port eintragen, *Verbinden*. Das Login-Skript
   meldet an; nach einigen Sekunden steht die Zustandszeile auf *Verbunden mit …*, und links erscheint das
   Verzeichnis des Servers (*nicht geprüft*, welches Verzeichnis zuerst erscheint: vermutlich `/usr/kermit`).
3. Eine Textdatei **holen** (Doppelklick) und eine kleine Datei **schreiben**; im Protokoll steht die
   Aushandlung (Prüfsumme, Paketlänge), der Fortschritt und der Fehlertext der Gegenstelle, falls etwas
   abgelehnt wird.
4. *Server der Gegenstelle beenden (Finish)* oder *Trennen*: an der WEGA meldet sich `login:` wieder.

An der WEGA-Konsole prüft man den Kermit selbst mit `kermit` und `show parameters`
(`Line: /dev/tty, speed: …, mode: remote`).

### e) Fehlersuche für die Einrichtung

* **`login:` erscheint nicht:** die getty meldet sich erst nach einem Zeichen — das Login-Skript sendet RETURN
  und wartet. Nach dem Einschalten kommt die Zeile ohne Parität, nach einer Sitzung mit gerader Parität
  (`<D7><C5>GA<A0>lo<E7>i<EE>:`); das Programm vergleicht ohne das achte Bit.
* **Anmeldung scheitert:** Kennwort stimmt nicht, oder das Konto fehlt. Zeile in `/etc/passwd` prüfen
  (Benutzer, Kennwort, Heimatordner, Shell).
* **`You don't have a password. Choose one.`:** das Konto hat ein leeres Kennwort — mit `passwd kermit`
  setzen.
* **Nach dem Kennwort kommt kein Server:** `/usr/kermit/.kermrc` fehlt oder gehört nicht `kermit`; ohne
  `server` bleibt der Kermit auf dem Befehlsprompt `C-Kermit>` stehen.
* **Zeitüberschreitung:** nach Name und Kennwort braucht die Gegenseite rund zehn Sekunden Maschinenzeit bis zum
  Server. Davor eingehende Pakete würden noch vom Terminaltreiber behandelt. Frist erhöhen
  (*Frist je Anmeldeschritt*, *Frist je Paket*), die Geschwindigkeit der Gegenstelle prüfen.
* **Name abgelehnt:** länger als 14 Zeichen — vorher umbenennen.
* **Datei unbrauchbar:** Binär/Text verwechselt; *Binär* wählen.
* **Fehler 'unable to work with floppy':** beim Einlesen der Diskette unter WEGA — die Diskettenbefehle gehen nur an
  der Systemkonsole und nur nach Start über die WEGA-Startdiskette.

## Fehlersuche

* **„Verbindung fehlgeschlagen":** nichts lauscht auf Host und Port. Im Emulator den Kanal als Telnet-Server
  freigeben und den Port prüfen — oder hier *Lauschen* wählen und den Emulator als Telnet-Client verbinden
  lassen.
* **Anmeldung scheitert:** *Protokoll* öffnen (Ansicht ▸ Protokoll): dort stehen die Schritte des
  Login-Skripts und das zuletzt Gelesene (Steuerzeichen als `<1B>` sichtbar gemacht).
* **Zeitüberschreitung bei einer Übertragung:** die Leitung ist langsam (9600 Baud ≈ ein Kilobyte je Sekunde),
  oder der Emulator läuft nicht in Echtzeit. *Frist je Paket* erhöhen, *Geschwindigkeit der Gegenstelle* einstellen.
* **Der Server startet nicht:** es ist kein Ordner freigegeben (Meldung im Streifen), oder der Port ist belegt.
* **Es kommt nur eine Verbindung an:** ein Lauscher nimmt nur eine Verbindung an — *wieder lauschen* setzen.
* **„Abgebrochen":** der Abbruch war gewollt; die Verbindung bleibt, wenn er geordnet war.
* **Name zu lang / Datei abgelehnt:** der Text der Gegenstelle steht im Protokoll und im Streifen.
* **Datei kaputt:** Binär/Text prüfen; ein Programm nie im Textmodus übertragen.
* **Beide Seiten warten:** *Hart abbrechen* beendet die Sitzung.

## Tastenkürzel

| Kürzel | Wirkung |
|--------|---------|
| Strg+Umschalt+K | Als Client verbinden |
| Strg+Umschalt+T | Trennen |
| Strg+Umschalt+S | Server starten |
| Strg+Umschalt+→ | In den Ordner holen |
| Strg+Umschalt+← | Zum Server schreiben |
| Strg+Umschalt+Entf | Auf dem Server löschen |
| Strg+Umschalt+R | Aktualisieren |
| Strg+Umschalt+E | Einstellungen |
| Strg+Umschalt+L | Protokoll |
| Strg+Umschalt+H | Dieses Handbuch |
| Strg+Umschalt+Q | Beenden |

Alle Kürzel tragen Strg+Umschalt, wie bei den Nachbarprogrammen.

## Die anderen Werkzeuge

Im Menü *Werkzeuge* des Emulators und des DiskTool steht *K1520 Kermit starten*. Dieses Programm läuft dann
als eigener Prozess neben dem Emulator weiter. Das DiskTool öffnet die Platten und Disketten der WEGA
(tar-Disketten, Dateisystem), mit dem Emulator zusammen sind es die drei Werkzeuge der WEGA-Arbeit.

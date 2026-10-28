# K1520 Kermit — Handbuch

Version {{VERSION}}

K1520 Kermit überträgt Dateien zwischen diesem Rechner und einem anderen Rechner, auf dem ebenfalls
ein Kermit-Programm läuft. Gedacht ist es vor allem für die **WEGA im P8000-Emulator** (`p8000emu`).
Es arbeitet aber mit jedem Kermit zusammen, der über **Telnet** oder **RFC 2217** erreichbar ist.

Das Programm kennt zwei Rollen:

* Als **Client** stellt es Anfragen an einen Kermit-Server: Verzeichnis zeigen, Dateien holen,
  Dateien schreiben, Dateien löschen.
* Als **Server** gibt es einen Ordner dieses Rechners frei. Ein Kermit auf der Gegenseite holt
  von dort Dateien oder legt welche ab.

Wo was steht:

* Die ersten Schritte stehen in *Der übliche Weg*.
* Was Kermit ist und wie es arbeitet, erklärt *Was ist Kermit*.
* Wie ein CP/M-Rechner mit seinem Kermit-80 Dateien austauscht (mit diesem Programm oder mit der WEGA),
  steht in *Kermit-80 auf den CP/M-Rechnern*.
* Wie man Kermit auf der WEGA einrichtet, beschreibt *Kermit Einrichtung unter WEGA*. Auf der
  mitgelieferten WEGA-3.1-Platte ist das schon erledigt.
* Wenn etwas nicht klappt, hilft *Fehlersuche*.

## Der übliche Weg

Die WEGA soll Dateien liefern und annehmen. Dafür braucht man die mitgelieferte WEGA-3.1-Platte
(`p8000_wega31`); auf ihr ist Kermit fertig eingerichtet.

1. Im `p8000emu` WEGA von der Platte starten und warten, bis `WEGA login:` an der Konsole steht.
2. Im `p8000emu` unter *Einstellungen ▸ Schnittstellen* den Block **tty0** auf *Telnet* und *Server*
   stellen, Port **5000**, *Starten*.
3. In K1520 Kermit *Verbindung ▸ Als Client verbinden…* wählen, die Voreinstellung
   **WEGA 3.1 (Benutzer kermit)** nehmen und *Verbinden* drücken.
4. Das Programm meldet sich selbst an. Nach einigen Sekunden zeigt die linke Seite die Dateien
   der WEGA. Dateien mit Doppelklick, mit den Pfeilknöpfen oder durch Ziehen übertragen.
5. Zum Schluss *Verbindung ▸ Server der Gegenstelle beenden (Finish)* wählen. Die WEGA meldet sich
   danach wieder mit `login:`.

Die Einzelheiten stehen in *Fall 1 — die WEGA ist der Server*. Den umgekehrten Weg (die WEGA holt
Dateien von diesem Rechner) beschreibt *Fall 2 — die WEGA ist der Client*.

## Was ist Kermit

**Herkunft.** Kermit ist ein Verfahren zur Dateiübertragung, ein sogenanntes **Protokoll**. Es
entstand 1981 an der Columbia University in New York, benannt nach dem Frosch aus der Muppet Show.
Ziel war, Dateien zwischen ganz unterschiedlichen Rechnern auszutauschen: Großrechnern,
Minicomputern und den ersten Mikrorechnern. Kermit-Programme gibt es deshalb für fast jedes System.
Auf der WEGA-Platte liegt **C-Kermit 4.2**, eine Fassung für UNIX-Systeme aus dem Jahr 1985.

**Die Idee.** Kermit verschickt nur Zeichen, die jede Leitung unverändert durchlässt: druckbare
ASCII-Zeichen. Es braucht keine besondere Hardware und keine Flusssteuerung. Deshalb läuft es über
eine einfache serielle Leitung, über ein Modem oder, wie hier, über eine Telnet-Verbindung zum
Emulator.

**Pakete.** Kermit zerlegt jede Übertragung in kleine **Pakete**. Ein Paket enthält:

* ein Startzeichen,
* seine Länge,
* eine laufende Nummer,
* einen Typ (was das Paket bedeutet),
* die eigentlichen Daten,
* eine Prüfsumme.

Die wichtigsten Paketarten sieht man im Protokollfenster des Programms:

| Typ | Bedeutung |
|-----|-----------|
| S, I | Beginn: beide Seiten tauschen ihre Fähigkeiten aus |
| F | Dateiname |
| D | Daten |
| Z | Ende der Datei |
| B | Ende der Übertragung |
| Y, N | Quittung (in Ordnung) bzw. Bitte um Wiederholung |
| E | Fehler, mit einem Text der Gegenstelle |
| R, G | Anfragen eines Clients an einen Server |

**Quittung und Wiederholung.** Der Empfänger bestätigt jedes Paket. Ist ein Paket beschädigt, bittet
er um Wiederholung. Bleibt die Antwort aus, wartet der Absender eine bestimmte Zeit (die **Frist**)
und schickt das Paket noch einmal. Erst nach mehreren vergeblichen Versuchen gibt er auf und meldet
einen Fehler. Das nächste Paket geht immer erst nach der Quittung hinaus. Das macht Kermit sicher,
aber nicht schnell.

**Prüfsumme.** An der Prüfsumme erkennt der Empfänger, ob ein Paket unterwegs verfälscht wurde.
Kermit kennt drei Arten: Typ 1 (6 Bit), Typ 2 (12 Bit) und Typ 3 (CRC-16, die sicherste). Beide Seiten
einigen sich zu Beginn auf eine Art.

**Quotierung.** Zeichen, die eine Leitung verändern oder verschlucken könnte, ersetzt Kermit durch
ein Präfix und ein druckbares Zeichen. Das nennt man **Quotierung**:

* **Steuerzeichen** bekommen das Präfix `#`. Aus einem Wagenrücklauf (CR) wird zum Beispiel `#M`.
* **Bytes mit gesetztem achtem Bit** (Werte über 127) bekommen das **8-Bit-Präfix** `&`. So überstehen auch
  Binärdaten eine Leitung, die nur sieben Bit überträgt.
* **Gleiche Bytes hintereinander** fasst ein Wiederholungszähler mit dem Präfix `~` zusammen.

**Aushandlung.** Im ersten Paket nennt jede Seite, was sie kann: Paketlänge, Prüfsumme, Präfixe.
Danach gilt, was beide beherrschen. Normale Pakete sind höchstens 94 Zeichen lang. Längere Pakete
(**Langpakete**) kennen nur neuere Kermit-Fassungen; C-Kermit 4.2 kennt sie nicht.

**Text und Binär.** Im **Binärmodus** kommt jede Datei Byte für Byte an. Im **Textmodus** werden die
Zeilenenden angepasst: UNIX und dieser Rechner schreiben LF, auf der Leitung steht CR LF. Programme
und Archive müssen binär übertragen werden, sonst sind sie danach unbrauchbar.

**Client und Server.** Ein **Server** wartet auf Anweisungen und führt sie aus: Dateien annehmen und
liefern, das Verzeichnis zeigen, das Verzeichnis wechseln, Dateien löschen, sich beenden. Der
**Client** gibt diese Anweisungen. Jedes Kermit-Programm kann beide Rollen übernehmen. Wer welche
Rolle spielt, legt der Anwender fest.

## So arbeitet K1520 Kermit

**Bibliothek und Fenster.** Das Protokoll steckt in einer eigenen Programmbibliothek
(`libk1520kermit`). Diese Bibliothek benutzen das Fenster und das Kommandozeilenprogramm
`k1520kermit` gemeinsam. Das Fenster lässt sie in einem eigenen Arbeitsfaden laufen. Deshalb bleibt es
auch während einer Übertragung bedienbar, und jede Übertragung lässt sich abbrechen.

**Die Leitung.** Zum Emulator führt eine **Telnet**-Verbindung oder eine **RFC-2217**-Verbindung.
RFC 2217 ist Telnet mit zusätzlichen Befehlen für Baudrate und Zeichenformat einer seriellen Leitung.
Das Programm überträgt alle 256 Bytewerte unverändert.

Die Verbindung kann auf zwei Arten entstehen:

* **Verbinden:** Das Programm baut die Verbindung zum Emulator auf. Im Emulator ist der Kanal dann
  ein *Telnet-Server*.
* **Lauschen:** Das Programm wartet, bis sich der Emulator meldet. Im Emulator ist der Kanal dann ein
  *Telnet-Client*.

Welche Seite verbindet, hat nichts damit zu tun, wer Kermit-Client und wer Kermit-Server ist.

**Anmeldung.** Auf der WEGA wartet vor dem Kermit-Server eine Anmeldung (`login:`). Das Programm
erledigt sie mit einem kleinen **Login-Skript**: Es sendet RETURN, bis `login:` erscheint, dann den
Benutzernamen und das Kennwort. Danach wartet es, bis der Kermit-Server startet. Die Anmeldezeile der
WEGA kommt manchmal mit gesetztem achtem Bit an (Parität). Das Skript vergleicht deshalb nur die
unteren sieben Bit.

**Das Kennwort** gilt ohne weiteres nur für die laufende Sitzung. Nur mit dem Haken *Zugangsdaten für
diese Verbindung speichern* schreibt das Programm Benutzername und Kennwort **im Klartext** in die
Konfigurationsdatei `kermit.yaml`, und zwar in den Eintrag dieser Verbindung (`client.login_name` und
`client.kennwort`, ebenso in der Liste `zuletzt`). Die Datei ist **nicht geschützt**. Das Kennwort dient
nur der Anmeldung an dieser Gegenstelle (der getty der WEGA), nicht dem Schutz von Daten.

Für das Speichern gilt:

* Es geschieht nur mit gesetztem Haken und erst, nachdem die Anmeldung gelungen ist.
* Ein späteres Speichern überschreibt die alten Werte.
* Die Voreinstellung *WEGA 3.1 (Benutzer kermit)* trägt `kermit` / `kermit` nur für die Sitzung ein.
  Gespeichert wird erst, wenn der Haken gesetzt ist.

**Maschinenzeit.** Der Emulator schickt die Zeichen einer seriellen Leitung in **Maschinenzeit**,
also im Takt der nachgebildeten Maschine. Läuft der Emulator fünfmal so schnell wie das echte Gerät,
kommen auch die Zeichen fünfmal so schnell. Läuft er unter Last langsamer, kommen sie langsamer. Das
Programm misst dieses Tempo selbst und passt seine Fristen daran an (siehe *Einstellungen*,
„Geschwindigkeit der Gegenstelle").

**Sicherheit im Server-Betrieb.** Der freigegebene Ordner ist die Grenze für die Gegenstelle. Pfade,
die aus ihm hinausführen, weist das Programm ab: `..` oberhalb des Ordners, absolute Pfade,
Laufwerksbuchstaben und Verknüpfungen nach außen. Befehle zum Ausführen von Programmen nimmt der
Server nicht an.

## Das Fenster

Links steht der **Server**, rechts ein **Ordner dieses Rechners**. Über dem rechten Ordner stehen eine
Adresszeile und ein Knopf zur Ordnerwahl; die Rücktaste geht eine Ebene höher. Zwischen den beiden
Seiten stehen die Knöpfe für die beiden Richtungen:

* **Holen** (Pfeil nach rechts) kopiert die markierten Dateien des Servers in den Ordner rechts.
* **Schreiben** (Pfeil nach links) kopiert die markierten Dateien von rechts zum Server.
* **Auf dem Server löschen** entfernt die markierten Dateien des Servers. Vorher fragt das Programm
  nach.

Ein Doppelklick auf einen Ordner oder auf `..` wechselt das Verzeichnis. Ein Doppelklick auf eine
Datei überträgt sie auf die andere Seite. Dateien lassen sich auch von einer Seite auf die andere
ziehen, ebenso aus dem Dateimanager auf die linke Seite. Ordner überträgt Kermit nicht, nur Dateien.

**Im Client-Betrieb** zeigt die linke Seite Name, Größe, Datum und Rechte jeder Datei. Diese Angaben
stammen aus der Ausgabe von `ls -l` auf dem Server. Der Pfad über der Liste gilt ab dem Verzeichnis,
in dem die Sitzung begonnen hat.

**Im Server-Betrieb** zeigt die linke Seite stattdessen die **Übertragungen und Ereignisse**: je
Datei eine Zeile mit Byte-Zähler und Zustand (*läuft*, *fertig*), dazu die Meldungen der Sitzung.

**Fortschritt und Abbruch.** Während einer Übertragung erscheint ein Fortschrittsfenster.
*Abbrechen* beendet die Übertragung geordnet; die Verbindung bleibt danach bestehen. Antwortet die
Gegenstelle nicht mehr, beendet ein zweiter Klick (*Hart abbrechen*) die ganze Sitzung.

**Meldungen.** Fehler und Hinweise erscheinen in einem Streifen über den beiden Seiten. Dort kann man
sie auch schließen. Alles, was geschieht, steht im **Protokoll** (*Ansicht ▸ Protokoll*), auch der
Fehlertext der Gegenstelle.

**Statuszeile.** Sie zeigt den Zustand: *Getrennt*, *Verbunden mit …*, *Lauscht auf Port …* oder
*Übertragung: …*. Im Server-Betrieb steht *Server:* davor. Rechts steht die Übertragungsart.

**Symbolleiste.** *Ansicht ▸ Symbolleiste* blendet sie ein oder aus.

## Verbindung herstellen (Client)

*Verbindung ▸ Als Client verbinden…* öffnet den Verbindungsdialog. Die Felder:

* **Voreinstellung:** *WEGA 3.1 (Benutzer kermit)* füllt alle Felder für die mitgelieferte
  WEGA-Platte aus: Telnet, Verbinden, `127.0.0.1`, Port 5000, Anmeldung als `kermit` mit dem
  Kennwort `kermit`. Außerdem setzt sie die WEGA-Werte der Übertragung (siehe *Einstellungen*).
  Darunter stehen die zuletzt benutzten Verbindungen.
* **Leitung:** *Telnet* oder *RFC 2217*.
* **Aufbau:** *Verbinden* (das Programm verbindet sich mit dem Emulator) oder *Lauschen* (der
  Emulator verbindet sich mit dem Programm). Beim Lauschen bedeutet Port 0: einen freien Port nehmen.
  Er steht dann in der Statuszeile.
* **Host** und **Port:** die Adresse des Emulators. Der Host gilt nur beim Verbinden.
* **Baud-Vorschlag:** nur bei RFC 2217. Maßgeblich bleibt die Baudrate, die das Betriebssystem im
  Emulator eingestellt hat.
* **Anmelden (Login-Skript für eine getty):** Haken setzen, wenn vor dem Kermit-Server eine Anmeldung
  wartet. **Ohne diesen Haken meldet sich das Programm nicht an und zeigt auch das Fenster
  *Anmeldung* nicht.**
* **Benutzername** und **Kennwort:** Beides kann hier schon stehen. Bleibt der Benutzername leer, fragt
  das Fenster *Anmeldung*, sobald die Gegenstelle `login:` zeigt.
* **Zugangsdaten für diese Verbindung speichern:** speichert Name und Kennwort im Klartext in der
  `kermit.yaml` (siehe *So arbeitet K1520 Kermit*). Darunter steht ein Hinweis dazu.

**Das Fenster „Anmeldung“.** Verlangt die Gegenstelle eine Anmeldung und sind keine Zugangsdaten bekannt,
öffnet sich das Fenster. Seine Kopfzeile lautet „<Host:Port> verlangt eine Anmeldung.“ Es enthält:

1. **Benutzername** und **Kennwort** (die Eingabe des Kennworts bleibt verdeckt). Darüber steht der
   Hinweis „Möglicherweise ist es einfach kermit / kermit.“ Es ist nur ein Hinweis, die Felder sind leer.
2. Den Haken **Zugangsdaten für diese Verbindung speichern** mit dem Hinweis auf den Klartext.
3. Die Knöpfe **Anmelden** und **Abbrechen**.

Lehnt die Gegenstelle die Eingabe ab, erscheint das Fenster erneut. Es zeigt rot „Anmeldung abgelehnt.
Bitte erneut versuchen.“, und die Felder enthalten die letzte Eingabe. Gespeicherte oder im Verbindungsdialog
eingetragene Zugangsdaten gelten beim ersten Versuch ohne Fenster. Schlagen sie fehl, kommt das Fenster.

*Abbrechen* beendet das Verbinden still und schließt die Leitung. Die Statuszeile zeigt „Verbinden
abgebrochen.“ Ist ein Benutzer ohne Kennwort eingetragen und lehnt die Gegenstelle ab, fragt das Programm
nicht neu, sondern meldet „Verbindung fehlgeschlagen: Anmeldung abgelehnt“.

Ein lauschendes Programm nimmt genau **eine** Verbindung an. Soll es danach weiter warten, *Verbindung ▸
Nach der Trennung wieder lauschen* einschalten.

**Trennen und Beenden.** *Verbindung ▸ Server der Gegenstelle beenden (Finish)* beendet den
Kermit-Server auf der Gegenseite ordentlich. Auf der WEGA endet damit die Sitzung des Benutzers
`kermit`, und `login:` erscheint wieder. *Verbindung ▸ Trennen* schließt nur die Leitung. Der
Kermit-Server auf der WEGA läuft danach weiter (siehe *Fehlersuche*).

## Server-Modus: einen Ordner freigeben

Im Server-Betrieb gibt das Programm einen Ordner dieses Rechners frei. Die Gegenstelle sieht nur
diesen Ordner und seine Unterordner.

1. *Verbindung ▸ Ordner freigeben…* wählen und den Ordner bestimmen. Ohne freigegebenen Ordner
   startet der Server nicht.
2. *Verbindung ▸ Als Server einrichten…* wählen und Leitung, Aufbau und Port eintragen. Für die
   WEGA passt die Voreinstellung *WEGA als Client (hier lauschen, Kermit auf tty3)*: Telnet, Lauschen,
   Port 5001.
3. *Verbindung ▸ Server starten* (Strg+Umschalt+S) wählen.
4. Zum Beenden *Verbindung ▸ Server stoppen* wählen. Das schließt auch die Leitung.

**Anmeldung erzwingen.** Im Dialog *Als Server einrichten…* lässt sich der Server wie die WEGA
absichern:

1. Den Haken **Anmeldung erzwingen** setzen.
2. **Benutzername** und **Kennwort** eintragen. Vorgabe ist `kermit` / `kermit`; beides lässt sich ändern.
   Beide stehen im Klartext in der `kermit.yaml` (Zweig `server.login`). Der Dialog weist darauf hin.

Der Server verhält sich dann wie getty und `login` der WEGA. Nach dem Verbindungsaufbau sendet er `login:`
und danach `Password:` (die Eingabe wird nicht zurückgesendet). Stimmt das Paar, meldet er sich mit „C-Kermit
server starting“ und beginnt zu arbeiten. Sonst folgt `Login incorrect`, und `login:` erscheint wieder.
Nach `finish` oder `logout` des Clients meldet sich der Server erneut mit `login:`. Die Statuszeile zeigt
„Server: … (mit Anmeldung)“.

Auf der Kommandozeile: `k1520kermit serve --login-pflicht NAME[:KENNWORT] …` (ohne Kennwort gilt `kermit`).

Weitere Schalter im Menü *Verbindung*:

* **Nur lesen:** Die Gegenstelle darf Dateien holen, aber nichts schreiben und nichts löschen. Die
  Einstellung gilt ab dem nächsten Start des Servers.
* **Nach der Trennung wieder lauschen:** Der Server wartet nach dem Ende einer Verbindung auf die
  nächste, auch wenn die Gegenstelle ohne `finish` getrennt hat. Ohne diesen Schalter endet der
  Server mit der Verbindung und muss neu gestartet werden.

## Einstellungen

*Ansicht ▸ Einstellungen…* (Strg+Umschalt+E) öffnet die Einstellungen der Übertragung.

* **Übertragungsart:** *Binär* überträgt Byte für Byte (Vorgabe). *Text* passt die Zeilenenden an
  (hier LF, auf der Leitung CR LF). *Automatik nach Endung* überträgt Dateien mit den Endungen `.txt`,
  `.c`, `.h`, `.mac`, `.asm` und `.doc` als Text, alle anderen binär.
* **Namen beim Senden** und **Namen beim Empfangen:** *Literal* lässt den Dateinamen, wie er ist.
  *Konvertiert* schreibt ihn wie C-Kermit um: Großbuchstaben beim Senden, Kleinbuchstaben beim
  Empfangen. Vorgabe: senden literal, empfangen konvertiert.
* **Prüfsumme:** 1, 2 oder 3. Vorgabe ist 3 (CRC-16). Kann die Gegenstelle das nicht, einigen sich
  beide auf eine einfachere Art.
* **Paketlänge:** bis 94 normale Pakete, darüber Langpakete. Die Gegenstelle kann kürzere Pakete
  verlangen.
* **Frist je Paket:** wie lange das Programm auf eine Antwort wartet, bevor es ein Paket wiederholt
  (Vorgabe 10 Sekunden). Die Zeit gilt im Tempo der Gegenstelle. Läuft die Gegenstelle nur halb so
  schnell wie das echte Gerät, wartet das Programm also doppelt so lange.
* **Wiederholungen je Paket:** wie oft ein Paket wiederholt wird, bevor das Programm aufgibt
  (Vorgabe 10).
* **Frist je Anmeldeschritt:** wie lange das Login-Skript auf eine Antwort warten darf (Vorgabe 60
  Sekunden). Jedes ankommende Zeichen startet die Frist neu.
* **Geschwindigkeit der Gegenstelle:** Ein Emulator kann schneller oder langsamer laufen als das
  echte Gerät. *Automatisch* (Vorgabe) misst das Tempo an den ankommenden Zeichen und passt die
  Fristen an. Einen festen Wert (0,1x bis 20x) braucht man nur, wenn die Messung danebenliegt, etwa
  bei einem stark gedrosselten Emulator. Eine schnelle Gegenstelle bekommt keine kürzere Frist; sie
  wiederholt verlorene Pakete selbst.
* **Längster Zielname:** Dateinamen, die länger sind, lehnt das Programm ab, statt sie zu kürzen. Die
  WEGA erlaubt höchstens 14 Zeichen. Vorgabe: keine Grenze.
* **Vorhandene Dateien beim Empfang ersetzen:** Ist der Haken gesetzt (Vorgabe), überschreibt eine
  empfangene Datei eine gleichnamige.
* Der Knopf **WEGA-Werte** setzt Paketlänge 94, Namen höchstens 14 Zeichen und Prüfsumme 3.

Das Programm speichert seine Einstellungen in der Datei `kermit.yaml` im Konfigurationsordner.
Dazu gehören auch Fensterlage, Symbolleiste, der freigegebene Ordner und die zuletzt benutzten
Verbindungen. Kennwörter stehen nie darin.

## Fall 1 — die WEGA ist der Server

Auf der mitgelieferten WEGA-3.1-Platte gibt es einen Benutzer **`kermit`** mit dem Kennwort
**`kermit`**. Wer sich so anmeldet, bekommt keine Shell, sondern sofort einen Kermit-Server. Dieses
Programm ist dann der Client.

1. Im `p8000emu` das Vollgerät mit der Platte `p8000_wega31` starten: Rechner einschalten, RETURN,
   RETURN, bei „Press NMI" den NMI-Taster. WEGA startet dann von selbst. Nach einigen Minuten
   Maschinenzeit steht `WEGA login:` an der Konsole.
2. Im `p8000emu` *Einstellungen ▸ Schnittstellen* öffnen. Im Block **tty0** *Telnet* und *Server*
   wählen, Port **5000** eintragen und *Starten* drücken. Statt tty0 geht auch tty2 oder tty4 bis tty7;
   dann den passenden Port im Verbindungsdialog eintragen. Geprüft ist tty0.
3. In K1520 Kermit *Verbindung ▸ Als Client verbinden…* wählen, Voreinstellung **WEGA 3.1 (Benutzer
   kermit)**, *Verbinden*.
4. Das Login-Skript meldet sich an. Bis der Server bereit ist, vergehen etwa zwölf Sekunden
   Maschinenzeit. Dann steht in der Statuszeile *Verbunden mit …*, und links erscheint das
   Verzeichnis `/usr/kermit`.
5. Dateien holen, schreiben oder löschen. Mit Doppelklick auf einen Ordner oder auf `..` wechselt man
   auf der WEGA das Verzeichnis, zum Beispiel nach `/tmp`.
6. Zum Schluss *Verbindung ▸ Server der Gegenstelle beenden (Finish)*. Die WEGA zeigt danach wieder
   `login:`.

Was man über die WEGA als Server wissen sollte:

* **Dateinamen:** Die WEGA verschickt Namen in Großbuchstaben. Mit der Vorgabe *Namen beim Empfangen:
  konvertiert* kommen sie hier klein an. Dateien, die die WEGA empfängt, schreibt sie mit kleinen
  Buchstaben.
* **Namenslänge:** WEGA-Namen haben höchstens 14 Zeichen. Mit den WEGA-Werten lehnt das Programm
  längere Namen ab. Ohne diese Grenze kürzt die WEGA den Namen stillschweigend.
* **Rechte:** Empfangene Dateien gehören auf der WEGA dem Benutzer `wega` und haben den Modus 666.
* **Tempo:** Die Leitung läuft mit 9600 Baud. Gemessen wurden 270 bis 430 Byte je Sekunde
  Maschinenzeit. Eine Datei von 100 KB braucht also rund sechs Minuten Maschinenzeit. Läuft der
  Emulator ungebremst (etwa fünf- bis sechsmal so schnell wie das Gerät), ist sie nach gut einer Minute
  da.

## Fall 2 — die WEGA ist der Client

Hier ist dieses Programm der Server, und die WEGA holt oder bringt Dateien. Die WEGA benutzt dafür
die Leitung **tty3**. Das ist am P8000 die Druckerleitung; auf ihr wartet keine Anmeldung.

1. In K1520 Kermit *Verbindung ▸ Ordner freigeben…* wählen und einen Ordner bestimmen.
2. *Verbindung ▸ Als Server einrichten…*, Voreinstellung *WEGA als Client (hier lauschen, Kermit auf
   tty3)*. Das Programm lauscht dann per Telnet auf Port 5001.
3. *Verbindung ▸ Server starten* (Strg+Umschalt+S).
4. Im `p8000emu` *Einstellungen ▸ Schnittstellen* öffnen. Im Block **tty3** *Telnet* und *Client*
   wählen, Host `127.0.0.1` und Port **5001** eintragen, *Starten*.
5. An der WEGA-Konsole als `wega` anmelden und Kermit aufrufen:

   ```
   kermit
   C-Kermit>set line /dev/tty3
   C-Kermit>set speed 9600
   C-Kermit>set file type binary
   C-Kermit>send datei
   C-Kermit>get datei
   C-Kermit>remote directory
   C-Kermit>finish
   C-Kermit>exit
   ```

   `send` schickt eine Datei an dieses Programm, `get` holt eine aus dem freigegebenen Ordner.
   `send datei anderername` schickt sie unter einem anderen Namen. `remote directory` zeigt den
   freigegebenen Ordner. `finish` beendet den Server dieses Programms, `exit` beendet Kermit auf der
   WEGA.

Hinweise:

* Ohne `set file type binary` überträgt C-Kermit 4.2 im Textmodus. Eine Textdatei kommt dann mit CR LF
  an. Beim Empfang im Textmodus verwirft C-Kermit 4.2 jedes CR.
* `remote cwd ordner` wechselt auf dem Server das Verzeichnis. C-Kermit fragt dabei nach einem
  Kennwort; einfach RETURN drücken.
* Geprüft ist dieser Ablauf an der Konsole. An tty0, tty2 oder tty4 bis tty7 sollte er genauso gehen;
  das ist nicht geprüft.

## Kermit-80 auf den CP/M-Rechnern

Auf den mitgelieferten Systemdisketten liegt die Datei **`KERMIT.COM`**. Das ist *Kermit-80*, das
Kermit-Programm für CP/M. Damit tauscht ein emulierter CP/M-Rechner Dateien mit diesem Programm oder mit
der WEGA aus. Kermit-80 ist nur **Client**: Es sendet Dateien, holt Dateien und fragt einen Server, aber es
kann selbst keinen Server abgeben. Auf der Gegenseite braucht es also immer einen Server.

Wo `KERMIT.COM` liegt, und welche Schnittstelle es benutzt:

| Rechner (Diskette) | Schnittstelle im Emulator |
|---|---|
| A5120 (CP/A und SCPX) | **DFÜ/V.24** |
| K8915 (SCPX 8915) | **V.24** (X4) |
| PC 1715 (SCP 1715, CP/A 1715) | **V.24** (X5) |
| PC 1715W (SCP 3.0) | **V.24** (X5) |
| PRG 710 und 710-1 (SCPX) | **V.24** (X4) |
| P8000, 8-Bit-Teil (OS/M) | **tty0** |

Jeder Rechner hat seine eigene Fassung. Sie stellt die Leitung selbst auf **9600 Baud, 8 Bit, keine Parität,
1 Stoppbit** ein; einstellen muss man nichts. Auf CP/Z und UDOS gibt es Kermit-80 nicht.

**Starten.** Am Prompt des Betriebssystems:

```
A>kermit
Kermit-80 v4.11 configured for A5120 (K8025 A33-A) with Generic (Dumb) CRT Terminal type selected
Kermit-80   0A:>
```

`Kermit-80   0A:>` ist die Eingabeaufforderung; die Laufwerkskennung davor kann anders lauten. Alle
Befehle unten tippt man dort. Mit `?` zeigt Kermit-80 an jeder Stelle, was möglich ist. Beenden mit `exit`.

### Mit dem PC-Server von K1520 Kermit

Dieses Programm ist der Server, der CP/M-Rechner der Client. Die Leitung läuft über Telnet, wie in
*Fall 2 — die WEGA ist der Client*.

1. In K1520 Kermit *Verbindung ▸ Ordner freigeben…* wählen und einen Ordner bestimmen.
2. *Verbindung ▸ Als Server einrichten…* wählen. Die Voreinstellung *WEGA als Client (hier lauschen,
   Kermit auf tty3)* passt: Telnet, Lauschen, Port **5001**.
3. *Verbindung ▸ Server starten* (Strg+Umschalt+S).
4. Im Emulator *Einstellungen ▸ Schnittstellen* öffnen. Beim Block der Schnittstelle aus der Tabelle oben
   (A5120: **DFÜ/V.24**) *Telnet* und *Client* wählen, Host `127.0.0.1`, Port **5001**, *Starten*.
5. Im CP/M-Rechner `kermit` aufrufen und die Befehle von unten eingeben.

Die Leitung dieses Wegs ist **8-Bit-rein**. Kermit-80 braucht deshalb keine besondere Einstellung der
Parität (Vorgabe `none`).

**Textdateien (7 Bit).** Eine CP/M-Textdatei endet mit einem Ctrl-Z (1AH); der Rest des letzten
128-Byte-Satzes ist mit Ctrl-Z aufgefüllt.

```
Kermit-80   0A:>set file-mode ascii
Kermit-80   0A:>send brief.txt
```

`set file-mode ascii` sendet nur bis zum ersten Ctrl-Z. Die Zeilenenden CR LF bleiben unverändert. Der
Server legt die Datei im freigegebenen Ordner an, mit kleinen Buchstaben (`brief.txt`).

**Binärdateien (8 Bit).**

```
Kermit-80   0A:>set file-mode binary
Kermit-80   0A:>send programm.com
Kermit-80   0A:>get daten.bin
```

`set file-mode binary` schickt alle 128-Byte-Sätze einer Datei vollständig, auch den letzten. Alle 256
Bytewerte kommen unverändert an. Holt Kermit-80 eine Datei, deren Länge kein Vielfaches von 128 ist,
füllt es den Rest des letzten Satzes mit Ctrl-Z auf, weil CP/M keine Bytelänge kennt. Die Füllbytes
gehören nicht zur Datei.

Weitere Befehle am Prompt:

| Befehl | Wirkung |
|---|---|
| `send datei` | Datei zum Server schicken. Mit Platzhaltern: `send *.com` |
| `get datei` | Datei vom Server holen. Sie landet auf dem Laufwerk des Prompts |
| `remote directory` | Verzeichnis des freigegebenen Ordners anzeigen |
| `remote delete datei` | Datei auf dem Server löschen (nicht bei *Nur lesen*) |
| `set block-check-type 3-character-crc-ccitt` | die stärkste Blockprüfung einstellen (auch `1-character-checksum`, `2-character-checksum`) |
| `finish` | den Server beenden |
| `exit` | Kermit-80 verlassen |

Dateinamen kommen in Großbuchstaben aus CP/M. Mit der Vorgabe *Namen beim Empfangen: konvertiert* legt der
Server sie klein an, zum Beispiel `programm.com`.

**Mit Anmeldung.** Der Server kann wie die WEGA eine Anmeldung verlangen (siehe *Server-Modus*, *Anmeldung
erzwingen*). Kermit-80 hat keinen Anmeldeablauf; man meldet sich im Terminalmodus von Hand an, wie im
Abschnitt *Mit der WEGA als Server*:

1. In K1520 Kermit den Server mit gesetztem Haken *Anmeldung erzwingen* starten.
2. Am CP/M-Rechner `kermit` aufrufen und `set file-mode binary` eingeben.
3. `connect` eingeben und RETURN drücken, bis `login:` erscheint.
4. Den Benutzernamen eingeben. Bei `Password:` das Kennwort eingeben; es wird nicht angezeigt.
5. Nach der Meldung „C-Kermit server starting“ mit **Strg+\** und dann `c` zum Prompt zurückkehren.
6. `remote directory`, `get`, `send` und zuletzt `finish` wie oben.

Nach `finish` zeigt ein neues `connect` wieder `login:`.

### Mit der WEGA als Server

Hier ist der P8000 mit WEGA der Server, der CP/M-Rechner der Client. Man braucht zwei Emulatoren
nebeneinander: den P8000 (`p8000emu`) und den CP/M-Rechner. Die Platte `p8000_wega31` hat den Benutzer
`kermit` schon (siehe *Fall 1 — die WEGA ist der Server*).

1. Im `p8000emu` WEGA starten, bis `WEGA login:` an der Konsole steht.
2. Im `p8000emu` *Einstellungen ▸ Schnittstellen*: Block **tty0** auf *Telnet* und *Server*, Port **5000**,
   *Starten*.
3. Im Emulator des CP/M-Rechners *Einstellungen ▸ Schnittstellen*: bei der Schnittstelle aus der Tabelle
   oben *Telnet* und *Client*, Host `127.0.0.1`, Port **5000**, *Starten*.
4. Im CP/M-Rechner `kermit` aufrufen und anmelden. Kermit-80 kennt kein Login-Skript.
   Man meldet sich im Terminalmodus von Hand an:

```
Kermit-80   0A:>set file-mode binary
Kermit-80   0A:>connect
[Connected to remote host.  Type Control-\C to return;
 type Control-\? for command list]
```

   Jetzt gehen alle Tasten an die WEGA. RETURN drücken, bis `login:` erscheint. Dann `kermit` und RETURN,
   bei `Password:` wieder `kermit` und RETURN. Nach einigen Sekunden meldet sich der Server:

```
C-Kermit server starting.  Return to your local machine by typing
its escape sequence for closing the connection, and issue further
commands from there.  To shut down the C-Kermit server, issue the
FINISH command and then reconnect.
```

5. Zurück zu Kermit-80: **Strg+\** drücken (die Taste Strg gedrückt halten und das Zeichen `\` tippen),
   dann den Buchstaben **`c`**. Kermit-80 meldet `[Connection closed, back at micro]` und zeigt wieder
   die Eingabeaufforderung. Das **Fluchtzeichen** ist in dieser Fassung Strg+\; Strg+] geht **nicht**.
   Zweimal hintereinander gedrückt, geht das Fluchtzeichen selbst an die WEGA.
6. Der Server der WEGA läuft jetzt. Befehle wie oben, mit den Besonderheiten unten.
7. Zum Schluss `finish`. Der Server der WEGA endet, getty meldet sich wieder mit `login:`.

```
Kermit-80   0A:>remote directory
Kermit-80   0A:>get text.txt
Kermit-80   0A:>send brief.txt
Kermit-80   0A:>finish
```

`remote directory` zeigt das Verzeichnis `/usr/kermit` der WEGA im Format von `ls -l`. Die WEGA
legt empfangene Dateien dort mit kleinen Buchstaben an.

**Textdateien (7 Bit) mit der WEGA.** Die Leitung tty0 der WEGA löscht auf dem Weg zur WEGA das achte
Bit. Reiner Text kommt trotzdem an:

```
Kermit-80   0A:>set file-mode binary
Kermit-80   0A:>get text.txt
```

Mit `set file-mode binary` bleibt jedes Byte, wie es die WEGA liefert. Die WEGA beendet Zeilen mit LF
allein, nicht mit CR LF. Ein CP/M-Editor erwartet CR LF; eine so geholte Datei muss also bei Bedarf
umgewandelt werden. Beim Senden zur WEGA bleiben die CR LF der CP/M-Datei stehen. Mit `set file-mode ascii` gehen nur die Zeichen bis
zum ersten Ctrl-Z hinaus, ohne die Füllbytes des letzten Satzes (gegen den PC-Server geprüft).

**Binärdateien (8 Bit) mit der WEGA.** Wegen des gelöschten achten Bits muss Kermit-80 das Bit **in das
Datenpaket verpacken** („8th bit prefixing“). Das tut es nur, wenn eine Parität eingestellt ist. Vor der
Übertragung also:

```
Kermit-80   0A:>set parity space
Kermit-80   0A:>set file-mode binary
Kermit-80   0A:>get programm.bin
Kermit-80   0A:>send programm.com
```

Mit eingestellter Parität setzt Kermit-80 das achte Bit jedes gesendeten Zeichens selbst (bei `space` auf 0)
und verpackt Datenbytes ab 80H in ein Präfix. Auch `mark`, `even` und `odd` lösen die Verpackung aus; gegen die
WEGA geprüft ist `space`. Dann kommen alle 256 Bytewerte unverändert an, in beiden Richtungen. **Ohne**
eingestellte Parität verliert jedes Byte ab 80H sein achtes Bit; die Datei ist danach unbrauchbar. Hat die
Gegenseite die Verpackung nicht eingeschaltet, warnt Kermit-80 am Bildschirm.

Die Parität gilt auch für die Terminalverbindung (`connect`). Wer sie für die Übertragung einstellt, kann sie
danach mit `set parity none` zurücknehmen.

### Wenn etwas nicht klappt

* **Kermit-80 meldet sofort einen Fehler oder wartet endlos bei `Starting ...`:** Die Leitung steht nicht.
  Im Emulator *Einstellungen ▸ Schnittstellen* prüfen: Block gestartet, Host und Port richtig, Status
  *Verbunden*. Bei der WEGA zuerst `connect` und prüfen, ob `login:` erscheint.
* **Nach `connect` passiert nichts:** Bei der WEGA RETURN drücken; getty antwortet erst auf einen
  Zeilenumbruch.
* **Strg+\ kommt nicht an:** Ob das Zeichen `\` mit gedrückter Strg-Taste im Emulator ankommt, hängt von der
  Tastaturbelegung ab (an der deutschen Tastatur liegt `\` auf AltGr+ß). Das ist nicht geprüft.
* **Binärdatei von der WEGA ist beschädigt:** `set parity space` vor `get` und `send` vergessen.
* **Eine Datei mit dem Namen gibt es auf dem CP/M-Rechner schon:** `set collision` legt fest, was Kermit-80
  mit einer ankommenden Datei gleichen Namens tut: `backup`, `discard`, `overwrite` oder `rename`.

## Kermit Einrichtung unter WEGA

Dieser Abschnitt richtet Kermit auf einer WEGA-3.1-Platte ein. Auf der mitgelieferten Platte
`p8000_wega31_platte.img.gz` (im Paket im Ordner `p8000_wega31`) ist das schon geschehen. Wer diese
Platte benutzt, braucht nur den Schnellstart. Die übrigen Unterabschnitte erklären, was auf der Platte
eingerichtet ist und wie man das auf einer anderen Platte selbst einrichtet.

### Schnellstart

**WEGA als Server** (dieses Programm holt und bringt Dateien):

1. `p8000emu` mit der Platte `p8000_wega31` starten, warten bis `WEGA login:` erscheint.
2. *Einstellungen ▸ Schnittstellen*, Block **tty0**: *Telnet*, *Server*, Port 5000, *Starten*.
3. In K1520 Kermit *Verbindung ▸ Als Client verbinden…*, Voreinstellung *WEGA 3.1 (Benutzer kermit)*,
   *Verbinden*.
4. Dateien übertragen.
5. *Verbindung ▸ Server der Gegenstelle beenden (Finish)*.

**WEGA als Client** (die WEGA holt und bringt Dateien):

1. In K1520 Kermit *Verbindung ▸ Ordner freigeben…*, dann *Verbindung ▸ Als Server einrichten…* mit der
   Voreinstellung *WEGA als Client (hier lauschen, Kermit auf tty3)*, dann *Server starten*.
2. Im `p8000emu` *Einstellungen ▸ Schnittstellen*, Block **tty3**: *Telnet*, *Client*, Host
   `127.0.0.1`, Port 5001, *Starten*.
3. An der WEGA-Konsole: `kermit`, `set line /dev/tty3`, `set speed 9600`, `set file type binary`,
   dann `send`, `get`, `remote directory`, zum Schluss `finish` und `exit`.

### Was eingerichtet wird

Damit die WEGA als Kermit-Server arbeitet, braucht sie fünf Dinge:

| Was | Ort | Rechte und Besitzer |
|-----|-----|---------------------|
| das Programm C-Kermit 4.2 | `/usr/bin/kermit` | Modus 4755, Besitzer `wega` (uid 0), Gruppe 0 |
| die Handbuchseite | `/usr/man/man1/kermit.1` | Modus 644 |
| der Heimatordner des Benutzers `kermit` | `/usr/kermit` | Modus 755, uid 7, gid 1 |
| die Anmeldedatei | `/usr/kermit/.profile` | Modus 644, uid 7, gid 1 |
| die Startdatei von Kermit | `/usr/kermit/.kermrc` | Modus 644, uid 7, gid 1 |

Dazu kommt eine Zeile in `/etc/passwd`:

```
kermit:KmhrRNsdxD7LY:7:1::/usr/kermit:/bin/sh
```

Die Felder bedeuten:

* `kermit` ist der Benutzername.
* `KmhrRNsdxD7LY` ist das verschlüsselte Kennwort `kermit`.
* `7` ist die Benutzernummer (uid). Sie war auf der Platte frei.
* `1` ist die Gruppe `other`.
* `/usr/kermit` ist der Heimatordner.
* `/bin/sh` ist die Shell.

`/usr/kermit/.profile` enthält eine einzige Zeile:

```
exec /usr/bin/kermit
```

`/usr/kermit/.kermrc` enthält drei Zeilen:

```
set file type binary
server
exit
```

**So läuft eine Anmeldung ab.** Nach dem Kennwort startet die Shell `/bin/sh`. Sie liest `.profile`
und ersetzt sich durch Kermit. Kermit liest beim Start `.kermrc`: Es schaltet auf Binärmodus und
startet den Server. Beendet der Client den Server mit *Finish*, führt Kermit die nächste Zeile
`exit` aus. Damit endet die Sitzung, und die WEGA zeigt wieder `login:`.

**Warum es genau so sein muss:**

* **Ein Kennwort ist Pflicht.** Hat ein Benutzer kein Kennwort, verlangt der `login` der WEGA bei der
  Anmeldung ein neues (`You don't have a password. Choose one.`). Ein Login-Skript kommt dann nicht
  weiter. Das Kennwort `kermit` ist für einen Laborrechner gedacht. Hängt die Maschine an einem Netz,
  sollte man es mit `passwd kermit` ändern und im Verbindungsdialog das neue Kennwort eintragen.
* **Die Shell muss `/bin/sh` sein, nicht `/usr/bin/kermit`.** C-Kermit führt die Befehle des Clients
  zum Anzeigen und Löschen über die Shell aus, die in `/etc/passwd` steht. Wäre das Kermit selbst,
  erschiene statt des Verzeichnisses die Gebrauchsanweisung von Kermit.
* **In `.kermrc` darf `set file names literal` nicht stehen.** Mit dieser Zeile gibt C-Kermit 4.2
  wegen eines Fehlers im Programm keine Datei mehr heraus; jede Anfrage endet mit „Can't open file".
  Ohne die Zeile verschickt der Server die Namen in Großbuchstaben. Das Programm schreibt sie beim
  Empfang wieder klein (Einstellung *Namen beim Empfangen: konvertiert*).
* **`set file type binary`** sorgt dafür, dass der Server Dateien unverändert liefert. Ohne die
  Zeile wäre Text die Vorgabe.
* **`/usr/bin/kermit` ist setuid root** (Modus 4755), wie auf der Diskette. Kermit braucht das für
  seine Sperrdateien in `/usr/spool/uucp`.

### Die Kermit-Disketten

Kermit kommt von der Diskette `kermit.cqm`. Sie liegt dem Emulator nicht bei. Die Diskette enthält
kein Dateisystem, sondern ein `tar`-Archiv (Format `tar720`). Alles liegt im Ordner `kermit/`:

* `kermit/kermit` — das fertige Programm für den Z8000: C-Kermit 4.2(030), 78 560 Byte, vom
  Februar 1988. Es läuft unter WEGA 3.1 ohne Änderung.
* `kermit/ckermi.nr` — die Handbuchseite im Format von `nroff`.
* die Quelltexte (`ck*.c`, `makefile`) und weitere Beschreibungen. Sie werden nicht gebraucht.

Eine zweite Diskette, `kermit4.cqm`, enthält nur gepackte Quelltexte. Auch sie wird nicht gebraucht.

Es gibt zwei Wege, Kermit auf eine Platte zu bringen. Der erste ist geprüft, der zweite nicht.

### Einrichten mit dem DiskTool (geprüft)

So ist die mitgelieferte Platte entstanden. Der Emulator muss dabei ausgeschaltet sein oder die Platte
abgetrennt haben, denn er schreibt die Platte sonst mit seinem eigenen Stand zurück. Das DiskTool
legt vor der ersten Änderung eine Sicherung `<abbild>~` an.

In den Befehlen steht `platte.img.gz` für die Plattendatei und `k1520disktool` für das
Kommandozeilenprogramm des DiskTool.

1. Die beiden Dateien von der Diskette holen:

   ```
   k1520disktool get kermit.cqm kermit/kermit kermit/ckermi.nr --to auszug
   ```

   Sie liegen danach in `auszug/kermit/`.

2. Programm und Handbuchseite auf die Platte bringen und die Rechte setzen. `md0` ist das
   Dateisystem, das unter WEGA als `/usr` erscheint:

   ```
   k1520disktool put platte.img.gz auszug/kermit/kermit --as md0/bin/kermit
   k1520disktool attr platte.img.gz md0/bin/kermit --mode 4755 --uid 0 --gid 0
   k1520disktool put platte.img.gz auszug/kermit/ckermi.nr --as md0/man/man1/kermit.1
   ```

3. Den Heimatordner anlegen:

   ```
   k1520disktool mkdir platte.img.gz md0/kermit
   k1520disktool attr platte.img.gz md0/kermit --mode 0755 --uid 7 --gid 1
   ```

4. Auf diesem Rechner zwei Textdateien `profile` und `kermrc` mit dem Inhalt aus *Was eingerichtet
   wird* anlegen. Die Zeilen müssen mit LF enden, nicht mit CR LF (unter Windows beim Speichern
   darauf achten). Dann:

   ```
   k1520disktool put platte.img.gz profile --as md0/kermit/.profile
   k1520disktool put platte.img.gz kermrc --as md0/kermit/.kermrc
   k1520disktool attr platte.img.gz md0/kermit/.profile --uid 7 --gid 1
   k1520disktool attr platte.img.gz md0/kermit/.kermrc --uid 7 --gid 1
   ```

5. `/etc/passwd` ergänzen. Das Wurzeldateisystem heißt hier `md2`:

   ```
   k1520disktool get platte.img.gz md2/etc/passwd --to pw
   ```

   An die Datei `pw/md2/etc/passwd` die Zeile aus *Was eingerichtet wird* anhängen und sie
   zurückschreiben. Weil die Datei schon existiert, braucht `put` den Schalter `--force`:

   ```
   k1520disktool put platte.img.gz pw/md2/etc/passwd --as md2/etc/passwd --force
   ```

6. Die Platte prüfen:

   ```
   k1520disktool check platte.img.gz --full
   ```

   Die Prüfung soll „ohne Befund" melden.

Gibt es eine Datei auf der Platte schon, lehnt `put` ab („Datei existiert bereits"). Mit `--force`
wird sie ersetzt. Danach stimmen Modus und Besitzer nicht mehr; sie müssen mit `attr` neu gesetzt
werden.

### Einrichten unter WEGA (nicht geprüft)

Dieser Weg benutzt nur die WEGA selbst. Er ist im Emulator **nicht geprüft**: weder das Auspacken der
Diskette noch die Befehle zum Anlegen des Benutzers. Maßgeblich ist das Ergebnis in *Was eingerichtet
wird*; die Befehle sind ein Vorschlag.

Voraussetzungen: WEGA ist über die WEGA-Startdiskette gestartet, und man ist an der **Konsole** als
`wega` angemeldet. Nur dann funktionieren die Diskettenbefehle (siehe *Arbeiten mit Disketten unter
WEGA* im Handbuch des Emulators). Die Startdiskette bleibt in Laufwerk A:.

1. Die Diskette `kermit.cqm` in Laufwerk **B:** einlegen.
2. Die beiden Dateien vom Rohgerät `/dev/rfd1` auspacken:

   ```
   cd /tmp
   tar xvf /dev/rfd1 kermit/kermit kermit/ckermi.nr
   ```

3. Programm und Handbuchseite an ihren Platz bringen:

   ```
   cp /tmp/kermit/kermit /usr/bin/kermit
   chmod 4755 /usr/bin/kermit
   cp /tmp/kermit/ckermi.nr /usr/man/man1/kermit.1
   ```

4. Den Benutzer anlegen. Die Zeile bekommt zunächst ein leeres Kennwortfeld; `passwd` setzt dann das
   Kennwort (zweimal `kermit` eingeben):

   ```
   echo 'kermit::7:1::/usr/kermit:/bin/sh' >> /etc/passwd
   passwd kermit
   ```

5. Heimatordner, `.profile` und `.kermrc` anlegen. Bei `cat` die drei Zeilen eintippen und mit
   Strg+D abschließen:

   ```
   mkdir /usr/kermit
   echo 'exec /usr/bin/kermit' > /usr/kermit/.profile
   cat > /usr/kermit/.kermrc
   set file type binary
   server
   exit
   ```

6. Besitzer und Gruppe setzen:

   ```
   chown kermit /usr/kermit /usr/kermit/.profile /usr/kermit/.kermrc
   chgrp other /usr/kermit /usr/kermit/.profile /usr/kermit/.kermrc
   ```

7. Vor dem Ausschalten wie immer `sync;sync` eingeben.

Auf `/usr` ist genug Platz: Das Programm braucht rund 78 KB, auf der Platte waren vorher 537 KB frei.

### Das System einstellen

* **Anmeldeleitungen:** Die WEGA startet eine Anmeldung (`getty`) auf der Konsole und auf tty0, tty2
  und tty4 bis tty7. Für Fall 1 ist an der WEGA nichts weiter einzustellen. Im `p8000emu` wird der
  gewählte Kanal unter *Einstellungen ▸ Schnittstellen* als Telnet-Server freigegeben. Die Leitungen
  laufen mit 9600 Baud.
* **tty3** ist die Druckerleitung. Auf ihr wartet keine Anmeldung. Deshalb benutzt die WEGA sie in
  Fall 2 als Kermit-Client. Im `p8000emu` wird tty3 dafür als Telnet-Client zu diesem Programm
  eingestellt.
* **Achtes Bit:** Auf dem Weg von diesem Rechner zur WEGA löscht tty0 das achte Bit jedes Zeichens. In
  der Gegenrichtung kommt es unverändert an. Das Programm schlägt deshalb immer das 8-Bit-Präfix `&` vor, und
  C-Kermit nimmt es an. Damit kommen auch Binärdateien heil an. Einzustellen ist dafür nichts. Ob tty3 das achte Bit
  durchlässt, ist nicht gemessen; dort arbeitet das Programm ebenfalls mit dem Präfix.
* **Prüfsumme und Paketlänge:** C-Kermit 4.2 übernimmt die Prüfsumme, die das Programm vorschlägt
  (Vorgabe 3). Langpakete kennt es nicht; die Paketlänge ist deshalb 94.
* **Dateinamen** haben auf der WEGA höchstens 14 Zeichen.
* **Binär oder Text:** Programme und Archive immer binär übertragen. Text nur für Textdateien wählen.

### Probe

1. An der WEGA-Konsole als `wega` Kermit aufrufen und die Einstellungen ansehen:

   ```
   kermit
   C-Kermit>show parameters
   C-Kermit>exit
   ```

   Kermit meldet sich mit `C-Kermit 4.2(030) PRERELEASE # 2, 5 March 85, AT&T System III/System V`.
   `show parameters` zeigt unter anderem `Line: /dev/tty, speed: -1, mode: remote`.
2. `man kermit` zeigt die Handbuchseite.
3. Fall 1 durchspielen: verbinden wie in *Fall 1 — die WEGA ist der Server*. Links erscheint das
   Verzeichnis `/usr/kermit`.
4. Eine Datei holen und eine kleine Datei schreiben. Im Protokoll (*Ansicht ▸ Protokoll*) stehen die
   ausgehandelten Werte, der Fortschritt und gegebenenfalls der Fehlertext der WEGA.
5. *Verbindung ▸ Server der Gegenstelle beenden (Finish)*. Die WEGA zeigt wieder `login:`.

### Fehlersuche bei der Einrichtung

* **`login:` erscheint nicht.** Die WEGA zeigt die Anmeldezeile erst nach einem Zeichen. Das
  Login-Skript sendet deshalb RETURN und wartet. Erscheint auch dann nichts, ist WEGA noch nicht im
  Mehrbenutzerbetrieb, oder der Kanal ist im `p8000emu` nicht freigegeben.
* **Die Anmeldung scheitert.** Das Kennwort stimmt nicht, oder das Konto fehlt. Die Zeile in
  `/etc/passwd` prüfen.
* **Die WEGA meldet `You don't have a password. Choose one.`** Das Konto hat kein Kennwort. Mit
  `passwd kermit` eines setzen.
* **Nach der Anmeldung startet kein Server.** `/usr/kermit/.profile` oder `/usr/kermit/.kermrc` fehlt,
  oder in `.kermrc` fehlt die Zeile `server`. Kermit wartet dann an seiner Eingabeaufforderung
  `C-Kermit>`.
* **Statt des Verzeichnisses erscheint die Gebrauchsanweisung von Kermit.** Als Shell des Benutzers
  `kermit` steht `/usr/bin/kermit` in `/etc/passwd`. Richtig ist `/bin/sh`.
* **Jedes Holen endet mit „Can't open file".** In `.kermrc` steht `set file names literal`. Die Zeile
  löschen.
* **Zeitüberschreitung bei der Anmeldung.** Zwischen Kennwort und Serverstart braucht die WEGA etwa
  zwölf Sekunden Maschinenzeit. Läuft der Emulator stark gebremst, dauert das in Echtzeit länger.
  *Frist je Anmeldeschritt* erhöhen.
* **Ein Name wird abgelehnt.** Er ist länger als 14 Zeichen. Die Datei vorher umbenennen.
* **Eine Datei ist nach der Übertragung unbrauchbar.** Sie wurde als Text übertragen. *Binär* wählen.
* **Die WEGA meldet `unable to work with floppy`.** Die Diskettenbefehle gehen nur an der Konsole und
  nur, wenn WEGA über die WEGA-Startdiskette gestartet wurde.

## Die Kommandozeile k1520kermit

Dieselbe Bibliothek steckt im Kommandozeilenprogramm `k1520kermit`. Im Quellbaum ruft man es mit
`tools/dev.sh tool k1520kermit …` auf. Die vollständige Beschreibung steht in `tools/k1520kermit.md`.

Beispiele für Fall 1 (der `p8000emu` bietet tty0 als Telnet-Server auf Port 5000 an):

```
k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit ls -l
k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit put brief.txt --as brief
k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit get README --to ~/wega
k1520kermit --telnet 127.0.0.1:5000 --login kermit:kermit logout
```

Beispiel für Fall 2 (das Programm lauscht auf Port 5003, der `p8000emu` verbindet tty3 dorthin):

```
k1520kermit serve --listen-telnet 5003 ~/austausch
```

* **Kommandos:** `ls`, `get`, `put`, `cd`, `rm`, `finish`, `logout`, `serve`. Jedes Kommando baut
  eine eigene Verbindung auf und trennt sie am Ende wieder.
* **Leitung:** `--telnet HOST:PORT` oder `--rfc2217 HOST:PORT` zum Verbinden, `--listen-telnet PORT`
  oder `--listen-rfc2217 PORT` zum Lauschen. `--login NAME:KENNWORT` meldet sich an.
* **Ausgabe:** Auf der Standardausgabe stehen nur die Ergebnisse, alle Hinweise gehen auf die
  Fehlerausgabe. `--json` liefert die Ergebnisse maschinenlesbar.
* **Rückgabewerte:** `0` in Ordnung; `1` Fehler bei der Übertragung oder Fehlermeldung der
  Gegenstelle, auch Abbruch mit Strg+C; `2` falscher Aufruf, keine Verbindung oder Anmeldung
  gescheitert.

## Fehlersuche

* **„Verbindung fehlgeschlagen".** Unter dieser Adresse wartet niemand. Im `p8000emu` den Kanal als
  Telnet-Server freigeben und den Port vergleichen. Oder umgekehrt: hier *Lauschen* wählen und den
  Kanal im Emulator als Telnet-Client einstellen.
* **Das Fenster *Anmeldung* erscheint nicht.** Im Verbindungsdialog fehlt der Haken *Anmelden (Login-Skript
  für eine getty)*. Ohne ihn meldet sich das Programm nicht an.
* **Die Gegenstelle meldet `Login incorrect`.** Benutzername oder Kennwort stimmen nicht. Beim eigenen
  Server stehen sie im Dialog *Als Server einrichten…*. Nach mehreren falschen Eingaben beginnt der Server
  einfach wieder mit `login:`.
* **Die Anmeldung scheitert.** *Ansicht ▸ Protokoll* öffnen. Dort stehen die Schritte des
  Login-Skripts und der zuletzt empfangene Text. Steuerzeichen erscheinen darin als `<1B>` usw.
* **Nach *Trennen* zeigt die WEGA kein `login:` mehr.** *Trennen* schließt nur die Leitung; der
  Kermit-Server auf der WEGA läuft weiter. Beim nächsten Mal den Haken bei *Anmelden* weglassen,
  dann antwortet der laufende Server sofort. Oder einmal verbinden und *Server der Gegenstelle
  beenden (Finish)* wählen. Am Programm selbst ist dieser Fall nicht eigens geprüft, nur an der
  Bibliothek.
* **Zeitüberschreitung bei einer Übertragung.** Die Leitung ist langsam (9600 Baud, weniger als ein
  Kilobyte je Sekunde), oder der Emulator läuft deutlich langsamer als das echte Gerät. *Frist je
  Paket* erhöhen oder die *Geschwindigkeit der Gegenstelle* fest einstellen.
* **Der Server startet nicht.** Es ist kein Ordner freigegeben (Hinweis im Streifen), oder der Port ist
  belegt.
* **Es kommt nur eine Verbindung an.** Ein lauschendes Programm nimmt nur eine Verbindung an.
  *Nach der Trennung wieder lauschen* einschalten.
* **„Abgebrochen".** Die Übertragung wurde abgebrochen. War der Abbruch geordnet, bleibt die
  Verbindung bestehen.
* **Eine Datei wird abgelehnt.** Den Grund nennt die Gegenstelle; er steht im Streifen und im
  Protokoll. Häufig ist der Name zu lang.
* **Eine Datei ist kaputt angekommen.** Die Übertragungsart prüfen. Programme nie als Text
  übertragen.
* **Beide Seiten warten aufeinander.** *Hart abbrechen* beendet die Sitzung.

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

Alle Kürzel beginnen mit Strg+Umschalt, wie in den anderen Programmen dieser Reihe.

## Die anderen Werkzeuge

Im Menü *Werkzeuge* des Emulators und des DiskTool steht der Eintrag *K1520 Kermit starten*. Das
Programm läuft dann als eigener Prozess neben dem Emulator. Für die Arbeit mit der WEGA ergänzen sich
drei Programme: der Emulator (`p8000emu`) führt die WEGA aus, das DiskTool liest und schreibt ihre
Platten und Disketten, und K1520 Kermit tauscht Dateien mit der laufenden WEGA aus.

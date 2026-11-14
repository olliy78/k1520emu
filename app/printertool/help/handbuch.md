# K1520 Printer — Handbuch

Version {{VERSION}}

K1520 Printer ist ein **Hardware-Emulator der Nadeldrucker der K1520-Familie**. Es ist kein
Textfilter: im Drucker läuft die **Original-Firmware** auf einem emulierten Z80, und das Blatt
entsteht **Punkt für Punkt**, so wie der Nadelkopf die Punkte setzt. Ein Rechner-Emulator
(A5120, K8915, PRG 710, PC 1715, P8000, MC80) oder ein beliebiges Terminalprogramm schickt dem
Drucker seine Zeichen über eine serielle Leitung (Telnet oder RFC 2217).

Nachgebildet sind **SD 1157/264**, **K6316**, **K6313** und **K6311**, jeweils mit den
Original-EPROMs, die im Katalog stehen.

Wo was steht:

* Die ersten Schritte stehen in *Der übliche Weg*.
* Die Bedienelemente des Fensters beschreibt *Das Fenster*.
* Welche Drucker es gibt und was von ihnen belegt ist, steht in *Die Druckertypen* und in
  *Was nachgebildet ist und was nicht*.
* Welche Firmware läuft und wie man sie wechselt, erklärt *Die EPROM-Wahl*.
* Tasten, Lampen und Einschalttests je Gerät stehen in *Bedienfeld und Einschalttests*.
* Wenn etwas nicht klappt, hilft *Fehlersuche*.

## Der übliche Weg

Ein Rechner-Emulator soll auf dem Drucker ausdrucken. Beispiel: der A5120 mit seiner
Druckerschnittstelle.

1. K1520 Printer starten (`bash run_k1520printer.sh`). Das Fenster zeigt ein leeres Blatt und
   den SD 1157/264 im Zustand *online*.
2. Im Reiter *Drucker* den **Druckertyp** wählen. Bei Typen mit mehreren EPROM-Sätzen erscheint
   darunter das Feld **EPROM**; die Vorgabe ist meist richtig (siehe *Die EPROM-Wahl*). Ein
   Wechsel legt den Drucker neu an und leert das Blatt; das Programm fragt vorher.
3. Im Reiter *Schnittstellen* den Block der Schnittstelle ansehen, die der Drucker benutzt
   (SD 1157: die im Reiter *Drucker* gewählte; K6316, K6313 und K6311: die im Typ genannte).
   Ausgeliefert ist die **IFSS als Telnet-Server auf `127.0.0.1`, Port 5000**, eingeschaltet.
   Die **V.24** steht auf Port **5001** bereit, ist aber **aus**: erst *Starten* drücken.
4. Im Rechner-Emulator die Druckerschnittstelle als **Client** auf `127.0.0.1` und den Port
   des Druckers einstellen und *Verbinden* drücken (beim A5120 *Einstellungen ▸ Schnittstellen*,
   Block *Drucker*, Telnet, Client, `127.0.0.1`, Port 5000).
5. Im Gastsystem etwas ausdrucken, zum Beispiel `TYPE DATEI.TXT` auf den Drucker umleiten oder
   ein Programm, das auf den Drucker schreibt. Das Blatt füllt sich zeilenweise; der Druckkopf
   ist als rotes Band zu sehen.
6. Das Ergebnis mit *Datei ▸ Blatt als PDF speichern…* (`Strg+Umschalt+D`) oder
   *Blatt drucken…* (`Strg+Umschalt+P`) ausgeben. *Blatt leeren* (`Strg+Umschalt+L`) beginnt
   ein neues Blatt.

Ohne Rechner-Emulator geht es auch mit einem Terminalprogramm: `telnet 127.0.0.1 5000`, Text
eintippen, mit Eingabe bestätigen. Die Zeile wird gedruckt, sobald ein Zeilenende ankommt.

## Das Fenster

* **Mitte: das Blatt.** Weißes Papier, schwarze Punkte. Beim Druck schiebt sich das Blatt
  zeilenweise nach oben; die Zeile, die der Kopf gerade druckt, bleibt im Bild (*Blatt folgen*).
  Der **Druckkopf** ist als rotes Band über den neun Nadeln zu sehen und als Dreieck auf der
  Schiene am unteren Rand; seine Lage gibt die x-Position des Wagens an (Statuszeile: Zoll und
  Spalte bei 10 Zeichen je Zoll).
* **Rechts oben: das Bedienfeld** mit Netzschalter, Lampen, Tasten und Schaltern des Druckers.
  Es wird aus dem gewählten Typ gebaut und sieht je Gerät anders aus.
* **Rechts unten: die Einstellungen** mit den Reitern *Drucker* und *Schnittstellen*.
* **Statuszeile:** Druckertyp, Zustand (online, offline, Netz aus, FEHLER), Schnittstelle,
  Server und Verbindungen, Kopfposition.

Das Blatt lässt sich mit dem Mausrad rollen (das beendet *Blatt folgen*) und mit
**Strg + Mausrad** vergrößern; *Blatt einpassen* passt es der Fensterbreite an. Das Papier ist
15 Zoll breit; eingepasst sind die Punkte klein, vergrößert erscheinen sie einzeln.

### Der Reiter *Drucker*

* **Druckertyp.** Wechsel legt den Drucker neu an (Rückfrage); das Blatt wird dabei geleert,
  speichern Sie es vorher als PDF. Nicht wählbare Typen stehen grau mit dem Grund im Tooltip.
* **EPROM.** Nur sichtbar, wenn der Katalog für den Typ mehr als einen Satz kennt. Darunter
  stehen Hinweis, Güte, Programmstand und Zeichensatz des gewählten Satzes. Die Wahl wird je
  Typ gemerkt. Ein Wechsel startet den Drucker neu.
* **Schnittstelle.** Nur am SD 1157 wählbar (der Schalterblock des Druckers legt fest, ob die
  Firmware IFSS oder V.24 benutzt); bei den anderen Typen folgt sie dem Typ. Ein Wechsel
  startet den Drucker neu.
* **Tempo.** Rechengeschwindigkeit gegenüber dem echten Drucker.
* **Seitenlänge.** 4, 6, 8 oder 12 Zoll: so wird das Blatt für PDF und Druck in Seiten
  geteilt (im Bild als gestrichelte Linie). Der Formularvorschub der Firmware fährt dagegen
  fest 12 Zoll.

### Der Reiter *Schnittstellen*

Derselbe Reiter wie in den Rechner-Emulatoren, siehe *Schnittstellen und Verbindung*.

## Die Druckertypen

| Typ | Schnittstelle | EPROM-Sätze im Katalog | Stand |
|-----|---------------|------------------------|-------|
| SD 1157/264 | IFSS oder V.24 (wählbar) | *repariert* (Vorgabe), *Original-Abzug* | druckt; Selbsttest, Text und ESC-Folgen Punkt für Punkt gegen den Zeichengenerator belegt |
| K6316 (IFSS) | IFSS | ein Satz (5 EPROMs) | druckt; Schnitt, Vorsteckbeleg, Einschalttests |
| K6316 (V.24) | V.24 | ein Satz (5 EPROMs) | wie IFSS |
| K6313 (V.24) | V.24 | 7 Sätze, darunter zwei 16-KiB-Fassungen | druckt; Hardware aus der Firmware erschlossen |
| K6313 (IFSS) | IFSS (Stromschleife) | 5 Sätze, darunter zwei 16-KiB-Fassungen | Text und Selbsttest wie am V.24; Kassette ohne Plan |
| K6311 (V.24) | V.24 | 2 Sätze | druckt; Hardware nur aus dem ROM erschlossen [?] |
| K6311 (IFSS) | IFSS | 1 Satz (nur 8 KiB) | Text und Selbsttest; Hardware erschlossen [?] |

**Nicht wählbar:** SD 1157/267 (die zweite Papierbahn wird nicht nachgebildet) und SD 1152/257
Typenrad (in Vorbereitung). **Nicht umgesetzt:** die Schnittstellen **Centronics** und **IFSP**
(Parallelweg) aller Typen; Abzüge, die nur Centronics sprechen, stehen deshalb nicht im Katalog.

## Die EPROM-Wahl

Die Firmware ist der eigentliche Drucker. Alle EPROM-Sätze stehen in **einer Datei**, dem Katalog
`drucker_roms.yaml`. Sie trägt je Satz Metadaten und die Bytes als **Intel-HEX**; die
Originalabzüge liegen im Quellbaum unter `doc/EPROMS/`, zur Laufzeit wird nur der Katalog
gebraucht.

Je Eintrag zeigt die Oberfläche:

* den **Titel** (bei K6313/K6311 die Etikettenkennung des Abzugs, z. B. `3.35-3-80/13`), mit
  dem Zusatz *(Vorgabe)* beim vorgeschlagenen Satz,
* einen **Hinweis**, zum Beispiel welche Schnittstellen oder Zeichensätze der Satz hat,
* die **Güte**: `ok`, `unvollstaendig`, `defekt` oder `unbekannt`,
* Programmstand und Zeichensatz.

**Vorgabe** ist je Typ genau ein Satz. Beim SD 1157 ist das *repariert*: im Original-Abzug ist
vermutlich ein Bit gekippt (Byte 0D34H), der Eingabepuffer leert sich nie, und über die
Schnittstelle bleibt der Drucker nach dem ersten Zeichen stehen. Die reparierte Fassung ändert
nur dieses eine Byte. *Original-Abzug* ist der unveränderte Abzug (der Selbsttest läuft damit).

**Etikett und Druckbild.** Beim K6311 widersprechen sich Etikettenliste und Druckbild: der Abzug
`12128062` (Etikett GERMAN) druckt US-ASCII, `12128062_1` (Etikett UK ASCII) den deutschen
Satz mit Pfundzeichen (`#` = £, `@` = §, `[ \ ]` = Ä Ö Ü, `{ | } ~` = ä ö ü ß). Der Katalog
nennt das Druckbild.

**Aliase.** Alte Namen (z. B. `repariert` in einer früheren Konfiguration) bleiben als Aliase
gültig; eine ältere Konfiguration wird darum weiter gelesen.

**Den Katalog ändern.** Die Datei ist Text und darf mit einem Texteditor geändert werden. Wer
Daten ändert, muss die **Zeilenprüfsumme** der Intel-HEX-Zeile mitführen: sie ist die Prüfung.
Dazu kommen das Feld `groesse` und die Längenregel je Typ. Findet das Programm einen Fehler,
meldet es *Datei:Zeile: Eintrag 'Schlüssel': …*; der **ganze Katalog** gilt dann als unbrauchbar,
es gibt keinen Rückfall auf eine eingebaute Fassung.

**Wo das Programm den Katalog sucht**, in dieser Reihenfolge:

1. die Umgebungsvariable `K1520_PRINTER_ROMS` (Datei oder Ordner; ist sie gesetzt, gilt
   **nur** sie),
2. neben der Installation unter `share/k1520emu/`,
3. im Quellbaum `data/`.

`--paths` zeigt den Suchweg. Fehlt der Katalog, **startet der Drucker nicht** und nennt die
gesuchten Orte.

## Bedienfeld und Einschalttests

Das Bedienfeld folgt dem Typ. Gemeinsam: **Netz** ist der Netzschalter (*Aus*: der Drucker
steht, alle Lampen sind dunkel, das Blatt bleibt; *Ein*: Neustart, `/RESET`), *Neustart*
schaltet aus und ein. Was die Firmware nur **beim Einschalten** liest, wirkt erst nach einem
Neustart; das ist das Verhalten der Original-Firmware, kein Fehler des Emulators.

### SD 1157/264

| Element | Wirkung |
|---------|---------|
| **ONLINE** | Online/Offline (rastend). Offline druckt der Drucker nicht und nimmt PV1, PV2 und TEST an. |
| **PV1** | Papiervorschub: kurz = eine Zeile, gehalten (ab etwa 0,85 s) = Formularvorschub. Nur offline. |
| **PV2** | Papiervorschub der zweiten Bahn. Ohne Wirkung: der SD 1157/264 hat nur eine Bahn. |
| **TEST** | Selbsttest: Titelzeile und zwei Zeilen Zeichensatz (siehe unten). |
| **Kopf an** | Druckkopf dauernd angeschwenkt (sonst nur während des Drucks). |
| **6 lpi** | Zeilenabstand 6 oder 8 Zeilen je Zoll; wirkt nach dem nächsten Neustart. |
| **Papier da** | aus = Papierende. |

**Lampen:** *Netz* (grün), *ONLINE* (grün: bereit), *FEHLER* (rot: Havarie, z. B.
Farbbandhavarie, oder Zeitüberschreitung beim Papiervorschub).

**TEST wirkt nur nach einem Neustart im Offline-Zustand.** Die Firmware wertet TEST nur aus,
wenn sie **offline eingeschaltet** wurde. Also:

1. **ONLINE** ausschalten,
2. **Neustart** (`Strg+Umschalt+R`),
3. **TEST** drücken.

Schaltet man ONLINE nur im Betrieb aus, bleibt die ONLINE-Lampe an, PV1/PV2 wirken, TEST nicht.
Unter dem Bedienfeld erscheint dann ein Hinweis mit dem Knopf *Neustart jetzt*. Ebenso liest
die Firmware *6 lpi* und *ONLINE* für die Lampe nur beim Einschalten.

Die Menüeinträge *ONLINE*, *PV1*, *PV2* und *TEST* im Menü *Drucker* gehören zum SD 1157 und
sind bei den anderen Typen gesperrt; dort bedient man das Bedienfeld.

### K6316

Die Firmware wertet die Tasten beim Einschalten erst **nach der Synchronisation** aus (etwa
8 Sekunden); die Taste also so lange halten, bis der Test beginnt.

| Beim Netz-Ein gehalten | Wirkung |
|------------------------|---------|
| **S5** | Selbsttest mit Druck |
| **S5 + S4** | Selbsttest ohne Druck |
| **S1** | Lichtschrankentest (FEHLER leuchtet bei freier oberer, ONLINE bei freier unterer Lichtschranke; S1 erneut beendet ihn) |
| **S2** | Hexdump: jedes empfangene Byte als zwei Hexziffern, 32 je Zeile (die Schnittstelle überträgt nur 7 Bit) |
| **S3** | Test der Vorsteckeinrichtung |

Im Betrieb: **S1** schaltet zwischen ON-LINE und OFF-LINE um, **S2** und **S3** schieben das
Papier vor und zurück (nur offline), **S4** wirft den Beleg aus. Dazu kommen der Drehschalter
*Betriebsart* (Aufwickeln, Schneiden, Endlosverarbeitung), der Kopfhebel, die Haube, *Papier da*
und die **DIL-Schalter S11–S26** (Tooltips nennen, was belegt ist; nicht belegte Schalter sind mit
[?] gekennzeichnet). Ist die Haube offen oder der Kopf abgeschwenkt, unterbleibt die
Synchronisation, und die Lampe FEHLER blinkt.

Der Kasten **Vorsteckbeleg** (nur K6316) steckt einen Beleg der gewählten Größe vor die
Lichtschranke bzw. nimmt ihn wieder heraus. Einziehen, Positionieren und Auswerfen macht die
Firmware: auf Befehl des Rechners oder offline mit **S4**. Was gedruckt wird, solange der Beleg
unter der Druckzeile liegt, erscheint auf dem Belegblatt im Kasten, nicht auf dem
Endlospapier. Die Lage der Lichtschranken zur Druckzeile ist aus der Firmware erschlossen, nicht
am Gerät gemessen.

### K6313

Tasten: **ON/OFF LINE**, **LF ▲** (Zeilenvorschub vor), **LF ▼** (zurück), **FF** (Formularvorschub),
*Papier da*; Lampen *POW*, *ON LINE*, *ERR*, *PE*.

| Beim Netz-Ein gehalten | Wirkung |
|------------------------|---------|
| **LF ▲** | Selbsttest mit Kennungszeile, danach der Zeichenumlauf (läuft, solange die Taste gehalten wird) |
| **LF ▼** | derselbe Selbsttest ohne Kennungszeile |
| **FF + LF ▲** | Hexdump: jedes empfangene Byte als Hexzahl |

Die **DIL-Schalter 5-1 bis 18-2** (Gruppe im Bedienfeld) legen unter anderem Zeichensatz,
Zeilenabstand, Formatlänge, Baudrate, Parität und Protokoll fest; die Tooltips nennen die
Belegung. **Der Schalter 5-2 muss EIN sein**, sonst verlässt die Firmware beim Einschalten den
Normalweg und startet nicht (Vorgabe: 5-2 EIN, 13-2 EIN = EPSON-Steuercodes).

Der Hexdump arbeitet im EPSON-Satz erst bei vollem Druckpuffer (32 Byte je Zeile); beim ISO-Satz
druckt der Rest beim Wechsel nach OFF LINE.

### K6311

Tasten: **ON/OFF LINE**, **TEST**, **PAPIER ▲**, **PAPIER ▼**, *Papier da*; Lampen *POW*,
*ON LINE*, *ERR*. DIL-Schalter gibt es nicht; Baudrate und Datenformat belegt der Emulator mit
Vorgaben (V.24: 9600 Bd, 8 Bit, keine Parität, ein Stoppbit; IFSS: 9600 Bd, 7 Bit, Parität
ungerade, ein Stoppbit).

| Bedienung | Wirkung |
|-----------|---------|
| **ON/OFF LINE** | schaltet zwischen ON LINE und OFF LINE um |
| **TEST** (offline) | Selbsttest: Kennungszeile aus dem ROM, danach der Zeichenumlauf |
| **PAPIER ▲** (offline) | Papier rückwärts, solange gehalten |
| **PAPIER ▼** (offline) | Wagen nach rechts, Papier vorwärts (Zeilen zu 1/6 Zoll), solange gehalten |
| **TEST + PAPIER ▼** beim Netz-Ein | Hexdump-Modus |

Die Beschriftung der Tasten ist aus dem ROM erschlossen [?]. Eine PE-Lampe schreibt das ROM
nicht; sie wird deshalb nicht gezeigt. Ohne Papier (*Papier da* aus) geht die Firmware OFF
LINE; mit Papier und *ON/OFF LINE* druckt sie weiter.

## Schriftarten und Steuerfolgen

Was die Steuerfolgen am Blatt bewirken, ist **gegen unabhängige Prüfbilder** belegt (jedes Zeichen
an seinem Punkt). Die Folgen schickt der Rechner als Text; der Emulator wertet sie nicht selbst
aus, es ist die Firmware, die sie ausführt.

**K6313** (EPSON-Satz, DIL 13-2 EIN, und ISO-Satz mit den 16-KiB-Abzügen):

| Wirkung | EPSON | ISO |
|---------|-------|-----|
| Fettdruck | `ESC E` | |
| Breitdruck | `ESC W 1` (`ESC SO` bis Zeilenende) | `ESC [ 1 m` (bis Zeilenende) |
| Elite (12 Zeichen/Zoll) | `ESC M` | `ESC [ 1 SP K` bzw. `2 SP K` |
| komprimiert | `ESC SI` (`DC2` oder `ESC P` zurück auf Pica) | `ESC [ 4 SP K` bzw. `3 SP K` (`ESC [ 0 SP K` zurück) |
| Unterstreichen | `ESC - 1` | `ESC [ 4 m` (bis Zeilenende) |
| Zeilenabstand | `ESC 0`, `ESC 1`, `ESC 2`, `ESC 3 n`, `ESC A n`, `ESC J n` | `LF`, `ESC [ n e`, `ESC [ n d` |
| Formularvorschub | `FF` (Länge nach DIL 12-1/12-2: 12, 11, 6 oder 5,5 Zoll) | `FF` (12 Zoll) |

**K6311:** Breitdruck (`ESC [ 1 m`, bis Zeilenende) und Unterstreichen (`ESC [ 4 m`, bis Zeilenende).
Hat ein Zeichen eine eigene Nadel 9 (g, j, p, q, y, Komma, Semikolon, Unterstrich), entfallen die
Unterstreichungsstriche. `ESC [ n K` ohne Leerzeichen kennt das ROM nicht; `ESC [ n SP K` ist
bekannt, ändert den Zeichenabstand aber nicht.

**SD 1157/264:** Normal-, Breit- und Schrägschrift, Tabulator (Vielfache von 8), Formularvorschub und
Zeilenabstand 6/8 lpi sind belegt.

**Nicht belegt** (die Firmware löst etwas aus, das Druckbild ist aber nicht gegen ein Prüfbild
bestätigt): am K6313 **Schrägschrift** (`ESC [ 3 m`), **Hoch- und Tiefschrift** (`ESC S 0/1`),
**Doppeldruck** (`ESC G`), **`ESC C`/`ESC N`** (Formularlänge in Zeilen, Perforationssprung),
Papier zurück für Traktoren (`ESC [ n u`) und die Umschaltung auf den 8-Bit-Zeichensatz; am K6311
**Schrägschrift** und der **Hexdump** (er druckt, aber seine Teilung ist nicht belegt).

## Schnittstellen und Verbindung

Der Reiter *Schnittstellen* ist derselbe wie bei den Rechner-Emulatoren: je Schnittstelle
(**IFSS**, **V.24**) Betriebsart *Telnet* oder *RFC 2217*, Rolle *Server* oder *Client*, Host,
Port. Der Drucker benutzt die Schnittstelle, die zum Typ gehört (am SD 1157 die im Reiter
*Drucker* gewählte); die andere ist ohne Funktion.

Ausgeliefert: **IFSS = Telnet-Server auf Port 5000, eingeschaltet; V.24 = Telnet-Server auf Port
5001, ausgeschaltet.** Im Rechner-Emulator verbindet man dessen Druckerschnittstelle als Client mit
`127.0.0.1` und dem Port. Betriebsart, Rolle, Host und Port sind gesperrt, solange die
Schnittstelle läuft (erst *Beenden*).

**Flusssteuerung (Gastverhalten).** Die Firmware des SD 1157 und des K6316 steuert den Fluss selbst
mit **DC1/DC3** (XON/XOFF): DC1 nach dem Einschalten, DC3 bei Druckbeginn einer Zeile und bei
vollem Puffer (nach etwa 1 000 Zeichen), DC1 wieder bei leerem Puffer und fertigem Druck. Eine
Gegenstelle, die DC3 nicht beachtet, verliert Daten hinter etwa 1 KiB. Am V.24 kommt das erste DC1
erst, wenn eine Verbindung besteht (beim K6316 zusätzlich nur mit gesetztem DIL-Schalter S17,
sonst arbeitet er mit DTR). Offline im Betrieb druckt der SD 1157 den Auftrag zu Ende, gibt aber
kein DC1 mehr aus, bis er wieder online ist. Beim K6313 ist das Protokoll Sache der DIL-Schalter
17-1 und 17-2 (DTR oder XON/XOFF).

## Ausgabe: PDF und Drucken

* **Blatt als PDF speichern** (`Strg+Umschalt+D`) schreibt eine Datei mit einer Seite je
  Formular. Jeder Nadelpunkt ist ein schwarzer Kreis; die Seite ist 15 Zoll breit und so hoch
  wie die gewählte Seitenlänge. Leere Seiten am Ende entfallen.
* **Blatt drucken** (`Strg+Umschalt+P`) öffnet den Druckdialog des Betriebssystems. Das 15 Zoll
  breite Blatt wird auf die Papierbreite verkleinert (nie vergrößert); die Seiten brechen an der
  Seitenlänge oder, wenn das Papier kürzer ist, an dessen Ende.
* **Blatt leeren** (`Strg+Umschalt+L`) löscht das Blatt; die Papierlage bleibt. Speichern Sie es
  vorher als PDF: das Blatt wird nicht mit der Konfiguration gesichert.

## Was nachgebildet ist und was nicht

Ehrlich gesagt, wie genau:

* **Druckbild.** Selbsttest und Text stimmen bei allen vier Typen **Punkt für Punkt** mit dem
  Zeichengenerator im jeweiligen ROM überein, vor- und rückwärts gedruckte Zeilen eingeschlossen.
  Das ist gegen unabhängig aus dem ROM berechnete Bilder geprüft, nicht gegen ein Foto eines
  echten Geräts.
* **Hardware.** Beim **SD 1157** und **K6316** liegen Quelle bzw. Schaltplan vor. Beim **K6313**
  ist die Hardware aus der Firmware und Teilen des Plans erschlossen, beim **K6311**
  **ausschließlich aus dem ROM erschlossen [?]**: es gibt weder Schaltplan noch Handbuch. Was
  dort mit [?] steht (Tastenbeschriftung, Lampenrollen, Abmessung Pica, Abgleich zwischen Vor- und
  Rückwärtszeile), ist Annahme.
* **Zeilenversatz vor/zurück.** Die absolute Flugzeit des Kopfes ist unbekannt; der Abgleich
  zwischen vor- und rückwärts gedruckten Zeilen ist gewählt, damit beide deckungsgleich liegen.
  Beim K6313 haben Rückwärtszeilen je nach Schrift einen kleinen Ursprungsversatz (einige Pixel,
  Ursache offen).
* **K6313, Abzug 53538500** (neue Platine): die Nadelfreigabe `/PC` ist **nicht nachgebildet**
  (ihre Polarität ist je Platine verschieden und nicht aus einem Plan lesbar). Folge: die erste
  Zeile nach dem Netz-Ein trägt Fremdpunkte; ab der zweiten Zeile stimmt das Bild.
* **IFSS-Kassette** des K6313/K6311: es gibt keinen Plan; ihre Wirkungsweise (Pegelwandlung,
  fest aktive Steuerleitungen) ist aus dem ROM erschlossen [?].
* **Summer** (BEL, Papierende): im Kern beobachtbar, in der Oberfläche **weder zu hören noch zu
  sehen**.
* **Nicht nachgebildet:** Centronics und IFSP, die zweite Bahn des SD 1157/267, Farbband und
  Typenrad. Abzüge, die nur Centronics sprechen, oder deren Zeichenerzeugung anders aufgebaut ist
  (`37239503`, `36539211`, `5c53a001`), stehen nicht im Katalog.

## Fehlersuche

* **Das Programm startet nicht und nennt `drucker_roms.yaml`.** Der Katalog fehlt oder ist
  unbrauchbar. `k1520printer --paths` zeigt, wo gesucht wird; mit `K1520_PRINTER_ROMS` kann man
  Datei oder Ordner vorgeben. Bei einem Schemafehler steht Datei, Zeile und Eintrag in der
  Meldung (zum Beispiel eine falsche Prüfsumme nach einer Handänderung).
* **Es wird nichts gedruckt.** Lampe ONLINE aus? Dann ONLINE einschalten (SD 1157) bzw. ON/OFF LINE
  drücken. *Papier da* gesetzt? Ist der Drucker noch nicht in der Grundstellung (Wagen
  synchronisiert, ONLINE leuchtet)? Das dauert nach dem Einschalten Sekunden (K6316 etwa 8 s).
* **Der Rechner-Emulator verbindet nicht.** Läuft die Schnittstelle des Druckers (Knopf *Beenden*
  statt *Starten* im Reiter)? Die V.24 ist ab Werk aus. Stimmt der Port (5000 IFSS, 5001 V.24)?
  Stimmt die Schnittstelle des Typs mit der gestarteten überein?
* **Nach dem ersten Zeichen bleibt der SD 1157 stehen.** Dann läuft vermutlich der *Original-Abzug*
  (siehe *Die EPROM-Wahl*); den Satz *repariert* wählen.
* **Daten gehen verloren.** Die Gegenstelle beachtet DC3 nicht (siehe *Flusssteuerung*) oder die
  Baudrate stimmt nicht; beim K6313 prüfen Sie die DIL-Schalter für Baudrate und Protokoll.
* **TEST oder ein Einschalttest tut nichts.** Siehe *Bedienfeld und Einschalttests*: der Test
  wird nur beim Einschalten ausgewertet (SD 1157: offline einschalten; K6316: Taste lange genug
  halten; K6313: Taste ab dem Netz-Ein halten).
* **K6313 startet nicht.** Der DIL-Schalter 5-2 muss EIN sein.
* **Zeichen sehen falsch aus (Umlaute, Sonderzeichen).** Der Zeichensatz kommt aus dem EPROM-Satz
  und beim K6313 zusätzlich aus den DIL-Schaltern 8-1 bis 9-2; K6311: siehe *EPROM-Wahl*.

## Tastenkürzel

Alle Kürzel tragen Strg+Umschalt.

| Kürzel | Wirkung |
|---|---|
| Strg+Umschalt+D | Blatt als PDF speichern |
| Strg+Umschalt+P | Blatt drucken |
| Strg+Umschalt+Q | Beenden |
| Strg+Umschalt+N | Netzschalter |
| Strg+Umschalt+R | Neustart (Netz aus und ein) |
| Strg+Umschalt+O | ONLINE umschalten (SD 1157) |
| Strg+Umschalt+L | Blatt leeren |
| Strg+Umschalt+B | Blatt einpassen |
| Strg+Umschalt+F | Blatt folgen |
| Strg+Umschalt+H | Handbuch |

## Aufruf

`bash run_k1520printer.sh` startet das Programm. Zusätzlich:

* `--instance NAME` öffnet eine zweite, eigene Konfiguration (`k1520printer-NAME.yaml`) neben der
  ersten, so lassen sich mehrere Drucker nebeneinander betreiben,
* `--config DATEI` legt die Konfigurationsdatei fest,
* `--paths` zeigt die aufgelösten Pfade (Bibliothek, Vorgaben, Suchweg des EPROM-Katalogs).

Gespeichert werden Druckertyp, EPROM-Wahl je Typ, Schnittstellen, Tempo, Ansicht und Fenster.

## Die anderen Werkzeuge

K1520 Printer gehört zur selben Familie wie die übrigen Programme:

* die **Rechner-Emulatoren** (`a5120emu`, `k8915emu`, `prg710emu`, `pc1715emu`, `p8000emu`,
  `mc80emu`): sie schicken ihre Druckerausgabe über Telnet oder RFC 2217 an diesen Drucker,
* **k1520DiskTool**: Dateien auf Disketten und Plattenabbildern,
* **K1520 Kermit**: Dateiaustausch mit der WEGA des P8000.

Jedes Programm hat sein eigenes Handbuch.

# K1520 Printer

Hardware-Emulator der Nadeldrucker der K1520-Familie.  Version {{VERSION}}.  Es ist kein Textfilter: auf dem Z80 des Druckers läuft die **Original-Firmware**, und das Blatt entsteht Punkt für Punkt, wie ihn der Nadelkopf setzt.  Ein Rechner-Emulator (A5120, K8915, PRG 710, PC 1715, P8000, MC80) oder ein beliebiges Terminalprogramm schickt dem Drucker seine Zeichen über eine serielle Leitung.

## Das Fenster

* **Mitte: das Blatt.**  Weißes Papier, schwarze Punkte.  Beim Druck schiebt sich das Blatt zeilenweise nach oben; die Zeile, die der Kopf gerade druckt, bleibt im Bild (*Blatt folgen*).  Der **Druckkopf** ist als rotes Band über den neun Nadeln zu sehen und als Dreieck auf der Schiene am unteren Rand: seine Lage gibt die x-Position des Wagens an (Statuszeile: Zoll und Spalte bei 10 Zeichen je Zoll).
* **Rechts oben: das Bedienfeld** mit Netzschalter, den Lampen, den Tasten und Schaltern des Druckers.
* **Rechts unten: die Einstellungen** mit den Reitern *Drucker* und *Schnittstellen*.
* **Statuszeile:** Druckertyp, Zustand (online/offline/Netz aus), gewählte Schnittstelle, Server und Verbindungen, Kopfposition.

Das Blatt lässt sich mit dem Mausrad rollen (das beendet *Blatt folgen*) und mit **Strg + Mausrad** vergrößern; *Blatt einpassen* passt es der Fensterbreite an.  Das Papier ist 15 Zoll breit (3000 Punkte zu 1/200 Zoll); eingepasst sind die Punkte klein, vergrößert erscheinen sie einzeln.

## Das Bedienfeld

| Element | Wirkung |
|---|---|
| **Netz** | Netzschalter.  *Aus*: der Drucker steht, alle Lampen sind dunkel, das Blatt bleibt.  *Ein*: Neustart (/RESET). |
| **ONLINE** | Online/Offline (rastend).  Offline druckt der Drucker nicht und nimmt PV1, PV2 und TEST an. |
| **PV1** | Papiervorschub: kurz = eine Zeile, gehalten (ab etwa 0,85 s) = Formularvorschub.  Nur offline. |
| **PV2** | Papiervorschub der zweiten Bahn.  Ohne Wirkung: der SD 1157/264 hat nur eine Bahn, und die zweite Bahn des Typs /267 wird nicht nachgebildet. |
| **TEST** | Selbsttest: Titelzeile und zwei Zeilen Zeichensatz.  Siehe unten: wirkt nur nach einem Offline-Einschalten. |
| **Kopf an** | Druckkopf dauernd angeschwenkt (sonst nur während des Drucks). |
| **6 lpi** | Zeilenabstand 6 oder 8 Zeilen je Zoll; wirkt nach dem nächsten Neustart. |
| **Papier da** | aus = Papierende. |

**Lampen:** *Netz* (grün), *ONLINE* (grün: bereit), *FEHLER* (rot: Havarie, z. B. Farbbandhavarie, oder Zeitüberschreitung beim Papiervorschub).

### TEST wirkt nur nach einem Neustart im Offline-Zustand

Das ist das Verhalten der Original-Firmware, kein Fehler des Emulators.  Die Firmware wertet TEST nur aus, wenn sie **offline eingeschaltet** wurde: also **ONLINE ausschalten, dann *Neustart***, danach TEST drücken.  Schaltet man ONLINE nur im Betrieb aus, bleibt die ONLINE-Lampe an, PV1/PV2 wirken, TEST nicht.  Unter dem Bedienfeld erscheint dann ein Hinweis mit dem Knopf *Neustart jetzt*.  Ebenso liest die Firmware die Tasten *6 lpi* und *ONLINE* für die Lampe nur beim Einschalten.

### K6311: Hardware erschlossen [?]

Zum K6311 (Vorläufer des K6313) gibt es **keinen Schaltplan und kein Handbuch**.  Alles, was der Emulator von ihm weiß, ist aus dem Programm im ROM (Abzug 12128062, V.24) **erschlossen** und mit [?] zu lesen.  Die Firmware läuft an, richtet die serielle Schnittstelle ein (Baudrate und Datenformat liest sie an den Ports BCH/BDH, die der Emulator mit Vorgaben belegt: 9600 Bd, 8 Bit, keine Parität, ein Stoppbit) und nimmt Text über V.24 an.  **Gedruckt wird nichts**: die Belegung von Wagenmotor, Formularmotor, Nadeln, Tasten und der Lampen ON LINE/ERR ist am K6311 anders als am K6313 und nicht bekannt; deshalb zeigt das Bedienfeld nur die Netzlampe, und das Blatt bleibt leer.  Das ist eine Lücke der Nachbildung, kein Fehler der Firmware.

### K6313: Stand der Nachbildung

Der K6313 läuft mit seiner Original-Firmware (Fassung kurz, V.24-Kassette) bis in die Grundstellung: Wagen synchronisiert, ON LINE leuchtet.  **Die Hardware ist aus der Firmware erschlossen**, nicht aus einem vollständigen Plan; Angaben mit [?] in den Hinweistexten der Tasten und DIL-Schalter sind Annahmen.  Der Selbsttest (LF vor bzw. LF zurück beim Netz-Ein gehalten) und Text über V.24 drucken; das Druckbild stimmt Punkt für Punkt mit dem Zeichengenerator im ROM überein.  IFSS, Centronics und die 16-KiB-Fassung fehlen.  Die DIL-Schalter 5-1…18-2 liegen in einer Matrix; 5-2 muss EIN bleiben, sonst startet die Firmware nicht im Normalpfad.

### K6316: Einschalttests und Vorsteckbeleg

Beim K6316 wertet die Firmware die Tasten beim Einschalten erst **nach der Synchronisation** aus (etwa 8 Sekunden).  Die Taste also so lange halten:
**S5** = Selbsttest mit Druck, **S5+S4** = Selbsttest ohne Druck, **S1** = Lichtschrankentest (FEHLER leuchtet bei freier oberer, ONLINE bei freier unterer Lichtschranke; S1 erneut beendet ihn),
**S2** = Hexdump (jedes empfangene Byte als zwei Hexziffern, 32 je Zeile; die Schnittstelle überträgt nur 7 Bit), **S3** = Test der Vorsteckeinrichtung.

Der Kasten **Vorsteckbeleg** (nur K6316) steckt einen Beleg der gewählten Größe vor die Lichtschranke bzw. nimmt ihn wieder heraus.  Einziehen, Positionieren und Auswerfen
macht die Firmware: auf Befehl des Rechners oder offline mit **S4** (Auswurf).  Was gedruckt wird, solange der Beleg unter der Druckzeile liegt, erscheint auf dem Belegblatt im Kasten, nicht auf dem Endlospapier.
Die Lage der Lichtschranken zur Druckzeile ist aus der Firmware erschlossen, nicht am Gerät gemessen.

## Einstellungen

* **Druckertyp.**  Wählbar sind SD 1157/264, K6316 (IFSS und V.24) K6313 (V.24) sowie K6311 (V.24, Hardware erschlossen, druckt nicht).  Grau und nicht wählbar sind: SD 1157/267 (nicht vorgesehen, die zweite Papierbahn wird nicht nachgebildet), SD 1152/257 Typenrad (in Vorbereitung).  Ein Wechsel legt den Drucker neu an (Rückfrage); das Blatt wird dabei geleert.
* **ROM-Fassung.**  *repariert* ist die Vorgabe.  Im Original-Abzug des SD-1157-EPROMs ist vermutlich ein Bit gekippt (Byte 0D34H): der Eingabepuffer leert sich nie, und über die Schnittstelle bleibt der Drucker nach dem ersten Zeichen stehen.  Die reparierte Fassung ändert nur dieses eine Byte.  *Original-Abzug* ist der unveränderte Abzug (der Selbsttest läuft damit).
* **Schnittstelle.**  Der Schalterblock des Druckers wählt, ob die Firmware die Schnittstelle *IFSS* oder *V.24* benutzt.  Ein Wechsel startet den Drucker neu.
* **Tempo.**  Rechengeschwindigkeit gegenüber dem echten Drucker.
* **Seitenlänge.**  4, 6, 8 oder 12 Zoll: so wird das Blatt für PDF und Druck in Seiten geteilt (im Bild als gestrichelte Linie).  Der Formularvorschub der Firmware fährt im Emulator fest 12 Zoll.

## Schnittstellen und Verbindung zu einem Rechner

Der Reiter *Schnittstellen* ist derselbe wie bei den Rechner-Emulatoren: je Schnittstelle (**IFSS**, **V.24**) Betriebsart *Telnet* oder *RFC 2217*, Rolle *Server* oder *Client*, Host, Port.  Die Vorgabe ist ein **Telnet-Server auf Port 5000** (IFSS) bzw. 5001 (V.24); im Rechner-Emulator verbindet man dessen Druckerschnittstelle (z. B. beim A5120 *Drucker*) als Client mit `127.0.0.1:5000`.  Der Drucker benutzt die Schnittstelle, die im Reiter *Drucker* gewählt ist.

**Flusssteuerung (Gastverhalten):** Die Firmware steuert den Fluss selbst mit **DC1/DC3** (XON/XOFF): DC1 nach dem Einschalten, DC3 bei Druckbeginn einer Zeile und bei vollem Puffer (nach etwa 1 000 Zeichen), DC1 wieder bei leerem Puffer und fertigem Druck.  Eine Gegenstelle, die DC3 nicht beachtet, verliert Daten hinter etwa 1 KiB.  Am V.24 kommt das erste DC1 erst, wenn eine Verbindung besteht.  Offline im Betrieb druckt der Drucker den Auftrag zu Ende, gibt aber kein DC1 mehr aus, bis er wieder online ist.

## Ausgabe: PDF und Drucken

* **Blatt als PDF speichern** schreibt eine Datei mit einer Seite je Formular.  Jeder Nadelpunkt ist ein schwarzer Kreis in Mechanikmaß (Zeilen zu 1/144 Zoll, Punkte zu 1/200 Zoll); die Seite ist 15 Zoll breit und so hoch wie die gewählte Seitenlänge.  Leere Seiten am Ende entfallen.
* **Blatt drucken** öffnet den Druckdialog des Betriebssystems.  Das 15 Zoll breite Blatt wird auf die Papierbreite verkleinert (nie vergrößert); die Seiten brechen an der Seitenlänge oder, wenn das Papier kürzer ist, an dessen Ende.
* **Blatt leeren** löscht das Blatt; die Papierlage bleibt.  Speichern Sie es vorher als PDF.

## Tastenkürzel

Alle Kürzel tragen Strg+Umschalt.

| Kürzel | Wirkung |
|---|---|
| Strg+Umschalt+D | Blatt als PDF speichern |
| Strg+Umschalt+P | Blatt drucken |
| Strg+Umschalt+Q | Beenden |
| Strg+Umschalt+N | Netzschalter |
| Strg+Umschalt+R | Neustart (Netz aus und ein) |
| Strg+Umschalt+O | ONLINE umschalten |
| Strg+Umschalt+L | Blatt leeren |
| Strg+Umschalt+B | Blatt einpassen |
| Strg+Umschalt+F | Blatt folgen |
| Strg+Umschalt+H | Handbuch |

## Aufruf

`bash run_k1520printer.sh` startet das Programm.  `--instance NAME` öffnet eine zweite, eigene Konfiguration (`k1520printer-NAME.yaml`) neben der ersten, `--config DATEI` legt die Konfigurationsdatei fest, `--paths` zeigt die aufgelösten Pfade.  Gespeichert werden Drucker, ROM-Fassung, Schnittstellen, Tempo, Ansicht und Fenster.

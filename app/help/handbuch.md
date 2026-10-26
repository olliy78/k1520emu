# a5120emu / k8915emu / prg710emu / pc1715emu / p8000emu / mc80emu — Kurzhandbuch

Version {{VERSION}}

Dieses Programm ist ein Emulator für Bürocomputer. Es enthält den **A5120 Emulator**
(`a5120emu`), den **K8915 Emulator** (`k8915emu`), den **PRG710 Emulator**
(`prg710emu`), den **PC1715 Emulator** (`pc1715emu`), den **P8000 Emulator**
(`p8000emu`) und den **MC80 Emulator** (`mc80emu`) — mit eigener Konfiguration und eigener Tastatur.
Was nur den K8915, den PRG, den PC 1715, den P8000 bzw. den MC80 betrifft, steht in den Abschnitten
„Der K8915 Emulator", „Der PRG710 Emulator", „Der PC1715 Emulator", „Der P8000 Emulator" und
„Der MC80 Emulator" (die Bedienung des P8000 selbst — Selbsttest, Monitore, UDOS, WEGA — beschreibt das
Kapitel „Bedienung des P8000").
Nachgebildet werden **Bus und Steckkarten** — Z80,
Speicher, Bildschirmkarte, Tastatur und Diskettensteuerung; der Z80-Code von
Boot-ROM, BIOS und Betriebssystem läuft darin **unverändert**. Es gibt deshalb
keine eingebauten Abkürzungen und keine Betriebssystem-Nachbauten: was auf der
eingelegten Diskette steht, startet so, wie es 1985 gestartet ist — CP/A, SCPX,
UDOS.

Zum Dateiaustausch mit den Disketten gibt es das Schwesterprogramm
**k1520DiskTool**; es hat sein eigenes Handbuch.

## Der übliche Weg

1. **Starten.** Das Fenster kommt hoch, die Maschine ist bereits eingeschaltet
   und startet kalt — mit den Disketten, die beim letzten Mal eingelegt waren.
   Beim allerersten Start ist noch keine eingelegt; dann bleibt der Bildschirm
   nach dem Boot-ROM stehen.
2. **Diskette einlegen** — *Datei ▸ Diskette einlegen ▸ Laufwerk A:* oder der
   Knopf *Mount* im Kasten „Laufwerke". Angenommen werden `.hfe`, `.dmk`,
   `.img`, `.cqm` (CopyQM), `.td0` (TeleDisk) und `.imd` (ImageDisk).
3. **Kaltstart.** Eine frisch eingelegte Diskette bootet nicht von selbst — der
   Rechner sieht sie erst beim nächsten Start: *Maschine ▸ Rückstellen*
   (Strg+Umschalt+R).
4. **Arbeiten.** Ein Klick in die Bildröhre, dann tippen: die echte Tastatur
   bedient den emulierten Rechner. Die Bildschirmtastatur (*Ansicht ▸ Tastatur*)
   zeigt mit und nimmt auch Mausklicks an.
5. **Alles wird gemerkt.** Fenstergröße (auch „maximiert"), die Aufteilung
   zwischen den Kästen, die Symbolleiste, die Bildröhre, die
   Laufwerksbestückung und die eingelegten Disketten stehen beim nächsten Start
   wieder so da. Von Hand speichern muss man nur, wer **mehrere** Einrichtungen
   nebeneinander führen will (*Datei ▸ Konfiguration speichern…*).

## Das Fenster

**Bildröhre** — die Nachbildung des Bildschirms, samt Nachleuchten, Zeilenraster
und Wölbung. Sie hat den Tastaturfokus: was man tippt, geht an den emulierten
Rechner, nicht an die Oberfläche. Doppelklick oder F11 macht sie zum Vollbild,
Esc kommt zurück.

**Tastatur** — eine maßstäbliche Nachbildung der K7637. Sie zeigt an, welche
Taste die echte Tastatur gerade anspricht, und trägt die Anzeigen des Rechners
(Feststeller, Betriebsanzeige). Beim ersten Start ist sie zugeklappt.

**Laufwerke** — je bestücktem Steckplatz ein Kasten: Leuchte, Dateiname,
Schreibschutz, das erkannte Format, und die Knöpfe zum Einlegen, Anlegen,
Speichern unter und für die echte Diskette.

**Lochstreifen** — Leser und Stanzer, nur solange die Lochstreifen-Karte steckt
(siehe *Lochstreifen* unten).

**Kassette** — die beiden Kassettenlaufwerke, nur solange die Kassetten-Karte steckt
(siehe *Kassette* unten).

**AUP** — Sparkassentastatur K7633, Kundentastatur, Schlüsselschalter und Lampen,
nur solange die AUP steckt (siehe *AUP (Sparkasse)* unten; nur im A5120 Emulator).

**Einstellungen** — vier Reiter: *Allgemein* (Modell, Takt, RAM-Disk, Lochstreifen, Kassette, AUP), *Laufwerke*
(welcher Laufwerkstyp in welchem Steckplatz steckt), *Schnittstellen* (die seriellen
Anschlüsse des Rechners nach außen, ein Block je Schnittstelle — siehe *Serielle
Schnittstellen* unten) und *CRT* (das Aussehen der Bildröhre).

Jeder dieser Kästen lässt sich zuklappen, herausziehen und woanders andocken;
*Ansicht* holt ihn zurück.

**Symbolleiste** — die Abkürzung für die häufigen Wege; ausblendbar unter
*Ansicht ▸ Symbolleiste* und einrichtbar (siehe unten). **Im Menü steht immer
alles** — was man aus der Leiste wirft, bleibt erreichbar.

**Statuszeile** — links die letzte Meldung, rechts der Zustand der Maschine:
Takt, und je Laufwerk eine Leuchte mit dem Namen der eingelegten Diskette.
Dazu kommen — **nur wenn es etwas zu sagen gibt** — die seriellen Schnittstellen.

## Die Symbolleiste einrichten

*Ansicht ▸ Symbolleiste einrichten…* öffnet eine Liste: angehakt heißt „steht in
der Leiste", und die Reihenfolge der Liste ist die der Leiste — Einträge lassen
sich mit der Maus verschieben. *Trennstrich einfügen* setzt eine Lücke zwischen
zwei Gruppen, *Standard* stellt die Leiste des ersten Starts wieder her.

Ob die Knöpfe Symbol, Text oder beides tragen, steht unter *Ansicht ▸
Symbolleistenstil*. Beides — Inhalt und Stil — wird mit der Konfiguration
gemerkt.

## Die Statuszeile lesen

**Takt** — der eingestellte Takt der Maschine, wortgleich mit dem Auswahlfeld
unter *Einstellungen ▸ Allgemein*: `2,45 MHz` (Echtzeit), `2 × 2,45 MHz`,
`5 × 2,45 MHz` oder `10 × 2,45 MHz`. Was der Wirtsrechner davon
tatsächlich hält, steht im **Tooltip** („Gemessen: 9,8 × 2,45 MHz") — samt dem
Hinweis, wenn er nicht mitkommt. Als Anzeige taugte der gemessene Wert nicht:
er schwankt von Sekunde zu Sekunde und liest sich wie ein Fehler, wo keiner ist.

**Je Laufwerk eine Leuchte und ein Feld:**

| Leuchte | Bedeutung |
|---------|-----------|
| leerer Kreis | keine Diskette im Laufwerk, kein Zugriff |
| schwarzer Kreis | Diskette eingelegt |
| roter Kreis | das Laufwerk ist angesprochen |

Rot leuchtet auch ein **leeres** Laufwerk, sobald es angesprochen wird — genau
wie die Leuchte am echten Gerät. Das ist die Auskunft, auf die es dann ankommt:
das Gastsystem wartet auf eine Diskette, die niemand eingelegt hat.

Daneben `A: cpa780.hfe  R/W`: Buchstabe, Name der eingelegten Abbilddatei und ob
die Maschine darauf schreiben darf (`R/W`) oder nicht (`R/O`). Der volle Pfad
steht im Tooltip. Ein nicht bestückter Steckplatz bekommt weder Leuchte noch
Feld. So sieht man den Diskettenzugriff auch bei zugeklapptem Laufwerkskasten.

**Serielle Schnittstellen** — zwei Felder hinter dem Takt, die nur erscheinen,
wenn sie etwas zu sagen haben: `Telnet/RFC2217 Server Port: 5000` (auf welchen Ports
ein Server tatsächlich lauscht — das ist nicht immer der eingestellte, siehe unten)
und `V.24 verbunden, Drucker verbunden` (welche Schnittstellen eine Verbindung
haben). Ein Client, der noch versucht, und eine Ausgabe in eine Datei erscheinen dort
nicht; sie stehen im Block. Der Tooltip nennt Protokoll und Gegenstelle.

Zykluszähler und Bildrate standen hier früher. Beide sagen über die Maschine
nichts, was man beim Arbeiten wissen will.

## Ein- und Ausschalten, Rückstellen

**Einschalten ist immer ein Kaltstart**: das Boot-ROM wird wieder eingeblendet,
alle Disketten neu eingelegt, der Rechner läuft vom Anfang an. Das ist der
Unterschied zum echten Netzschalter nicht — beim A5120 war es genauso.

**Rückstellen** (Strg+Umschalt+R) ist das systemweite `/RESET`: Neustart vom
Boot-ROM, ohne dass der Rechner zwischendurch aus war. Das ist der Weg, nachdem
man eine Diskette gewechselt hat.

**Ausschalten** hält den Lauf an und macht die Röhre dunkel. Änderungen an einer
Diskette sind da längst in der Datei — zurückgeschrieben wird kurz nach jedem
Schreibzugriff, nicht erst beim Beenden.

## Disketten einlegen und auswerfen

Über *Datei ▸ Diskette einlegen ▸ Laufwerk …* oder den Knopf im Laufwerkskasten.
Ein Laufwerk nimmt nur eine Diskette: wo schon eine liegt, ist der Menüpunkt
gesperrt — erst auswerfen.

**Nach dem Format wird gefragt — aber nur bei `.img`.** Ein `.hfe` oder `.dmk`
trägt seine Geometrie selbst; da gibt es nichts zu wählen. Ein rohes
Sektorabbild enthält dagegen nur die Sektorinhalte, und wie sie sich auf Spuren
verteilen, steht nirgends darin. Deshalb geht nach der Dateiauswahl ein Dialog
auf. Vorgeschlagen wird das Katalogformat, dessen Abbildgröße genau zur Datei
passt — das einzige Merkmal, das ein `.img` über sich selbst preisgibt.

**Hinter „Format:" steht der Befund**, nicht eine Einstellung: der Emulator
vermisst die eingelegte Diskette und hält das Gemessene gegen den Formatkatalog.
Steht dort **unbekannt**, passt kein Eintrag — oder es passen zwei gleich gut.
Lesen und Schreiben geht trotzdem, der Diskettencontroller arbeitet
formatunabhängig; nur als `.img` lässt sich so eine Diskette nicht speichern
(siehe unten). Der Befund zieht im laufenden Betrieb nach: formatiert das
Gastsystem die Diskette um, steht dort kurz darauf das neue Format.

**Schreibschutz** — der Haken *Write-Protect* wirkt sofort, auch bei laufender
Maschine, und steht in der Statuszeile als `R/O`. Bei einer unersetzlichen
Diskette ist er die billigste Versicherung.

**Änderungen gehen in die Datei zurück**, und zwar von selbst: kurz nach der
letzten Schreibbewegung (eine Schreibpause von etwa einer halben Sekunde
Maschinenzeit). Ein Formatierlauf schreibt die Datei deshalb einmal am Ende neu
und nicht einmal je Spur.

## Leere Disketten und „Speichern unter"

*Leere Diskette* legt eine **echte Leerdiskette** an — unformatiert, in der
Geometrie des Laufwerks. Genau das ist der Anwenderfall: das Gastsystem
formatiert sie selbst (`FORMAT.COM` unter CP/A, ebenso UDOS). Dafür braucht es
einen Behälter, der „unformatiert" ausdrücken kann, also `.hfe` oder `.dmk`. Ein
rohes `.img` kennt diesen Zustand nicht; wählt man es, wird vorformatiert
angelegt — und dann wird nach dem Format gefragt.

*Speichern unter…* schreibt die eingelegte Diskette unter neuem Namen — auch in
einen **anderen Behälter** (`.img` → `.hfe`, `.hfe` → `.dmk`) — und arbeitet ab
dann dort weiter. Ausgegeben wird im **erkannten** Format; gefragt wird hier
nicht. `.img` ist aus zwei Gründen verwehrt: wenn die Diskette etwas trägt, was
ein Sektorabbild nicht darstellen kann (eine unformatierte Spur, oder Daten
hinter der Datenprüfsumme — dort legt UDOS seine Verkettung ab), und wenn das
Format **unbekannt** ist. Im zweiten Fall wäre die Sektorreihenfolge des Abbilds
geraten, und ein geratenes Abbild sieht heil aus und ist es nicht.

## CopyQM-Abbilder

Ein `.cqm` (Abbild des Programms CopyQM) lässt sich einlegen wie jedes andere Abbild und
ist **les- und schreibbar**: der Haken *Write-Protect* ist frei setzbar, die Statuszeile
zeigt `R/W` bzw. `R/O` nach dem tatsächlichen Schutz. Die Geometrie steht in der Datei,
ein Formatdialog erscheint deshalb nicht. Bezeichnung, Etikett, Interleave, Prüfwert und
der **Kommentar** des Abbilds stehen als Tooltip am Abbildnamen und bleiben beim
Zurückschreiben erhalten. Je nach Rechner zeigt der Laufwerkskasten Hinweise zu den Grenzen
des Formats (UDOS, Bootdisketten) — kein Meldungsfenster.

**Grenzen.** CopyQM speichert Sektorbytes und **eine einzige** Geometrie: eine
Sektorgröße, eine Sektorzahl, MFM mit voller Datenrate, lückenlose Sektornummern. Nicht
darstellbar und deshalb als `.cqm` abgelehnt (die Datei bleibt dann unverändert; im
Laufwerkskasten steht „Abbild nicht speicherbar: …“, das Gastsystem läuft mit dem
Speicherabbild weiter) sind:

* **gemischte Formate**, z. B. CP/A-Bootdisketten mit 128-Byte-Systemspuren,
* **UDOS** auf A5120 und PRG 710: die Dateiverkettung und der Sektorkontrollblock stehen
  hinter der Datenprüfsumme (UDOS1715/NDOS legt die Verkettung in eigene Sektoren und geht),
* **FM**-Spuren und halbe Datenrate,
* **unformatierte** Disketten und Spuren.

*Speichern unter…* und *Leere Diskette* bieten `.cqm` an und nennen bei einem
ungeeigneten Medium den Grund im Dialog. Wofür welches Format: Bootdisketten und UDOS als
`.hfe`; Austausch und Archiv einheitlicher Disketten als `.cqm` oder `.img`. Der
Vergleich aller Formate steht im Handbuch des DiskTool („Containerformate im Vergleich“).

## TeleDisk- und ImageDisk-Abbilder

Ein `.td0` (TeleDisk) oder `.imd` (ImageDisk) wird wie ein `.cqm` eingelegt: les- und
schreibbar, kein Formatdialog (die Geometrie steht in der Datei). Version, Datum,
Datenrate, Spuren, Sektoren, der **Kommentar** und etwaige Warnungen stehen als Tooltip am
Abbildnamen. **Fehler- und Löschmarken** einzelner Sektoren bleiben erhalten; **UDOS** auf
A5120 und PRG 710 lässt sich in keinem der beiden Formate darstellen (Sektorkontrollblock
hinter der Datenprüfsumme), unformatierte Disketten ebenfalls nicht. Ein `.td0` wird immer
unkomprimiert („TD“) geschrieben; **Pseudosektoren** (Zusatzblöcke der Datei) bleiben nur
beim Speichern in `.td0` erhalten. *Speichern unter…* und *Leere Diskette* bieten beide
Endungen an und nennen bei einem ungeeigneten Medium den Grund im Dialog.

## Laufwerke bestücken

*Einstellungen ▸ Laufwerke* — vier Steckplätze der K5122, je einer wählbar:

| Typ | Was |
|-----|-----|
| K5601 | 5,25″ zweiseitig, 80 Spuren, MFM, 800K — die Standardbestückung |
| K5600.10 | 5,25″ einseitig, 40 Spuren, 200K |
| K5600.20 | 5,25″ einseitig, 80 Spuren, 400K |
| MF3200 | 8″ einseitig, 77 Spuren, nur FM, 300K |
| MF6400 | 8″ einseitig, 77 Spuren, FM und MFM, 600K |
| kein Laufwerk | der Steckplatz bleibt leer |

Der A5120 kam mit drei K5601 (A:, B:, C:); der vierte Platz war frei. Eine
Änderung baut die Maschine neu auf und startet sie kalt — der Laufwerkstyp steht
im Kern fest, sobald die Maschine läuft.

## Wenn die Diskette nicht zum Laufwerk passt

Sie wird trotzdem eingelegt — und **übersetzt**. 5,25″ gibt es mit 48 tpi (40
Spuren) und 96 tpi (80); ein Hinweis im Laufwerkskasten sagt dann, was gilt:

* *Double Step aktiviert* — 40-Spur-Diskette im 80-Spur-Laufwerk. Das Gastsystem
  muss sie schrittverdoppelt lesen, also im Betriebssystem einen
  40-Spur-Laufwerkstyp einstellen.
* *Laufwerk liest nur jede zweite Spur* — 80-Spur-Diskette im 40-Spur-Laufwerk.
  Bei einer einseitig im Doppelschritt beschriebenen Diskette ist das genau
  richtig.
* *Nur Seite 0 verwendbar* — zweiseitige Diskette im einseitigen Laufwerk.

Das ist kein Fehler, deshalb steht es im Kasten und nicht in einem
Meldungsfenster.

## Eine echte Diskette am Greaseweazle

Mit einem [Greaseweazle](https://github.com/keirf/greaseweazle)-Adapter und einem
echten Laufwerk lässt sich eine **echte Diskette** einlegen: Knopf *Physisch…*
im Laufwerkskasten. Gelesen und geschrieben wird **spurweise nach Bedarf** — der
Umweg „erst die ganze Diskette in eine Datei" entfällt, und der Rechner bootet
von der eingelegten Scheibe.

Drei Dinge, die man wissen sollte:

* **Zwei Schlösser.** Der Haken *Auf die echte Diskette schreiben* im Dialog
  entscheidet, ob überhaupt auf die Scheibe geschrieben werden darf — er ist
  gesetzt, denn im Emulator wird die Diskette benutzt und nicht angesehen. Nimmt
  man ihn heraus, liegt sie schreibgeschützt im Laufwerk. Das zweite Schloss ist
  der Haken *Write-Protect* im Laufwerkskasten: er wirkt wie die Kerbe am Rand
  der echten Diskette, sofort und auch bei laufender Maschine, das Gastsystem
  meldet dann „schreibgeschützt" — und weil so gar keine geänderte Spur entsteht,
  geht auch nichts an den Adapter. Bei einer unersetzlichen Diskette ist er die
  billigste Versicherung; in der Pfadzeile steht, was gerade gilt.
* **Geschrieben gilt erst nach dem Zurücklesen.** Jede geschriebene Spur wird
  sofort wieder gelesen und verglichen; der Füllstand steht im Laufwerkskasten.
* **Eine Spur, die sich nicht schreiben lässt**, meldet sich mit einem Fenster.
  Dann liegt das Abbild im Speicher noch heil, die Diskette nicht mehr — der
  Ausweg ist *Diskette neu beschreiben* auf eine frische Scheibe.

Das Paket `greaseweazle` ist eine freiwillige Zutat; fehlt es, sagt der Knopf,
woran es liegt.

## Serielle Schnittstellen

Die seriellen Anschlüsse des Rechners lassen sich mit der Außenwelt verbinden: mit
einem Terminalprogramm, mit einem anderen Rechner im Netz, mit einem zweiten Emulator
oder — über einen Adapter — mit einem echten seriellen Gerät wie einem Drucker. Was
der Rechner sendet, geht über das Netz hinaus, was ankommt, landet im Empfänger seiner
Schnittstellenkarte; das Programm im Rechner merkt davon keinen Unterschied zu einem
Kabel.

Eingestellt wird das im Reiter **Schnittstellen** im Kasten *Einstellungen*
(*Ansicht ▸ Einstellungen*). Je Schnittstelle gibt es einen Block; **die Namen und was
sie können, kommen vom Rechner** — so, wie sie am Gerät beschriftet sind:

| Rechner | Schnittstellen |
|---------|----------------|
| A5120 (Anschlusssteuerung K8025) | DFÜ/V.24, DFÜ/IFSS, Drucker |
| K8915 (Anschlusssteuerung K7028) | Drucker/IFSS1 an X3, V.24 an X4, DFÜ/IFSS2 an X5 |

**V.24** ist die Spannungsschnittstelle mit Steuerleitungen (RTS, CTS, DTR, DSR, DCD),
**IFSS** die Stromschleife des DDR-Standards — sie kennt nur Senden und Empfangen. Die
Tastatur steht als Zeile „fest verdrahtet" darunter; an ihr gibt es nichts zu stellen.

### K8915: den Prüfstecker gesteckt lassen

Der Selbsttest im Boot-ROM des K8915 prüft beim Einschalten auch die drei seriellen
Schnittstellen: er sendet auf jeder ein Zeichen und erwartet, dass **dasselbe Zeichen
zurückkommt** — am Gerät steckt dafür ein Prüfstecker bzw. ist die Stromschleife
geschlossen. Im Emulator übernimmt das der Schalter **Rx/Tx-Loop**, und der ist beim
K8915 deshalb **von Anfang an gesetzt**.

Fehlt das Echo, meldet der Selbsttest einen **SIO-Fehler** (Kennbuchstabe in der
Diagnosezeile), und der Rechner **wartet auf eine Taste**:
**RETURN** setzt den Start fort (`#` wiederholt den Selbsttest). Das ist harmlos — das
Betriebssystem selbst braucht das Echo nicht —, kostet aber bei jedem Kaltstart einen
Tastendruck.

Empfehlung: den Loop an allen Schnittstellen gesetzt lassen, die nicht verbunden
werden, und ihn nur an der **einen** abschalten, die man braucht. Weil der Zustand des
Schalters gemerkt wird, kommt die Meldung dann bei jedem Kaltstart wieder; wer das nicht
will, setzt den Loop vor dem Rückstellen wieder und schaltet ihn nach dem Start ab.

### Telnet — Terminalprogramme und Rechner im Netz

**Telnet** ist das einfachste Protokoll, um Zeichen über eine Netzverbindung zu
schicken; fast jedes Terminalprogramm spricht es. Damit lässt sich zum Beispiel:

* **sich mit einem Terminalprogramm in den Emulator einwählen** — der Emulator ist
  **Server**, das Terminalprogramm verbindet sich zu ihm: unter Windows etwa
  **PuTTY** (Verbindungsart *Telnet*) oder **Tera Term**, unter Linux und macOS
  `telnet 127.0.0.1 5000`. Was man dort tippt, kommt am Rechner an; was ein Programm im
  Rechner auf die Schnittstelle ausgibt, steht im Terminalfenster. Das Echo macht —
  wie am Gerät — der Rechner, nicht das Terminalprogramm.
* **den Emulator als Terminal an einen anderen Rechner hängen** — der Emulator ist
  **Client** und wählt sich bei einem Telnet-Server ein, etwa einem Linux- oder
  Unix-Rechner mit `telnetd`, einem Mailbox-System oder einem zweiten Emulator. Ein
  Terminalprogramm im emulierten Rechner bedient dann die Gegenstelle.

**Die Steuerleitungen werden bei Telnet NICHT übertragen**, ebenso wenig Baudrate und
Zeichenformat. Was der Rechner auf RTS und DTR setzt, erfährt die Gegenseite nicht, und
die Eingänge CTS, DSR und DCD zeigen nur „verbunden" (aktiv, solange die Verbindung
steht). Für reinen Textaustausch ist das gleichgültig; ein Programm, das auf
Steuerleitungen achtet oder mit ihnen den Datenfluss bremst, braucht RFC 2217.

### RFC 2217 — eine echte V.24 über das Netz

**RFC 2217** (eigentlich „Telnet Com Port Control Option", festgelegt 1997 in der
gleichnamigen Norm RFC 2217 der IETF) ist eine **Erweiterung von Telnet**, die alles
mitnimmt, was eine serielle Leitung außer den Zeichen noch hat:

* die **Übertragungsparameter** — Baudrate, Datenbits, Parität, Stoppbits,
* die **Steuerleitungen** — RTS und DTR in die eine, CTS, DSR, DCD und RI in die andere
  Richtung —, dazu Break und das Leeren der Puffer.

Damit wird über das Netz eine **vollständige V.24 nachgebildet**: ein Programm im
Rechner, das mit RTS/CTS den Datenfluss steuert oder auf DCD wartet, verhält sich wie
am Kabel. Das Protokoll ist kein Allgemeingut, aber verbreitet: viele Netzwerk-Adapter
für serielle Geräte und die gängigen Werkzeuge zum Weiterreichen eines seriellen Ports
ins Netz sprechen es.

Zwei Dinge, die man dazu wissen sollte:

* **Maßgeblich ist der emulierte Rechner.** Baudrate und Format stellt das Programm im
  Rechner an seiner Schnittstellenkarte ein; der Emulator **meldet** sie der Gegenseite.
  Verlangt die Gegenseite andere Werte, wird das nicht übernommen, sondern unter
  *Gegenseite* mit Warnfarbe angezeigt. Ist der Emulator **Client** an einem echten
  Port, stellt sich dieser Port damit auf die Werte des Rechners ein.
* **Die Rolle bestimmt die Verdrahtung.** Als **Client** ist der Emulator ein Gerät an
  einem fernen Port — die Leitungen gehen gerade durch (RTS → RTS). Als **Server**
  spielt er selbst den Port, an den sich die Gegenseite anschließt; die Leitungen werden
  dann **wie in einem Nullmodemkabel gekreuzt** (RTS der Gegenseite → CTS des Rechners,
  DTR → DSR und DCD). Zwei Emulatoren, einer Server und einer Client, sind so über ein
  richtiges Nullmodemkabel verbunden.

### Echte serielle Geräte und Rechner anschließen

Der Emulator spricht keinen COM-Port des PCs direkt an. Stattdessen macht ein kleines
Programm einen **echten Port zum RFC-2217-Server**, und der Emulator verbindet sich als
**RFC2217-Client** dorthin. So lässt sich eine am PC eingebaute serielle Schnittstelle
ebenso anbinden wie ein **USB-Seriell-Adapter** — und daran ein echter Drucker, ein
Modem oder ein echter A5120 bzw. K8915 als Gegenstelle. Brauchbare Programme:

* **Linux:** `ser2net` (der übliche Weg, auch auf einem Raspberry Pi), `socat`
  (Pseudo-Port, der sich zu einem Server des Emulators verbindet).
* **Windows:** `com0com` mit `hub4com` (virtueller Port bzw. Weiterleitung eines echten
  Ports mit RFC 2217); für eine reine Telnet-Verbindung genügen PuTTY oder Tera Term.
* **Beide:** das Beispielprogramm `rfc2217_server.py` von **pyserial** (Python) macht
  aus einem lokalen Port einen RFC-2217-Server.

Es geht auch ohne PC dazwischen: **Netzwerk-Adapter für serielle Geräte** aus der
Industrie (sogenannte Geräteserver) beherrschen
RFC 2217, ebenso Bastelprojekte auf Basis von **Arduino**-Boards mit Netzanschluss
(ESP8266/ESP32) oder eines **Raspberry Pi** mit `ser2net`. Damit steht zum Beispiel ein
**echter serieller Drucker** irgendwo im Netz und druckt, was der emulierte Rechner
ausgibt.

Zwei Hinweise für die Hardware: eine V.24 arbeitet mit ±12 V, die Anschlüsse von
Arduino und Raspberry Pi mit 3,3 V bzw. 5 V — dazwischen gehört ein **Pegelwandler**.
Und eine **IFSS** ist eine Stromschleife (20 mA); für sie braucht es einen
**IFSS-Wandler**, ein V.24-Adapter allein genügt nicht.

### Der Reiter „Schnittstellen" im Einzelnen

**Betriebsart**

* **Telnet** — Zeichen über das Netz, ohne Steuerleitungen (siehe oben).
* **RFC2217** — wie Telnet, mit Baudrate, Format und Steuerleitungen.
* **Datei** — alles, was der Rechner sendet, wird in eine Datei geschrieben (etwa
  für einen Drucker). Beim Umschalten fragt ein Dialog nach dem Namen; **Starten**
  legt die Datei neu an (und überschreibt eine vorhandene).

**Rolle** (nur Telnet/RFC2217): als **Server** wartet der Emulator auf Verbindungen,
als **Client** wählt er sich selbst bei einer Gegenstelle ein (**Host** und **Port**).
Ein Server nimmt eine Verbindung zur Zeit an; jede weitere wird abgewiesen. Ist der
eingestellte Port belegt, nimmt **Starten** den nächsten freien darüber — der
tatsächliche steht im Kopf des Blocks („lauscht auf 5001") und in der Statuszeile.
Solange die Schnittstelle läuft, zeigt auch das (gesperrte) Port-Feld den **tatsächlich
benutzten** Port; nach dem Beenden steht wieder der eingestellte darin, und der wird
auch gespeichert.
Hinter dem Host steht, als was er gelesen wurde (IPv4, IPv6, Hostname oder
**ungültig** — dann bleibt der Knopf gesperrt).

**Der Knopf** heißt im Server *Starten*/*Beenden*, im Client *Verbinden*/*Trennen*.
Ein Client versucht **jede Sekunde** von neuem, bis die Gegenstelle antwortet — auch
nachdem sie die Verbindung getrennt hat. Das hört nur mit *Trennen* auf, und der Knopf
heißt schon während der Versuche so. Der Punkt im Kopf ist grau (aus), gelb (lauscht
oder verbindet), grün (verbunden) oder rot (Fehler); Fehler und Hinweise stehen als
Zeile im Block, nicht in einem Fenster. Solange eine Schnittstelle läuft, sind
Betriebsart, Rolle, Host, Port und Datei gesperrt.

**Weitere Einstellungen**

* **Rx/Tx-Loop** — der Prüfstecker: was der Rechner sendet, kommt am selben Anschluss
  wieder an; an einer V.24 sind zugleich RTS mit CTS und DTR mit DSR/DCD verbunden. Das
  schließt jede Verbindung aus; der Knopf ist gesperrt, solange es gesetzt ist, und
  eine laufende Verbindung wird beim Setzen beendet.
* **RTS/CTS-Brücke** (nur V.24) — am Stecker RTS mit CTS (und DTR mit DSR/DCD)
  verbunden: für Gegenstellen, die selbst keine Leitungen liefern (Telnet).
* **XON/XOFF** — den Empfang anhalten, solange der Rechner XOFF gesendet hat; so geht
  bei ungleich schnellen Enden nichts verloren. Bei Übertragungen von Binärdaten aus.
* **Takt** — wo der Rechner eine Brücke für die Taktquelle des Anschlusses hat.

Darunter zeigt der Block, womit der **Gast** (das Programm im Rechner) die
Schnittstelle gerade eingestellt hat, z. B. `Gast 9600 Bd 8N1`. Das Format steht in
der üblichen Schreibweise: Datenbits, Parität, Stoppbits — `8N1`, `7E1`, `8O1`
(Parität **N** keine, **E** gerade/even, **O** ungerade/odd, **M** mark, **S** space;
anderthalb Stoppbits als `1.5`). Der Tooltip erklärt das am konkreten Format.

Bei V.24 folgen die **Leitungen als Leuchten**: hinter **Ausgänge →** die Leitungen,
die der Rechner treibt (RTS, DTR), hinter **Eingänge ←** die, die er empfängt (CTS,
DSR, DCD). Die Beschriftung nennt die Richtung, nicht den Zustand. **Hellgrün
leuchtend** heißt aktiv, **gedimmt dunkelgrün** inaktiv, ein bloßer **Umriss**
unbekannt; der Tooltip nennt es ausgeschrieben („CTS aktiv"). Die Ausgänge leuchten
nur, wenn das Programm im Rechner sie setzt — CP/A und SCPX tun das nicht, dort
bleiben RTS und DTR dunkel. Die Eingänge sind unverbunden inaktiv; bei Telnet (und
Datei) werden sie aktiv, sobald eine Verbindung steht, bei RFC2217 zeigen sie, was die
Gegenseite meldet, mit **RTS/CTS-Brücke** oder **Rx/Tx-Loop** folgen sie RTS und DTR.
Angezeigt wird, was am Rechner anliegt — er muss dafür laufen; im angehaltenen Rechner
ändert sich nichts.

Bei RFC2217 steht eine weitere Zeile **Gegenseite**: Baudrate und Format, mit Warnzeichen
und Warnfarbe, wenn sie von denen des Gastes abweichen, dazu die Leitungen der Gegenseite
als Leuchten. Was bekannt ist, hängt von der Rolle ab: ein **Server** sieht die
Ausgänge RTS und DTR des Clients, ein **Client** die Eingänge CTS, DSR, DCD und RI des
Servers. Was die Gegenseite noch nicht gemeldet hat, bleibt Umriss.

**Beispiele**

* *Ein Terminalprogramm anschließen:* Schnittstelle auf **Telnet**, **Server**,
  **Starten**; dann in PuTTY Verbindungsart *Telnet*, Host `127.0.0.1`, Port 5000 (bzw.
  der Port aus der Anzeige) — oder `telnet 127.0.0.1 5000`.
* *Zwei Emulatoren koppeln:* im einen **Server**, im anderen **Client** mit
  Host `127.0.0.1` (oder dem Namen des anderen PCs) und dem Port des Servers, beide auf
  RFC2217 oder beide auf Telnet. Die Reihenfolge ist gleichgültig — der Client versucht,
  bis der Server da ist.
* *Einen echten Port des PCs verwenden:* den Port mit `ser2net` (Linux) bzw. `com0com`
  und `hub4com` (Windows) zum RFC-2217-Server machen; der Emulator verbindet sich als
  **RFC2217-Client** dorthin. Umgekehrt öffnet `socat -d -d pty,raw,echo=0
  tcp:127.0.0.1:5000` unter Linux einen Pseudo-Port auf den Server des Emulators.
* *Aus Python:* `serial.serial_for_url("rfc2217://127.0.0.1:5000")` (pyserial)
  gegen einen RFC2217-Server des Emulators.

**Sicherheit:** ein Server lauscht auf **allen Netzschnittstellen** des
PCs und verlangt **keine Anmeldung** — wer ihn erreicht, sitzt an der
Schnittstelle des Rechners. Für den Betrieb im offenen Netz gehört eine Firewall
davor, oder man bindet den Dienst über einen Tunnel (`ssh -L`) nach außen.

**Gemerkt wird** alles, was im Block steht, samt dem Zustand beim Beenden: was lief,
wird beim nächsten Start **wieder aufgenommen** — ein Server startet auf dem
eingestellten Port (ist der belegt, startet er **nicht**; der nächste freie wird ins
Feld eingetragen, und man startet von Hand), ein Client versucht wieder, eine Datei
wird **angehängt** statt überschrieben. Mit gesetztem Rx/Tx-Loop wird nichts
gestartet.

### Das Prüfprogramm SERTEST

**SERTEST** (*Serial Test*, `SERTEST.COM`) prüft die seriellen Schnittstellen eines
A5120 (unter CP/A), eines K8915 (unter SCPX 8915), eines PC 1715 (unter CP/A 1715), eines PC 1715W (unter SCP 3.0) und eines
PRG 710 bzw. PRG 710-1 (unter SCPX) — im Emulator ebenso wie am echten Gerät. Es steht auf den mitgelieferten Systemdisketten (`a5120_cpa_k5601_system` für den A5120,
`k8915scpx_boot1.hfe` für den K8915, `pc1715_cpa1715_system.hfe` für den PC 1715); gestartet wird es am Prompt mit `SERTEST`. Für den PC 1715W und die PRG
liegt es nicht auf den Lieferdisketten; mit dem DiskTool (`put`) auf eine SCP-3.0- bzw. SCPX-Diskette kopieren. Es
erkennt selbst, auf welchem Rechner es läuft, stellt die geprüfte Schnittstelle für die
Dauer der Prüfung auf 9600 Bd 8N1 und hinterher wieder so ein, wie das Betriebssystem
sie erwartet. Die Tastatur prüft es nicht (sie bleibt die Eingabe des Programms).

**Bedienung.** Ohne Argumente fragt SERTEST alles ab. Nach dem Start zeigt es den
erkannten Rechner und die Liste der Schnittstellen mit ihren Nummern:

```
Serial Test V0.3
Rechner: A5120 (K8025)
Schnittstellen:
  1  DFUE/V.24      SIO A33 Kanal A   V.24
  2  DFUE/IFSS      SIO A33 Kanal B   IFSS
  3  Drucker        SIO A32 Kanal B   IFSS
  -  Tastatur K7637 SIO A32 Kanal A   (Tastatur)
Tester (Aktiv) oder Gegenstelle (Passiv)? T/G
```

Am K8915 sind es `1 Drucker/IFSS1`, `2 V.24` und `3 DFUE/IFSS2`, am PC 1715 und am PC 1715W `1 Drucker` (nur Senden — sein Empfänger ist die Tastatur) und `2 V.24`, am PRG 710 `1 V.24`,
`2 IFSS Hauptdrucker` und `3 ZIFSS`, am PRG 710-1 `1 V.24` und `2 ZIFSS` (die Tastatur sitzt dort an der SIO A32-B und wird nie angefasst). Mit **T** wird der
Rechner zum **Tester**: SERTEST fragt je Schnittstelle `Test der … ? J/N` und für jede
gewählte, ob mit **Prüfstecker** und/oder mit **Gegenstelle** geprüft werden soll; am
Ende stehen eine Zusammenfassung und `SERTEST ENDE OK` bzw. `SERTEST ENDE FEHLER`. Mit
**G** wird er zur **Gegenstelle** für einen anderen Rechner (siehe unten).
**Strg+C** bricht an jeder Stelle ab; SERTEST stellt die Schnittstellen dann zurück.

Dasselbe ohne Rückfragen über die Kommandozeile:

```
SERTEST T n [/P] [/G] [/A]   Tester an Schnittstelle n
SERTEST G n                  Gegenstelle an Schnittstelle n
  /P nur Prüfsteckertest, /G nur Test mit Gegenstelle (ohne beide: beide)
  /A automatisch: keine Rückfragen, kein Warten auf eine Taste
  /M:A, /M:K, /M:P, /M:W, /M:R bzw. /M:S   Rechner A5120, K8915, PC 1715, PC 1715W,
                                           PRG 710 bzw. PRG 710-1 vorgeben (falls die Erkennung irrt)
```

Jedes Ergebnis steht als eigene Zeile da, z. B. `SERTEST DFUE/V.24 ECHO: OK`, sonst
`FEHLER` mit dem Grund oder `ENTFAELLT`, wenn ein Teil für diese Schnittstelle nicht
gilt (Steuerleitungen gibt es an einer IFSS nicht).

**Mit Prüfstecker** (`SERTEST T n /P`). Ein Prüfstecker verbindet am Anschluss den
Ausgang mit dem Eingang — was gesendet wird, kommt sofort zurück; an einer V.24 sind
außerdem RTS mit CTS und DTR mit DSR und DCD verbunden. Im Emulator ist das der
Schalter **Rx/Tx-Loop** der Schnittstelle, am Gerät ein gesteckter Prüfstecker bzw.
eine geschlossene Stromschleife. Geprüft werden:

* **DATEN-LOOP** — alle 256 Zeichenwerte, jedes muss unverändert zurückkommen. Ohne
  Prüfstecker endet das sofort mit `FEHLER KEIN ECHO BEI 00H`.
* **LEITUNGEN-LOOP** (nur V.24) — RTS und DTR werden in allen Kombinationen gesetzt;
  je Kombination steht eine Zeile mit dem, was an CTS und DCD gemessen wurde, und dem,
  was erwartet war.

Ein Prüfstecker sieht nicht alles: eine falsch eingestellte Baudrate fällt hier nicht
auf, denn Sender und Empfänger laufen mit demselben Takt. Das zeigt erst der Test mit
einer Gegenstelle.

**Mit einem anderen Rechner als Gegenstelle** (`SERTEST T n /G` auf dem einen,
`SERTEST G n` auf dem anderen). Zwei Rechner werden über ein **Nullmodemkabel**
verbunden (an einer IFSS: Sendeschleife des einen an die Empfangsschleife des anderen),
und zwar Schnittstelle gleicher Art — V.24 an V.24, IFSS an IFSS; die Nummern dürfen
sich unterscheiden. **Zuerst die Gegenstelle starten**, dann den Tester. Die Gegenstelle
schickt alles Empfangene zurück und läuft, bis man sie mit Strg+C beendet. Der Tester
prüft:

* **LEITUNGEN** (nur V.24) — schaltet RTS und DTR Schritt für Schritt und lässt sich von
  der Gegenstelle bestätigen, dass sie drüben ankommen (ohne `/A` je Schritt nach einem
  Tastendruck).
* **ECHO** — 4096 Bytes hin und zurück, Byte für Byte verglichen; dazu meldet die
  Gegenstelle ihre eigenen Empfangsfehler.
* **FLUSS-HW** (nur V.24) und **FLUSS-XON** — wie ECHO, aber die Gegenstelle bremst
  zwischendurch absichtlich, einmal über die Steuerleitung (RTS/CTS), einmal mit
  XOFF/XON. Bestanden ist der Teil nur, wenn sie wirklich gebremst hat und trotzdem
  nichts verloren ging.

Als Gegenstelle taugt ein zweiter Emulator ebenso wie ein echter Rechner:

* **Zwei Emulatoren:** im einen die Schnittstelle als **RFC2217-Server**, im anderen
  als **RFC2217-Client** auf dessen Port — das ergibt ein Nullmodemkabel samt
  Steuerleitungen. (Mit Telnet kommen nur ECHO und FLUSS-XON durch; die Leitungsteile
  brauchen RFC 2217.) Beim K8915 vorher den **Rx/Tx-Loop** dieser Schnittstelle
  abschalten. Für **FLUSS-XON** an der Schnittstelle der *Gegenstelle* den Schalter
  **XON/XOFF** setzen, für ECHO und FLUSS-HW dort **aus** lassen — dort gehen alle
  Bytewerte über die Leitung, auch das XOFF-Zeichen.
* **Ein echter A5120 oder K8915:** über ein Nullmodemkabel an einen Port des PCs, der
  wie oben beschrieben zum RFC-2217-Server gemacht ist; der Emulator ist
  **RFC2217-Client**. Ebenso lassen sich zwei echte Rechner direkt mit einem Kabel
  gegeneinander prüfen.

Typische Befunde: `ZEITUEBERLAUF BESTAETIGUNG` heißt, drüben läuft keine Gegenstelle,
das Kabel ist nicht gekreuzt oder die Baudraten passen nicht zusammen; `FEHLER RR1 …`
oder `FALSCH …` bei bestandenem Prüfstecker deuten auf einen falschen **Baudtakt**
einer Seite (am Gerät eine Brücke auf der Schnittstellenkarte). Ohne Gegenstelle endet
ein Tester immer mit `SERTEST ENDE FEHLER` — das ist richtig so.

## Der Takt der Maschine

*Einstellungen ▸ Allgemein ▸ Takt*: `2,45 MHz` — das ist der Takt des echten
A5120 — sowie `2 ×`, `5 ×` und `10 × 2,45 MHz`.  Schafft der Wirtsrechner eine Stufe
nicht, läuft die Maschine so schnell er kann; die Oberfläche bleibt bedienbar.

`2,45 MHz` ist das, was die Uhr des Gastsystems erwartet — sie zählt Taktzyklen
und geht bei jedem Vielfachen entsprechend falsch. Schneller ist gut, um einen
Kaltstart abzukürzen; wer die Uhr braucht, bleibt beim Nenntakt.

Der eingestellte Takt steht links in der Statuszeile, der **gemessene** in
dessen Tooltip.

## Modell und Optionen

*Einstellungen ▸ Allgemein ▸ Modell* kennt drei Rechner: **A5120**, **A5130** und
**K8924**. Ein Modell ist nur eine Sammlung von **Vorgaben** — die Wahl trägt Werte
in die Optionsfelder darunter, in das Feld *Kassette* und in die Laufwerke ein,
**ohne Rückfrage**. Danach lässt sich jede Einzelheit ändern; wer das Modell erneut
wählt, bekommt wieder dessen Vorgaben. Im Modellfeld steht dafür kein „(angepasst)“.
Der Fenstertitel folgt dem Modell („A5120 Emulator“, „A5130 Emulator“,
„K8924 Emulator“); das Programm heißt weiter `a5120emu`.

| | A5120 | A5130 | K8924 |
|---|---|---|---|
| RAM | K3526 64 KB | K3526 64 KB | K3526 64 KB |
| Bildschirmkarte | K7024 24x80 | K7024 24x80 | K7024 24x80 |
| Tastatur | K7637 | K7636 | K7637 |
| Lade-ROM | V0.9_2 | V0.9_2 | V0.9_2 |
| Laufwerke | 3 × K5601 | 4 × K5601 | 3 × K5601 |
| Kassette | aus | aus | aus |

Die Optionen stehen unter *Einstellungen ▸ Allgemein*: *RAM*, *Bildschirmkarte*,
*Tastatur*, *Lade-ROM* und *Erweiterungsmodul*, dazu *Kassette* (Abschnitt
*Kassette*) und die Laufwerke (*Einstellungen ▸ Laufwerke*). **Jede Änderung baut
die Maschine neu auf** — wie ein Kaltstart: es sind Steckkarten und Brücken, keine
Betriebsarten, die sich im Lauf umlegen ließen. (Einzige Ausnahme: der
Bildschirm-Umschalter, siehe unten.)

**Ein Konfigurationsstand aus einer früheren Fassung** (Modelle „A5120.16“,
„A5120 (64×16, K7636)“, „A5120 Kassette“, „A5120 BCU 880“) wird **nicht
umgesetzt**: der unbekannte Modellname gilt als nicht gesetzt, es greift die Vorgabe
des A5120. Die Kombinationen von früher sind jetzt Optionen — etwa „64×16 mit K7636“
als *K7023* + *4x K3525* + *K7636* und Laufwerke K5600.10 —, und wer sie neu
zusammenstellt, speichert damit die neue Konfiguration. Gültige einzelne Optionen aus
dem alten Stand bleiben erhalten.

**Laufwerke:** das Modell setzt sie nur im Augenblick der Modellwahl. Die alten
Geräte hatten drei Laufwerke K5600.10 (5¼″, 40 Spuren, einseitig); wer eines
nachstellt, stellt sie selbst ein (*Einstellungen ▸ Laufwerke*). Das
Doppelschritt-Verfahren (40-Spur-Diskette im 80-Spur-Laufwerk und umgekehrt) steht im
Abschnitt *Wenn die Diskette nicht zum Laufwerk passt*. **Viele alte Geräte sind
später auf K5601/K5122 umgerüstet worden**; wer Disketten aus einem solchen Gerät
hat, nimmt eher die 80-Spur-Laufwerke. Das Modell **A5120 8″** (K5602) gibt es nicht.

### Bildschirmkarte und Umschalter

*Bildschirmkarte* hat drei Einträge:

* **K7024 Auflösung 24x80** — nur die K7024 steckt (80 Zeichen × 24 Zeilen).
* **K7023 Auflösung 16x64** — nur die K7023 steckt (64 × 16, Zelle 8×16 Punkte, Bild
  512×256; der Kursor ist ein Balken in den untersten Zeilen der Zelle; die
  Helligkeitsmarken 04H–0FH der K7023.01 stellt der Emulator nicht dar).
* **K7024 und K7023 mit Umschalter** — beide Karten stecken, wie im A5130 und im
  K8924, in denen ein Schalter zwischen ihnen wählt.

Bei diesem dritten Eintrag wird der **Schiebeschalter** unter dem Feld bedienbar
(links *K7024 24x80*, rechts *K7023 16x64*), sonst ist er ausgegraut. Er schaltet
zwei Dinge zugleich:

* das **Videosignal** — welche Karte auf dem Bild erscheint. Das wirkt **sofort**,
  ohne Neustart, auch im laufenden Programm;
* die **Wickelbrücke** der Zentralen Recheneinheit (Port 0AH, Bit 6), an der das
  Betriebssystem erkennt, welches Format es schreiben soll. CP/A liest sie **nur beim
  Kaltstart** — nach dem Umlegen muss man den Rechner neu starten, damit CP/A das
  andere Format nimmt, genau wie am Gerät.

Beide Karten hängen immer am Bus; ein Schreibzugriff auf den Bildbereich
(FC00H–FFFFH) trifft beide. Das heißt: **ein Fehlbild auf der nicht passenden Karte
ist gewollt.** Läuft CP/A im Format 24x80 und der Schalter steht auf K7023, zeigt
diese Karte dieselben Bytes anders gedeutet — Müll. Ein Programm, das selbst im
16x64-Format nach FC00H schreibt, ist dagegen auf der K7023 richtig zu lesen und
zerstört nebenbei den unteren Teil von CP/As 24x80-Bild. Umschalten stellt das
richtige Bild her; ein Fehler des Emulators ist es nicht.

Dass der Schalter auch die Wickelbrücke umlegt, ist eine **Modellannahme**: Nach den
Schaltplänen führt kein Anschluss die Brücke aus dem Rechner heraus, am Gerät ist das
nicht belegt. Mit nur einer Karte gibt es keinen Schalter; die Brücke ergibt sich aus
der Karte. Der Schalterzustand wird gemerkt (`general.bildschirm_schalter`) und
steht auch im Zwischenstand. Es gibt dafür kein Tastenkürzel.

### Tastatur

*Tastatur* wählt die Bauart — bei **allen drei Modellen**, unabhängig von der
Bildschirmkarte und vom RAM:

* **K7637** (seriell, am Kanal der K8025) — Vorgabe bei A5120 und K8924.
* **K7636** (parallel am U-Bus der Zentralen Recheneinheit) — Vorgabe beim A5130. Das
  Tastenbild des A5130 ist das der K7636.01; eine eigene Bildschirmtastatur gibt es
  dafür nicht.

Die Bildschirmtastatur wechselt mit dieser Option, nicht mit dem Modell. Zur K7636:

* Kleinschrift ist die Grundstellung; SHIFT und LOCK liefern selbst keinen Code und
  schalten auf Großschrift (anders als an der K7634).
* **ET2** ist die Steuertaste (Control-Ersatz), **ET1** liefert FFH — ob daraus ein
  Zeilenende wird, hängt vom System ab (BCU 880 Satz 2: nur die Taste ↵).
* **DEL LINE** (13H) liefert ESC-Code, **INS LINE** (03H) ein eigenes Zeichen;
  die Tasten S…S9 sind PF0…PF9, INS MODE ist das numerische Minus.
* Die Lampen (Wahl 0–3, INS MODE, Fehler) setzt der Gast per Kommando; nur die
  K7636 hat sie.
* Nach dem Einschalten schickt die Tastatur ein Typzeichen (80H); BCU 880 holt es
  nicht ab — es erscheint als erstes Zeichen der ersten Eingabezeile. Erst eine
  leere Zeile eingeben.

### RAM

*RAM* wählt die Speicherkarten: **1x K3525 16KB**, **2x K3525 32KB**, **4x K3525
64KB** (je 16 KB ab 0000H im 4000H-Raster) oder **K3526 64KB**. Gesperrt ist nichts:
Software, die mehr Speicher braucht (CP/A will den oberen Bereich), läuft mit
weniger nicht. Die K3525-Karten stehen vor dem
Einschalten mit FFH im Speicher. Ein Erweiterungsmodul geht nur mit der K3526.

### Lade-ROM

*Lade-ROM* wählt das Programm in der Zentralen Recheneinheit, das nach dem
Einschalten die Diskette lädt:

* **V0.9_2** — die Vorgabe; startet CP/A, SCPX, UDOS und die übrigen Systeme.
* **V0.9_1** — die ältere Fassung desselben Programms.
* **V0.6** — ein anderes Programm: es liest Zylinder 1 und startet die BCU-880-
  Disketten „SYL auf Spur 1“ (Abschnitt *BCU 880*).

Das Lade-ROM des K8924 (V0.9a) ist ein handgepatchter Abzug und fehlt in der
Auswahl.

### Erweiterungsmodul (A5120.16)

*Erweiterungsmodul* steckt **EM064** oder **EM256** mit dem 16-Bit-Prozessor U8001
zusätzlich zum U880 — das ist der A5120.16. Es geht **nur am A5120** und nur mit
der K3526 als RAM; sonst ist das Feld ausgegraut (der gespeicherte Wert bleibt).

Mit Erweiterungsmodul zeigt die Statuszeile zwei zusätzliche Leuchten und den
Modus:

| Leuchte | Bedeutung |
|---------|-----------|
| V1 | RAMEN — das Erweiterungsmodul hat den Speicher eingeblendet |
| V2 | 8-Bit-Mode — die Steuerkarte fährt den U880-Bus |

**V1 zeigt nicht den Paritätsfehler**, auch wenn die Leuchte auf der Karte
dafür Platz hätte — nach dem Schaltplan hängt sie an RAMEN. Daneben steht
`Modus: 8-Bit` oder `Modus: 16-Bit`, je nachdem, welcher Prozessor gerade den
Bus hat. Ohne Erweiterungsmodul fehlen beide Leuchten.

**CP/A** (die vorhandenen Disketten von 1989) läuft mit K7023 und K7636 unverändert
bis `A>`; es erkennt die K7023 an der Wickelbrücke der Zentralen Recheneinheit und
die K7636 selbst.

### BCU 880

Das Betriebssystem BCU 880 (V1.2) bootet von den Disketten „SYL auf Spur 1“ bis
`A>`. Sie brauchen das Lade-ROM V0.6. Dafür unter *Einstellungen ▸ Allgemein*
einstellen: *Lade-ROM* **V0.6**, *Bildschirmkarte* **K7023 Auflösung 16x64**,
*Tastatur* **K7636** und *RAM* **4x K3525 64KB** (ein eigenes Modell dafür gibt es
nicht). Im Ordner der
Beispieldisketten liegen dafür unter `a5120_bcu/`:

| Datei | Inhalt |
|-------|--------|
| `a5120_bcu12_k5600-10_start-syl.hfe` | Startdiskette (BCU 880 V1.2, Satz 2 „SYL auf Spur 1“) — nach A: |
| `a5120_bcu12_k5600-10_arbeit-sys1.hfe` | Arbeitsdiskette „SYS1“ (dieselbe V1.2 ohne Lader; PIP, STAT, INIT, `KMBG.REL`, MBASIC, DDT …) — nach B:, nach dem Start auch nach A: |
| `bcu_kassette_formatiert.k52` | eine von BCU-INIT formatierte Kassette (für Gerät 2 = F:) |

So geht es:

1. **Laufwerke einstellen** (*Einstellungen ▸ Laufwerke*): A: und B: auf
   **K5600.10** (5¼″, 40 Spuren, einseitig). Die Disketten haben 40 Spuren; in
   einem 80-Spur-Laufwerk K5601 (der Laufwerkskasten meldet dann den
   Doppelschritt) findet das Lade-ROM den Lader auf Spur 1 nicht, der Rechner
   bleibt stehen. Das Modell stellt die Laufwerke nur bei der Modellwahl um, nicht
   bei einer einzelnen Option (siehe oben).
2. Startdiskette nach A:, Arbeitsdiskette nach B:, Kaltstart. Nach einigen
   Sekunden meldet sich `BCU 880 VERS 1.2` mit `A>`.
3. Zuerst eine **leere Zeile** eingeben (Typzeichen der K7636, siehe oben). Eine
   Zeile schließt die Taste **↵** ab (diese BCU-Fassung übersetzt ET1 nicht).
4. `dir b:` zeigt die Arbeitsdiskette. Die Startdiskette ist keine
   Dateidiskette — `dir a:` zeigt dort Systemcode.

* **Warmstart braucht die Diskette „ohne SYL“ in A:** — nach dem Kaltstart von
  der SYL-Diskette also die Arbeitsdiskette einlegen, bevor ^C oder ein Programm
  endet, sonst stürzt das System ab.
* **Kassette:** dazu unter *Einstellungen ▸ Allgemein* das Feld *Kassette*
  einschalten (das ergibt die Karte K5020 mit zwei Laufwerken K5200)
  und im Kasten *Kassette* ein Band in Gerät 2 legen, etwa
  `bcu_kassette_formatiert.k52` (vorher kopieren: der Emulator schreibt in die
  Datei zurück). `OPEN B:KMBG` lädt den Treiber (Meldung „kmbg opened“, `KMBG.REL`
  liegt auf der Arbeitsdiskette). Die Kassette im Gerät 2 ist **F:**, Gerät 1 ist E:.
  `B:INIT` formatiert sie (Antwort Y), danach `PIP F:=B:STAT.COM` und zurück. BCU
  wartet im Vorspann ohne Frist; eine frische Kassette muss man nicht erst
  zurückspulen.
* **INIT verlassen:** INIT fragt nach dem Formatieren „AGAIN?“ und endet nur mit
  Return (0DH), den die K7636.01 nicht liefert. Ausweg: während des Formatierens
  **DEL LINE** drücken (entspricht ^S), danach **INS LINE** (entspricht ^C) —
  Warmstart.
* **CP/A und SCPX** haben keinen Kassettentreiber: CP/A bedient Kassetten
  „außerhalb des BIOS“ (es hält dafür nur Interruptplätze frei), SCPX bringt nur
  die Bibliothek `KMBG.REL` für eigene Programme mit (auf der SCPX-Diskette
  zusammen mit dem Prüfprogramm `KMBGTEST`).
* **Version V2.1:** vom Gerät liegt kein Lade-ROM vor, das sie startet. Ein
  V2.1-System läuft nur als **Eigenbau** (V1.2-Lader mit V2.1-System und
  V1.2-Plattentreiber, `tools/bcu_v21_eigenbau.py`) — **kein Original**.

## MUTOS 8000 (A5120.16)

MUTOS 8000 ist ein UNIX für den 16-Bit-Teil des A5120.16 (U8001). Dieser Abschnitt ist eine
**Kurzanleitung** für den Rechner mit **drei K5601** (A:, B:, C:; die Standardbestückung).
Vorbereitung: *Einstellungen ▸ Allgemein ▸ Erweiterungsmodul* auf **EM256**, Laufwerke wie ausgeliefert
(*Einstellungen ▸ Laufwerke*). Bis jetzt nur im Emulator erprobt, nicht an echter Hardware; die
Schritte unten sind mit dem Kommandozeilen-Debugger durchgespielt, in dieser Oberfläche nicht
Schritt für Schritt abgenommen. MUTOS startet im **Einbenutzerbetrieb** (`single user login`);
einen Mehrbenutzerbetrieb gibt es noch nicht.

**Die Disketten** liegen im Ordner `a5120_16_MUTOS/` der Beispieldisketten (K5601, MFM):

| Datei | Rolle | Einhängepunkt |
|-------|-------|---------------|
| `a5120_mutos8000_boot_udos_k5601_nur_a.hfe` | Ladediskette (UDOS + Lader + Kernel) | – |
| `a5120_mutos8000_root_mfm.hfe` | Wurzel (`/bin`, `/etc`, `/dev`) | `/` |
| `a5120_mutos8000_cmd_mfm.hfe` | Befehle (`ls`, `cp`, `ed`, `mkfs` …) | `/mnt` |
| `a5120_mutos8000_usr_mfm.hfe` | C-Übersetzer (`cc`, `as`, `ld`) | `/usr` |
| `a5120_mutos8000_bin_mfm.hfe` | `adb`, `mt`, `tar` | `/tmp` |
| `a5120_mutos8000_cc1_mfm.hfe`, `a5120_mutos8000_cc2_mfm.hfe` | Inhalt der Original-Compilerdisketten (cc1: `as`, `cc`, `ld`, `ed`, Header, `c0`, `c1`; cc2: `c2`, `cpp`, `libc.a`, `nm`) | `/usr` (nur eine zugleich) |

Im **GitHub-Repository** (`disks/a5120_16_MUTOS/`) liegen außerdem die Fassungen für **MF3200**
(8″, FM; Dateien mit `_fm` im Namen), gemischte Bestückungen und weitere Ladedisketten. Eine Diskette
passt nur zur Aufzeichnung ihres Laufwerks: K5601 nimmt nur MFM, MF3200 nur FM.

**Arbeitskopien verwenden.** MUTOS schreibt in die Wurzel und in eingehängte Disketten; der
Emulator schreibt die Datei zurück. Die Originale deshalb zuerst kopieren und nur mit den Kopien
arbeiten.

### Starten (Einbenutzerbetrieb)

1. **Ladediskette `…_boot_udos_k5601_nur_a.hfe` in A:** einlegen. Diese Ladediskette meldet UDOS beim Start nur
   Laufwerk A: an; deshalb dürfen die **Wurzel in C:** und z. B. die **`cmd`-Diskette in B:** schon beim
   Einschalten eingelegt sein (die übrigen Ladedisketten brauchen B:–C: leer, sonst
   `DISK INITIALIZATION ERROR` bzw. `I/O ERROR`).
2. Rechner einschalten. UDOS zeigt das Menü `LOADER  M U T O S 8000`. Eingabe: `BOOT MUTOS`, RETURN.
3. Nach einigen Sekunden verlangt der Lader: `remove mutos8000 boot - disk` / `insert mutos8000 root - disk on rk2`.
   Liegt die Wurzel schon in C:, genügt `r` und RETURN (die Ladediskette wird nicht mehr gebraucht; sie darf in A:
   bleiben). Sonst jetzt die Wurzel in C: einlegen, dann `r`.
4. Es folgt `mem = 196608` und `single user login:` — Eingabe `root` (kein Kennwort). Prompt `#`.

### Weitere Disketten einhängen

Laufwerk = Gerät: **A: = `/dev/rk0`, B: = `rk1`, C: = `rk2` (Wurzel)**. A: ist jetzt frei (Ladediskette auswerfen).
Was noch nicht liegt, einlegen, dann als `root`:

    # /etc/mount /dev/rk0 /usr          (usr-Diskette in A:)
    # /etc/mount /dev/rk1 /mnt          (cmd-Diskette in B:)
    # ls /mnt/bin
    # cc -o /tmp/x /tmp/x.c             (C-Programm übersetzen)

Die Befehle der Disketten stehen ohne Pfad zur Verfügung (`PATH` in `/.profile`). Zum Wechseln:
**`/etc/umount /dev/rk0`**, erst danach die Diskette tauschen und neu einhängen (z. B. die `bin`-Diskette
nach `/tmp`). Vor dem Abschalten alles aushängen und `sync` eingeben.

> **Warnung: C: (`rk2`) ist auch der Swap-Bereich.** Die Wurzel belegt nur die ersten 250 Blöcke der
> Diskette; **ab Block 250 (etwa ab Spur 38 von 77) lagert MUTOS im laufenden Betrieb Prozesse aus und
> schreibt dort Daten** — ohne Meldung und ohne dass ein Dateisystem darauf wüsste. In C: darf deshalb
> **nie eine andere Diskette liegen**, solange MUTOS läuft: sie würde vermutlich zerstört. Aus demselben
> Grund nie ein Dateisystem mit mehr als 250 Blöcken auf der Wurzel anlegen (kein `mkfs … 494` dort).

### Mehrbenutzerbetrieb und zweites Terminal

MUTOS kennt kein `init 2`. Sein `init` wartet im Einbenutzerbetrieb auf das **Ende der Shell** und
startet danach auf jedem in `/etc/ttys` freigegebenen Gerät ein `getty`. Die Wurzel gibt die Konsole
und **`tty1`** schon frei; `tty1` ist die Schnittstelle **DFÜ/V.24** des Rechners.

1. Wie oben starten, anmelden, die Disketten einhängen (`/etc/mount …`). Die Einhängungen bleiben später erhalten.
2. **Zweites Terminal anschließen** (Möglichkeiten unten).
3. Die Einbenutzer-Shell mit **Strg+Z** beenden (das EOF-Zeichen dieses Systems; `Strg+D` und `exit` tun hier nichts).
   Auf der Konsole und am zweiten Terminal erscheint `login:`; beide Plätze arbeiten gleichzeitig,
   `who` zeigt `console` und `tty1`, `ps` (ohne Argumente) die Prozesse.

Beim nächsten Start beginnt wieder der Einbenutzerbetrieb; `/etc/rc` gibt es nicht, eingehängt wird jedes Mal von Hand.
**Strg+C** bricht ein laufendes Programm ab (die Taste *Entf* tut das nicht).

**Zweites Terminal — mit dem P8000-Terminal (ohne Telnet-Programm, auch unter Windows).**
Das Programm `p8000emu` in der Betriebsart *Nur Terminal* (`p8000emu --mode terminal`) ist ein Arbeitsplatz mit dem
Originalterminal und der Flachtastatur K7673; es spricht selbst Telnet.

1. Im A5120-Emulator: *Einstellungen ▸ Schnittstellen*, Block **DFÜ/V.24**: Betriebsart *Telnet*, Rolle *Server*,
   einen Port wählen (z. B. 5000), starten.
2. Im Terminal: *Maschine ▸ Verbindung zum Rechner…*, Rechner `127.0.0.1` (oder die Adresse des PCs, auf dem der
   Emulator läuft), denselben Port, *Verbinden*. Die Statuszeile zeigt „verbunden". Reihenfolge egal; der Server muss vor dem *Verbinden* laufen.
3. Nach Strg+Z auf der A5120-Konsole meldet sich `login:` am Terminal. Anmelden als `root` (kein Kennwort).

Die Strg-Tasten gehen an das Gastsystem: **Strg+Z** (EOF), **Strg+C** (Abbruch) wurden am Terminal geprüft; Strg+S/Strg+Q sind
nicht erprobt. Strg wird am Terminal wie an der K7673 gedrückt (Wirtstaste oder in der Bildschirmtastatur einrasten lassen).
Gleichwertig geht jedes andere Terminalprogramm, das Telnet (oder RFC 2217) spricht, z. B. PuTTY (Verbindungsart *Telnet*); der Gast meldet keine Baudrate, das ist unschädlich.

### Disketten formatieren und verwenden

Zwei getrennte Schritte: **formatieren** (Spuren schreiben) und **Dateisystem anlegen** (`mkfs`). Für eine
Datendiskette gilt: 494 Blöcke, K5601 = MFM.

**Im Gastsystem:**

1. Eine *Leere Diskette* (`.hfe`) anlegen (Laufwerkskasten, Knopf *Leere Diskette*) und in B: einlegen —
   **vor MUTOS**, mit der Ladediskette in A: am Lader-Menü (also nach dem Einschalten, bevor `BOOT MUTOS` kommt).
2. `FORM.M` eingeben. Fragen: `drive ?` → `1` (B:), `sectorlength` → `0` (= 128 Byte), `ready ?` → `Y`; jeweils RETURN.
3. Rechner zurückstellen und wie oben starten (ein direkter `BOOT MUTOS` nach `FORM.M` scheitert mit
   `I/O ERROR C2`). Die Wurzel kommt in C:.
4. Nach der Anmeldung die `cmd`-Diskette in A: legen und nach `/mnt` einhängen (dort liegt `mkfs`); die formatierte
   Diskette bleibt in B:. Dann `/mnt/etc/mkfs /dev/rk1 494` ausführen (Ausgabe `isize = 152`). Dann `/etc/mount /dev/rk1 /usr` usw.

Unter MUTOS selbst kann man nicht formatieren, nur `mkfs`.

**Mit dem DiskTool** (Menü *Werkzeuge*): *Datei ▸ Neue Diskette* mit dem Dateisystem **`mutos8k_26x128_mfm`** (K5601; `mutos8k_26x128_fm`
für MF3200) legt eine fertig formatierte Datendiskette samt Dateisystem an; Dateien holt und schreibt man wie bei jeder
anderen Diskette (*Datei ▸ Abbild öffnen*). Auf der Kommandozeile:

    k1520disktool create neu.hfe --fs mutos8k_26x128_mfm --blocks 494
    k1520disktool put neu.hfe datei.txt
    k1520disktool ls -l neu.hfe

Dieselbe Diskette nie gleichzeitig im DiskTool und im Emulator offen halten. Eine so angelegte Diskette
hängt MUTOS ohne `mkfs` direkt ein.

### Dateien mit `tar` auf Diskette sichern und zurücklesen

MUTOS hat ein V7-`tar` (Diskette `bin`, Programm `tar`). Es schreibt **ohne Dateisystem** einen
nackten Archivstrom auf das **Rohgerät** einer Diskette: **`/dev/rrk0` = A:, `/dev/rrk1` = B:,
`/dev/rrk3` = D:** (`/dev/rrk2` ist die Wurzel, nie benutzen). Die Diskette muss nur formatiert sein, ein
`mkfs` ist weder nötig noch erwünscht. Eine solche **tar-Diskette** liest das DiskTool (siehe unten und
dessen Handbuch), und umgekehrt kann MUTOS eine vom DiskTool geschriebene lesen.

Vier Regeln für jeden Aufruf:

* **`f` mit Gerät angeben** (`tar cvf /dev/rrk1 …`) — das Vorgabegerät `/dev/mt0` gibt es nicht.
* **Relative Namen** aus dem passenden Verzeichnis (`cd /usr; tar cvf … include`). Absolute Namen werden beim
  Auspacken wieder absolut zurückgeschrieben und überschreiben dabei Vorhandenes.
* **`tar` ruft `pwd` und (beim Auspacken von Verzeichnissen) `mkdir`.** Fehlen sie in `/bin`, `/usr/bin` oder
  `/mnt/bin`, meldet es `pwd failed!` bzw. `tar: cannot find mkdir!` und jede weitere Datei scheitert.
* **Vor jedem Diskettenwechsel `sync`.** MUTOS puffert Schreibzugriffe über `/dev/rkN` und würde sie sonst auf
  die **neue** Diskette schreiben. Das Rohgerät `/dev/rrkN` ist ungepuffert, aber `sync` schadet nie.

Die Kapazität einer tar-Diskette ist **494 Blöcke zu 512 Byte = 252 928 Byte** (Format 26 × 128, s. u. für 8 × 512);
das Archiv braucht je Datei einen Kopfblock und rundet die Daten auf 512 Byte auf. Ein einzelner `tar`-Lauf, der
das Ende der Diskette erreicht, **hängt stumm** (Strg+C holt den Prompt zurück) — größere Archive gehen nur mit
mehreren Disketten (unten).

**Einmalig vorbereiten (drei K5601, Format 26 × 128).** Die Wurzel hat nur etwa 46 KB frei, die
Programme liegen auf `cmd` und `bin`. Die vier nötigen Programme kopiert man einmal dauerhaft in `/bin` der
Wurzel (Arbeitskopie der Wurzel verwenden!):

1. `cmd` in A:, `bin` in B: einlegen.
2. `/etc/mount /dev/rk0 /mnt`, dann `/etc/mount /dev/rk1 /tmp` (die Shell darf dabei **nicht** in `/tmp` stehen,
   sonst `mount: Mount device busy`; im Zweifel vorher `cd /`).
3. `cp /tmp/tar /mnt/bin/pwd /mnt/bin/dd /mnt/bin/mkdir /bin`, danach `sync`. Danach `df /dev/rk2`: etwa 25 Blöcke frei.
4. `/etc/umount /dev/rk0` und `/etc/umount /dev/rk1`. Beide Laufwerke sind wieder frei.

Mehr passt nicht in die Wurzel: `cmp` und `rm` bleiben auf `cmd` (zum Vergleichen und Aufräumen `cmd` wieder in A:
einhängen). Die Wurzel auch als **Arbeitsplatz** knapp halten — `tar … | dd` legt seine Pipe dort ab und braucht
mindestens etwa 10 freie Blöcke (`no space on dev 4/2` heißt: Wurzel voll).

**Eine Diskette schreiben, listen, auspacken.** Beispiel: `usr` in A: (nach `/usr` eingehängt), eine leere
formatierte Diskette in B: (Laufwerkskasten *Leere Diskette* — ein `.hfe` mit Format *udos_ss77*, oder im DiskTool
*Neue Diskette* mit `tar_mutos8k_26x128_mfm_leer`; beides geht):

    # cd /usr
    # tar cvf /dev/rrk1 include lib/crt0.o
    a include/sys/stat.h 2 blocks
    …
    # cd /
    # sync
    # tar tvf /dev/rrk1                  (Tar: blocksize = 8 — dann die Liste)
    # cd /tmp
    # tar xvf /dev/rrk1 include/stdio.h lib/crt0.o     (nur eine Auswahl: die Wurzel ist das Ziel und klein)

`tar` legt die Verzeichnisse beim Auspacken selbst an. Zum Vergleichen `cmd` einhängen und `cmp datei original`
— keine Ausgabe heißt gleich. Aufräumen mit `rm -r /tmp/include /tmp/lib`. Die Diskette bleibt nach dem Auswerfen
und Wiedereinlegen lesbar; die Sektoren stehen ab Zylinder 1 (Zylinder 0 bleibt unberührt).

Eine **nie beschriebene** formatierte Diskette (nur Füllbytes) endet mit `tar tvf` nicht sauber; die vom DiskTool
angelegten `…_leer`-Disketten tragen deshalb im Datenbereich Nullblöcke und gelten als leeres Archiv.

**Mehrere Disketten (Multi-Volume).** `tar` selbst kennt keine Folgedisketten. Man teilt den Strom mit `dd`
selbst auf Disketten zu je **494 Blöcken** auf. Dazu muss der Strom beide Male **gleich** erzeugt werden (nichts
an den Quelldateien ändern). Beispiel mit `usr` in A: und leeren Disketten nacheinander in B::

    # cd /
    # tar cf - usr bin | dd of=/dev/rrk1 bs=512 count=494
    494+0 records in
    494+0 records out
    # sync                               (Diskette 1 aus B: nehmen, Diskette 2 einlegen)
    # tar cf - usr bin | dd of=/dev/rrk1 bs=512 skip=494 count=494
    183+0 records in
    183+0 records out
    # sync

Eine dritte Diskette folgt mit `skip=988`. Die letzte Diskette trägt den Rest samt Endmarke. **Zurücklesen**
nach einem Auspacken (Ziel ist die Wurzel; die Archivdisketten liegen in A: und B:, `usr` vorher aushängen):

    # cd /tmp
    # (dd if=/dev/rrk0 bs=512 count=494; dd if=/dev/rrk1 bs=512 count=494) | tar xvf - bin/date usr/include/time.h

Der Strom darf dabei über die Diskettengrenze laufen (`bin/date` beginnt auf Diskette 1 und endet auf Diskette 2).
Mit nur drei Laufwerken passt ein komplettes Auspacken größerer Sätze nicht in die Wurzel — man packt eine
**Auswahl** aus oder hängt eine große Datendiskette als Ziel ein (dann `cd` dorthin). Ein Auflisten über beide
Disketten (`… | tar tvf -`) läuft ohne Fehler, dauert aber mehrere Milliarden Takte und sieht wie ein Hänger aus.
Gibt man eine **einzelne** Satzdiskette an `tar tvf`, endet die Liste mit `err on dev 13/0 … er=0,305` und
`Tar: tape read error`: sie ist kein abgeschlossenes Archiv, der Inhalt davor ist vollständig.

**Im DiskTool.** Eine so geschriebene Diskette erkennt das DiskTool als **`tar_mutos8k_26x128_mfm`**
(MF3200: `…_fm`) und öffnet mehrere Disketten eines Satzes mit **Datei ▸ tar-Satz öffnen…** als EINEN Strom.
Auf der Kommandozeile:

    k1520disktool tar-satz v1.hfe v2.hfe -l          # Dateiliste des Satzes
    k1520disktool tar-satz v1.hfe v2.hfe --to ordner # alles herausholen
    k1520disktool tar-satz-neu --format tar_mutos8k_26x128_mfm satz.hfe --put datei1 --put ordner

Das DiskTool **schreibt** diese Disketten ebenfalls (Dateien hinzufügen, löschen, über eine Diskette hinaus als
Satz speichern); MUTOS liest das Ergebnis mit `tar tvf`/`tar xvf` bzw. der `dd`-Pipeline. Der Weg ist also in
beiden Richtungen offen: Dateien vom PC auf eine tar-Diskette und von dort nach MUTOS, oder umgekehrt.

**Das zweite Sektorformat, 8 × 512 (DISK-Satz).** Mit der Ladediskette `a5120_mutos8000_boot_udos_k5601_8x512.hfe`
arbeitet MUTOS mit 8 Sektoren zu 512 Byte. Die tar-Diskette trägt dann **608 Blöcke = 311 296 Byte**; die Blöcke
liegen **linear** ab Zylinder 1 (Block n auf Zylinder 1 + n/8, Sektor n mod 8 + 1), ohne Verschränkung. Das DiskTool
kennt sie als **`tar_mutos8k_8x512_mfm`** (leer: `…_leer`). Über das Ende hinaus meldet `tar` hier sauber
`Tar: tape write error` (statt zu hängen). Der Ablauf ist derselbe; mit der Satzbestückung
Ladediskette in A:, **`…_cmd_8x512.hfe` in B:** (`/mnt`), **`…_root_8x512.hfe` in C:**, D: leer (nach dem Start
nimmt man die Ladediskette aus A: und legt dort die tar-Diskette ein):

    # /bin/mount /dev/rk1 /mnt
    # cd /
    # /mnt/bin/tar cvf /dev/rrk3 etc bin                   (leere Diskette in D:)
    # /mnt/bin/tar tvf /dev/rrk3
    # /mnt/bin/tar cf - etc bin mnt | /bin/dd of=/dev/rrk3 bs=512 count=608     (Folgedisketten: skip=608, …)
    # (/bin/dd if=/dev/rrk0 bs=512 count=608; /bin/dd if=/dev/rrk3 bs=512 count=608) | /mnt/bin/tar xvf - dateiname

Hier liegt `tar` auf `cmd` und die Wurzel ist die gelieferte `…_root_8x512.hfe` (mit der `pwd` von 1986: die `pwd`
der alten DISK-ROOT-Diskette hängt, und mit ihr `tar`). Die Original-`.td0`-Disketten dieses Satzes lassen sich bis
auf `cmd.td0` einlegen; die Hintergründe stehen in `doc/design/39_mutos8000.md` (A5c).

### Sicherungskopien

* **Die Datei kopieren** — das ist die ganze Diskette. Erst die Diskette **auswerfen** (oder den Rechner
  ausschalten), da der Emulator erst dabei die letzten Änderungen zurückschreibt, dann die `.hfe`
  im Dateimanager kopieren. Vorher in MUTOS `/etc/umount` und `sync`.
* Alternativ *Speichern unter…* beim eingelegten Laufwerk: sichert die Diskette unter neuem Namen
  (und arbeitet ab dann an der Kopie weiter).
* Eine **unersetzliche Diskette** mit dem Haken *Write-Protect* im Laufwerkskasten schützen.
* Die Wurzel vor dem ersten Start sichern: sie wird bei jedem Lauf verändert.
* Einzelne Dateien sichert das DiskTool mit *Datei holen* bzw. `k1520disktool get neu.hfe --to ordner`.

Genaueres und die Hintergründe (Laufwerkstypen, andere Bestückungen, Geometrie 8 × 512, Swap verlegen):
`doc/design/39_mutos8000.md`.

## RAM-Floppy RAF

*Einstellungen ▸ Allgemein ▸ RAM-Disk* steckt eine RAM-Floppy des ZWG der AdW
auf die E/A-Adressen 88H/89H — in allen drei Programmen: **keine** (die Vorgabe),
**RAF 128** (128 KByte), **RAF 512** (512 KByte) oder **RAF-2M** (2 MByte).

Ein Wechsel schaltet die Maschine aus und baut sie neu auf, wie beim Modell —
eine Karte steckt man nicht im Betrieb.

Das Betriebssystem braucht einen Treiber:

* **CP/A und SCPX:** `RAF512.COM` legt die RAM-Disk als Laufwerk **P:** an,
  `RAFCPM.COM` als **M:**. Am K8915 ist M: oft schon belegt — dort `RAF512`.
* Ein **CP/A mit eingebautem RAF-Treiber** legt M: beim Kaltstart selbst an.
* Beim ersten Mal meldet der Treiber „undefiniert" und legt ein leeres
  Verzeichnis an; danach „noch wie bei letzter Benutzung geladen".

Was den Inhalt überlebt:

| Ereignis | Inhalt |
|----------|--------|
| *Rückstellen* (RESET) | bleibt |
| Aus- und Einschalten, Wechsel von Modell/Laufwerken/RAM-Disk, Beenden | weg — außer mit Stand-by |
| dasselbe mit **Stand-by** | bleibt |

**Inhalt beim Beenden behalten (Stand-by 5PG)** — das Kästchen darunter, nur
wählbar, wenn eine RAF gesteckt ist; ab Werk aus. Gesetzt, schreibt das Programm
den Inhalt beim Beenden, beim Ausschalten und vor jedem Neuaufbau der Maschine in
den Konfigurationsordner (`raf_a5120emu.bin`, `raf_k8915emu.bin` bzw.
`raf_prg710emu.bin` — je Programm eine eigene Datei) und lädt ihn beim
Einschalten wieder. Passt die Datei nicht zum gewählten Typ (etwa eine
512-KByte-Ablage bei RAF-2M), wird sie nicht geladen — die Statuszeile sagt es —
und beim nächsten Sichern ersetzt. Ohne Häkchen wird keine Datei gelesen oder
geschrieben; eine vorhandene bleibt liegen.

**A5120.16 und RAF** gehen zusammen: das Erweiterungsmodul belegt M: (CP/A mit
EM256-BIOS), `RAF512.COM` legt die RAF daneben als P: an.

## Lochstreifen

*Einstellungen ▸ Allgemein ▸ Lochstreifen (SIF1000, K6022)* steckt die
Anschlusskarte ADA K6022 mit einem **Lochstreifenleser** (daro 1210) und einem
**Lochstreifenstanzer** (daro 1215) — in allen drei Programmen, ab Werk aus. Ein
Wechsel schaltet die Maschine aus und baut sie neu auf, wie bei der RAM-Disk.

Mit gesteckter Karte erscheint der Kasten **Lochstreifen** (neben *Laufwerke*,
*Ansicht ▸ Lochstreifen* holt ihn zurück). Er hat zwei Blöcke:

* **Lochstreifenleser** — *Öffnen…* legt eine Banddatei ein, *Entnehmen* nimmt sie
  heraus. Darunter steht, wie weit gelesen ist (`120 von 480 Byte gelesen`) und
  wann das Bandende erreicht ist. Der Leser gibt vorn und hinten ein Stück
  Leerband (Nullbytes) dazu. Die Datei selbst wird nie verändert.
* **Lochstreifenstanzer** — *Öffnen…* **bindet** den Stanzer an eine Datei: was
  gestanzt wird, schreibt das Programm nach jeder Stanzpause dorthin, wie bei einer
  Diskette. Eine vorhandene Datei ist der Anfang des Bandes, es wird angehängt.
  *Lösen* beendet die Bindung (das Band bleibt im Stanzer), *Neues Band* fängt
  von vorn an und leert eine gebundene Datei sofort. Ohne Datei bleibt das Band
  nur im Speicher. *Stanzer ein* abgehakt: der Stanzer quittiert nicht, der
  Treiber meldet nach etwa einer Sekunde einen Fehler (UDOS: `ERROR C2`).

Beide Dateien samt Format werden gemerkt und beim nächsten Start wieder
eingelegt bzw. gebunden; fehlt eine, sagt es die Statuszeile, und das Gerät
bleibt leer. Beide Bänder überstehen ein Rückstellen.

**Nach jeder Dateiwahl wird das Format erfragt** — vorgeschlagen nach dem Inhalt
der Datei, bei einer neuen Datei nach ihrer Endung:

| Format | Endung | Inhalt |
|--------|--------|--------|
| Roh | `.ptp`, `.bin` | ein Byte je Sprosse, wie gestanzt — eine Textdatei des PCs lässt sich so direkt einlesen |
| Intel HEX | `.hex`, `.ihx` | Adresse = Stelle auf dem Band, Lücken = 00H |
| ASCII-Art | `.txt`, `.tape` | das Lochbild zum Ansehen und Bearbeiten |

ASCII-Art zeigt den Streifen von oben nach unten, Spur 8 links, das
Transportloch zwischen Spur 3 und 4; rechts stehen Wert und Zeichen. Zeilen mit
`;` sind Kommentar. Beim Einlesen zählt das Lochbild — steht rechts ein Wert,
der nicht dazu passt, wird die Datei mit Zeilennummer abgewiesen:

```
;    8 7 6 5 4   3 2 1
;  +-------------------+
   | . . . . . o . . . |  00
   | . O . . . o . . O |  41  A
   | O . . O . o . O O |  93
;  +-------------------+
```

**Gasttreiber:** am **PRG 710** hat UDOS ihn dabei: `DO TWRITE.1215 DATEI F=A`
stanzt eine Datei, `DO TREAD.1210 NEU F=A` liest das eingelegte Band in die
Datei `NEU` (sie entsteht auf der zweiten Diskettenseite, `CAT` meldet
„DRIVE 4“). **`F=A` gehört dazu**: der Treiber `PTAPE.6022` überträgt nur Text
(gerade Parität, Zeilenende NL); das Vorgabeformat von `TAPE.WRITE` kommt nicht
zurück (`ERROR C9`). `ERROR C2` beim Lesen heisst: kein Band im Leser. Für
**A5120** und **K8915** liegt kein Treiber bei — die Karte steckt (E/A E0H–E7H),
ein Programm muss sie selbst bedienen.

## Eine startfähige Diskette erstellen (A5120)

Startfähig wird eine Diskette durch zweierlei: die **Systemspuren** vor dem Dateisystem
(das Lade-ROM liest sie blind ein) und die Systemdateien dahinter. Der Weg hängt vom
Betriebssystem ab. In allen Fällen gilt: im Emulator mit *Leere Diskette* ein
unformatiertes Medium als `.hfe` oder `.dmk` anlegen (Abschnitt *Leere Disketten und
„Speichern unter“*) und das Gastsystem formatieren lassen; die Geometrie des
Laufwerks bestimmt die Diskette.

**CP/A** (Beispieldiskette `a5120_cpa/a5120_cpa_k5601_system.hfe`, trägt `FORMAT.COM`,
`CPABCGEN.COM` und `@OS.COM`):

1. Systemdiskette nach A:, *Leere Diskette* nach B:, Kaltstart.
2. `FORMAT` (Verfahren und Laufwerk B: wählen, ganze Diskette, mit Vergleichs-Lesen).
3. Zurück am Prompt `CPABCGEN B:` — es schreibt die Systemspuren und `@OS.COM` auf B:
   und meldet `OK`. Danach `DIR B:` zur Kontrolle.
4. B: nach A: legen, Kaltstart: CP/A startet bis `A>`.

**SCPX 1526 V1.7** (Beispieldiskette `a5120_scpx17/a5120_scpx17_k5601_system.hfe`;
Quelle: `tools/scpx_a5120/inhalt/LIESMICH.TXT`). Für die Standardformate genügt es, die
Systemdiskette zu kopieren. Eine Systemdiskette im Format 16×256 entsteht so:

1. `INIT B:` mit Format 0 (16×256 doppelseitig) auf einer *Leeren Diskette* in B:.
2. `MODF B:` stellt B: für die laufende Sitzung auf Format 0.
3. `SYSP` erzeugt das System für die gewählte Tastatur, den Bildschirm und die
   Laufwerke (alle BIOS-Module liegen auf der Diskette); bei „Welches Format für
   5,25-Zoll-Lw?“ `0` wählen und als Ausgabe-Laufwerk B: angeben.
4. Die Dateien (PIP, STAT, …) kopiert man danach mit `PIP`.

`SYSG` allein genügt nicht: es kopiert die Systemspuren im Format des Quelllaufwerks.

**UDOS 4.3** (Beispieldiskette `a5120_udos43/a5120_udos43_k5601_entwickler.hfe`;
Quelle: `doc/udos_diskettenformat.md` §12.2). Vorder- und Rückseite sind getrennte
Laufwerke (0 und 4 …); gebraucht werden zwei FORMAT-Läufe und zwei MOVE:

    %FORMAT   SYSTEMDISK? Y   DRIVE? 1   ID? SYSDISK     READY? Y
    %FORMAT   SYSTEMDISK? N   DRIVE? 5   ID? SYSDISK.B   READY? Y
    %MOVE * S=0 D=1 P=&
    %MOVE * S=4 D=5 P=&

`SYSTEMDISK? Y` ist der einzige Unterschied zur Datendiskette: FORMAT schreibt dann
Urlader und Bootabbild mit; ohne `Y` bleibt der Bildschirm beim Kaltstart schwarz.
`P=&` bei `MOVE` ist Pflicht, sonst bleiben die als SECRET markierten Dateien liegen.
Die Laufwerkstypen (`SET DISKCON`) setzen Grenzen — Matrix in
`doc/udos_diskettenformat.md` §12.3 (bootfähig herstellbar: `41`, `31` und `41` am 8″).

**BCU 880 V1.2.** Die Startdisketten „SYL auf Spur 1“ sind reine Startdisketten: SYL-Lader
auf Zylinder 1, das System auf Zylinder 2–4, **kein** Dateisystem für Anwenderdateien
(Abschnitt *BCU 880*, Quelle `doc/a5120_alt/bcu.md` §2). Gearbeitet wird mit der
Arbeitsdiskette „ohne SYL“ in B:. Was belegt ist:

* Auf der Arbeitsdiskette (`a5120_bcu12_k5600-10_arbeit-sys1.hfe`) liegt `SYSGEN.COM`;
  der Text im Programm ist `SYSGEN VER 2.0` von Digital Research („SOURCE DRIVE NAME …“,
  „DESTINATION DRIVE NAME …“, „FUNCTION COMPLETE“). Es ist also das übliche CP/M-SYSGEN,
  das Systemspuren zwischen Laufwerken kopiert. **Ob es dabei den SYL-Lader auf Zylinder 1
  mit der Systemlage der Startdiskette erzeugt, ist nicht belegt [?]** — es wurde nicht
  gefahren, und die „ohne SYL“-Spuren (System auf Zylinder 0–1) unterscheiden sich von der
  SYL-Lage (`bcu.md` §2 und §5). Daneben liegen `SYS8.COM`, `SYS80.COM`, `FORMAT.COM`,
  `INIT.COM`, `LOAD.COM`; was sie im Einzelnen tun, ist hier nicht ausgewertet [?].
* Ein eigenes BCU-Systemspurbild baut das DiskTool nicht: es kennt für diese Lage kein
  Dateisystem (`info` meldet „kein bekanntes Dateisystem“), `create --boot` und
  `boot-put` sind dafür nicht erprobt [?].
* Der sichere Weg im Emulator ist deshalb die **Kopie** der Startdiskette
  (*Speichern unter…* im Laufwerkskasten, `.hfe` → `.hfe`); sie bootet mit dem Lade-ROM
  V0.6 wie das Original. Die Disketten „ohne SYL“ starten mit keinem vorhandenen
  Lade-ROM (`doc/a5120_alt/laderom.md` §5.2) — sie sind Arbeitsdisketten.

## AUP (Sparkasse)

*Einstellungen ▸ Allgemein ▸ AUP (Sparkasse)* steckt den Adapter Universelle Ports
**AUP 062-8531** in den A5120 (nur im A5120 Emulator, bei allen drei Modellen ab Werk
aus; **ein Modell „K8924 Sparkasse“ gibt es nicht**, es ist diese Option). Ein
Wechsel schaltet die Maschine aus und baut sie neu auf. Die Karte belegt E/A
**40H–43H**; die Tastatur des Modells (K7636/K7637) bleibt, wie sie ist — die AUP
bringt die Sparkassentastatur **K7633** und die **Kundentastatur** zusätzlich.

Die Unterfelder: *mit Sonderschnittstelle* steckt CTC B0H–B3H und SIO B4H–B7H mit;
SIO-Kanal B erscheint im Reiter *Schnittstellen* als vierter Anschluss
(*Sonderschnittstelle*). *Alpha-Tabelle* wählt, welche Codetabelle der K7633 gilt:
*folgt der ALPHA-Lampe* (Vorgabe), *immer ohne* oder *immer mit Alpha*. Das ist eine
**Annahme** — die Unterlagen sagen nicht, wer zwischen den Tabellen umschaltet.

Mit gesteckter Karte erscheint der Kasten **AUP** (neben *Laufwerke*, *Ansicht ▸ AUP*
holt ihn zurück):

* **SS1 / SS2** sind die beiden Schlüsselschalter; Einschalten sendet AAH bzw. A9H an
  den Rechner. Die Stellung wird nach jedem Neuaufbau aus der Maschine gelesen.
* **Lampen** SEL, ALPHA, GET (LED der Kundentastatur) und Fehler. Die Fehlerlampe
  blinkt, solange der Rechner sie eingeschaltet hat. **Hupe** blitzt kurz auf und
  gibt, wo das System es zulässt, einen kurzen Ton.
* **Tastatur K7633:** ein Raster nach den Positionsnamen der Unterlagen
  (Zeile A–F, Spalte 01–13) — **nicht** die Geometrie der Tastatur: die Beschriftung
  der Kappen steht nur auf einem Foto. Jede Taste zeigt ihren Positionsnamen und den
  Code ohne Alpha; der Hinweistext nennt Position und beide Codes.
* **Kundentastatur:** der Ziffernblock; die zwei Tasten *F3D* und *F3F* senden beide
  den Code 19H, ihre Bedeutung ist offen.

Bedient wird mit der Maus (gedrückt halten wirkt wie eine gehaltene Taste). **Die
Tasten des PCs sind der AUP nicht zugeordnet** — die Unterlagen geben keine
Zuordnung her, und es gibt wie überall kein Tastenkürzel. Die Karte gehört nicht in
den Zwischenstand. Ob der Rechner die Karte abfragt, hängt von seiner Software ab
(das Prüfprogramm `AUPTEST.COM` zeigt die Codes am Bildschirm).

## K3521 (batteriegepuffertes RAM)

*Einstellungen ▸ Allgemein ▸ K3521 (batteriegepuffertes RAM)* steckt bis zu **vier**
Karten **K3521.30** in den A5120 (nur im A5120 Emulator, bei allen drei Modellen ab Werk
*keine*). Jede Karte ist ein 4-KB-CMOS-RAM mit Akkupufferung; ein Wechsel schaltet die
Maschine aus und baut sie neu auf. Das Auswahlfeld nennt die Anzahl, daneben steht die
**Startadresse** der ersten Karte (Hex, Vorgabe **8000**). Die Karten liegen lückenlos
hintereinander: Karte *i* bei *Start + i · 1000H*, also mit 2 Karten und Start 8000 bei
8000H und 9000H. Eine Eingabe, die kein Hex ist, nicht im 4-KB-Raster liegt (0000, 1000
… F000) oder deren Karten über FFFFH hinausreichen, wird mit einer roten Meldung
abgewiesen; das Feld springt auf den letzten gültigen Wert zurück.

Die Karte verdeckt beim Lesen das RAM darunter. Ihr Inhalt bleibt über **Reset und
Aus-/Einschalten** und wird je Karte in einer Datei (`k3521_<Programm>_<Nr>.bin`, 4096
Byte, im Konfigurationsordner) über das Ende des Programms hinaus aufbewahrt. Der
CP/A-Kaltstart überschreibt je nach Lage Speicher; die Vorgabe 8000 liegt dort, wo er den
Inhalt in Ruhe lässt (7000H–AFFFH). Zum Prüfen der Karte und zum Eintippen von Text
dient das Programm `K3521TST.COM` auf den Systemdisketten (`K3521TST 8000 T` für den
Speichertest, `K3521TST 8000 E` für den Editor; ohne Parameter erscheint die Hilfe).

## Kassette

*Einstellungen ▸ Allgemein ▸ Kassette (K5221, AKB K5020)* steckt den
Anschlussbaustein K5020 mit zwei Kassettenlaufwerken K5200 — in allen Programmen
außer dem P8000, ab Werk aus. Ein Wechsel schaltet die Maschine aus und baut sie neu
auf, wie beim Lochstreifen. Die Karte liegt am A5120 und am PRG 710 auf E/A
30H–37H, am K8915 und am PC 1715 auf D0H–D7H (am PC 1715 nur mechanisch: es gibt
dort keinen Gasttreiber), am MC80 auf 30H–37H (Bedienung unter *Der MC80 Emulator*).

**Zurückspulen vor KMBG/FEX:** Programme, die die Kassette über `KMBG.REL` (SCPX)
oder FEX bedienen, warten nur rund 2,5 s auf das Magnetband. Eine frisch eingelegte
Kassette steht noch im Vorspann und wird in dieser Zeit nicht erreicht (Status 04);
sie muss deshalb vorher zurückgespult werden (UMS bzw. das Rückspulkommando des
Programms).

**Beispielbänder** im Ordner der Beispieldisketten:

* `a5120_bandtest/bandtest_boot.k52` — **Demo, Eigenbau** (kein Robotron-Band): ein
  kleines Startprogramm für das Lade-ROM des A5120. Kassette in Gerät 1, **keine**
  Diskette in A:, Kaltstart — das Lade-ROM probiert erst die Laufwerke durch und lädt
  dann vom Band; nach gut zehn Sekunden steht `KASSETTE OK` in der ersten Zeile.
* `a5120_bcu/bcu_kassette_formatiert.k52` — eine von BCU 880 formatierte Kassette
  (Abschnitt *BCU 880*).

Mit gesteckter Karte erscheint der Kasten **Kassette** (neben *Laufwerke*,
*Ansicht ▸ Kassette* holt ihn zurück), mit einem Block je Laufwerk (Gerät 1 und 2):

* **Lampe und Datei** — die Lampe leuchtet, solange das Band läuft. Darunter stehen
  die Seite (A oder B) und die Kopfstelle (`12,3 von 90,0 mm, 4 Blöcke`).
* **Einlegen…** legt eine `.k52`-Datei ein und **bindet** sie: was das Programm
  schreibt, landet nach einer Schreibpause in der Datei, wie bei einer Diskette. Eine
  Datei, die es noch nicht gibt, wird als leere Kassette angelegt.
* **Neue** legt eine leere Kassette ohne Datei ein (sie bleibt im Speicher und wird
  nicht gemerkt). **Auswerfen** schreibt die Datei zurück und nimmt die Kassette
  heraus; läuft das Band oder hat der Rechner das Gerät reserviert, geht es nicht.
* **Wenden** dreht die Kassette (Seite A ↔ B), nur bei stehendem Band.
* **Seite A / B schreibgeschützt** — der Schreibschutz je Seite.

Gemerkt werden je Laufwerk Datei und Schreibschutz; beim nächsten Start wird die
Kassette wieder eingelegt (eine fehlende Datei entsteht leer neu). Die Bänder
überstehen ein Rückstellen des Rechners, aber keinen Zwischenstand (*Maschine ▸
Zwischenstand* sichert die Karte nicht). Für die Kassette gibt es kein
Tastenkürzel.

### Von Kassette starten

Das Lade-ROM des A5120 (Quelle: `doc/kassette/treiber.md` §6.1) probiert nach dem
Einschalten zuerst die Diskettenlaufwerke durch, dann die Kassette: **Gerät 1**, nach fünf
Fehlversuchen **Gerät 2**, dann Halt. Es spult das Band zum Anfang und liest **vier Blöcke
zu je 128 Byte** hintereinander nach **0400H**. Steht in 0400H–0402H die Kennung `SYL`,
springt es nach **0434H**; sonst geht es in den Fehlerpfad. Gesamtfrist rund 136 s.

Es startet also nur ein Band, das genau so aufgebaut ist: ein **Bootband**. Eines baut man
mit `k1520tape bootband <programm.bin> <band.k52> [A|B]` (legt 4 × 128 Byte an, die Datei
wird auf 512 Byte aufgefüllt); das Programm muss bei Versatz 34H einsetzen und `SYL` am
Anfang tragen. Als Beispiel dienen `tools/kassette/bandtest/` (Quelle, `build.py`) und das
fertige Demo-Band `a5120_bandtest/bandtest_boot.k52`.

Ehrlich dazu:

* **Für BCU 880, CP/A und SCPX ist keine Systemkassette bekannt**, und Originalbänder
  liegen nicht vor. Ein Betriebssystem von Band zu starten hieße, einen eigenen Lader als
  Bootband zu schreiben (Eigenbau, nicht belegt).
* **CP/A und SCPX haben keinen Kassettentreiber** (CP/A: Kassetten „außerhalb des BIOS“;
  SCPX: nur die Bibliothek `KMBG.REL`).
* **BCU 880 hat einen Treiber** (Laufwerk F:), aber ob BCU von Band startet, ist nicht
  belegt [?].
* Der Floppy-SYL-Lader von Spur 0 taugt nicht als Bandinhalt: bei Versatz 34H liegt dort
  ein `NOP` vor der K5122-Initialisierung.

**Kassette je System:**

| System | Kassettenbedienung |
|--------|--------------------|
| BCU 880 | Laufwerk E: (Gerät 1) und F: (Gerät 2); `INIT` formatiert, `OPEN B:KMBG` lädt den Treiber |
| SCPX | nur die Bibliothek `KMBG.REL` und das Prüfprogramm `KMBGTEST` |
| CP/A | kein Treiber |
| EGOS (MC80) | Kommandos `KINIT`, `WRITE`, `READ` (Abschnitt *Der MC80 Emulator*) |

## Rechtsklick auf das Bild: Text kopieren, Zwischenablage einfügen

Das Menü der rechten Maustaste auf dem Bildschirm gibt es bei **allen** Maschinen:

* **Bildschirminhalt als Text kopieren** — der Inhalt des Textbildes (80 × 24, beim PC 1715 samt
  Statuszeile 25 Zeilen) landet als reiner Text, nicht gerendert, in der Zwischenablage; Zeilen
  ohne Schlussleerzeichen, Attribut- und Steuerzeichen als Leerzeichen.
* **Zwischenablage über Tastatur einfügen** — der Text aus der Zwischenablage wird Zeichen für
  Zeichen **wie getippt** an den Gast gegeben, mit **120 Zeichen je Sekunde** (Wirtszeit): zügig, aber
  nicht schneller, als Tastatur und Gast folgen. Zeilenenden werden zu *Return*, Tabulatoren zu *Tab*.
  **Zeichen, für die es keine Taste gibt** (z. B. Umlaute auf einer Tastatur ohne sie), werden
  **übersprungen**; die Statuszeile sagt danach, wie viele eingegeben und wie viele übersprungen wurden.
  Solange das Einfügen läuft, steht an derselben Stelle **Einfügen abbrechen**. Tippt der Gast
  zwischendurch zurück (Passwortabfrage, Menü), gilt, was er sieht — der Emulator wartet nicht auf ihn.
  Am Originalterminal des P8000 nimmt die K7673 nur etwa **7 Zeichen je Sekunde** an (sie entprellt jede
  Taste über 41 Abtastungen); dorthin wird entsprechend langsamer eingefügt.

Tastenkürzel gibt es für beides nicht (Strg+C und Strg+V gehören dem Gast).

## Die Bildröhre einstellen

*Einstellungen ▸ CRT*: Leuchtfarbe, Helligkeit, Kontrast, Wölbung, Rundung der
Ecken, Bildlage und -größe. Es sind dieselben Regler, die eine echte Röhre
hinten hatte, und sie wirken sofort. *Zurücksetzen* stellt die Vorgabe her.

Zwei davon sind mehr als Geschmack: **Wölbung** und **Bildgröße** entscheiden
darüber, ob die Ecken des Textbilds noch im sichtbaren Bereich liegen.

## Die Tastatur

Die echte Tastatur bedient den emulierten Rechner — sie geht durch die
Nachbildung hindurch, die dabei mitzeigt, welche K7637-Taste angesprochen wird.
Die Zuordnung folgt der **Position**: die Taste an der Stelle, an der sie auf dem
PC liegt, spricht die K7637-Taste an derselben Stelle an.

Weil jeder Tastendruck dem Gast gehört — **auch** `Strg+C`, `Strg+S`, `Strg+P`
und die Funktionstasten, die CP/M braucht —, trägt jede Bedienung des Fensters
`Strg+Umschalt`. Die einzige Ausnahme ist F11 (Vollbild).

**K8915:** die Tastatur K7672 wiederholt eine gehaltene Taste selbst — nach etwa
einer Sekunde, dann etwa zehnmal je Sekunde (gerechnet, nicht am Gerät gemessen),
und wie am Gerät nicht bei Umschalt, Strg, Tab und den Ziffern 1 3 5 7 9. Das
Wiederholen des PCs wird deshalb nicht weitergereicht. Umschalt und Strg gehen als
eigene Tasten durch; ALT gibt es nur an der Bildschirmtastatur.

## Konfiguration — was gemerkt wird und wo

Alles, was man einstellt, landet fortlaufend in
`~/.config/k1520emu/a5120emu.yaml` bzw. `k8915emu.yaml` (Windows:
`%APPDATA%\K1520emu`) — jedes der beiden Programme hat seine eigene Datei, man
kann also den A5120 mit drei Laufwerken und sichtbarer Tastatur und den K8915 mit
zwei Laufwerken ohne Tastatur nebeneinander führen. Gemerkt werden Bildröhre,
Takt, Modell, RAM-Disk (samt Stand-by), Lochstreifen-Karte samt Leser- und
Stanzerdatei, Kassetten-Karte samt den Dateien beider Laufwerke, AUP-Option (A5120), Laufwerksbestückung, eingelegte Disketten, die seriellen Schnittstellen
(samt dem, was lief), Größe und Lage des Fensters
(auch „maximiert"), die Lage und Breite der Kästen samt der Trennlinien
dazwischen, und der Inhalt der Symbolleiste.

*Datei ▸ Konfiguration speichern…* legt dieselbe Datei woanders ab — für mehrere
Einrichtungen nebeneinander (etwa „CP/A mit drei Laufwerken" und „UDOS mit 8″").
*Konfiguration laden…* holt sie zurück, startet die Maschine kalt und übernimmt
die geladene Einrichtung als die neue laufende.

*Ansicht ▸ Standard zurücksetzen* geht den umgekehrten Weg: es stellt den
Zustand her, in dem der Emulator nach der Installation aufgeht — Bildröhre,
Takt, Laufwerksbestückung, Fenstergröße, Kästen und Symbolleiste — und
**überschreibt damit die gespeicherte Konfiguration**; deshalb wird gefragt. Die
eingelegten Disketten bleiben dabei liegen: was zurückgesetzt wird, ist die
Einrichtung, nicht die Maschine. Die Vorgabe selbst ist eine Datei des
Programms (`share/k1520emu/default_config_a5120.yaml` bzw.
`default_config_k8915.yaml`, im Quellbaum unter `data/`) und hat denselben
Aufbau wie eine gespeicherte Konfiguration — wer einen anderen
Auslieferungszustand will, kopiert den Inhalt seiner `a5120emu.yaml` dorthin.

Frühere Fassungen hießen die Datei des A5120 `config.yaml`. Findet der A5120
Emulator beim Start noch eine `config.yaml`, aber keine `a5120emu.yaml`, benennt
er sie **einmal** um — die Einrichtung geht beim Update also nicht verloren.

## Wo die Dateien liegen

| Ordner | Wofür |
|--------|-------|
| `K1520emu/Disketten` (im Dokumentenordner) | die Abbilder |
| `~/.config/k1520emu/a5120emu.yaml`, `k8915emu.yaml` | die Konfiguration je Programm |
| `share/k1520emu/default_config_a5120.yaml`, `…_k8915.yaml` (in der Installation) | der Auslieferungszustand |

Beim ersten Start einer Fassung werden die mitgelieferten Beispieldisketten
dort in einen eigenen Ordner ausgepackt (`Beispieldisketten_v03`, für eine
Vorabversion mit Zusatz wie `Beispieldisketten_v03-beta.2`) — gegliedert nach
`<maschine>_<system>/`. Was sonst im Arbeitsordner liegt, bleibt unberührt. Verschieben lässt sich das mit den
Umgebungsvariablen `K1520_DATA` (Arbeitsordner) und `K1520_DISKS` (nur die
Abbilder). Wo das Programm gerade sucht, sagt `a5120emu --paths`.

## Von der Kommandozeile

```
a5120emu [DISKETTE …]     bis zu vier Abbilder, in Laufwerksreihenfolge A: B: C: D:
k8915emu [DISKETTE …]     bis zu zwei Abbilder, A: B:
a5120emu --paths          aufgelöste Pfade zeigen
a5120emu --help           Kurzhilfe
```

Im Quellbaum heißen die Starter `run_a5120emu.sh` und `run_k8915emu.sh`; beide
rufen `app/main.py` auf, der zweite mit `--machine k8915`.

Die genannten Disketten liegen **beim Kaltstart schon im Laufwerk** — die
Maschine bootet also von der ersten. Sie ersetzen die gemerkte Belegung nur für
diesen Lauf; gespeichert wird davon nichts.

Ohne Oberfläche fährt dieselbe Maschine unter `k1520dbg` (Fehlersuche,
Skriptbetrieb); `k1520dbg DISKETTE --console` ist die Konsolenfassung.

## Die anderen Werkzeuge

Im Menü **Werkzeuge** stehen die Programme, die zur selben Installation
gehören und dieselben Disketten anfassen:

* **K8915 Emulator starten**, **A5120 Emulator starten** usw. — die jeweils
  anderen Emulatoren, als eigenes Programm mit eigener Konfiguration.

* **k1520DiskTool starten** — das Diskettenwerkzeug: Dateien von einer Diskette
  in einen Ordner holen und wieder zurückschreiben, Disketten anlegen, prüfen,
  reparieren. Es startet als eigenes Programm und läuft neben dem Emulator
  weiter; das Diskettenwerkzeug hat sein eigenes Handbuch.
* **K1520 Kermit starten** — Dateiaustausch mit der WEGA oder einem anderen Kermit über Telnet
  oder RFC 2217 (Client und Server); eigenes Programm mit eigenem Handbuch.
* **Werkzeugkonsole öffnen** — ein Konsolenfenster, in dem der Debugger
  `k1520dbg` und die Kommandozeile des DiskTool **ohne Pfadangabe** laufen. Es
  steht bereits im Diskettenordner, und beim Öffnen steht ein Beispielaufruf
  mit einer wirklich vorhandenen Diskette da.

Die Konsole wird über eine Startdatei geöffnet, die im Konfigurationsordner
liegt (`werkzeugkonsole.sh`, unter Windows `werkzeugkonsole.cmd`). Sie wird bei
jedem Öffnen neu geschrieben — wer sie anpassen will (ein eigener Assembler im
Suchpfad, ein anderer Arbeitsordner), kopiert sie sich woandershin.

> Eine Diskette, die hier im Laufwerk liegt, darf zugleich unter `k1520dbg`
> offen sein: der Debugger arbeitet standardmäßig auf einer Kopie und schreibt
> nicht in die Datei zurück.

## Der K8915 Emulator

`k8915emu` ist derselbe Emulator für den **robotron K8915** (Version 3, 5¼″):
ZRE mit 128 KB, Bildschirmkarte K7024, Tastatur K7672, Diskettensteuerung K5122
mit zwei K5601. Was anders ist:

* **Einschalten** startet den Selbsttest des Boot-ROMs (ROM, RAM, KEY, CTC, SIO —
  in der letzten Zeile unter „DIAGNOSTIC"). Er dauert rund 12 Sekunden
  Maschinenzeit, bei der Vorgabe 10 × Takt also gut eine Sekunde. Danach steht
  `* Coldstart *  Disk on A: ready ? --> <ENTER>` — **RETURN** lädt das System
  von A:. SCPX 8915 richtet beim Start die RAM-Disk E: ein und meldet sich mit `A>`.
* **Modell** — *Einstellungen ▸ Allgemein ▸ Modell*: **K8915 V3** (ZRE 045-8762,
  128 KB; die Vorgabe) oder **K8915 V2** (ZRE K2521 mit der RAM-Karte K3528,
  64 KB, ohne die zweite Speicherbank — der Autostart `rade` meldet dort „no
  RAM-device“, danach steht `A>` wie gewohnt). Beide mit Tastatur K7672 und
  demselben SCPX 8915. Ein Wechsel erzeugt die Maschine neu (wie ein Kaltstart).
  **K8915 V1** (Tastatur K7634) steht ausgegraut darin: ohne den Urlader-Baustein
  für 0400H ist sie nicht startfähig.
* **V2: der Selbsttest läuft durch** (ROM, KEY, CTC, SIO, RAM) bis zur Meldung
  `* Coldstart *`; RETURN lädt das System von A:. Der Emulator benutzt dafür die
  **reparierte Fassung** des dritten ROM-Bausteins (177): der Abzug vom Gerät
  trägt ein gekipptes Bit, mit ihm endete der Selbsttest mit „ROM C“ und
  16 Pieptönen. Der Abzug selbst liegt unverändert bei den Quellen.
* **Takt** 2,4576 MHz (`10 × 2,4576 MHz` usw.), bei beiden Modellen.
* **Laufwerke:** zwei K5601 wie am Gerät; wählbar sind nur 5¼″-Laufwerke.
* **Die Frontplatte in der Statuszeile** — sechs Lampen, von links nach rechts
  wie am Gerät von oben nach unten, jede mit ihrem Schild daneben (`Run`, `Input`,
  `Output`, `Mode`, `Error`, `Power`; der volle Name steht im Tooltip):

  | Lampe | Farbe | leuchtet |
  |-------|-------|----------|
  | Run | grün | solange die Emulation läuft (am Gerät vermutlich `/HALT` der CPU — nicht belegt) |
  | Input File | gelb | beim Lesen von der Diskette |
  | Output File | gelb | beim Schreiben auf die Diskette |
  | RUN Mode | gelb | System bereit (erlischt während eines Diskettenzugriffs) |
  | ERROR | rot | Fehler — Selbsttest, Lesen oder Schreiben |
  | Power | rot | solange der Rechner eingeschaltet ist |

  Die vier mittleren schaltet das Betriebssystem selbst (Anzeigelatch 61H).
* **NMI-Taster** — *Maschine ▸ NMI-Taster* und in der Symbolleiste neben
  *Reset*, ohne Tastenkürzel. Wie am Gerät ist das kein Rückstellen: solange das
  Boot-ROM eingeblendet ist (Selbsttest, Lader), gehen die Lampen aus und der
  Selbsttest beginnt von vorn. **Unter SCPX** liegt an der Einsprungstelle RAM —
  die CPU springt dorthin, meist in einen Absturz. Das ist das Verhalten des
  Geräts nach den Unterlagen und wird bewusst nicht abgefangen; danach hilft
  *Rückstellen*.
* **Die Bildschirmtastatur ist die K7672** (*Ansicht ▸ Tastatur*): deutsche
  Belegung, Funktionsreihe CTRL · ALT1 ^S MOD2 PF1 · PF2–PF9 · CLEAR RESET BREAK,
  Mittelblock PF10–PF12, PA1–PA3, GRAPH und Kursorkreuz, Ziffernblock mit CE, `=`
  und ENTER. Die roten, grünen und grauen Zweitbeschriftungen sind die des
  Originals (rot = die PC-Bedeutung der Taste: `Pause`, `Pg Up`, `Prt Sc` …).
  Jede Taste sendet, was die Tastatur-Firmware für sie vorsieht: unter SCPX den
  PC-Scancode, im Boot-ROM das Zeichen. Tasten, für die es dort nichts gibt
  (die Funktionstasten im Boot-ROM, `CL` — schaltet am Gerät nur den
  Tastenklick um), federn zurück und senden nichts. **Umschalt** und **CTRL**
  rasten für genau eine Taste, **CAPS LOCK** ist eine echte Taste. Die drei
  Lampen **GRAPH**, **CAPS** und **READY** (grün: die Tastatur darf senden)
  zeigen den Zustand der Tastatur.
* Die **PC-Tastatur** geht wie beim A5120 an den Rechner; SCPX setzt die
  Tasten selbst in Zeichen um (deutsche Belegung: `z`/`y` getauscht). `Strg+Pause`
  des K8915 liegt auf `^S` der Bildschirmtastatur. Die **Rücktaste** des PCs
  ist die Kursortaste `←` der K7672: SCPX macht daraus `^H`, und die
  Eingabezeile löscht das Zeichen auch am Schirm. Die Taste `|←|` ergibt unter
  SCPX dagegen `DEL` — das Zeichen verschwindet aus der Eingabe, wird aber noch
  einmal angezeigt (`A>dirxx` führt `dir` aus), wie am Gerät. **Entf** ist die
  Taste `DEL` der K7672 (`^G`, in Editoren wie TP: Zeichen unter dem Kursor
  löschen).

### Bootdiskette mit DISGEN erstellen

DISGEN ist ein Maskenprogramm: In einem **Auswahlfeld** wählt der
**Anfangsbuchstabe** (klein genügt) oder **Esc** schaltet zum nächsten Eintrag,
**Return** übernimmt das Feld bzw. den ganzen Block, die **Kursortasten**
wandern zwischen den Feldern eines Blocks, **Strg+C** geht eine Ebene zurück
(im Befehlsfeld: DISGEN beenden).

1. In A: die Systemdiskette, in B: eine leere Diskette (*Leere Diskette*).
2. B: formatieren: `format` Return, Verfahren `24` Return, Laufwerk `B`
   (groß, mit Umschalt) Return, `00` Return, `79` Return, `01` Return, `Y`
   Return — nach „FUNCTION COMPLETE“ `Y` Return.
3. `disgen` Return, dann Return (Read system tracks) und Return (Laufwerk A).
4. **Nur mit der Diskette 901** (sie stellt B: auf 16 × 256 Byte ein):
   `c` Return (Change device properties), Return (number of drives),
   `b` Return, zweimal Pfeil ↓ bis `sector length`, zweimal Esc bis `1024`,
   Return, Return.
5. `w` Return (Write system tracks), `b` Return.
6. `e` Return (Exit). Die neue Diskette startet jetzt, in A: eingelegt, bis `A>`.

**`write error or device not ready`** beim Schreiben heißt fast immer: B: ist
in DISGEN anders eingestellt, als die Diskette formatiert ist (Schritt 4
vergessen). Return bringt nur ins Laufwerksfeld zurück — mit **Strg+C**
heraus, dann mit `e` Return beenden und bei Schritt 3 neu anfangen: eine
Umstellung **nach** dem Fehlschlag reicht nicht, erst der Neustart von DISGEN
räumt den Puffer des BIOS. Wer DISGEN mit Strg+C verlässt und danach
Steuerzeichen (`^D` …) statt Buchstaben sieht: einmal Strg drücken.

## Der PRG710 Emulator

`prg710emu` ist derselbe Emulator für die Programmiergeräte **robotron PRG 710**
und **PRG 710-1** (ZRE K2521, Speicher mit Seitenverwaltung, Bildschirmkarte
K7024, Diskettensteuerung K5122 mit zwei K5601). Was anders ist:

* **Modell** — *Einstellungen ▸ Allgemein ▸ Modell*: PRG 710 (Tastatur K7609 an
  einem 8279) oder PRG 710-1 (Tastatur K7672). Ein Wechsel erzeugt die Maschine
  neu und tauscht die Bildschirmtastatur; die Wahl wird in `prg710emu.yaml`
  gemerkt.
* **Takt** 2,4576 MHz, **Laufwerke** zwei K5601 wie am Gerät.
* **Einschalten** zeigt „NKM-LOADER“ und wartet auf die **Starttaste**: am 710
  **ET1** (die Taste `ET1` der Bildschirmtastatur oder **Return** des PCs), am
  710-1 **ENTER**. Ohne Diskette folgt „DISKERROR C2“ und „NO SYSTEM“.
* **UDOS 4.3** meldet sich mit der Datumsmaske „Neues Datum :__.__.19__“ — sechs
  Ziffern eintippen (TTMMJJ), **ohne** ET. Danach steht der Prompt `%`.
  Beispiele: `CAT D=0 P=&` (Verzeichnis; ohne `P=&` meldet das 710-1 „FILE NOT
  FOUND“), `DATE`, `COPY OS.INIT 1/KOPIE` (auf Laufwerk 1).
* **Großschrift:** UDOS kennt nur Großbuchstaben. Am 710 liefert die Tastatur sie von
  selbst, am 710-1 schaltet der Systemstart die Feststellung der K7672 ein (CAPS-Lampe);
  wer sie mit der Feststelltaste ausschaltet, bekommt Kleinbuchstaben, und `cat` ist für
  UDOS ein unbekanntes Kommando.
  Ein Rückstellen löscht das UDOS-Datum nicht; die Datumsabfrage kann dann entfallen.
* **SCPX 1526** bootet am 710-1 bis `A>` (`DIR`, `STAT`, `PIP B:=…`).
* **Die Bildschirmtastatur des 710 (K7609)** ist dem Foto des Geräts
  nachgebaut und sendet je Taste den Tastencode des Geräts. **`↵`** ist ET1
  (Return, Starttaste), **`ST`** ist ET2, **`TB`** der Tabulator — Tasten wie
  alle anderen, ohne Kürzel erreichbar. Der Ziffernblock sendet dieselben Codes
  wie die Ziffernreihe. Die **unbeschrifteten** Kappen tragen eine angenommene
  Belegung (der Kurzhinweis nennt sie): die Ovale neben der Leertaste sind die
  Umschaltung, die Kappe links von `Q` ist STRG (beide rasten für genau eine
  Taste), neben `-` liegt `BS`, neben `#` der Drucker-Umschalter, links von `Y`
  der Merker. `+1`, `-1`, `FC`, `BA`, `FW` und `CL` gehören dem PRG-Betrieb und
  haben unter UDOS/SCPX keinen Code: sie federn zurück und senden nichts. `$`
  und `%` haben keine Kappe — über die PC-Tastatur gehen sie.
* **Schnittstellen** — am 710 drei (V.24 X4, IFSS Hauptdrucker X6, ZIFSS
  Zusatzdrucker X5), am 710-1 zwei (V.24, ZIFSS); A32-B trägt dort die Tastatur
  und steht als feste Schnittstelle im Reiter. Dahinter folgt in beiden der
  **Fernschreiber** (siehe unten). `SET PRI TO V24` lädt unter UDOS
  den Druckertreiber `DRUCK.V24`, `SET PRI ON` spiegelt die Ausgabe dorthin.
* Die **PC-Tastatur** geht wie beim A5120 an den Rechner. Am 710-1 wiederholt die
  K7672 eine gehaltene Taste selbst; am 710 gibt es keine Wiederholung.

### Der EPROMmer

Der PRG brennt U555 (1 KB, wie 2708) und U2716 (2 KB) in **einem** Sockel; das
Programm dazu ist `PROG` (UDOS) bzw. `PROG.COM` (SCPX). Im Emulator ist der
Sockel **virtuell**: ein PROM ist ein rohes Abbild (`.bin`). Der Kasten
**EPROMmer** (*Ansicht ▸ EPROMmer*; die Bedienung auch unter *Maschine ▸
EPROMmer*) zeigt, was steckt, welchen Typ das Programm eingestellt hat und ob
Versorgung und Programmierspannung anliegen.

* **PROM-Abbild einlegen…** — eine Datei bis 1 KB wird ein U555, bis 2 KB ein
  U2716; kürzere werden mit FFH aufgefüllt.
* **Leeres PROM einlegen** — ein gelöschtes U555 oder U2716 (alle Zellen FFH).
* **PROM-Abbild speichern unter…** — den Inhalt sichern, etwa nach dem Brennen.
  Ein gebranntes, noch nicht gespeichertes PROM steht als „geändert“ da.
* **PROM UV-löschen** — wie im Löschgerät; die Software kann nicht löschen.
* **PROM entnehmen.**

**Der Sockel wird gemerkt:** ein PROM, das an einer Datei hängt (eingelegt oder
gespeichert), steckt beim nächsten Start wieder im Sockel (`prg710emu.yaml`);
fehlt die Datei, bleibt der Sockel leer und das Protokoll sagt warum. Ein
**ungespeichertes** PROM wird nicht gemerkt: beim Beenden und beim Modellwechsel
fragt das Programm *Speichern / Verwerfen / Abbrechen*.

Gebrannt wird wie am Gerät nur von 1 nach 0: ein zweites Brennen ohne Löschen
ergibt die UND-Verknüpfung. Stimmt der in `PROG` eingestellte Typ (`T`, „PROMGROESSE
1 ODER 2 K-BYTE“) nicht mit dem gesteckten PROM überein, wird gelesen, aber nicht
gebrannt — das Protokoll sagt es. Beispiel unter UDOS: `PROG`, `N`, dann `C`
(kopieren): Quell-PROM einlegen, `J`; bei „COPY-PROM STECKEN“ ein leeres PROM
einlegen, `J`, `J`. Das Brennen dauert in Maschinenzeit gut 50 ms je Byte (U2716)
bzw. 100 Durchläufe über das ganze PROM (U555). Die Antworten in `PROG` sind
einzelne Tasten ohne ET.

### Lochstreifen und Fernschreiber

Leser und Stanzer an der K6022 sind eine Option wie an den anderen Rechnern —
siehe *Lochstreifen* oben (dort auch die Bedienung unter UDOS). Der PRG hat
dazu einen **Fernschreiber** an der Karte 590069:

* **Der Fernschreiber** ist ein Anschluss im Reiter *Schnittstellen* wie die
  anderen — als **Datei** wird daraus ein Fernschreibprotokoll, über **Telnet**
  liest ein Terminal mit. Nachgebildet ist der Fernschreiber selbst: hinaus geht
  Text, nicht die 5-Bit-Zeichen der Leitung. Benutzt wird er von SCPX am 710-1 mit
  einem BIOS `B17172FS`/`B17272FS` (Druckerausgabe, z. B. **^P** und `DIR`); er
  schreibt nur Großbuchstaben, Zeichen ohne Gegenstück im Fernschreibalphabet
  (`>` `*` `#` …) als Zwischenraum, mit 100 Baud — etwa 13 Zeichen je Sekunde.

## Der PC1715 Emulator

`pc1715emu` ist derselbe Emulator für den Bürocomputer **robotron PC 1715**
(U880 mit Urlader-ROM, Bildschirmsteuerung 8275, Diskettensteuerung K5122 mit zwei
K5601, Tastatur mit eigenem Prozessor). Was anders ist:

* **Modell** — *Einstellungen ▸ Allgemein ▸ Modell*: PC 1715 mit dem Bildschirm
  **K7222** (80 × 24, Vorgabe) oder **K7221** (64 × 16). Der Bildschirm ist eine
  Bestückungsvariante der Zentraleinheit: ein Wechsel erzeugt die Maschine neu
  (Kaltstart) und wird in `pc1715emu.yaml` gemerkt. Der **PC 1715W**
  (1987: 256 KB mit Bankumschaltung, U8272 mit DMA, 4 MHz) ist ein eigenes Modell der
  Auswahl; den Bildschirm K7221 gibt es dort nicht.
* **Zeichensatz und Tastatur** — *Einstellungen ▸ Allgemein*: die **Zeichengenerator-
  EPROMs** der Zentraleinheit (**Deutsch** S619 + S602, Vorgabe; **Polnisch**
  S641 + S619; **Kyrillisch** S643 + S605) und das **Tastatur-ROM** (**S600**
  QWERTY, Vorgabe, oder **TAST_618**, QWERTZ mit vertauschtem Y/Z und anderer
  Zeichensetzung — die Bildschirmtastatur beschriftet sich danach). Beides sind
  Bestückungsvarianten: ein Wechsel startet die Maschine kalt und wird in
  `pc1715emu.yaml` gemerkt. Beim **PC 1715W** ist der Zeichensatz ausgegraut (er kommt
  von Diskette), die Tastatur bleibt wählbar. Die Fassung der Floppy-Karte und der
  Urlader haben keine Auswahl.
* **Takt** 2,458 MHz (1715W: 3,9936 MHz), **Laufwerke** zwei K5601.
* **PC 1715W** — bootet **SCP 3.0** (CP/M 3; die Diskette `pc1715w_scp30_system.hfe`
  liegt dem Paket bei). Der Zeichensatz liegt im RAM: der Lader füllt ihn, danach
  tauscht `MODCS name.ZGF[1]` bzw. `[2]` den Satz (die `SC6xx.ZGF` stehen auf der
  Diskette). Motor und Lampe der Laufwerke folgen dem Motorregister bzw. dem
  Zugriff der Diskettensteuerung. Die RAM-Disk (Bänke 3–5) ist ungeprüft; Quasi-Grafik
  und Zusatzkarten sind nicht nachgebaut.
* **Bildschirm** — die unterste Zeile ist die **Statuszeile** von CP/A (inverse
  Anzeige von Laufwerk, Zeichensatz und Feststellung); sie gehört zum Bild.
* **Booten** geht vom Urlader: er sucht Spur 0 Sektor 1 der Laufwerke 0 bis 3 und
  startet SCP 1715, CP/A 1715 oder UDOS 1715 von der eingelegten Diskette; ohne
  Diskette versucht er nach einigen Sekunden den Ladevorgang über **V.24** (X5).
* **Die Bildschirmtastatur** zeichnet die Matrix der Tastatur. **SHIFT**, **CTRL**
  und **REP** wirken nur, solange sie gehalten werden: ein Klick merkt sie
  (gelber Rand), sie werden beim nächsten Tastenklick **vor** der Taste gedrückt
  und danach gelöst. **LOCK** (Buchstaben groß) und **SI/SO** (zweiter
  Zeichensatz) rasten im Tastaturprozessor selbst; ihr Rand zeigt den Stand, den
  die Nachbildung mitgezählt hat. **ET** ist Return. Dazu Funktionstasten
  F1–F15, Cursor- und Ziffernblock mit `CE` und `00`. Das Tastenbild ist dem Foto
  der Tastatur nachgebaut: `®` ist **REP**, die unbeschrifteten Ovale sind
  **LOCK** (mit Leuchte) und die beiden **SHIFT**; die zweite Leuchte neben SI/SO
  zeigt dessen Stand.
* Die **PC-Tastatur** geht wie beim A5120 an den Rechner: Zeichen als Zeichen
  (die Umschaltung sucht sich der Rechner selbst), Pfeile, Entf, Einfg und F1–F12
  als die entsprechenden Tasten. Eine **gehaltene** Taste wiederholt nur, wenn
  zugleich **REP** gedrückt ist — so will es der Tastaturprozessor.
* **Schnittstellen** — **Drucker (X4)** und **V.24 (X5)**; die Tastatur hängt
  fest an SIO-A und steht nicht im Reiter. Ein Drucker ohne Verbindung gilt als
  besetzt.

## Der MC80 Emulator

`mc80emu` ist derselbe Emulator für die Mikrorechner **MC80.30** und **MC80.31** des
VEB Elektronik Gera (U880 mit 2,4576 MHz, Betriebssystem **EGOS** im ROM, Grafikkarte
VIS 2a mit 512 × 256 Bildpunkten, abgesetzte Tastatur mit eigenem Prozessor) sowie für
die älteren **MC80.21** und **MC80.22** (eigener Abschnitt am Ende des Kapitels). Was
anders ist:

* **Modell** — *Einstellungen ▸ Allgemein ▸ Modell*: **MC80.30** (Vorgabe) oder
  **MC80.31** (zusätzlich die EPROM-Programmsegmente 10H/20H). Ein
  Wechsel erzeugt die Maschine neu (Kaltstart) und wird in `mc80emu.yaml` gemerkt. Die
  **MC80.21/.22** sind wählbar (siehe unten); die **MC80.33** (vom Treiber-ROM der AKM
  liegt nur die Hälfte vor) steht ausgegraut in der Auswahl.
* **Speichererweiterung** — *Einstellungen ▸ Allgemein*: eine oder zwei Karten **SPE 2**
  (die zweite bringt das RAM-Segment 02H). Ein Wechsel startet die Maschine kalt.
* **Keine Laufwerke** — der Kasten „Laufwerke“, die Diskettenmenüs und die
  Laufwerksfelder der Statuszeile fehlen. RAM-Disk und Lochstreifen gibt es am MC80
  nicht.
* **Kassette** — *Einstellungen ▸ Allgemein ▸ Kassette* steckt die Kassettenkarte
  K5020 an E/A 30H (MC80.30: Laufwerk LW 1200, MC80.31: K5200; siehe *Kassette*).
  EGOS bedient nur **Laufwerk 1**. Nach `INIT`: `KINIT 1` formatiert die Kassette
  (gut eine Minute Bandzeit), `WRITE 1,NAME,01,4000,40FF` schreibt den Bereich
  4000H–40FFH aus Segment 01 unter NAME, `READ 1,NAME,R` holt ihn an dieselbe Adresse
  zurück und hängt ihn in die Kommandokette — steht dort ein Programm mit Kennbyte,
  ist es danach wieder ein Kommando. Eine neu angelegte Kassette hat am MC80 einen
  kurzen Vorspann (sie ist eine Kompaktkassette) und keine Anfangs-/Endmarke (die verträgt EGOS nicht); man muss sie
  nicht vorher zurückspulen.
* **Einschalten** — EGOS meldet sich nach gut einer Sekunde mit
  „mc 80.30. microcomputer“ und „Version 3.0“, unten steht `OK` und der Prompt `>`.
  **Erst `INIT` eingeben** (mit ENTER): das legt die Kommandokette im RAM an; Tasten,
  die vor dem Ende von INIT kommen, gehen verloren.
* **Programme laden** — *Maschine ▸ Speicherabzug laden…* lädt eine Binärdatei ab einer
  Adresse in den Arbeitsspeicher (erfragt wird die Adresse; steht sie im Dateinamen wie
  bei `tetris_4000_4fff.bin`, ist sie vorgeschlagen). Gestartet wird mit dem
  EGOS-Kommando des Programms, z. B. **TETRIS**: nach `INIT` ab 4000H laden, dann
  `te` + ENTER; **HASI** ebenso ab 4000H mit `hasi`. **Spiele und andere Programme erst nach
  `INIT` laden** — vorher gibt es die Kommandokette nicht, in die sie sich einhängen.
  Kein Tastenkürzel.
* **Die Bildschirmtastatur** zeichnet das Tastenbild der Betriebsdokumentation
  (Bild A5): Hauptfeld mit **CTRL**, Feststeller **⇩** und den beiden Umschalttasten
  **⇧**, darunter **OFF**, Leertaste und **ENTER**; rechts die acht **Steuertasten**
  S0–S7 (Sondertasten 0–7 von EGOS) und der Ziffernblock. Die Tastatur sendet
  **Zeichen**, nicht Tastenpositionen. Kursortasten, **CL**, **CRLF**, **÷** und **00**
  lassen sich drücken, senden aber nichts — ihre Codes sind nicht überliefert. Die
  Leuchten links zeigen CTRL und Umschaltung, die rechts neben CL die Bereitschaft.
* Die **PC-Tastatur**: Zeichen als Zeichen (EGOS tauscht in der Vorgabe Groß und
  Klein — „a“ erscheint als „A“), **Enter** = ENTER, **Esc** = OFF (Abbruch),
  **Tab** = Tabulator, **F1–F8** = Steuertasten S0–S7. Die Pfeiltasten wirken nicht.
* **Text kopieren** — der Bildschirm ist eine Grafik; *Maschine ▸ Bildschirminhalt als
  Text kopieren* (oder Rechtsklick) erkennt die EGOS-Schrift wieder und liefert 25
  Textzeilen.

### MC80.21 und MC80.22

Die Geräte der älteren Reihe haben **nichts mit dem MC80.3x gemeinsam außer dem Takt**:
andere Zentraleinheit (K2521, Betriebssystem **BSYS** statt EGOS), Programmspeicher aus
steckbaren EPROM-Sockeln (PFS), und **kein Bildwiederholspeicher mit Grafikkarte**. Das
Bild ist eine **Oszilloskop-Darstellung**: die Display-Logik (DPL) lenkt den Strahl zeilenweise
ab, schreibt acht Textzeilen zu je 32 Zeichen (5 × 7-Punktraster) und kann statt einer Zeile
eine **Analogkurve** zeichnen. Der Emulator bildet das als Bild von 528 × 256 Punkten ab; die
Bildwechselfrequenz liegt bei etwa 50 Hz.

* **Modell** — *Einstellungen ▸ Allgemein ▸ Modell*: **MC80.21** oder **MC80.22**. Beide
  verhalten sich im Emulator gleich (ein Unterschied ist nicht belegt). Die Wahl der
  Speicherkarten SPE 2 gilt nur für die .30/.31 und ist hier ausgegraut.
* **Einschalten** — das BSYS zeigt ein **Menü**: `TASTENTEST`, `INIT`, `BAND`, `EDITOR`,
  `BASIC-80.2` … mit Adresse vor jeder Zeile. Mit **Kursor ↓/↑** wählt man eine Zeile,
  **ENTER** startet sie, **OFF** führt zurück. Die Zeichenform kommt aus einem Ersatz-
  Zeichengenerator (5 × 7-Satz); einzelne Zeichen (`@ & ^ \`) sehen am Gerät anders aus.
* **Kassette** — fest eingebaut: das Magnetbandinterface (MBI) mit Laufwerk SK 1200. Das
  Kästchen *Kassette* ist deshalb ausgegraut; der Kasten „Kassette“ zeigt das eine
  Laufwerk. Neue Bänder sind Kompaktkassetten. Im Menü `BAND` speichert und lädt man
  Programme (Name, Blocknummer, Anfangs- und Endadresse eingeben, je mit ENTER).
* **Bildschirmtastatur** — die Tasten der 8 × 7-Matrix mit BK, CL, CTR, UC (zweite Ebene)
  und Kursor. **Die Lage der Tasten ist nicht belegt** (das Tastaturfoto der Unterlagen ist
  zu klein): die Nachbildung ordnet sie logisch nach der Codetabelle des BSYS an (QWERTZ).
  Die PC-Tastatur wirkt wie üblich; **Esc** = OFF, **Rück/Entf** = CL, **Pause** = BK,
  Pfeile = Kursor, **Strg** = CTR.
* **Programme laden** — *Speicherabzug laden…* schreibt eine Datei ab einer Adresse in den
  Speicher; wirksam ist das nur im RAM (Bereich ab C000H, die OPS-Karte). Wie ein Programm
  gestartet wird, ist für den MC80.2x nicht überliefert [?].
* **Nicht vorhanden** am MC80.2x: Lochstreifen, RAM-Disk, Laufwerke.
* **Text kopieren** — liefert die acht Textzeilen des Bildes (Analogzeilen erscheinen leer).

## Der P8000 Emulator

`p8000emu` ist derselbe Emulator für den 16-Bit-Arbeitsplatzcomputer **EAW P8000**:
8-Bit-Karte mit U880 (Monitor MON8, Betriebssystem **UDOS**), 16-Bit-Karte mit U8001 und
Speicherverwaltung (Monitor MON16, **WEGA**/UNIX) und Winchesterkontroller (WDC). Der P8000 hat
**keinen Bildschirm am Rechner**: seine Konsole ist ein serielles **Terminal** (tty1) — der
Emulator bildet das echte Terminal Typ 2 samt Flachtastatur nach (nächster Abschnitt). Es gibt
**ein** Programm; ob es den Rechner samt Terminal oder nur den Terminal-Arbeitsplatz zeigt, wählt
die *Betriebsart*. Was anders ist:

* **Rechnerausstattung und Betriebsart** — *Einstellungen ▸ Allgemein*. *Rechnerausstattung*:
  **Vollgerät** (16-Bit-Teil und Winchester, Vorgabe), **ohne Winchester** (nur UDOS und Monitor)
  oder **nur 8-Bit-Teil** (die Koppelsoftware meldet dort „Hardware Error in Connection" — das ist
  das Soll eines P8000 ohne 16-Bit-Karte). *Betriebsart*: **Computer mit Terminal** (Vorgabe) oder
  **nur Terminal** (s. u.). Ein Wechsel startet die Maschine kalt. Alte Konfigurationen mit dem
  früheren schlanken Kern-Terminal werden beim Laden auf die Ausstattung mit Originalterminal
  abgebildet.
* **Voreinstellung P8000 / P8000 Compact** — *Einstellungen ▸ Allgemein ▸ Modell*. Beide sind **Vorgabe-Sätze**, keine
  eigenen Maschinen (wie A5120/A5130/K8924 beim A5120): die Wahl übernimmt ihre Werte ohne Rückfrage und baut die Maschine
  neu auf; danach ist jede Option einzeln änderbar, auch das erneute Wählen derselben Voreinstellung setzt sie wieder.
  **P8000** (Vorgabe): Platinenindex 1/1 (8-Bit 1 / 16-Bit 1), MON8 3.0, MON16 3.0, Hauptspeicher 4 × 256 KB, RTC U130X, 2 Diskettenlaufwerke K5601, 1 Winchester
  K5504.50. **P8000 Compact** (1989, die P8000 im Tischgehäuse mit unveränderten Platinen): Platinenindex 3/4 (8-Bit 3 / 16-Bit 4), MON8 3.1, MON16 3.1, 4 × 1 MB, RTC U130X,
  3 Diskettenlaufwerke K5601, 1 Winchester K5504.50. WDC-Firmware und Plattenparameter gehören nicht dazu und
  bleiben. Der Platinenindex gehört zur Voreinstellung (MON8 3.0 meldet am Index 3/4 nach der RESET-Taste „Hardware Error in
  Connection“), ist danach aber einzeln änderbar. Die Voreinstellung ist unabhängig von *Rechnerausstattung* und *Betriebsart*; der Fenstertitel folgt ihr
  (*P8000 Emulator* / *P8000 Compact Emulator*). Die Epson RTC-72421 (Community-Nachbau) bleibt unter *RTC* wählbar, ist aber
  nie Vorgabe. Nachgebildet ist nur die Variante mit 16-Bit-Teil und Originalterminal; eine 8-Bit-Compact und der WDOS-Adapter fehlen.
* **ROM-Fassungen und Bestückung** — ebenfalls unter *Allgemein*: Platinenindex (beide 1 oder 8-Bit 3 /
  16-Bit 4; die Voreinstellung setzt ihn), **MON8** 3.0/3.1, **MON16** 3.0/3.1/3.3, Hauptspeicher der
  16-Bit-Seite (256 KB, 1 MB, 2 MB) und **WDC-Firmware** 4.2/4.0.05/3.4.05. Wo eine Wahl am
  Modell nichts bewirkt, ist das Feld ausgegraut. Jeder Wechsel ist ein Kaltstart.
  Die **Uhrenkarte** (Gruppe *RTC*) steht im Abschnitt *Die Echtzeituhr*.
* **Takt** 4 MHz (Vorgabe fünffach: der Hardwaretest mit Speichertest braucht rund 16 s
  Maschinenzeit, bis „Press RETURN" erscheint). **Laufwerke**: zwei K5601 (UDOS-Laufwerk 0
  und 1, hier **A:** und **B:**); der externe Stecker bleibt leer.
* **Das Terminal** — der Kasten *Bildschirm* zeigt das Originalterminal (tty1 = Konsole) im
  selben Röhrenbild wie die anderen Emulatoren: Helligkeit, Kontrast, Krümmung, Phosphorfarbe und
  die übrigen Regler stehen unter *Einstellungen ▸ CRT* und wirken genau wie dort. ADM31 gilt nach
  dem Einschalten, umschalten tut der Gast (WEGA: `ttytype`).
* **Tasten** — die Tastatur des Wirtsrechners geht an das Terminal; Belegung und Bildschirmtastatur
  (Flachtastatur K7673.09) im Abschnitt *Das Originalterminal*. **F11** (Vollbild) ist die einzige
  Taste, die das Fenster für sich behält.
* **Text kopieren** — Rechtsklick auf das Bild oder *Maschine ▸ Bildschirminhalt als Text kopieren*:
  der Inhalt des Terminalbildes (80 × 24, nicht gerendert, ohne Schlussleerzeichen) landet als reiner
  Text in der Zwischenablage. Ein Tastenkürzel gibt es nicht (Strg+C gehört dem Gast).
  Der Rechtsklick bietet außerdem *Zwischenablage über Tastatur einfügen* (siehe *Rechtsklick auf das Bild*).
* **Mitschrift** — *Einstellungen ▸ Allgemein ▸ Mitschrift*: ein Feld mit dem Dateinamen und daneben der
  Ordnerknopf, der eine `.txt` wählt oder anlegt. Alles, was das Terminal **vom Rechner empfängt**, wird dort
  als Text mitgeschrieben (Zeilenenden, Tabulatoren, druckbare Zeichen; Escape-Folgen wie Cursor setzen und
  Löschen fallen weg). Eine vorhandene Datei wird **fortgesetzt**, nie überschrieben. Weil der Rechner
  getippte Zeichen zurückspiegelt (Echo), steht darin auch die Eingabe — Passwörter nicht, die werden nicht
  gespiegelt. Eine angefangene Zeile (Eingabeaufforderung) erscheint nach einer Sekunde Ruhe. Feld leeren
  schaltet die Mitschrift aus; ist die Datei nicht beschreibbar, meldet es die Statuszeile und die
  Mitschrift stoppt, der Emulator läuft weiter. Das Feld gilt für beide Betriebsarten (Computer mit
  Terminal und nur Terminal) und wird in der Konfiguration gemerkt.
* **Mehrere Winchester** — *Einstellungen ▸ Laufwerke ▸ Winchester-Laufwerke*: 1 bis 3 (Vorgabe 1). Jedes Laufwerk bekommt im
  Kasten *Laufwerke* einen eigenen Plattenkasten (*Laufwerk 0*, *1*, *2*) mit **Anschließen…**, **Neue Platte…**, **Abtrennen**
  und eigener Lampe; die Platte jedes Laufwerks wird gemerkt. Die Zahl wirkt ohne Neustart der Maschine, der WDC erkennt neu
  angeschlossene Platten aber erst beim Hochlauf (*Rückstellen* oder *Rechner ein*). Wer die Zahl verringert, trennt die
  weggenommenen Laufwerke ab (vorher wird zurückgeschrieben). Ohne Winchesterkontroller (*ohne Winchester*, *nur 8-Bit-Teil*)
  ist die Wahl gesperrt. Das Lämpchen *Platte* der Frontplatte leuchtet bei Zugriff auf irgendeines der Laufwerke; zwei
  Instanzen können dieselbe Plattendatei nicht gleichzeitig belegen (Sperre je Datei). Unter WEGA ist eine zweite
  Platte an Laufwerk 1 nutzbar — sie muss vom selben Typ sein wie Laufwerk 0 und braucht einen Kern mit `md10`.
  Die mitgelieferte WEGA-3.1-Platte hat ihn schon und kommt mit einer fertigen zweiten Platte
  (`p8000_wega31_usr2.avr.img.gz`; Abschnitt *Zweite Festplatte unter WEGA einrichten*); Laufwerk 2 kann WEGA
  nicht ansprechen.
* **Winchesterplatte** — im Kasten *Laufwerke* unter den Disketten (je Laufwerk einer). **Anschließen…** hängt ein
  vorhandenes Abbild an den WDC, **Neue Platte…** legt eines an (Typ K5504.50 u. a.; Wahl
  **Inhalt**: Vorgabe *Unformatiert, wie ein neues Laufwerk* — ohne Parametersatz, formatieren mit
  `sa.format` im Gast; oder *Formatiert mit Parametersatz (K5504.50, leer)*, nur für Sonderfälle),
  **Abtrennen** löst es. Es gibt **keine vorgegebene Platte** (das Programm legt von selbst keine an; der Plattenkasten
  schlägt für *Neue Platte…* den Namen `p8000_platte.k5504.img` im Diskettenordner vor). **Der Dateiname trägt den Typ:**
  `<name>.<kürzel>.img` mit `k5504` (K5504.50), `d5126`, `d5146` oder `vs`; *Neue Platte…* hängt das Kürzel
  selbst an, und der Dialog *Anschließen…* zeigt bei den älteren WDC-Fassungen (unten) nur die Dateien des
  passenden Typs — *Alle Dateien* bleibt der Ausweg; eine Datei ohne Kürzel wird nur auf ihre Größe geprüft.
  Die Endung bleibt `.img`, damit DiskTool und Dateimanager die Abbilder weiter erkennen.
  **Gepackt:** *Neue Platte…* legt standardmäßig `<name>.<kürzel>.img.gz` an (gzip; eine leere K5504.50 sind
  47 MB roh und gut 200 KB gepackt) — abwählbar mit *Gepackt speichern*. Beide Arten lassen sich anschließen;
  das Programm erkennt die Packung am Inhalt und schreibt in derselben Art zurück. `gunzip` macht aus einer
  gepackten Platte jederzeit ein rohes `.img`, `gzip` umgekehrt. Eine Platte hat **keinen Schreibschutz**; geschrieben
  wird von selbst — im Hintergrund, nach einer halben Sekunde ohne Plattenzugriff, aber höchstens alle 5 Sekunden —
  und sofort beim Abtrennen, bei *Rückstellen*, bei einem Zwischenstand und beim Beenden (das Beenden wartet, bis die
  Datei steht). Der WDC erkennt die Platte beim
  Hochlauf: eine im Betrieb angeschlossene sieht der Gast erst nach *Rückstellen* oder
  *Rechner ein*. Die Lampe im Kasten und **Platte** in der Statuszeile leuchten bei jedem
  Zugriff. *Hinweis:* der 16-Bit-Monitor versucht nach dem Hardwaretest, von der Platte zu
  starten; auf einer leeren (E5-gefüllten) Platte endet das in einer Eingabeschleife ohne
  Prompt — das ist Gastverhalten, kein Fehler, und der Grund, warum das Programm keine leere
  Platte vorgibt. Erst die WEGA-Installation macht die Platte startfähig (Ablauf im Kapitel
  *Bedienung des P8000*).
* **WDC-Firmware und Plattentyp** (*Einstellungen ▸ Allgemein*, Feld *WDC-Firmware*): bei **4.2** (Vorgabe)
  steht der Parametersatz auf der Platte, der Typ ist frei wählbar, und der Parametersatz muss zur
  Dateigröße passen. Bei **4.0.05** und **3.4.05** (*experimentell*) sind die Laufwerksdaten im EPROM
  eingebrannt: das Laufwerk ist immer die **K5504.50** (1024 Zylinder, 5 Köpfe, 18 Sektoren,
  47 185 920 Byte). Die Typwahl in *Neue Platte…* ist dann gesperrt, und eine Datei anderer Größe
  weist das Programm mit Klartext ab („Firmware 3.4.05 gehört zu K5504.50 …, die Datei hat … B“). Wechselt man die
  Firmware bei angeschlossener Platte, die nicht passt, fragt das Programm vor dem Neustart, statt die
  Platte still zu trennen. Die 3.4.05 schreibt ihre Spuren anders als die 4.2 (Sektoren der Reihe nach);
  der Emulator bildet das nach, und `sa.format` V1.4 der WEGA-3.0-Startdiskette läuft damit bis zum
  Formatieren und zum Schreiben der Bad-Track-Tabelle (eine Spur geprüft, nicht die ganze Platte).
  Der Parametersatz einer Platte wird **nicht** vom Programm ergänzt (*Parametersatz der Platte: aus*);
  nur das WEGA-3.1-Abbild des AVR-Emulators (1380/10/18) braucht *ergänzen*.
* **Statuszeile** (nur *Computer mit Terminal*) — Lampen **Run** (RUN-LED der 16-Bit-Karte), **16-Bit** (der U8001 läuft),
  **Platte** (WDC-Zugriff) und **Power**, dann der Takt und die Laufwerke. Ohne
  16-Bit-Karte liefert der Kern keine Lampen: sie bleiben dunkel.
* **NMI** (nur *Computer mit Terminal*) — *Maschine ▸ NMI-Taster*: die NMI-Taste des Geräts; ist die 16-Bit-Karte
  freigegeben, geht sie an den U8001 („Press NMI" im Hardwaretest von MON16).
* **Zwischenstand** (nur *Computer mit Terminal*) — *Maschine ▸ Zwischenstand sichern…/laden…* (P8KS): sichert die Maschine
  samt Terminalbild, aber **nicht** Disketten und Platte; beim Laden müssen dieselben Abbilder
  liegen, und Modell, ROM-Fassungen, Hauptspeicher und Laufwerke müssen übereinstimmen (sonst
  bleibt die Maschine unverändert, mit Begründung). Zuverlässig nur **am Prompt**: Zeichen im
  Flug an den seriellen Schnittstellen und in den Wandlern stehen nicht im Stand.
* **Schnittstellen** — tty1 hängt fest am Terminal; **tty0, tty2, tty3** (8-Bit-Karte) und mit
  16-Bit-Karte **tty4–tty7** stehen im Reiter *Schnittstellen* (Telnet, RFC 2217, Datei). Ein
  zweites Terminal an WEGA (`getty` auf tty0, tty2, tty4–tty7) ist also ein Telnet-Server an dem
  Kanal mit einem Arbeitsplatz (*Betriebsart: nur Terminal*, s. u.) oder einem Terminalprogramm davor (für
  Vollbildprogramme wie `vi` die Terminalart beachten: WEGA 3.0 führt alle Kanäle als ADM31). Ohne
  Verbindung verfallen die Zeichen in Zeichenzeit.
* **Bootweg** — Hardwaretest, „U880-Softwaremonitor … Press RETURN", RETURN, danach bootet MON8
  von der eingelegten UDOS-Systemdiskette (A:). Mit 16-Bit-Karte startet `x` bzw. die
  Koppelsoftware von UDOS den U8000-Monitor an tty1.
* Eine **RAM-Disk** (RAF), den **Lochstreifen** und die **Kassette** gibt es am P8000 nicht; die Einstellungen
  bieten sie dort nicht an.

### Das Originalterminal und der Mehrplatzbetrieb

Der P8000 hat das **echte Terminal Typ 2**: Z8-Rechner mit der
Original-Firmware P8T 5.0, 8275 und Zeichengenerator, dazu die **Flachtastatur K7673.09**. Der
Escape-Folgen-Parser, die Zeichensätze, Cursor und Blinken stammen aus der Firmware, nicht aus
einer Nachbildung. Gewählt wird die *Betriebsart* unter *Einstellungen ▸ Allgemein*:

* **Computer mit Terminal** (Vorgabe): der Rechner in der gewählten Ausstattung (Vollgerät / ohne
  Winchester / nur 8-Bit); seine Konsole tty1 ist das Originalterminal.
* **Nur Terminal**: ein **Arbeitsplatz** ohne Rechner. Er hat weder Laufwerke noch Winchester noch
  Frontplatte: der Reiter *Laufwerke*, der Kasten, die Lampen und Laufwerksfelder der Statuszeile, der
  NMI-Taster, *Diskette einlegen/auswerfen* und der Zwischenstand verschwinden; seine serielle Leitung
  führt zu einem Rechner (*Maschine ▸ Verbindung zum Rechner…*). Die gewählte Rechnerausstattung bleibt
  gemerkt und ist beim Zurückschalten wieder da. Dieselbe Wahl trifft der Start mit
  `p8000emu --mode terminal` (bzw. `--mode computer`).

**Bild.** Das Terminal liefert das fertige Pixelbild (640 × 312); es wird nur neu geholt, wenn die
Maschine ein neues Bild geliefert hat, und füllt den Bildschirmkasten wie bei den anderen Maschinen.
Farbe, Helligkeit und alles Übrige stellt *Einstellungen ▸ CRT* ein; eine eigene Farb- oder
Zoomwahl am Bild gibt es nicht. Der Rechtsklick bietet *Bildschirminhalt als Text kopieren*.

**Tastatur.** Die Tasten des Wirtsrechners gehen über die **Tastenmatrix der K7673**: ein Zeichen wird auf
die Taste (und gegebenenfalls Umschalt) zurückgeführt, die es erzeugt — ein deutsches `ä ö ü ß` landet auf
den Tasten des Zeichensatzes 2 (erst **SI/SO**, F8, einschalten). Strg, Umschalt und Feststelltaste gehen als
die entsprechenden Tasten, **Return**, **Esc**, **Tab**, **Rücktaste** (BS), **Entf** (DEL), die Pfeile,
**Pos1** und **Einfg** (CHAR INSERT) ebenfalls; **F2** PAGE ERASE, **F3** LINE INSERT, **F4** CHAR INSERT,
**F5** LINE DELETE, **F6** CHAR DELETE, **F7**/Pause BREAK, **F8** SI/SO, **F9** MODE, **F10** VIDEO,
**F12** ON/OFF. Die Wiederholung gehaltener Tasten macht die Tastatur selbst. Die **Bildschirmtastatur**
(*Ansicht ▸ Tastatur*) zeigt die Matrix mit allen 105 belegten Tasten: ein Klick drückt die Taste,
**Umschalt und Strg rasten** beim Klick ein (zweiter Klick löst sie), die **rechte Maustaste** hält jede
Taste fest. Anordnung und Beschriftung folgen dem Foto einer echten Tastatur (deutsches Tastenfeld, Funktionsreihe mit
OFF, SI/SO, MOD, VIDEO, BREAK und F1–F11, Bearbeitungsblock, Cursorkreuz, Ziffernblock mit CE und 00);
die LEDs OFF, CAPS und MOD sitzen wie dort im Anzeigefeld oben rechts. Bei F1–F11, CE und den Zeilen-/Zeichentasten des Bearbeitungsblocks beruht die Zuordnung zur Matrix auf einer
Annahme (die Tooltips nennen den Scancode); die meisten F-Tasten haben im Terminal 5.0 keine Wirkung. Die Kürzel des Fensters
(nur mit Strg+Umschalt, F11) bleiben, alles Übrige gehört dem Terminal.

**Mehrplatz Schritt für Schritt** (ein Rechner, ein oder mehrere Arbeitsplätze). Ein installiertes WEGA
startet `getty` im Mehrbenutzerbetrieb auf der Konsole (tty1) und auf **tty0, tty2, tty4, tty5, tty6, tty7**
(`/etc/inittab`); **tty3** ist dem Drucker vorbehalten. Im Gast ist also nichts zu ändern:

1. **Rechner starten:** `run_p8000emu.sh`, Betriebsart *Computer mit Terminal*, Platte mit installiertem
   WEGA anschließen. Nach dem Hardwaretest RETURN, RETURN (UDOS mit Koppelsoftware), **NMI** (*Maschine ▸
   NMI-Taster*) bei „Press NMI"; danach startet WEGA **von selbst** („> boot", „: md(0,16000)wega" erscheinen
   ohne Eingabe — nichts tippen), prüft die Dateisysteme und meldet nach rund vier Minuten Maschinenzeit
   „WEGA login:". An der Konsole anmelden (Superuser `wega`, Kennwort `root`).
2. **Telnet-Server an tty4 einschalten:** *Einstellungen ▸ Schnittstellen*, Block **tty4**, Betriebsart
   Telnet, Rolle **Server**, Port **5004** (der Vorschlag), **Starten**. In der Statuszeile steht
   „Telnet/RFC2217 Server Port: 5004". Die anderen Leitungen tty0, tty2, tty3, tty5–7 laufen genauso
   (Vorschläge 5000, 5002, 5003, 5005–5007).
3. **Zweiten Prozess als Terminal verbinden:** `run_p8000emu.sh --mode terminal --instance platz2`
   (Betriebsart *nur Terminal*, eigene Konfiguration), *Maschine ▸ Verbindung zum Rechner…*: Art Telnet,
   Rolle Client, Rechner `127.0.0.1` (oder die Adresse des Rechners im Netz), Port **5004**, **Verbinden**.
   Die Statuszeile zeigt „Rechner: verbunden mit …". Ein Client versucht es selbst weiter, bis der Rechner
   antwortet; nur *Trennen* beendet das.
4. **Anmeldung:** `getty` hat seine erste Anmeldezeile meist schon ins Leere geschickt, bevor der
   Arbeitsplatz verbunden war — **RETURN** am Arbeitsplatz holt „WEGA login:" (das Einschalt-00H des
   Terminals stört nicht). Name und Kennwort wie an der Konsole; `who` an der Konsole zeigt danach jede
   Anmeldung mit ihrem Kanal, `write wega tty4` schickt eine Nachricht hinüber, `exit` meldet ab, und die
   Anmeldezeile kommt von selbst wieder. Die Terminalart steht in `/etc/ttytype`: WEGA 3.0 führt alle
   Kanäle als `P8` (ADM31) — die Betriebsart, in der das Terminal einschaltet. Vor dem Ausschalten
   `sync;sync` an der Konsole. Ohne laufendes Betriebssystem zeigt ein Test am Monitor, dass die
   Verbindung steht: `pw 25 05`, `pw 25 68`, `pw 24 54` am Rechner schreiben ein „T" auf die Leitung tty0 (Port-
   Ausgabebefehle des Monitors MON8; der Arbeitsplatz an tty0 zeigt das Zeichen).

Mehrere Instanzen nebeneinander: `--instance NAME` (oder die Umgebungsvariable `K1520_INSTANZ`) legt eine
eigene Konfiguration `p8000emu-NAME.yaml` an und setzt den Namen in den Fenstertitel; `--config DATEI`
legt die Konfigurationsdatei von Hand fest. Disketten- und Plattenabbilder gehören je einer Instanz: ein
Abbild, das ein anderer Prozess schon hält, wird nicht angeschlossen (Meldung in der Statuszeile).

### Die Echtzeituhr (RTC)

Die Uhrenkarte steckt am 16-Bit-Teil; *Einstellungen ▸ Allgemein ▸ RTC* hat dafür zwei Felder
nebeneinander. Ein Wechsel ist ein Kaltstart.

* **Typ.** *Original U130x* ist die Karte des Geräts; *Epson RTC-72421* ist der Nachbau von
  pofo.de. *keine* lässt den Bus an der Stelle leer. Vorgabe der Auslieferung: *Original U130x*.
  **Welche Platte die Karte liest:** der Kern der ausgelieferten **WEGA-3.1-Platte** erkennt
  **beide** Typen und meldet beim Start `RFT U130X RTC found` bzw. `Seiko Epson RTC-72421 found`.
  Der Kern der **WEGA-3.0-Platte** (Stand 02/88) hat gar keinen Uhrentreiber: er ignoriert jede
  Karte und läuft mit der Zeit, die das Dateisystem zuletzt kannte — das ist kein Fehler des
  Emulators. Auf der 3.1-Platte zeigt `date` die Wirtszeit schon **ohne Zeitversatz** richtig
  (2026 ist kein Problem).
* **Datum beim Start.** WEGA fragt beim Hochfahren immer „Enter Date … / Enter Time …“; davor steht
  „Last known date and time:“ — das ist die Zeit, die der Kern von der Karte gelesen hat. Wer
  nicht antwortet, bekommt nach zwei Minuten genau diese Zeit; wer Datum und Zeit eintippt,
  überschreibt sie (die Zeit lässt sich nicht mit RETURN überspringen).
* **Zeitversatz.** Die Uhr zeigt dem Gast die Zeit des Wirts um 0, 28 oder 40 Jahre zurück
  (gerechnet wird nur, was der Gast liest und schreibt). *−28 Jahre* trifft Wochentag **und**
  Schaltjahrmuster, *−40 Jahre* nur das Schaltjahrmuster. Vorgabe: *−40 Jahre*. Das Feld ist nur
  mit gesteckter Karte aktiv.   Alle drei Werte sind für **beide** Karten wählbar; der Emulator bildet die 2019-Grenze des echten
  U130x-Chips bewusst nicht nach, die Karte arbeitet auch ohne Versatz. Was dabei zu beachten ist:
  ohne Versatz lässt sich die U130x unter WEGA nicht mit `sa.timer` stellen (der Treiber nimmt nur
  die Jahre 1980–2019 an); *−28 Jahre* trifft im Wirtsjahr 2028 den Jahr-2000-Fehler der
  Stellroutinen von `sa.timer` (beide Karten); die **harte Grenze** ist 2038, weil WEGA die Zeit in
  32 Bit rechnet — ohne Versatz geht es bis 2037, mit −28 bis 2065, mit −40 bis 2077. Der
  Kerntreiber der 3.1-Platte liefert in Schaltjahren ab März ein Datum einen Tag zu früh
  (gemessen für beide Karten: 15.03.2028 erscheint als 14.03.); der 29.02. stimmt. Den Wochentag liest kein Treiber. Die Schaltfläche *Uhr nach
  Wirtszeit stellen* ist von alledem nicht betroffen (sie schreibt die Karte direkt).
* **Ohne 16-Bit-Teil** (Ausstattung *nur 8-Bit*) sind beide Felder ausgegraut; die Wahl bleibt
  gemerkt und gilt wieder, sobald die Ausstattung ein 16-Bit-Teil hat. Im Betrieb *nur Terminal*
  fehlt die Gruppe ganz.
* **Zeitzone.** Die Karte läuft immer in **UTC**; WEGA liest sie als GMT und rechnet mit seiner
  eigenen `TZ` (ausgeliefert `MEZ-1MES`) in Ortszeit um. Das Programm zeigt deshalb die Kartenzeit
  **in der Ortszeit des Wirts**, also das, was `date` unter WEGA zeigt. Setzt man unter WEGA eine
  andere `TZ`, zeigt `date` entsprechend eine andere Zeit als dieses Feld. Eine Batteriedatei aus
  der Zeit vor dieser Regel (Ortszeit gespeichert) geht bis zum nächsten *Uhr nach Wirtszeit stellen*
  um die Zonendifferenz vor; zurück springt sie nie.
* **Anzeige.** *Uhrzeit des Gastsystems* nennt die Zeit, die der Gast von der Karte liest (mit
  Versatz, in Ortszeit); bei laufender Maschine ändert sie sich einmal je Minute. Ist die Maschine aus, steht dort
  die Zeit, mit der sie jetzt starten würde. Dieselbe Zeit steht als Feld **RTC:** in der Statuszeile.
* **Warum die Uhr nie zurückspringt.** Die Uhr zählt in **Maschinenzeit**, nicht nach der Wanduhr.
  WEGA liest sie nur beim Start und zählt danach selbst; läuft die Emulation schneller als in
  Echtzeit (Vorgabe fünffach), eilt die Gastzeit der Wirtszeit voraus. Folgte die Karte der Wanduhr,
  stünde sie beim nächsten Start vor den Zeitstempeln der Dateien, und `make` hielte Quellen für
  neuer als ihre Ergebnisse. Der Stand liegt in einer kleinen Batteriedatei im Konfigurationsordner
  (`rtc_p8000emu.txt`, je Instanz eine eigene) und wird beim Beenden, beim Stellen und etwa einmal je
  Minute Maschinenzeit geschrieben. Beim Start gilt der spätere von „Stand plus Abwesenheit“ und
  „Wirtszeit jetzt“.
* **Uhr nach Wirtszeit stellen** setzt die Uhr auf die jetzige Wirtszeit (mit gewähltem Versatz),
  auch bei ausgeschalteter Maschine. Würde die Gastzeit dadurch zurückgehen, fragt das Programm
  vorher: Dateien, die der Gast mit der späteren Zeit angelegt hat, tragen dann Zeitstempel aus der
  Zukunft. Ein Tastenkürzel gibt es nicht.

## Bedienung des P8000

Der P8000 erklärt sich am Gerät nicht von selbst: es gibt kein Menü, nur ein Terminal mit einer
Eingabeaufforderung, und welche Eingabe man braucht, hängt davon ab, *wer* gerade antwortet — der
Monitor des 8-Bit-Teils, der Monitor des 16-Bit-Teils, UDOS oder WEGA. Dieses Kapitel führt durch die
Abläufe. Die Befehle stammen aus den Quellen der Monitore (`U880SM.S`, `p.init.s` …), der Einführung
in die Software (Anhang C und D), dem Installationsprotokoll von WEGA 3.1 und den Abnahmeläufen des
Emulators; was sich auf keine dieser Quellen stützen ließ, steht nicht hier oder ist als *nicht
geprüft* gekennzeichnet. Alle Eingaben werden mit RETURN abgeschlossen; Groß- und Kleinschreibung
gilt beim U880-Monitor gleich, beim U8000-Monitor nur Großbuchstaben (Handbuch Anhang D).

### Überblick: was ist was

* **8-Bit-Teil (U880)** — die erste Karte. Sie trägt den Monitor **MON8** („U880-Softwaremonitor"),
  den Hardwaretest, den Diskettenkontroller (zwei Laufwerke A: und B:, in UDOS 0 und 1) und die
  seriellen Kanäle **tty0, tty1, tty2, tty3**. Auf ihr läuft **UDOS** (Prompt `%`).
* **16-Bit-Teil (U8001)** — die zweite Karte mit Speicherverwaltung (MMU), Hauptspeicher und den
  Kanälen **tty4–tty7**. Sie trägt den Monitor **MON16** („U8000-Softwaremonitor", Prompt `*`). Auf ihr
  läuft **WEGA** (UNIX, Prompt `#`). Sie wird vom 8-Bit-Teil gestartet; **Diskettenzugriffe** von WEGA
  gehen nur, solange auf dem 8-Bit-Teil UDOS läuft.
* **Winchester (WDC)** — der Plattenkontroller (hier eine K5504.50 mit 1024 Zylindern, 5 Köpfen,
  18 Sektoren, 92 070 Blöcke zu 512 Byte). Ohne Platte startet WEGA nicht.
* **Terminal (tty1)** — der P8000 hat keinen eigenen Bildschirm. Seine **Konsole** ist tty1 mit 9600
  Baud. Der Emulator zeigt dafür das Originalterminal Typ 2 mit Flachtastatur K7673.09 (siehe die
  vorangehenden Abschnitte). Weitere Arbeitsplätze hängen an tty0, tty2 und tty4–tty7; **tty3** ist dem
  Drucker vorbehalten.

Wo die einzelnen Teile im Emulator eingestellt werden:

* *Einstellungen ▸ Allgemein*: **Modell** (Voreinstellung *P8000* oder *P8000 Compact*), **Rechnerausstattung** (*Vollgerät*, *ohne Winchester*, *nur
  8-Bit-Teil*), **Betriebsart** (*Computer mit Terminal* oder *nur Terminal*), **Hauptspeicher
  (16-Bit)**, ROM-Fassungen.
* *Ansicht ▸ Laufwerke*: Disketten A: und B: (Compact: auch C:) sowie die **Plattenkästen** (1–3, *Einstellungen ▸ Laufwerke ▸ Winchester-Laufwerke*) (*Anschließen…*, *Neue
  Platte…*, *Abtrennen*).
* *Datei ▸ Diskette einlegen* (oder *Mount* im Laufwerkskasten): Diskette einlegen.
* *Maschine ▸ Rückstellen*, *Maschine ▸ NMI-Taster*, *Maschine ▸ Verbindung zum Rechner…* (nur
  Betriebsart *nur Terminal*), *Maschine ▸ Bildschirminhalt als Text kopieren*.

### Einschalten und Selbsttest

Nach dem Einschalten (beim Programmstart oder mit *Maschine ▸ Rechner einschalten*) läuft zuerst das
Terminal an und meldet sich mit `ADM31/9600 baud/Video Attr. on`; erst danach spricht der Rechner. **Mit
dem Tippen warten, bis die Einschaltmeldung des Rechners da ist** — die ersten Zehntelsekunden
nach der Terminalmeldung nimmt die Tastatur nichts an, und ein zu früh getipptes Zeichen fehlt.

Ein erfolgreicher Start des 8-Bit-Teils sieht so aus (das Beispiel stammt vom echten Gerät; bei MON8
3.0 steht dort 3.0):

```
P8000 Hardwaretest U880 - Version 3.1

U880-Softwaremonitor Version 3.1 - Press RETURN
```

Der Hardwaretest prüft EPROM-Prüfsumme, statischen RAM, PIO, CTC, SIO, Diskettenkontroller, DMA und
dynamischen RAM. Der Speichertest braucht **rund 16 Sekunden Maschinenzeit** und steht lange auf einer
Ziffer (Schritt 30); bei der Vorgabe-Geschwindigkeit des Emulators (fünffach) dauert er entsprechend
kürzer. Dann erscheint „Press RETURN". **RETURN** führt zum Prompt **`>`** des U880-Monitors.

*Fehlermeldungen* des Hardwaretests haben die Form `*** ERROR nn  parameter …`; nach einem Fehler läuft
der Test in der Regel mit dem nächsten Schritt weiter. Die Nummern des 8-Bit-Teils laut Handbuch:
00 EPROM-Prüfsumme, 05–08 statischer RAM, 10–11 PIO, 15–17 CTC, 20–21 SIO, 25 Diskettenkontroller
(keine Bereitmeldung), 26–27 DMA, 30–33 dynamischer RAM.

Der **16-Bit-Hardwaretest** (`P8000 Hardwaretest U8001 - Version 3.1`) läuft, wenn der U8000-Monitor
gestartet wird, und schließt mit

```
MAXSEG=<0F>
*
```

ab. `MAXSEG` ist die Nummer des höchsten gefundenen 64-KB-Segments (hexadezimal) und damit der
Speicherausbau:

| Hauptspeicher (*Einstellungen ▸ Allgemein ▸ Hauptspeicher*) | MAXSEG |
|---|---|
| 2 × 256 KB = 512 KB | `07` |
| 4 × 256 KB = 1 MB (Standard) oder 1 × 1 MB | `0F` |
| 4 × 1 MB = 4 MB | `3F` |
| RAM-Karte 16 MB bestückt mit 2 MB / 4 MB | `1F` / `3F` |
| RAM-Karte 16 MB bestückt mit 8 MB oder 16 MB | `7E` (Segment 7FH fehlt: Hardware, kein Fehler) |

Die Fehlernummern des 16-Bit-Teils laut Handbuch (Auszug): 40 EPROM-Prüfsumme (fatal), 45–48 statischer
RAM, 50–51 PIO, 55–57 CTC, 60–61 SIO, 70 kein DRAM (fatal), 71 Segmentadresse, 72–75 dynamischer RAM,
76 keine fehlerfreien Segmente oberhalb Segment 0 (fatal), 80–97 MMU. Fatale Fehler brechen den Test ab.

**Typische Meldungen und was sie bedeuten**

* `*** ERROR 52 C1 …`, `*** ERROR 53 C1 …`, `*** ERROR 54 C1` im 16-Bit-Hardwaretest: der Test des
  **Winchesterkontrollers** schlägt fehl. Nach `p.test.s` ist 52 „Schreiben in den WDC-RAM fehlerhaft"
  (Parameter: Rückgabecode des WDC, Testwert `AAAA`/`5555`), 53 „Lesen bzw. Vergleich fehlerhaft"
  (zusätzlich der gelesene Wert) und 54 „falsche Kommandoauswertung" (der Rückgabecode war nicht `01`).
  Der Code `C1` bedeutet, dass der WDC nicht antwortet — im Emulator der Zustand ohne Winchester
  (*Rechnerausstattung ▸ ohne Winchester* bzw. *nur 8-Bit-Teil*); die Deutung deckt sich mit dem
  Mitschnitt eines Geräts ohne Winchesterbeisteller. Im Handbuch der Einführung stehen 52–54 nicht. Der
  Test läuft danach bis `MAXSEG=<..>` und `*` weiter. Kommt derselbe Fehler mit Rückgabecode `08` und
  `MAXSEG=<07>` (so im Mitschnitt vom Gerät des Anwenders), war an der Hardware etwas defekt; ein
  Emulatorlauf mit fehlerfreiem Ausbau zeigt keine Fehlerzeile. Im *Vollgerät* **ohne Platte** antwortet
  der WDC zwar, findet aber kein Laufwerk: `ERROR 52 C1` und `ERROR 53 27` (27 = kein Laufwerk bereit).
  Der WDC wartet beim Hochlauf bis zu 32 Sekunden Maschinenzeit auf Laufwerk 0 — so lange steht der
  Test nach „P8000 Hardwaretest U8001", bevor die Fehler, `MAXSEG` und `*` kommen. Mit unformatierter
  Platte heißt die Zeile `ERROR 52 39` (kein Parametersatz).
* Nach `MAXSEG=<..>` **kein `*`**, keine Fehlerzeile: der Test war fehlerfrei, und der Monitor startet die
  Platte (AUTOBOOT). Ist auf ihr kein WEGA, bleibt er stehen — *Rückstellen*, `x`, und bei „Press NMI"
  **RETURN** statt NMI (siehe „Der U8000-Monitor").
* `DISK ERROR` beim Start von der Platte (U8000-Monitor): die Platte ist nicht lesbar, nicht
  formatiert oder enthält keinen Urlader (Block 0). `HARD DISK ERROR 81` mit Blinkcode wurde am
  Gerät des Anwenders bei einer Platte mit Lesefehlern beobachtet (Bedeutung der Zahl *nicht geprüft*).
* `INSERT SYSTEMDISK` (MON8): die Diskette in A: ist keine Systemdiskette; `DISK ERROR` (MON8):
  Spur 0, Sektor 1 von Laufwerk 0 ist nicht lesbar — keine Diskette eingelegt oder falsches Format.
* `Hardware Error in Connection` (UDOS, beim Start der Koppelsoftware `WEGA`): es gibt keine
  16-Bit-Karte (*Rechnerausstattung ▸ nur 8-Bit-Teil*) oder die Kopplung ist defekt. Bei *nur 8-Bit-Teil*
  ist das **das Soll**, kein Fehler. `U8000-Port do not work` ist die zweite Meldung der Kopplungsprüfung.

**Wiederholen, Rückstellen, NMI**

* *Maschine ▸ Rückstellen* (Strg+Umschalt+R) ist die RESET-Taste des Geräts. Laut Handbuch führt sie —
  anders als das Einschalten — **nicht** erneut durch den Hardwaretest, sondern in den U880-Monitor
  („Press RETURN"). *Maschine ▸ Rechner ausschalten* und wieder *einschalten* (Strg+Umschalt+P) startet
  vollständig kalt mit Hardwaretest und lässt alle eingelegten Disketten neu einlegen.
* *Maschine ▸ NMI-Taster* ist die NMI-Taste. Solange der U8000 nicht läuft, wirkt sie nur auf den U880;
  läuft der U8000 (Monitor oder WEGA), wirkt sie **nur** auf den U8000. Direkt nach der Meldung
  „U8000-Softwaremonitor … **Press NMI**" startet sie den 16-Bit-Hardwaretest und — wenn er fehlerfrei
  besteht — WEGA von der Platte (AUTOBOOT; mit einer leeren Platte kommt danach kein `*`, siehe „Der
  U8000-Monitor"); sonst gibt der U8000-Monitor „NMI" aus und kehrt zur Eingabe zurück.
* Den Test des 8-Bit-Teils wiederholt der Monitorbefehl **`T`**, den des 16-Bit-Teils `T` am Prompt `*`.

### Der U880-Monitor (MON8)

Prompt **`>`**. Alle Zahlen **hexadezimal**, ohne Kennzeichen; ein Zahlenwert, der mit einem Buchstaben
beginnt (`A0`), darf so geschrieben werden. Eine fehlerhafte Eingabe quittiert der Monitor mit `?`.
Während einer Ausgabe hält **Strg+S** an, **Strg+Q** gibt wieder frei; Zeichen löschen: BS, Zeile
löschen: DEL (RUBOUT).

*Der Monitor liest nach einer Eingabe unter Umständen erst nach Millionen Takten wieder.* Die nächste Zeile
erst tippen, wenn das `>` (bzw. die Antwort) da ist. Ein **leeres RETURN** am `>` startet den Urlader
(bootet die Diskette in A:) und endet ohne Diskette mit `DISK ERROR` — also nur mit Absicht drücken.

| Befehl | Syntax | Wirkung und Beispiel |
|---|---|---|
| `D` | `D adr (anzahl)` | Speicher ansehen. Ohne Anzahl ein Byte, das mit einem neuen Wert überschrieben werden kann (Wert + RETURN schreibt und geht weiter, `_` geht zurück, `Q` beendet). `D 1000 20` zeigt 32 Byte ab 1000H. |
| `C` | `C adr1 adr2 anzahl` | zwei Bereiche vergleichen; Unterschiede werden ausgegeben. |
| `F` | `F anfang ende byte` | Bereich füllen. `F 1000 1FFF 00` |
| `M` | `M quelle ziel anzahl` | Bereich verschieben. |
| `PR` | `PR port` | Port lesen. `PR 24` |
| `PW` | `PW port byte` | Port schreiben. `PW 24 55` schreibt 55H an Port 24H (SIO0 Kanal A Daten = tty0). |
| `R` | `R (register)` | Register ansehen/ändern (`A B C D E F H L I A' … IX IY PC SP`); ohne Namen alle. |
| `B` | `B (adr)` | Haltepunkt (RAM-Adresse); ohne Adresse löscht den letzten. |
| `G` | `G (adr)` | Programm starten; ohne Adresse am aktuellen PC fortsetzen. `G 1000` |
| `N` | `N (anzahl)` | Einzelschritt (Standard 1, höchstens 256), nach jedem Befehl die Register. |
| `I` | `I` | Interruptstatus zeigen/ändern: `O1` erlaubt, `O0` gesperrt. |
| `GE` | `GE datei` | UDOS-Maschinencodedatei von Diskette in den RAM laden, ohne UDOS zu starten (Satzlänge höchstens 400H). |
| `S` | `S datei anfang ende (E=einsprung)(RL=satzlänge)` | Speicherbereich als UDOS-Datei auf die Diskette schreiben. Danach in UDOS **zuerst `I`** geben (Belegungsplan hat sich geändert). |
| `Q` | `Q` | zurück zu UDOS, wenn es vorher lief (nach `T` nicht möglich). |
| `T` | `T` | Hardwaretest des 8-Bit-Teils. |
| `O` | `O` | Betriebssystem von der Diskette in A: starten (UDOS, OS/M, IS/M); Fehler `DISK ERROR` oder `INSERT SYSTEMDISK`. |
| `X` | `X` | **U8000-Monitor starten** (nur mit 16-Bit-Karte); er meldet sich an tty1 mit „Press NMI". |

Die Befehle `D` bis `X` stehen im Quelltext `U880SM.S` in dieser Form; die übrigen Systemparameter
(Nullen nach LF, Prompt-Zeichen 3EH `>` …) sind veränderbare RAM-Zellen, hier nicht nötig.

Ein kleiner Test, ob Port-Ausgabe funktioniert (siehe auch den Mehrplatzbetrieb): `pw 25 05`, `pw 25 68`
schaltet tty0 auf 8 Bit mit Sender ein, danach schreibt `pw 24 54` ein „T" auf die Leitung tty0.

### Der U8000-Monitor (MON16)

Prompt **`*`**. Er wird mit **`x`** und RETURN vom U880-Monitor aus (oder von der WEGA-Startdiskette)
gestartet und meldet sich mit `U8000-Softwaremonitor Version 3.1 - Press NMI`. Jetzt gibt es zwei Wege:

* **NMI-Taste** (*Maschine ▸ NMI-Taster*): der 16-Bit-Hardwaretest läuft bis `MAXSEG=<..>`. **Meldet er
  einen Fehler** (z. B. `ERROR 52` ohne Platte oder mit unformatierter Platte), erscheint danach `*`.
  **Besteht er fehlerfrei** — Winchesterkontroller und Platte mit Parametersatz —, startet der Monitor
  den Urlader in Block 0 der Platte (AUTOBOOT, wie am Gerät). Mit installiertem WEGA ist das der
  Kaltstart von der Platte; ist die Platte formatiert, aber **leer** (z. B. die früher vom Programm
  angelegte `p8000_platte.img` oder *Formatiert mit Parametersatz*), läuft die CPU in die leeren Bytes,
  und **nach `MAXSEG` kommt kein `*`** mehr — nur *Rückstellen* hilft. Der Plattenkasten weist auf eine
  solche Platte hin.
* **RETURN** (oder eine andere Taste) statt NMI: der Monitor geht ohne 16-Bit-Hardwaretest und ohne
  AUTOBOOT sofort zum Prompt (`?`, dann `*`). Das ist der Weg zu `O U` / `boot` / `ud(0,0)sa.format`,
  solange auf der Platte noch kein WEGA steht. Den Hardwaretest holt `T` am `*` nach.

Alle Befehle mit **Großbuchstaben**, Zahlen hexadezimal und mit einer **Ziffer** beginnend (`0B`, nicht
`B`). Eine Adresse darf eine Segmentangabe in spitzen Klammern haben: `<2>1000`.

| Befehl | Syntax | Wirkung |
|---|---|---|
| `D` | `D adr (anzahl) (W/B/L)` | Speicher ansehen/ändern als Wort (Standard), Byte oder Langwort. |
| `C`, `F`, `M` | wie beim U880, Wortformat | vergleichen, füllen (gerade Anfangsadresse), verschieben. |
| `PR`, `PW` | `PR port (W)`, `PW port (W) werte` | Ein-/Ausgabeports. |
| `PRS`, `PWS` | wie `PR`/`PW` | **Special-I/O**, also die Register der Speicherverwaltung (MMU). |
| `R` | `R (register)` | Register `R0`…`R15`, `SG` (Segment), `PC`, `FC`, `RF`, `N4`, `N5`, `PS`, `P0`, ferner `H0`/`L0` …, `RR0` …. |
| `B` | `B (adr (zähler))` | Haltepunkt (nur auf gerader RAM-Adresse), optional erst beim n-ten Durchlauf. |
| `G`, `N` | wie beim U880 | starten bzw. Einzelschritt. |
| `HR`, `HW` | `HR block puffer (gerät)` | **Plattenblock** (512 Byte) lesen/schreiben, Gerät 0–3. Vorsicht: `HW` schreibt. |
| `GE`, `S` | `GE datei (segment)`, `S datei anf ende` | UDOS-Datei laden/sichern über das laufende UDOS des 8-Bit-Teils. |
| `Q`, `QRES` | | zum 8-Bit-Teil zurück; `QRES` setzt den 16-Bit-Teil dabei zurück. |
| `T` | `T` | 16-Bit-Hardwaretest. |
| `O` | `O D` / `O F` / `O U` | WEGA starten: `O D` von der **Platte** (Block 0), `O F` von **Diskette** (`boot0.flp`), `O U` bootet von der **UDOS-Diskette** (Meldung `BOOTING FROM UDOS FLOPPY`, danach Prompt `>` und `boot`, siehe unten). |

`RGE`, `RS`, `RQ` gelten nur an einem seriell angeschlossenen Fremdsystem statt des Terminals; ob der
Emulator diesen Weg zeigt, ist *nicht geprüft*. Weitere Einträge der Befehlstabelle in `p.init.s`
(`FF`, `FR`, `FW`, `LOAD`, `SEND`, `QUIT`) sind im Handbuch nicht beschrieben und hier nicht erklärt.

### UDOS booten (8-Bit-Teil)

1. **Systemdiskette in A: einlegen**: *Datei ▸ Diskette einlegen ▸ Laufwerk A:* (oder *Mount* im
   Laufwerkskasten). Für den P8000 ist die **WEGA-Startdiskette** eine UDOS-Systemdiskette. Eine
   `.img` fragt nach dem Format (das UDOS-Format der Startdiskette ist `k5601_16x256`: 80 Spuren,
   zweiseitig, 16 Sektoren zu 256 Byte je Seite), `.hfe`/`.dmk` nie.
2. **Kalt starten** (*Maschine ▸ Rechner ausschalten*, dann *einschalten*), sofern die Diskette erst
   jetzt eingelegt wurde — ein laufender Rechner sieht den Wechsel nicht.
3. Nach dem Hardwaretest: **RETURN** bei „Press RETURN", warten auf das `>`, dann **RETURN** (oder `O`)
   — der Urlader liest Spur 0 Sektor 1 (Kennung `P8000SYS`), danach laden OS und NDOS.
4. UDOS meldet sich mit dem Prompt **`%`**. Auf der WEGA-Startdiskette läuft vorher die
   Startdatei `OS.INIT` (u. a. `KINIT`, `WEGA`); ohne 16-Bit-Karte meldet die Koppelsoftware `Hardware
   Error in Connection` und kehrt zum Prompt `%` zurück.

Wichtige UDOS-Kommandos (Namen aus der Einführung in die Software; die Syntax im Einzelnen
*nicht geprüft*, `HELP`, wo vorhanden, erläutert sie):

| Kommando | Wirkung |
|---|---|
| `CAT` | Verzeichnis (UDOS kennt **kein `dir`**: Antwort `NONEXISTENT COMMAND`). Zeigt nur Nicht-Systemdateien. |
| `DATE` | Datum ein-/ausgeben |
| `STATUS` | Zustand der Diskette |
| `COPY`, `COPY.DISK` | Dateien bzw. ganze Disketten kopieren |
| `DELETE`, `RENAME`, `MOVE` | löschen, umbenennen, verschieben |
| `COMPARE`, `DUMP`, `EXTRACT` | vergleichen, Hexdump, Dateiangaben |
| `FORMAT` | Diskette formatieren — **fehlt auf der WEGA-Startdiskette**; nur von einer UDOS-Diskette, die es enthält |
| `DEBUG` | zurück in den U880-Monitor |
| `ERROR`, `ERRORS` | Fehlercode erläutern, Fehlerstatistik der Diskette |
| `SETFD` | Laufwerkskonfiguration (Format) setzen |
| `DO` | Kommandodatei ausführen |
| `EDIT`, `ASM`, `U8000ASM`, … | Editor, Assembler (Systemprogramme, soweit auf der Diskette) |

Namen von Dateien in anderen Laufwerken: `ud(0,0)name` ist die Datei `name` auf UDOS-Laufwerk 0 (A:).
**Eine leere Diskette formatieren:** *Leere Diskette* im Laufwerkskasten legt ein unformatiertes Medium
ein, das UDOS mit `FORMAT` formatieren muss. `FORMAT` steht nicht auf der WEGA-Startdiskette; ob und wie
es sich am P8000 mit der UDOS-2.2-Diskette bedienen lässt, ist *nicht geprüft*.

### WEGA: die Platte vorbereiten und installieren

Das gilt nur für das *Vollgerät*. Die Disketten der WEGA-Auslieferung (WEGA-Startdiskette,
`root1`–`root5`, `usr1`–`usr9`) gehören nicht zum Programm; sie müssen als `.img`/`.hfe` vorliegen. Die
Datenträger sind 9 × 512 Byte, zweiseitig, 80 Spuren (Formatwahl beim Einlegen: 80 × 2 × 9 × 512). Die
ausführliche Anleitung ist das Installationshandbuch von WEGA; hier die Schrittfolge, wie sie im
Installationsprotokoll von WEGA 3.1 steht und im Emulator geprüft ist.

1. **Platte anlegen.** Im Plattenkasten *Neue Platte…* (Typ K5504.50, Inhalt **Unformatiert**, die
   Vorgabe) und anschließen. Die Platte ist danach wie ein fabrikneues Laufwerk: **ohne Parametersatz**.
   Eine im Betrieb angeschlossene Platte erkennt der Rechner erst nach *Rückstellen* bzw. Aus- und
   Einschalten. Der Hardwaretest endet dann mit `ERROR 52` (der WDC meldet „Error in PAR&BTT"), der Monitor
   bleibt bedienbar: `O U`, am `>` `boot`, dann `ud(0,0)sa.format` — es meldet „PAR not ok"/„BTT not ok"
   und fragt den Plattentyp ab (Protokoll unten; im Emulator bis zu diesem Bild geprüft). Wer *Formatiert
   mit Parametersatz* wählt, bekommt eine Platte mit PAR und sonst nur E5; MON16 startet nach dem
   Hardwaretest genau diese E5-Bytes (AUTOBOOT) und landet in einer Eingabeschleife ohne `*` — Gastverhalten.
2. **Startdiskette in A:** (WEGA-Startdiskette), kalt starten, bei „Press RETURN" **RETURN**, am `>`
   noch einmal **RETURN**: UDOS startet, und die Koppelsoftware der Startdatei meldet
   „U8000-Softwaremonitor … Press NMI" (am Gerät geht auch **`x`** am `>`). Den **NMI-Taster** drücken;
   mit der unformatierten Platte endet der Test bei `ERROR 52 39`, `MAXSEG=<..>` und `*`. (Ist die Platte
   schon formatiert, aber noch ohne WEGA, statt NMI **RETURN** drücken — sonst bootet der Monitor die leere
   Platte und es kommt kein `*`.)
3. Am `*`: **`O U`** — Antwort `BOOTING FROM UDOS FLOPPY` und der Prompt `>`. Dann **`boot`**. Es folgt
   `Boot` und der Prompt **`:`** des Urladers. Von hier ruft man Programme der Startdiskette mit
   `ud(0,0)programm` auf.
4. **Formatieren:** `ud(0,0)sa.format`. Das Programm (bei WEGA 3.1: Version 4.1) fragt: Laufwerkstyp
   (Nummer `4` = ROB K5504.50), Parameter ok (`y`), manuelle Eingabe schlechter Spuren (`n`), Formatbeginn
   (`a`), Formatierung bestätigen (`y`), PAR und BTT zurückschreiben (`y`), Ende (`y`). Die Fragen sind
   die der Fassung 4.1 aus dem Protokoll; die WEGA-3.0-Startdiskette trägt die ältere Fassung 1.4 für
   Firmware 3.x. Sie passt nicht zur WDC-Firmware 4.2, wohl aber zur **3.4.05** (*Einstellungen ▸
   Allgemein ▸ WDC-Firmware*, experimentell): `sa.format` V1.4 meldet dann `WDC_V.3.4.05`, 1024 Zylinder, 5 Köpfe,
   18 Sektoren, 92070 Blöcke und fragt Bad-Track-Tabelle, Formatbereich (Zylinder und Kopf) und Bestätigung ab. Das Formatieren
   der ganzen Platte dauert rund **21 Minuten Maschinenzeit**; die Dauer in der Oberfläche habe ich *nicht
   gemessen*. `ud(0,0)sa.verify` prüft danach (fragt nach Anfangs- und Endzylinder, am Protokoll 0 und
   1023).
5. **Dateisysteme anlegen:** `ud(0,0)sa.mkfs` — Größe `13000`, Name `md(0,0)` (das spätere `/usr`);
   noch einmal `ud(0,0)sa.mkfs` — Größe `7000`, Name `md(0,16000)` (das Wurzeldateisystem).
6. **Wurzel einspielen:** `ud(0,0)sa.install`. Datum `MM/DD/YY`, Quelle `fd(1,0)`, Ziel `md(0,16000)`;
   **die Quelldiskette `root1` erst jetzt in B: einlegen** (liegt sie schon beim Laden von `sa.install`
   in B:, scheitert das Laden mit UDOS-Fehler `C4`), auf die Frage je Eintrag `A` (alle). Bei „next input
   disk ? (y/n)" die nächste Diskette (`root2` … `root5`) in B: einlegen, `y`; nach der letzten `n`.
7. Dasselbe für `/usr` mit Ziel `md(0,0)` und den Disketten `usr1` … `usr9`.
8. **Erster Start:** am `:` **`md(0,16000)wega`**. Der Kern meldet sich („WEGA Kernel -- Release …",
   Dateisystemtabelle), dann steht der Einbenutzerbetrieb (Prompt `#`).
9. **Einrichten:** `/etc/new.install` (Fragen `neu angelegt werden ? (j/n)` → `j`, Größen
   `4000` und `60732` bzw. RETURN für die Standardwerte), danach **`init 2`**. Datum `MM/DD/YY` und
   Uhrzeit `HH:MM` eingeben, nach den Dateisystemprüfungen und „Going multi-user in 30 seconds!" erscheint
   `login:`. Diese Schritte brauchen mehrere Minuten Maschinenzeit.

### WEGA starten, anmelden, beenden

* **Starten von der Platte (nach der Installation):** Rechner einschalten, **RETURN**, **RETURN** (UDOS mit
  der Koppelsoftware von der WEGA-Startdiskette in A:), bei „Press NMI" den **NMI-Taster**. Danach
  wird **nichts mehr getippt**: der Urlader von der Platte gibt `> boot` und `: md(0,16000)wega` selbst
  ein, die Dateisysteme werden geprüft, und nach rund vier Minuten Maschinenzeit steht `WEGA login:`.
  Von Hand geht es am Prompt `*` mit `O D` und am Urladeprompt `:` mit `md(0,16000)wega`.
* **Anmelden:** Name **`wega`**, Kennwort **`root`** (Auslieferung). Der Prompt ist `#`, das
  Willkommensbild folgt.
* **Erste Kommandos:** `date` (Datum), `who` (wer ist angemeldet), `ls /` und `ls -l /bin`
  (Verzeichnisse), `who am i` (eigener Kanal), `echo text`.
* **Mehrbenutzerbetrieb:** WEGA startet `getty` auf der Konsole, tty0, tty2 und tty4–tty7 (tty3 =
  Drucker). Ein zweiter Arbeitsplatz ist ein weiteres `p8000emu` in der Betriebsart *nur Terminal*; die
  Schritte stehen im Abschnitt *Das Originalterminal und der Mehrplatzbetrieb*. Am Arbeitsplatz holt
  **RETURN** die Anmeldezeile. `write wega tty4` schickt eine Nachricht, `exit` meldet ab.
* **Herunterfahren:** `halt` (meldet „System is coming down. Thirty seconds to forced log-off", wartet,
  beendet die Prozesse und endet im Einbenutzerbetrieb), dann **`sync;sync`**. Erst danach ausschalten
  (*Maschine ▸ Rechner ausschalten*) — sonst prüft der nächste Start das Dateisystem mit der Meldung
  „BOOT WEGA (NO SYNC!)". Auch ohne `halt` gilt vor jedem Abschalten: `sync;sync`.
* Die Platte schreibt der Emulator beim Abtrennen, bei einem Zwischenstand und beim Beenden von selbst
  zurück.

### Arbeiten mit Disketten unter WEGA

Dieser Abschnitt spielt den ganzen Weg durch: eine Diskette unter WEGA **formatieren und mit einem
Dateisystem beschreiben**, zwei weitere mit einem **Mehrdisketten-`tar`-Archiv** auf dem Raw-Gerät füllen, alle drei mit dem
**DiskTool** auslesen, auf einem PC (GNU `tar`) bearbeiten und wieder auf Disketten bringen. Der Ablauf ist im Emulator
durchgespielt und als Systemtest hinterlegt (`P8000WegaDiskette.FormatMountTarUndDiskTool`, Label
`wega_install`, `tools/dev.sh test-wega`); die Bildschirmauszüge unten stammen aus diesem Lauf. Die
Oberflächenschritte des DiskTool (GUI) sind beschrieben, aber nicht Schritt für Schritt abgenommen.

**Voraussetzungen.**

* Das Vollgerät mit **installiertem WEGA** läuft im Mehrbenutzerbetrieb (Abschnitt *WEGA starten, anmelden,
  beenden*), angemeldet als `wega` an der **Konsole**. Die Diskettenbefehle (`format`, die `fd`-Treiber) gehen
  in WEGA **nur an der Systemkonsole** und nur, wenn WEGA über die **WEGA-Startdiskette** gestartet wurde
  (nicht über das U880-Monitorkommando `X`) — sonst „unable to work with floppy".
* Die Startdiskette steckt in **A:** (`fd0`). Alle hier beschriebenen Disketten kommen nach **B:**, also `fd1`.
* Die Geräte: `/dev/fd1` ist das **Blockgerät** (für Dateisysteme, mit Puffer), `/dev/rfd1` das **Raw-Gerät**
  (für `tar`, `dd`; auch `/dev/tardev`). Die Namen tragen das Format: `fd1` = `fd196ds9` (80 Spuren,
  zweiseitig, 9 Sektoren zu 512 Byte = **720 KiB**, das Standardformat), daneben `fd148ds9`, `fd196ss9`,
  `fd148ss9` und `fd196ds16` (16 × 256 Byte). Diese Beschreibung bleibt beim Standardformat.

#### 1  Leere Diskette bereitstellen

Im Laufwerkskasten von **B:** auf *Leere Diskette* drücken und einen Namen mit der Endung `.hfe` (oder
`.dmk`) wählen, z. B. `wega_fs.hfe`. Das ist eine **echte Leerdiskette**: unformatiert, in der Geometrie des
Laufwerks — das Formatieren erledigt WEGA selbst. Sie ist danach in B: eingelegt (sonst *Datei ▸ Diskette
einlegen ▸ Laufwerk B:*). Ein `.img` kennt „unformatiert" nicht und taugt hier nicht.

#### 2  Diskette mit Dateisystem: formatieren, mounten, beschreiben

Ein Dateisystem entsteht in zwei Schritten: `format` schreibt die Spuren (Sektorköpfe), `mkfs` das
Dateisystem darauf. `1440` ist die Größe in Blöcken zu 512 Byte (= 720 KiB).

    #1 format fd1
    ready? y
    #2 /etc/mkfs /dev/fd1 1440
    isize = 472
    m/n = 1 72
    #3 mkdir /dsk
    #4 /etc/mount /dev/fd1 /dsk
    WARNING!! - mounting: <> as </dsk>

* `format fd1` fragt `ready? ` — mit **`y`** und RETURN quittieren. Es dauert rund 30 Sekunden Maschinenzeit
  (126 Mio. Takte bei 4 MHz), `mkfs` rund 5.
* Das Verzeichnis, auf das gemountet wird, muss **vorher da sein** (`mkdir /dsk`). Fehlt es, meldet `mount`
  „No such file or directory" und bindet nichts ein.
* Die Warnzeile `WARNING!! - mounting: <> as </dsk>` ist normal: das Dateisystem hat keinen Namen (`<>`).
* Ohne Argumente zeigt `/etc/mount` die Tabelle der eingebundenen Dateisysteme, `df` den freien Platz.

Jetzt wie auf jedem UNIX: eine Textdatei mit `echo`, ein Verzeichnis, eine Handbuchseite hineinkopieren.

    #5 echo "Hallo von WEGA" > /dsk/hallo.txt
    #6 mkdir /dsk/man
    #7 cp /usr/man/man1/tar.1 /dsk/man/tar.1
    #8 ls -lR /dsk
    total 2
    -rw-rw-rw- 1 wega     system       15 May 30 20:56 hallo.txt
    drwxrwxrwx 2 wega     system       48 May 30 20:56 man
    /dsk/man:
    total 12
    -rw-r--r-- 1 wega     system     5801 May 30 20:56 tar.1
    #9 cat /dsk/hallo.txt
    Hallo von WEGA

Abschluss: **aushängen, prüfen, Puffer schreiben**, erst dann auswerfen.

    #10 /etc/umount /dev/fd1
    #11 /etc/fsck /dev/fd1
    /dev/fd1
    ** Phase 1 - Check Blocks and Sizes
    ** Phase 2 - Check Pathnames
    ** Phase 3 - Check Connectivity
    ** Phase 4 - Check Reference Counts
    ** Phase 5 - Check Free List
    5 files 16 blocks 1365 free
    #12 sync

`fsck` meldet nichts Auffälliges (fünf i-nodes belegt, 16 Blöcke). Erst jetzt die Diskette auswerfen (*Datei ▸ Diskette auswerfen ▸ Laufwerk B:*
bzw. der Knopf im Laufwerkskasten) — der Emulator schreibt die Datei dabei zurück.

#### 3  Ein Archiv über mehrere Disketten: `tar` auf dem Raw-Gerät

Der Vorteil von `tar`-Disketten: ohne Dateisystem lässt sich **mehr archivieren, als auf eine Diskette passt**.
WEGA-`tar` schreibt von selbst über mehrere Disketten (Mehrdisketten-Archiv) und fragt am Ende einer Diskette
`insert next disk :`. Hier werden `/usr/man/man1`, `man2`, `man3` und `manM` archiviert: 451 Dateien, rund
1,2 MB Nutzdaten, **zwei volle Disketten**.

**Vorbereitung:** zwei neue *Leere Diskette* (`tar_a.hfe`, `tar_b.hfe`) und **beide vorab formatieren**
(`format fd1` wie in Schritt 2: einlegen, formatieren, auswerfen). Während `tar` auf die nächste Diskette
wartet, ist die Konsole belegt; formatiert werden kann dann nicht mehr. Danach `tar_a` in B: einlegen:

    #1 cd /usr/man; tar cvf /dev/rfd1 man1 man2 man3 manM
    a man1/...
    ...
    insert next disk :

Jetzt **`tar_b` einlegen** (*Datei ▸ Diskette einlegen ▸ Laufwerk B:* ersetzt die volle Diskette) und **RETURN**
drücken. `tar` schreibt weiter und endet:

    a manM/acct.M 7 blocks
    tar: allocated space on disk 1424 blocks with 512 bytes
    tar:   free    space on disk   16 blocks from 1440 blocks
    #2

Die beiden Platzzeilen betreffen die **letzte** Diskette. Danach `tar_b` auswerfen.

* Die Datei auf der Diskettengrenze wird **mitten im Dateiinhalt geteilt**: Diskette 2 beginnt nicht mit einem
  `tar`-Kopf. Einzeln sind die Disketten deshalb keine vollständigen Archive, nur zusammen.
* Das Schreiben dauerte rund drei Minuten Maschinenzeit.
* Pfade **relativ** angeben (`cd /usr/man; tar … man1`), damit das Archiv anderswo auspackbar ist.
* Lesen geht genauso: `tar xvf /dev/rfd1` (oder `tar tvf …`) verlangt am Ende der ersten Diskette
  `insert next disk :` — Diskette wechseln, RETURN.

#### 4  Mit dem DiskTool auslesen

Das DiskTool (Menü *Werkzeuge* des Emulators, oder `bash run_disktool.sh`; Kommandozeile `k1520disktool`, im
Projekt über `tools/dev.sh tool k1520disktool …`) erkennt die Disketten selbst:

    $ k1520disktool info wega_fs.hfe
    Format:      k5601_9x512
    Dateisystem: wega720
    Datentraeger: 2 Dateien, 7 KB belegt, 683 KB frei
    $ k1520disktool info tar_a.hfe
    Dateisystem: tar720

    $ k1520disktool ls -l wega_fs.hfe
    -rw-rw-rw-   1     0     0       15 1989-05-30 18:56 hallo.txt
    drwxrwxrwx   2     0     0       48 1989-05-30 18:56 man
    -rw-r--r--   1     0     0     5801 1989-05-30 18:56 man/tar.1
    $ k1520disktool fsck wega_fs.hfe --full
    ohne Befund

* Die Uhrzeiten liegen zwei Stunden vor denen, die WEGA zeigt (20:56 → 18:56); WEGA rechnet in der Zeitzone
  „MES", das DiskTool in UTC.
* `fsck` des DiskTool prüft, ohne zu schreiben.

**Dateien der Dateisystemdiskette holen:**

    $ k1520disktool get wega_fs.hfe --to auszug/fs          # hallo.txt, man/tar.1 (+ .fileinfo-Dateien)

**Das Mehrdisketten-Archiv** liest man als **tar-Satz**: die Disketten in der Reihenfolge des Schreibens
angeben. Das DiskTool verkettet ihre Datenströme, damit wird auch die geteilte Datei vollständig:

    $ k1520disktool tar-satz tar_a.hfe tar_b.hfe             # 451 Namen
    $ k1520disktool tar-satz tar_a.hfe tar_b.hfe --to lokal  # alles entpacken (+ .fileinfo-Dateien)

Für die Weiterarbeit mit dem Archiv als Datei (Linux-`tar`) holt `tar-export` den Strom **je Diskette**
unverändert heraus; jede Diskette ist 737 280 Byte lang:

    $ k1520disktool tar-export tar_a.hfe teil_a.tar
    $ k1520disktool tar-export tar_b.hfe teil_b.tar

**In der Oberfläche (GUI):** `Datei ▸ Abbild öffnen` (`Strg+O`) — oder die `.hfe` auf das Fenster ziehen. Im
Kopf steht das erkannte Dateisystem (`wega720` bzw. `tar720`); links die Dateien mit Modus, Besitzer und Zeit.
Dateien markieren und mit `→|` in den Ordner rechts holen. Ein Mehrdisketten-Archiv öffnet `Datei ▸ tar-Satz
öffnen…`, die `tar`-Datei einer einzelnen Diskette entsteht mit `Diskette ▸ Diskette als .tar exportieren…`.
Ausführlich: Handbuch des DiskTool, Abschnitt *tar-Archive herausholen, bearbeiten und zurückschreiben*.

#### 5  Auf dem PC entpacken — mit GNU tar

Auf dem PC wird **GNU tar** gebraucht: nur das kann mit `--format=v7` das historische **V7-Format** schreiben,
das WEGA-`tar` versteht (siehe Schritt 6). Unter Linux ist es vorinstalliert. Unter Windows bekommt man es mit
dem Paket **Git for Windows** (Git Bash); alle Befehle sind dort **identisch** zu denen unter Linux. Andere
`tar`-Programme (etwa das `tar.exe` von Windows) habe ich nicht geprüft.

Die Teilstücke werden **in der Reihenfolge der Disketten** hintereinandergelegt (`cat`) und wie ein Archiv
entpackt; `tar` liest sie als einen Strom:

    $ mkdir lokal
    $ cat teil_a.tar teil_b.tar | tar -xvf - -C lokal
    $ ls lokal
    man1  man2  man3  manM
    $ find lokal -type f | wc -l
    451

* Das Ergebnis ist dasselbe wie `tar-satz … --to lokal` (451 Dateien); nur die `.fileinfo`-Zusatzdateien kommen
  dort noch dazu.
* Die Nullauffüllung am Ende der letzten Diskette stört nicht, `tar` hört an der Endmarke auf.
* Als normaler Benutzer setzt `tar` die Besitzer nicht um (UID 0 der Diskette); mit `sudo` und
  `--numeric-owner` bleiben sie. Leere Verzeichnisse legt WEGA-`tar` nicht ins Archiv.

#### 6  Der Weg zurück

**a) Ein V7-Archiv für WEGA erzeugen.** Ändern Sie etwas im Ordner (hier eine neue Datei), dann **ein** Archiv
(das DiskTool teilt es in b) selbst an den Diskettengrenzen, 737 280 Byte je Diskette):

    $ echo "Neu von Linux" > lokal/man2/NEU.TXT
    $ cd lokal
    $ find . -type f | sed 's|^\./||' | sort | tar --format=v7 -cf ../alles.tar -T -
    $ cd ..
    $ ls -l alles.tar
    -rw-rw-r-- 1 olliy olliy 1474560 ... alles.tar

* `--format=v7` ist Pflicht: GNU- und ustar-Köpfe versteht WEGA nicht. Pfadnamen sind auf 99 Zeichen begrenzt.
* Nur **Dateien** ins Archiv (`find -type f`): die Verzeichniseinträge, die GNU-`tar` sonst mitschreibt,
  quittiert WEGA-`tar` beim Auspacken mit „cannot create".
* Nicht mit `tar -M -L` in Stücke zerlegen: `--format=v7` und `-M` vertragen sich nicht („GNU features wanted
  on incompatible archive format"), und mit einem GNU-Format schreibt `-M` Fortsetzungsköpfe, die WEGA nicht kennt.
  Das DiskTool (oder `split`) schneidet den einen V7-Strom nur an der Diskettengrenze, genau wie WEGA-`tar` es selbst täte.
* Hier gab es zwei Stücke (1 474 560 Byte, durch den Blockungsfaktor von GNU-`tar` auf 10 KiB aufgefüllt); wie
  viele es sind, hängt von der Archivgröße ab.

**b) Auf Disketten schreiben.** Das ganze Archiv kommt als *fertiger Container* in einen neuen tar-Satz; beim
Speichern verteilt ihn das DiskTool an den Diskettengrenzen auf nummerierte, ganz gleiche Abbilder (so, wie `split`
es täte). Das Zerschneiden mit `split` ist dafür nicht mehr nötig:

    $ k1520disktool tar-satz-neu --format k5601_9x512 teil.hfe --tar-container alles.tar
    Warnung: …  (nur wenn der erste Kopf nicht V7 ist; abgelegt wird trotzdem)
    alles.tar → Archiv (2 Abbild(er))
    Satz: 2 Abbild(er), 1474560 Byte Strom, 737280 Byte je Abbild
      teil1.hfe
      teil2.hfe
    gespeichert: 2 Abbild(er)

Passt das Archiv auf EINE Diskette, genügt `create teil.hfe --fs tar720_leer` und `tar-put teil.hfe alles.tar`
(die Datei wird das Archiv der leeren Diskette; die Diskette bleibt danach bearbeitbar). In der GUI:
`Datei ▸ Neue tar-Diskette / tar-Satz…` (Art *Satz*, Format 720K), die `.tar` auf die Diskettenansicht ziehen und
*Ist ein fertiger tar-Container* wählen, dann speichern.

**c) Datei auf der WEGA-Dateisystemdiskette austauschen.** Das DiskTool schreibt in die WEGA-Diskette, als
wäre es das Betriebssystem:

    $ k1520disktool rm  wega_fs.hfe hallo.txt
    geloescht: hallo.txt
    $ echo "Datei vom Linux-Rechner" > neu.txt
    $ k1520disktool put wega_fs.hfe neu.txt
    $ k1520disktool ls -l wega_fs.hfe
    drwxrwxrwx   2     0     0       48 1989-05-30 18:56 man
    -rw-r--r--   1     0     0     5801 1989-05-30 18:56 man/tar.1
    -rw-r--r--   1     0     0       24 ...             neu.txt
    $ k1520disktool fsck wega_fs.hfe --full
    ohne Befund

(In der GUI: erst den Schreibschutz aufheben (`Diskette ▸ Schreibschutz`, `Strg+R`), die Datei links markieren
und löschen, `neu.txt` rechts markieren und mit `|←` einfügen, dann `Datei ▸ Speichern` (`Strg+S`).)

**d) Zurück in WEGA.** Die Dateisystemdiskette zuerst in B: einlegen, **prüfen**, mounten, lesen. Die Prüfung ist
die Gegenprobe, dass das DiskTool ein Dateisystem geschrieben hat, das WEGA ohne Beanstandung liest:

    #1 /etc/fsck /dev/fd1
    ** Phase 1 - Check Blocks and Sizes
    ...
    5 files 16 blocks 1365 free
    #2 /etc/mount /dev/fd1 /dsk
    #3 ls /dsk
    man     neu.txt
    #4 cat /dsk/neu.txt
    Datei vom Linux-Rechner
    #5 /etc/umount /dev/fd1

(`mkdir /dsk` nur, wenn es das Verzeichnis noch nicht gibt.) Dann die Diskette auswerfen, `teil1.hfe` in B: einlegen
und das Mehrdisketten-Archiv in ein temporäres Verzeichnis auf der großen Platte `/z` entpacken (`/tmp` ist nur
2 MB groß). Am Ende der ersten Diskette fragt `tar` nach der nächsten — `teil2.hfe` einlegen, RETURN:

    #6 mkdir /z/tmp
    #7 cd /z/tmp; tar xvf /dev/rfd1
    x man1/...
    ...
    insert next disk :                       ← teil2.hfe einlegen, RETURN
    ...
    x manM/xq.M, 4619 bytes, 10 archiv blocks
    #8 find /z/tmp -type f -print | wc -l
    452
    #9 cat /z/tmp/man2/NEU.TXT
    Neu von Linux

`tar xvf` legt die Verzeichnisse selbst an. Zur Gegenprobe vergleicht `cat man1/* | sum` in `/z/tmp` und in
`/usr/man` — beide liefern dieselbe Prüfsumme (auch für `man3`), die Datei auf der Diskettengrenze ist also
unversehrt. Aufräumen später mit `rm -r /z/tmp`.

#### Stolpersteine

* **Immer erst `umount`, dann auswerfen** — und `sync`. Solange ein Dateisystem gemountet ist, liegen
  Änderungen noch im Puffer des Systems und fehlen auf der Diskette. `tar` auf dem Raw-Gerät schreibt dagegen
  sofort.
* **Disketten für ein Mehrdisketten-`tar` vorher formatieren** (siehe Schritt 3) und in der richtigen
  Reihenfolge einlegen. Eine Folgediskette allein ist kein Archiv.
* **Dieselbe Diskette nie gleichzeitig im DiskTool und im Emulator offen halten.** Der Emulator schreibt
  beim Auswerfen zurück; das DiskTool arbeitet auf der Datei.
* `format` löscht alles. Es gibt keine Rückfrage außer `ready? `.
* Die Konsole von WEGA ist die C-Shell: Umleitung von Fehlermeldungen heißt `>&` (nicht `2>&1`), Schleifen
  schreibt man mit `sh -c '…'`.
* `tar`-Disketten, die das DiskTool geschrieben hat, sind dort danach nur lesbar. Soll eine geändert werden:
  Archiv holen, auf dem PC ändern, neu teilen und auf neue Leerdisketten schreiben.

### Zweite Festplatte unter WEGA einrichten

Zur WEGA-3.1-Platte gehört eine **fertig vorbereitete zweite Platte**: `p8000_wega31_usr2.avr.img.gz` im Ordner
`p8000_wega31/` der Beispieldisketten, ein leeres Dateisystem **`/usr2`** von rund 127 MB (248 220 Blöcke zu 512 Byte). Die
mitgelieferte WEGA-3.1-Platte (`p8000_wega31_platte.avr.img.gz`) kennt sie schon: Kern, Gerätedateien und die
Einträge zum Einhängen sind eingerichtet. **WEGA auf der mitgelieferten 3.1-Platte ist also bereits für das Einhängen einer zweiten Platte vorbereitet** — wer nur diese beiden Abbilder benutzt, muss nichts generieren.

#### Die mitgelieferte zweite Platte anschließen

1. *Einstellungen ▸ Laufwerke ▸ Winchester-Laufwerke* auf **2** stellen (die Voreinstellungen P8000 und P8000
   Compact schließen von Haus aus nur **eine** Winchester an; auch das erneute Wählen einer Voreinstellung setzt die Zahl auf 1 zurück).
2. Im Kasten *Laufwerke* beim Plattenkasten **Laufwerk 1** auf **Anschließen…** und `p8000_wega31_usr2.avr.img.gz`
   wählen. Die Platte muss vom Typ **WEGA31-AVR** sein wie Laufwerk 0 (der Typ steht im Dateinamen, `.avr.`).
3. *Maschine ▸ Rückstellen* (oder aus- und einschalten): der WDC erkennt eine im Betrieb angeschlossene Platte erst
   beim Hochlauf. Startdiskette in A:, RETURN, RETURN, NMI wie gewohnt.

Die Auswahl **3** trägt den Zusatz „wird aktuell nicht von WEGA unterstützt“: der Emulator kennt drei Laufwerke, WEGA
kann aber nur zwei ansprechen.

Fertig. Beim Mehrbenutzerstart erscheinen die Meldungen (hier mit Platte):

    Checking second hard disk (md10) ...
    Second hard disk found - checking /usr2
    /dev/rmd10
    File System: /usr2 Volume: 1
    …

Nach der Anmeldung steht `/usr2` in `/etc/mount` (`/usr2 on /dev/md10 …`), und alles, was dort abgelegt wird,
liegt auf der zweiten Platte. Das Prüfen eines Dateisystems dieser Größe verlängert den Start um rund **5 Minuten**
(etwa 290 Sekunden Maschinenzeit; ohne zweite Platte entfällt es). Bei jedem Start mit Platte prüft `fsck` `/usr2` neu; zum Herunterfahren hängt
`/etc/umfs` sie vor `/usr` aus.

#### Ohne zweite Platte

Dieselbe Platte läuft auch mit **einem** Laufwerk, ohne Fehler beim Einhängen. Der Kern kennt `md10`, und ein
Zugriff darauf ohne angeschlossenes Laufwerk erzeugt eine Meldung des Kerns, die sich nicht abschalten lässt.
Deshalb fragt `/etc/rc_csh` beim Start **einmal** nach und rahmt die Kernzeilen ein:

    Checking second hard disk (md10) ...
    err on dev 0/10
    bn=0 er=FF18,3
    md: fatal error
    Second hard disk not present - /usr2 not mounted.

Die drei Zeilen dazwischen sind **der Kern, nicht ein Fehler des Systems**: Der Zugriff auf Laufwerk 1 ging ins
Leere, `/usr2` bleibt ein leeres Verzeichnis auf der Wurzel, und nichts wird später eingehängt. Das Ergebnis der
Abfrage steht in der Marke `/etc/md10.da`: nur wenn sie da ist, hängen `/etc/mfs` und `/etc/umfs` `md10`
ein bzw. aus.

#### Eigene Platte oder anderes Abbild: der lange Weg

Das Folgende ist der Weg, den die mitgelieferten Abbilder schon hinter sich haben; man braucht ihn für eine **eigene
zweite Platte** (andere Größe, anderer Inhalt), für eine **andere WEGA-Installation** oder um zu verstehen, was
im Abbild steht. Der Winchesterkontroller kann mehrere Laufwerke bedienen, WEGA benutzt aber nur, was im **Kern**
steht: die Aufteilung der Platten ist beim Generieren des Kerns fest eingetragen (`/usr/sys/h/mdsize.h`). Der
Kern der **ursprünglichen** WEGA-3.1-Platte kannte für Laufwerk 1 nur Bereiche der Größe **0** — ohne neuen Kern
geht es also nicht. Der Weg folgt dem WEGA-Systemhandbuch (Abschnitt 4.4 „Einbinden eines zweiten
Hard-Disk-Laufwerks" und 5.5 „Generierung des Kerns"). Er ist im Emulator durchgespielt; derselbe Ablauf erzeugt
die mitgelieferten Abbilder (`tools/p8000/zweite_platte_bereitstellen.sh`, Systemtest
`P8000WegaZweitePlatte.Bereitstellen`), und die Wächter `OhneZweitePlatteLaeuftUnauffaellig` und
`MitAusgelieferterZweiterPlatteEingebunden` (Label `wega_install`, `tools/dev.sh test-wega`) prüfen das Ergebnis.
Die Befehle und Bildschirmauszüge unten stammen aus diesem Lauf. Auf der mitgelieferten Platte sind die Schritte 2
bis 4 bereits getan; wer sie dort wiederholt, tut es auf eigene Verantwortung (`mknod` und die `ed`-Einträge
würden doppelt).

**Ein `/etc/fstab` gibt es in WEGA nicht.** An seiner Stelle stehen drei Dateien: die Shell-Prozedur
**`/etc/mfs`** hängt die Dateisysteme ein (`/etc/rc_csh` ruft sie beim Übergang in den Mehrbenutzerbetrieb auf;
`/etc/umfs` ist dieselbe Datei unter zweitem Namen und hängt aus), **`/etc/rc_csh`** prüft vorher jedes
Dateisystem mit `fsck`, und **`/etc/checklist`** ist die Liste, die `fsck` ohne Argumente prüft.
`/etc/mnttab` ist nur die Tabelle der gerade eingehängten Dateisysteme (beim Start geleert).

**Voraussetzungen.**

* Vollgerät mit WDC-Firmware **4.2** und installiertem WEGA auf **Laufwerk 0**.
* *Einstellungen ▸ Laufwerke ▸ Winchester-Laufwerke* auf **2** (die Voreinstellungen P8000 und P8000 Compact
  bringen 1 Laufwerk mit).
* **Die zweite Platte muss vom selben Typ sein wie die erste.** Die Firmware 4.2 übernimmt beim Hochlauf den
  Parametersatz von Laufwerk 0 und verlangt für Laufwerk 1 und 2 genau denselben; ein anderer Typ endet im
  Hardwaretest mit `*** ERROR 52 3A` (Init-Fehler 38 + Laufwerk 1) und ist für WEGA nicht benutzbar. Zur
  ausgelieferten WEGA-3.1-Platte gehört der Typ **WEGA31-AVR** (1380 Zylinder, 10 Köpfe), zu einer selbst
  installierten Platte meist **K5504.50**.

#### Eigene Platte, Schritt 1: anlegen und formatieren

Im Plattenkasten *Laufwerk 1* **Neue Platte…**: Typ wie Laufwerk 0, Inhalt *Unformatiert* (Vorgabe). Danach
*Maschine ▸ Rückstellen* oder aus- und einschalten — eine im Betrieb angeschlossene Platte erkennt der WDC erst
beim Hochlauf. Startdiskette in A:, **RETURN**, **RETURN**, bei „Press NMI" den **NMI-Taster**. Weil Laufwerk 1
noch keinen Parametersatz hat, startet WEGA nicht von selbst; der Hardwaretest endet mit

    *** ERROR 52   3A  AAAA
    MAXSEG=<0F>
    *

Am `*` **`O U`**, am `>` **`boot`**, am `:` das Formatierprogramm von der Platte:

    : md(0,16000)sa.format
    >>>  Format Hard-Disk 4.1  <<<
    Firmwareversion 'WDC_4.2'
    Number of Drives: 2  (Drive 0 1)
    Error on RESET
    Error in PAR&BTT on Drive 1 (PAR not ok) (BTT not ok)
    No. of Drive formatted: 1
    PAR --- Drivetype: 'WDC-Emulator'
    PAR --- Cylinders: 1380  Heads: 10  Sectors: 18  Praecomp: 1380  Ramp: 1
    Parameter for Drive ok ? (y/l/p/q)y
      No Entries in Bad Track Table (BTT) of Drive 1
    Manual Input of bad Track of Drive 1 (y/n/q) ? n
      Format Begin: Cylinder (a/Start-Cylinder) a
    Ready for Format Drive 1 from Cyl 0 Hd 0
                               to Cyl 1379 Hd 9 ? (y/n/q) y
    …
    Rewrite PAR&BTT from WDC-RAM to HD-Drive 1 ? (y/n)y
    End of 'sa.format' (y/n) ? y

* **Laufwerksnummer `1`** — die Frage kommt nur, wenn der WDC zwei Laufwerke meldet.
* `sa.format` schlägt den Parametersatz **von Laufwerk 0** vor; genau der ist richtig (**`y`**). Mit `l` käme die
  Typliste — ein anderer Typ als der von Laufwerk 0 wäre aber der Fehler von oben.
* Das Formatieren der ganzen Platte (13 800 Spuren) dauert rund 13 Milliarden Takte, also knapp eine Stunde
  Maschinenzeit; im Emulator mit voller Geschwindigkeit wenige Minuten.

Danach am `:` **`md(0,16000)wega`**: WEGA startet mit dem alten Kern im Einbenutzerbetrieb (`#1`); **`init 2`**,
Datum und Zeit beantworten, als `wega` anmelden.

#### Eigene Platte, Schritt 2: neuen Kern generieren (`sysgen`)

    #1 cd /usr/sys/conf
    #2 sysgen

`sysgen` zeigt die bisherige Aufteilung (`file system layout`) und fragt alle Bereiche von Laufwerk 0 **neu**
ab. **Hier die angezeigten Werte unverändert eingeben** — wer sie ändert, verlegt `/`, `/usr` oder `/z`, und
WEGA findet seine Dateisysteme nicht mehr. Für die ausgelieferte WEGA-3.1-Platte:

| Frage | Antwort |
|---|---|
| `Do you want to change system constants ? :` | `n` |
| `Do you want to change file system layout ? :` | `y` |
| `size of /usr :`, `/swap`, `/root`, `/tmp`, `/z` | `13000`, `3000`, `7000`, `4000`, `221220` |
| `size of /md5 :` | `0` |
| `Do you want to add an second harddisk ? :` | `y` |
| `size of /md10 :` | `248220` (ganze Platte: 1379 Zylinder × 10 Köpfe × 18 Sektoren) |
| `size of /md11 :` | `0` |
| `Do you want to change version parameters ? :` | `n` |
| `Do you want to add new driver to the system ? :` | `n` |

Für eine K5504.50 ist die ganze Platte `92070` Blöcke (1023 × 5 × 18). Wer Laufwerk 1 teilen will, gibt
`md11`, `md12` … Größen; die Bereiche liegen der Reihe nach ab Block 0. `sysgen` übersetzt und bindet:

    New wega is building now ! Wait a minute !
    …
    New wega : /usr/sys/conf/wega
    New boot is building now !
    …
    New boot : /usr/sys/conf/boot

Den neuen Kern einsetzen, den alten aufheben (das neue `boot` wird nicht gebraucht, `/` liegt unverändert):

    #3 cp /wega /wega.old
    #4 cp /usr/sys/conf/wega /wega
    #5 chmod 400 /wega
    #6 sync

Dann **`sync`** noch einmal und kalt starten (*Rückstellen* bzw. aus/ein, RETURN, RETURN, NMI). Jetzt haben
beide Platten einen Parametersatz, WEGA startet wieder von selbst — mit dem neuen Kern. Startet er nicht, am
`*` `O U`, `boot` und am `:` `md(0,16000)wega.old` (*nicht im Emulator geprüft*).

#### Eigene Platte, Schritt 3: Gerätedateien, Dateisystem, einhängen

Als `wega` angemeldet (die Zahlen vor den Befehlen sind der Prompt):

    #1 /etc/mknod /dev/md10 b 0 10
    #2 /etc/mknod /dev/rmd10 c 0 10
    #3 /etc/chmog 640 bin 0 /dev/md10 /dev/rmd10
    #4 mkdir /usr2
    #5 /etc/mkfs /dev/rmd10 248220
    isize = 65528
    m/n = 1 72
    #6 /etc/labelit /dev/rmd10 /usr2 1
    #7 /etc/mount /dev/md10 /usr2
    #8 /etc/mlf /usr2
    #9 /etc/chmog 644 bin 0 /usr2/lost+found

* Die Gerätenummern: Blockgerät `b 0 n`, Raw-Gerät `c 0 n`; **Laufwerk 1 hat die Bereiche `md10`–`md19`**
  (Laufwerk 0: `md0`–`md9`). Ein zweiter Bereich hieße `md11`/`rmd11`.
* `mkfs` nimmt die Größe aus `sysgen` (`/md10`). `labelit` gibt dem Dateisystem den Namen, den `fsck` beim
  Start anzeigt. `/usr2` ist ein frei gewählter Name.

#### Eigene Platte, Schritt 4: dauerhaft einbinden

Zwei Dateien ändern, und zwar **bedingt**: ein Kern mit `md10` verträgt ein fehlendes Laufwerk 1 nur, wenn
niemand darauf zugreift (jeder Zugriff erzeugt den Kernblock `md: fatal error`). Mit `vi` oder, wie im Test, mit
`ed` (die Zeilen nach `a` werden mit **Tab** eingerückt, `.` allein beendet die Eingabe, `w` schreibt, `q`
verlässt):

    #10 ed /etc/mfs
    /etc\/mount \/dev\/usr/a
    			test -f /etc/md10.da && /etc/mount /dev/md10 /usr2
    .
    /etc\/umount \/dev\/tmp/a
    			test -f /etc/md10.da && /etc/umount /dev/md10
    .
    w
    q
    #11 ed /etc/rc_csh
    /dev\/rz/a
    	echo "Checking second hard disk (md10) ..."
    	rm -f /etc/md10.da
    	dd if=/dev/rmd10 of=/dev/null count=1 >& /dev/null
    	if ( $status == 0 ) then
    		echo "Second hard disk found - checking /usr2"
    		touch /etc/md10.da
    		/etc/fsck -y -t /tmp/scratch /dev/rmd10
    	else
    		echo "Second hard disk not present - /usr2 not mounted."
    	endif
    .
    w
    q

* `/etc/mfs` und `/etc/umfs` sind **eine** Datei (zwei Namen); der erste Eintrag hängt beim Start ein (nach
  `/dev/usr`), der zweite beim Herunterfahren aus — **vor** `/dev/usr`, hinter `/dev/tmp`: zuletzt eingehängt,
  zuerst ausgehängt.
* In `/etc/rc_csh` gehört die Prüfung hinter die Zeile `… /dev/rz`: nur dort ist `/tmp` eingehängt, und für
  Dateisysteme über 30 000 Blöcke verlangt das Systemhandbuch `fsck` mit `-t` und einer Hilfsdatei. WEGAs
  Shell ist die C-Shell, `rc_csh` also csh-Syntax (`>&`, `if ( … ) then`); `mfs` ist dagegen eine
  Bourne-Shell-Prozedur.
* **`/etc/checklist` bleibt unverändert.** Sie ist die Liste für `fsck` ohne Argumente (interaktiv); ein Eintrag
  `/dev/rmd10` liefe ohne Platte in einen Lesefehler, und `rc_csh` prüft die Platte ohnehin selbst.
* Wer **ausschließlich mit zwei Platten** arbeitet, kann die Abfrage weglassen und die drei Zeilen
  unbedingt eintragen (`/etc/mount /dev/md10 /usr2`, `/etc/umount /dev/md10`,
  `/etc/fsck -y -t /tmp/scratch /dev/rmd10`); fehlt dann die Platte, meldet der Start Lesefehler.

Probe und Daten:

    #15 cp /etc/passwd /usr2/passwd.kopie
    #16 echo zweite Platte LW1 > /usr2/notiz
    #17 sum /etc/passwd /usr2/passwd.kopie
    29362 1 /etc/passwd
    29362 1 /usr2/passwd.kopie
    #18 df; /etc/mount
    …
    /usr2   (/dev/md10 ):   240021 blocks   65507 i-nodes
    …
    /usr2 on /dev/md10 read/write on Sun Oct 18 12:05:12 2026

Zum Schluss **`sync`** (zweimal) und neu starten.

#### Eigene Platte, Schritt 5: nach dem Neustart

Die Mehrbenutzer-Prüfung zeigt die neue Platte:

    Checking second hard disk (md10) ...
    Second hard disk found - checking /usr2
    /dev/rmd10
    File System: /usr2 Volume: 1
    5 files 8 blocks 240021 free

und nach der Anmeldung steht `/usr2` in der Tabelle — eingehängt beim Start, nicht von Hand:

    #1 /etc/mount; cat /usr2/notiz; ls /usr2
    …
    /usr2 on /dev/md10 read/write on Sun Oct 18 12:00:03 2026
    zweite Platte LW1
    lost+found   notiz        passwd.kopie

#### Was nicht geht oder nicht geprüft ist

* **Zwei verschiedene Plattentypen** an einem WDC 4.2 — die Firmware lehnt Laufwerk 1 ab (`ERROR 52 3A`).
* **Ein drittes Laufwerk unter WEGA:** Emulator und WDC kennen drei, aber `sa.format` 4.1 bricht bei mehr als
  zwei Laufwerken ab, und die Kerntabelle (`wpar.c`) hat nur `md0`–`md19`, also zwei Laufwerke.
* **Ohne `sysgen`**: der Kern der ursprünglichen WEGA-3.1-Platte führt `md10`–`md19` mit Größe 0 und weist jeden
  Zugriff ab (aus dem Kern gelesen, im Emulator nicht ausprobiert). Die mitgelieferte Platte hat den neuen Kern
  schon (`/wega`; der alte liegt als `/wega.old` daneben).
* **Nicht geprüft:** WEGA 3.0, ein Paar K5504.50-Platten, mehrere Bereiche auf Laufwerk 1, Starten von
  Laufwerk 1.

### Terminal und Tastatur

Das Terminal ist ein Terminal **ADM31** (VT100 wählbar), 80 × 24, 9600 Baud. Beim Einschalten gilt ADM31;
WEGA führt alle Kanäle als ADM31 (`/etc/ttytype`: `P8`). Die Tasten des Wirtsrechners gehen über die
Tastenmatrix der K7673.09 an das Terminal (Näheres im Abschnitt *Das Originalterminal*). Die für die
Bedienung des P8000 wichtigen:

| Taste (Wirtsrechner) | Taste am P8000-Terminal | Wirkung |
|---|---|---|
| Rücktaste | BS | letztes Zeichen löschen |
| Entf | DEL | im Monitor: Eingabezeile löschen (RUBOUT, 7FH) |
| Esc | ESC | Escape (Folgen für Vollbildprogramme) |
| Pfeiltasten | Cursor | Cursorsteuerung (ADM31: FF, BS, VT, LF) |
| F2 / F3 / F4 | PAGE ERASE / LINE INSERT / CHAR INSERT | Bild löschen, Zeile einfügen, Zeichen einfügen |
| F5 / F6 | LINE DELETE / CHAR DELETE | Zeile bzw. Zeichen entfernen |
| F7 oder Pause | BREAK | sendet eine Unterbrechung (Break); der Anmeldeprozess `getty` schaltet daraufhin zur nächsten Baudrate |
| F8 | SI/SO | Zeichensatz 1 ↔ 2 (deutsche Umlaute `ä ö ü ß` liegen im Satz 2) |
| F9 | MODE | ADM31 ↔ VT100; jede Umschaltung initialisiert das Terminal neu |
| F10 | VIDEO | Videoattribute (Blinken, invers) ein/aus |
| F12 | ON/OFF | On-line/Off-line: im Off-line-Zustand erscheinen Tastenanschläge nur lokal |
| Strg + Buchstabe | CTRL | Steuerzeichen; **Strg+S** hält die Ausgabe an, **Strg+Q** gibt sie frei |
| Umschalt, Feststell | SHIFT, CAPS LOCK | wie üblich |

Die Bildschirmtastatur (*Ansicht ▸ Tastatur*) zeigt dieselben Tasten und rastet Umschalt und Strg beim Klick
ein. Die Zuordnung der F-Tasten F1 und F11 sowie von CE zur Matrix beruht auf einer Annahme (siehe dort).
**Text kopieren:** Rechtsklick auf das Bild oder *Maschine ▸ Bildschirminhalt als Text kopieren*; ein
Tastenkürzel gibt es nicht, weil Strg+C dem Gast gehört.

Bei hoher Zeichenmenge (z. B. bei vielen Bildlöschungen hintereinander) bremst WEGA über XON/XOFF; das ist
die Firmware des Terminals und kein Fehler.

### Fehlersuche

* **Der Bildschirm bleibt leer.** Zuerst die Betriebsart: *nur Terminal* ohne Verbindung zum Rechner zeigt
  nur die Einschaltmeldung des Terminals. Mit *Maschine ▸ Verbindung zum Rechner…* verbinden (oder
  *Computer mit Terminal* wählen). Nach dem Einschalten dauert es einen Moment, bis die Meldung erscheint.
* **Rechner nicht verbunden** (Statuszeile „Rechner: …"): Gegenstelle ist kein Server oder die Adresse
  stimmt nicht; ein Client versucht es selbst weiter. Der Server (Rechner) lauscht erst nach *Starten*
  im Reiter *Schnittstellen*.
* **Tasten gehen am Anfang verloren.** Erst nach der Einschaltmeldung des Rechners tippen; am Original
  kostet jede Taste außerdem Zeit (die Eingabe kommt nach und nach an). Nach einer Eingabe am Monitor
  erst weitertippen, wenn das Echo/der Prompt da ist.
* **Nach dem Hardwaretest kein Prompt `*`, nur ein stehender Bildschirm** (Vollgerät mit leerer Platte):
  der 16-Bit-Monitor versucht, von der Platte zu starten (AUTOBOOT) und läuft in eine Eingabeschleife.
  Ursache: eine Platte *mit Parametersatz, aber leer* — der Monitor startet die E5-Bytes. Das ist
  Gastverhalten; Platte im Plattenkasten *Abtrennen*, *Rückstellen*, dann *Neue Platte…* mit Inhalt
  **Unformatiert** anlegen (siehe auch Schritt 1 der Installation).
* **`ERROR 52/53/54`:** der WDC antwortet nicht (siehe oben); **`DISK ERROR`**: Platte nicht lesbar bzw.
  nicht installiert; **`md: io error`** (Urlader): die Platte ist nicht formatiert oder nicht angeschlossen.
* **„Firmware 3.4.05 gehört zu K5504.50 …, die Datei hat … B“ beim Anschließen.** Die älteren WDC-Fassungen
  (4.0.05, 3.4.05) kennen nur dieses eine Laufwerk; eine Platte anderer Größe (D5126, D5146, VS) braucht die
  Firmware 4.2. Umgekehrt meldet die 3.4.05 beim Hochlauf (Hardwaretest `ERROR 52`, Rückgabe 06) „Bad-Track-Tabelle fehlt“ auf einer
  neuen Platte — `sa.format` V1.4 legt sie an; Fehler 05 hieße „Tabelle nicht lesbar“ (Spurlage der 4.2 statt der 3.x).
* **Die neu angeschlossene Platte wird nicht gefunden.** *Rückstellen* — der WDC erkennt die Platte beim
  Hochlauf.
* **`sa.install` meldet UDOS-Fehler `C4` beim Laden.** Die Quelldiskette zu früh in B: eingelegt (siehe
  Schritt 6).
* **`login:` erscheint am Arbeitsplatz nicht.** Am Arbeitsplatz RETURN drücken; `getty` hat die erste
  Anmeldezeile schon gesendet, bevor die Verbindung stand.
* **Die Datei- oder Plattenabbilder sind „von einem anderen Prozess belegt".** Ein Abbild gehört einer
  Instanz; zwei Programme dürfen dieselbe Datei nicht gleichzeitig anschließen.

## Tastenkürzel

| Kürzel | Wirkung |
|--------|---------|
| F11 | Vollbild ein/aus (Esc verlässt es ebenfalls) |
| Strg+Umschalt+O | Konfiguration laden |
| Strg+Umschalt+S | Konfiguration speichern |
| Strg+Umschalt+P | Rechner ein- oder ausschalten |
| Strg+Umschalt+R | Rückstellen (Neustart vom Boot-ROM) |
| Strg+Umschalt+H | Dieses Handbuch |
| Strg+Umschalt+Q | Beenden |

Alle übrigen Tasten gehören dem emulierten Rechner.

## Wenn etwas nicht geht

**Der Bildschirm bleibt nach dem Einschalten dunkel oder zeigt nur das
Boot-ROM.** Es liegt keine bootfähige Diskette in A:. Einlegen, dann
*Rückstellen*.

**Die neu eingelegte Diskette wird nicht gefunden.** Ein laufendes
Betriebssystem sieht den Wechsel nicht von selbst — *Rückstellen* (oder beim
Gastsystem einen Warmstart auslösen).

**Ein `.img` liest sich als Müll.** Das rohe Sektorabbild trägt seine Geometrie
nicht — im Formatdialog beim Einlegen wurde das falsche gewählt. Auswerfen, neu
einlegen, ein anderes Format nehmen. `.hfe` und `.dmk` haben dieses Problem
nicht.

**Der Tooltip am Takt meldet, dass der Wirtsrechner nicht mitkommt.** Meist, weil die
Bildröhre mit allen Effekten auf einer großen Fläche gezeichnet wird. Fenster
kleiner machen oder unter *CRT* die aufwendigen Regler zurücknehmen.

**Die Tastatur tippt ins Leere.** Der Fokus liegt nicht auf der Röhre: einmal
hineinklicken. (Normalerweise holt das Fenster ihn von selbst zurück.)

**Der K8915 bleibt nach dem Selbsttest mit einem Buchstaben stehen.** Der
Buchstabe hinter dem Testnamen ist die Fehlerkennung des Boot-ROMs; RETURN
lädt trotzdem.

**Gar nichts startet.** `a5120emu --paths` (bzw. `k8915emu --paths`) sagt, wo das Programm die
Kernbibliothek und den Formatkatalog sucht — das ist die erste Frage, wenn
etwas nicht gefunden wird.

## Bugs, Wünsche und Anregungen

Fehler, Wünsche und Anregungen bitte über das Ticketsystem des Projekts auf GitHub
melden: <https://github.com/olliy78/k1520emu/issues>. Ein neues Ticket legt man
dort mit *New issue* an; dafür braucht es ein (kostenloses) GitHub-Konto. Bevor Sie
eins schreiben, lohnt ein Blick, ob dasselbe schon gemeldet ist.

Hilfreich in einer Fehlermeldung sind:

* die **Version** (*Hilfe ▸ Über …* bzw. oben in diesem Handbuch),
* das **Betriebssystem** des Rechners, auf dem der Emulator läuft,
* **was Sie getan haben**, was Sie erwartet haben und was stattdessen geschah,
* bei Problemen mit einer Diskette nach Möglichkeit das **Abbild** selbst
  (als Anhang am Ticket, falls seine Weitergabe erlaubt ist),
* bei „wird nicht gefunden“ die Ausgabe von `--paths` (siehe oben).

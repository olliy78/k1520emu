# K5: Kernel aus Modulen neu binden (2026-10-28)

`tools/mutos8000/kernel/linkbau.py` erzeugt aus `mutos_1216` mit `z8kdis` je Modul eine Assemblerquelle (`s/*.s`, eingecheckt), uebersetzt
sie im MUTOS-Gast mit `as -u -o`, bindet mit `ld -s -o` in Linkreihenfolge und vergleicht.  Linkkarte: `linkkarte.txt`
(`modul NAME TEXT-VON TEXT-BIS DATEN-VON DATEN-BIS`, Ende ausschliesslich; `bss`, `konstwert`, `adr`, `dwp`).

## Gemessen (Gast-`as`/`ld`)
* **`as -u`** (so ruft `cc` ihn): undefinierte Namen sind extern.  Ohne `-u` meldet er jede Deklaration `X name PR` als `undefined symbol`.
* `ld -s -o /tmp/lk m1.o m2.o ...`: Kopf 16 B (Flag 1) + Text + Daten, Text ab 0, alle Texte in Modulreihenfolge, dann alle Daten; Daten
  ohne Luecken (jede Objekt-Datensektion wird auf gerade Laenge gefuellt, daher muessen Datenschnitte gerade sein).
  Eine GLOBAL-Marke im Unterprogramm (`G` / `name L` vor dem Unterprogramm, `name:` darin) wird zum Textsymbol und von fremden Modulen aufgeloest.
* **Gemeinsame Variablen** (`X name A[%n B]` = undefiniert mit Wert n) teilt `ld` in Hash-Reihenfolge zu, nicht in Deklarationsfolge.  Die BSS-Variablen
  des Kernels (8DC0..D532) liegen deshalb als nullgefuellte GLOBAL-Daten in der Adressfolge im letzten Modul `bss`; der Kernel ist Text+Daten (bis 8DC0),
  die Nullen dahinter werden abgeschnitten (das Original ist auf 512 B aufgerundet: 36 352 B).
* Modulnamen duerfen keine Schluesselwoerter sein (`pe`, `po` ...: `syntax error`); z8kdis schreibt `<name> module`, Unterprogramm `_p_<name>`.
* Namen zaehlen nur mit 8 Zeichen: laengere oder in 8 Zeichen doppelte Karten-Namen werden zu `Lxxxx`.

## Befunde zur Zuordnung
* **Daten sind nicht nach Modulen trennbar**: Modul `low` (PSA, `mch`) besitzt 875A..8766, Modul `daten` den ganzen Rest (8766..8DC0); die Zeichenketten
  stehen nicht in Textreihenfolge (kernel_struktur.md §6).  Fuer Bytegleichheit und Verschiebe-Test genuegt das; die genaue Aufteilung ist K6/K7 (die C-Module bringen ihre eigenen Daten mit).
* **Adressen unter den Direktwerten**: nur PUSH/LD mit Funktionsanfang (CALL-Ziel, Name, `code`) und jeder Wert im Datenbereich/BSS; `AND/CP/ADD`
  gegen Werte wie 2000/7F00 sind Konstanten.  Ausnahmen in der Linkkarte (`konstwert 8000 8800 C000`, `adr`).  Zeigertabellen in Datenbereichen: `dwp`.
* **Nie ausgefuehrter Code muss Code sein**, sonst bleiben seine Adressen Zahlen (`call csv`-Funktionsanfaenge dort stehen jetzt in der Karte).
* **Postfach D532..D586 ist absolut**: `mutos_io` (U880) kennt D532 fest.  Liegt es im BSS (Reihenfolge nach der Datengroesse), bleibt der Kernel nach einer
  Verschiebung stehen (Halt in `idle`).  Deshalb endet `bss` bei D532 und darueber bleibt es Konstante.

## Verschiebe-Test
`linkbau.py --verschiebe prf`: ein `nop` am Ende von `prf`, neu binden (36 354 B), als `mutos_1216` auf die Ladediskette (`k1520disktool rm` + `put`,
`.fileinfo` mit Blockgroesse 36 480) und booten.  **Bestanden**: der um 2 Byte verlaengerte Kernel (alle Adressen ab `prf` und alle Daten/BSS-Symbole um 2 verschoben) bootet bis `ls -l /`.
Der erste Versuch scheiterte (Halt nach `mem = 196608`): liegen Adresskonstanten als Zahl im Code, bleibt das Bild bytegleich und bricht erst hier.  Gefunden und in die Karte genommen:
Woerter in `db`-Bereichen, die Zeiger sind (`dwp`), Funktionszeiger mit `PUSH` (`adr`), nie ausgefuehrter Code (`code`/`lin` in `karte.txt`: Abdeckung 85 % -> 93,6 %, Rest 42 B).
Schalter von `ld`: `-s` genuegt; kein `-n`/`-i`/`-T` noetig, Text beginnt bei 0.  `linkbau.py --verschiebe` laeuft als eigener Gastlauf (die /tmp-Diskette fasst nur ein Bindeergebnis).

# k3521tst — Prüfprogramm für die K3521.30 (batteriegepuffertes 4-KB-RAM)

`k3521tst.com` ist ein CP/A-Programm (Z80, .COM) für den A5120/K8924 (Entwurf 33 N10i; Karte:
`core/cards/k3521/k3521.h`, bis zu vier Karten seit N10h). Teil des Projekts K1520emu, Autor olliy78.

    K3521TST <hexadresse> <modus>        Beispiel: K3521TST 8000 T

`modus` **T** = Speichertest, **E** = Editor; Groß- und Kleinschreibung gleich. Ohne Parameter oder
bei ungültigem Parameter erscheint der Hilfetext (Beschreibung, Aufruf, Parameter, Beispiel) und zuletzt
die Meldung (`Keine Parameter.`, `Adresse ist keine Hex-Zahl (1 bis 4 Stellen).`, `Modus fehlt …`,
`Zu viele Parameter.`, `Modus nicht zulaessig …`, `Adresse nicht zulaessig …` mit dem erlaubten Bereich).

## Adressgrenzen (Annahme)

Das 4-KB-Fenster ab `<hexadresse>` muss im **4-KB-Raster** liegen und ganz im **freien TPA**:
* nicht unter `PEND` (Programmende samt Stapel, auf 4 KB aufgerundet — in der Praxis ab **1000H**),
* nicht über dem BDOS-Einsprung (Wort bei 0006H): `Adresse + 0FFFH < [0006H]`. Unter CP/A V25.09.89 steht
  der Einsprung bei C406H (Banner „TPA 100H - 0C405H“); letzte zulässige Adresse ist dort **B000H**.
Die Grenze wird zur Laufzeit aus 0006H gebildet, nicht fest. Die Karte der Emulator-Vorgabe (**8000H**) liegt
mittendrin; der CP/A-Kaltstart lässt dort Inhalte stehen (Merkposten Nr. 17).

## Modus T — zerstörungsfrei

Vorgabe, weil dieselbe Karte Nutzdaten tragen kann: der Inhalt wird in einen 4-KB-Puffer **außerhalb** des
Fensters gesichert (hinter dem Programm, sonst unter dem BDOS), danach laufen
`00`, `FF`, `55`, `AA`, Adressmuster (H xor L) und invers, Wanderbit 1 und 0 (je 8 Durchläufe) und
**March C-** (w0 / ⇑ r0 w1 / ⇑ r1 w0 / ⇓ r0 w1 / ⇓ r1 w0 / r0); zuletzt wird der Inhalt zurückgeschrieben
und gegen die Sicherung geprüft (`Rueckschreiben`). Je Stufe eine Zeile `OK` bzw.
`FEHLER n, erster bei aaaaH Soll ss Ist ii`; Ende `K3521TST OK` oder `K3521TST FEHLER n` (Gesamtzahl), dann
Warmstart. Nur ein Reset/Absturz **mitten im Test** ließe den Inhalt verloren gehen.

## Modus E — Editor

Der Bildschirm (K7024, 80 × 24 ab F800H, wird direkt beschrieben) zeigt den Inhalt als Text, Bytes außerhalb
20H–7EH als `.`. Druckbare Zeichen gehen **sofort** in die Karte; der Cursor (BIOS-Treiber, `ESC zeile spalte`)
folgt, der Ausschnitt scrollt zeilenweise (80 Byte je Zeile, 4096 = 51,2 Zeilen). **Annahmen zu den Tasten**
(CP/A-Standardstrings der Kursortasten, `bios.mac` `costs`; die K7637-Emulation liefert Pfeil links/rechts/hoch/
runter als 0x96/0x97/0x94/0x95, die das BIOS in diese Zeichen übersetzt):

| Taste | Code |
|-------|------|
| links | ^H (08H) — zugleich Backspace, bewegt nur; ^S (13H) auch |
| rechts / hoch / runter | ^D (04H) / ^E (05H) / ^X (18H) |
| DEL | schreibt ein Leerzeichen an die Cursorstelle und geht eine Stelle zurück |
| ESC (1BH) oder ^C (03H) | Ende (löscht den Schirm, `K3521TST ENDE`, Warmstart) |

^C ist zugleich „Seite runter“ der K7637; Seitentasten sind darum nicht belegt. Ohne gesteckte Karte ist das Fenster
gewöhnliches RAM (der Test läuft dann trotzdem grün).

## Bau und Prüfung

`python3 tools/k3521tst/build.py` (M80 + LINKMT über `tools/cpm_bau.py`); die `.com` ist eingecheckt, Wächter
`cli_k3521tst_com_passt_zur_quelle`. Gastlauf: `K3521Tst.*` (`tests/integration/test_k3521tst.cpp`). Das
Programm liegt auf den A5120-Bootdisketten (CP/A und SCPX 1.7, `tools/disketten_beigaben.py`); **unter SCPX
ungeprüft** (nur CP/A im Gast gelaufen; die Cursorsteuerung der SCPX-Konsole ist nicht verglichen).

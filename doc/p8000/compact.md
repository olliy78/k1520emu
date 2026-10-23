# P8000 Compact als Hardwarevariante (Ticket #15, Stand 2026-10-23)

Gekennzeichnet: **[gelesen]** = in einer Quelle belegt, **[abgeleitet]** = Schluss daraus, **[offen]** = Anwenderfrage.

## 1. Quellen
* robotrontechnik.de, `html/computer/p8000compact.htm` (lokaler Spiegel `~/projects/robotron/Robotron_Technik/www.robotrontechnik.de/html/computer/p8000compact.html`).
* Prospekt `http://www.pofo.de/P8000/notes/P8000_compact.pdf` (pofo.de: TLS-Zertifikat defekt, nur lesend mit `curl -k`).
* `http://www.ycdtotv.de/p8000/` (Abschnitt „P 8000 Compact“: EPROM-Abzüge, `/etc/insfdhd2`, `inittab`/`config`, Fotos).

## 2. Unterschiede zur P8000
* **[gelesen]** Die Compact (1989, letztes EAW-Projekt) übernimmt 8-Bit-Platine, 16-Bit-Platine und Winchester-Controller der P8000
  unverändert; geändert sind nur Stecker. Daraus **[abgeleitet]**: gleiche Schaltung, also kein neuer Kernbaustein.
* **[gelesen]** Festplatte(n) samt Controller im Gehäuse (1 oder 2 × 44 MB, ST512), 2 Floppys, batteriegepufferte Uhr
  (Neuentwicklung, ab Zwei-Prozessor-Variante) und EPROM-Programmer gehören zur Grundausstattung; bis 4 MB DRAM, 5 Steckplätze für
  DRAM und Uhr; bis 7 Terminals; Betriebssysteme WEGA/WDOS/OS/M/UDOS.
* **[gelesen]** Fotos auf ycdtotv.de: Compact #1 mit 2 × K 5504.50 und **3 Floppys**, Compact #2 mit 2 × K 5504.50 und 2 Floppys.
* **[gelesen]** Hardwarevarianten: reine 8-Bit (U880) / Zwei-Prozessor (U880 + U8001) / Drei-Prozessor (+ U80601-WDOS-Adapter, nur
  ein Exemplar bekannt).
* **Ziel im Emulator: nur die Zwei-Prozessor-Variante mit Terminal.** Eine 8-Bit-Compact und der WDOS-Adapter werden
  ausdrücklich nicht nachgebildet; das P8000-Terminal bleibt unverändert.

## 3. EPROM-Befund
Die vier Compact-Abzüge von ycdtotv.de sind per `cmp` und SHA-256 **bitgleich** mit den vorhandenen:

| ycdtotv.de | vorhanden | SHA-256 |
|---|---|---|
| `wega3.1-8_1-3522.bin` | `eproms/8BIT/MON8_1_3.1` | `1de27889c164…751bfe` |
| `wega3.1-8_2-cb96.bin` | `eproms/8BIT/MON8_2_3.1` | `376b7fa31319…3d06d3151` |
| `wdc4.2_1-2c43.bin` | `eproms/WDC/WDC_1_4.2` | `9eee35382b05…f9be674` |
| `wdc4.2_2-5d66.bin` | `eproms/WDC/WDC_2_4.2` | `a1bbac99a2ce…4c697bc2e3` |

Es sind keine neuen Abzüge nötig (nicht die `_NUR8BIT`-Fassungen). Ein MON16-Abzug der Compact ist auf ycdtotv.de **nicht** aufgeführt.
Weitere Compact-spezifische EPROM-Hinweise fanden sich auf ycdtotv.de/pofo.de nicht (nur Anwendungsdateien: `insfdhd2`, `inittab`/`config`,
`datem.c`/`KTIME`).

## 4. Umsetzung
**Voreinstellung, kein Modell (Anwenderentscheid 2026-10-23).** P8000 und P8000 Compact sind im `p8000emu` Vorgabe-Sätze einer Bauart
(`Programmprofil.bauarten`, *Einstellungen ▸ Allgemein ▸ Modell*), wie A5120/A5130/K8924 beim A5120; Entwurf 26 §8.7. Der Kern bleibt
unverändert (gleiche Schaltung, Kernparameter wie das Vollgerät).

| | P8000 | P8000 Compact |
|---|---|---|
| MON8 / MON16 | 3.0 / 3.0 | 3.1 / 3.1 |
| Hauptspeicher | 4 × 256 KB | 4 × 1 MB |
| RTC | U130X | U130X |
| Disketten K5601 | 2 | 3 |
| Winchester K5504.50 | 1 | 1 |

Die Wahl übernimmt die Werte ohne Rückfrage (Kaltstart), danach ist jede Option einzeln änderbar. Hinzu kommt *Einstellungen ▸ Laufwerke ▸
Winchester-Laufwerke* (1–3) mit je einem Plattenkasten (Kern: bis drei Laufwerke, Merkposten 61).

## 5. Offen / beantwortet
1. ~~Uhrentyp der Compact~~ **beantwortet (Anwender): U130X.** Die Epson RTC-72421 ist ein Community-Nachbau, bleibt als Option wählbar, ist nie Vorgabe.
2. ~~MON16-Version und Platinenindex~~ **beantwortet (Anwender): MON8/MON16 3.1** (die klassische P8000 bekommt 3.0); Platinenindex bleibt 8-Bit 3 / 16-Bit 4.
3. ~~Dritte Floppy~~ **beantwortet (Anwender): 3 Disketten-Laufwerke K5601** in der Voreinstellung (Treiber/Belegung im Gast weiter ungeprüft **[offen]**).
4. **[offen]** Anzahl und Belegung der bis zu 7 Terminal-Anschlüsse (der Emulator hat die tty-Schnittstellen der P8000).
5. Zweite Platte: 2 × K 5504.50 sind belegt. Kern: bis DREI Laufwerke (`Config::platte/platte1/platte2`, Merkposten 61), Oberfläche: bis drei Plattenkästen (2026-10-23). **[offen]** Ein Gast-Lesebefehl an LW 1/2 (MON16/WEGA) ist nicht getestet.

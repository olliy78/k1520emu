# PCMC — Datenaustausch PC <-> MC80.30 (Issue #24)

Quelle: `~/projects/robotron/mc80/PCMC_komplett/` (PCMC v0.2, KW-WILLISOFT 2007: Windows-Programm
`PCMC.exe`, EPROM-Erweiterung, Quelltexte, Anleitung `PCMC.pdf`, IFSS-USB-Wandler).  Abzug und Quelle
liegen in `doc/EPROMS/MC80/pcmc/` (`pcmc-2800.bin`, `PCMC_ROM_2800.ASM`).

## Hardware / Verdrahtung

* EPROM 2708 **ab 2800H** (3. Fassung der SPE 2; ausgeliefert waren nur 2000H und 2400H bestueckt).  Der Abzug
  `2400H` mit der geaenderten RAM-Kettung (damit PCMC unter CATA erscheint) ist **bytegleich** zu `spe_2.rom`
  der MAME-Sammlung — im Kern aendert sich nur 2800H (`Mc80Spe2::Config::mc8030Pcmc`, `Mc80Machine::Config::pcmc`,
  C-ABI-Schluessel `pcmc=1`).
* Programm im ROM, 8 Byte RAM bei F300H (`ramz1`/`ramz2`) fuer die Adressanzeige.
* Leitung: IFSS XS503 = ASP **SIO B** (D2H Daten, D3H Steuer), CTC K1 (D5H), **4800 Bd 8N1** (WR4 = 44H, x16;
  CTC-Zeitkonstante 1, Eingang halber Systemtakt, ÷2).  Am PC ein IFSS-USB-Wandler (virtueller COM-Port).

## Protokoll (aus `PCMC_ROM.ASM`, ohne Pruefsumme, Handschlag je Byte)

| Richtung | MC80-Menue | Ablauf |
|---|---|---|
| PC → MC80 | `E` | PC: Start lo, Start hi, **Laenge−1** lo, hi, dann die Daten; der MC80 antwortet auf JEDES Byte mit demselben Byte, erst dann das naechste (nach dem 4. Kopfbyte erst nach der Adressanzeige). |
| MC80 → PC | `S` | PC: Start lo, hi, **Ende** lo, hi (ohne Echo); MC80 sendet die Bytes, der PC bestaetigt jedes mit demselben Byte, der MC80 vergleicht (sonst „Uebertragungsfehler !“). |

Dateien: `Start lo, Start hi, Laenge−1 lo, hi` + Daten (Beispiel der Anleitung: 4000H, 300H Byte → `00 40 FF 02`).
Eine am MC80 geholte Datei bekommt den Kopf selbst.  Der MC80 wartet ohne Frist; Fristen und Fortschritt hat
nur die PC-Seite.  Gedruckt wird die Bereichsanzeige „Von - Bis: aaaaH - eeeeH“ (die Endadresse beim Empfangen
ist `Start + Laenge−1`, mit dem Kopf der Datei gerechnet).

## Im Emulator

* `app/pcmc.py` (Protokoll, `PcmcLeitung` = Telnet-Client mit BINARY in beide Richtungen: sonst wird 0DH als
  0DH 00 uebertragen), `app/ui/pcmc_widget.py` (Kasten), Aktion `pcmc` (*Maschine ▸ PCMC-Gegenstelle*).
* Einschalten macht die IFSS zum Telnet-Server auf 127.0.0.1 (freier Port) und verbindet sich;
  Ausschalten stellt die vorherige IFSS-Einstellung wieder her, auch in der gespeicherten Konfiguration
  (`PcmcWidget.fuer_konfig`).  Eine belegte IFSS wird nicht angefasst; ein offener Telnet-Server wird mitbenutzt.
* Waechter: `Mc80Pcmc.*` (Kern, PC-Seite als Telnet-Client am Hub), `py_pcmc` (Protokoll, Ende zu Ende ueber die
  Oberflaeche).

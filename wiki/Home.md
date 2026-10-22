# K1520emu

K1520emu ist eine Sammlung hardwarenaher Emulatoren und Werkzeuge für Rechner aus dem
K1520-Bausteinsystem der DDR und den EAW P8000. Der Emulator bildet die Hardware nach:
Boot-ROM, BIOS und Betriebssystem laufen als echter Maschinencode statt als
Betriebssystem-Nachbau.

## Emulierte Rechner

| Rechner | Betriebssysteme |
|---|---|
| robotron A5120 (einschließlich A5120.16) | CP/A, SCPX, UDOS, BCU 880 |
| robotron K8915 (V3 und V2) | SCPX 8915 |
| PRG 710 / PRG 710-1 | UDOS, SCPX |
| robotron PC 1715 / PC 1715W | SCP 1715, CP/A, CP/Z, UDOS 1715, SCP 3.0 |
| MC80.30 / MC80.31 | EGOS |
| EAW P8000 | UDOS, WEGA (UNIX) |

Enthalten sind außerdem ein DiskTool zum Lesen und Schreiben von Diskettenabbildern,
der Debugger `k1520dbg` und Kermit zum Dateiaustausch mit WEGA. Die fertigen Pakete
für Windows und Linux sind auf der
[Releases-Seite](https://github.com/olliy78/k1520emu/releases) verfügbar.

## Dokumentation

- [Architektur des Emulators](Architecture)
- [Handbuch des Emulators](Emulator-Handbook)
- [Handbuch des DiskTools](DiskTool-Handbook)
- [Handbuch von K1520 Kermit](Kermit-Handbook)
- [Handbuch des Debuggers](Debugger-Handbook)

Weitere Informationen stehen in der
[Projekt-README](https://github.com/olliy78/k1520emu/blob/main/README.md).
Die Programmquellen stehen unter MIT-Lizenz; für mitgelieferte Diskettenabbilder,
EPROM-Abzüge und digitalisierte Unterlagen gelten die Rechte der jeweiligen
Rechteinhaber.

[[Home-English|English]]

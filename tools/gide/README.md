# Gast-Software der Hardwareoptionen (GIDE, Uhr, USB, Netz) auf den Systemdisketten

Die Karten **GIDE** (IDE-Platte + RTC 72421), **GUN-USB** (VDIP1) und **KCNet** (W5100) sind nur mit Gast-Software
benutzbar.  Diese Software liegt seit **2026-11-11** im Repository und auf den ausgelieferten Systemdisketten
(`disks/`), damit die Optionen nach dem Auspacken des Pakets sofort gehen.

> **Anwenderentscheidung, kein Rechtsbefund.**  Der frühere Grundsatz „Fremdsoftware nur lokal, nie ins Repo"
> (`doc/merkposten/gun.md`, Entwurf 50 §2a) ist für genau die Dateien in `fremd/` **aufgehoben** — auf ausdrückliche
> Anweisung des Anwenders („Wir müssen die notwendige Software auch auf die Disketten bringen, die wir mit dem Installer
> ausliefern"). Der Anwender hat die **Weitergabe mit dem Paket am 2026-11-13 ausdrücklich freigegeben**; eine Rückfrage bei den Urhebern gab es nicht. Die Tabelle unten gibt den
> Kenntnisstand wieder. Soll `fremd/` doch aus dem Paket, geht das über (`packaging/build_payload.sh`, Abschnitt „Gast-Software der Hardwareoptionen").

## Herkunft und Status

| Datei(en) | Was | Autor / Version | Fundort (abgerufen 2026-11-10) | Weitergabe |
|-----------|-----|-----------------|--------------------------------|------------|
| `fremd/poppe/gidehp00..f0.com`, `eradir.com` | GIDE-Treiber für CP/A (nachladbar, Laufwerke ab dem ersten freien Buchstaben) und Verzeichnis-Anlegen | Heiko Poppe, V0.2 | hc-ddr.hucki.net, Wiki *z9001/cpm/gide* | **ungeklärt**; für den Quelltext der Treiberfamilie nennt die Fundstelle „private Nachnutzung gestattet, nicht kommerziell" |
| `fremd/kingstener/gidn_50h.com` (Ausgangsstück für `GIDN90.COM`) | nachladbarer GIDE-Treiber für SCPX/SCP, Laufwerk N: | Kingstener (DL), V0.4, 2014; „basiert auf RAF2008-Treiber und gideide.mac (C) 1995 Tilmann Reh"; Reassemblierung V. Pohlers | hc-ddr.hucki.net, Wiki *z9001/cpm/gide* | **ungeklärt**; laut Wiki „mit freundlicher Genehmigung" des Autors dort eingestellt — keine Erlaubnis für uns |
| `fremd/utools/{usb,udir,uget,uput}.com` | UTools 1.4: USB-Stick am VDIP1 (PIO DCh..DFh) | © 2008–2011 M. Leubner; CP/M-Fassung (AC1) von Ralph Hänsel | hc-ddr.hucki.net, Wiki *z9001/module_sonstige/guk_v8* | **ungeklärt** |
| `fremd/kcnet/CPMN1587.COM`, `TFTP1287.COM` | CPMNET 1.5 (2008) und TFTP 1.2 (2009) für die KCNet-Karte | `>susowa<` = Ralf Kaestner | kc85.info (Software-Seite der KCNet) | **ungeklärt**; die Nutzungsbedingungen der Seite waren aus der Entwicklungsumgebung nicht abrufbar (kein Netz) — **nachzuholen** |
| `uhr/` (`UHR.COM`, `src/uhr.mac`) | Uhr der GIDE-Karte anzeigen/stellen | **eigenes Werk**, MIT; Registerbelegung aus dem Verständnis von Rehs `gidertc.mac`, kein Quelltext übernommen | — | frei (MIT) |
| `basis_anpassen.py`, `karten.py` | Umschreiben der Basis, Zusammenstellung je Rechner | eigen, MIT | — | frei |

Die Dateien von Tilmann Reh (`gideide.mac`, `gidertc.mac`) und die Quelltexte der Fremdprogramme liegen **nicht** im
Repository; sie standen nur für das Verständnis zur Verfügung (Entwurf 50 §2a).

## Welche Diskette trägt was

Die Software passt zur **Vorauswahl der Oberfläche** (`app/profil.py` `karten_vorauswahl`, Handbuch „Zusatzkarten"):
GIDE **90H** überall; USB **84H** (A5120 alle Modelle, K8915 V3, PC 1715/1715W) bzw. **8CH** (K8915 Gen 2, PRG 710/710-1);
Netz **C0H** (A5120, K8915, PC 1715) bzw. **28H** (PRG 710/710-1).  Aufstellung: `karten.py` (`RECHNER`); der Waechter
`cli_karten_beigaben` vergleicht jede Diskette bytegleich damit.

| Systemdiskette (`disks/…`) | GIDE-Treiber | Uhr | USB | Netz | frei danach |
|----------------------------|--------------|-----|-----|------|-------------|
| A5120 CP/A (`a5120_cpa`) | Poppe `GIDEHP90` + `ERADIR` | `UHR` | 84H | C0H | 82 KB |
| A5120 SCPX 1.7 (`a5120_scpx17`) | Kingstener `GIDN90` | `UHR` | 84H | C0H | 160 KB |
| K8915 SCPX 8915 (`k8915_scpx8915`) | Kingstener `GIDN90` | `UHR` | V3 84H (`USB`…), Gen 2 8CH (`USB8C`…) | C0H | 268 KB |
| PRG 710 SCPX 1.5 (`prg710_scpx15`) | Poppe `GIDEHP90` + `ERADIR` (Kingstener läuft unter SCPX 1.5 nicht) | `UHR` | 8CH | 28H | 48 KB |
| PRG 710-1 SCPX 1.7 (`prg710-1_scpx17`) | Kingstener `GIDN90` | `UHR` | 8CH | 28H | 36 KB |
| PC 1715 CP/A 1715 (`pc1715_cpa1715`) | Poppe `GIDEHP90` + `ERADIR` | `UHR` | 84H | C0H | 260 KB |
| PC 1715 SCP 1715 V0006/V0007 | Kingstener `GIDN90` (ohne PIP: `SAVE` zum Beschreiben) | `UHR` | 84H | C0H | 190 / 72 KB |
| PC 1715W SCP 3.0 (`pc1715w_scp30`) | **keiner** (kein Treiber für SCP 3.0 bekannt) | `UHR` | 84H | **fehlt** (Netz-Pakete 33 KB, nur 12 KB frei) | 12 KB |

Auf jeder Diskette dazu `GIDE.TXT`, `USB.TXT` und (außer PC 1715W) `NETZ.TXT` — kurze Bedienung.
Auf **nicht** bestückten Systemdisketten (UDOS, MicroDOS, SIOS, BCU, CP/Z, BS620 …) liegt nichts: dort ist unklar, ob die CP/M-
Treiber laufen (nicht geprüft), oder es gibt kein CP/M.  Wer eine andere **Basis** einstellt, braucht passende Dateien —
`basis_anpassen.py` (unten) oder bei Poppe die fertige Fassung (`fremd/poppe/`, im Paket
`share/k1520emu/karten_software/`).

## Bauweise je Programm

| Programm | Bauweise | Nachweis |
|----------|----------|----------|
| `GIDEHP90.COM`, `ERADIR.COM` | **unverändert** aus der Quelle des Autors (für jede Basis gibt es eine Fassung) | Systemtests `KartenBeigaben.A5120_CPA`, `PC1715_CPA1715`, `PRG710_SCPX15` |
| `GIDN90.COM` | **umgeschrieben** aus `gidn@50h.com`: 16 Operanden (Hochnibble 5 → 9); `gidn(0xD0)`/`gidn(0xF0)` erzeugen **bytegleich** die Fassungen `@D0h`/`@F0h` des Autors (SHA-256 im Wächter) | `test_karten_software.py::test_gidn_*`, Systemtests K8915/A5120 SCPX/PRG 710-1/SCP 1715 |
| `USB/UDIR/UGET/UPUT` für 84H/8CH | **umgeschrieben**: 16 Port-Operanden im gemeinsamen Teil (`USBINC.Z80`), in allen vier Programmen an denselben Stellen; DCh ergibt das Original. **Neuaufbau aus dem Quelltext war nicht möglich**: der Linker (SLRNK) braucht `DSLIBS.REL`/`SYSLIBS.REL` des Autors (`$MEMRY`, `CONDIN`, `COUT`, `EPRINT`, `PA2HC`, `U2MTIM`), die nicht zum Paket gehören | Disassemblierung der Stellen, Systemtests auf 6 Rechnern |
| `CPMNET.COM`, `TFTP.COM` | C0H **unverändert**; für 28H **umgeschrieben**: 10 Operanden des Treiberblocks 0109..028E (`LD C,n`, `IN/OUT (n)`), in beiden Programmen identisch; sonst kommt C0H..C3H im ganzen Programm nicht vor (disassembliert) | Systemtest `KartenBeigaben.PRG710_*` (28H) |
| `UHR.COM` | **eigen**, `tools/gide/uhr/build.py` (M80 + LINKMT über die CPA_Workbench, wie SERTEST); `.com` eingecheckt, Wächter `cli_uhr_com` | alle Systemtests |

## Werkzeuge

```sh
python3 tools/gide/basis_anpassen.py usb 8C  --out ordner   # UTools für eine andere Basis (Vielfaches von 4)
python3 tools/gide/basis_anpassen.py net 28  --out ordner   # CPMNET/TFTP
python3 tools/gide/basis_anpassen.py gidn 90 --out ordner   # Kingstener-Treiber (Vielfaches von 10H)
python3 tools/gide/uhr/build.py [--check]                   # UHR.COM
python3 tools/gide/karten.py --check --tool build/k1520disktool   # Wächter
```

Disketten **neu bauen** (Reihenfolge egal; jede Diskette hat ihr Bauskript, `tools/diskbau.py` / `karten.py` liefern die Dateien):
`tools/cpa_a5120`, `scpx_a5120`, `scpx_k8915`, `scpx_prg` (beide PRG), `cpa_pc1715`, `scp_pc1715` (V0006, V0007 — **danach
`git checkout disks/pc1715_cpz22/…`**, das Skript baut die CP/Z-Diskette mit und die ist nicht bytegleich neu zu erzeugen),
`scp_pc1715w`, je mit `--tool build/k1520disktool`.

## Tests

- `KartenBeigaben.*` (`tests/integration/test_karten_beigaben.cpp`, **ohne Überspringen**): je Rechner die AUSGELIEFERTE Diskette (Kopie)
  booten, GIDE auf 90H, USB, Netz auf der Vorauswahl-Basis stecken und die Programme laufen lassen — Treiber starten, Datei über die
  Platte hin und zurück (bitgleich), `UHR` zeigt/stellt die Zeit, `USB FWV`/`UDIR`/`UGET` gegen einen Temp-Ordner als Stick,
  `TFTP`/`CPMNET` melden Version und „online".  Nur Loopback, kein UDP-Port 69.
- `py_karten_software` (Umschreiben, Zusammenstellung, Vorauswahl = Basis der Software), `cli_karten_beigaben`, `cli_uhr_com`.
- Die älteren Fälle `GideSystem.*`/`GunUsbSystem.*`/`GunNetSystem.*` bleiben (sie benutzen die lokal liegende Software und
  überspringen sich ohne sie).

## Grenzen (Stand 2026-11-11)

- **Kingstener `GIDN` läuft nicht unter SCPX 1.5** (PRG 710): `DIR N:` zeigt leere Namen; dort ist Poppe eingesetzt (läuft).
- **SCP 1715** hat kein brauchbares `PIP` (V0006: „REQUIRES SCP3"; V0007: fehlt) → der Test schreibt mit `SAVE`.
- **Netz am PC 1715W** passt nicht mehr auf die Systemdiskette (12 KB frei).  Vorschlag, falls gewünscht: eine kleine Zusatzdiskette
  `pc1715w_karten.hfe` (nur CPMNET/TFTP + `NETZ.TXT`) — nicht angelegt.
- Nicht ausgestattet (nicht geprüft, ob die CP/M-Treiber dort laufen): MicroDOS 1526/1715, CP/Z, SIOS, UDOS.
- Das Netz-Menü von CPMNET meldet „DHCP/DNS/Ping" ohne Wirkung (W5100-Modell ohne Broadcast/ICMP).

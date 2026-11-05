# Drucker-Unterlagen (k1520printer, Entwurf 49)

Bestand für den Druckeremulator `k1520printer` (Ticket #28, `doc/design/49_k1520printer.md`).  Alle Originale sind Scans
und stammen von [tiffe.de/Robotron](https://www.tiffe.de/Robotron/) (Holm Tiffe) bzw. aus der lokalen Sammlung
`~/projects/robotron/Drucker/`; eingesammelt am 2026-11-04/05.  EPROM-Abzüge liegen getrennt unter `doc/EPROMS/K6316/` und
`doc/EPROMS/SD1157/` (je mit README).

| Ordner | Gerät | Firmware | Schaltplan | Stand |
|---|---|---|---|---|
| `k6316/` | **K6316** Schalterdrucker (Bank), 9 Nadeln, 80 Spalten, 100 Z/s, Rollen/Leporello, Schneideinrichtung | **ja**: V.24- und IFSS-Satz, je 5 × 2716 | **ja** (19 Blätter, PDF + Scans Stadtarchiv Sömmerda) + Servicehandbuch 74 S. + Manual 36 S. | vollständig |
| `sd1157/` | **SD 1157** Seriendrucker, 9 Nadeln, 132/210 Spalten, 180 Z/s, U880 in Sloteinheit (11 LP) | **ja**: `1157.rom` + Zeichengenerator, **kommentierte Quelle `1157.mac`** (JDE 1984–86, angepasste Fassung) | **nein** — Hardware aus der Quelle rekonstruierbar (E/A-Belegung im Kopf von `1157.mac`) | Firmware vollständig, Hardware [?] |
| `k6313/` | **K6313/K6314** Hard-Copy-Drucker, 9 Nadeln | **nein** (EPROM-Kassette nicht öffentlich, s. Entwurf 49 §2.2) | ja (5 Blätter) + Servicehandbuch + Manual 1–3 | wartet auf Abzug |
| `sd1152_257/` | **SD 1152/257** Typenraddrucker | nein | Servicehandbuch 1987, Manual, Ersatzteilkatalog | Kandidat Typenrad, später |

## Herkunft im Einzelnen

* `k6316/original/`: `K6316_Manual.pdf` ← `Drucker/K6316/Manual/K6316.pdf`; `K6316_Service.pdf`/`_Service_v.pdf` (zwei
  Fassungen derselben Scans, verschieden kodiert) ← `…/Schaltungsunterlagen/SERVICE.PDF`/`SERVICEv.pdf`; `K6316_Plaene.pdf`
  ← `PL+äNE.PDF`; `K6316_Serviceschaltplaene.pdf` ← `Schalterdrucker_K_6316_Serviceschaltpläne.pdf`; `schaltplan_scans/01–19.jpg`
  ← `…/Schaltungsunterlagen/` (Stadtarchiv Sömmerda, 3307×2340); `v24-6313-auf-6316.txt` (Hinweis: V.24-Kassette des K6313 im
  K6316 braucht die 5 EPROMs des K6316).
* `sd1157/`: `1157.mac`, `1157.txt`, `1157.dok` ← `jde/1157/` (falsch einsortiert, Neuerervorschlag „Einsatz SD 1157 an SKR",
  Reg.-Nr. 1258/4/H/9); `original/SD1157_Interfacebeschreibung_IFSS.pdf` ← `Drucker/Interfacebschreibung_IFSS_SD1157.pdf`
  (BÜWK 1983, 9600 Bd, Puffer 1 KiB alphanumerisch / 230 B Grafik, Codes nach RöW 3565-76).
* `k6313/original/`: lokale Sammlung (Servicehandbuch, Schaltpläne, Manual 1–3, DIP-Schalter, ISO-Befehlssatz).
* `sd1152_257/original/`: `Drucker/SD1152-257/` + Kundeninformation IFSS + handschriftliche Anschlussnotiz SD 1152 an A5120 (`misc/…`).

## Weitere Quellen (nicht kopiert)

* robotrontechnik.de (lokal gespiegelt unter `~/projects/robotron/Robotron_Technik/`): `html/drucker/sd11xx.html` (SD 1157:
  1981–87, 16 325 Stück, Varianten 264/267/269, Farbband fest im Gehäuse, Schnittstellen IFSS/V.24/PIO/IFSP/Centronics, Testtaste),
  `k631x.html` (K6316: 1984–90, 16 110 Stück, Varianten .01–.05), `k63xx_module.html` (K6316-IFSS-Modul nicht mit anderen tauschbar).
* Robotrontechnik-Forum, Threads 6045/8310/6421/8849 (K631x-Kassetten, Kennungen).
* Volle Dateiliste tiffe.de/Robotron (12 422 Dateien) im Scratchpad der Sitzung vom 2026-11-05 erzeugt; Ergebnis: keine weiteren Druckerabzüge.

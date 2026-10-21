# Kermit-80 (CP/M-Kermit) als Beigabe

Kermit-80 4.11 liegt als `KERMIT.COM` auf den Systemdisketten von A5120, K8915, PC 1715 (SCP 1715, CP/A 1715),
PC 1715W (SCP 3.0) und PRG 710/710-1 — zum Ausprobieren der V.24-Schnittstelle gegen den Server von K1520 Kermit.  Es ist nur Client.

| Datei | Inhalt |
|-------|--------|
| `quelle/` | rechnerunabhängiger Teil (`cpsker.hex`) und Quellen der Überlagerung (`cp*.asm`), Handbuch `cpkerm.txt` |
| `COPYRIGHT.txt` | Urhebervermerk (reist als `share/doc/lizenzen/kermit80-COPYRIGHT.txt` im Paket mit) |
| `bau.py` | übersetzt die Überlagerung je Rechner neu und fügt `KERMIT.COM` zusammen |
| `kermit_{a5120,k8915,pc1715,pc1715w,prg710,p8000}.com` | fertige Programme, eingecheckt; `p8000` (OS/M am P8000, tty0) liegt keiner Diskette bei |

```sh
python3 tools/kermit80/bau.py a5120 --out tools/kermit80/kermit_a5120.com   # CPA_TOOLS=<Werkzeugkette> nötig
python3 tools/kermit80/bau.py --selbsttest                                  # bildet CPVGEN.HEX bytegleich nach
tools/dev.sh test-format -R Kermit80                                        # Wächter
```

Bedienung am Gerät: `KERMIT`, dann `set file-mode binary`, `send DATEI` bzw. `get DATEI`; die Gegenstelle ist der
Server von K1520 Kermit (V.24-Kanal als Telnet-Client auf den PC).  Stand und Festlegungen:
`doc/design/36_kermit.md` §6.8, `doc/merkposten/kermit.md`.

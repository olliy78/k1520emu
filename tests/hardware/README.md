# Hardware-Testgruppe: echte Diskette am Greaseweazle

Entwurf: `doc/design/40_hardware_testgruppe.md`.  **Nicht Teil der Regression** (ctest sammelt nur
`tests/python/`); braucht Greaseweazle + K5601 + eine Diskette, die überschrieben werden darf.

```sh
tools/dev.sh test-hardware                        # nur lesend: H0, H1, H3.1
tools/dev.sh test-hardware --schreiben            # + Schreibstufen H2–H7 (BESCHREIBT die Diskette)
tools/dev.sh test-hardware --schreiben -k h4m     # nur die Format-Matrix (47 Formate, lang)
```

Optionen: `--keine-sicherung`, `--zurueckspielen` (Sicherung am Ende zurück). Sicherung liegt in
`out/hw_sicherung/`, Bildschirmprotokolle in `out/hw_log/`. Umgebung: `K1520_GW_DRIVE` (a),
`_CYLS` (80), `_HEADS` (2), `_RATE` (250). Lange Läufe in den Hintergrund; der Adapter darf nur von
EINEM Prozess gehalten werden (nicht parallel starten).

| Datei | Inhalt |
|---|---|
| `test_h0_vorpruefung` | Adapter, Drehzahl, Zellzahl |
| `test_h1_lesepfad` | Wiederholbarkeit, ganze Diskette über `Sync`, Gegenprobe mit dem Dekoder von `gw` |
| `test_h2_schreibpfad` | Spur/Abbild schreiben, Pause, Dekoder-Gegenprobe |
| `test_h3_boot` | Boot vom Abbild und von der echten Diskette |
| `test_h4_format` | FORMAT+CPABCGEN+Kaltstart; Format-Matrix von `format_all.py` (inkl. einseitig / Doppelschritt) |
| `test_h5_disktool` | DiskTool live (API + `--physical`), fsck, frisch neu einhängen |
| `test_h6_rundlauf` | Emulator ↔ DiskTool an derselben Diskette |
| `test_h7_betriebsarten` | Doppelschritt (K5600.10/.20-Kompatibilität) über die Sitzung |

# auptest — Prüfprogramm für die AUP 062-8531 (Tastaturteil)

`auptest.com` ist ein CP/A-Programm (Z80, .COM) für den A5120/K8924 mit der Adapterkarte **AUP 062-8531**
auf E/A 40H–43H (Entwurf 33 N10c, `core/cards/aup/aup.h`).  Es fragt die Karte ab wie die ZRE
(kein Interrupt) und zeigt jede Taste als Zeile:

    TT C0          Terminaltastatur K7633, Code C0H
    KT 35 '5'      Kundentastatur, Code 35H (druckbar: Zeichen dahinter)

Bedienung an der Rechnertastatur: `S`/`A`/`G`/`F` schalten SEL-, ALPHA-, GET- und Fehlerlampe um,
`H` gibt ein Hupensignal, `R` setzt die AUP zurück (FFH), `Q` oder `^C` beendet.  Nach jedem
Kommando steht `KMD xx` (gesendetes Kommandobyte) auf dem Schirm.  Ohne Karte (IN 41H liefert
Bits außer 0/1) meldet es `Keine AUP auf 40H-43H (IN 41H = xxH)`.

**Annahme** (wie im Emulator): Gültigkeitsbit 0 = K7633, Bit 1 = Kundentastatur, die übrigen Bits 0.
Am Gerät ungeprüft — der Serviceschaltplan fehlt (`doc/a5130_k8924/aup_analyse.md`).

Bau: `python3 tools/auptest/build.py` (M80 + LINKMT über `tools/cpm_bau.py`); die `.com` ist
eingecheckt, Wächter `cli_auptest_com_passt_zur_quelle`.  Gastlauf: `AupMaschine.CpaGast*`.
**Noch nicht auf den Bootdisketten** (`tools/disketten_beigaben.py`): das zöge den Neubau der
CP/A-/SCPX-Systemdisketten in `disks/` nach sich (offen).

## auptsio — Sonderschnittstelle (N10d)

`auptsio.com` prüft die **Sonderschnittstelle** der AUP (CTC B0H–B3H, SIO B4H–B7H, Kanal B): es
programmiert CTC-Kanal 0 (Vorteiler 16, Konstante 1) und SIO-Kanal B (x16, 8N1) auf 9600 Bd, sendet
`HALLO` und liest zurück, was ankommt.  Ausgabe `EMPF HALLO` + `AUPTSIO OK` (z. B. im Loop des
Reiters *Schnittstellen*), `AUPTSIO KEIN EMPFANG` ohne Gegenstelle, `Keine Sonderschnittstelle (IN B7H = FFH)`
ohne Option.  Gepollt, kein Interrupt.  Registerordnung ist Annahme A9 (B4 Daten A, B5 Steuer A,
B6 Daten B, B7 Steuer B).  Gastlauf: `AupSonder.CpaGast*`; derselbe `build.py` baut/prüft beide Programme.


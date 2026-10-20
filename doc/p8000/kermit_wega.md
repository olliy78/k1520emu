# Kermit auf WEGA 3.1 (Entwurf 36, AP K1)

Stand 2026-10-18.  Test: `tests/system/test_p8000_wega_kermit.cpp`
(`P8000WegaKermit.ServerHinterGetty`, Label `wega_install`, `tools/dev.sh test-wega`; ≈ 87 s Rechenzeit,
390 s Maschinenzeit bis `login:`).

## 1. Befund

- Das **mitgelieferte Z8000-Binary** (`kermit.tar` der Kermit-Diskette, C-Kermit 4.2(030), 78 560 B, setuid root)
  **läuft unter WEGA 3.1 unverändert**.  Rückfall (`kermitd`-Wrapper, Selbstbau) war nicht nötig.
- Auf der ausgelieferten Platte fehlte Kermit (geprüft an einer Kopie: kein `kermit` in `md0/bin`, `md2/bin`).
- **`.kermrc` wird gelesen** (`ckuser.c::cmdini`: `$HOME/.kermrc`, sonst `./.kermrc`), auch wenn `kermit` als
  Login-Shell läuft.  `server` ohne Argumente geht; nach `G F` (Finish) kehrt `server` zurück, die nächste Zeile
  `exit` beendet die Sitzung, getty meldet sich wieder.  `G L` (Logout) beendet über `kill(0,9)` ebenfalls.
- **Ein kennwortloser Benutzer ist unmöglich.**  Der System-III-`login` antwortet bei leerem Kennwortfeld
  `You don't have a password.  Choose one.` / `New password:` und startet `passwd`.  Deshalb hat `kermit`
  das Kennwort **`kermit`** (Laborrechner; Hash mit `perl -e 'print crypt("kermit","Km")'` = `KmhrRNsdxD7LY`;
  dieselbe Rechnung reproduziert den vorhandenen `wega`-Hash `K5LCdPJ/oCesU` für `root`).  Ein Login-Skript
  der PC-Seite sendet also Name **und** Kennwort.

## 2. Was auf der Platte steht

| Pfad (DiskTool) | Im Betrieb | Modus / Besitzer |
|---|---|---|
| `md0/bin/kermit` | `/usr/bin/kermit` | 4755, 0:0 (`wega`/`system`) |
| `md0/man/man1/kermit.1` | `man kermit` (Quelle `ckermi.nr`, nroff-man) | 0644 |
| `md0/kermit/` | Home `/usr/kermit` | 0755, 7:1 |
| `md0/kermit/.kermrc` | `set file type binary` / `server` / `exit` (K1 zusätzlich `set file names literal`, in K7 gestrichen) | 0644, 7:1 |
| `md0/kermit/.profile` | `exec /usr/bin/kermit` (seit K7) | 0644, 7:1 |
| `md2/etc/passwd` | eine Zeile angehängt: `kermit:KmhrRNsdxD7LY:7:1::/usr/kermit:/bin/sh` (K1: Shell `/usr/bin/kermit`) | Rest byteweise unverändert |

uid 7 war frei (belegt: 0–6, 10, 24), gid 1 = `other` (`/etc/group` kennt `system:0`, `other:1`, `adm:4`).
`.kermrc`: binär, damit der Server Dateien unverändert (LF) liefert; die Vorgabe von C-Kermit 4.2 wäre Text
(CR LF auf der Leitung, beim Empfang jedes CR verworfen).  Literale Namen (K1) gehen NICHT: siehe §6.  Platz danach: md0 443 KB, md2
379 KB frei; `k1520disktool check --full` ohne Befund.  Eingespielt mit `k1520disktool put/mkdir/attr`
(`attr` an Platten seit diesem AP).

## 3. Beobachtungen am laufenden WEGA

Konsole (`wega`/`root`):

```
#1 ls -l /usr/bin/kermit /usr/kermit /etc/passwd
-rw-r--r-- 1 wega     system      379 Oct 18 ..:.. /etc/passwd
-rwsr-xr-x 1 wega     system    78560 Oct 18 ..:.. /usr/bin/kermit
/usr/kermit:
total 1
-rw-r--r-- 1 kermit   other        56 Oct 18 ..:.. .kermrc
#2 kermit
C-Kermit 4.2(030) PRERELEASE # 2, 5 March 85, AT&T System III/System V
Type ? for help
C-Kermit>show parameters
Line: /dev/tty, speed: -1, mode: remote, modem-dialer: direct
 Parity: none, duplex: full, flow: xon/xoff, handshake: none
 Timeout: 7, delay: 5
 Padding: 0, pad character: 0
 Packet start: 1, end: 13, length: 90, block check: 1
...
```

`man kermit | head` meldet `Reformatting kermit.1` und zeigt `KERMIT(1C)` — die Seite wird sauber genommen.

tty0 (Hub-Kanal als Telnet-Server, Gegenstelle ein rohes Socket + `TelnetCodec`, Telnet-binär ausgehandelt):

1. `login:` (nach RETURN) → `kermit\r` → `Password:` → `kermit\r`.
2. Der Server sendet die Tagesmeldung (`/etc/motd`, ASCII-Bild „WEGA", ≈ 1,7 KB), dann
   `C-Kermit 4.2(030) PRERELEASE # 2, 5 March 85, AT&T System III/System V\r\nType ? for help\r\n\r\n` und
   `C-Kermit server starting. …FINISH command and then reconnect.\n\r\n` (kein Shell-Prompt).  Nach der
   Wartezeit ohne Eingabe NAKt der Server von selbst (`<01># N3\r`, Typ `N`, Folge 0).
3. Init-Paket (Typ `I`, Prüfsumme 1, Folge 0, Daten `~* @-#Y1`) → Antwort
   `<01>, Y~* @-#Y1~%\r`: Typ `Y`, Folge 0, Daten = MAXL 94, TIME 10, NPAD 0, PADC `@`, EOL CR, QCTL `#`,
   QBIN `Y`, CHKT `1`, REPT `~` (9 Zeichen), Prüfsumme gültig.  Der Server bietet also 8-Bit-Präfix und
   Wiederholungszähler an; Prüfsumme 1 und Paketlänge 94 (C-Kermit 4.2 kann weder 2/3 noch Langpakete).
4. Danach beginnt der Client mit **Folge 0** (der Server setzt die Zählung beim `I` zurück).
   `G F` → `Y`-Paket, `C-Kermit server done`, dann `exit`: Sitzung beendet, **getty meldet sich wieder**.
   `G L` → `Y`-Paket, danach ebenso.  Beides ist ein Plus: kein Aufräumen nötig.

## 4. Für die PC-Seite (K3/K7)

- **getty gibt die Anmeldezeile nach einer Sitzung mit gerader Parität aus** (`WEGA login:` als
  `<D7><C5>GA<A0>lo<E7>i<EE>:`); nach dem Einschalten kommt sie ohne.  Das Login-Skript muss Bit 7 vor dem
  Vergleich löschen (der Test tut es in `Anlage::rx7`).  Die Protokolldaten selbst sind davon nicht betroffen.
- Vor dem ersten Paket kommen **1,9 KB Text** (Kennwortzeile, ESC-Sequenzen der Tagesmeldung, Kermit-Banner);
  die Paketerkennung muss auf `SOH` synchronisieren.  Eine bestehende Verbindung braucht ≈ 10 s
  Maschinenzeit zwischen Name/Kennwort und dem Server (Anmeldung, `.kermrc`); davor eingehende Pakete würden
  noch vom Terminaltreiber (cooked, Echo) behandelt.
- Die Wartezeit im Test (40 Mio. Takte = 10 s) ist Maschinenzeit; im Hub bei 1× Takt auch Echtzeit.
- Offen für K7: 8-Bit-Sauberkeit und Durchsatz mit echten Dateien, Dateinamenlänge (14 Zeichen), `G D`
  (läuft über `ls -l` des Gasts), Verhalten von `server` bei `SIGHUP` (Trennen der Verbindung mitten im Betrieb).

## 5. Client am WEGA (kein Gastcode)

Anmelden wie gewohnt (Konsole, tty0, tty2), `kermit`, dann gegen einen PC-Server (Leitung `tty3`, die
Druckerleitung des P8000, Hub-Kanal als Telnet-Client):

```
C-Kermit>set line /dev/tty3
C-Kermit>set speed 9600
C-Kermit>set file type binary
C-Kermit>send datei
C-Kermit>get datei
C-Kermit>remote directory
```

Hinweis: `kermit` ist setuid root für die Sperrdateien (`/usr/spool/uucp`).

## 6. Gegenprobe der PC-Bibliothek (AP K7, 2026-10-18)

Test `tests/system/test_p8000_wega_kermit_austausch.cpp` (`P8000WegaKermit.*`, Label `wega_install`), Messwerte
und Folgerungen: `doc/design/36_kermit.md` §6.5, Festlegungen `doc/merkposten/kermit.md`.  Zwei Fehler der
K1-Einrichtung, beide nur mit dem echten Austausch sichtbar und auf der Platte behoben (zweite Fassung):

1. **`REMOTE DIRECTORY` lieferte die Gebrauchsanweisung von kermit.**  C-Kermit führt `G D`/`G E`/`G T`/`G U`/
   `G W` über die **Login-Shell aus `/etc/passwd`** aus (`ckzunx.c` `zxcmd()`: `execl(pw_shell, "-c", "ls -l
   …")`) — mit Shell `/usr/bin/kermit` also `kermit -c "ls -l "`.  Jetzt: Shell `/bin/sh`, `.profile` startet
   kermit mit `exec` (Abmelden nach `exit`/`FINISH` wie vorher).
2. **Mit `set file names literal` gibt der Server keine Datei her** („Can't open file").  `ckfns.c` `sfile()`:
   ohne Sendenamen und bei `fncnv == 0` steht `strcpy(filnam,pktnam)` statt `strcpy(pktnam,filnam)` — der
   UNINITIALISIERTE Paketname überschreibt den Dateinamen.  Gestrichen; die Namen gehen nun konvertiert hinaus
   (`MixedCase.Txt` → `MIXEDCASE.TXT`), ankommende schreibt C-Kermit klein (`zrtol`).

Weitere Beobachtungen am laufenden Server: die Leitung tty0 löscht Bit 7 auf dem Weg PC → WEGA (8-Bit-Präfix
`&` nötig), WEGA → PC ist 8-Bit-sauber; empfangene Dateien gehören `wega` (setuid root, kein `setuid(getuid())`
vor `creat`), Modus 666; WEGA kürzt Namen über 14 Zeichen stumm (`einsehrlangername.bin` → `einsehrlangern`);
ein abgebrochenes Senden (Z mit „D") hinterlässt keine Datei; `remote cwd` am Client fragt nach einem Kennwort.


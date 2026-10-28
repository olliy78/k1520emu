# Gemeinsame Header fuer den C-Rueckbau des MUTOS-Kernels (AP K7a)

Eine Wahrheit fuer alle Module `c/<modul>.c`: V7-Stil (Glieder mit Vorsatz `p_`, `i_`, `u_` ... wegen des V7-`cc`, der Strukturglieder,
Tags und Variablen in EINEM Namensraum fuehrt, siehe `../../make/README.md`), nur K&R (kein `void`, keine Prototypen, kein `enum`,
`#define` statt `# define`).  Die Header liegen im Gast flach neben den Quellen (`gast_lauf.sh` kopiert `h/*.h` dorthin;
`#include "param.h"` genuegt).  Alles Globale ist `extern`, erst die Module definieren (`struct proc *slpque[64]` in `slp.c` usw.).
Namen und Reihenfolge nach V7/WEGA (`OlliL/P8000`, `WEGA/src/head/sys`); Abstaende und Groessen nur dort belegt, wo der Kernel sie zeigt.

Stufen: **belegt** = an Kernelcode gemessen (Zugriff mit Offset, durch bytegleiche Funktionen bestaetigt, Zahlen in `../../../../../doc/mutos_kernel/kernel_namen.md`),
**V7** = Reihenfolge nach V7, Groesse durch die Strukturlaenge gedeckt, **geraten** = nicht gemessen (Fuellfeld oder V7-Annahme, vor Gebrauch pruefen).

| Header | Belegt | Geraten / offen |
|--------|--------|-----------------|
| `types.h` | `dev_t int`, `daddr_t long`, `ino_t unsigned`, `time_t/off_t long`, Zeiger 16 Bit | `label_t` = 9 Worte (aus u_segflg bei +12h) |
| `param.h` | NPROC 30 (proc A11C..A464, K7c), MAXUPRC 15 (fork vergleicht mit 15, K7c), NOFILE 20, NINODE/NFILE 50, NBUF 20, NMOUNT 5, NCALL 9 (aus callout CE58..CE8E), HZ 50, BSIZE 512, NINDIR 128, PZERO 25, NZERO 20, PUSER 50, Fehlernummern, `major/minor/makedev` | NSIG 17 (psignal: `CP #11h`; u_signal[17]; kein SIGCLD/SIGPWR), MAXUPRC, SSIZE, CMAPSIZ, SMAPSIZ, NCARGS |
| `user.h` | `u` bei FC00; Abstaende u_segflg 12 .. u_cstime 100 (Liste im Kopf der Datei, mit Zahl der Zugriffe) | `u_rsav` (0..11), `u_signal[17]`, Fuellfeld `u_x54[14]` (ohne Zugriff), `u_x104` (Rest ab 104h: 104h 35 Zugriffe, 10Ah Signale, 112.. Profil) |
| `user.h` (K7c) | ab 104h aufgeloest: `u_ar0` 104 (Trap-Rahmen), `u_prof` 10A (Skalierung 110), `u_sep` 113, `u_ex[20]` 118 (Kopf der ausgefuehrten Datei, Magic E807/E707), `u_exname[14]` 140, `u_start` 14E, `u_cmask` 156 (umask), `u_fpsaved` 15A, `u_fparea` 15C..; `u_tsize/u_dsize/u_ssize` unsigned (SLL/SRL im Kernel) | `u_x106`, `u_x112`, `u_x114`, `u_x152`, `u_x158` (ohne Zugriff), Namen der u_ex-Worte (nur Indizes: 0 mag, 1/2/3 tsize/dsize/bsize klein, 5 Einsprung klein, 8 Einsprung gross, 14/15/16 gross) |
| `proc.h` | alle Felder bis `p_link`, 28 Byte; `p_sig` ist EIN Wort (+06, psignal), `p_uid` +08, `p_pgrp` +0A (newproc/wait) -- korrigiert gegenueber `kernel_namen.md` | `p_textp`, `p_clktim` (V7/WEGA-Reihenfolge) |
| `inode.h` | 74 Byte; `i_flag`, `i_count`, `i_dev`, `i_number`, `i_uid` (+0A), `i_addr[13]` je 4 Byte (+12), `i_lastr` (+46) | `i_mode`, `i_nlink`, `i_gid`, `i_size` (V7-Reihenfolge) |
| `file.h` | 8 Byte; `f_flag`, `f_count`, `f_un` (+4; erstes Wort `f_chan`, V7-Multiplex; closef reicht es an close weiter) | `f_inode` (+2) |
| `buf.h` | 24 Byte, alle Felder; `b_resid` bei +16h (readi: `BSIZE - b_resid`) | `b_pad15` |
| `conf.h` | `bdevsw` 8 Byte (open, close, strategy, d_tab), `cdevsw` 14 Byte (open, close, read, write, ioctl, stop, tty), `devtab` nach V7 (b_forw/b_back wie buf) | Aufbau `devtab` jenseits `b_forw/b_back` |
| `filsys.h` | `s_ronly` bei +19Dh (aus access), Abstaende bis dorthin | Felder dahinter |
| `mount.h` | 6 Byte: `m_dev`, `m_bufp`, `m_inodp` | Reihenfolge der letzten zwei (V7) |
| `text.h` | x_daddr 0, x_caddr 2, x_size 4, x_iptr 6, x_count 8, x_ccount 9, x_flag 0Ah (aus swapin/sched) | Rest; xlock/xunlock... sind Stubs |
| `callo.h` | c_time 0, c_arg 2, c_func 4 (aus timeout) | |
| `systm.h` | Adressen im Kopf der Datei | `sysent`-Feldnamen |
| `dir.h`, `map.h`, `ino.h` | dir: 16 Byte | map, ino: V7 |
| `tty.h`, `reg.h` | nichts | **Platzhalter**, Platzhalter-Struktur nach V7; Module `tty`/`trap` muessen messen |

Verwendung im Modul: Kopf `#include "param.h" "systm.h" "dir.h" "user.h" ...` in V7-Reihenfolge; `proc.h`, `inode.h` usw. vor `user.h`
nur, wenn `user.h` deren Struktur braucht -- Zeiger auf unvollstaendige Strukturen genuegen (`struct proc *u_procp`).
Neue Felder: erst im Kernel messen, dann hier das Fuellfeld ersetzen und diese Tabelle fuehren; **nie** ein Feld "raten und stehen lassen".

## K7b (2026-10-28): Aenderungen an `buf.h`, `inode.h`, neu `user_ext.h`
* `buf.h`: **`b_error` liegt bei +15, nicht +14** (geterror `15(bp)`, physio `clrb 15`); +14 ist `b_xmem` (physio/swap: Segmentbyte von `b_addr`); `B_TAPE` 02000 (bdwrite); `b_resid` jetzt belegt (readi, physio).  Stufe: belegt.
* `inode.h`: `NADDR` 13 (itrunc/iexpand), `PIPSIZ` 4096 (writep).  Stufe: belegt.
* `user_ext.h` (neu, nur Zugriffe auf feste Abstaende von `u`, keine Strukturaenderung): `U_CMASK` u+156 (umask), `U_SEP` u+113 (Byte), `U_P104` u+104 (Zeiger).  K7c hat `user.h` inzwischen um `u_ar0` (104) und `u_x156` erweitert -- sobald diese Namen feststehen, die drei Makros ersetzen.
* `conf.h`, `filsys.h`, `mount.h`, `dir.h`, `ino.h`: unveraendert; durch iget/alloc/nami/getfs/update bestaetigt (filsys: s_nfree +6, s_free +8, s_ninode +D0, s_inode +D2, s_flock +19A, s_ilock +19B, s_fmod +19C, s_ronly +19D, s_time +19E, s_tfree +1A2, s_tinode +1A6; dinode 64 Byte mit atime +34h, mtime +38h, ctime +3Ch).

## K7d (2026-10-28): neu `mbx.h`, `tty.h` jetzt gemessen, `user_ext.h` um zwei Zeilen
* `mbx.h` (neu): Mailbox-Eintrag zur U880-Seite, 14 Byte -- `mb_cmd` 0 (Platte 0, Drucker 2, Terminal 4), `mb_unit` 1, `mb_dir` 2, `mb_stat` 3, `mb_seg` 4 (Wort), `mb_addr` 6, `mb_cnt` 8, `mb_blk` A (long).
  Belegt durch rkstart (Platte, fest bei D532), ttxfer/putchar (je Leitung ab D26A), lpxfer (ab D516), fd (D558).  Bei tty/lp ist `mb_stat` zugleich "Leitung bereit".
* `tty.h` (jetzt belegt statt Platzhalter): 60-Byte-`struct tty` (A530, 4 Leitungen) mit **einer** Eingabewarteschlange `t_inq` (+0), `t_outq` (+6), `t_oproc` (+C), `t_pgrp` +10, `t_obuf` +12,
  `t_dev` +14, Zeichen `t_erase`/`t_kill` +18/+19, `t_flags` +1A, `t_eof`..`t_brkc` +20..+27, `t_state` +28, `t_lstate` +2A, `t_cnt2c` +2C, `t_delct` +2E, `t_ocnt` +36, `t_hwp` +38, `t_hwf` +3A.
  Geraten/ungeklaert: +0E, +16, +1C..+1F, +30..+35 (Zugriffe nur in ttyinput/ttioctl, die noch nicht zurueckgebaut sind).  Das ist der System-III/ZEUS-Aufbau, **nicht** der V7-Platzhalter von K7a (der alte Inhalt ist ersetzt).
  `struct clist` steht jetzt in `tty.h` (pipe.c hat eine eigene gleichlautende Fassung, bitte nicht beide einbinden).
* `user_ext.h`: `U_TTYP` (u+114) und `U_TTYD` (u+116) fuer syopen/ttyopen (Kontrollterminal).  **Falle des Gast-`cpp`:** Namen sind nur auf 8 Zeichen signifikant -- ein Include-Waechter `USER_EXT_K7D`
  kollidierte mit `USER_EXT_H` und schaltete den ganzen Zusatz still ab ("U_TTYP undefined").  Waechter daher kurz und eindeutig (`UXK7D_H`).
* `conf.h` unveraendert (bdevsw 8 Byte, cdevsw 14 Byte durch conf.c bestaetigt: Datenbytes 8766..88C5 gleich).

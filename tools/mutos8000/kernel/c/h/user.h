/* user.h -- die user-Struktur (u), fest bei FC00 (u + Kernelstapel, USIZE 0x400 Byte).
 * Abstaende aus den Zugriffen im Kernel belegt (Zahl der Zugriffe in Klammern), Reihenfolge nach V7/WEGA:
 *   u_segflg 12(11) u_error 13(122) u_deverr 14(1) u_uid 16(20) u_gid 18(8) u_ruid 1A(5) u_rgid 1C(4) u_procp 1E(45)
 *   u_ap 20(33) u_r 22 (r_val1 22, r_val2 24; als long ueberlagert) u_base 26(24) u_count 28(41) u_offset 2A(36)
 *   u_cdir 2E(6) u_rdir 30(8) u_dbuf 32 u_dirp 40(10) u_dent 42 (d_ino 42, d_name 44) u_pdir 52(8)
 *   u_ofile 62 (Index mit CP #20) u_pofile 8A u_arg 9E u_tsize A8 u_dsize AA u_ssize AC u_qsav AE u_ssav C0
 *   u_signal D2 u_utime F4 u_stime F8 u_cutime FC u_cstime 100.
 * GERATEN: u_rsav (0..11, 9 Worte, aus u_segflg bei 12 gefolgert), u_signal[17] (aus dem Platz D2..F3), die
 * Fuellfelder u_x54 (14 Byte ohne Zugriff) und u_x104 (Rest ab 104h, 35 Zugriffe auf 104 -- vermutlich u_state --,
 * 10A Signalzustellung, 112.. Profil u. a.: noch keinem Namen zugeordnet).  Nur nichtsegmentierter Aufbau.
 * K7c: ab 104 jetzt u_ar0 (104), u_prof (10A, Skalierung 110), u_x156, u_fpsaved (15A), u_fparea (15C..).
 * Gliedernamen tragen Praefix u_ (V7-cc: ein Namensraum fuer Glieder und Variablen). */
#ifndef USER_H
#define USER_H
#include "param.h"
#include "dir.h"
struct user {
	label_t		u_rsav;		/* 000 geraten: 18 Byte */
	char		u_segflg;	/* 012 0: Nutzer-D, 1: System, 2: Nutzer-I */
	char		u_error;	/* 013 */
	char		u_deverr;	/* 014 (einmal benutzt) */
	char		u_x15;		/* 015 Fuellfeld */
	int		u_uid;		/* 016 */
	int		u_gid;		/* 018 */
	int		u_ruid;		/* 01A */
	int		u_rgid;		/* 01C */
	struct proc	*u_procp;	/* 01E */
	int		*u_ap;		/* 020 */
	union {				/* 022 Rueckgabewerte des Systemaufrufs */
		struct {
			int	r_val1;
			int	r_val2;
		};
		off_t	r_off;
		time_t	r_time;
	} u_r;
	caddr_t		u_base;		/* 026 */
	unsigned	u_count;	/* 028 */
	off_t		u_offset;	/* 02A */
	struct inode	*u_cdir;	/* 02E */
	struct inode	*u_rdir;	/* 030 */
	char		u_dbuf[DIRSIZ];	/* 032 */
	caddr_t		u_dirp;		/* 040 */
	struct direct	u_dent;		/* 042 */
	struct inode	*u_pdir;	/* 052 */
	char		u_x54[14];	/* 054 Fuellfeld, nichts zugeordnet */
	struct file	*u_ofile[NOFILE];	/* 062 */
	char		u_pofile[NOFILE];	/* 08A */
	int		u_arg[5];	/* 09E */
	unsigned	u_tsize;	/* 0A8 */
	unsigned	u_dsize;	/* 0AA */
	unsigned	u_ssize;	/* 0AC */
	label_t		u_qsav;		/* 0AE */
	label_t		u_ssav;		/* 0C0 */
	int		u_signal[17];	/* 0D2 geraten: 17 Eintraege */
	time_t		u_utime;	/* 0F4 */
	time_t		u_stime;	/* 0F8 */
	time_t		u_cutime;	/* 0FC */
	time_t		u_cstime;	/* 100 */
	int		*u_ar0;		/* 104 Zeiger auf den Registerrahmen des Traps (trap.c, sendsig; K7c) */
	char		u_x106[4];	/* 106 ohne Zugriff */
	struct {			/* 10A Profil (trap: addupc(pc, &u_prof, ticks)); Skalierung bei 110 ist belegt */
		int		*pr_base;
		unsigned	pr_size;
		unsigned	pr_off;
		unsigned	pr_scale;
	} u_prof;
	char		u_x112;		/* 112 */
	char		u_sep;		/* 113 Byte, exec (getxfile) */
	char		u_x114[4];	/* 114..117 */
	int		u_ex[20];	/* 118 u_exdata: Kopf der ausgefuehrten Datei (Magic E807/E707); Worte 118 (mag), 11A, 11C, 11E, 122 (Einsprung),
					   128 (Einsprung grosser Kopf), 134/136/138 (Text/Daten/BSS des grossen Kopfes) */
	char		u_exname[14];	/* 140 Kommandoname (exec kopiert ihn nach u_dbuf) */
	time_t		u_start;	/* 14E Startzeit (fork) */
	char		u_x152[4];	/* 152..155 */
	int		u_cmask;	/* 156 umask (V7 u_cmask; main loescht es, sys4.c umask) */
	int		u_x158;		/* 158 ohne Zugriff */
	int		u_fpsaved;	/* 15A Gleitpunktzustand gesichert (trap, swtch, newproc: savfp(u_fparea)) */
	char		u_fparea[0x3FC - 0x15C];	/* 15C.. bis zum Stapel */
};
extern struct user	u;			/* FC00 */
/* Fehlernummern: belegt EPERM 1, EBADF 9, ENFILE 23, EMFILE 24, ENOMEM 12; Rest V7 (nicht einzeln gemessen). */
#define EPERM	1
#define ENOENT	2
#define ESRCH	3
#define EINTR	4
#define EIO	5
#define ENXIO	6
#define E2BIG	7
#define ENOEXEC	8
#define EBADF	9
#define ECHILD	10
#define EAGAIN	11
#define ENOMEM	12
#define EACCES	13
#define EFAULT	14
#define ENOTBLK	15
#define EBUSY	16
#define EEXIST	17
#define EXDEV	18
#define ENODEV	19
#define ENOTDIR	20
#define EISDIR	21
#define EINVAL	22
#define ENFILE	23
#define EMFILE	24
#define ENOTTY	25
#define ETXTBSY	26
#define EFBIG	27
#define ENOSPC	28
#define ESPIPE	29
#define EROFS	30
#define EMLINK	31
#define EPIPE	32
#define EDOM	33
#define ERANGE	34
#endif

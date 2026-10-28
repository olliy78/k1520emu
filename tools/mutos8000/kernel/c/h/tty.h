/* tty.h -- tty-Struktur des MUTOS-Kernels (K7d), 60 (3CH) Byte, vier Stueck ab A530 (ttyt), Ausgabepuffer 80 Byte je Leitung ab D2A2,
 * Mailbox-Eintrag je Leitung ab D26A (h/mbx.h).  Aus den Zugriffen von sioopen/ttread/ttwrite/ttystart/ttyflush vermessen; das ist ein
 * System-III/ZEUS-Aufbau mit EINER Eingabewarteschlange (t_inq, kanonische Verarbeitung im Eingabeweg), nicht der V7-Aufbau von K7a.
 *   +00 t_inq (clist: c_cc 0, c_cf 2, c_cl 4)   +06 t_outq (clist)   +0C t_oproc (ttstart)
 *   +10 t_pgrp   +12 t_obuf   +14 t_dev   +18 t_erase (7F)   +19 t_kill (15H)   +1A t_flags (ECHO 8, CRMOD 10H, RAW 20H; Vorgabe 518H)
 *   +20 t_eof +21 t_eol  +22 t_intrc (03) +23 t_quitc (1C) +24 t_startc (11) +25 t_stopc (13) +26 t_suspc (1A) +27 t_brkc (FF)
 *   +28 t_state (TIMEOUT 1, WOPEN 2, ISOPEN 4, ... 10H Verbindung, 80H exklusiv, 100H Ausgabe angehalten, 400H gestoppt)
 *   +2A t_lstate (Bit 0 Eingabe im Gange, Bit 1 Ausgabe wartet auf 0x80-Zeichen)  +2C, +2E: Zaehler/Merker  +36 t_ocnt  +38 t_hwp  +3A t_hwf
 * GERATEN/ungeklaert: +0E, +16, +1C..+1F, +30..+35 (Fuellfelder; Zugriffe nur in ttyinput/ttioctl, noch nicht zurueckgebaut). */
#ifndef TTY_H
#define TTY_H
#include "types.h"
#include "mbx.h"
struct clist {
	int	c_cc;
	char	*c_cf;
	char	*c_cl;
};
struct tty {
	struct clist	t_inq;		/* 00 */
	struct clist	t_outq;		/* 06 */
	int		(*t_oproc)();	/* 0C */
	int		t_x0e;		/* 0E */
	int		t_pgrp;		/* 10 */
	char		*t_obuf;	/* 12 */
	dev_t		t_dev;		/* 14 */
	int		t_x16;		/* 16 */
	char		t_erase;	/* 18 */
	char		t_kill;		/* 19 */
	int		t_flags;	/* 1A */
	int		t_x1c;		/* 1C */
	int		t_x1e;		/* 1E */
	char		t_eof;		/* 20 */
	char		t_eol;		/* 21 */
	char		t_intrc;	/* 22 */
	char		t_quitc;	/* 23 */
	char		t_startc;	/* 24 */
	char		t_stopc;	/* 25 */
	char		t_suspc;	/* 26 */
	char		t_brkc;		/* 27 */
	int		t_state;	/* 28 */
	int		t_lstate;	/* 2A */
	char		t_cnt2c;	/* 2C */
	char		t_x2d;		/* 2D */
	char		t_delct;	/* 2E */
	char		t_x2f;		/* 2F */
	char		t_x30[6];	/* 30 */
	int		t_ocnt;		/* 36 */
	struct rkmbx	*t_hwp;		/* 38 */
	char		t_hwf;		/* 3A */
	char		t_x3b;		/* 3B */
};
extern struct tty ttyt[4];		/* A530 */
#define TIMEOUT	01
#define WOPEN	02
#define ISOPEN	04
#define CARR_ON	020
#define XCLUDE	0200
#define ENOTTY	25
#define EBUSY_	16
#endif

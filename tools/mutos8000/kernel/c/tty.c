/* tty.c -- Terminalleitungen ueber die Mailbox (Kernel mutos_1216, 60CC..6432 und 6D44..7E16 soweit zurueckgebaut).
 * Vier Leitungen (Major 0, cdevsw[0]); Ausgabe/Eingabe laufen ueber Mailbox-Eintraege je Leitung (ttyhw, D26A, 14 Byte) zur U880-Seite,
 * die Zeichen kommen mit Aufruf von tcrint(dev, zeichen<<8) herein.  Ausgabepuffer ttyobuf D2A2 (80 Byte je Leitung).
 * Handassembler: nichts hier (getc/putc stehen in pipe.c).  Bildschirmprotokoll der Konsole: conptr 8C96 (Ring 8DE8..8E68).
 * Struktur der tty: h/tty.h.  Namen der Wrapper (60CC..623C) frei vergeben (tc...), die V7-Namen tragen die Kernfunktionen. */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "user_ext.h"
#include "inode.h"
#include "proc.h"
#include "file.h"
#include "tty.h"

extern struct rkmbx ttyhw[4];		/* D26A */
extern char ttyobuf[4][80];		/* D2A2 */
extern char *conptr;			/* 8C96 */
extern char conlog[];			/* 8DE8 */
extern char conend[];			/* 8E68 */
int ttstart(), ttrstrt();

/* KERNEL 60CC */
tcopen(dev, flag)
dev_t dev;
{
	register struct tty *tp;
	register struct rkmbx *hp;
	register d;

	d = minor(dev);
	if (d >= 4) {
		u.u_error = ENXIO;
		return;
	}
	tp = &ttyt[d];
	tp->t_obuf = ttyobuf[d];
	hp = &ttyhw[d];
	tp->t_hwp = hp;
	tp->t_hwf = 0;
	tp->t_oproc = ttstart;
	if ((tp->t_state & ISOPEN) == 0) {
		tp->t_state = 0x14;
		tp->t_flags = 0x518;
		if (d)
			hp->mb_stat = 1;
		ttinit(tp);
	} else if ((tp->t_state & XCLUDE) && u.u_uid) {
		u.u_error = EBUSY;
		return;
	}
	ttyopen(dev, tp);
}

/* KERNEL 616C */
tcclose(dev)
dev_t dev;
{
	register struct tty *tp;

	tp = &ttyt[minor(dev)];
	ttyclose(tp);
}

/* KERNEL 618E */
tcread(dev)
dev_t dev;
{
	ttread(&ttyt[minor(dev)]);
}

/* KERNEL 61B0 */
tcwrite(dev)
dev_t dev;
{
	ttwrite(&ttyt[minor(dev)]);
}

/* KERNEL 61D2 */
tcxint(dev)
dev_t dev;
{
	register struct tty *tp;

	tp = &ttyt[minor(dev)];
	ttkick(tp);
	if (tp->t_outq.c_cc == 0 || tp->t_outq.c_cc <= 50)
		wakeup(&tp->t_outq);
}

/* KERNEL 620E */
tcrint(dev, ch)
dev_t dev;
register unsigned ch;
{
	register c;
	register struct tty *tp;

	tp = &ttyt[minor(dev)];
	c = ch >> 8;
	ttyinput(c, tp);
}

/* KERNEL 623C */
tcioctl(dev, cmd, addr, flag)
dev_t dev;
{
	if (ttioctl(cmd, &ttyt[minor(dev)], addr, dev) == 0)
		u.u_error = ENOTTY;
}

/* KERNEL 6274 */
ttstart(tp)
register struct tty *tp;
{
	register c, n;
	register char *cp;
	register struct rkmbx *hp;

	hp = tp->t_hwp;
	cp = tp->t_obuf;
	if (hp->mb_stat == 0 || (tp->t_state & 0x100) || (tp->t_lstate & 2))
		return;
	n = 0;
	while (n < 0x50) {
		if ((c = getc(&tp->t_outq)) < 0)
			break;
		if (tp->t_flags & 0x20)
			*cp++ = c & 0x7F;
		else if (c <= 0x7F)
			*cp++ = c;
		else if (c == 0x80) {
			tp->t_lstate |= 2;
			break;
		} else {
			timeout(ttrstrt, tp, (c & 0x7F) + 4);
			tp->t_state |= TIMEOUT;
			break;
		}
		if (n++ > 40 && c == '\n')
			break;
	}
	if ((tp->t_ocnt = n) > 0)
		ttxfer(tp);
	if (c < 0 && (tp->t_state & 0x40)) {
		tp->t_state &= ~0x40;
		wakeup(&tp->t_outq);
	}
}

/* KERNEL 633A */
ttxfer(tp)
register struct tty *tp;
{
	register struct rkmbx *hp;

	hp = tp->t_hwp;
	hp->mb_cmd = 4;
	hp->mb_unit = minor(tp->t_dev);
	hp->mb_stat = 0;
	hp->mb_seg = 0;
	hp->mb_addr = tp->t_obuf;
	hp->mb_cnt = tp->t_ocnt;
	hp->mb_dir = 1;
	mapalloc(hp, 4);
}

/* KERNEL 6384 */
putchar(c)
register c;
{
	register s, t;
	register struct rkmbx *hp;
	register char *bp;

	if (c) {
		*conptr++ = c;
		if (conptr >= conend)
			conptr = conlog;
	}
	hp = &ttyhw[0];
	bp = ttyobuf[0];
	t = 10000;
	do {
		if (hp->mb_stat)
			break;
	} while (--t != 0);
	if (c == 0)
		return;
	s = splx(7);
	hp->mb_cmd = 4;
	hp->mb_unit = 0;
	hp->mb_stat = 0;
	hp->mb_seg = 0;
	hp->mb_addr = bp;
	hp->mb_cnt = 1;
	hp->mb_dir = 0x81;
	*bp = c;
	if (c == '\n') {
		*++bp = '\r';
		hp->mb_cnt = 2;
	}
	mapalloc(hp, 4);
	putchar(0);
	hp->mb_stat = 1;
	splx(s);
}

/* KERNEL 6432 */
ttyinit()
{
	register struct rkmbx *hp;

	hp = &ttyhw[0];
	hp->mb_stat = 1;
}

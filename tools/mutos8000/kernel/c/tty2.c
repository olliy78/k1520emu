/* tty2.c -- tty-Kern (Kernel mutos_1216, 6D44..6E7A, 715C..71F2, 7948..7A34 ohne Eingabe/Ausgabeweg, 7C4A..7E16):
 * ttyopen, ttinit, ttyclose, ioctl (Systemaufruf 54), ttywait, ttyflush, bitset, ttyspec, ttflow, ttrstrt, ttkick, ttread, ttwrite.
 * Noch nicht zurueckgebaut (Assembler-Vorlage in kernel/s/tty.s, mbx.s): ttioctl 6E7A (738 B), ttyinput 7222 (912 B), 75B2, 769A, 772E, 78A8,
 * ttyoutput 7A34 (534 B).  Namen: V7/System III. */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "user_ext.h"
#include "inode.h"
#include "proc.h"
#include "file.h"
#include "conf.h"
#include "tty.h"

#define ASLEEP	0x40

/* KERNEL 6D44 */
ttyopen(dev, tp)
dev_t dev;
register struct tty *tp;
{
	register struct proc *pp;

	pp = u.u_procp;
	tp->t_dev = dev;
	if (pp->p_pgrp == 0) {
		U_TTYP = (int)tp;
		U_TTYD = dev;
		if (tp->t_pgrp == 0)
			tp->t_pgrp = pp->p_pid;
		pp->p_pgrp = tp->t_pgrp;
	}
	tp->t_state &= ~WOPEN;
	tp->t_state |= ISOPEN;
}

/* KERNEL 6D8C */
ttinit(tp)
register struct tty *tp;
{
	tp->t_intrc = 03;
	tp->t_quitc = 034;
	tp->t_startc = 021;
	tp->t_stopc = 023;
	tp->t_suspc = 032;
	tp->t_brkc = 0377;
	tp->t_erase = 0177;
	tp->t_kill = 025;
	tp->t_eof = 0;
	tp->t_eol = 0;
}

/* KERNEL 6DD0 */
ttyclose(tp)
register struct tty *tp;
{
	tp->t_pgrp = 0;
	ttywait(tp);
	tp->t_state = 0;
	tp->t_lstate = 0;
}

struct a {
	int	fdes;
	int	cmd;
	caddr_t	cmarg;
};

/* KERNEL 6DF0 */
ioctl()
{
	register struct file *fp;
	register struct inode *ip;
	register struct a *uap;
	register dev, fmt;

	uap = (struct a *)u.u_ap;
	if ((fp = getf(uap->fdes)) == NULL)
		return;
	if (uap->cmd == 0x6601) {
		u.u_pofile[uap->fdes] |= 1;
		return;
	}
	if (uap->cmd == 0x6602) {
		u.u_pofile[uap->fdes] &= ~1;
		return;
	}
	ip = fp->f_inode;
	fmt = ip->i_mode & 0xF000;
	if (fmt != 0x2000 && fmt != 0x3000) {
		u.u_error = ENOTTY;
		return;
	}
	dev = ip->i_addr[0];
	(*cdevsw[major(dev)].d_ioctl)(dev, uap->cmd, uap->cmarg, fp->f_flag);
}

/* KERNEL 715C */
ttywait(tp)
register struct tty *tp;
{
	spl5();
	while (tp->t_outq.c_cc && (tp->t_state & CARR_ON)) {
		(*tp->t_oproc)(tp);
		tp->t_state |= ASLEEP;
		sleep(&tp->t_outq, 29);
	}
	ttyflush(tp);
	spl0();
}

/* KERNEL 71A4 */
ttyflush(tp)
register struct tty *tp;
{
	register s;

	wakeup(tp);
	wakeup(&tp->t_outq);
	s = spl6();
	tp->t_state &= ~0x100;
	tp->t_lstate &= ~0x1E;
	(*cdevsw[major(tp->t_dev)].d_stop)(tp);
	while (getc(&tp->t_outq) >= 0)
		;
	while (getc(tp) >= 0)
		;
	tp->t_delct = 0;
	tp->t_cnt2c = 0;
	splx(s);
}

/* KERNEL 7948 */
bitset(map, n)
char *map;
register n;
{
	if (n < 0)
		return(0);
	return(map[n >> 3] & (1 << (n & 7)));
}

/* KERNEL 797A */
ttyspec(c, tp)
register c;
register struct tty *tp;
{
	register char *p;
	register unsigned i;

	if (c < 0)
		return(0);
	p = &tp->t_intrc;
	for (i = 0; i < 6; i++)
		if (*p++ == c)
			return(1);
	if (tp->t_erase == c)
		return(1);
	if (tp->t_kill == c)
		return(1);
	switch (c) {
	case 0x10:
		if (tp->t_eol == 0)
			break;
	case 0x12:
	case 0x17:
		return(1);
	}
	return(0);
}

/* KERNEL 79E0 */
ttflow(tp)
register struct tty *tp;
{
	register n;

	n = tp->t_inq.c_cc;
	if (n > 0x100) {
		ttyflush(tp);
		tp->t_state &= ~0x400;
	}
	if (n >= 0x80 && (tp->t_state & 0x400) == 0) {
		if (putc(tp->t_stopc, &tp->t_outq) == 0) {
			tp->t_state |= 0x400;
			ttkick(tp);
		}
	}
}

/* KERNEL 7C4A */
ttrstrt(tp)
register struct tty *tp;
{
	tp->t_state &= ~TIMEOUT;
	ttkick(tp);
}

/* KERNEL 7C62 */
ttkick(tp)
register struct tty *tp;
{
	register s;

	s = spl5();
	if ((tp->t_state & 0x121) == 0)
		(*tp->t_oproc)(tp);
	splx(s);
}

/* KERNEL 7C90 */
ttread(tp)
register struct tty *tp;
{
	register c;

	if ((tp->t_state & CARR_ON) == 0)
		return(0);
	spl5();
	goto top;
chk:
	if (tp->t_inq.c_cc != 0)
		goto have;
wt:
	if ((tp->t_state & CARR_ON) == 0)
		goto out;
	sleep(tp, 28);
top:
	if ((tp->t_flags & 0x22) == 0 && tp->t_delct == 0)
		goto wt;
	if (tp->t_flags & 0x22)
		goto chk;
have:
	spl0();
	if (tp->t_flags & 0x22) {
		while (tp->t_inq.c_cc && passc(getc(tp)) >= 0)
			;
	} else {
		do {
			c = getc(tp);
			if (c == 0xFF)
				break;
		} while (passc(c) >= 0);
		if (c != 0xFF && (*tp->t_inq.c_cf & 0xFF) == 0xFF)
			c = getc(tp);
		if (c == 0xFF)
			tp->t_delct--;
		if (tp->t_eol) {
			tp->t_lstate |= 1;
			tp->t_lstate &= ~6;
			tp->t_cnt2c = 0;
		} else
			tp->t_lstate &= ~1;
	}
	if ((tp->t_state & 0x400) && tp->t_inq.c_cc < 0x33) {
		if (putc(tp->t_startc, &tp->t_outq) == 0) {
			tp->t_state &= ~0x400;
			ttkick(tp);
		}
	}
out:
	return(tp->t_inq.c_cc);
}

/* KERNEL 7DA6 */
ttwrite(tp)
register struct tty *tp;
{
	register c;

	if ((tp->t_state & CARR_ON) == 0)
		return(0);
	goto cnt;
slp:
	ttkick(tp);
	tp->t_state |= ASLEEP;
	sleep(&tp->t_outq, 29);
	goto top;
put:
	ttyoutput(c, tp);
cnt:
	if (u.u_count == 0)
		goto fin;
	spl5();
top:
	if (tp->t_outq.c_cc > 100 || (tp->t_lstate & 4))
		goto slp;
	spl0();
	c = cpass();
	if (c >= 0)
		goto put;
fin:
	ttkick(tp);
	return(0);
}

/* tty2.c -- tty-Kern (Kernel mutos_1216, Modul 6D44..7E16): ttyopen, ttinit, ttyclose, ioctl (Systemaufruf 54), ttioctl, ttywait, ttyflush,
 * ttyinput (samt ttycol, ttyecho, ttyrub, unputc), bitset, ttyspec, ttflow, ttyoutput, ttrstrt, ttkick, ttread, ttwrite.
 * K7e: ttioctl, ttyinput + Hilfsfunktionen, ttyoutput und die Tabellen (Zeichen-Abbildung 8C9A, Steuerzeichen 8D1A, Wortzeichen 8D2A,
 * Paare 8D52) kamen dazu.  Namen: V7/System III; ttycol/ttyecho/ttyrub/unputc und die Tabellennamen frei vergeben.
 * Die Sprungtabellen der switch-Anweisungen (8D3A ttioctl, 8D46 ttyoutput) erzeugt der Compiler selbst (Hash ueber cmd % 6 bzw. dichte Tabelle). */
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

struct cblock {
	struct cblock	*c_next;
	char		c_info[14];
};
extern struct cblock *cfreelist;	/* D266 */
extern int cf_x882;			/* 8882 (conf.c): Zahl der Leitungsdisziplinen */
extern char ttylcpr[];

/* Tabellen (Datenbereich 8C9A..8D2A): Eingabeabbildung bei LCASE (Paritaetsbit 0x80 darin, Maske 0177 benutzt), Steuerzeichen (Bitmenge), Wortzeichen */
char ttylcmap[128] = {
	0x00, 0x80, 0x80, 0x00, 0x80, 0x00, 0x00, 0x80,
	0x80, 0x00, 0x00, 0x80, 0x00, 0x80, 0x80, 0x00,
	0x80, 0x00, 0x00, 0x80, 0x00, 0x80, 0x80, 0x00,
	0x00, 0x80, 0x80, 0x00, 0x80, 0x00, 0x00, 0x80,
	0x80, 0x7C, 0x00, 0x80, 0x00, 0x80, 0x80, 0x60,
	0x7B, 0xFD, 0x80, 0x00, 0x80, 0x00, 0x00, 0x80,
	0x00, 0x80, 0x80, 0x00, 0x80, 0x00, 0x00, 0x80,
	0x80, 0x00, 0x00, 0x80, 0x00, 0x80, 0x80, 0x00,
	0x80, 0x00, 0x00, 0x80, 0x00, 0x80, 0x80, 0x00,
	0x00, 0x80, 0x80, 0x00, 0x80, 0x00, 0x00, 0x80,
	0x00, 0x80, 0x80, 0x00, 0x80, 0x00, 0x00, 0x80,
	0x80, 0x00, 0x00, 0x80, 0x5C, 0x80, 0xFE, 0x00,
	0x00, 0xC1, 0xC2, 0x43, 0xC4, 0x45, 0x46, 0xC7,
	0xC8, 0x49, 0x4A, 0xCB, 0x4C, 0xCD, 0xCE, 0x4F,
	0xD0, 0x51, 0x52, 0xD3, 0x54, 0xD5, 0xD6, 0x57,
	0x58, 0xD9, 0xDA, 0x00, 0x80, 0x00, 0x00, 0x80,
};
char ttyctl[16] = {
	0x7F, 0xC0, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
};
char ttyword[16] = {
	0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0xFF, 0x03,
	0xFE, 0xFF, 0xFF, 0x07, 0xFE, 0xFF, 0xFF, 0x07,
};


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
	tp->t_eofc = 032;
	tp->t_brkc = 0377;
	tp->t_erase = 0177;
	tp->t_kill = 025;
	tp->t_width = 0;
	tp->t_page = 0;
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

/* KERNEL 6E7A */
ttioctl(com, tp, arg, flag)
register struct tty *tp;
caddr_t arg;
{
	register x;
	register char *q;
	unsigned a;

	switch (com) {

	case 0x7400:
		a = tp->t_line;
		if (copyout((caddr_t)&a, arg, 2))
			u.u_error = EFAULT;
		return(1);

	case 0x7401:
		if (copyin(arg, (caddr_t)&a, 2)) {
			u.u_error = EFAULT;
			return(1);
		}
		if (a >= cf_x882) {
			u.u_error = ENXIO;
			return(1);
		}
		if (tp->t_line)
			(*linesw[tp->t_line].l_close)(tp);
		if (a)
			(*linesw[a].l_open)(flag, tp, arg);
		if (u.u_error)
			return(1);
		tp->t_line = a;
		return(1);

	case 0x740D:
		tp->t_state |= XCLUDE;
		return(1);

	case 0x740E:
		tp->t_state &= ~XCLUDE;
		return(1);

	case 0x7409:
		ttywait(tp);
	case 0x740A:
		if (copyin(arg, (caddr_t)&tp->t_x16, 6))
			u.u_error = EFAULT;
		return(1);

	case 0x7408:
		if (copyout((caddr_t)&tp->t_x16, arg, 6))
			u.u_error = EFAULT;
		return(1);

	case 0x740B:
		ttywait(tp);
		x = tp->t_page;
		if (copyin(arg, (caddr_t)&tp->t_x16, 12)) {
			u.u_error = EFAULT;
			return(1);
		}
		tp->t_page &= 0177;
		if (tp->t_page != x) {
			if (tp->t_page)
				tp->t_lstate |= 1;
			else
				tp->t_lstate &= ~1;
			tp->t_cnt2c = 0;
		}
		return(1);

	case 0x740C:
		if (copyout((caddr_t)&tp->t_x16, arg, 12))
			u.u_error = EFAULT;
		return(1);

	case 0x7402:
		tp->t_state |= 0x200;
		return(1);

	case 0x740F:
		a = 0;
		if (tp->t_flags & (RAW|CBREAK))
			a = tp->t_inq.c_cc;
		else if (tp->t_delct) {
			q = tp->t_inq.c_cf;
			while ((*q++ & 0377) != 0377)
				a++;
		}
		if (copyout((caddr_t)&a, arg, 2))
			u.u_error = EFAULT;
		return(1);

	case 0x7410:
		ttyflush(tp);
		return(1);

	case 0x6408:
	case 0x6409:
		(*linesw[tp->t_line].l_ioctl)(com, tp, arg);
		return(1);

	case 0x7411:
		if (copyin(arg, (caddr_t)&tp->t_intrc, 6))
			u.u_error = EFAULT;
		return(1);

	case 0x7412:
		if (copyout((caddr_t)&tp->t_intrc, arg, 6))
			u.u_error = EFAULT;
		return(1);
	}
	return(0);
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

/* KERNEL 7222 */
ttyinput(c, tp)
register c;
register struct tty *tp;
{
	register flg;

	c &= 0377;
	if ((tp->t_flags & RAW) == 0)
		c &= 0177;
	flg = tp->t_flags;
	if (flg & TANDEM)
		ttflow(tp);
	if ((flg & RAW) == 0 || (flg & (RAW|CBREAK)) == (RAW|CBREAK)) {
		if (tp->t_state & 0x100) {
			if (tp->t_startc == c) {
				tp->t_state &= ~0x100;
				goto kick;
			}
			if (tp->t_stopc == c)
				return;
			tp->t_state &= ~0x100;
			ttkick(tp);
		} else {
			if (tp->t_stopc == c) {
				tp->t_state |= 0x100;
				(*cdevsw[major(tp->t_dev)].d_stop)(tp);
				return;
			}
			if (tp->t_startc == c)
				return;
		}
	}
	if (flg & RAW)
		goto store;
	if (tp->t_lstate & 8) {
		tp->t_lstate &= ~8;
		if ((flg & LCASE) && (ttylcmap[c] & 0177)) {
			c = ttylcmap[c];
			c &= 0177;
			goto put;
		}
		if (ttyspec(c, tp))
			goto put;
		putc('\\', tp);
	}
	if (c == 0x10 && tp->t_page) {
		tp->t_lstate ^= 1;
		if ((tp->t_lstate & 2) == 0)
			goto echo;
	}
	if (tp->t_lstate & 2) {
		tp->t_lstate &= ~6;
		if (c == '\r' || c == '\n')
			tp->t_cnt2c = tp->t_page - 1;
		else
			tp->t_cnt2c = 0;
		if (c != tp->t_quitc && c != tp->t_intrc)
			goto kick;
	}
	if (c == tp->t_quitc || c == tp->t_intrc) {
		ttyflush(tp);
		c = (tp->t_intrc == c) ? SIGINT : SIGQUIT;
		signal(tp->t_pgrp, c);
		ttyecho('\n', tp);
		goto kick;
	}
	if ((flg & CBREAK) == 0) {
		if (c == tp->t_erase) {
			if (bitset(ttyctl, c)) {
				ttyrub(unputc(tp), tp);
				goto kick;
			}
			unputc(tp);
			goto echo;
		}
		if (c == tp->t_kill) {
			if ((flg & 0x100) && bitset(ttyctl, c)) {
				for (;;) {
					c = unputc(tp);
					if (c < 0)
						goto kick;
					if ((tp->t_col & 0377) > 0)
						ttyrub(c, tp);
				}
			}
			while (unputc(tp) >= 0)
				;
			ttyecho(c, tp);
			ttyecho('\n', tp);
			goto kick;
		}
		if (c == tp->t_eofc) {
			ttyecho('\n', tp);
			goto eol;
		}
		if (c == '\\') {
			tp->t_lstate |= 8;
			ttyecho(c, tp);
			goto kick;
		}
		if (c == 0x12) {
			if (tp->t_outq.c_cc)
				goto kick;
			ttyecho('\n', tp);
			ttycol(tp, 1);
			goto kick;
		}
		if (c == 0x17) {
			ttyrub(c = unputc(tp), tp);
			if (!bitset(ttyword, c))
				goto kick;
			for (;;) {
				c = unputc(tp);
				if (!bitset(ttyword, c))
					break;
				ttyrub(c, tp);
			}
			if (c < 0)
				goto kick;
			putc(c, tp);
			goto kick;
		}
	}
store:
	if (c == '\r' && (flg & CRMOD))
		c = '\n';
	if ((flg & LCASE) && c >= 'A' && c <= 'Z')
		c += 'a' - 'A';
	if (tp->t_inq.c_cc > 256) {
		ttyflush(tp);
		return;
	}
put:
	putc(c, tp);
eol:
	if ((flg & (RAW|CBREAK)) || c == '\n' || c == tp->t_eofc || c == tp->t_brkc) {
		if ((flg & (RAW|CBREAK)) == 0)
			if (putc(0377, tp) == 0)
				tp->t_delct++;
		wakeup((caddr_t)tp);
	}
echo:
	if (ttyecho(c, tp) == 0)
		return;
kick:
	ttkick(tp);
}

/* KERNEL 75B2 -- Spalte am Ende der Eingabe berechnen (f = 1: dabei das Eingegebene neu ausgeben) */
ttycol(tp, f)
register struct tty *tp;
{
	register char *p;
	register n, dl, w;
	register col;

	p = tp->t_inq.c_cf;
	dl = tp->t_delct;
	w = tp->t_width ? (tp->t_width & 0377) : 0377;
	col = 0;
	n = tp->t_inq.c_cc;
	while (n--) {
		if (dl) {
			if ((*p++ & 0377) == 0377)
				dl--;
		} else {
			if (((tp->t_flags & LCASE) && *p >= 'A' && *p <= 'Z') || ttyspec(*p, tp)) {
				if (f)
					ttyecho('\\', tp);
				else
					col++;
			}
			if (f)
				ttyecho(*p++, tp);
			else {
				if (*p++ == '\t')
					col += 8 - (col & 07);
				else
					col++;
				if (col >= w)
					col -= w;
			}
		}
		if (((int)p & 15) == 0)
			p = ((char **)p)[-8] + 2;
	}
	return(col);
}

/* KERNEL 769A -- Zeichen zurueckmelden (Echo) */
ttyecho(c, tp)
register c;
register struct tty *tp;
{
	c &= 0177;
	if ((tp->t_flags & ECHO) == 0)
		return(0);
	if (bitset(ttyctl, c) && (tp->t_flags & 0x200) == 0)
		return(0);
	if (tp->t_lstate & 0x10) {
		tp->t_lstate &= ~0x10;
		ttyoutput(']', tp);
	}
	if ((tp->t_flags & LCASE) && c >= 'A' && c <= 'Z')
		c += 'a' - 'A';
	if (c == '\n' && (tp->t_lstate & 1) && (tp->t_flags & RAW) == 0 && tp->t_cnt2c)
		tp->t_cnt2c--;
	ttyoutput(c, tp);
	return(1);
}

/* KERNEL 772E -- Zeichen auf dem Bildschirm loeschen (Rubout, CRT-Form) */
ttyrub(c, tp)
register c;
register struct tty *tp;
{
	register n;

	if ((tp->t_flags & ECHO) == 0)
		return;
	if (c < 0) {
		ttyoutput(7, tp);
		return;
	}
	if ((tp->t_flags & 0x100) == 0) {
		if ((tp->t_lstate & 0x10) == 0) {
			tp->t_lstate |= 0x10;
			ttyoutput('[', tp);
		}
		ttyoutput(c, tp);
		return;
	}
	n = 1;
	if (bitset(ttyctl, c)) {
		n = 0;
		if (tp->t_flags & 0x200)
			n = 2;
		if (ttyspec(c, tp))
			n++;
	} else if (c == '\t') {
		n = ttycol(tp, 0);
		if (n) {
			n = (tp->t_col & 0377) - ((tp->t_dtab & 0377) >= n ? (tp->t_dtab & 0377) : n);
		} else {
			n = (tp->t_col & 0377) - (tp->t_dtab & 0377);
			if (n == 0)
				n = 8;
		}
		if ((tp->t_dtab & 0377) >= (tp->t_col & 0377) - n)
			tp->t_dtab = 0;
	} else if (tp->t_flags & LCASE) {
		if ((c >= 'A' && c <= 'Z') || c == '`' || c >= '{')
			n++;
	}
	if (n < 0 || (tp->t_col & 0377) < n) {
		ttyoutput('\n', tp);
		ttycol(tp, 1);
		return;
	}
	while (n--) {
		ttyoutput('\b', tp);
		ttyoutput(' ', tp);
		ttyoutput('\b', tp);
	}
}

/* KERNEL 78A8 -- letztes Zeichen aus der Zeichenliste nehmen (-1, wenn leer oder Zeilenende) */
unputc(p)
register struct clist *p;
{
	register struct cblock *bp;
	register char *cp;
	register c, s;

	s = spl6();
	if ((cp = p->c_cl) == 0 || (c = *--cp & 0377) == 0377) {
		splx(s);
		return(-1);
	}
	p->c_cl = cp;
	cp -= 2;
	if (--p->c_cc <= 0) {
		bp = (struct cblock *)((int)cp & ~15);
		p->c_cf = p->c_cl = 0;
		bp->c_next = cfreelist;
		cfreelist = bp;
	} else if (((int)cp & 15) == 0) {
		bp = (struct cblock *)cp;
		bp->c_next = cfreelist;
		cfreelist = bp;
		bp = (struct cblock *)((int)p->c_cf & ~15);
		while (bp->c_next != (struct cblock *)cp)
			bp = bp->c_next;
		bp->c_next = 0;
		p->c_cl = (char *)bp + 16;
	}
	splx(s);
	return(c);
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
		if (tp->t_page == 0)
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

/* KERNEL 7A34 */
ttyoutput(c, tp)
register c;
register struct tty *tp;
{
	register char *colp;

	if (c == '\n' && (tp->t_flags & CRMOD))
		ttyoutput('\r', tp);
	if ((tp->t_flags & RAW) == 0) {
		c &= 0177;
		if ((tp->t_flags & 0x200) && bitset(ttyctl, c)) {
			ttyoutput('^', tp);
			if (c == 0177)
				c = '#';
			else
				c |= 0140;
		}
		if (tp->t_page && (tp->t_lstate & 1)) {
			if ((c == '\n' && ++tp->t_cnt2c >= tp->t_page) || c == '\f') {
				tp->t_cnt2c = 0;
				tp->t_lstate |= 4;
				putc(0200, &tp->t_outq);
			}
		}
	}
	if (tp->t_flags & LCASE) {
		if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
		else if (c >= 'A' && c <= 'Z')
			ttyoutput('\\', tp);
		else {
			colp = ttylcpr;
			while (*colp++) {
				if (*colp++ == c) {
					ttyoutput('\\', tp);
					c = colp[-2];
					break;
				}
			}
		}
	}
	colp = &tp->t_col;
	if (c == '\t' && (tp->t_flags & XTABS)) {
		if (tp->t_dtab == 0)
			tp->t_dtab = tp->t_col;
		do
			ttyoutput(' ', tp);
		while ((*colp & 07) != 0);
		return;
	}
	if (c >= ' ' && c <= '~') {
		if (tp->t_width && (*colp & 0377) >= (tp->t_width & 0377))
			ttyoutput('\n', tp);
		(*colp)++;
		putc(c, &tp->t_outq);
		return;
	}
	putc(c, &tp->t_outq);
	switch (c & 0177) {

	case 8:
		if (*colp)
			(*colp)--;
		return;

	case 9:
		*colp |= 07;
		(*colp)++;
		c = tp->t_dtab;
		break;

	case 10:
		if (tp->t_flags & XTABS)
			tp->t_dtab = 0;
		*colp = 0;
		c = tp->t_dnl;
		break;

	case 11:
	case 12:
		c = tp->t_dvt;
		break;

	case 13:
		*colp = 0;
		c = tp->t_dcr;
		break;

	default:
		return;
	}
	if (c && (tp->t_flags & RAW) == 0)
		putc(c | 0200, &tp->t_outq);
}

/* Paare (Zeichen, Ersatz) fuer die Ausgabe bei LCASE, abgeschlossen durch 0 */
char ttylcpr[] = "({)}!|^~'`";

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
		if (tp->t_page) {
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

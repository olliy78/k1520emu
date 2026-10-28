/* subr.c -- Dateiblock-Abbildung und Kleinkram (Kernel mutos_1216, 20FE..243A; timeout 2A4C liegt hinter dem Taktgeber).
 * Daten: rablock = 8DE4 (daddr_t, V7 `rablock`), callout = CE58..CE8E (9 Eintraege zu 6 Byte, BSS), Text 8AC2 "Timeout table overflow".
 * Namen: 236C = passc, 23CA = cpass (rufen die Nutzerraum-Zugriffe 0508 = subyte und 04CC = fubyte -- K6b nannte sie
 * suword/fuword; sie arbeiten hier auf einzelnen Bytes). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "inode.h"
#include "buf.h"
#include "callo.h"

daddr_t rablock;
struct callo callout[NCALL];

/* KERNEL 20FE */
daddr_t
bmap(ip, bn, rwflg)
register struct inode *ip;
daddr_t bn;
{
	register i;
	struct buf *bp, *nbp;
	int j, sh;
	daddr_t nb, *bap;
	dev_t dev;

	if (bn < 0) {
		u.u_error = EFBIG;
		return(0);
	}
	dev = ip->i_dev;
	rablock = 0;
	if (bn < 10) {
		i = bn;
		nb = ip->i_addr[i];
		if (nb == 0) {
			if (rwflg == B_READ || (bp = alloc(dev)) == NULL)
				return((daddr_t)-1);
			nb = bp->b_blkno;
			bdwrite(bp);
			ip->i_addr[i] = nb;
			ip->i_flag |= IUPD|ICHG;
		}
		if (i < 9)
			rablock = ip->i_addr[i+1];
		return(nb);
	}
	sh = 0;
	nb = 1;
	bn -= 10;
	for (j = 3; j > 0; j--) {
		sh += 7;
		nb <<= 7;
		if (bn < nb)
			break;
		bn -= nb;
	}
	if (j == 0) {
		u.u_error = EFBIG;
		return(0);
	}
	nb = ip->i_addr[13-j];
	if (nb == 0) {
		if (rwflg == B_READ || (bp = alloc(dev)) == NULL)
			return((daddr_t)-1);
		nb = bp->b_blkno;
		bdwrite(bp);
		ip->i_addr[13-j] = nb;
		ip->i_flag |= IUPD|ICHG;
	}
	for (; j <= 3; j++) {
		bp = bread(dev, nb);
		if (bp->b_flags & B_ERROR) {
			brelse(bp);
			return(0);
		}
		bap = (daddr_t *)bp->b_addr;
		sh -= 7;
		i = (bn >> sh) & 0177;
		nb = bap[i];
		if (nb == 0) {
			if (rwflg == B_READ || (nbp = alloc(dev)) == NULL) {
				brelse(bp);
				return((daddr_t)-1);
			}
			nb = nbp->b_blkno;
			bdwrite(nbp);
			bap[i] = nb;
			bdwrite(bp);
		} else
			brelse(bp);
	}
	if (i < 127)
		rablock = bap[i+1];
	return(nb);
}

/* KERNEL 236C */
passc(c)
register c;
{
	register id;

	if ((id = u.u_segflg) == 1)
		*u.u_base = c;
	else if (subyte(u.u_base, c) < 0) {
		u.u_error = EFAULT;
		return(-1);
	}
	u.u_count--;
	u.u_offset++;
	u.u_base++;
	return(u.u_count == 0 ? -1 : 0);
}

/* KERNEL 23CA */
cpass()
{
	register c, id;

	if (u.u_count == 0)
		return(-1);
	if ((id = u.u_segflg) == 1)
		c = *u.u_base;
	else if ((c = fubyte(u.u_base)) < 0) {
		u.u_error = EFAULT;
		return(-1);
	}
	u.u_count--;
	u.u_offset++;
	u.u_base++;
	return(c & 0377);
}

/* KERNEL 242A */
nodev()
{
	u.u_error = ENODEV;
}

/* KERNEL 2438 */
nulldev()
{
}

/* KERNEL 243A */
bcopy(from, to, n)
caddr_t from, to;
{
	register i;
	register caddr_t f, t;

	i = n;
	f = from;
	t = to;
	do
		*t++ = *f++;
	while (--i);
}

/* KERNEL 2A4C -- liegt im Kernel hinter dem Taktgeber (clock 2842), gehoert zum Zeitgeber-Modul */
timeout(fun, arg, tim)
int (*fun)();
caddr_t arg;
{
	register struct callo *p1, *p2;
	register t;
	int s;

	t = tim;
	p1 = &callout[0];
	s = spl7();
	while (p1->c_func != 0 && p1->c_time <= t) {
		t -= p1->c_time;
		p1++;
	}
	if (p1 >= &callout[NCALL])
		panic("Timeout table overflow");
	p1->c_time -= t;
	p2 = p1;
	while (p2->c_func != 0)
		p2++;
	while (p2 >= p1) {
		(p2+1)->c_time = p2->c_time;
		(p2+1)->c_func = p2->c_func;
		(p2+1)->c_arg = p2->c_arg;
		p2--;
	}
	p1->c_time = t;
	p1->c_func = fun;
	p1->c_arg = arg;
	splx(s);
}

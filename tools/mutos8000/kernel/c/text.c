/* text.c -- Textverwaltung (Kernel mutos_1216, 562C..57D4).  Es gibt keine Texttabelle (kein geteilter Text, Befund K6a):
 * die V7-Funktionen xlock/xunlock/xccdec/xumount/xrele sind leere Stumpfe mit csv/cret (nur `register i;`), xalloc ein Stumpf
 * mit Registerparameter.  Daten: swapmap = A0DC, coremap = A09C, runout = 8DD8, Text 8C64 "out of swap space".
 * saccess (55AE) steht im Kernel noch vor xswap, gehoert aber zu sys2.c (siehe dort).
 * Namen: 562C ist in V7 `xswap` (K6b nannte es xalloc). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "proc.h"
#include "text.h"
#include "map.h"
#include "inode.h"

#define ctodu(x)	(((unsigned)(x)+1)>>1)

/* KERNEL 562C */
xswap(p, ff, os)
register struct proc *p;
register os;
{
	register a, t;
	register struct user *up;

	if (os == 0)
		os = p->p_size;
	t = p->p_addr << 2;
	up = (struct user *)(t + 0xEC00);
	t = up->u_dsize;
	t = ctodu(t) + 2;
	a = (int)up->u_ar0;
	a += 0x400;
	a += (int)up;
	a = *(int *)(a + 0x10);
	t += ctodu(0x100 - ((unsigned)a >> 8));
	a = malloc(swapmap, t);
	if (a == NULL)
		panic("out of swap space");
	p->p_flag |= SLOCK;
	xccdec(p->p_textp);
	swap(a, p->p_addr, 0x104, 0, up);
	if (ff)
		mfree(coremap, os, p->p_addr);
	p->p_addr = a;
	p->p_flag &= ~(SLOAD|SLOCK);
	p->p_time = 0;
	if (runout) {
		runout = 0;
		wakeup((caddr_t)&runout);
	}
}

/* KERNEL 5702 */
xfree()
{
	register struct text *xp;
	register struct inode *ip;

	if ((xp = u.u_procp->p_textp) != NULL) {
		xlock(xp);
		xp->x_flag &= ~XLOCK;
		u.u_procp->p_textp = NULL;
		ip = xp->x_iptr;
		if (--xp->x_count == 0 && (ip->i_mode & ISVTX) == 0) {
			xp->x_iptr = NULL;
			mfree(swapmap, (xp->x_size+1)>>1, xp->x_daddr);
			mfree(coremap, xp->x_size, xp->x_caddr);
			ip->i_flag &= ~ITEXT;
			if (ip->i_flag & ILOCK)
				ip->i_count--;
			else
				iput(ip);
		} else
			xccdec(xp);
	}
}

/* KERNEL 5790 */
xalloc(ip)
register struct inode *ip;
{
}

/* KERNEL 579C */
xstub1()
{
	register i;
}

/* KERNEL 57A4 */
xlock()
{
	register i;
}

/* KERNEL 57AC */
xunlock()
{
	register i;
}

/* KERNEL 57B4 */
xccdec()
{
	register i;
}

/* KERNEL 57BC */
xumount()
{
	register i;
}

/* KERNEL 57C4 */
xrele()
{
	register i;
}

/* KERNEL 57CC */
xstub2()
{
	register i;
}

/* varianten.c -- Quellformen-Versuche fuer die DJNZ-Frage (AP K7a): Funktionen mit Kernelmarke, je Variante ein Name.
 * Kernelziele: 5C86 clrbuf (DEC/JR NZ), 2780 iomove (DEC/JR NZ im Schreibast, DJNZ im Leseast), 6384 putchar (DJNZ). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "buf.h"

/* KERNEL 5C86 */
cbA(bp)
struct buf *bp;
{
	register *p;
	register c;

	p = (int *)bp->b_addr;
	c = 256;
	do
		*p++ = 0;
	while (--c);
	bp->b_resid = 0;
}
/* KERNEL 5C86 */
cbB(bp)
struct buf *bp;
{
	register *p;
	register c;

	p = (int *)bp->b_addr;
	for (c = 256; c > 0; c--)
		*p++ = 0;
	bp->b_resid = 0;
}
/* KERNEL 5C86 */
cbC(bp)
struct buf *bp;
{
	register *p;
	register c;

	p = (int *)bp->b_addr;
	c = 256;
	while (c--)
		*p++ = 0;
	bp->b_resid = 0;
}
/* KERNEL 5C86 */
cbD(bp)
struct buf *bp;
{
	register *p;
	register c;

	p = (int *)bp->b_addr;
	c = 256;
	do {
		*p++ = 0;
		c--;
	} while (c != 0);
	bp->b_resid = 0;
}
/* KERNEL 5C86 */
cbE(bp)
struct buf *bp;
{
	register *p;
	register c;

	p = (int *)bp->b_addr;
	c = 256;
	do {
		*p++ = 0;
	} while (--c > 0);
	bp->b_resid = 0;
}
/* KERNEL 5C86 */
cbF(bp)
struct buf *bp;
{
	register *p;
	register c;

	p = (int *)bp->b_addr;
	for (c = 0; c < 256; c++)
		*p++ = 0;
	bp->b_resid = 0;
}

/* KERNEL 2780 */
ioB(cp, n, flag)
register caddr_t cp;
register n;
{
	register t;

	if (n == 0)
		return;
	if (u.u_segflg != 1 && (n & 01) == 0 && ((int)cp & 01) == 0 && ((int)u.u_base & 01) == 0) {
		if (flag == B_WRITE)
			t = copyin(u.u_base, (caddr_t)cp, n);
		else
			t = copyout((caddr_t)cp, u.u_base, n);
		if (t) {
			u.u_error = EFAULT;
			return;
		}
		u.u_base += n;
		u.u_offset += n;
		u.u_count -= n;
		return;
	}
	if (flag == B_WRITE) {
		do {
			if ((t = cpass()) < 0)
				return;
			*cp++ = t;
		} while (--n);
	} else {
		do
			if (passc(*cp++) < 0)
				return;
		while (--n);
	}
}
/* KERNEL 2780 */
ioC(cp, n, flag)
register caddr_t cp;
register n;
{
	register t;

	if (n == 0)
		return;
	if (u.u_segflg != 1 && (n & 01) == 0 && ((int)cp & 01) == 0 && ((int)u.u_base & 01) == 0) {
		if (flag == B_WRITE)
			t = copyin(u.u_base, (caddr_t)cp, n);
		else
			t = copyout((caddr_t)cp, u.u_base, n);
		if (t) {
			u.u_error = EFAULT;
			return;
		}
		u.u_base += n;
		u.u_offset += n;
		u.u_count -= n;
		return;
	}
	if (flag == B_WRITE) {
		for (; n > 0; n--) {
			if ((t = cpass()) < 0)
				return;
			*cp++ = t;
		}
	} else {
		for (; n > 0; n--)
			if (passc(*cp++) < 0)
				return;
	}
}
/* KERNEL 2780 */
ioD(cp, n, flag)
register caddr_t cp;
register n;
{
	register t;

	if (n == 0)
		return;
	if (u.u_segflg != 1 && (n & 01) == 0 && ((int)cp & 01) == 0 && ((int)u.u_base & 01) == 0) {
		if (flag == B_WRITE)
			t = copyin(u.u_base, (caddr_t)cp, n);
		else
			t = copyout((caddr_t)cp, u.u_base, n);
		if (t) {
			u.u_error = EFAULT;
			return;
		}
		u.u_base += n;
		u.u_offset += n;
		u.u_count -= n;
		return;
	}
	if (flag == B_WRITE) {
		do {
			t = cpass();
			if (t < 0)
				return;
			*cp++ = t;
			n--;
		} while (n);
	} else {
		do {
			if (passc(*cp++) < 0)
				return;
			n--;
		} while (n);
	}
}

struct mbx {
	char m_cmd;
	char m_b;
	char m_c;
	char m_flag;
	int m_w;
	char *m_buf;
	int m_len;
};
char *msgp;
char msgbuf[128];

/* KERNEL 6384 */
puA(c)
register c;
{
	register s, n;
	register struct mbx *mp;
	register char *p;

	if (c) {
		*msgp++ = c;
		if (msgp >= &msgbuf[128])
			msgp = msgbuf;
	}
	mp = (struct mbx *)0xD26A;
	p = (char *)0xD2A2;
	n = 10000;
	do {
		if (mp->m_flag)
			break;
	} while (--n);
	if (c) {
		s = splx(7);
		mp->m_cmd = 4;
		mp->m_b = 0;
		mp->m_flag = 0;
		mp->m_w = 0;
		mp->m_buf = p;
		mp->m_len = 1;
		mp->m_c = 0x81;
		*p = c;
		if (c == '\n') {
			p++;
			*p = '\r';
			mp->m_len = 2;
		}
		mapalloc(mp, 4);
		putchar(0);
		mp->m_flag = 1;
		splx(s);
	}
}
/* KERNEL 6384 */
puB(c)
register c;
{
	register s, n;
	register struct mbx *mp;
	register char *p;

	if (c) {
		*msgp++ = c;
		if (msgp >= &msgbuf[128])
			msgp = msgbuf;
	}
	mp = (struct mbx *)0xD26A;
	p = (char *)0xD2A2;
	for (n = 10000; n > 0; n--)
		if (mp->m_flag)
			break;
	if (c) {
		s = splx(7);
		mp->m_cmd = 4;
		mp->m_b = 0;
		mp->m_flag = 0;
		mp->m_w = 0;
		mp->m_buf = p;
		mp->m_len = 1;
		mp->m_c = 0x81;
		*p = c;
		if (c == '\n') {
			p++;
			*p = '\r';
			mp->m_len = 2;
		}
		mapalloc(mp, 4);
		putchar(0);
		mp->m_flag = 1;
		splx(s);
	}
}
/* KERNEL 6384 */
puC(c)
register c;
{
	register s, n;
	register struct mbx *mp;
	register char *p;

	if (c) {
		*msgp++ = c;
		if (msgp >= &msgbuf[128])
			msgp = msgbuf;
	}
	mp = (struct mbx *)0xD26A;
	p = (char *)0xD2A2;
	n = 10000;
	while (n-- > 0)
		if (mp->m_flag)
			break;
	if (c) {
		s = splx(7);
		mp->m_cmd = 4;
		mp->m_b = 0;
		mp->m_flag = 0;
		mp->m_w = 0;
		mp->m_buf = p;
		mp->m_len = 1;
		mp->m_c = 0x81;
		*p = c;
		if (c == '\n') {
			p++;
			*p = '\r';
			mp->m_len = 2;
		}
		mapalloc(mp, 4);
		putchar(0);
		mp->m_flag = 1;
		splx(s);
	}
}

/* KERNEL 243A */
bcV1(from, to, n)
caddr_t from, to;
{
	register i;
	register caddr_t f, t;

	i = n;
	f = from;
	t = to;
	while (--i >= 0)
		*t++ = *f++;
}
/* KERNEL 243A */
bcV2(from, to, n)
caddr_t from, to;
{
	register i;
	register caddr_t f, t;

	i = n;
	f = from;
	t = to;
	do {
		*t = *f;
		t++;
		f++;
		i--;
	} while (i != 0);
}
/* KERNEL 243A */
bcV3(from, to, n)
caddr_t from, to;
{
	register i;
	register caddr_t f, t;

	i = n;
	f = from;
	t = to;
	for (; i != 0; i--)
		*t++ = *f++;
}

/* rdwri.c -- Lesen/Schreiben von Inodes (Kernel mutos_1216, 2460..2842).  Wie V7 readi/writei; Besonderheit: bmap liefert
 * -1 fuer ein Loch (readi nullt dann mit geteblk/clrbuf).  Daten: rablock = 8DE4 (siehe subr.c), cdevsw 8796 (d_read +4),
 * 879A/879C = cdevsw[0].d_read/d_write.  Kein eigener Text. */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "inode.h"
#include "buf.h"
#include "conf.h"

/* KERNEL 2460 */
readi(ip)
register struct inode *ip;
{
	struct buf *bp;
	dev_t dev;
	daddr_t lbn, bn;
	off_t diff;
	register on, n;
	register type;

	if (u.u_count == 0)
		return;
	if (u.u_offset < 0) {
		u.u_error = EINVAL;
		return;
	}
	ip->i_flag |= IACC;
	dev = (dev_t)ip->i_addr[0];
	type = ip->i_mode & IFMT;
	if (type == IFCHR || type == 0030000) {
		return((*cdevsw[major(dev)].d_read)(dev));
	}
	do {
		lbn = bn = u.u_offset >> BSHIFT;
		on = u.u_offset & BMASK;
		n = min((unsigned)(BSIZE - on), u.u_count);
		if (type != 0060000 && type != 0070000) {
			diff = ip->i_size - u.u_offset;
			if (diff <= 0)
				return;
			if (diff < n)
				n = diff;
			bn = bmap(ip, bn, B_READ);
			if (u.u_error)
				return;
			dev = ip->i_dev;
		} else
			rablock = bn + 1;
		if (bn < 0) {
			bp = geteblk();
			clrbuf(bp);
		} else if (ip->i_lastr + 1 == lbn)
			bp = breada(dev, bn, rablock);
		else
			bp = bread(dev, bn);
		ip->i_lastr = lbn;
		n = min((unsigned)n, BSIZE - bp->b_resid);
		if (n != 0)
			iomove(bp->b_addr + on, n, B_READ);
		brelse(bp);
	} while (u.u_error == 0 && u.u_count != 0 && n > 0);
}

/* KERNEL 25FE */
writei(ip)
register struct inode *ip;
{
	struct buf *bp;
	dev_t dev;
	daddr_t bn;
	register n, on;
	register type;

	if (u.u_offset < 0) {
		u.u_error = EINVAL;
		return;
	}
	dev = (dev_t)ip->i_addr[0];
	type = ip->i_mode & IFMT;
	if (type == IFCHR || type == 0030000) {
		ip->i_flag |= IUPD|ICHG;
		(*cdevsw[major(dev)].d_write)(dev);
		return;
	}
	if (u.u_count == 0)
		return;
	do {
		bn = u.u_offset >> BSHIFT;
		on = u.u_offset & BMASK;
		n = min((unsigned)(BSIZE - on), u.u_count);
		if (type != 0060000 && type != 0070000) {
			bn = bmap(ip, bn, B_WRITE);
			if (bn < 0)
				return;
			dev = ip->i_dev;
		}
		if (n == BSIZE)
			bp = getblk(dev, bn);
		else
			bp = bread(dev, bn);
		iomove(bp->b_addr + on, n, B_WRITE);
		if (u.u_error != 0)
			brelse(bp);
		else
			bdwrite(bp);
		if (u.u_offset > ip->i_size && (type == IFDIR || type == IFREG))
			ip->i_size = u.u_offset;
		ip->i_flag |= IUPD|ICHG;
	} while (u.u_error == 0 && u.u_count != 0);
}

/* KERNEL 2766 */
min(a, b)
unsigned a, b;
{
	if (a < b)
		return(a);
	return(b);
}

/* KERNEL 2780 */
iomove(cp, n, flag)
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
		while (n--) {
			if ((t = cpass()) < 0)
				return;
			*cp++ = t;
		}
	} else {
		while (n--)
			if (passc(*cp++) < 0)
				return;
	}
}

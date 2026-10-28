/* alloc.c -- Plattenverwaltung (Kernel mutos_1216, 2F48..34F5): alloc, free, badblock (intern), ialloc, ifree, getfs, update.
 * Daten: updlock 8DDE, mount[] A07E (6 Byte je Eintrag: m_dev, m_bufp, m_inodp), inode[] 907A..9EEE, time 8DCC.
 * Texte: 8AF2 "Bad free count", 8B01 "no space", 8B0A "bad block", 8B14 "Out of inodes", 8B22 "bad count", 8B2C "no fs".
 * Superblock im Speicher: s_nfree +6, s_free[50] long +8, s_ninode +D0, s_inode[100] +D2, s_flock +19A, s_ilock +19B,
 * s_fmod +19C, s_ronly +19D, s_time +19E, s_tfree +1A2, s_tinode +1A6.  badblock ist hier nicht exportiert (kein Symbol). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "inode.h"
#include "mount.h"
#include "ino.h"
#include "filsys.h"
#include "buf.h"

int updlock;
static badblock();

/* KERNEL 2F48 */
struct buf *
alloc(dev)
dev_t dev;
{
	register struct filsys *fp;
	register struct buf *bp;
	daddr_t bno;

	fp = getfs(dev);
	while (fp->s_flock)
		sleep((caddr_t)&fp->s_flock, PINOD);
	do {
		if (fp->s_nfree <= 0)
			goto nospace;
		if (fp->s_nfree > NICFREE) {
			prdev("Bad free count", dev);
			goto nospace;
		}
		bno = fp->s_free[--fp->s_nfree];
		if (bno == 0)
			goto nospace;
	} while (badblock(fp, bno, dev));
	if (fp->s_nfree <= 0) {
		fp->s_flock++;
		bp = bread(dev, bno);
		if ((bp->b_flags & B_ERROR) == 0) {
			fp->s_nfree = *((int *)bp->b_addr);
			bcopy((caddr_t)((int *)bp->b_addr + 1), (caddr_t)fp->s_free, sizeof(fp->s_free));
		}
		brelse(bp);
		fp->s_flock = 0;
		wakeup((caddr_t)&fp->s_flock);
		if (fp->s_nfree <= 0)
			goto nospace;
	}
	bp = getblk(dev, bno);
	clrbuf(bp);
	if (fp->s_tfree)
		fp->s_tfree--;
	fp->s_fmod = 1;
	return(bp);

nospace:
	fp->s_nfree = 0;
	fp->s_tfree = 0;
	prdev("no space", dev);
	u.u_error = ENOSPC;
	return(NULL);
}

/* KERNEL 3086 */
free(dev, bno)
dev_t dev;
daddr_t bno;
{
	register struct filsys *fp;
	register struct buf *bp;

	fp = getfs(dev);
	fp->s_fmod = 1;
	while (fp->s_flock)
		sleep((caddr_t)&fp->s_flock, PINOD);
	if (badblock(fp, bno, dev))
		return;
	if (fp->s_nfree <= 0) {
		fp->s_nfree = 1;
		fp->s_free[0] = 0;
	}
	if (fp->s_nfree >= NICFREE) {
		fp->s_flock++;
		bp = getblk(dev, bno);
		*((int *)bp->b_addr) = fp->s_nfree;
		bcopy((caddr_t)fp->s_free, (caddr_t)((int *)bp->b_addr + 1), sizeof(fp->s_free));
		fp->s_nfree = 0;
		bwrite(bp);
		fp->s_flock = 0;
		wakeup((caddr_t)&fp->s_flock);
	}
	fp->s_free[fp->s_nfree++] = bno;
	fp->s_tfree++;
	fp->s_fmod = 1;
}

/* KERNEL-intern 316A badblock */
static
badblock(fp, bn, dev)
register struct filsys *fp;
daddr_t bn;
dev_t dev;
{
	if (bn < fp->s_isize || bn >= fp->s_fsize) {
		prdev("bad block", dev);
		return(1);
	}
	return(0);
}

/* KERNEL 319E */
struct inode *
ialloc(dev)
dev_t dev;
{
	register struct filsys *fp;
	register struct buf *bp;
	register struct inode *ip;
	int i;
	struct dinode *dp;
	ino_t ino;
	daddr_t adr;

	fp = getfs(dev);
	while (fp->s_ilock)
		sleep((caddr_t)&fp->s_ilock, PINOD);
loop:
	if (fp->s_ninode > 0) {
		ino = fp->s_inode[--fp->s_ninode];
		if (ino < ROOTINO)
			goto loop;
		ip = iget(dev, ino);
		if (ip == NULL)
			return(NULL);
		if (ip->i_mode == 0) {
			for (i = 0; i < NADDR; i++)
				ip->i_addr[i] = 0;
			if (fp->s_tinode)
				fp->s_tinode--;
			fp->s_fmod = 1;
			return(ip);
		}
		iput(ip);
		goto loop;
	}
	fp->s_ilock++;
	ino = 1;
	for (adr = SUPERB+1; adr < fp->s_isize; adr++) {
		bp = bread(dev, adr);
		if (bp->b_flags & B_ERROR) {
			brelse(bp);
			ino += INOPB;
			continue;
		}
		dp = (struct dinode *)bp->b_addr;
		for (i = 0; i < INOPB; i++) {
			if (dp->di_mode == 0) {
				for (ip = &inode[0]; ip < &inode[NINODE]; ip++)
					if (dev == ip->i_dev && ino == ip->i_number)
						goto cont;
				fp->s_inode[fp->s_ninode++] = ino;
				if (fp->s_ninode >= NICINOD)
					break;
			}
		cont:
			ino++;
			dp++;
		}
		brelse(bp);
		if (fp->s_ninode >= NICINOD)
			break;
	}
	fp->s_ilock = 0;
	wakeup((caddr_t)&fp->s_ilock);
	if (fp->s_ninode > 0)
		goto loop;
	prdev("Out of inodes", dev);
	u.u_error = ENOSPC;
	fp->s_tinode = 0;
	return(NULL);
}

/* KERNEL 335A */
ifree(dev, ino)
dev_t dev;
ino_t ino;
{
	register struct filsys *fp;

	fp = getfs(dev);
	fp->s_tinode++;
	if (fp->s_ilock)
		return;
	if (fp->s_ninode >= NICINOD)
		return;
	fp->s_inode[fp->s_ninode++] = ino;
	fp->s_fmod = 1;
}

/* KERNEL 339E */
struct filsys *
getfs(dev)
dev_t dev;
{
	register struct mount *mp;
	register struct filsys *fp;

	for (mp = &mount[0]; mp < &mount[NMOUNT]; mp++)
		if (mp->m_bufp != NULL && mp->m_dev == dev) {
			fp = (struct filsys *)mp->m_bufp->b_addr;
			if (fp->s_nfree > NICFREE || fp->s_ninode > NICINOD) {
				prdev("bad count", dev);
				fp->s_nfree = 0;
				fp->s_ninode = 0;
			}
			return(fp);
		}
	panic("no fs");
	return(NULL);
}

/* KERNEL 3400 */
update()
{
	register struct inode *ip;
	register struct mount *mp;
	register struct buf *bp;
	struct filsys *fp;

	if (updlock)
		return;
	updlock++;
	for (mp = &mount[0]; mp < &mount[NMOUNT]; mp++)
		if (mp->m_bufp != NULL) {
			fp = (struct filsys *)mp->m_bufp->b_addr;
			if (fp->s_fmod == 0 || fp->s_ilock != 0 || fp->s_flock != 0 || fp->s_ronly != 0)
				continue;
			bp = getblk(mp->m_dev, (daddr_t)SUPERB);
			if (bp->b_flags & B_ERROR)
				continue;
			fp->s_fmod = 0;
			fp->s_time = time;
			bcopy((caddr_t)fp, bp->b_addr, BSIZE);
			bwrite(bp);
		}
	for (ip = &inode[0]; ip < &inode[NINODE]; ip++)
		if ((ip->i_flag & ILOCK) == 0 && ip->i_count != 0) {
			ip->i_flag |= ILOCK;
			ip->i_count++;
			iupdat(ip, &time, &time);
			iput(ip);
		}
	updlock = 0;
	bflush(NODEV);
}

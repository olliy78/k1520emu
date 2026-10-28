/* bio.c -- Pufferverwaltung (Kernel mutos_1216, 57D4..60C7): bread, breada, bwrite, bdwrite, bawrite, brelse, incore, getblk,
 * geteblk, iowait, notavail, iodone, clrbuf, mapalloc (5CAE), mapstart (5CF6), swap, bflush, physio, geterror.
 * Nicht hier: binit liegt im Kernel bei 07D2 (main.c).
 * Daten: bdevsw 8766 (strategy +4 = 876A, d_tab +6 = 876C), buf[] 8E9A, bfreelist A518 (av_forw A51E, av_back A520),
 * nblkdev 8DD0, swapdev 887E, swplo 8884, swbuf1 CE28, swbuf2 CE40 (je 24 Byte), swapmap A0DC, mapq D57C (4 Worte), mapbusy D584,
 * usrseg 8DC0 (Segment fuer Nutzerzugriffe), Texte 8C76 "blkdev", 8C7D "devtab", 8C84 "IO err in swap". */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "user_ext.h"
#include "inode.h"
#include "proc.h"
#include "buf.h"
#include "conf.h"

extern struct buf swbuf1, swbuf2;
extern int swapmap[];
extern int mapq[4];
extern char mapbusy;
extern int usrseg;

/* KERNEL 57D4 */
struct buf *
bread(dev, blkno)
dev_t dev;
daddr_t blkno;
{
	register struct buf *bp;

	bp = getblk(dev, blkno);
	if (bp->b_flags & B_DONE)
		return(bp);
	bp->b_flags |= B_READ;
	bp->b_bcount = BSIZE;
	(*bdevsw[major(dev)].d_strategy)(bp);
	iowait(bp);
	return(bp);
}

/* KERNEL 581A */
struct buf *
breada(dev, blkno, rablkno)
dev_t dev;
daddr_t blkno, rablkno;
{
	register struct buf *bp, *rabp;

	bp = NULL;
	if (!incore(dev, blkno)) {
		bp = getblk(dev, blkno);
		if ((bp->b_flags & B_DONE) == 0) {
			bp->b_flags |= B_READ;
			bp->b_bcount = BSIZE;
			(*bdevsw[major(dev)].d_strategy)(bp);
		}
	}
	if (rablkno && !incore(dev, rablkno) && bfreelist.av_forw != &bfreelist) {
		rabp = getblk(dev, rablkno);
		if (rabp->b_flags & B_DONE)
			brelse(rabp);
		else {
			rabp->b_flags |= B_READ|B_ASYNC;
			rabp->b_bcount = BSIZE;
			(*bdevsw[major(dev)].d_strategy)(rabp);
		}
	}
	if (bp == NULL)
		return(bread(dev, blkno));
	iowait(bp);
	return(bp);
}

/* KERNEL 58F0 */
bwrite(bp)
register struct buf *bp;
{
	register flag;

	flag = bp->b_flags;
	bp->b_flags &= ~(B_READ | B_DONE | B_ERROR | B_DELWRI | B_AGE);
	bp->b_bcount = BSIZE;
	(*bdevsw[major(bp->b_dev)].d_strategy)(bp);
	if ((flag & B_ASYNC) == 0) {
		iowait(bp);
		brelse(bp);
	} else if (flag & B_DELWRI)
		bp->b_flags |= B_AGE;
	else
		geterror(bp);
}

/* KERNEL 594A */
bdwrite(bp)
register struct buf *bp;
{
	register struct buf *dp;

	dp = (struct buf *)bdevsw[major(bp->b_dev)].d_tab;
	if (dp->b_flags & B_TAPE)
		bawrite(bp);
	else {
		bp->b_flags |= B_DELWRI | B_DONE;
		brelse(bp);
	}
}

/* KERNEL 5984 */
bawrite(bp)
register struct buf *bp;
{
	bp->b_flags |= B_ASYNC;
	bwrite(bp);
}

/* KERNEL 599A */
brelse(bp)
register struct buf *bp;
{
	register struct buf **backp;
	register s;

	if (bp->b_flags & B_WANTED)
		wakeup((caddr_t)bp);
	if (bfreelist.b_flags & B_WANTED) {
		bfreelist.b_flags &= ~B_WANTED;
		wakeup((caddr_t)&bfreelist);
	}
	if (bp->b_flags & B_ERROR)
		bp->b_dev = (dev_t)NODEV;
	s = spl6();
	if (bp->b_flags & B_AGE) {
		backp = &bfreelist.av_forw;
		(*backp)->av_back = bp;
		bp->av_forw = *backp;
		*backp = bp;
		bp->av_back = &bfreelist;
	} else {
		backp = &bfreelist.av_back;
		(*backp)->av_forw = bp;
		bp->av_back = *backp;
		*backp = bp;
		bp->av_forw = &bfreelist;
	}
	bp->b_flags &= ~(B_WANTED|B_BUSY|B_ASYNC|B_AGE);
	splx(s);
}

/* KERNEL 5A22 */
incore(dev, blkno)
dev_t dev;
daddr_t blkno;
{
	register struct buf *bp, *dp;

	dp = (struct buf *)bdevsw[major(dev)].d_tab;
	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw)
		if (bp->b_blkno == blkno && bp->b_dev == dev)
			return(1);
	return(0);
}

/* KERNEL 5A64 */
struct buf *
getblk(dev, blkno)
dev_t dev;
daddr_t blkno;
{
	register struct buf *bp, *dp;

	if (major(dev) >= nblkdev)
		panic("blkdev");
loop:
	spl0();
	dp = (struct buf *)bdevsw[major(dev)].d_tab;
	if (dp == NULL)
		panic("devtab");
	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw) {
		if (bp->b_blkno != blkno || bp->b_dev != dev)
			continue;
		spl6();
		if (bp->b_flags & B_BUSY) {
			bp->b_flags |= B_WANTED;
			sleep((caddr_t)bp, PRIBIO+1);
			goto loop;
		}
		spl0();
		notavail(bp);
		return(bp);
	}
	spl6();
	if (bfreelist.av_forw == &bfreelist) {
		bfreelist.b_flags |= B_WANTED;
		sleep((caddr_t)&bfreelist, PRIBIO+1);
		goto loop;
	}
	spl0();
	notavail(bp = bfreelist.av_forw);
	if (bp->b_flags & B_DELWRI) {
		bp->b_flags |= B_ASYNC;
		bwrite(bp);
		goto loop;
	}
	bp->b_flags = B_BUSY;
	bp->b_back->b_forw = bp->b_forw;
	bp->b_forw->b_back = bp->b_back;
	bp->b_forw = dp->b_forw;
	bp->b_back = dp;
	dp->b_forw->b_back = bp;
	dp->b_forw = bp;
	bp->b_dev = dev;
	bp->b_blkno = blkno;
	return(bp);
}

/* KERNEL 5B70 */
struct buf *
geteblk()
{
	register struct buf *bp, *dp;

loop:
	spl6();
	while (bfreelist.av_forw == &bfreelist) {
		bfreelist.b_flags |= B_WANTED;
		sleep((caddr_t)&bfreelist, PRIBIO+1);
	}
	spl0();
	dp = &bfreelist;
	notavail(bp = bfreelist.av_forw);
	if (bp->b_flags & B_DELWRI) {
		bp->b_flags |= B_ASYNC;
		bwrite(bp);
		goto loop;
	}
	bp->b_flags = B_BUSY;
	bp->b_back->b_forw = bp->b_forw;
	bp->b_forw->b_back = bp->b_back;
	bp->b_forw = dp->b_forw;
	bp->b_back = dp;
	dp->b_forw->b_back = bp;
	dp->b_forw = bp;
	bp->b_dev = (dev_t)NODEV;
	return(bp);
}

/* KERNEL 5BFC */
iowait(bp)
register struct buf *bp;
{
	spl6();
	while ((bp->b_flags & B_DONE) == 0)
		sleep((caddr_t)bp, PRIBIO);
	spl0();
	geterror(bp);
}

/* KERNEL 5C2E */
notavail(bp)
register struct buf *bp;
{
	register s;

	s = spl6();
	bp->av_back->av_forw = bp->av_forw;
	bp->av_forw->av_back = bp->av_back;
	bp->b_flags |= B_BUSY;
	splx(s);
}

/* KERNEL 5C62 */
iodone(bp)
register struct buf *bp;
{
	bp->b_flags |= B_DONE;
	if (bp->b_flags & B_ASYNC)
		brelse(bp);
	else {
		bp->b_flags &= ~B_WANTED;
		wakeup((caddr_t)bp);
	}
}

/* KERNEL 5C86 */
clrbuf(bp)
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

/* KERNEL 5CAE */
mapalloc(bp, n)
register bp, n;
{
	while (mapq[n >> 1])
		if (!mapbusy)
			mapstart();
	mapq[n >> 1] = bp;
	if (!mapbusy)
		mapstart();
}

/* KERNEL 5CF6 */
mapstart()
{
	register bp, i;

	mapbusy++;
again:
	bp = 0;
	for (i = 0; i < 4; i++)
		if (mapq[i]) {
			bp = mapq[i];
			while (*(int *)06)
				;
			*(int *)06 = bp;
			mapaddr();
			mapq[i] = 0;
		}
	if (bp)
		goto again;
	mapbusy = 0;
}

/* KERNEL 5D4A */
swap(blkno, coreaddr, count, rdflg, up)
register blkno;
int coreaddr;
register count;
int rdflg;
register struct user *up;
{
	register struct buf *bp;
	register c, t;

	if (count >= 0x104) {
		swap(blkno, ((unsigned)up + 0xFF) >> 8, 4, rdflg);
		blkno += 2;
		t = 2;
		c = up->u_tsize + up->u_dsize;
		swap(blkno, coreaddr, c, rdflg);
		c = ctod(c);
		t += c;
		blkno += c;
		c = *(int *)((char *)up + 0x104);
		c += 0x400;
		c += (int)up;
		c = *(int *)(c + 16);
		c = ((unsigned)c >> 8) & ~1;
		coreaddr += c;
		c = 0x100 - c;
		swap(blkno, coreaddr, c, rdflg);
		t += ctod(c);
		blkno += ctod(c);
		if (rdflg == B_READ)
			mfree(swapmap, t, blkno - t);
		return;
	}
	bp = &swbuf1;
	if ((bp->b_flags & B_BUSY) && (swbuf2.b_flags & B_WANTED) == 0)
		bp = &swbuf2;
	spl6();
	while (bp->b_flags & B_BUSY) {
		bp->b_flags |= B_WANTED;
		sleep((caddr_t)bp, PSWP+1);
	}
	while (count) {
		bp->b_flags = B_BUSY | B_PHYS | rdflg;
		bp->b_dev = swapdev;
		c = count;
		if (c > 16)
			c = 16;
		bp->b_bcount = c << 8;
		bp->b_blkno = (daddr_t)blkno + swplo;
		bp->b_addr = (caddr_t)(coreaddr << 8);
		bp->b_xmem = (coreaddr >> 8) & 0177;
		(*bdevsw[major(swapdev)].d_strategy)(bp);
		spl6();
		while ((bp->b_flags & B_DONE) == 0)
			sleep((caddr_t)bp, PSWP);
		count -= c;
		coreaddr += c;
		blkno += ctod(c);
	}
	if (bp->b_flags & B_WANTED)
		wakeup((caddr_t)bp);
	spl0();
	bp->b_flags &= ~(B_BUSY|B_WANTED);
	if (bp->b_flags & B_ERROR)
		panic("IO err in swap");
}

/* KERNEL 5F10 */
bflush(dev)
dev_t dev;
{
	register struct buf *bp;

loop:
	spl6();
	for (bp = bfreelist.av_forw; bp != &bfreelist; bp = bp->av_forw) {
		if ((bp->b_flags & B_DELWRI) && (dev == NODEV || dev == bp->b_dev)) {
			bp->b_flags |= B_ASYNC;
			notavail(bp);
			bwrite(bp);
			goto loop;
		}
	}
	spl0();
}

/* KERNEL 5F56 */
physio(strat, bp, dev, rw)
int (*strat)();
register struct buf *bp;
{
	register unsigned base;
	register nb;
	int ts;

	base = (unsigned)u.u_base;
	if ((base & 01) || (u.u_count & 0777) || u.u_count == 0 || base >= base + u.u_count)
		goto bad;
	ts = u.u_tsize;
	if (U_SEP)
		ts = 0;
	nb = (base >> 8) & 0377;
	if (nb < ts)
		goto bad;
	if ((((base + u.u_count) >> 8) & 0377) >= ts + u.u_dsize && *(unsigned *)(U_P104 + 16) > (unsigned)u.u_base)
		goto bad;
	if (((u.u_count + 0377) >> 8) > 16)
		goto bad;
	spl6();
	while (bp->b_flags & B_BUSY) {
		bp->b_flags |= B_WANTED;
		sleep((caddr_t)bp, PRIBIO+1);
	}
	bp->b_flags = B_BUSY | B_PHYS | rw;
	bp->b_dev = dev;
	bp->b_addr = (caddr_t)base;
	bp->b_xmem = (usrseg >> 8) & 0377;
	bp->b_blkno = u.u_offset >> BSHIFT;
	bp->b_bcount = u.u_count;
	bp->b_error = 0;
	u.u_procp->p_flag |= SLOCK;
	(*strat)(bp);
	spl6();
	while ((bp->b_flags & B_DONE) == 0)
		sleep((caddr_t)bp, PRIBIO);
	u.u_procp->p_flag &= ~SLOCK;
	if (bp->b_flags & B_WANTED)
		wakeup((caddr_t)bp);
	spl0();
	bp->b_flags &= ~(B_BUSY|B_WANTED);
	u.u_count = bp->b_resid;
	geterror(bp);
	return;
bad:
	u.u_error = EFAULT;
}

/* KERNEL 60A8 */
geterror(bp)
register struct buf *bp;
{
	if (bp->b_flags & B_ERROR)
		if ((u.u_error = bp->b_error) == 0)
			u.u_error = EIO;
}

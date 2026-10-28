/* main.c -- Systemstart (Kernel mutos_1216, 0674..087A): main, iinit, binit.  cinit (3D04) steht im Kernel hinter pipe.c, siehe ureg.c.
 * Daten: proc[0] = A11C, rootdir = 8DC6, szicode = 8B4C, icode = 8B32 (ab 8B42 der String "/etc/init"), rootdev 887C,
 * Text 88C6 "iinit", Puffer: buf = 8E9A (24 Byte je), Datenpuffer buffers = A628 (512 Byte je), bfreelist = A518,
 * mount[0] = A07E (m_dev, m_bufp), time = 8DCC. */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "proc.h"
#include "text.h"
#include "map.h"
#include "inode.h"
#include "file.h"
#include "buf.h"
#include "conf.h"
#include "filsys.h"
#include "mount.h"

#define btoc(x)		((unsigned)((x)+255)>>8)
#define USIZE		4

extern char icode[];
extern int szicode;
extern struct inode *rootdir;
extern char buffers[NBUF][512];

/* KERNEL 0674 */
main()
{
	startup();
	proc[0].p_addr = 0;
	proc[0].p_size = 0;
	proc[0].p_stat = SRUN;
	proc[0].p_flag |= SLOAD|SSYS;
	proc[0].p_nice = NZERO;
	u.u_procp = &proc[0];
	u.u_cmask = 0;
	kstub();
	spl0();
	cinit();
	binit();
	iinit();
	rootdir = iget(rootdev, ROOTINO);
	rootdir->i_flag &= ~ILOCK;
	u.u_cdir = iget(rootdev, ROOTINO);
	u.u_cdir->i_flag &= ~ILOCK;
	u.u_rdir = NULL;
	if (newproc()) {
		expand(USIZE+btoc(szicode));
		estabur(0, btoc(szicode), 0, 0, 2);
		copyout((caddr_t)icode, (caddr_t)0, szicode);
		return;
	}
	sched();
}

/* KERNEL 0746 */
iinit()
{
	register struct buf *cp, *bp;
	register struct filsys *fp;

	(*bdevsw[major(rootdev)].d_open)(rootdev, 1);
	bp = bread(rootdev, (daddr_t)1);
	cp = geteblk();
	if (u.u_error)
		panic("iinit");
	bcopy((caddr_t)bp->b_addr, (caddr_t)cp->b_addr, 0x1B8);
	brelse(bp);
	mount[0].m_bufp = cp;
	mount[0].m_dev = rootdev;
	fp = (struct filsys *)cp->b_addr;
	fp->s_flock = 0;
	fp->s_ilock = 0;
	fp->s_ronly = 0;
	time = fp->s_time;
}

/* KERNEL 07D2 */
binit()
{
	register struct buf *bp;
	register struct devtab *dp;
	register i;
	struct bdevsw *bdp;

	bfreelist.b_forw = bfreelist.b_back = bfreelist.av_forw = bfreelist.av_back = &bfreelist;
	for (i = 0; i < NBUF; i++) {
		bp = &buf[i];
		bp->b_dev = NODEV;
		bp->b_addr = buffers[i];
		bp->b_error = 0;
		bp->b_back = &bfreelist;
		bp->b_forw = bfreelist.b_forw;
		bfreelist.b_forw->b_back = bp;
		bfreelist.b_forw = bp;
		bp->b_flags = B_BUSY;
		brelse(bp);
	}
	for (bdp = bdevsw; bdp->d_open; bdp++) {
		dp = bdp->d_tab;
		if (dp) {
			dp->b_forw = (struct buf *)dp;
			dp->b_back = (struct buf *)dp;
		}
		nblkdev++;
	}
}

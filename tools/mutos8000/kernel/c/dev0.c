/* dev0.c -- Treiber Major 0 / cdevsw[10] (Kernel mutos_1216, 7E16..85B0, im Lauf nie erreicht): Floppy ueber die Mailbox (WEGA-fd-Muster).
 * Zurueckgebaut: fdopen 7E16, fdclose 7E94, fdsub 7F00, fdstrategy 7F94, fdread 84E4, fdwrite 851C, fdphys 8554.
 * OFFEN: fdstart 7FDC (574 B, Zustandsautomat in fdtab+C... 8E76, Mailbox-Befehle 6/Sektor, Seek) und fdioctl 85B0 (426 B, switch ueber 6D01.. mit Sprungtabelle).
 * Dieses Modul ist im Kernel OHNE Optimierer uebersetzt (alle Spruenge JP statt JR, Rumpf hinter `JP`-Sprung am Funktionsanfang): beim
 * Vergleich die Zeile ohne -O (Datei <name>.o) benutzen, nicht <name>O.o.
 * Daten: fdtab 8E6A (struct buf-Kopf wie rktab; b_bcount = 8E76 Zustand), fdu D566 (16 Byte je Laufwerk, 1 Laufwerk), fdmbx D558 (Mailbox 14 Byte),
 * fdbuf D540 (Puffer fuer fdsub), rfdbuf CEAC (physio). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "buf.h"
#include "conf.h"
#include "mbx.h"

struct fdu {
	char		fu_flag;	/* 00 offen */
	char		fu_x1;		/* 01 */
	daddr_t		fu_blk;		/* 02 */
	int		fu_x6;		/* 06 */
	int		fu_x8, fu_xa, fu_xc;
	int		fu_init;	/* 0E */
};
extern struct fdu fdu[1];		/* D566 */
extern struct rkmbx fdmbx[1];		/* D558 */
extern struct buf fdbuf, rfdbuf, fdtab;	/* D540, CEAC, 8E6A */

/* KERNEL 7E16 */
fdopen(dev, flag)
dev_t dev;
{
	register d;
	register struct rkmbx *mp;
	register struct fdu *fp;

	d = dev & 7;
	if (d >= 1 || (fp = &fdu[d])->fu_flag) {
		u.u_error = ENXIO;
		return;
	}
	if (fp->fu_init == 0) {
		mp = &fdmbx[d];
		mp->mb_stat = 1;
		fp->fu_init = 1;
	}
	fdtab.b_flags |= 0x400;
	fp->fu_flag = 1;
	fp->fu_blk = 0;
	fp->fu_x6 = 0x7FFF;
	fp->fu_x1 = 0;
}

/* KERNEL 7E94 */
fdclose(dev, flag)
register dev, flag;
{
	register struct fdu *fp;

	fp = &fdu[dev & 7];
	if (flag == 2 || ((flag & 2) && fp->fu_x1))
		fdsub(dev, 0x61, 1);
	if ((dev & 0x80) == 0)
		fdsub(dev, 0x51, 0);
	fp->fu_flag = 0;
}

/* KERNEL 7F00 */
fdsub(dev, a, b)
{
	register struct buf *bp;

	bp = &fdbuf;
	spl5();
	while (bp->b_flags & B_BUSY) {
		if (bp->b_bcount == 0 && (bp->b_flags & B_DONE))
			break;
		bp->b_flags |= B_WANTED;
		sleep(bp, PRIBIO);
	}
	bp->b_flags = B_BUSY | B_READ;
	spl0();
	bp->b_dev = dev;
	bp->b_bcount = b;
	bp->b_resid = a;
	bp->b_blkno = 0;
	fdstrategy(bp);
	if (b) {
		iowait(bp);
		if (bp->b_flags & B_WANTED)
			wakeup(bp);
		bp->b_flags &= B_ERROR;
	}
}

/* KERNEL 7F94 */
fdstrategy(bp)
register struct buf *bp;
{
	bp->av_forw = NULL;
	spl5();
	if (fdtab.av_forw == NULL)
		fdtab.av_forw = bp;
	else
		fdtab.av_back->av_forw = bp;
	fdtab.av_back = bp;
	if (fdtab.b_bcount == 0)
		fdstart();
	spl0();
}

/* KERNEL 84E4 */
fdread(dev)
dev_t dev;
{
	fdphys(dev);
	if (u.u_error)
		return;
	physio(fdstrategy, &rfdbuf, dev, B_READ);
}

/* KERNEL 851C */
fdwrite(dev)
dev_t dev;
{
	fdphys(dev);
	if (u.u_error)
		return;
	physio(fdstrategy, &rfdbuf, dev, B_WRITE);
}

/* KERNEL 8554 */
fdphys(dev)
dev_t dev;
{
	register d;
	register struct fdu *fp;
	long lb;

	d = dev & 7;
	if (d >= 1) {
		u.u_error = ENXIO;
		return;
	}
	fp = &fdu[d];
	lb = u.u_offset >> 9;
	fp->fu_blk = lb;
	fp->fu_x6 = (int)lb + 1;
}

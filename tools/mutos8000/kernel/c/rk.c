/* rk.c -- Geraete hinter der Mailbox und /dev/mem (Kernel mutos_1216, 69CE..6D44): mmread, mmwrite (cdevsw[2]),
 * rkopen, rkclose, rkstrategy, rkstart, rkintr, rkread, rkwrite (Major 4, Plattenersatz ueber die Mailbox D532 zur U880-Seite),
 * syopen, syread, sywrite, syioctl (/dev/tty, V7 sys.c; K6b-Karte kannte sie als 6C8E/6CC6/6CEC/6D12).
 * Daten: rktab 8E82 (Aufbau devtab, dahinter: Merker "Befehl aktiv" 8E8E, Fehlerzaehler 8E98), Mailbox D532 (Aufbau siehe unten),
 * rrkbuf CE94 (Puffer fuer physio), usrseg 8DC0, cdevsw 8796.
 * Mailbox (Entwurf 39 §11): +0 Befehl, +1 Einheit, +2 Richtung (0 = lesen, 1 = schreiben), +3 Status (1 = ok, C8 = Einheit fehlt),
 * +4 Segment, +6 Adresse, +8 Zaehler, +A Blocknummer (long); rkstart startet ueber mapalloc(mp, 0) (5CAE). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "user_ext.h"
#include "inode.h"
#include "proc.h"
#include "buf.h"
#include "conf.h"
#include "mbx.h"

struct rkt {
	char		rk_active;
	char		rk_errcnt;
	struct buf	*rk_forw;
	struct buf	*rk_back;
	struct buf	*rk_actf;
	struct buf	*rk_actl;
	int		rk_x;
	int		rk_busy;	/* 8E8E */
	int		rk_y[4];
	unsigned	rk_errs;	/* 8E98 */
};
extern struct rkt rktab;
extern struct buf rrkbuf;
extern int usrseg;
#define MBX	((struct rkmbx *)0xD532)

/* KERNEL 69CE */
mmread(dev)
dev_t dev;
{
	register c, bn, on;
	int a, d;

	if (minor(dev) == 2)
		return;
	do {
		bn = u.u_offset >> 16;
		on = u.u_offset;
		a = usrseg;
		spl7();
		usrseg = bn << 8;
		if (minor(dev) == 1)
			usrseg = 0;
		c = fubyte(on);
		if (c < 0)
			u.u_error = ENXIO;
		usrseg = a;
		spl0();
	} while (u.u_error == 0 && passc(c) >= 0);
}

/* KERNEL 6A52 */
mmwrite(dev)
dev_t dev;
{
	register c, bn, on;
	int a, d;

	if (minor(dev) == 2) {
		u.u_count = 0;
		return;
	}
	for (;;) {
		bn = u.u_offset >> 16;
		on = u.u_offset;
		c = cpass();
		if (c < 0)
			return;
		if (u.u_error)
			return;
		a = usrseg;
		spl7();
		usrseg = bn << 8;
		if (minor(dev) == 1)
			usrseg = 0;
		if (subyte(on, c) < 0)
			u.u_error = ENXIO;
		usrseg = a;
		spl0();
	}
}

/* KERNEL 6ADA */
rkopen(dev)
dev_t dev;
{
	int d, x;

	d = minor(dev);
	if (d >= 4)
		u.u_error = ENXIO;
}

/* KERNEL 6AFE */
rkclose(dev)
dev_t dev;
{
	register i;
}

/* KERNEL 6B06 */
rkstrategy(bp)
register struct buf *bp;
{
	if (bp->b_blkno >= 0x260L) {
		bp->b_flags |= B_ERROR;
		bp->b_error = ENXIO;
		iodone(bp);
		return;
	}
	bp->av_forw = NULL;
	spl5();
	if (rktab.rk_actf == NULL)
		rktab.rk_actf = bp;
	else
		rktab.rk_actl->av_forw = bp;
	rktab.rk_actl = bp;
	if (rktab.rk_busy == 0)
		rkstart();
	spl0();
}

/* KERNEL 6B60 */
rkstart()
{
	register struct rkmbx *mp;
	register struct buf *bp;
	int pad[5];

	if ((bp = rktab.rk_actf) == NULL)
		return;
	mp = MBX;
	rktab.rk_busy++;
	mp->mb_cmd = 0;
	mp->mb_unit = minor(bp->b_dev);
	mp->mb_stat = 0;
	mp->mb_seg = bp->b_xmem;
	mp->mb_addr = bp->b_addr;
	mp->mb_cnt = bp->b_bcount;
	mp->mb_blk = bp->b_blkno;
	if (bp->b_flags & B_READ)
		mp->mb_dir = 0;
	else
		mp->mb_dir = 1;
	mapalloc(mp, 0);
}

/* KERNEL 6BCA */
rkintr()
{
	register c1, c2;
	register struct buf *bp;
	register struct rkmbx *mp;

	if (rktab.rk_busy == 0)
		return;
	mp = MBX;
	bp = rktab.rk_actf;
	rktab.rk_busy = 0;
	if (mp->mb_stat != 1) {
		if (mp->mb_stat == 0xC8) {
			bp->b_error = ENXIO;
		fail:
			bp->b_flags |= B_ERROR;
		} else {
			c1 = mp->mb_dir;
			c2 = mp->mb_stat;
			deverror(bp, c1 & 0377, c2 & 0377);
			if (++rktab.rk_errs > 3) {
				goto fail;
			}
			rkstart();
			return;
		}
	}
	rktab.rk_errs = 0;
	rktab.rk_actf = bp->av_forw;
	bp->b_resid = 0;
	iodone(bp);
	rkstart();
}

/* KERNEL 6C52 */
rkread(dev)
dev_t dev;
{
	physio(rkstrategy, &rrkbuf, dev, B_READ);
}

/* KERNEL 6C70 */
rkwrite(dev)
dev_t dev;
{
	physio(rkstrategy, &rrkbuf, dev, B_WRITE);
}

/* KERNEL 6C8E */
syopen(dev, flag)
dev_t dev;
{
	if (U_TTYP == 0) {
		u.u_error = ENXIO;
		return;
	}
	(*cdevsw[major(U_TTYD)].d_open)(U_TTYD, flag);
}

/* KERNEL 6CC6 */
syread(dev)
dev_t dev;
{
	(*cdevsw[major(U_TTYD)].d_read)(U_TTYD);
}

/* KERNEL 6CEC */
sywrite(dev)
dev_t dev;
{
	(*cdevsw[major(U_TTYD)].d_write)(U_TTYD);
}

/* KERNEL 6D12 */
syioctl(dev, cmd, arg, flag)
dev_t dev;
{
	(*cdevsw[major(U_TTYD)].d_ioctl)(U_TTYD, cmd, arg, flag);
}

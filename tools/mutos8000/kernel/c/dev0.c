/* dev0.c -- Treiber Major 0 / cdevsw[10] (Kernel mutos_1216, 7E16..8760, im Lauf nie erreicht): Floppy ueber die Mailbox (WEGA-fd-Muster).
 * fdopen 7E16, fdclose 7E94, fdsub 7F00, fdstrategy 7F94, fdstart 7FDC, fdintr 821A, fdadj 8468, fdread 84E4, fdwrite 851C, fdphys 8554, fdioctl 85B0.
 * fdintr und fdadj haben im Kernel keinen Aufrufer ueber die Aufrufkarte (fdintr kommt aus dem Interrupt) und standen deshalb nicht in
 * funktionen.txt; die Uebersicht (uebersicht.py) zaehlt sie nicht mit.
 * Dieses Modul ist im Kernel OHNE Optimierer uebersetzt (alle Spruenge JP statt JR, Rumpf hinter einem `JP` am Funktionsanfang): beim
 * Vergleich die Zeile ohne -O (Datei <name>.o) benutzen, nicht <name>O.o.
 * Daten: fdtab 8E6A (struct buf-Kopf wie rktab; b_bcount 8E76 = Zustand, b_resid 8E80 = Wiederholungen), fdu D566 (16 Byte je Laufwerk, 1 Laufwerk),
 * fdmbx D558 (Mailbox 14 Byte), fdbuf D540 (Puffer fuer fdsub), rfdbuf CEAC (physio).  Die Tabelle fdcmd 8D6A und die Sprungtabellen der
 * switch-Anweisungen (8D5E, 8D64, 8D78, 8D88) liegen hinter den Tabellen von tty2.c. */
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
	unsigned	fu_x6;		/* 06 (unsigned: fdstart vergleicht mit LD R5 / XOR R4,R4) */
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

/* fdstart: naechsten Auftrag aus fdtab holen und ueber die Mailbox starten (fdtab.b_bcount = Zustand: 1 Positionieren, 2 Lesen/Schreiben,
 * 3 Befehl der Steuerung (fdbuf), 4 Befehl 51H); fdtab.b_resid zaehlt Wiederholungen.  b_resid des Auftrags traegt den Mailbox-Befehl
 * (25H/27H Spur vor/zurueck, 0/1 Lesen/Schreiben, sonst die Steuerbefehle von fdsub). */
/* KERNEL 7FDC */
fdstart()
{
	register struct fdu *fp;
	register struct rkmbx *mp;
	register struct buf *bp;
	int d, x;
	long lb;

	while ((bp = fdtab.av_forw) != NULL) {
		d = bp->b_dev & 7;
		fp = &fdu[d];
		mp = &fdmbx[d];
		fp->fu_x8 = mp->mb_stat;
		fp->fu_xa = mp->mb_dir;
		fp->fu_xc = mp->mb_cnt;
		fp->fu_x1 = 0;
		if (mp->mb_stat == 0xF5 && fdtab.b_bcount == 2) {
			bp->b_resid = 0x25;
			mp->mb_cnt = 1;
			fdtab.b_bcount = 1;
			goto issue;
		}
		if (fp->fu_flag == -1 || fp->fu_x8 == 0) {
			bp->b_flags |= B_ERROR;
			goto done;
		}
		if (bp == &fdbuf) {
			if (bp->b_resid == 6)
				goto done;
			if (bp->b_resid == 0x51)
				fdtab.b_bcount = 4;
			else
				fdtab.b_bcount = 3;
			if (bp->b_resid == 0x25 || bp->b_resid == 0x27)
				mp->mb_cnt = bp->b_bcount;
			goto issue;
		}
		if (bp->b_blkno > fp->fu_x6) {
			bp->b_flags |= B_ERROR;
			bp->b_error = ENXIO;
			goto done;
		}
		if (bp->b_blkno == fp->fu_x6 && (bp->b_flags & B_READ)) {
			bp->b_resid = bp->b_bcount;
			clrbuf(bp);
			goto done;
		}
		if ((bp->b_flags & B_READ) == 0)
			fp->fu_x6 = (int)bp->b_blkno + 1;
		lb = fp->fu_blk;
		if (lb == bp->b_blkno) {
			if ((bp->b_flags & B_READ) == 0) {
				if (fdtab.b_resid)
					x = 1;
				else
					x = 1;
			} else
				x = 0;
			fdtab.b_bcount = 2;
			mp->mb_cnt = bp->b_bcount;
			mp->mb_addr = bp->b_addr;
			mp->mb_seg = bp->b_xmem & 3;
			bp->b_resid = x;
			goto issue;
		}
		fdtab.b_bcount = 1;
		if (lb < bp->b_blkno) {
			bp->b_resid = 0x25;
			mp->mb_cnt = (int)bp->b_blkno - (int)lb;
		} else {
			bp->b_resid = 0x27;
			mp->mb_cnt = (int)lb - (int)bp->b_blkno;
		}
	issue:
		mp->mb_cmd = 6;
		mp->mb_unit = d;
		mp->mb_dir = bp->b_resid;
		mp->mb_stat = 0;
		mapalloc(mp, 6);
		return;
	done:
		fdtab.av_forw = bp->av_forw;
		iodone(bp);
	}
}

/* fdintr: Ende eines Mailbox-Auftrags (Interrupt; kein Aufrufer im Kernel, nie gelaufen) */
/* KERNEL 821A */
fdintr()
{
	register struct rkmbx *mp;
	register struct buf *bp;
	register struct fdu *fp;
	register s;
	int d;

	if ((bp = fdtab.av_forw) == NULL)
		return;
	d = bp->b_dev & 7;
	fp = &fdu[d];
	mp = &fdmbx[d];
	if (fdtab.b_bcount == 4) {
		fdtab.b_bcount = 3;
		if (mp->mb_stat == 0)
			return;
	}
	fp->fu_xa = mp->mb_dir;
	fp->fu_x8 = mp->mb_stat;
	fp->fu_xc = mp->mb_cnt;
	if ((bp->b_flags & B_READ) == 0)
		fp->fu_x1 = 1;
	s = fdtab.b_bcount;
	fdtab.b_bcount = 0;
	if (mp->mb_stat == 1)
		goto ok;
	if (mp->mb_stat == 0xF5) {
		fdadj(bp);
		if (s == 2) {
			fdtab.b_bcount = s;
			fdstart();
			return;
		}
		s = 3;
		fp->fu_xc = 0;
		mp->mb_cnt = 0;
		goto fin;
	}
	if (bp == &rfdbuf && (bp->b_flags & B_READ) && mp->mb_stat == 0xF6)
		goto ok;
	if (!(mp->mb_stat == 0xC1 && mp->mb_stat == 0xF3) && s == 2) {
		if (++fdtab.b_resid < 7) {
			fp->fu_blk++;
			goto start;
		}
		goto report;
	}
	if (fp->fu_flag && bp != &rfdbuf)
		fp->fu_flag = -1;
report:
	printf("tm%d: bn = %d %d old er = %x err stat = %x\n", d, bp->b_blkno, fp->fu_x8, mp->mb_stat & 0377);
	bp->b_flags |= B_ERROR;
	goto fin;
ok:
	switch (s) {

	case 2:
		fp->fu_blk++;
		goto fin;

	case 3:
		if (bp == &fdbuf) {
			switch (bp->b_resid) {
			case 0x25:
				fp->fu_blk -= bp->b_bcount;
				break;
			case 0x27:
				fp->fu_blk += bp->b_bcount;
				break;
			}
		}
		goto fin;

	case 1:
		fp->fu_blk = bp->b_blkno;
		goto start;

	default:
		printf("tmintr\n");
		bp->b_flags |= B_ERROR;
	}
fin:
	fdtab.b_resid = 0;
	fdtab.av_forw = bp->av_forw;
	bp->b_resid = bp->b_bcount - mp->mb_cnt;
	iodone(bp);
	if (fdtab.av_forw == 0)
		return;
start:
	fdstart();
}

/* fdadj: Blocknummer nach einem Positionsfehler nachfuehren.  Im Kernel ist `mp` (R12) NICHT belegt: die Funktion liest das R12 des
 * Aufrufers fdintr (dort bp); hier genauso als nicht initialisierte Registervariable nachgebaut. */
/* KERNEL 8468 */
fdadj(bp)
register struct buf *bp;
{
	register struct rkmbx *mp;
	register struct fdu *fp;

	fp = &fdu[bp->b_dev & 7];
	if (bp == &fdbuf) {
		if (fp->fu_blk > bp->b_blkno) {
			fp->fu_blk -= mp->mb_cnt;
			fp->fu_x6 = (int)fp->fu_blk;
		} else {
			fp->fu_blk += mp->mb_cnt;
			fp->fu_x6 = (int)fp->fu_blk - 1;
		}
	} else
		fp->fu_x6 = (int)bp->b_blkno;
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

/* Mailbox-Befehle von fdioctl 6D01 (Befehl 0..6): Lesen-Spur 61H, 45H, 47H, 25H/27H Spur vor/zurueck, 51H, 6 */
int fdcmd[] = { 0x61, 0x45, 0x47, 0x25, 0x27, 0x51, 6 };

/* fdioctl: 6D01 Auftrag an die Steuerung (Befehlscode, Anzahl), 6D02 Zustand lesen */
/* KERNEL 85B0 */
fdioctl(dev, cmd, arg, flag)
dev_t dev;
caddr_t arg;
{
	register struct fdu *fp;
	register struct buf *bp;
	register n;
	int d, x;
	int lv[2];
	int info[8];

	d = dev & 7;
	fp = &fdu[d];
	bp = &fdbuf;
	switch (cmd) {

	case 0x6D01:
		if (copyin(arg, (caddr_t)lv, 4)) {
			u.u_error = EFAULT;
			return;
		}
		switch (lv[0]) {
		case 0:
			n = lv[1];
			x = 1;
			break;
		case 1:
		case 2:
			n = lv[1];
			x = 0x7FFF;
			break;
		case 3:
		case 4:
			n = 1;
			x = lv[1];
			break;
		case 5:
		case 7:
			n = 1;
			x = 1;
			break;
		default:
			u.u_error = ENXIO;
			return;
		}
		if (n <= 0 || x == 0) {
			u.u_error = ENXIO;
			return;
		}
		while (--n >= 0) {
			fdsub(dev, fdcmd[lv[0]], x);
			if ((lv[0] == 3 || lv[0] == 4) && bp->b_resid) {
				u.u_error = EIO;
				break;
			}
			if (bp->b_flags & B_ERROR)
				break;
			if (fp->fu_x8 & 0xF4)
				break;
		}
		geterror(bp);
		return;

	case 0x6D02:
		info[1] = fp->fu_xa;
		info[2] = fp->fu_x8;
		info[3] = fp->fu_xc;
		info[0] = 4;
		if (copyout((caddr_t)info, arg, 16))
			u.u_error = EFAULT;
		return;

	default:
		u.u_error = ENXIO;
	}
}

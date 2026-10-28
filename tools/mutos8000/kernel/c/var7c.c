/* var7c.c -- Varianten (K7c): gleiche Marken wie die Module, Namen <= 7 Zeichen.  Nur zum Vergleichen. */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "proc.h"
#include "inode.h"
#include "file.h"
#include "buf.h"
#include "conf.h"
#include "map.h"

extern char buffers[NBUF][512];

/* KERNEL 07D2 */
binitA()
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
		bp->b_pad15 = 0;
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

/* KERNEL 2DF0 */
mallocB(mp, size)
struct map *mp;
{
	register a;
	register struct map *bp;

	for (bp = mp; bp->m_size; bp++) {
		if (bp - mp >= 15)
			break;
		if (bp->m_size >= size) {
			a = bp->m_addr;
			bp->m_addr += size;
			if ((bp->m_size -= size) == 0)
				do {
					bp++;
					(bp-1)->m_addr = bp->m_addr;
				} while ((bp-1)->m_size = bp->m_size);
			return(a);
		}
	}
	return(0);
}

/* KERNEL 3D42 */
esta1(nt, nd, ns, sep, xrw)
unsigned nt, nd, ns;
{
	if (nt + nd + ns <= 256)
		return(0);
	u.u_error = ENOMEM;
	return(-1);
}

/* KERNEL 3D42 */
esta2(nt, nd, ns, sep, xrw)
unsigned nt, nd, ns;
{
	if (nt + nd + ns > 256) {
		u.u_error = ENOMEM;
		return(-1);
	} else
		return(0);
}

/* KERNEL 3D42 */
esta3(nt, nd, ns, sep, xrw)
unsigned nt, nd, ns;
{
	register e;

	if (nt + nd + ns > 256) {
		e = ENOMEM;
		u.u_error = e;
		return(-1);
	}
	return(0);
}

/* KERNEL 51D6 */
rdwrA(mode)
register mode;
{
	register struct file *fp;
	register struct inode *ip;
	register struct a {
		int		fdes;
		char		*cbuf;
		unsigned	count;
	} *uap;

	uap = (struct a *)u.u_ap;
	fp = getf(uap->fdes);
	if (fp == NULL)
		return;
	if ((fp->f_flag & mode) == 0) {
		u.u_error = EBADF;
		return;
	}
	u.u_base = (caddr_t)uap->cbuf;
	u.u_count = uap->count;
	u.u_segflg = 0;
	if ((fp->f_flag & FPIPE) != 0) {
		if (mode == FREAD)
			readp(fp);
		else
			writep(fp);
	} else {
		ip = fp->f_inode;
		if (fp->f_flag & 030)
			u.u_offset = 0;
		else
			u.u_offset = fp->f_offset;
		if ((ip->i_mode & IFCHR) == 0)
			plock(ip);
		if (mode == FREAD)
			readi(ip);
		else
			writei(ip);
		if ((ip->i_mode & IFCHR) == 0)
			prele(ip);
		if ((fp->f_flag & 030) == 0)
			fp->f_offset += uap->count - u.u_count;
	}
	u.u_r.r_val1 = uap->count - u.u_count;
}

/* KERNEL 49F6 */
waitA()
{
	register f;
	register struct proc *p;

	f = 0;
loop:
	for (p = &proc[0]; p <= proctop; p++)
		if (u.u_procp->p_pid == p->p_ppid) {
			f++;
			if (p->p_stat == SZOMB) {
				u.u_r.r_val1 = p->p_pid;
				u.u_r.r_val2 = p->p_addr;
				u.u_cutime += *(time_t *)&p->p_size;
				u.u_cstime += *(time_t *)&p->p_textp;
				p->p_pid = 0;
				p->p_ppid = 0;
				p->p_pgrp = 0;
				p->p_sig = 0;
				p->p_flag = 0;
				p->p_wchan = 0;
				p->p_stat = NULL;
				if (p == proctop)
					while (proctop--, proctop->p_stat == NULL)
						;
				return;
			}
			if (p->p_stat == SSTOP) {
				if ((p->p_flag & SWTED) == 0) {
					p->p_flag |= SWTED;
					u.u_r.r_val1 = p->p_pid;
					u.u_r.r_val2 = (fsig(p) << 8) | 0177;
					return;
				}
				continue;
			}
		}
	if (f) {
		sleep((caddr_t)u.u_procp, PWAIT);
		goto loop;
	}
	u.u_error = ECHILD;
}

/* KERNEL 4EB0 */
chdirB(ipp)
register struct inode **ipp;
{
	register struct inode *ip;

	ip = namei(uchar, 0);
	if (ip == NULL)
		return;
	if ((ip->i_mode & IFMT) == IFDIR) {
		if (access(ip, IEXEC) == 0) {
			prele(ip);
			if (*ipp) {
				plock(*ipp);
				iput(*ipp);
			}
			*ipp = ip;
			return;
		}
	} else
		u.u_error = ENOTDIR;
	iput(ip);
}

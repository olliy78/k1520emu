/* sys1.c -- Prozesssteuerung (Kernel mutos_1216, 41AC..4C14): exece, getxfile, setregs, rexit, exit, wait, fork, sbreak.
 * V7-Aufbau mit Z8000-Eigenheiten: der Rahmen der Argumente liegt am oberen Ende des 64-KB-Segments des Prozesses
 * (ucp = -nc - 2), Magic E807 = nichtsegmentierter Kopf (16 Byte), E707 = grosser Kopf (40 Byte, Bit 8 gesetzt); u_ex[] ist der
 * eingelesene Kopf (u.u_exdata).  Die Argumentstrings gehen ueber 10 Swap-Bloecke (malloc(swapmap, 10)).
 * Nutzerraumzugriffe: fubyte = 04CC (Byte), fuword = 04DA (Wort), subyte = 0508, suword = 0514 (Karte K6b hat sie vertauscht).
 * Daten: regloc 88CC..88D4 (8 Byte: Registerindizes, setregs), Text 8C58 "Out of swap" (exece) -- ohne Zeilenende.
 * Die Namen 4AD4 `fork` und 4BD0 `sbreak` sind die V7-Namen; die Marken tragen die Kerneladresse. */
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
#include "reg.h"

#define btoc(x)		((unsigned)((x)+255)>>8)
#define ctod2(x)	(((unsigned)(x)+1)>>1)
#define SIZE_OF_BLK	10

extern char regloc[8];

struct xproc {
	char		xp_a[0x10];
	int		xp_xstat;
	time_t		xp_utime;
	time_t		xp_stime;
};

/* KERNEL 41AC */
exece()
{
	register nc;
	register char *cp;
	register struct buf *bp;
	register struct a {
		char	*fname;
		char	**argp;
		char	**envp;
	} *uap;
	int na;
	int ne;
	int ta;
	int bno;
	int ucp;
	int ap;
	int c;
	struct inode *ip;

	if ((ip = namei(uchar, 0)) == NULL)
		return;
	if (access(ip, IEXEC) || (ip->i_mode & IFMT) != IFREG || (ip->i_mode & 0111) == 0) {
		u.u_error = EACCES;
		goto bad2;
	}
	bno = 0;
	bp = 0;
	na = 0;
	ne = 0;
	nc = 0;
	uap = (struct a *)u.u_ap;
	bno = malloc(swapmap, SIZE_OF_BLK);
	if (bno == 0)
		panic("Out of swap");
	if (uap->argp) for (;;) {
		ap = 0;
		if (uap->argp) {
			ap = fuword((caddr_t)uap->argp);
			uap->argp++;
		}
		if (ap == 0 && uap->envp) {
			uap->argp = 0;
			if ((ap = fuword((caddr_t)uap->envp)) == 0)
				break;
			uap->envp++;
			ne++;
		}
		if (ap == 0)
			break;
		na++;
		if (ap == -1)
			u.u_error = EFAULT;
		do {
			if (nc >= 0x13FF)
				u.u_error = E2BIG;
			if ((c = fubyte((caddr_t)ap++)) < 0)
				u.u_error = EFAULT;
			if (u.u_error)
				goto bad;
			if ((nc & 0777) == 0) {
				if (bp)
					bdwrite(bp);
				bp = getblk(swapdev, bno + (nc>>BSHIFT) + swplo);
				cp = bp->b_addr;
			}
			nc++;
			*cp++ = c;
		} while (c > 0);
	}
	if (bp)
		bdwrite(bp);
	bp = 0;
	nc = (nc + 1) & ~1;
	ta = (na + 3) * 2;
	if (getxfile(ip, nc, ta) || u.u_error)
		goto bad;
	ucp = -nc - 2;
	ap = ucp - ta;
	u.u_ar0[8] = ap;
	suword((caddr_t)ap, na - ne);
	nc = 0;
	for (;;) {
		ap += 2;
		if (na == ne) {
			suword((caddr_t)ap, 0);
			ap += 2;
		}
		if (--na < 0)
			break;
		suword((caddr_t)ap, ucp);
		do {
			if ((nc & 0777) == 0) {
				if (bp)
					brelse(bp);
				bp = bread(swapdev, bno + (nc>>BSHIFT) + swplo);
				bp->b_flags |= B_AGE;
				bp->b_flags &= ~B_DELWRI;
				cp = bp->b_addr;
			}
			subyte((caddr_t)ucp++, (c = *cp++));
			nc++;
		} while (c & 0377);
	}
	suword((caddr_t)ap, 0);
	suword((caddr_t)ucp, 0);
	setregs();
bad:
	if (bp)
		brelse(bp);
	if (u.u_error)
		for (nc = 0; nc < SIZE_OF_BLK; nc++) {
			bp = getblk(swapdev, bno + nc + swplo);
			bp->b_flags |= B_AGE;
			bp->b_flags &= ~B_DELWRI;
			brelse(bp);
		}
	mfree(swapmap, SIZE_OF_BLK, bno);
bad2:
	iput(ip);
}

/* KERNEL 44B6 */
getxfile(ip, nargc, ucount)
register struct inode *ip;
{
	register unsigned ds;
	register sep;
	register unsigned ts, ss;
	register i;
	int overlay;
	long lsize;

	u.u_base = (caddr_t)&u.u_ex[0];
	u.u_count = 2;
	u.u_offset = 0;
	u.u_segflg = 1;
	readi(ip);
	if (u.u_error)
		goto bad;
	if (u.u_ex[0] & 0x100)
		u.u_count = 0x28;
	else
		u.u_count = 0x10;
	u.u_offset = 0;
	u.u_segflg = 1;
	u.u_base = (caddr_t)&u.u_ex[0];
	readi(ip);
	u.u_segflg = 0;
	if (u.u_error)
		goto bad;
	if (u.u_count) {
		u.u_error = ENOEXEC;
		goto bad;
	}
	sep = 0;
	overlay = 0;
	if (u.u_ex[0] == (int)0xE807) {
		lsize = (long)(unsigned)u.u_ex[2] + (unsigned)u.u_ex[1];
		u.u_ex[2] = lsize;
		if (lsize != (unsigned)u.u_ex[2]) {
			u.u_error = ENOMEM;
			goto bad;
		}
		u.u_ex[1] = 0;
	} else if (u.u_ex[0] == (int)0xE707) {
		lsize = (long)(unsigned)u.u_ex[15] + (unsigned)u.u_ex[14];
		u.u_ex[15] = lsize;
		if (lsize != (unsigned)u.u_ex[15]) {
			u.u_error = ENOMEM;
			goto bad;
		}
		u.u_ex[14] = 0;
	} else {
		u.u_error = ENOEXEC;
		goto bad;
	}
	if (u.u_ex[0] & 0x100) {
		if (u.u_ex[14] && (ip->i_flag & ITEXT) == 0 && ip->i_count != 1) {
			u.u_error = ETXTBSY;
			goto bad;
		}
	} else {
		if (u.u_ex[1] && (ip->i_flag & ITEXT) == 0 && ip->i_count != 1) {
			u.u_error = ETXTBSY;
			goto bad;
		}
	}
	if (u.u_ex[0] & 0x100) {
		ts = btoc(u.u_ex[14]);
		lsize = (long)(unsigned)u.u_ex[15] + (unsigned)u.u_ex[16];
	} else {
		ts = btoc(u.u_ex[1]);
		lsize = (long)(unsigned)u.u_ex[2] + (unsigned)u.u_ex[3];
	}
	if (lsize != (unsigned)lsize) {
		u.u_error = ENOMEM;
		goto bad;
	}
	ds = btoc((unsigned)lsize);
	ss = btoc(nargc + ucount) + 5;
	if (overlay) {
		if ((u.u_sep == 0 && btoc(ts) != btoc(u.u_tsize)) || nargc) {
			u.u_error = ENOMEM;
			goto bad;
		}
		ds = u.u_dsize;
		ss = u.u_ssize;
		sep = u.u_sep;
		xfree();
		xalloc(ip);
		if (u.u_ex[0] & 0x100)
			u.u_ar0[12] = u.u_ex[8] & ~1;
		else
			u.u_ar0[12] = u.u_ex[5] & ~1;
	} else {
		if (estabur(ts, ds, ss, sep, 2))
			return(1);
		u.u_prof.pr_scale = 0;
		xfree();
		for (i = 0x100; --i >= 0; )
			clearseg(u.u_procp->p_addr + i);
		xalloc(ip);
		estabur(0, ds, 0, 0, 2);
		u.u_base = 0;
		if (u.u_ex[0] & 0x100) {
			u.u_offset = (unsigned)u.u_ex[14] + 0x28;
			u.u_count = u.u_ex[15];
		} else {
			u.u_offset = (unsigned)u.u_ex[1] + 16;
			u.u_count = u.u_ex[2];
		}
		readi(ip);
		if (u.u_procp->p_flag & STRC)
			psignal(u.u_procp, SIGTRAP);
		else {
			if (ip->i_mode & ISUID)
				if (u.u_uid != 0) {
					u.u_uid = ip->i_uid;
					u.u_procp->p_uid = ip->i_uid;
				}
			if (ip->i_mode & ISGID)
				u.u_gid = ip->i_gid;
		}
	}
	u.u_tsize = ts;
	u.u_dsize = ds;
	u.u_ssize = ss;
	u.u_sep = sep;
	estabur(ts, ds, ss, sep, 2);
bad:
	return(overlay);
}

/* KERNEL 47E2 */
setregs()
{
	register int *rp;
	register char *cp;
	register i;

	for (rp = &u.u_signal[0]; rp < &u.u_signal[NSIG]; rp++)
		if ((*rp & 1) == 0)
			*rp = 0;
	for (cp = &regloc[0]; cp < &regloc[8]; )
		u.u_ar0[*cp++] = 0;
	u.u_r.r_val1 = 0;
	u.u_r.r_val2 = 0;
	if (u.u_ex[0] & 0x100)
		u.u_ar0[12] = u.u_ex[8] & ~1;
	else
		u.u_ar0[12] = u.u_ex[5] & ~1;
	for (rp = (int *)u.u_fparea; rp < (int *)&u.u_fparea[0x90]; )
		*rp++ = 0;
	for (i = 0; i < NOFILE; i++) {
		if (u.u_pofile[i] & 1) {
			closef(u.u_ofile[i]);
			u.u_ofile[i] = NULL;
			u.u_pofile[i] &= ~1;
		}
	}
	bcopy((caddr_t)u.u_exname, (caddr_t)u.u_dbuf, DIRSIZ);
}

/* KERNEL 48A6 */
rexit()
{
	register *uap;

	uap = u.u_ap;
	exit((uap[0] & 0377) << 8);
}

/* KERNEL 48C4 */
exit(rv)
{
	register i;
	register struct proc *p, *q;
	register struct file *f;

	p = u.u_procp;
	p->p_flag &= ~(STRC|SULOCK);
	p->p_clktim = 0;
	for (i = 0; i < NSIG; i++)
		u.u_signal[i] = 1;
	for (i = 0; i < NOFILE; i++) {
		f = u.u_ofile[i];
		u.u_ofile[i] = NULL;
		closef(f);
	}
	plock(u.u_cdir);
	iput(u.u_cdir);
	if (u.u_rdir) {
		plock(u.u_rdir);
		iput(u.u_rdir);
	}
	xfree();
	mfree(coremap, p->p_size, p->p_addr);
	p->p_stat = SZOMB;
	((struct xproc *)p)->xp_xstat = rv;
	((struct xproc *)p)->xp_utime = u.u_cutime + u.u_utime;
	((struct xproc *)p)->xp_stime = u.u_cstime + u.u_stime;
	i = 0;
	for (q = &proc[0]; q <= proctop; q++) {
		if (q->p_ppid == p->p_pid) {
			q->p_ppid = 1;
			if (q->p_stat == SZOMB)
				i++;
			else if (q->p_stat == SSTOP)
				setrun(q);
		} else if (q->p_pid == p->p_ppid)
			wakeup((caddr_t)q);
	}
	if (i)
		wakeup((caddr_t)&proc[1]);
	swtch();
}

/* KERNEL 49F6 */
wait()
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
				u.u_r.r_val2 = ((struct xproc *)p)->xp_xstat;
				u.u_cutime += ((struct xproc *)p)->xp_utime;
				u.u_cstime += ((struct xproc *)p)->xp_stime;
				p->p_pid = 0;
				p->p_ppid = 0;
				p->p_pgrp = 0;
				p->p_sig = 0;
				p->p_flag = 0;
				p->p_wchan = 0;
				p->p_stat = NULL;
				if (p == proctop)
					for (;;) {
						proctop--;
						if (proctop->p_stat != NULL)
							break;
					}
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

/* KERNEL 4AD4 */
fork()
{
	register struct proc *p1, *p2;
	register a, size;

	size = ctod2(u.u_dsize) + 2;
	size += ctod2(0x100 - ((unsigned)u.u_ar0[8] >> 8));
	a = malloc(swapmap, size);
	if (a == 0) {
		u.u_error = ENOMEM;
		goto out;
	}
	mfree(swapmap, size, a);
	a = 0;
	p2 = NULL;
	for (p1 = &proc[0]; p1 < &proc[NPROC]; p1++) {
		if (p1->p_stat == NULL && p2 == NULL)
			p2 = p1;
		else {
			if (p1->p_uid == u.u_uid && p1->p_stat != NULL)
				a++;
		}
	}
	if (p2 == NULL || (u.u_uid != 0 && (p2 == &proc[NPROC-1] || a > MAXUPRC))) {
		u.u_error = EAGAIN;
		goto out;
	}
	if (p2 > proctop)
		proctop = p2;
	p1 = u.u_procp;
	if (newproc()) {
		u.u_r.r_val1 = p1->p_pid;
		u.u_start = time;
		u.u_cstime = 0;
		u.u_stime = 0;
		u.u_cutime = 0;
		u.u_utime = 0;
		return;
	}
	u.u_r.r_val1 = p2->p_pid;
out:
	u.u_ar0[12] += 2;
}

/* KERNEL 4BD0 */
sbreak()
{
	register a, n, d;
	int x;

	n = btoc(*u.u_ap);
	d = n - u.u_dsize;
	if (d > 0) {
		a = u.u_procp->p_addr + n;
		while (d--)
			clearseg(--a);
	}
	u.u_dsize = n;
}

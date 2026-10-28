/* slp.c -- Schlafen/Aufwecken/Scheduler (Kernel mutos_1216, 18D4..2100).  System-III-nahe Fassung wie WEGA slp.c, mit
 * Z8000-Eigenheiten (resume/savu, copyseg).  Daten: slpque = CEC6 (64 Zeiger, BSS); Texte 8A8E "zero wchan", 8A99
 * "proc on q\n", 8AA4 "Running a dead proc", 8AB8 "no procs"; runq 8DC8, runrun 8DC4, runin 8DD6, runout 8DD8,
 * curpri 8DDA, mpid 8DD4, proctop 8A8C, coremap A09C. */
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

#define SQSIZE	0100
struct proc *slpque[SQSIZE];
#define HASH(x)	(( (int) x >> 5) & (SQSIZE-1))

int mpid;
int sflag1;		/* 8DE2: Anforderung des Update im Scheduler (Name geraten) */
int sflag0;		/* 8DE0 */

/* KERNEL 18D4 */
sleep(chan, pri)
caddr_t chan;
{
	register struct proc *rp;
	register s, h;

	rp = u.u_procp;
	s = spl6();
	if (chan == 0)
		panic("zero wchan");
	rp->p_stat = SSLEEP;
	rp->p_wchan = chan;
	rp->p_pri = pri;
	h = HASH(chan);
	rp->p_link = slpque[h];
	slpque[h] = rp;
	if (pri > PZERO) {
		if (issig()) {
			rp->p_wchan = 0;
			rp->p_stat = SRUN;
			slpque[h] = rp->p_link;
			spl0();
			goto psig;
		}
		spl0();
		if (runin != 0) {
			runin = 0;
			wakeup((caddr_t)&runin);
		}
		swtch();
		if (issig())
			goto psig;
	} else {
		spl0();
		swtch();
	}
	splx(s);
	return;

psig:
	resume(u.u_procp->p_addr, u.u_qsav);
}

/* KERNEL 19A8 */
wakeup(chan)
register caddr_t chan;
{
	register struct proc *p, *q;
	register i;
	register s;
	struct proc *sp;

	s = spl6();
	i = HASH(chan);
	p = slpque[i];
	q = NULL;
	while (p != NULL) {
		if (p->p_wchan == chan && p->p_stat != SZOMB) {
			if (q == NULL)
				sp = slpque[i] = p->p_link;
			else
				sp = q->p_link = p->p_link;
			p->p_wchan = 0;
			setrun(p);
			p = sp;
			continue;
		}
		q = p;
		p = p->p_link;
	}
	splx(s);
}

/* KERNEL 1A30 */
setrq(p)
struct proc *p;
{
	register struct proc *q;
	register s;

	s = spl6();
	for (q = runq; q != NULL; q = q->p_link)
		if (q == p) {
			printf("proc on q\n");
			goto out;
		}
	p->p_link = runq;
	runq = p;
out:
	splx(s);
}

/* KERNEL 1A7C */
setrun(p)
register struct proc *p;
{
	register caddr_t w;

	if (p->p_stat == 0 || p->p_stat == SZOMB)
		panic("Running a dead proc");
	if (w = p->p_wchan) {
		wakeup((caddr_t)w);
		return;
	}
	p->p_stat = SRUN;
	setrq(p);
	if (p->p_pri < curpri)
		runrun++;
	if (runout != 0 && (p->p_flag & SLOAD) == 0) {
		runout = 0;
		wakeup((caddr_t)&runout);
	}
}

/* KERNEL 1AE8 */
setpri(pp)
register struct proc *pp;
{
	register p;

	p = (pp->p_cpu & 0377) / 16;
	p += PUSER + pp->p_nice - NZERO;
	if (p > 127)
		p = 127;
	if (curpri > p)
		runrun++;
	pp->p_pri = p;
}

/* KERNEL 1B34 */
sched()
{
	register struct proc *rp, *p;
	register outage, inage;
	register maxsize;

loop:
	if (sflag1) {
		sflag1 = 0;
		spl0();
		update();
		sflag0 = 0;
		sflag1 = 0;
	}
	spl6();
	outage = -20000;
	for (rp = &proc[0]; rp <= proctop; rp++)
		if (rp->p_stat == SRUN && (rp->p_flag & SLOAD) == 0 &&
		    rp->p_time - (rp->p_nice - NZERO) * 8 > outage) {
			p = rp;
			outage = rp->p_time - (rp->p_nice - NZERO) * 8;
		}
	if (outage == -20000) {
		runout++;
		sleep((caddr_t)&runout, PSWP);
		goto loop;
	}
	spl0();
	if (swapin(p))
		goto loop;
	spl6();
	p = NULL;
	maxsize = -1;
	inage = -1;
	for (rp = &proc[0]; rp <= proctop; rp++) {
		if (rp->p_stat == SZOMB ||
		    (rp->p_flag & (SSYS | SLOCK | SULOCK | SLOAD)) != SLOAD)
			continue;
		if (rp->p_textp && rp->p_textp->x_flag & XLOCK)
			continue;
		if (rp->p_stat == SSLEEP && rp->p_pri >= PZERO || rp->p_stat == SSTOP) {
			if (maxsize < rp->p_size) {
				p = rp;
				maxsize = rp->p_size;
			}
		} else if (maxsize < 0 && (rp->p_stat == SRUN || rp->p_stat == SSLEEP)) {
			if (rp->p_time + rp->p_nice - NZERO > inage) {
				p = rp;
				inage = rp->p_time + rp->p_nice - NZERO;
			}
		}
	}
	spl0();
	if (maxsize >= 0 || (outage >= 3 && inage >= 2)) {
		p->p_flag &= ~SLOAD;
		xswap(p, 1, 0);
		goto loop;
	}
	spl6();
	runin++;
	sleep((caddr_t)&runin, PSWP);
	goto loop;
}

/* KERNEL 1CCA */
swapin(p)
register struct proc *p;
{
	register struct text *xp;
	register a;
	register x;
	register t;

	if ((a = malloc(coremap, p->p_size)) == NULL)
		return(0);
	if (xp = p->p_textp) {
		xlock(xp);
		if (xp->x_ccount == 0) {
			if ((x = malloc(coremap, xp->x_size)) == NULL) {
				xunlock(xp);
				mfree(coremap, p->p_size, a);
				return(0);
			}
			xp->x_caddr = x;
			if ((xp->x_flag & XLOAD) == 0)
				swap(xp->x_daddr, x, xp->x_size, B_READ);
		}
		xp->x_ccount++;
		xunlock(xp);
	}
	t = a;
	t <<= 2;
	t += 0xEC00;
	swap(p->p_addr, a, 0x104, B_READ, t);
	p->p_addr = a;
	p->p_flag |= SLOAD;
	p->p_time = 0;
	return(1);
}

/* KERNEL 1D8E */
qswtch()
{
	setrq(u.u_procp);
	swtch();
}

/* Absolute Zellen im u-Rest (FD5A/FD5C: Gleitpunkt-/Sicherungszustand, Namen geraten) */
#define U_FPSAVED	(*(int *)0xFD5A)
#define U_FPAREA	((char *)0xFD5C)

/* KERNEL 1DA4 */
swtch()
{
	register n;
	register struct proc *p, *q, *pp, *pq;

	if (u.u_procp != &proc[0]) {
		if (savu(u.u_rsav)) {
			sureg();
			return;
		}
		if (U_FPSAVED == 0) {
			savfp(U_FPAREA);
			U_FPSAVED = 1;
		}
		resume(proc[0].p_addr, u.u_qsav);
	}
	if (savu(u.u_qsav) == 0 && savu(u.u_rsav))
		return;
loop:
	spl6();
	runrun = 0;
	pp = NULL;
	q = NULL;
	n = 128;
	for (p = runq; p != NULL; p = p->p_link) {
		if (p->p_stat == SRUN && (p->p_flag & SLOAD)) {
			if (p->p_pri < n) {
				pp = p;
				pq = q;
				n = p->p_pri;
			}
		}
		q = p;
	}
	p = pp;
	if (p == NULL) {
		idle();
		goto loop;
	}
	q = pq;
	if (q == NULL)
		runq = p->p_link;
	else
		q->p_link = p->p_link;
	curpri = n;
	spl0();
	n = p->p_flag & SSWAP;
	p->p_flag &= ~SSWAP;
	resume(p->p_addr, n ? u.u_ssav : u.u_rsav);
}

/* KERNEL 1EA2 */
newproc()
{
	struct proc *p, *up;
	int a1;
	register struct proc *rpp, *rip;
	register n, a;

	p = NULL;
retry:
	mpid++;
	if (mpid >= 30000) {
		mpid = 0;
		goto retry;
	}
	for (rpp = &proc[0]; rpp <= proctop; rpp++) {
		if (rpp->p_stat == 0 && p == NULL)
			p = rpp;
		if (rpp->p_pid == mpid || rpp->p_uid == mpid)
			goto retry;
	}
	rpp = p;
	if (rpp == NULL)
		panic("no procs");
	rip = u.u_procp;
	up = rip;
	rpp->p_stat = SRUN;
	rpp->p_clktim = 0;
	rpp->p_flag = SLOAD;
	rpp->p_uid = rip->p_uid;
	rpp->p_pgrp = rip->p_pgrp;
	rpp->p_nice = rip->p_nice;
	rpp->p_textp = rip->p_textp;
	rpp->p_pid = mpid;
	rpp->p_ppid = rip->p_pid;
	rpp->p_time = 0;
	rpp->p_cpu = 0;
	for (n = 0; n < NOFILE; n++)
		if (u.u_ofile[n] != NULL)
			u.u_ofile[n]->f_count++;
	if (up->p_textp) {
		up->p_textp->x_count++;
		up->p_textp->x_ccount++;
	}
	u.u_cdir->i_count++;
	if (u.u_rdir)
		u.u_rdir->i_count++;
	rpp = p;
	u.u_procp = rpp;
	rip = up;
	n = 256;
	a1 = rip->p_addr;
	rpp->p_size = n;
	if (savu(u.u_ssav)) {
		sureg();
		return(1);
	}
	if (U_FPSAVED == 0) {
		savfp(U_FPAREA);
		U_FPSAVED = 1;
	}
	a = malloc(coremap, n);
	if (a == NULL) {
		rip->p_stat = SIDL;
		rpp->p_addr = a1;
		n = 4;
		a1 = (unsigned)0xFCFF >> 8;
		a = rpp->p_addr >> 8 << 10;
		a += 0xECFF;
		a = (unsigned)a >> 8;
		while (n--)
			copyseg(a1++, a++);
		xswap(rpp, 0, 0);
		rip->p_stat = SRUN;
	} else {
		rpp->p_addr = a;
		while (n--)
			copyseg(a1++, a++);
		n = 4;
		a1 = (unsigned)0xFCFF >> 8;
		a = rpp->p_addr >> 8 << 10;
		a += 0xECFF;
		a = (unsigned)a >> 8;
		while (n--)
			copyseg(a1++, a++);
	}
	u.u_procp = rip;
	setrq(rpp);
	rpp->p_flag |= SSWAP;
	return(0);
}

/* KERNEL 20F6 */
expand()
{
	register i;
}

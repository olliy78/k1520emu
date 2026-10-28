/* clock.c -- Taktgeber (Kernel mutos_1216, 2842..2A4C): clock.  timeout (2A4C) steht in subr.c (gehoert im Kernel hinter clock).
 * System-III-Form (WEGA): Zeitgeberliste callout[] (CE58, 9 Eintraege zu 6 Byte), Nutzer-/Systemzeit, lbolt/time, Prozess-
 * Rechenzeit p_cpu = p_cpu*8/10 + p_nice - NZERO (SCHMAG 8/10), p_clktim (SIGALRM = 14).
 * Der Interrupteintritt gibt die gesicherten Register und den Rahmen als Argumente weiter: 4 dev, 6 Takte (obere Haelfte des Wortes),
 * 1A Prioritaet des Unterbrochenen (und 7 = nicht verschachtelt), 1C FCW (4000 = Systemmodus), 20 PC.
 * Daten: cf_x88a (888A, conf.c), sflag0/sflag1 (8DE0/8DE2, slp.c), runin 8DD6, runout 8DD8, runrun 8DC4, lbolt 8DCA, time 8DCC. */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "proc.h"
#include "callo.h"

extern int cf_x88a, sflag0, sflag1;

/* KERNEL 2842 */
clock(dev, tk, f2, f3, f4, f5, f6, f7, f8, f9, f10, ps0, ps, pcseg, pc)
unsigned tk;
{
	register struct callo *p1, *p2;
	register struct proc *pp;
	register n;
	register t;
	int a;

	t = tk >> 8;
	if (callout[0].c_func) {
		n = t;
		p2 = &callout[0];
		do {
			while (p2->c_time <= 0 && p2->c_func)
				p2++;
			p2->c_time--;
		} while (--n > 0);
		if ((ps0 & 07) == 0) {
			spl5();
			if (callout[0].c_time <= 0) {
				p1 = &callout[0];
				while (p1->c_func != 0 && p1->c_time <= 0) {
					(*p1->c_func)(p1->c_arg);
					p1++;
				}
				p2 = &callout[0];
				while (p2->c_func = p1->c_func) {
					p2->c_time = p1->c_time;
					p2->c_arg = p1->c_arg;
					p1++;
					p2++;
				}
			}
		}
	}
	if ((ps & 0x4000) == 0) {
		u.u_utime += t;
		if (u.u_prof.pr_scale)
			addupc(pc, &u.u_prof, t);
	} else
		u.u_stime += t;
	pp = u.u_procp;
	if (++pp->p_cpu == 0)
		pp->p_cpu--;
	lbolt += t;
	if (lbolt >= HZ) {
		if (ps0 & 07)
			return;
		lbolt -= HZ;
		++time;
		spl1();
		if (++sflag0 >= 30 && cf_x88a) {
			++sflag1;
			if (runout) {
				runout = 0;
				wakeup((caddr_t)&runout);
			}
		}
		++runrun;
		wakeup((caddr_t)&lbolt);
		for (pp = &proc[0]; pp <= proctop; pp++)
			if (pp->p_stat && pp->p_stat < SZOMB) {
				if (pp->p_time != 127)
					pp->p_time++;
				if (pp->p_clktim)
					if (--pp->p_clktim == 0)
						psignal(pp, SIGALRM);
				a = (pp->p_cpu & 0377) * 8 / 10 + pp->p_nice - NZERO;
				if (a < 0)
					a = 0;
				if (a > 255)
					a = 255;
				pp->p_cpu = a;
				if (pp->p_pri >= PUSER)
					setpri(pp);
			}
		if (runin) {
			runin = 0;
			wakeup((caddr_t)&runin);
		}
	}
}

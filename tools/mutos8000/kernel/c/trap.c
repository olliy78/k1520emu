/* trap.c -- Trap- und Systemaufruf-Eintritt in C (Kernel mutos_1216, 087A..0B3A): trap, nosys, nullsys.
 * Der Eintritt selbst (trapent 030E) ist Assembler.  Rahmen am Aufruf von trap (r14-relativ): dev 4, ident 6, dann der gerettete
 * Registerrahmen ab 8 (u.u_ar0 = &r0): ar0[8] = Nutzer-SP (18h), FCW 1Ch, Segment-PC 1Eh, PC 20h.  Bit 14 des FCW = Systembetrieb
 * (fehlt es, wird USER (16) zu dev addiert), Bit 7 = C-Flag (Fehleranzeige wie EBIT in V7), Bit 11 geloescht beim Einzelschritt.
 * Fallunterscheidung ueber dev (10..31): 10/26 Paritaet, 11/27 Paritaet II, 15/31 unerwartete Unterbrechung, 16 Busfehler-Signal,
 * 17 Befehlsemulation (F009), 18 privilegierter Befehl (ident = Opcode 7C04..06), 19 Einzelschritt, 22 SYSTEMAUFRUF, 25 Segmentfehler
 * (Stackwachstum), 28 nichts.
 * Daten: nested = 88E0, argp = 88E2 (enthaelt FC9E = u.u_arg), Texte 88E4 "parity during ", 8920.., 8969, 8990, 8997, 89C3,
 * sysent = 8B58 (4 Byte je Eintrag), Sprungtabelle der Faelle 88F4.  Gleitpunkt: savfp 03F8 / restfp 03FE (Assembler). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "proc.h"
#include "reg.h"

#define USER	16
#define EBIT	0200

int nested;
int *argp = u.u_arg;

struct blk4 {
	int	w[4];
};

/* KERNEL 087A */
trap(dev, ident, r0, r1, r2, r3, r4, r5, r6, r7, sp, xx, ps, pcseg, pc)
register unsigned ident;
{
	register i, a;
	register struct sysent *callp;
	register t;
	struct blk4 *rp;
	int tt;
	time_t syst;

	syst = u.u_stime;
	u.u_fpsaved = 0;
	if ((ps & 0x4000) == 0)
		dev |= USER;
	u.u_ar0 = &r0;
	switch (dev & 0xFF) {
	default:
		if (nested)
			for (;;)
				;
		nested++;
		printf("seg = %x\n", 0);
		printf("aps = %x\n", &ps);
		printf("pc = %x %x   ps = %x\n", pcseg, pc, ps);
		printf("trap type %x\n", dev);
		printf("ident = %x\n", ident);
		panic("trap");

	case 0+USER:
		i = SIGBUS;
		break;

	case 1+USER:
		pc += 2;
		if (fuword(pc-4) == 0xF009 && u.u_signal[SIGILL] == 0)
			goto out;
		i = SIGILL;
		break;

	case 2+USER:
		if (ident == 0x7C06)
			i = SIGTRAP;
		else if (ident == 0x7C04)
			i = SIGEMT;
		else if (ident == 0x7C05)
			i = SIGIOT;
		else
			i = SIGILL;
		break;

	case 3+USER:
		i = SIGTRAP;
		ps &= ~0x0800;
		break;

	case 6+USER:
		u.u_error = 0;
		ps &= ~EBIT;
		a = pc;
		callp = &sysent[ident & 077];
		if (callp == sysent) {
			sigret();
			goto out;
		}
		rp = (struct blk4 *)u.u_ar0;
		*(struct blk4 *)argp = *rp;
		u.u_dirp = (caddr_t)u.u_arg[0];
		u.u_r.r_val1 = u.u_ar0[4];
		u.u_r.r_val2 = u.u_ar0[5];
		u.u_ap = u.u_arg;
		if (save(u.u_qsav)) {
			if (u.u_error == 0)
				u.u_error = EINTR;
		} else
			(*callp->sy_call)();
		if (u.u_error) {
			ps |= EBIT;
			u.u_ar0[4] = u.u_error;
			u.u_ar0[5] = u.u_deverr;
		} else {
			u.u_ar0[4] = u.u_r.r_val1;
			u.u_ar0[5] = u.u_r.r_val2;
		}
		goto out;

	case 9+USER:
		a = sp;
		if (backup(u.u_ar0) == 0)
			if (grow(a))
				goto out;
		i = SIGSEG;
		break;

	case 10:
	case 10+USER:
		printf("%s running - segment = %x offset = %x\n", "parity during ", pcseg, pc);
		if (dev & USER)
			goto sigbus;
		panic("parity");

	case 11:
	case 11+USER:
		i = ident >> 14;
		t = (ident << 2) & 0xF600;
		printf("%s waiting - segment = %x offset = %x + ??\n", "parity during ", i, t);
		goto out;

	case 15:
	case 15+USER:
		printf("Random interrupt (ident = %x) ignored\n", ident);
		goto out;

	case 12+USER:
		goto out;
	}
	psignal(u.u_procp, i);
out:
	if (issig())
		psig();
	curpri = setpri(u.u_procp);
	if (runrun)
		qswtch();
	if (u.u_prof.pr_scale)
		addupc(pc, &u.u_prof, (int)(u.u_stime - syst));
	if (u.u_fpsaved)
		restfp(u.u_fparea);
	return;
sigbus:
	i = SIGBUS;
	psignal(u.u_procp, i);
	goto out;
}

/* KERNEL 0B2A */
nosys()
{
	register i;

	u.u_error = EINVAL;
}

/* KERNEL 0B38 */
nullsys()
{
}

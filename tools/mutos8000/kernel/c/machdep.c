/* machdep.c -- Maschinenabhaengiges in C (Kernel mutos_1216, 34F6..36A0): startup, ein RET-Stumpf, sendsig, sigret.
 * Handassembler (mch.s, hier NICHT nachgebaut): csv/cret, savu/resume, spl0..spl7/splx, fuword/fubyte/suword/subyte,
 * copyin/copyout/copyseg/clearseg, idle, Trap-Eintritt 030E, start 027E.
 * Daten: Segmentwahl der Nutzerzugriffe usegm = 8DC0 (suword/fuword lesen sie), maxmem = 8DDC, Text 8B4E "mem = %D\n".
 * Benennung der Nutzerraumzugriffe: 04CC = fubyte (Byte lesen), 04DA = fuword (Wort lesen), 0508 = subyte, 0514 = suword -- die
 * Karte (kernel_namen.md K6b) hat sie vertauscht; in den Quellen gilt diese Zuordnung.
 * K6b nannte 3630 "backup"; es ist die Rueckkehr aus dem Signal (sigret, Gegenstueck zu sendsig, Unterfunktion 0 der sysent). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "proc.h"
#include "map.h"

extern int usegm;
extern int maxmem;

/* KERNEL 34F6 */
startup()
{
	register unsigned c;
	register t;

	mchinit();
	c = 0xFCFF;
	c >>= 8;
	c += 4;
	t = 0x55;
	for (; c < 0x400; c++) {
		usegm = c & 0x7F00;
		subyte(c << 8, t);
		if (fubyte(c << 8) != t)
			break;
		clearseg(c);
		maxmem++;
		mfree(coremap, 1, c);
	}
	printf("mem = %D\n", (long)maxmem << 8);
	usegm = 0;
	if (maxmem > 256)
		maxmem = 256;
	mfree(swapmap, nswap, 1);
	swplo--;
}

/* KERNEL 35AC */
kstub()
{
}

/* KERNEL 35AE */
sendsig(p, n)
{
	register sp;

	sp = u.u_ar0[8];
	sp -= 8;
	grow(sp);
	suword(sp+6, u.u_ar0[12]);
	suword(sp+4, u.u_ar0[11]);
	suword(sp+2, u.u_ar0[10]);
	suword(sp, u.u_ar0[-1]);
	u.u_ar0[8] = sp;
	u.u_ar0[12] = p;
}

/* KERNEL 3630 */
sigret()
{
	register sp;

	sp = u.u_ar0[8];
	u.u_ar0[12] = fuword(sp+6);
	u.u_ar0[11] = fuword(sp+4);
	u.u_ar0[10] = fuword(sp+2);
	u.u_ar0[8] = sp + 8;
}

/* prf.c -- Konsolenausgabe und panic (Kernel mutos_1216, Adressen 1724..18D3; putchar liegt bei 6384 im Treiberteil).
 * Marke KERNEL xxxx = Kerneladresse der Funktion; vergleich.py prueft sie bytegleich gegen das Abbild.
 * Daten dieses Moduls (alle in der Daten-/BSS-Reihenfolge des Kernels, Adressen im Abbild):
 *   8A4A "0123456789ABCDEF"   (printn)        8A5B "panic: %s\n"   (panic)
 *   8A66 "%s on dev %u/%u\n"  (prdev)         8A77 "err"           (deverror)
 *   8A7B "bn=%D er=%o,%o\n"   (deverror)
 *   panicstr = CEC4 (BSS, Wort).  printn legt die Ziffernfolge in der Textpool-Reihenfolge der Funktionen ab. */
#include "param.h"
#include "systm.h"
#include "buf.h"

char *panicstr;

/* KERNEL 1724 */
printf(fmt, x1)
register char *fmt;
unsigned x1;
{
	register c;
	register unsigned *adx;
	char *s;

	adx = &x1;
loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0')
			return;
		putchar(c);
	}
	c = *fmt++;
	if (c == 'd' || c == 'u' || c == 'o' || c == 'x')
		printn((long)*adx, c == 'o' ? 8 : (c == 'x' ? 16 : 10));
	else if (c == 's') {
		s = (char *)*adx;
		while (c = *s++)
			putchar(c);
	} else if (c == 'D') {
		printn(*(long *)adx, 10);
		adx += (sizeof(long) / sizeof(int)) - 1;
	}
	adx++;
	goto loop;
}

/* KERNEL 17E0 */
printn(n, b)
long n;
{
	long a;

	if (n < 0) {
		putchar('-');
		n = -n;
	}
	if (a = n / b)
		printn(a, b);
	putchar("0123456789ABCDEF"[(int)(n % b)]);
}

/* KERNEL 1856 */
panic(s)
char *s;
{
	panicstr = s;
	update();
	printf("panic: %s\n", s);
	for (;;)
		idle();
}

/* KERNEL 187A */
prdev(str, dev)
char *str;
dev_t dev;
{
	printf("%s on dev %u/%u\n", str, major(dev), minor(dev));
}

/* KERNEL 18A4 */
deverror(bp, o1, o2)
register struct buf *bp;
{
	prdev("err", bp->b_dev);
	printf("bn=%D er=%o,%o\n", bp->b_blkno, o1, o2);
}

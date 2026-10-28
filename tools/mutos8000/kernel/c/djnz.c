/* DJNZ-Frage (AP K7a): welche Schleifenform erzeugt `DEC Rn,#1 ; JR NZ` (Kernel bcopy/clrbuf/iomove-Teil) und welche DJNZ?
 * Nur zum Vergleich im .s gedacht (cc -O -S); Kernel: 243A bcopy, 5C86 clrbuf, 2780 iomove (DEC/JR NZ bei 2822, DJNZ bei 283C). */
struct buf {
	int b_flags;
	int *b_addr;
	char b_error;
};
int ok;
int passc(), cpass();

/* A: do..while(--n), Zeiger register */
fa(from, to, n)
register char *from, *to;
register n;
{
	do
		*to++ = *from++;
	while (--n);
}
/* B: Zaehler als unsigned */
fb(from, to, n)
register char *from, *to;
register unsigned n;
{
	do
		*to++ = *from++;
	while (--n);
}
/* C: while(n--) */
fc(from, to, n)
register char *from, *to;
register n;
{
	while (n--)
		*to++ = *from++;
}
/* D: for(;n;n--) */
fd(from, to, n)
register char *from, *to;
register n;
{
	for (; n; n--)
		*to++ = *from++;
}
/* E: Zaehler nicht register (auto) */
fe(from, to, n)
register char *from, *to;
int n;
{
	do
		*to++ = *from++;
	while (--n);
}
/* F: Rumpf mit Aufruf und Abbruch, do..while(--n) (iomove-Schreibast, Kernel DJNZ) */
ff(cp, n)
register char *cp;
register n;
{
	do {
		if (passc(*cp++) < 0)
			return;
	} while (--n);
}
/* G: wie F mit while(n--) (iomove-Leseast, Kernel DEC/JR NZ) */
fg(cp, n)
register char *cp;
register n;
{
	char c;

	while (n--) {
		if ((c = cpass()) < 0)
			return;
		*cp++ = c;
	}
}
/* H: cpass-Ast mit int-Zwischenwert, do..while(--n) */
fh(cp, n)
register char *cp;
register n;
{
	register t;

	do {
		if ((t = cpass()) < 0)
			return;
		*cp++ = t;
	} while (--n);
}
/* I: Wortschleife clrbuf, do..while(--c) */
fi(bp)
register struct buf *bp;
{
	register *wp;
	register c;

	wp = bp->b_addr;
	c = 256;
	do
		*wp++ = 0;
	while (--c);
	bp->b_error = 0;
}
/* J: wie I, aber while(c--) */
fj(bp)
register struct buf *bp;
{
	register *wp;
	register c;

	wp = bp->b_addr;
	c = 256;
	while (c--)
		*wp++ = 0;
	bp->b_error = 0;
}
/* K: for(c=256; c>0; c--) */
fk(bp)
register struct buf *bp;
{
	register *wp;
	register c;

	wp = bp->b_addr;
	for (c = 256; c > 0; c--)
		*wp++ = 0;
	bp->b_error = 0;
}
/* L: do..while(--c > 0) */
fl(bp)
register struct buf *bp;
{
	register *wp;
	register c;

	wp = bp->b_addr;
	c = 256;
	do
		*wp++ = 0;
	while (--c > 0);
	bp->b_error = 0;
}
/* M: do..while(--c != 0) mit Zaehler char */
fm(bp)
register struct buf *bp;
{
	register *wp;
	register char c;

	wp = bp->b_addr;
	c = 100;
	do
		*wp++ = 0;
	while (--c != 0);
}
/* N: Zaehler wird im Rumpf benutzt */
fn(a, n)
register int *a;
register n;
{
	do {
		*a = n;
		a++;
	} while (--n);
}
/* O: zwei Schleifen hintereinander, Zaehler wiederverwendet */
fo(a, n)
register char *a;
register n;
{
	do
		*a++ = 0;
	while (--n);
	do
		*a++ = 1;
	while (--n);
}
/* P: --n als Anweisung am Schleifenende, Test getrennt */
fp(a, n)
register char *a;
register n;
{
	do {
		*a++ = 0;
		n--;
	} while (n);
}
/* Q: n -= 1 ... */
fq(a, n)
register char *a;
register n;
{
	do {
		*a++ = 0;
		n -= 1;
	} while (n != 0);
}
/* R: n global */
int gn;
fr(a)
register char *a;
{
	do
		*a++ = 0;
	while (--gn);
}

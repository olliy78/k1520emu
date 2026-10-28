/* DJNZ-Versuche Teil 3: Leseast von iomove (Kernel 2780) in sechs Formen, dazu Gegenproben. */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "buf.h"

/* KERNEL 2780 */
iR2(cp, n, flag)
register caddr_t cp;
register n;
{
	register t;

	if (n == 0)
		return;
	if (flag == B_WRITE) {
		do {
			if ((t = cpass()) < 0)
				return;
			*cp++ = t;
		} while (--n);
	} else {
		do {
			if (passc(*cp++) < 0)
				return;
		} while (--n);
	}
}
/* KERNEL 2780 */
iR3(cp, n, flag)
register caddr_t cp;
register n;
{
	register t;

	if (n == 0)
		return;
	if (flag != B_WRITE) {
		do {
			if (passc(*cp++) < 0)
				return;
		} while (--n);
	} else {
		do {
			if ((t = cpass()) < 0)
				return;
			*cp++ = t;
		} while (--n);
	}
}
/* KERNEL 2780 */
iR4(cp, n, flag)
register caddr_t cp;
register n;
{
	register t;

	if (n == 0)
		return;
	if (flag == B_WRITE) {
		do {
			if ((t = cpass()) < 0)
				return;
			*cp++ = t;
		} while (--n);
		return;
	}
	do {
		if (passc(*cp++) < 0)
			return;
	} while (--n);
}
/* KERNEL 2780 */
iR5(cp, n, flag)
register caddr_t cp;
register n;
{
	register t;

	if (n == 0)
		return;
	if (flag == B_WRITE) {
		do {
			if ((t = cpass()) < 0)
				return;
			*cp++ = t;
		} while (--n);
	} else {
		for (;;) {
			if (passc(*cp++) < 0)
				return;
			if (--n == 0)
				break;
		}
	}
}
/* Gegenprobe T1: Schleife mit FP-Bezug davor, nur eine Schleife */
t1(a, n, flag)
register char *a;
register n;
{
	if (flag)
		return;
	do {
		if (passc(*a++) < 0)
			return;
	} while (--n);
}
/* T2: wie T1, aber Schleife in else-Ast */
t2(a, n, flag)
register char *a;
register n;
{
	if (flag)
		n = 1;
	else {
		do {
			if (passc(*a++) < 0)
				return;
		} while (--n);
	}
}
/* T3: zwei Schleifen nacheinander, Aufruf im Rumpf, erste mit store */
t3(a, n)
register char *a;
register n;
{
	register t;

	do {
		if ((t = cpass()) < 0)
			return;
		*a++ = t;
	} while (--n);
}
/* T4: wie T3, aber t nicht register */
t4(a, n)
register char *a;
register n;
{
	int t;

	do {
		if ((t = cpass()) < 0)
			return;
		*a++ = t;
	} while (--n);
}
/* T5: Rumpf mit Aufruf ohne Abbruch */
t5(a, n)
register char *a;
register n;
{
	do {
		*a++ = cpass();
	} while (--n);
}
/* T6: Schleife enthaelt nur Speicherzugriff mit Byte */
t6(a, n)
register char *a;
register n;
{
	do {
		*a++ = 1;
	} while (--n);
}

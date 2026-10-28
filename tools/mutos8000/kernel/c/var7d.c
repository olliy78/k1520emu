/* var7d.c -- Varianten fuer K7d (nur zum Vergleich im Gastlauf; Marken wie im Original, Namen <= 7 Zeichen). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "tty.h"

/* KERNEL 797A */
ttysA(c, tp)
register c;
register struct tty *tp;
{
	register char *p;
	register unsigned i;

	if (c < 0)
		return(0);
	p = &tp->t_intrc;
	for (i = 0; i < 6; i++)
		if (*p++ == c)
			return(1);
	if (tp->t_erase == c || tp->t_kill == c)
		return(1);
	switch (c) {
	case 0x10:
		if (tp->t_eol == 0)
			break;
	case 0x12:
	case 0x17:
		return(1);
	}
	return(0);
}

/* KERNEL 797A */
ttysB(c, tp)
register c;
register struct tty *tp;
{
	register char *p;
	register unsigned i;

	if (c < 0)
		return(0);
	p = &tp->t_intrc;
	for (i = 0; i < 6; i++)
		if (*p++ == c)
			return(1);
	if (tp->t_erase == c || tp->t_kill == c)
		return(1);
	else if (c == 0x12 || c == 0x17)
		return(1);
	else if (c == 0x10 && tp->t_eol)
		return(1);
	return(0);
}

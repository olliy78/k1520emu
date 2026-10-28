/* var7d2.c -- Varianten putchar (DJNZ-Form der Wartschleife), K7d */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "tty.h"

extern struct rkmbx ttyhw[4];
extern char ttyobuf[4][80];
extern char *conptr;
extern char conlog[];
extern char conend[];

/* KERNEL 6384 */
pcA(c)
register c;
{
	register s, t;
	register struct rkmbx *hp;
	register char *bp;

	if (c) {
		*conptr++ = c;
		if (conptr >= conend)
			conptr = conlog;
	}
	hp = &ttyhw[0];
	bp = ttyobuf[0];
	t = 10000;
	while (hp->mb_stat == 0 && --t)
		;
	if (c == 0)
		return;
	s = splx(7);
	hp->mb_cmd = 4;
	hp->mb_unit = 0;
	hp->mb_stat = 0;
	hp->mb_seg = 0;
	hp->mb_addr = bp;
	hp->mb_cnt = 1;
	hp->mb_dir = 0x81;
	*bp = c;
	if (c == '\n') {
		*++bp = '\r';
		hp->mb_cnt = 2;
	}
	mapalloc(hp, 4);
	pcA(0);
	hp->mb_stat = 1;
	splx(s);
}


/* KERNEL 6384 */
pcB(c)
register c;
{
	register s, t;
	register struct rkmbx *hp;
	register char *bp;

	if (c) {
		*conptr++ = c;
		if (conptr >= conend)
			conptr = conlog;
	}
	hp = &ttyhw[0];
	bp = ttyobuf[0];
	t = 10000;
	for (t = 10000; t > 0; t--)
		if (hp->mb_stat)
			break;
	if (c == 0)
		return;
	s = splx(7);
	hp->mb_cmd = 4;
	hp->mb_unit = 0;
	hp->mb_stat = 0;
	hp->mb_seg = 0;
	hp->mb_addr = bp;
	hp->mb_cnt = 1;
	hp->mb_dir = 0x81;
	*bp = c;
	if (c == '\n') {
		*++bp = '\r';
		hp->mb_cnt = 2;
	}
	mapalloc(hp, 4);
	pcB(0);
	hp->mb_stat = 1;
	splx(s);
}


/* KERNEL 6384 */
pcC(c)
register c;
{
	register s, t;
	register struct rkmbx *hp;
	register char *bp;

	if (c) {
		*conptr++ = c;
		if (conptr >= conend)
			conptr = conlog;
	}
	hp = &ttyhw[0];
	bp = ttyobuf[0];
	t = 10000;
	do {
		if (hp->mb_stat)
			break;
	} while (--t != 0);
	if (c == 0)
		return;
	s = splx(7);
	hp->mb_cmd = 4;
	hp->mb_unit = 0;
	hp->mb_stat = 0;
	hp->mb_seg = 0;
	hp->mb_addr = bp;
	hp->mb_cnt = 1;
	hp->mb_dir = 0x81;
	*bp = c;
	if (c == '\n') {
		*++bp = '\r';
		hp->mb_cnt = 2;
	}
	mapalloc(hp, 4);
	pcC(0);
	hp->mb_stat = 1;
	splx(s);
}


/* KERNEL 6384 */
pcD(c)
register c;
{
	register s, t;
	register struct rkmbx *hp;
	register char *bp;

	if (c) {
		*conptr++ = c;
		if (conptr >= conend)
			conptr = conlog;
	}
	hp = &ttyhw[0];
	bp = ttyobuf[0];
	t = 10000;
	while (hp->mb_stat == 0)
		if (!--t)
			break;
	if (c == 0)
		return;
	s = splx(7);
	hp->mb_cmd = 4;
	hp->mb_unit = 0;
	hp->mb_stat = 0;
	hp->mb_seg = 0;
	hp->mb_addr = bp;
	hp->mb_cnt = 1;
	hp->mb_dir = 0x81;
	*bp = c;
	if (c == '\n') {
		*++bp = '\r';
		hp->mb_cnt = 2;
	}
	mapalloc(hp, 4);
	pcD(0);
	hp->mb_stat = 1;
	splx(s);
}


/* KERNEL 6384 */
pcE(c)
register c;
{
	register s, t;
	register struct rkmbx *hp;
	register char *bp;

	if (c) {
		*conptr++ = c;
		if (conptr >= conend)
			conptr = conlog;
	}
	hp = &ttyhw[0];
	bp = ttyobuf[0];
	t = 10000;
	do {
		if (hp->mb_stat != 0)
			break;
		t--;
	} while (t);
	if (c == 0)
		return;
	s = splx(7);
	hp->mb_cmd = 4;
	hp->mb_unit = 0;
	hp->mb_stat = 0;
	hp->mb_seg = 0;
	hp->mb_addr = bp;
	hp->mb_cnt = 1;
	hp->mb_dir = 0x81;
	*bp = c;
	if (c == '\n') {
		*++bp = '\r';
		hp->mb_cnt = 2;
	}
	mapalloc(hp, 4);
	pcE(0);
	hp->mb_stat = 1;
	splx(s);
}


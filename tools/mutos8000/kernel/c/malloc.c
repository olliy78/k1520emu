/* malloc.c -- Speicher- und Swapkarten (Kernel mutos_1216, 2DF0..2F48).  Eintrag {m_size, m_addr} je 4 Byte,
 * hoechstens 15 Eintraege plus Endemarke (die Kernelfassung prueft bp - mp gegen 15).  Daten: coremap = A09C, swapmap = A0DC
 * (je 16 Eintraege), runin = 8DD6 (mfree weckt auf der coremap den Scheduler), Text 8AE4 "map overflow\n". */
#include "param.h"
#include "systm.h"
#include "map.h"

/* KERNEL 2DF0 */
malloc(mp, size)
struct map *mp;
{
	register a;
	register struct map *bp;

	for (bp = mp; bp->m_size; bp++) {
		if (bp - mp >= 15)
			break;
		if (bp->m_size >= size) {
			a = bp->m_addr;
			bp->m_addr += size;
			bp->m_size -= size;
			if (bp->m_size == 0)
				do {
					bp++;
					(bp-1)->m_addr = bp->m_addr;
				} while ((bp-1)->m_size = bp->m_size);
			return(a);
		}
	}
	return(0);
}

/* KERNEL 2E5A */
mfree(mp, size, a)
struct map *mp;
register a;
{
	register struct map *bp;
	register t;

	bp = mp;
	if (bp == coremap && runin) {
		runin = 0;
		wakeup((caddr_t)&runin);
	}
	for (; bp->m_addr <= a && bp->m_size != 0; bp++)
		;
	if (bp > mp && (bp-1)->m_addr + (bp-1)->m_size == a) {
		(bp-1)->m_size += size;
		if (a + size == bp->m_addr) {
			(bp-1)->m_size += bp->m_size;
			while (bp->m_size) {
				bp++;
				(bp-1)->m_addr = bp->m_addr;
				(bp-1)->m_size = bp->m_size;
			}
		}
	} else {
		if (a + size == bp->m_addr && bp->m_size) {
			bp->m_addr -= size;
			bp->m_size += size;
		} else if (size) do {
			if (bp - mp > 15)
				panic("map overflow\n");
			t = bp->m_addr;
			bp->m_addr = a;
			a = t;
			t = bp->m_size;
			bp->m_size = size;
			bp++;
		} while (size = t);
	}
}

/* k7bvar.c -- Quellformen-Versuche K7b (Prefixfunktionen: nur der Anfang der Kernelfunktion, danach return).
 * Verglichen wird der Anfang des Kernelcodes (Laenge = Laenge der Prefixfunktion). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "inode.h"
#include "filsys.h"
#include "buf.h"

/* KERNEL 1372 */
iupA(ip, ta, tm)
register struct inode *ip;
time_t *ta, *tm;
{
	register struct buf *bp;

	if ((ip->i_flag & (IUPD|IACC|ICHG)) == 0 || getfs(ip->i_dev)->s_ronly != 0)
		return;
	bp = bread(ip->i_dev, itod(ip->i_number));
}

/* KERNEL 1372 */
iupB(ip, ta, tm)
register struct inode *ip;
time_t *ta, *tm;
{
	register struct buf *bp;

	if ((ip->i_flag & (IUPD|IACC|ICHG)) == 0 || getfs(ip->i_dev)->s_ronly)
		return;
	bp = bread(ip->i_dev, itod(ip->i_number));
}

/* KERNEL 1372 */
iupC(ip, ta, tm)
register struct inode *ip;
time_t *ta, *tm;
{
	register struct buf *bp;
	register s;

	if ((ip->i_flag & (IUPD|IACC|ICHG)) == 0)
		return;
	s = getfs(ip->i_dev)->s_ronly;
	if (s != 0)
		return;
	bp = bread(ip->i_dev, itod(ip->i_number));
}

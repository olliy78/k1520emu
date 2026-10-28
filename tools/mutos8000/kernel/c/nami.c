/* nami.c -- Pfadnamen (Kernel mutos_1216, 36A0..39B9): namei.  (uchar steht im Kernel hinter namei, 39BA, in pipe.c; ein schar gibt es nicht.)
 * Daten: rootdir 8DC6 (Zeiger), mount[] A07E (6 Byte je Eintrag; die Suche nach ".." beginnt bei 1), u_dbuf FC32, u_dent FC42.
 * Abweichung gegenueber V7: die Namensteile werden vor dem Verzeichnis-/Rechtetest gesammelt (Marke cl2). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "inode.h"
#include "mount.h"
#include "buf.h"

extern struct inode *rootdir;

/* KERNEL 36A0 */
struct inode *
namei(func, flag)
int (*func)();
{
	register struct inode *dp;
	register c;
	register char *cp;
	struct buf *bp;
	int i;
	dev_t d;
	daddr_t eo;

	dp = u.u_cdir;
	c = (*func)();
	if (c == '/') {
		dp = u.u_rdir;
		if (dp == NULL)
			dp = rootdir;
	}
	iget(dp->i_dev, dp->i_number);
	while (c == '/')
		c = (*func)();
	if (c == '\0' && flag != 0)
		u.u_error = ENOENT;

cloop:
	if (u.u_error)
		goto out;
	if (c == '\0')
		return(dp);
	cp = &u.u_dbuf[0];
	while (c != '/' && c != '\0' && u.u_error == 0) {
		if (cp < &u.u_dbuf[DIRSIZ])
			*cp++ = c;
		c = (*func)();
	}
	while (cp < &u.u_dbuf[DIRSIZ])
		*cp++ = '\0';
	while (c == '/')
		c = (*func)();

cl2:
	if ((dp->i_mode & IFMT) != IFDIR)
		u.u_error = ENOTDIR;
	access(dp, IEXEC);
	if (u.u_error)
		goto out;
	u.u_offset = 0;
	u.u_segflg = 1;
	eo = 0;
	bp = NULL;

eloop:
	if (u.u_offset >= dp->i_size) {
		if (bp != NULL)
			brelse(bp);
		if (flag == 1 && c == '\0') {
			if (access(dp, IWRITE))
				goto out;
			u.u_pdir = dp;
			if (eo)
				u.u_offset = eo - sizeof(struct direct);
			else
				dp->i_flag |= IUPD|ICHG;
			return(NULL);
		}
		u.u_error = ENOENT;
		goto out;
	}
	if ((u.u_offset & BMASK) == 0) {
		if (bp != NULL)
			brelse(bp);
		bp = bread(dp->i_dev, bmap(dp, (daddr_t)(u.u_offset >> BSHIFT), B_READ));
		if (bp->b_flags & B_ERROR) {
			brelse(bp);
			goto out;
		}
	}
	bcopy(bp->b_addr + (u.u_offset & BMASK), (caddr_t)&u.u_dent, sizeof(struct direct));
	u.u_offset += sizeof(struct direct);
	if (u.u_dent.d_ino == 0) {
		if (eo == 0)
			eo = u.u_offset;
		goto eloop;
	}
	for (i = 0; i < DIRSIZ; i++)
		if (u.u_dbuf[i] != u.u_dent.d_name[i])
			goto eloop;
	if (bp != NULL)
		brelse(bp);
	if (flag == 2 && c == '\0') {
		if (access(dp, IWRITE))
			goto out;
		return(dp);
	}
	d = dp->i_dev;
	if (u.u_dent.d_ino == ROOTINO && dp->i_number == ROOTINO && u.u_dent.d_name[1] == '.')
		for (i = 1; i < NMOUNT; i++)
			if (mount[i].m_bufp != NULL && mount[i].m_dev == d) {
				iput(dp);
				dp = mount[i].m_inodp;
				dp->i_count++;
				plock(dp);
				goto cl2;
			}
	iput(dp);
	dp = iget(d, u.u_dent.d_ino);
	if (dp == NULL)
		return(NULL);
	goto cloop;

out:
	iput(dp);
	return(NULL);
}

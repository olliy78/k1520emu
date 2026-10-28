/* iget.c -- Inode-Tabelle (Kernel mutos_1216, 1122..1723): iget, iexpand, iput, iupdat, itrunc, tloop, maknode, wdir.
 * Daten: inode[] = 907A..9EEE (50 Eintraege zu 74 Byte), mount[] = A07E (5 zu 6 Byte), rootdir 8DC6, time 8DCC,
 * Texte 8A1C "no imt", 8A23 "Inode table overflow", 8A39 "iaddress > 2^24"; Sprungtabelle von itrunc bei 8A16.
 * Keine Hash-Kette: iget sucht linear und merkt sich den am laengsten freien Eintrag (i_lastr = Freigabezeit).
 * Abweichung gegenueber V7: Blockadressen werden von Hand (3 Byte, vorne 0) umgesetzt, nicht ueber l3tol. */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "user_ext.h"
#include "inode.h"
#include "mount.h"
#include "ino.h"
#include "filsys.h"
#include "buf.h"

/* KERNEL 1122 */
struct inode *
iget(dev, ino)
dev_t dev;
ino_t ino;
{
	register struct inode *ip;
	register struct mount *mp;
	register struct inode *oip;
	register struct buf *bp;
	register struct dinode *dp;

loop:
	oip = NULL;
	for (ip = &inode[0]; ip < &inode[NINODE]; ip++) {
		if (ino == ip->i_number && dev == ip->i_dev) {
			if ((ip->i_flag & ILOCK) != 0) {
				ip->i_flag |= IWANT;
				sleep((caddr_t)ip, PINOD);
				goto loop;
			}
			if ((ip->i_flag & IMOUNT) != 0) {
				for (mp = &mount[0]; mp < &mount[NMOUNT]; mp++)
					if (mp->m_inodp == ip) {
						dev = mp->m_dev;
						ino = ROOTINO;
						goto loop;
					}
				panic("no imt");
			}
			if (ip->i_count++ == 0)
				ip->i_lastr = 0;
			ip->i_flag |= ILOCK;
			return(ip);
		}
		if (ip->i_count == 0 && (oip == NULL || oip->i_lastr > ip->i_lastr))
			oip = ip;
	}
	ip = oip;
	if (ip == NULL) {
		printf("Inode table overflow\n");
		u.u_error = ENFILE;
		return(NULL);
	}
	ip->i_dev = dev;
	ip->i_number = ino;
	ip->i_flag = ILOCK;
	ip->i_count++;
	ip->i_lastr = 0;
	bp = bread(dev, itod(ino));
	if ((bp->b_flags & B_ERROR) != 0) {
		brelse(bp);
		iput(ip);
		return(NULL);
	}
	dp = (struct dinode *)bp->b_addr;
	dp += itoo(ino);
	iexpand(ip, dp);
	brelse(bp);
	return(ip);
}

/* KERNEL 126A */
iexpand(ip, dp)
register struct inode *ip;
register struct dinode *dp;
{
	register char *p1;
	char *p2;
	int i;

	ip->i_mode = dp->di_mode;
	ip->i_nlink = dp->di_nlink;
	ip->i_uid = dp->di_uid;
	ip->i_gid = dp->di_gid;
	ip->i_size = dp->di_size;
	p1 = (char *)ip->i_addr;
	p2 = dp->di_addr;
	for (i = 0; i < NADDR; i++) {
		*p1++ = 0;
		*p1++ = *p2++;
		*p1++ = *p2++;
		*p1++ = *p2++;
	}
}

/* KERNEL 12FE */
iput(ip)
register struct inode *ip;
{
	if (ip->i_count == 1) {
		ip->i_flag |= ILOCK;
		if (ip->i_nlink <= 0) {
			itrunc(ip);
			ip->i_mode = 0;
			ip->i_flag |= IUPD|ICHG;
			ifree(ip->i_dev, ip->i_number);
		}
		iupdat(ip, &time, &time);
		prele(ip);
		ip->i_flag = 0;
		ip->i_lastr = time;
	}
	ip->i_count--;
	prele(ip);
}

/* KERNEL 1372 */
iupdat(ip, ta, tm)
register struct inode *ip;
time_t *ta, *tm;
{
	register struct buf *bp;
	register char *p1;
	struct dinode *dp;
	char *p2;
	int i;

	if ((ip->i_flag & (IUPD|IACC|ICHG)) == 0)
		return;
	if (getfs(ip->i_dev)->s_ronly != 0)
		return;
	bp = bread(ip->i_dev, itod(ip->i_number));
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		return;
	}
	dp = (struct dinode *)bp->b_addr;
	dp += itoo(ip->i_number);
	dp->di_mode = ip->i_mode;
	dp->di_nlink = ip->i_nlink;
	dp->di_uid = ip->i_uid;
	dp->di_gid = ip->i_gid;
	dp->di_size = ip->i_size;
	p1 = dp->di_addr;
	p2 = (char *)ip->i_addr;
	for (i = 0; i < NADDR; i++) {
		if (*p2++ != 0 && (ip->i_mode & IFMT) != 0030000 && (ip->i_mode & IFMT) != 0070000)
			printf("iaddress > 2^24\n");
		*p1++ = *p2++;
		*p1++ = *p2++;
		*p1++ = *p2++;
	}
	if (ip->i_flag & IACC)
		dp->di_atime = *ta;
	if (ip->i_flag & IUPD)
		dp->di_mtime = *tm;
	if (ip->i_flag & ICHG)
		dp->di_ctime = time;
	ip->i_flag &= ~(IUPD|IACC|ICHG);
	bdwrite(bp);
}

/* KERNEL 14F6 */
itrunc(ip)
register struct inode *ip;
{
	register i;
	dev_t dev;
	daddr_t bn;

	i = ip->i_mode & IFMT;
	if (i != IFREG && i != IFDIR)
		return;
	dev = ip->i_dev;
	for (i = NADDR-1; i >= 0; i--) {
		bn = ip->i_addr[i];
		if (bn == (daddr_t)0)
			continue;
		ip->i_addr[i] = (daddr_t)0;
		switch (i) {

		default:
			free(dev, bn);
			break;

		case NADDR-3:
			tloop(dev, bn, 0, 0);
			break;

		case NADDR-2:
			tloop(dev, bn, 1, 0);
			break;

		case NADDR-1:
			tloop(dev, bn, 1, 1);
		}
	}
	ip->i_size = 0;
	ip->i_flag |= ICHG|IUPD;
}

/* KERNEL 15B2 */
tloop(dev, bn, f1, f2)
dev_t dev;
daddr_t bn;
{
	register i;
	register struct buf *bp;
	register daddr_t *bap;
	daddr_t nb;

	bp = NULL;
	for (i = NINDIR-1; i >= 0; i--) {
		if (bp == NULL) {
			bp = bread(dev, bn);
			if (bp->b_flags & B_ERROR) {
				brelse(bp);
				return;
			}
			bap = (daddr_t *)bp->b_addr;
		}
		nb = bap[i];
		if (nb == (daddr_t)0)
			continue;
		if (f1) {
			brelse(bp);
			bp = NULL;
			tloop(dev, nb, f2, 0);
		} else
			free(dev, nb);
	}
	if (bp != NULL)
		brelse(bp);
	free(dev, bn);
}

/* KERNEL 1652 */
struct inode *
maknode(mode)
{
	register struct inode *ip;

	ip = ialloc(u.u_pdir->i_dev);
	if (ip == NULL) {
		iput(u.u_pdir);
		return(NULL);
	}
	ip->i_flag |= IACC|IUPD|ICHG;
	if ((mode & IFMT) == 0)
		mode |= IFREG;
	ip->i_mode = mode & ~U_CMASK;
	ip->i_nlink = 1;
	ip->i_uid = u.u_uid;
	ip->i_gid = u.u_gid;
	wdir(ip);
	return(ip);
}

/* KERNEL 16C2 */
wdir(ip)
struct inode *ip;
{
	if (u.u_pdir->i_nlink <= 0) {
		u.u_error = ENOTDIR;
	} else {
		u.u_dent.d_ino = ip->i_number;
		bcopy((caddr_t)u.u_dbuf, (caddr_t)u.u_dent.d_name, DIRSIZ);
		u.u_count = sizeof(struct direct);
		u.u_segflg = 1;
		u.u_base = (caddr_t)&u.u_dent;
		writei(u.u_pdir);
	}
	iput(u.u_pdir);
}

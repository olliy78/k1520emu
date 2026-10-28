/* sys3.c -- fstat, stat, stat1, dup, smount, sumount, getmdev, exec (Kernel mutos_1216, 3D6A..41AC).
 * exec (4198) ist nur der Stumpf, der das dritte Argument loescht und exece (sys1.c) ruft.
 * Daten: mount[] = A07E..A09C (5 Eintraege zu 6 Byte), inode[] = 907A, nblkdev = 8DD0, bdevsw = 8766 (d_open +0, d_close +2). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "proc.h"
#include "inode.h"
#include "file.h"
#include "buf.h"
#include "conf.h"
#include "filsys.h"
#include "mount.h"

struct stat {
	dev_t		st_dev;
	ino_t		st_ino;
	unsigned	st_mode;
	int		st_nlink;
	int		st_uid;
	int		st_gid;
	dev_t		st_rdev;
	off_t		st_size;
	time_t		st_atime;
	time_t		st_mtime;
	time_t		st_ctime;
};

/* Platteninode (64 Byte): die drei Zeiten liegen bei 34h, 38h, 3Ch */
struct dinode {
	char		di_x[0x34];
	time_t		di_atime;
	time_t		di_mtime;
	time_t		di_ctime;
};

/* KERNEL 3D6A */
fstat()
{
	register struct file *fp;
	register struct a {
		int		fdes;
		struct stat	*sb;
	} *uap;

	uap = (struct a *)u.u_ap;
	fp = getf(uap->fdes);
	if (fp == NULL)
		return;
	stat1(fp->f_inode, uap->sb, fp->f_flag & FPIPE ? fp->f_offset : (off_t)0);
}

/* KERNEL 3DA2 */
stat()
{
	register struct inode *ip;
	register struct a {
		char		*fname;
		struct stat	*sb;
	} *uap;

	uap = (struct a *)u.u_ap;
	ip = namei(uchar, 0);
	if (ip == NULL)
		return;
	stat1(ip, uap->sb, (off_t)0);
	iput(ip);
}

/* KERNEL 3DDC */
stat1(ip, ub, pipeadr)
register struct inode *ip;
struct stat *ub;
off_t pipeadr;
{
	register struct dinode *dp;
	register struct buf *bp;
	struct stat ds;

	iupdat(ip, &time, &time);
	ds.st_dev = ip->i_dev;
	ds.st_ino = ip->i_number;
	ds.st_mode = ip->i_mode;
	ds.st_nlink = ip->i_nlink;
	ds.st_uid = ip->i_uid;
	ds.st_gid = ip->i_gid;
	ds.st_rdev = (dev_t)ip->i_addr[0];
	ds.st_size = ip->i_size - pipeadr;
	bp = bread(ip->i_dev, itod(ip->i_number));
	dp = (struct dinode *)bp->b_addr;
	dp += itoo(ip->i_number);
	ds.st_atime = dp->di_atime;
	ds.st_mtime = dp->di_mtime;
	ds.st_ctime = dp->di_ctime;
	brelse(bp);
	if (copyout((caddr_t)&ds, (caddr_t)ub, sizeof(ds)) < 0)
		u.u_error = EFAULT;
}

/* KERNEL 3EB2 */
dup()
{
	register struct file *fp;
	register struct a {
		int	i;
		int	i2;
	} *uap;
	register i, m;

	uap = (struct a *)u.u_ap;
	m = uap->i & ~077;
	uap->i &= 077;
	fp = getf(uap->i);
	if (fp == NULL)
		return;
	if ((m & 0100) == 0) {
		if ((i = ufalloc()) < 0)
			return;
	} else {
		i = uap->i2;
		if (i < 0 || i >= NOFILE) {
			u.u_error = EBADF;
			return;
		}
		u.u_r.r_val1 = i;
	}
	if (i == uap->i)
		return;
	if (u.u_ofile[i] != NULL)
		closef(u.u_ofile[i]);
	u.u_ofile[i] = fp;
	fp->f_count++;
}

/* KERNEL 3F3C */
smount()
{
	dev_t dev;
	register struct inode *ip;
	register struct mount *mp;
	register struct filsys *fp;
	register struct a {
		char	*fspec;
		char	*freg;
		int	ronly;
	} *uap;
	struct mount *smp;
	struct buf *bp;

	uap = (struct a *)u.u_ap;
	dev = getmdev();
	if (u.u_error)
		return;
	u.u_dirp = (caddr_t)uap->freg;
	ip = namei(uchar, 0);
	if (ip == NULL)
		return;
	if (ip->i_count != 1 || (ip->i_mode & (IFBLK & IFCHR)) != 0)
		goto out;
	smp = NULL;
	for (mp = &mount[0]; mp < &mount[NMOUNT]; mp++) {
		if (mp->m_bufp != NULL) {
			if (dev == mp->m_dev)
				goto out;
		} else if (smp == NULL)
			smp = mp;
	}
	mp = smp;
	if (mp == NULL)
		goto out;
	(*bdevsw[major(dev)].d_open)(dev, !uap->ronly);
	if (u.u_error)
		goto out1;
	bp = bread(dev, (daddr_t)1);
	if (u.u_error) {
		brelse(bp);
		goto out1;
	}
	mp->m_inodp = ip;
	mp->m_dev = dev;
	mp->m_bufp = geteblk();
	bcopy((caddr_t)bp->b_addr, (caddr_t)mp->m_bufp->b_addr, BSIZE);
	fp = (struct filsys *)mp->m_bufp->b_addr;
	fp->s_ilock = 0;
	fp->s_flock = 0;
	fp->s_ronly = uap->ronly & 1;
	brelse(bp);
	ip->i_flag |= IMOUNT;
	prele(ip);
	return;
out:
	u.u_error = EBUSY;
out1:
	iput(ip);
}

/* KERNEL 4080 */
sumount()
{
	dev_t dev;
	register struct inode *ip;
	register struct mount *mp;
	struct buf *bp;

	dev = getmdev();
	if (u.u_error)
		return;
	xumount(dev);
	update();
	for (mp = &mount[0]; mp < &mount[NMOUNT]; mp++)
		if (mp->m_bufp != NULL && dev == mp->m_dev)
			goto found;
	u.u_error = EINVAL;
	return;
found:
	for (ip = &inode[0]; ip < &inode[NINODE]; ip++)
		if (dev == ip->i_dev) {
			if (ip->i_count) {
				u.u_error = EBUSY;
				return;
			}
			ip->i_number = 0;
		}
	(*bdevsw[major(dev)].d_close)(dev, 0);
	ip = mp->m_inodp;
	ip->i_flag &= ~IMOUNT;
	plock(ip);
	iput(ip);
	bp = mp->m_bufp;
	mp->m_bufp = NULL;
	brelse(bp);
}

/* KERNEL 413C */
getmdev()
{
	dev_t d;
	register struct inode *ip;

	ip = namei(uchar, 0);
	if (ip == NULL)
		return(NODEV);
	if ((ip->i_mode & IFMT) != IFBLK)
		u.u_error = ENOTBLK;
	d = (dev_t)ip->i_addr[0];
	if (major(d) >= nblkdev)
		u.u_error = ENXIO;
	iput(ip);
	return(d);
}

/* KERNEL 4198 */
exec()
{
	u.u_ap[2] = 0;
	exece();
}

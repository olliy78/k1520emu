/* sys2.c -- Dateizugriffe (Kernel mutos_1216, 51B2..562C): read, write, rdwr, open, creat, copen (V7 open1), close, seek, link, mknod,
 * saccess.  saccess steht im Kernel direkt vor xswap (55AE), gehoert zu sys2.  Pipes: rdwr ruft fuer FPIPE-Dateien readp/writep
 * (K6b nannte sie openp/closep; sie rufen readi/writei).  f_flag-Bits: FREAD 1, FWRITE 2, FPIPE 4, 030 = ohne Offset
 * (Geraet), seek verweigert 034 (ESPIPE). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "proc.h"
#include "inode.h"
#include "file.h"
#include "reg.h"

/* KERNEL 51B2 */
read()
{
	rdwr(FREAD);
}

/* KERNEL 51C4 */
write()
{
	rdwr(FWRITE);
}

/* KERNEL 51D6 */
rdwr(mode)
register mode;
{
	register struct file *fp;
	register struct inode *ip;
	register struct a {
		int		fdes;
		char		*cbuf;
		unsigned	count;
	} *uap;

	uap = (struct a *)u.u_ap;
	fp = getf(uap->fdes);
	if (fp == NULL)
		return;
	if ((fp->f_flag & mode) == 0) {
		u.u_error = EBADF;
		return;
	}
	u.u_base = (caddr_t)uap->cbuf;
	u.u_count = uap->count;
	u.u_segflg = 0;
	if (fp->f_flag & FPIPE) {
		if (mode == FREAD)
			readp(fp);
		else
			writep(fp);
	} else {
		ip = fp->f_inode;
		if (fp->f_flag & 030)
			u.u_offset = 0;
		else
			u.u_offset = fp->f_offset;
		if ((ip->i_mode & IFCHR) == 0)
			plock(ip);
		if (mode == FREAD)
			readi(ip);
		else
			writei(ip);
		if ((ip->i_mode & IFCHR) == 0)
			prele(ip);
		if ((fp->f_flag & 030) == 0)
			fp->f_offset += uap->count - u.u_count;
	}
	u.u_r.r_val1 = uap->count - u.u_count;
}

/* KERNEL 52BE */
open()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		int	mode;
	} *uap;

	uap = (struct a *)u.u_ap;
	ip = namei(uchar, 0);
	if (ip == NULL)
		return;
	copen(ip, ++uap->mode, 0);
}

/* KERNEL 52F2 */
creat()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		int	fmode;
	} *uap;

	uap = (struct a *)u.u_ap;
	ip = namei(uchar, 1);
	if (ip == NULL) {
		if (u.u_error)
			return;
		ip = maknode(uap->fmode & 07777 & (~ISVTX));
		if (ip == NULL)
			return;
		copen(ip, FWRITE, 2);
	} else
		copen(ip, FWRITE, 1);
}

/* KERNEL 5344 */
copen(ip, mode, trf)
register struct inode *ip;
register mode;
{
	register struct file *fp;
	int i;

	if (trf != 2) {
		if (mode & FREAD)
			access(ip, IREAD);
		if (mode & FWRITE) {
			access(ip, IWRITE);
			if ((ip->i_mode & IFMT) == IFDIR)
				u.u_error = EISDIR;
		}
	}
	if (u.u_error)
		goto out;
	if (trf == 1)
		itrunc(ip);
	prele(ip);
	if ((fp = falloc()) == NULL)
		goto out;
	fp->f_flag = mode & (FREAD|FWRITE);
	fp->f_inode = ip;
	i = u.u_r.r_val1;
	openi(ip, mode & FWRITE);
	if (u.u_error == 0)
		return;
	u.u_ofile[i] = NULL;
	fp->f_count--;
out:
	iput(ip);
}

/* KERNEL 53FE */
close()
{
	register struct file *fp;
	register struct a {
		int	fdes;
	} *uap;

	uap = (struct a *)u.u_ap;
	fp = getf(uap->fdes);
	if (fp == NULL)
		return;
	u.u_ofile[uap->fdes] = NULL;
	closef(fp);
}

/* KERNEL 542C */
seek()
{
	register struct file *fp;
	register struct a {
		int	fdes;
		off_t	off;
		int	sbase;
	} *uap;

	uap = (struct a *)u.u_ap;
	fp = getf(uap->fdes);
	if (fp == NULL)
		return;
	if (fp->f_flag & 034) {
		u.u_error = ESPIPE;
		return;
	}
	if (uap->sbase == 1)
		uap->off += fp->f_offset;
	else if (uap->sbase == 2)
		uap->off += fp->f_inode->i_size;
	if (uap->off < 0) {
		u.u_error = EINVAL;
		uap->off = 0;
	}
	fp->f_offset = uap->off;
	u.u_r.r_off = uap->off;
}

/* KERNEL 54AA */
link()
{
	register struct inode *ip, *xp;
	register struct a {
		char	*target;
		char	*linkname;
	} *uap;

	uap = (struct a *)u.u_ap;
	ip = namei(uchar, 0);
	if (ip == NULL)
		return;
	if ((ip->i_mode & IFMT) == IFDIR && !suser())
		goto out;
	prele(ip);
	u.u_dirp = (caddr_t)uap->linkname;
	xp = namei(uchar, 1);
	if (xp != NULL) {
		u.u_error = EEXIST;
		iput(xp);
		goto out;
	}
	if (u.u_error)
		goto out;
	if (u.u_pdir->i_dev != ip->i_dev) {
		iput(u.u_pdir);
		u.u_error = EXDEV;
		goto out;
	}
	wdir(ip);
	if (u.u_error == 0) {
		ip->i_nlink++;
		ip->i_flag |= ICHG;
	}
out:
	iput(ip);
}

/* KERNEL 5556 */
mknod()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		int	fmode;
		int	dev;
	} *uap;

	uap = (struct a *)u.u_ap;
	if (suser()) {
		ip = namei(uchar, 1);
		if (ip != NULL) {
			u.u_error = EEXIST;
			goto out;
		}
	}
	if (u.u_error)
		return;
	ip = maknode(uap->fmode);
	if (ip == NULL)
		return;
	ip->i_addr[0] = uap->dev;
out:
	iput(ip);
}

/* KERNEL 55AE */
saccess()
{
	register svuid, svgid;
	register struct inode *ip;
	register struct a {
		char	*fname;
		int	fmode;
	} *uap;

	uap = (struct a *)u.u_ap;
	svuid = u.u_uid;
	svgid = u.u_gid;
	u.u_uid = u.u_ruid;
	u.u_gid = u.u_rgid;
	ip = namei(uchar, 0);
	if (ip != NULL) {
		if (uap->fmode & (IREAD>>6))
			access(ip, IREAD);
		if (uap->fmode & (IWRITE>>6))
			access(ip, IWRITE);
		if (uap->fmode & (IEXEC>>6))
			access(ip, IEXEC);
		iput(ip);
	}
	u.u_uid = svuid;
	u.u_gid = svgid;
}

/* fio.c -- Dateizugriff (Kernel mutos_1216, 2AD4..2DEF): getf, closef, openi, access, owner, suser, ufalloc, falloc.
 * Daten: file[] = 9EEE..A07E (50 Eintraege zu 8 Byte), u_ofile = FC62, Text 8ADA "no file\n", nchrdev 8DD2, nblkdev 8DD0,
 * cdevsw 8796 (14 Byte je Eintrag: d_open +0, d_close +2), bdevsw 8766 (8 Byte: d_open +0, d_close +2). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "inode.h"
#include "file.h"
#include "conf.h"
#include "filsys.h"

/* KERNEL 2AD4 */
struct file *
getf(f)
register f;
{
	register *fp;

	if (f >= 0 && f < 20 && (fp = u.u_ofile[f]) != 0)
		return(fp);
	u.u_error = EBADF;
	return(0);
}

/* KERNEL 2B06 */
closef(fp)
register struct file *fp;
{
	register struct inode *ip;
	int flag, mode;
	dev_t dev;
	struct chan *cp;
	register int (*cfunc)();

	if (fp == NULL)
		return;
	if (fp->f_count > 1) {
		fp->f_count--;
		return;
	}
	ip = fp->f_inode;
	flag = fp->f_flag;
	cp = fp->f_un.f_chan;
	dev = (dev_t)ip->i_addr[0];
	mode = ip->i_mode;
	plock(ip);
	fp->f_count = 0;
	if (flag & FPIPE) {
		ip->i_mode &= ~(IREAD|IWRITE);
		wakeup((caddr_t)ip + 1);
		wakeup((caddr_t)ip + 2);
	}
	iput(ip);
	switch (mode & IFMT) {
	case IFCHR:
	case 0030000:
		cfunc = cdevsw[major(dev)].d_close;
		break;
	case IFBLK:
	case 0070000:
		cfunc = bdevsw[major(dev)].d_close;
		break;
	default:
		return;
	}
	if ((flag & 030) == 0) {
		for (fp = file; fp < &file[NFILE]; fp++)
			if (fp->f_count && fp->f_inode == ip)
				return;
	}
	(*cfunc)(dev, flag, cp);
}

/* KERNEL 2C0C */
openi(ip, rw)
register struct inode *ip;
{
	dev_t dev;
	register unsigned maj;

	dev = (dev_t)ip->i_addr[0];
	maj = major(dev);
	switch (ip->i_mode & IFMT) {
	case IFCHR:
	case 0030000:
		if (maj >= nchrdev)
			goto bad;
		(*cdevsw[maj].d_open)(dev, rw);
		break;
	case IFBLK:
	case 0070000:
		if (maj >= nblkdev)
			goto bad;
		(*bdevsw[maj].d_open)(dev, rw);
	}
	return;
bad:
	u.u_error = ENXIO;
}

/* KERNEL 2C8C */
access(ip, mode)
register struct inode *ip;
{
	register m;

	m = mode;
	if (m == IWRITE) {
		if (getfs(ip->i_dev)->s_ronly != 0) {
			u.u_error = EROFS;
			return(1);
		}
		if (ip->i_flag & ITEXT)
			xrele(ip);
		if (ip->i_flag & ITEXT) {
			u.u_error = ETXTBSY;
			return(1);
		}
	}
	if (u.u_uid == 0)
		return(0);
	if (u.u_uid != ip->i_uid) {
		m >>= 3;
		if (u.u_gid != ip->i_gid)
			m >>= 3;
	}
	if ((ip->i_mode & m) != 0)
		return(0);
	u.u_error = EACCES;
	return(1);
}

/* KERNEL 2D0A */
struct inode *
owner()
{
	register struct inode *ip;

	ip = namei(uchar, 0);
	if (ip == NULL)
		return(NULL);
	if (u.u_uid == ip->i_uid)
		return(ip);
	if (suser())
		return(ip);
	iput(ip);
	return(NULL);
}

/* KERNEL 2D48 */
suser()
{
	if (u.u_uid == 0)
		return(1);
	u.u_error = EPERM;
	return(0);
}

/* KERNEL 2D62 */
ufalloc()
{
	register i;

	for (i = 0; i < NOFILE; i++)
		if (u.u_ofile[i] == NULL) {
			u.u_r.r_val1 = i;
			u.u_pofile[i] = 0;
			return(i);
		}
	u.u_error = EMFILE;
	return(-1);
}

/* KERNEL 2D9A */
struct file *
falloc()
{
	register struct file *fp;
	register i;

	if ((i = ufalloc()) < 0)
		return(NULL);
	for (fp = &file[0]; fp < &file[NFILE]; fp++)
		if (fp->f_count == 0) {
			u.u_ofile[i] = fp;
			fp->f_count++;
			fp->f_offset = 0;
			return(fp);
		}
	printf("no file\n");
	u.u_error = ENFILE;
	return(NULL);
}

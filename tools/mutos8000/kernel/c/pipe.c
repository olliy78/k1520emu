/* pipe.c -- Pipes, Inode-Sperren, Zeichenlisten (Kernel mutos_1216, 39BA..3CF0): uchar, pipe, readp, writep, plock, prele, getc, putc.
 * Namen: K6b nannte readp/writep openp/closep; sie sind V7 readp/writep (PIPSIZ 4096, wchan ip+1 = Schreiber, ip+2 = Leser).
 * getc/putc stehen im Kernel hinter prele (Zeichenlisten fuer tty); Daten: cfreelist = D266 (BSS), cblock = 16 Byte (c_next + 14 Zeichen).
 * uchar liest ein Byte aus dem Nutzerraum (04CC = fubyte, wie in cpass). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "inode.h"
#include "file.h"

struct cblock {
	struct cblock	*c_next;
	char		c_info[14];
};
struct clist {
	int		c_cc;
	char		*c_cf;
	char		*c_cl;
};
extern struct cblock *cfreelist;
#define CROUND	017

/* KERNEL 39BA */
uchar()
{
	register c;

	c = fubyte(u.u_dirp++);
	if (c == -1)
		u.u_error = EFAULT;
	return(c);
}

/* KERNEL 39E2 */
pipe()
{
	register struct inode *ip;
	register struct file *rf, *wf;
	int r;

	ip = ialloc(pipedev);
	if (ip == NULL)
		return;
	rf = falloc();
	if (rf == NULL) {
		iput(ip);
		return;
	}
	r = u.u_r.r_val1;
	wf = falloc();
	if (wf == NULL) {
		rf->f_count = 0;
		u.u_ofile[r] = NULL;
		iput(ip);
		return;
	}
	u.u_r.r_val2 = u.u_r.r_val1;
	u.u_r.r_val1 = r;
	wf->f_flag = FWRITE|FPIPE;
	wf->f_inode = ip;
	rf->f_flag = FREAD|FPIPE;
	rf->f_inode = ip;
	ip->i_count = 2;
	ip->i_mode = IFREG;
	ip->i_flag = IACC|IUPD|ICHG;
}

/* KERNEL 3A68 */
readp(fp)
register struct file *fp;
{
	register struct inode *ip;

	ip = fp->f_inode;
	while (plock(ip), ip->i_size == 0) {
		prele(ip);
		if (ip->i_count < 2)
			return;
		ip->i_mode |= IREAD;
		sleep((caddr_t)ip + 2, PPIPE);
	}
	u.u_offset = fp->f_offset;
	readi(ip);
	fp->f_offset = u.u_offset;
	if (fp->f_offset == ip->i_size) {
		fp->f_offset = 0;
		ip->i_size = 0;
		if (ip->i_mode & IWRITE) {
			ip->i_mode &= ~IWRITE;
			wakeup((caddr_t)ip + 1);
		}
	}
	prele(ip);
}

/* KERNEL 3AFE */
writep(fp)
register struct file *fp;
{
	register c;
	register struct inode *ip;

	ip = fp->f_inode;
	c = u.u_count;
loop:
	plock(ip);
	if (c == 0) {
		prele(ip);
		u.u_count = 0;
		return;
	}
	if (ip->i_count < 2) {
		prele(ip);
		u.u_error = EPIPE;
		psignal(u.u_procp, SIGPIPE);
		return;
	}
	if (ip->i_size >= PIPSIZ) {
		ip->i_mode |= IWRITE;
		prele(ip);
		sleep((caddr_t)ip + 1, PPIPE);
		goto loop;
	}
	u.u_offset = ip->i_size;
	u.u_count = min((unsigned)c, (unsigned)PIPSIZ);
	c -= u.u_count;
	writei(ip);
	prele(ip);
	if (ip->i_mode & IREAD) {
		ip->i_mode &= ~IREAD;
		wakeup((caddr_t)ip + 2);
	}
	goto loop;
}

/* KERNEL 3BBE */
plock(ip)
register struct inode *ip;
{
	while (ip->i_flag & ILOCK) {
		ip->i_flag |= IWANT;
		sleep((caddr_t)ip, PINOD);
	}
	ip->i_flag |= ILOCK;
}

/* KERNEL 3BE0 */
prele(ip)
register struct inode *ip;
{
	ip->i_flag &= ~ILOCK;
	if (ip->i_flag & IWANT) {
		ip->i_flag &= ~IWANT;
		wakeup((caddr_t)ip);
	}
}

/* KERNEL 3BFC */
getc(p)
register struct clist *p;
{
	register struct cblock *bp;
	register c, s;

	s = spl6();
	if (p->c_cc <= 0) {
		c = -1;
		p->c_cc = 0;
		p->c_cf = p->c_cl = NULL;
	} else {
		c = *p->c_cf++ & 0377;
		if (--p->c_cc <= 0) {
			bp = (struct cblock *)(p->c_cf - 1);
			bp = (struct cblock *)((int)bp & ~CROUND);
			p->c_cf = NULL;
			p->c_cl = NULL;
			bp->c_next = cfreelist;
			cfreelist = bp;
		} else if (((int)p->c_cf & CROUND) == 0) {
			bp = (struct cblock *)(p->c_cf);
			bp--;
			p->c_cf = bp->c_next->c_info;
			bp->c_next = cfreelist;
			cfreelist = bp;
		}
	}
	splx(s);
	return(c);
}

/* KERNEL 3C80 */
putc(c, p)
register struct clist *p;
{
	register struct cblock *bp;
	register char *cp;
	register s;

	s = spl6();
	if ((cp = p->c_cl) == NULL || p->c_cc < 0) {
		if ((bp = cfreelist) == NULL) {
			splx(s);
			return(-1);
		}
		cfreelist = bp->c_next;
		bp->c_next = NULL;
		p->c_cf = cp = bp->c_info;
	} else if (((int)cp & CROUND) == 0) {
		bp = (struct cblock *)cp - 1;
		if ((bp->c_next = cfreelist) == NULL) {
			splx(s);
			return(-1);
		}
		bp = bp->c_next;
		cfreelist = bp->c_next;
		bp->c_next = NULL;
		cp = bp->c_info;
	}
	*cp++ = c;
	p->c_cc++;
	p->c_cl = cp;
	splx(s);
	return(0);
}

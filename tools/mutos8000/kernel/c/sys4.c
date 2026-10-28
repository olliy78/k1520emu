/* sys4.c -- kleine Systemaufrufe (Kernel mutos_1216, 4C14..51B2): gtime, ftime, stime, setuid, getuid, setgid, getgid, getpid, sync,
 * nice, unlink, chdir, chroot, chdirec, chmod, chown, ssig, kill, times, profil, alarm, pause, umask, utime.
 * Die K6b-Namen waren um eine Stelle verschoben: 5100 ist `alarm` (nicht nice), nice liegt bei 4D76.
 * Daten: time = 8DCC (long), lbolt = 8DCA, Treffer des Maschinenzeit-Zaehlers.  getswit gibt es hier nicht. */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "proc.h"
#include "inode.h"
#include "file.h"
#include "reg.h"

struct timeb {
	time_t		tb_time;
	unsigned	tb_millitm;
	short		tb_timezone;
	short		tb_dstflag;
};

/* KERNEL 4C14 */
gtime()
{
	u.u_r.r_time = time;
}

/* KERNEL 4C24 */
ftime()
{
	register struct a {
		struct timeb	*tp;
	} *uap;
	register unsigned ms;
	struct timeb tb;

	uap = (struct a *)u.u_ap;
	spl7();
	tb.tb_time = time;
	ms = lbolt;
	spl0();
	if (ms > HZ) {
		ms -= HZ;
		tb.tb_time++;
	}
	tb.tb_millitm = (int)ms * 1000 / HZ;
	tb.tb_timezone = 0;
	tb.tb_dstflag = 0;
	if (copyout((caddr_t)&tb, (caddr_t)uap->tp, sizeof(tb)) < 0)
		u.u_error = EFAULT;
}

/* KERNEL 4C94 */
stime()
{
	register struct a {
		time_t	time;
	} *uap;
	register struct inode *ip;

	uap = (struct a *)u.u_ap;
	if (suser()) {
		for (ip = &inode[0]; ip < &inode[NINODE]; ip++)
			ip->i_lastr = 0;
		time = uap->time;
	}
}

/* KERNEL 4CC6 */
setuid()
{
	register uid, *uap;

	uap = u.u_ap;
	uid = uap[0];
	if (u.u_ruid == uid || suser()) {
		u.u_uid = uid;
		u.u_procp->p_uid = uid;
		u.u_ruid = uid;
	}
}

/* KERNEL 4CF4 */
getuid()
{
	u.u_r.r_val1 = u.u_ruid;
	u.u_r.r_val2 = u.u_uid;
}

/* KERNEL 4D0C */
setgid()
{
	register gid, *uap;

	uap = u.u_ap;
	gid = uap[0];
	if (u.u_rgid == gid || suser()) {
		u.u_gid = gid;
		u.u_rgid = gid;
	}
}

/* KERNEL 4D32 */
getgid()
{
	u.u_r.r_val1 = u.u_rgid;
	u.u_r.r_val2 = u.u_gid;
}

/* KERNEL 4D4A */
getpid()
{
	u.u_r.r_val1 = u.u_procp->p_pid;
	u.u_r.r_val2 = u.u_procp->p_ppid;
}

/* KERNEL 4D6A */
sync()
{
	update();
}

/* KERNEL 4D76 */
nice()
{
	register n;
	register *uap;

	uap = u.u_ap;
	n = uap[0];
	if (n < 0 && !suser())
		n = 0;
	n += u.u_procp->p_nice;
	if (n >= 2*NZERO)
		n = 2*NZERO - 1;
	if (n < 0)
		n = 0;
	u.u_procp->p_nice = n;
}

/* KERNEL 4DBC */
unlink()
{
	register struct inode *ip, *pp;

	pp = namei(uchar, 2);
	if (pp == NULL)
		return;
	if (pp->i_number == u.u_dent.d_ino) {
		ip = pp;
		ip->i_count++;
	} else
		ip = iget(pp->i_dev, u.u_dent.d_ino);
	if (ip == NULL)
		goto out1;
	if ((ip->i_mode & IFMT) == IFDIR && !suser())
		goto out;
	if (ip->i_dev != pp->i_dev) {
		u.u_error = EBUSY;
		goto out;
	}
	if (ip->i_flag & ITEXT)
		xrele(ip);
	if (ip->i_flag & ITEXT && ip->i_nlink == 1) {
		u.u_error = ETXTBSY;
		goto out;
	}
	u.u_offset -= (unsigned)(DIRSIZ+2);
	u.u_base = (caddr_t)&u.u_dent;
	u.u_count = DIRSIZ+2;
	u.u_dent.d_ino = 0;
	writei(pp);
	ip->i_nlink--;
	ip->i_flag |= ICHG;
out:
	iput(ip);
out1:
	iput(pp);
}

/* KERNEL 4E84 */
chdir()
{
	chdirec(&u.u_cdir);
}

/* KERNEL 4E96 */
chroot()
{
	if (suser())
		chdirec(&u.u_rdir);
}

/* KERNEL 4EB0 */
chdirec(ipp)
register struct inode **ipp;
{
	register struct inode *ip;

	ip = namei(uchar, 0);
	if (ip == NULL)
		return;
	if ((ip->i_mode & IFMT) != IFDIR) {
		u.u_error = ENOTDIR;
		iput(ip);
		return;
	}
	if (access(ip, IEXEC)) {
		iput(ip);
		return;
	}
	prele(ip);
	if (*ipp) {
		plock(*ipp);
		iput(*ipp);
	}
	*ipp = ip;
}

/* KERNEL 4F1C */
chmod()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		int	fmode;
	} *uap;

	uap = (struct a *)u.u_ap;
	if ((ip = owner()) == NULL)
		return;
	ip->i_mode &= ~07777;
	if (u.u_uid)
		uap->fmode &= ~ISVTX;
	ip->i_mode |= uap->fmode & 07777;
	ip->i_flag |= ICHG;
	if (ip->i_flag & ITEXT && (ip->i_mode & ISVTX) == 0)
		xrele(ip);
	iput(ip);
}

/* KERNEL 4F78 */
chown()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		int	uid;
		int	gid;
	} *uap;

	uap = (struct a *)u.u_ap;
	if (!suser() || (ip = owner()) == NULL)
		return;
	ip->i_uid = uap->uid;
	ip->i_gid = uap->gid;
	ip->i_flag |= ICHG;
	iput(ip);
}

/* KERNEL 4FB0 */
ssig()
{
	register a;
	struct b {
		int	signo;
		int	fun;
	} *uap;

	uap = (struct b *)u.u_ap;
	a = uap->signo;
	if (a <= 0 || a >= NSIG || a == SIGKILL) {
		u.u_error = EINVAL;
		return;
	}
	u.u_r.r_val1 = u.u_signal[a];
	u.u_signal[a] = uap->fun;
	u.u_procp->p_sig &= ~(1 << (a-1));
}

/* KERNEL 501C */
kill()
{
	register struct proc *p, *q;
	register a;
	register struct b {
		int	pid;
		int	sig;
	} *uap;
	int f, priv;

	uap = (struct b *)u.u_ap;
	f = 0;
	a = uap->pid;
	priv = 0;
	if (a == -1 && u.u_uid == 0) {
		priv++;
		a = 0;
	}
	q = u.u_procp;
	for (p = &proc[0]; p <= proctop; p++) {
		if (p->p_stat == NULL)
			continue;
		if (a != 0 && p->p_pid != a)
			continue;
		if (a == 0 && ((p->p_pgrp != q->p_pgrp && priv == 0) || p <= &proc[1]))
			continue;
		if (u.u_uid != 0 && u.u_uid != p->p_uid)
			continue;
		f++;
		psignal(p, uap->sig);
	}
	if (f == 0)
		u.u_error = ESRCH;
}

/* KERNEL 50B0 */
times()
{
	register *uap;

	uap = u.u_ap;
	if (copyout((caddr_t)&u.u_utime, (caddr_t)uap[0], 16) < 0)
		u.u_error = EFAULT;
}

/* KERNEL 50D6 */
profil()
{
	register *uap;

	uap = u.u_ap;
	u.u_prof.pr_base = (int *)uap[0];
	u.u_prof.pr_size = uap[1];
	u.u_prof.pr_off = uap[2];
	u.u_prof.pr_scale = uap[3];
}

/* KERNEL 5100 */
alarm()
{
	register struct proc *p;
	register c;
	register *uap;

	uap = u.u_ap;
	p = u.u_procp;
	c = p->p_clktim;
	p->p_clktim = uap[0];
	u.u_r.r_val1 = c;
}

/* KERNEL 511E */
pause()
{
	for (;;)
		sleep((caddr_t)&u, PSLEP);
}

/* KERNEL 5132 */
umask()
{
	register *uap;
	register old;

	uap = u.u_ap;
	old = u.u_cmask;
	u.u_cmask = uap[0] & 0777;
	u.u_r.r_val1 = old;
}

/* KERNEL 5150 */
utime()
{
	register struct a {
		char	*fname;
		time_t	*tptr;
	} *uap;
	register struct inode *ip;
	time_t tv[2];

	uap = (struct a *)u.u_ap;
	if ((ip = owner()) == NULL)
		return;
	if (copyin((caddr_t)uap->tptr, (caddr_t)tv, sizeof(tv))) {
		u.u_error = EFAULT;
	} else {
		ip->i_flag |= IACC|IUPD|ICHG;
		iupdat(ip, &tv[0], &tv[1]);
		iput(ip);
	}
}

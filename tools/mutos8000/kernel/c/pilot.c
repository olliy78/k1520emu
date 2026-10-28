/* K7-Pilot: kleine Kernelfunktionen von mutos_1216, als C im Stil der Zeit (K&R/V7).
 * Jede Funktion traegt die Kerneladresse im Kommentar "KERNEL xxxx"; vergleich.py liest sie von dort.
 * Die user-Struktur ist nur so weit nachgebildet, wie die Funktionen sie brauchen
 * (Abstaende aus dem Kernel, u bei FC00).  Glieder tragen eindeutige Namen (V7-cc). */
struct proc {
	char p_a;
	char p_b;
	int p_c[3];
	int p_uid;
	int p_d;
	int p_pid;
	int p_ppid;
	int p_e[5];
	int p_nice;
};
struct user {
	char u_pad0[18];
	char u_segflg;
	char u_error;
	int u_pad14;
	int u_uid;
	int u_gid;
	int u_ruid;
	int u_rgid;
	struct proc *u_procp;
	int *u_ap;
	int u_rv1;
	int u_rv2;
	char u_pad2[60];
	int *u_ofile[20];
	char u_pofile[20];
};
struct user u;
struct buf {
	int b_flags;
	struct buf *b_forw;
	struct buf *b_back;
	struct buf *av_forw;
	struct buf *av_back;
	int b_dev;
	int b_x;
	int *b_addr;
	long b_blkno;
	char b_pad[2];
	int b_resid;
};
struct devt {
	struct buf *d_tab;
	int d_a[3];
};
struct devt devtab[8];
struct inod {
	char i_flag;
};
struct tt {
	char t_a[3];
	char t_b;
};
struct tt ttab;
long lbolt;
int cmask;

/* KERNEL 2766 */
min(a, b)
unsigned a, b;
{
	if (a < b)
		return(a);
	return(b);
}

/* KERNEL 2D48 */
suser()
{
	if (u.u_uid == 0)
		return(1);
	u.u_error = 1;
	return(0);
}

/* KERNEL 2D62 */
ufalloc()
{
	register i;

	for (i = 0; i < 20; i++)
		if (u.u_ofile[i] == 0) {
			u.u_rv1 = i;
			u.u_pofile[i] = 0;
			return(i);
		}
	u.u_error = 24;
	return(-1);
}

/* KERNEL 2AD4 */
getf(f)
register f;
{
	register *fp;

	if (f < 0 || f >= 20 || (fp = u.u_ofile[f]) == 0) {
		u.u_error = 9;
		return(0);
	}
	return(fp);
}

/* KERNEL 4C14 */
gtime()
{
	*(long *)&u.u_rv1 = lbolt;
}

/* KERNEL 4CF4 */
getuid()
{
	u.u_rv1 = u.u_ruid;
	u.u_rv2 = u.u_uid;
}

/* KERNEL 4D32 */
getgid()
{
	u.u_rv1 = u.u_rgid;
	u.u_rv2 = u.u_gid;
}

/* KERNEL 4D4A */
getpid()
{
	u.u_rv1 = u.u_procp->p_pid;
	u.u_rv2 = u.u_procp->p_ppid;
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

/* KERNEL 5100 */
nice()
{
	register struct proc *p;
	register n;
	register *uap;

	uap = u.u_ap;
	p = u.u_procp;
	n = p->p_nice;
	p->p_nice = uap[0];
	u.u_rv1 = n;
}

/* KERNEL 5132 */
umask()
{
	register *uap;
	register old;

	uap = u.u_ap;
	old = cmask;
	cmask = uap[0] & 0777;
	u.u_rv1 = old;
}

/* KERNEL 3D42 */
chkmem(a, b, c)
unsigned a, b, c;
{
	if (a + b + c > 256) {
		u.u_error = 12;
		return(-1);
	}
	return(0);
}

/* KERNEL 60A8 */
geterror(bp)
register struct buf *bp;
{
	if (bp->b_flags & 4) {
		if ((u.u_error = bp->b_pad[1]) == 0)
			u.u_error = 5;
	}
}

/* KERNEL 5C62 */
iodone(bp)
register struct buf *bp;
{
	bp->b_flags |= 2;
	if (bp->b_flags & 0400)
		brelse(bp);
	else {
		bp->b_flags &= ~0100;
		wakeup(bp);
	}
}

/* KERNEL 5C2E */
notavail(bp)
register struct buf *bp;
{
	register s;

	s = spl6();
	bp->av_back->av_forw = bp->av_forw;
	bp->av_forw->av_back = bp->av_back;
	bp->b_flags |= 010;
	splx(s);
}

/* KERNEL 5C86 */
clrbuf(bp)
struct buf *bp;
{
	register *p;
	register c;

	p = bp->b_addr;
	c = 256;
	do
		*p++ = 0;
	while (--c);
	bp->b_resid = 0;
}

/* KERNEL 243A */
bcopy(from, to, n)
register n;
register char *from, *to;
{
	do
		*to++ = *from++;
	while (--n);
}

/* KERNEL 3BBE */
plock(ip)
register struct inod *ip;
{
	while (ip->i_flag & 1) {
		ip->i_flag |= 020;
		sleep(ip, 10);
	}
	ip->i_flag |= 1;
}

/* KERNEL 3BE0 */
prele(ip)
register struct inod *ip;
{
	ip->i_flag &= ~1;
	if (ip->i_flag & 020) {
		ip->i_flag &= ~020;
		wakeup(ip);
	}
}

/* KERNEL 5A22 */
incore(dev, blkno)
unsigned dev;
long blkno;
{
	register struct buf *bp, *dp;

	dp = devtab[dev >> 8].d_tab;
	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw)
		if (bp->b_blkno == blkno && bp->b_dev == dev)
			return(1);
	return(0);
}

/* KERNEL 20F6 */
nulldev()
{
}

/* KERNEL 6ADA */
chkarg(a)
{
	int x, y;

	x = a & 0377;
	if (x >= 4)
		u.u_error = 6;
}

/* KERNEL 6432 */
setflag()
{
	register struct tt *p;

	p = &ttab;
	p->t_b = 1;
}

/* KERNEL 48A6 */
swapb()
{
	register *uap;

	uap = u.u_ap;
	callx((uap[0] & 0377) << 8);
}

/* ---- Varianten ---- */

/* KERNEL 2AD4 */
getf2(f)
register f;
{
	register *fp;

	if (f >= 0 && f < 20 && (fp = u.u_ofile[f]) != 0)
		return(fp);
	u.u_error = 9;
	return(0);
}

/* KERNEL 3D42 */
chkmem2(a, b, c)
unsigned a, b, c;
{
	if (a + b + c <= 256)
		return(0);
	u.u_error = 12;
	return(-1);
}

/* KERNEL 243A */
bcopy2(from, to, n)
char *from, *to;
int n;
{
	register i;
	register char *f, *t;

	i = n;
	f = from;
	t = to;
	do
		*t++ = *f++;
	while (--i);
}

/* KERNEL 20F6 */
nulldv2()
{
	register i;
}

/* KERNEL 5A22 */
incore2(dev, blkno)
unsigned dev;
long blkno;
{
	register struct buf *bp, *dp;

	dp = devtab[(int)(dev >> 8)].d_tab;
	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw)
		if (bp->b_blkno == blkno && bp->b_dev == dev)
			return(1);
	return(0);
}

/* KERNEL 5A22 */
incore3(dev, blkno)
unsigned dev;
long blkno;
{
	register struct buf *bp, *dp;
	register m;

	m = dev >> 8;
	dp = devtab[m].d_tab;
	for (bp = dp->b_forw; bp != dp; bp = bp->b_forw)
		if (bp->b_blkno == blkno && bp->b_dev == dev)
			return(1);
	return(0);
}

/* ---- Schleifenformen (DJNZ-Frage): Kernel hat DEC; JR NZ ohne DJNZ ---- */

/* KERNEL 243A */
bcB(from, to, n)
char *from, *to;
int n;
{
	register i;
	register char *f, *t;

	i = n;
	f = from;
	t = to;
	for (; i > 0; i--)
		*t++ = *f++;
}

/* KERNEL 243A */
bcC(from, to, n)
char *from, *to;
int n;
{
	register i;
	register char *f, *t;

	i = n;
	f = from;
	t = to;
	while (i--)
		*t++ = *f++;
}

/* KERNEL 243A */
bcD(from, to, n)
char *from, *to;
int n;
{
	register i;
	register char *f, *t;

	i = n;
	f = from;
	t = to;
	do {
		*t++ = *f++;
		i--;
	} while (i);
}

/* KERNEL 243A */
bcE(from, to, n)
char *from, *to;
int n;
{
	register i;
	register char *f, *t;

	i = n;
	f = from;
	t = to;
	do
		*t++ = *f++;
	while (--i != 0);
}

/* KERNEL 243A */
bcF(from, to, n)
char *from, *to;
unsigned n;
{
	register unsigned i;
	register char *f, *t;

	i = n;
	f = from;
	t = to;
	do
		*t++ = *f++;
	while (--i);
}

/* KERNEL 3D42 */
chkmm3(a, b, c)
unsigned a, b, c;
{
	register e;

	e = 12;
	if (a + b + c <= 256)
		return(0);
	u.u_error = e;
	return(-1);
}

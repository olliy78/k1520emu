/* conf.c -- Konfiguration (Kernel mutos_1216, initialisierte Daten 8766..88C5): bdevsw, cdevsw, linesw, rootdev/swapdev/pipedev,
 * swplo, nswap, Adresstabelle fuer ps/pstat (nlist-Ersatz).  Die Tabellen liegen hinter den Konstanten des low-Moduls (875A..8765) im Datenbereich;
 * Ziel dieses Moduls: Datenbytes gleich (werkzeug/datenvergleich.py KERNEL conf.o 8766).
 * Die Zahl der Eintraege ist im Kernel fest: bdevsw 4 + Abschluss (Major 4 = rk, Major 0 = Floppy-Treiber 7E16), cdevsw 14 + Abschluss.
 * Hinweis K6b: der Drucker ist cdevsw[5] (nicht 7); cdevsw[10] = Rohzugriff auf Major 0, cdevsw[13] = rk roh, linesw[0] = ttyopen... */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "conf.h"
#include "tty.h"
#include "proc.h"
#include "file.h"
#include "inode.h"
#include "callo.h"
#include "buf.h"

extern struct devtab fdtab, rktab;	/* 8E6A, 8E82 */
extern struct buf buf[], swbuf1, swbuf2, rrkbuf, bfreelist;
extern struct proc proc[];
extern struct file file[];
extern struct inode inode[];
extern struct callo callout[];
extern int cf_8c94, cf_a620, cf_a628, cf_a464, cf_ceac;
int fdopen(), fdclose(), fdstrategy(), fdread(), fdwrite(), fdioctl();
int rkopen(), rkclose(), rkstrategy(), rkread(), rkwrite();
int tcopen(), tcclose(), tcread(), tcwrite(), tcioctl();
int syopen(), syread(), sywrite(), syioctl(), mmread(), mmwrite();
int lpopen(), lpclose(), lpwrite();
int ttyopen(), ttread(), ttwrite(), ttyinput(), ttkick();

struct linesw {
	int	(*l_open)();
	int	(*l_close)();
	int	(*l_read)();
	int	(*l_write)();
	int	(*l_ioctl)();
	int	(*l_input)();
	int	(*l_output)();
};

struct bdevsw bdevsw[] = {
	{ fdopen,  fdclose,  fdstrategy, &fdtab },
	{ nodev,   nodev,    nodev,      0 },
	{ nodev,   nodev,    nodev,      0 },
	{ nodev,   nodev,    nodev,      0 },
	{ rkopen,  rkclose,  rkstrategy, &rktab },
	{ 0,       0,        0,          0 }
};

struct cdevsw cdevsw[] = {
	{ tcopen,  tcclose,  tcread,  tcwrite, tcioctl, nulldev, 0 },
	{ syopen,  nulldev,  syread,  sywrite, syioctl, nulldev, 0 },
	{ nulldev, nulldev,  mmread,  mmwrite, nodev,   nulldev, 0 },
	{ nodev,   nodev,    nodev,   nodev,   nodev,   nulldev, 0 },
	{ nodev,   nodev,    nodev,   nodev,   nodev,   nulldev, 0 },
	{ lpopen,  lpclose,  nodev,   lpwrite, nodev,   nulldev, 0 },
	{ nodev,   nodev,    nodev,   nodev,   nodev,   nulldev, 0 },
	{ nodev,   nodev,    nodev,   nodev,   nodev,   nulldev, 0 },
	{ nodev,   nodev,    nodev,   nodev,   nodev,   nulldev, 0 },
	{ nodev,   nodev,    nodev,   nodev,   nodev,   nulldev, 0 },
	{ fdopen,  fdclose,  fdread,  fdwrite, fdioctl, nulldev, 0 },
	{ nodev,   nodev,    nodev,   nodev,   nodev,   nulldev, 0 },
	{ nodev,   nodev,    nodev,   nodev,   nodev,   nulldev, 0 },
	{ rkopen,  rkclose,  rkread,  rkwrite, nodev,   nulldev, 0 },
	{ 0,       0,        0,       0,       0,       0,       0 }
};

struct linesw linesw[] = {
	{ ttyopen, nulldev, ttread, ttwrite, nodev, ttyinput, ttkick }
};

int cf_x876[3] = { 0, 0, 0 };			/* 8876 */
dev_t rootdev = 0x0402;				/* 887C */
dev_t swapdev = 0x0402;				/* 887E */
dev_t pipedev = 0x0402;				/* 8880 */
int cf_x882 = 1;				/* 8882 */
daddr_t swplo = 250;				/* 8884 */
int nswap = 244;				/* 8888 */
int cf_x88a = 0;				/* 888A */
int *cf_nlist[22] = {				/* 888C */
	(int *)proc, (int *)file, (int *)ttyt, (int *)&cf_8c94, (int *)&cf_a620, (int *)buf, (int *)&cf_a628, (int *)&bfreelist,
	(int *)&swbuf1, (int *)&swbuf2, (int *)&runin, (int *)&runout, (int *)&u, (int *)&lbolt, (int *)&time, (int *)callout,
	(int *)&rrkbuf, (int *)&cf_a464, (int *)inode, (int *)&swapdev, (int *)&swplo, (int *)&cf_ceac
};
int cf_x8b8[6] = { 0, 0, 0, 0, 0, 0 };		/* 88B8 */
struct proc *cf_proctop = (struct proc *)0x8A8C;	/* 88C4 */

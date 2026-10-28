/* param.h -- Konstanten.  Belegt (am Kernel gemessen): NOFILE 20, BSIZE 512, NINODE 50, NFILE 50, NBUF 20, NMOUNT 5,
 * NPROC <= 36, Schlafwarteschlange 64 (slpque, Hash (chan>>5)&63), 50 Hz Uhr, PZERO 25, NZERO 20, DIRSIZ 14,
 * ENFILE 23 / EMFILE 24 / EBADF 9 / EPERM 1.  Uebrige Zahlen aus V7/WEGA uebernommen (geraten, bei Gebrauch pruefen). */
#ifndef PARAM_H
#define PARAM_H
#include "types.h"
#define NBUF	20
#define NINODE	50
#define NFILE	50
#define NMOUNT	5
#define NOFILE	20
#define NSIG	17		/* psignal: sig < 17; u_signal[17] */
#define NCALL	9		/* aus callout CE58..CE8E (54 Byte, 6 Byte je Eintrag) gefolgert */
#define HZ	50
#define MAXMEM	64
#define NPROC	30		/* proc[] A11C..A464: 0x348 / 0x1C (K7c: fork) */
#define MAXUPRC	15		/* K7c: fork vergleicht a mit 15 */
#define SSIZE	5		/* geraten */
#define CMAPSIZ	50		/* geraten */
#define SMAPSIZ	50		/* geraten */
#define NCARGS	5120		/* geraten */
#define DIRSIZ	14
#define NICINOD	100
#define NICFREE	50
#define BSIZE	512
#define BMASK	0777
#define BSHIFT	9
#define NINDIR	(BSIZE/4)	/* daddr_t = 4 Byte im Speicher, 3 Byte auf Platte */
#define NMASK	0177
#define NSHIFT	7
#define ROOTINO	2
#define SUPERB	1
#define NODEV	(-1)
#define CMASK	0
#define PSWP	0
#define PINOD	10
#define PRIBIO	20
#define PZERO	25
#define NZERO	20
#define PPIPE	26
#define PWAIT	30
#define PSLEP	40
#define PUSER	50
#define ISIG	1		/* Kennzeichen fuer Signale in tty (geraten) */
#define SIGHUP	1
#define SIGINT	2
#define SIGQUIT	3
#define SIGILL	4
#define SIGTRAP	5
#define SIGIOT	6
#define SIGEMT	7
#define SIGFPE	8
#define SIGKILL	9
#define SIGBUS	10
#define SIGSEG	11
#define SIGSYS	12
#define SIGPIPE	13
#define SIGALRM	14
#define SIGTRM	15
#define SIGUSR1	16
#define SIGUSR2	17
#define major(x)	(int)(((unsigned)(x)>>8))
#define minor(x)	(int)((x)&0377)
#define makedev(x,y)	(dev_t)((x)<<8 | (y))
#define ctod(x)		(((x)+1)>>1)
#define itod(x)		(daddr_t)((((unsigned)(x)+15)>>3))
#define itoo(x)		(int)(((x)+15)&07)
#endif

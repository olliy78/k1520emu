/* systm.h -- globale Kernelvariablen (V7 systm.h).  Adressen im Kernel (Linkreihenfolge der Daten noch Aufgabe von K5):
 *   rootdev 887C swapdev 887E pipedev 8880 swplo 8884 (long) nswap 8888 | proctop 8A8C | Segment/Basis Nutzerzugriff 8DC0 |
 *   lbolt 8DCA time 8DCC (long) nblkdev 8DD0 runin 8DD6 runout 8DD8 curpri 8DDA runrun 8DC4 | slpque CEC6 (64 Zeiger).
 * Die Deklarationen sind hier nur Typangaben; im Modul c/<modul>.c steht sie als Variable je einmal (K5 verteilt sie). */
#ifndef SYSTM_H
#define SYSTM_H
#include "param.h"
extern dev_t		rootdev;
extern dev_t		swapdev;
extern dev_t		pipedev;
extern daddr_t		swplo;
extern int		nswap;
extern int		lbolt;
extern time_t		time;
extern int		nblkdev;
extern int		mpid;			/* 8DD4 */
extern int		nchrdev;		/* 8DD2 */
extern daddr_t		rablock;		/* 8DE4 */
extern char		runin;
extern char		runout;
extern char		runrun;
extern char		curpri;
extern struct inode	*rootdir;
extern struct proc	*proctop;
extern struct sysent {
	int	sy_narg;
	int	(*sy_call)();
} sysent[];
struct inode	*iget();
struct inode	*ialloc();
struct inode	*owner();
struct inode	*maknode();
struct inode	*namei();
struct buf	*bread();
struct buf	*breada();
struct buf	*getblk();
struct buf	*geteblk();
struct buf	*alloc();
struct filsys	*getfs();
struct file	*getf();
struct file	*falloc();
daddr_t		bmap();
int		uchar();
int		nodev();
int		nulldev();
#endif

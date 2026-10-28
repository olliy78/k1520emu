/* proc.h -- Prozesstabelle, 28 Byte je Eintrag ab A11C, Ende-Zeiger proctop = 8A8C.  Belegt (Lauf/Abzug): alles bis p_wchan,
 * p_link; p_textp (+16h) nach V7, p_clktim (+1Ah) nach V7/WEGA.  p_sig ist EIN Wort (+06), danach p_uid (+08) und p_pgrp (+0A)
 * -- die frueheren Tabellen (kernel_namen.md: long p_sig, p_uid +0A) lasen das anders. */
#ifndef PROC_H
#define PROC_H
#include "types.h"
struct proc {
	char		p_stat;		/* +00 */
	char		p_flag;		/* +01 */
	char		p_pri;		/* +02 */
	char		p_time;		/* +03 */
	char		p_cpu;		/* +04 */
	char		p_nice;		/* +05 */
	int		p_sig;		/* +06 Signalmaske, 16 Bit (psignal: sig < 17, 1<<(sig-1)) */
	int		p_uid;		/* +08 (newproc kopiert +08 und +0A vom Vater) */
	int		p_pgrp;		/* +0A [aus newproc/wait gefolgert; V7-Reihenfolge uid, pgrp] */
	int		p_pid;		/* +0C */
	int		p_ppid;		/* +0E */
	int		p_addr;		/* +10 (Zombie: Exit-Status) */
	int		p_size;		/* +12 (Zombie: xp_utime, long) */
	caddr_t		p_wchan;	/* +14 */
	struct text	*p_textp;	/* +16 */
	struct proc	*p_link;	/* +18 */
	int		p_clktim;	/* +1A */
};
extern struct proc	proc[];
extern struct proc	*runq;
#define SSLEEP	1
#define SWAIT	2
#define SRUN	3
#define SIDL	4
#define SZOMB	5
#define SSTOP	6
#define SLOAD	01
#define SSYS	02
#define SLOCK	04
#define SSWAP	010
#define STRC	020
#define SWTED	040
#define SULOCK	0100
extern struct proc	*slpque[];
#endif

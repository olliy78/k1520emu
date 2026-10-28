/* types.h -- Grundtypen des MUTOS-Kernels (nichtsegmentierter U8002-Kern, int = Zeiger = 16 Bit).
 * Fassung K7a: Namen nach V7/WEGA, Breiten aus den gemessenen Strukturen (c/h/README.md). */
#ifndef TYPES_H
#define TYPES_H
typedef	int		dev_t;
typedef	long		daddr_t;
typedef	char		*caddr_t;
typedef	unsigned	ino_t;
typedef	long		time_t;
typedef	long		off_t;
typedef	char		cnt_t;
typedef	int		label_t[9];	/* sichern/wiederherstellen: 9 Worte (u_segflg liegt bei u+0x12) */
#define NULL	0
#endif

/* buf.h -- Puffer, 24 Byte, 20 Stueck ab 8E9A, Daten A628+512*i.  Alle Felder ausser b_resid am Lauf belegt. */
#ifndef BUF_H
#define BUF_H
#include "types.h"
struct buf {
	int		b_flags;	/* +00 */
	struct buf	*b_forw;	/* +02 */
	struct buf	*b_back;	/* +04 */
	struct buf	*av_forw;	/* +06 */
	struct buf	*av_back;	/* +08 */
	dev_t		b_dev;		/* +0A */
	unsigned	b_bcount;	/* +0C */
	caddr_t		b_addr;		/* +0E */
	daddr_t		b_blkno;	/* +10 */
	char		b_error;	/* +14 */
	char		b_pad15;	/* +15 Fuellfeld */
	unsigned	b_resid;	/* +16 geraten (V7) */
};
extern struct buf	buf[NBUF];
extern struct buf	bfreelist;		/* A518 */
#define B_WRITE		0
#define B_READ		01
#define B_DONE		02
#define B_ERROR		04
#define B_BUSY		010
#define B_PHYS		020
#define B_MAP		040
#define B_WANTED	0100
#define B_AGE		0200
#define B_ASYNC		0400
#define B_DELWRI	01000
#endif

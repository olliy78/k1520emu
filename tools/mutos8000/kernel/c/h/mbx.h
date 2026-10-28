/* mbx.h -- Mailbox-Eintrag zur U880-Seite (mutos_io), 14 Byte; Entwurf 39 §11.  Belegt durch rkstart (6B60), ttxfer (633A), lpxfer (690A).
 * Die Mailbox der Platte liegt fest bei D532; je tty-Leitung gibt es einen Eintrag ab D26A (14 Byte je Leitung), je Drucker ab D516.
 * mb_stat dient bei tty/lp zugleich als "Leitung bereit"-Merker (Kernel setzt 1 nach dem Auftrag). */
#ifndef MBX_H
#define MBX_H
#include "types.h"
struct rkmbx {
	char		mb_cmd;		/* +00 Befehl (0 Platte, 2 Drucker, 4 Terminal) */
	char		mb_unit;	/* +01 Einheit / Nebengeraet */
	char		mb_dir;		/* +02 Richtung (0 lesen, 1 schreiben; Konsole 81H) */
	char		mb_stat;	/* +03 Status */
	int		mb_seg;		/* +04 Segment */
	char		*mb_addr;	/* +06 */
	unsigned	mb_cnt;		/* +08 */
	daddr_t		mb_blk;		/* +0A */
};
#endif

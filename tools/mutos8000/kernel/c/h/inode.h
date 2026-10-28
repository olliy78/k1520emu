/* inode.h -- Kerninode, 74 Byte, 50 Stueck ab 907A.  Belegt: i_flag, i_count, i_dev, i_number, i_uid (+0A), i_lastr (+46);
 * i_mode/i_nlink/i_gid/i_size/i_addr nach V7 und iexpand (Blockadresse 3 Byte -> long). */
#ifndef INODE_H
#define INODE_H
#include "types.h"
struct inode {
	char		i_flag;		/* +00 */
	cnt_t		i_count;	/* +01 */
	dev_t		i_dev;		/* +02 */
	ino_t		i_number;	/* +04 */
	unsigned	i_mode;		/* +06 */
	int		i_nlink;	/* +08 */
	int		i_uid;		/* +0A */
	int		i_gid;		/* +0C */
	off_t		i_size;		/* +0E */
	daddr_t		i_addr[13];	/* +12 .. +45 */
	daddr_t		i_lastr;	/* +46 */
};
extern struct inode	inode[];
#define ILOCK	01
#define IUPD	02
#define IACC	04
#define IMOUNT	010
#define IWANT	020
#define ITEXT	040
#define ICHG	0100
#define IFMT	0170000
#define IFDIR	0040000
#define IFCHR	0020000
#define IFBLK	0060000
#define IFREG	0100000
#define ISUID	04000
#define ISGID	02000
#define ISVTX	01000
#define IREAD	0400
#define IWRITE	0200
#define IEXEC	0100
#endif

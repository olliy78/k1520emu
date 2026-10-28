/* ino.h -- Platteninode (UNGEMESSEN, V7: 64 Byte, 8 je Block; Adressen 3 Byte).  Nur fuer iexpand/iupdat. */
#ifndef INO_H
#define INO_H
struct dinode {
	unsigned	di_mode;
	int		di_nlink;
	int		di_uid;
	int		di_gid;
	off_t		di_size;
	char		di_addr[40];
	time_t		di_atime;
	time_t		di_mtime;
	time_t		di_ctime;
};
#define INOPB	8
#endif

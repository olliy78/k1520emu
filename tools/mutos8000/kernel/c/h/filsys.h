/* filsys.h -- Superblock.  Belegt: s_ronly bei +19Dh (access), s_ninode/s_tfree usw. nach V7 (Zahl der Felder in
 * getfs/ialloc/ifree noch nicht einzeln nachgewiesen).  Aufbau V7 mit 3-Byte-Platte-Adressen im Speicher als long. */
#ifndef FILSYS_H
#define FILSYS_H
#include "param.h"
struct filsys {
	unsigned	s_isize;
	daddr_t		s_fsize;
	int		s_nfree;
	daddr_t		s_free[NICFREE];
	int		s_ninode;
	ino_t		s_inode[NICINOD];
	char		s_flock;
	char		s_ilock;
	char		s_fmod;
	char		s_ronly;
	time_t		s_time;
	daddr_t		s_tfree;
	ino_t		s_tinode;
	int		s_m;
	int		s_n;
	char		s_fname[6];
	char		s_fpack[6];
};
#endif

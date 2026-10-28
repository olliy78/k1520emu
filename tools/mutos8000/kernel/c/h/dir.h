/* dir.h -- Verzeichniseintrag (belegt: u_dent 16 Byte = ino 2 + name 14). */
#ifndef DIR_H
#define DIR_H
#include "param.h"
struct direct {
	ino_t	d_ino;
	char	d_name[DIRSIZ];
};
#endif

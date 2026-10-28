/* user_ext.h -- Felder der user-Struktur, die user.h (Fuellfeld u_x104) noch nicht benennt (AP K7b).
 * Als Zugriffe auf feste Abstaende von `u` (FC00) -- user.h wird hier nicht angefasst.  Nachfuehren, sobald user.h die Felder traegt.
 *   U_CMASK  u+156  umask (maknode: mode & ~umask; umask() 5132)
 *   U_SEP    u+113  Byte, getrennte I/D-Raeume (physio: ts = 0 wenn gesetzt)
 *   U_P104   u+104  Zeiger; physio liest das Wort bei +16 (untere Grenze des Stapelbereichs) */
#ifndef USER_EXT_H
#define USER_EXT_H
#define U_CMASK	(*(int *)((char *)&u + 0x156))
#define U_SEP	(*((char *)&u + 0x113))
#define U_P104	(*(char **)((char *)&u + 0x104))
#endif

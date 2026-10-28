/* sysent.c -- Systemaufruftabelle (Kernel mutos_1216, Daten 8B58..8C57, 64 Eintraege zu 4 Byte: Zahl der Argumente (ein Byte, dahinter
 * ein Fuellbyte), Funktion).  Nummern nach der WEGA-/System-III-Tabelle; Eintrag 0 ist nullsys (trap.c ruft bei callp == sysent
 * sigret selbst auf), 31/32/40/45/49..53/55..58/62/63 nosys (nicht belegt), 38/39 nullsys (Platzhalter). */
#include "param.h"
#include "systm.h"

int alarm(), chdir(), chmod(), chown(), chroot(), close(), creat(), dup(), exec(), exece(), fork(),
	fstat(), ftime(), getgid(), getpid(), getuid(), gtime(), ioctl(), kill(), link(), mknod(), nice(),
	nosys(), nullsys(), open(), pause(), pipe(), profil(), ptrace(), read(), rexit(), saccess(),
	sbreak(), seek(), setgid(), setuid(), smount(), ssig(), stat(), stime(), sumount(), sync(), times(),
	umask(), unlink(), utime(), wait(), write();

struct sysent sysent[64] = {
	{ 0, nullsys },		/* 0 */
	{ 1, rexit },		/* 1 */
	{ 0, fork },		/* 2 */
	{ 3, read },		/* 3 */
	{ 3, write },		/* 4 */
	{ 2, open },		/* 5 */
	{ 1, close },		/* 6 */
	{ 0, wait },		/* 7 */
	{ 2, creat },		/* 8 */
	{ 2, link },		/* 9 */
	{ 1, unlink },		/* 10 */
	{ 2, exec },		/* 11 */
	{ 1, chdir },		/* 12 */
	{ 0, gtime },		/* 13 */
	{ 3, mknod },		/* 14 */
	{ 2, chmod },		/* 15 */
	{ 3, chown },		/* 16 */
	{ 1, sbreak },		/* 17 */
	{ 2, stat },		/* 18 */
	{ 4, seek },		/* 19 */
	{ 0, getpid },		/* 20 */
	{ 3, smount },		/* 21 */
	{ 1, sumount },		/* 22 */
	{ 1, setuid },		/* 23 */
	{ 0, getuid },		/* 24 */
	{ 2, stime },		/* 25 */
	{ 4, ptrace },		/* 26 */
	{ 1, alarm },		/* 27 */
	{ 2, fstat },		/* 28 */
	{ 0, pause },		/* 29 */
	{ 2, utime },		/* 30 */
	{ 0, nosys },		/* 31 */
	{ 0, nosys },		/* 32 */
	{ 2, saccess },		/* 33 */
	{ 1, nice },		/* 34 */
	{ 1, ftime },		/* 35 */
	{ 0, sync },		/* 36 */
	{ 2, kill },		/* 37 */
	{ 0, nullsys },		/* 38 */
	{ 0, nullsys },		/* 39 */
	{ 1, nosys },		/* 40 */
	{ 2, dup },		/* 41 */
	{ 0, pipe },		/* 42 */
	{ 1, times },		/* 43 */
	{ 4, profil },		/* 44 */
	{ 0, nosys },		/* 45 */
	{ 1, setgid },		/* 46 */
	{ 0, getgid },		/* 47 */
	{ 2, ssig },		/* 48 */
	{ 0, nosys },		/* 49 */
	{ 0, nosys },		/* 50 */
	{ 0, nosys },		/* 51 */
	{ 0, nosys },		/* 52 */
	{ 0, nosys },		/* 53 */
	{ 3, ioctl },		/* 54 */
	{ 0, nosys },		/* 55 */
	{ 0, nosys },		/* 56 */
	{ 0, nosys },		/* 57 */
	{ 0, nosys },		/* 58 */
	{ 3, exece },		/* 59 */
	{ 1, umask },		/* 60 */
	{ 1, chroot },		/* 61 */
	{ 0, nosys },		/* 62 */
	{ 0, nosys },		/* 63 */
};

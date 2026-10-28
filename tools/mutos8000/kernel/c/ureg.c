/* ureg.c -- cinit (Zeichenlisten), sureg, estabur (Kernel mutos_1216, 3D04..3D6A; liegt zwischen pipe.c und sys3.c).
 * Daten: cfree = CF46 (50 Bloecke zu 16 Byte, BSS), cfreelist = D266, nchrdev = 8DD2, cdevsw = 8796.
 * estabur ist ohne Speicherverwaltungshardware nur noch eine Groessenpruefung (Summe <= 256 Segmente zu 256 Byte). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "conf.h"

#define NCLIST	50
struct cblock {
	struct cblock	*c_next;
	char		c_info[14];
};
struct cblock cfree[NCLIST];
struct cblock *cfreelist;

/* KERNEL 3D04 */
cinit()
{
	register ccp;
	register struct cblock *cp;
	register struct cdevsw *cdp;

	ccp = (int)cfree;
	ccp = (ccp+15) & ~15;
	for (cp = (struct cblock *)ccp; cp <= &cfree[NCLIST-1]; cp++) {
		cp->c_next = cfreelist;
		cfreelist = cp;
	}
	ccp = 0;
	for (cdp = cdevsw; cdp->d_open; cdp++)
		ccp++;
	nchrdev = ccp;
}

/* KERNEL 3D40 */
sureg()
{
}

/* KERNEL 3D42 */
estabur(nt, nd, ns, sep, xrw)
unsigned nt, nd, ns;
{
	if (nt + nd + ns > 256) {
		u.u_error = ENOMEM;
		return(-1);
	}
	return(0);
}

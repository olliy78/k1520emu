/* k7bv2.c -- Quellformen-Versuche K7b, Teil 2 (nur zum Ansehen der .s-Ausgabe). */
#include "param.h"
#include "systm.h"
#include "dir.h"
#include "user.h"
#include "inode.h"
#include "filsys.h"
#include "buf.h"

long vl;
struct filsys *vfp;

va1() { register i; vl = 2; for (vl = 2; vl < vfp->s_isize; vl++) i = 1; }
va2() { daddr_t adr; for (adr = SUPERB+1; adr < vfp->s_isize; adr++) bread(1, adr); }
va3() { daddr_t adr; for (adr = ROOTINO; adr < vfp->s_isize; adr++) bread(1, adr); }
va4() { daddr_t adr; for (adr = (daddr_t)2; adr < vfp->s_isize; adr++) bread(1, adr); }
va5() { daddr_t adr; adr = 2; while (adr < vfp->s_isize) { bread(1, adr); adr++; } }
va6() { daddr_t adr; for (adr = 2; adr < (daddr_t)vfp->s_isize; adr++) bread(1, adr); }
va7() { daddr_t adr; ino_t ino; ino = 1; for (adr = 2; adr < vfp->s_isize; adr++) { bread(1, adr); ino += 8; } }
vb1(ip, bn) register struct inode *ip; { register i; daddr_t bn2; i = 1; bn2 = ip->i_addr[i]; if (bn2 == 0) vl = 1; }
vb2(ip, bn) register struct inode *ip; { register i; daddr_t bn2; i = 1; if ((bn2 = ip->i_addr[i]) == 0) vl = 1; }
vb3(ip, bn) register struct inode *ip; { register i; daddr_t bn2; i = 1; bn2 = ip->i_addr[i]; if (!bn2) vl = 1; }
vc1(ip) register struct inode *ip; { if (getfs(ip->i_dev)->s_ronly != 0) return; vl = 1; }
vc2(ip) register struct inode *ip; { register char c; c = getfs(ip->i_dev)->s_ronly; if (c != 0) return; vl = 1; }
vc3(ip) register struct inode *ip; { if ((ip->i_flag & 1) == 0) return; if (getfs(ip->i_dev)->s_ronly != 0) return; vl = 1; }
vc4(ip) register struct inode *ip; { if ((ip->i_flag & 1) == 0) return; if (getfs(ip->i_dev)->s_ronly != 0) return; bread(1, 2L); }

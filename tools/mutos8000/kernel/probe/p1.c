int tab[4] = { 1, 2, 3, 4 };
int *ptab[2] = { tab, &tab[2] };
char str[] = "hallo";
char *msg = "welt";
char cb = 5;
long lv = 100000L;
int zero;
char zb[3];
struct s { int a; char b; long c; struct s *next; } sv, *sp;
int (*fp)();
int f1(a, b) int a, b;
{
	register int i, j;
	int k;
	k = a * b + a / b - a % b;
	k = (a << 3) + (b >> 2) + (a & b) + (a | b) + (a ^ b) + ~a + -b;
	for (i = 0; i < 10; i++) {
		if (a > b) k++; else k--;
		if (a == b || i != 3) k += 2;
		while (j) j--;
		do k--; while (k > 100);
	}
	switch (a) {
	case 1: k = 10; break;
	case 2: k = 20; break;
	case 3: k = 30; break;
	case 200: k = 40; break;
	default: k = 0;
	}
	return k ? i : j;
}
f2(p, q) char *p, *q;
{
	while (*p++ = *q++)
		;
	sp->a = 3;
	sp->b = 'x';
	sp->next->c = 7L;
	sv.a++;
	tab[a()] = 5;
	return (*fp)(1, p) + zero;
}
unsigned f3(u, v) unsigned u, v;
{
	char c;
	c = u;
	zb[1] = c;
	if (u < v) return u / v;
	return u >> 1;
}
long f4(x, y) long x, y;
{
	long z;
	z = x * y + x / y;
	z = x << 2;
	z = x >> 3;
	z += lv;
	if (x > y) z = -z;
	return z + (long)f3(1, 2) + (x & 0xffL);
}

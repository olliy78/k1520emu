int tab[4] = { 1, 2, 3, 4 };
sum(p, n) int *p; int n;
{
	int s;
	s = 0;
	while (n--)
		s += *p++;
	return s;
}

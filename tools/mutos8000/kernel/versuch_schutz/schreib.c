/* schreib: ueberschreibt alles zwischen Datenende und Stapel mit AA (Schutzversuch ohne MMU). */
extern char end;
main()
{
	register char *p;
	char *top;
	int x;
	top = (char *)&x - 256;
	printf("schreib pid=%d end=%x top=%x\n", getpid(), &end, top);
	for (p = &end; p < top; p++)
		*p = 0xAA;
	printf("schreib: %u Bytes ueberschrieben\n", (unsigned)(top - &end));
	exit(0);
}

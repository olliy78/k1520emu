/* wache: Muster in einen Puffer, alle 2 s pruefen (Schutzversuch ohne MMU, Entwurf 41). */
char buf[4096];
main(argc, argv) char **argv;
{
	register int i, n;
	int sp;
	for (i = 0; i < 4096; i++)
		buf[i] = 0x5A;
	printf("wache %s pid=%d buf=%x..%x sp=%x\n", argv[1], getpid(), buf, buf + 4095, &sp);
	for (n = 0; n < 20; n++) {
		sleep(2);
		for (i = 0; i < 4096; i++)
			if (buf[i] != 0x5A) {
				printf("wache %s: ZERSTOERT bei %x (Wert %x)\n", argv[1], &buf[i], buf[i] & 0377);
				exit(1);
			}
	}
	printf("wache %s: heil\n", argv[1]);
	exit(0);
}

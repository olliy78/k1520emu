#include <stdio.h>
int main(argc, argv) int argc; char **argv;
{
	int i;
	char buf[40];
	FILE *fp;
	static int cnt = 7;
	for (i = 0; i < argc; i++)
		printf("%s %d\n", argv[i], cnt++);
	fp = fopen("x", "r");
	if (fp == NULL) exit(1);
	sprintf(buf, "%x", i);
	return strlen(buf) + abs(-3) + (int)sizeof buf;
}

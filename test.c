#include <stdio.h>
#include <wchar.h>
#include "utf8.h"

int
main(void)
{
	int c, col, o, off;
	char s[] = "	浪漫的夢想";

	for (off = 0; off < 10; off++) {
		for (col = off + 1; col < 25; col++) {
			for (c = 0; c < col; c++)
				putchar(c % 8 == 0 ? '>' : '_');
			printf(" %d\n", col);
			for (o = 0; o < off; o++)
				putchar('.');
			printf("%.*s\n\n", utf8_col(s, col, off), s);
		}
	}
	return 0;
}

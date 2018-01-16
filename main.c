#include <stdio.h>
#include "utf8.h"

int
main(void)
{
	int n = 100;
	char s[] = "浪漫的夢想", *cut;

	if ((cut = utf8_col(s, n)) == NULL) {
		printf("the whole string fit\n");
		return 0;
	}
	printf("%zd\n", cut - s);
	*cut = '\0';
	printf("%s\n", s);
	return 0;
}

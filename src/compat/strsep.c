#include <string.h>

#include "compat.h"

char *
strsep(char **str_p, char const *sep)
{
	char *s, *prev;

	if (*str_p == NULL)
		return NULL;

	for (s = prev = *str_p; strchr(sep, *s) == NULL; s++)
		continue;

	if (*s == '\0') {
		*str_p = NULL;
	} else {
		*s = '\0';
		*str_p = s + 1;
	}
	return prev;
}

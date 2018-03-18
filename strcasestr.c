#include <ctype.h>
#include <stddef.h>

#include "util.h"

char *
strcasesstr(const char *str1, const char *str2)
{
	const char *s1;
	const char *s2;

	for (;;) {
		s1 = str1;
		s2 = str2;
		while (*s1 != '\0' && tolower(*s1) == tolower(*s2))
			s1++, s2++;
		if (*s2 == '\0')
			return (char *) str1;
		if (*s1 == '\0')
			return NULL;
		str1++;
	}

	return NULL;
}

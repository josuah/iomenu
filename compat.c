#include "compat.h"
#include <ctype.h>
#include <stddef.h>
#include <string.h>

char *
strcasestr(const char *str1, const char *str2)
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

size_t
strlcpy(char *buf, char const *str, size_t sz)
{
	size_t len, cpy;

	len = strlen(str);
	cpy = (len > sz) ? (sz) : (len);
	memcpy(buf, str, cpy + 1);
	buf[sz - 1] = '\0';
	return len;
}

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

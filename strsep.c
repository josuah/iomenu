#include <string.h>

char *
strsep(char **strp, const char *delim)
{
	char	*s, *oldp;

	if (*strp == NULL)
		return NULL;
	for (s = oldp = *strp; ; s++) {
		if (*s == '\0') {
			*strp = NULL;
			return oldp;
		} else if (strchr(delim, *s) != NULL) {
			break;
		}
	}
	*s = '\0';
	*strp = s + 1;
	return oldp;
}

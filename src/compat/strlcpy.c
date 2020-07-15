#include "compat.h"

#include <string.h>

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

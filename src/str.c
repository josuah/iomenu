#include "str.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

/*
 * It is heavy on assert as an inexpensive way to detect memory corruption from
 * code around: underflows are checked through the magic header, overflow are
 * checked through the '\0' byte.
 */

size_t
str_length(struct str *str)
{
	return mem_length(str->mem) - sizeof "";
}

int
str_init(struct str *str, struct mem_pool *pool)
{
	str->mem = mem_alloc(pool, 1);
	if (str->mem == NULL)
		return -1;

	assert(str->mem[0] == '\0');
	assert(str_length(str) == 0);
	assert(mem_length(str->mem) == 1);
	return 0;
}

void
str_truncate(struct str *str, size_t len)
{
	assert(str->mem[str_length(str)] == '\0');

	if (mem_length(str->mem) > len)
		str->mem[len] = '\0';
	mem_resize((void **)&str->mem, len + 1);

	assert(str->mem[str_length(str)] == '\0');
}

int
str_append_mem(struct str *str, char const *buf, size_t len)
{
	void **pp = (void **)&str->mem;
	size_t old_sz;

	assert((old_sz = mem_length(str->mem)) > 0);
	assert(memchr(buf, '\0', len) == NULL);
	assert(str->mem[str_length(str)] == '\0');

	if (mem_shrink(pp, 1)) /* strip '\0' */
		return -1;
	if (mem_append(pp, buf, len) < 0)
		return -1;
	if (mem_append(pp, "", 1) < 0)
		return -1;

	assert(memcmp(str->mem + old_sz-1, buf, len) == 0);
	assert(str->mem[old_sz-1 + len] == '\0');
	assert(str->mem[str_length(str)] == '\0');
	return 0;
}

int
str_append_string(struct str *str, char const *s)
{
	assert(str->mem[str_length(str)] == '\0');

	return str_append_mem(str, s, strlen(s));
}

int
str_append_char(struct str *str, char c)
{
	assert(str->mem[str_length(str)] == '\0');

	str_c(str)[str_length(str)] = c;
	if (mem_append((void **)str->mem, "", 1) < 0)
		return -1;

	assert(str->mem[str_length(str)] == '\0');
	return 0;
}

int
str_append_fmt(struct str *str, char const *fmt, ...)
{
	va_list va;
	int n;

	assert(str->mem[str_length(str)] == '\0');

	va_start(va, fmt);
	n = vsnprintf(NULL, 0, fmt, va);
	assert(n > 0);
	if (mem_grow((void **)&str->mem, n + 1) < 0)
		return -1;
	vsprintf(str->mem, fmt, va);
	va_end(va);

	assert(str->mem[str_length(str)] == '\0');
	return 0;
}

char *
str_c(struct str *str)
{
	assert(str->mem[str_length(str)] == '\0');
	return str->mem;
}

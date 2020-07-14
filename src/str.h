#ifndef STR_H
#define STR_H

#include <stddef.h>

#include "mem.h"

/*
 * Length kept by struct mem_block.
 */

struct str {
	char *mem;
};

/** src/str.c **/
size_t str_length(struct str *str);
int str_init(struct str *str, struct mem_pool *pool);
void str_truncate(struct str *str, size_t len);
int str_append_mem(struct str *str, char const *buf, size_t len);
int str_append_string(struct str *str, char const *s);
int str_append_char(struct str *str, char c);
int str_append_fmt(struct str *str, char const *fmt, ...);
char * str_c(struct str *str);

#endif

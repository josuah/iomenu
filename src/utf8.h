#ifndef UTF8_H
#define UTF8_H

#include <stddef.h>
#include <stdint.h>

enum {
	UTF8_ACCEPT,
	UTF8_REJECT,
};

/** src/utf8.c **/
size_t utf8_encode(char *dest, uint32_t u);
uint32_t utf8_decode(uint32_t *state, uint32_t *codep, uint32_t byte);

#endif

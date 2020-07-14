/*
 * ASCII all have a leading '0' byte:
 *
 *   0xxxxxxx
 *
 * UTF-8 have one leading '1' and as many following '1' as there are
 * continuation bytes (with leading '1' and '0').
 *
 *   0xxxxxxx
 *   110xxxxx 10xxxxxx
 *   1110xxxx 10xxxxxx 10xxxxxx
 *   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 *   111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 *   1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 *
 * There is up to 3 continuation bytes -- up to 4 bytes per runes.
 */

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utf8.h"

static int
utflen(char const *str)
{
	int i, len;
	unsigned char const *s;

	s = (unsigned char const *)str;
	len =	(*s < 0x80) ? 1 : /* 0xxxxxxx < *s < 10000000 */
		(*s < 0xc0) ? 0 : /* 10xxxxxx < *s < 11000000 */
		(*s < 0xe0) ? 2 : /* 110xxxxx < *s < 11100000 */
		(*s < 0xf0) ? 3 : /* 1110xxxx < *s < 11110000 */
		(*s < 0xf8) ? 4 : /* 11110xxx < *s < 11111000 */
		(*s < 0xfc) ? 5 : /* 111110xx < *s < 11111100 */
		(*s < 0xfe) ? 6 : /* 1111110x < *s < 11111110 */
		(*s < 0xff) ? 7 : /* 11111110 < *s < 11111111 */
		0;

	/* check continuation bytes and '\0' */
	for (s++, i = 1; i < len; i++, s++) {
		if ((*s & 0xc0) != 0x80) /* 10xxxxxx & 11000000 */
			return 0;
	}

	return len;
}

static long
torune(char const **str, size_t len)
{
	long rune;
	int n;
	char mask[] = { 0x00, 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };

	if (len == 0 || len > 6)
		return 0;

	/* first byte */
	rune = *(*str)++ & mask[len];

	/* continuation bytes */
	for (n = len - 1; n > 0; n--, (*str)++)
		rune = (rune << 6) | (**str & 0x3f);	/* 10xxxxxx */

	return rune;
}

/*
 * Return the number of bytes required to encode <rune> into UTF-8, or
 * 0 if rune is too long.
 */
int
utf8_runelen(long rune)
{
	return	(rune <= 0x0000007f) ? 1 :
		(rune <= 0x000007ff) ? 2 :
		(rune <= 0x0000ffff) ? 3 :
		(rune <= 0x001fffff) ? 4 :
		(rune <= 0x03ffffff) ? 5 :
		(rune <= 0x7fffffff) ? 6 :
		0;
}

/*
 * Return the number of bytes in rune for the next char in <s>, or 0 if
 * is misencoded or if it is '\0'.
 */
int
utf8_utflen(char const *str)
{
	long rune;
	int len = utflen(str);

	rune = torune(&str, len);
	if (len != utf8_runelen(rune))
		return 0;

	return len;
}

/*
 * Return a rune corresponding to the firsts bytes of <str> or -1 if
 * the rune is invalid, and set <str> to the beginning of the next rune.
 */
long
utf8_torune(char const **str)
{
	long rune;
	int len;

	len = utflen(*str);
	rune = torune(str, len);
	if (len != utf8_runelen(rune))
		return -1;

	return rune;
}

/*
 * Encode the rune <rune> in UTF-8 in <str>, null-terminated, then return the
 * number of bytes written, 0 if <rune> is invalid.
 *
 * Thanks to Connor Lane Smith for the idea of combining switches and
 * binary masks.
 */
int
utf8_tostr(char *str, long rune)
{
	switch (utf8_runelen(rune)) {
	case 1:
		str[0] = rune;				/* 0xxxxxxx */
		str[1] = '\0';
		return 1;
	case 2:
		str[0] = 0xc0 | (0x1f & (rune >> 6));	/* 110xxxxx */
		str[1] = 0x80 | (0x3f & (rune));	/* 10xxxxxx */
		str[2] = '\0';
		return 2;
	case 3:
		str[0] = 0xe0 | (0x0f & (rune >> 12));	/* 1110xxxx */
		str[1] = 0x80 | (0x3f & (rune >> 6));	/* 10xxxxxx */
		str[2] = 0x80 | (0x3f & (rune));	/* 10xxxxxx */
		str[3] = '\0';
		return 3;
	case 4:
		str[0] = 0xf0 | (0x07 & (rune >> 18));	/* 11110xxx */
		str[1] = 0x80 | (0x3f & (rune >> 12));	/* 10xxxxxx */
		str[2] = 0x80 | (0x3f & (rune >> 6));	/* 10xxxxxx */
		str[3] = 0x80 | (0x3f & (rune));	/* 10xxxxxx */
		str[4] = '\0';
		return 4;
	case 5:
		str[0] = 0xf8 | (0x03 & (rune >> 24));	/* 111110xx */
		str[1] = 0x80 | (0x3f & (rune >> 18));	/* 10xxxxxx */
		str[2] = 0x80 | (0x3f & (rune >> 12));	/* 10xxxxxx */
		str[3] = 0x80 | (0x3f & (rune >> 6));	/* 10xxxxxx */
		str[4] = 0x80 | (0x3f & (rune));	/* 10xxxxxx */
		str[5] = '\0';
		return 5;
	case 6:
		str[0] = 0xfc | (0x01 & (rune >> 30));	/* 1111110x */
		str[1] = 0x80 | (0x3f & (rune >> 24));	/* 10xxxxxx */
		str[2] = 0x80 | (0x3f & (rune >> 18));	/* 10xxxxxx */
		str[3] = 0x80 | (0x3f & (rune >> 12));	/* 10xxxxxx */
		str[4] = 0x80 | (0x3f & (rune >> 6));	/* 10xxxxxx */
		str[5] = 0x80 | (0x3f & (rune));	/* 10xxxxxx */
		str[6] = '\0';
		return 6;
	default:
		str[0] = '\0';
		return 0;
	}
}

/*
 * Return 1 if the rune is a printable character, and 0 otherwise.
 */
int
utf8_isprint(long rune)
{
	return (0x1f < rune && rune != 0x7f && rune < 0x80) || 0x9f < rune;
}

/*
 * Return a pointer to the next rune in <str> or next byte if the rune
 * is invalid.
 */
char const *
utf8_nextrune(char const *str)
{
	int len;

	len = utf8_utflen(str);
	return (len == 0) ? str + 1 : str + len;
}

/*
 * Return a pointer to the prev rune in <str> or prev byte if the rune
 * is invalid.
 */
char const *
utf8_prevrune(char const *str)
{
	char const *s;

	for (s = str; (*s & 0x80) != 0; s--)
		;
	return (utf8_utflen(s) != 0) ? s : str - 1;
}

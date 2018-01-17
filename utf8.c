/*
 * ASCII all have a leading '0' byte:
 *
 *   0xxxxxxx
 *
 * UTF-8(7) have one leading '1' and as many following '1' as there are
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
 *
 * The whole character value is retreived into an 'x' and stored into a
 * (long)[].
 *
 * Thanks to Connor Lane Smith for the idea of combining switches and
 * binary masks.
 */

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "wcwidth.h"
#include "utf8.h"

/*
 * Return the number of bytes in rune for the `n` next char in `s`,
 * or 0 if is misencoded or if it is '\0'.
 */
size_t
utf8_len(char *s)
{
	unsigned char *sp = (unsigned char *) s;
	int i, len;

	len =	(*sp == 0x0) ? 0 :  /* 00000000 */
		(*sp < 0x80) ? 1 :  /* 0xxxxxxx < 10000000 */
		(*sp < 0xc0) ? 0 :  /* 10xxxxxx < 11000000 */
		(*sp < 0xe0) ? 2 :  /* 110xxxxx < 11100000 */
		(*sp < 0xf0) ? 3 :  /* 1110xxxx < 11110000 */
		(*sp < 0xf8) ? 4 :  /* 11110xxx < 11111000 */
		(*sp < 0xfc) ? 5 :  /* 111110xx < 11111100 */
		(*sp < 0xfe) ? 6 :  /* 1111110x < 11111110 */
		(*sp < 0xff) ? 7 :  /* 11111110 < 11111111 */
	                     0;

	/* check continuation bytes and '\0' */
	for (sp++, i = 1; i < len; i++, sp++) {
		if ((*sp & 0xc0) != 0x80)  /* 10xxxxxx & 11000000 */
			return 0;
	}

	return len;
}

/*
 * Return the number of bytes required to encode `rune` into UTF-8, or
 * 0 if rune is too long.
 */
size_t
utf8_runelen(long rune)
{
	return	(rune <= 0x0000007f) ? 1 : (rune <= 0x000007ff) ? 2 :
		(rune <= 0x0000ffff) ? 3 : (rune <= 0x001fffff) ? 4 :
		(rune <= 0x03ffffff) ? 5 : (rune <= 0x7fffffff) ? 6 : 0;
}

/*
 * Sets `rune' to a rune corresponding to the firsts `n' bytes of `s'.
 *
 * Return the number of bytes read or 0 if the string is misencoded.
 */
size_t
utf8_torune(long *rune, char *s)
{
	char mask[] = { 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };
	size_t i, len = utf8_len(s);

	if (len == 0 || len > 6 || (size_t) len > strlen(s))
		return 0;

	/* first byte */
	*rune = *s++ & mask[len - 1];

	/* continuation bytes */
	for (i = 1; i < len; i++)
		*rune = (*rune << 6) | (*s++ & 0x3f);  /* 10xxxxxx */

	/* overlong sequences */
	if (utf8_runelen(*rune) != len)
		return 0;

	return len;
}

/*
 * Encode the rune `rune' in utf-8 in `s', null-terminated, then return the
 * number of bytes written, 0 if `rune' is invalid.
 */
int
utf8_tostr(char *s, long rune)
{
	switch (utf8_runelen(rune)) {
	case 1:
		s[0] = rune;				/* 0xxxxxxx */
		s[1] = '\0';
		return 1;
	case 2:
		s[0] = 0xc0 | (0x1f & (rune >> 6));	/* 110xxxxx */
		s[1] = 0x80 | (0x3f & (rune));		/* 10xxxxxx */
		s[2] = '\0';
		return 2;
	case 3:
		s[0] = 0xe0 | (0x0f & (rune >> 12));	/* 1110xxxx */
		s[1] = 0x80 | (0x3f & (rune >> 6));	/* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (rune));		/* 10xxxxxx */
		s[3] = '\0';
		return 3;
	case 4:
		s[0] = 0xf0 | (0x07 & (rune >> 18));	/* 11110xxx */
		s[1] = 0x80 | (0x3f & (rune >> 12));	/* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (rune >> 6));	/* 10xxxxxx */
		s[3] = 0x80 | (0x3f & (rune));		/* 10xxxxxx */
		s[4] = '\0';
		return 4;
	case 5:
		s[0] = 0xf8 | (0x03 & (rune >> 24));	/* 111110xx */
		s[1] = 0x80 | (0x3f & (rune >> 18));	/* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (rune >> 12));	/* 10xxxxxx */
		s[3] = 0x80 | (0x3f & (rune >> 6));	/* 10xxxxxx */
		s[4] = 0x80 | (0x3f & (rune));		/* 10xxxxxx */
		s[5] = '\0';
		return 5;
	case 6:
		s[0] = 0xfc | (0x01 & (rune >> 30));	/* 1111110x */
		s[1] = 0x80 | (0x3f & (rune >> 24));	/* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (rune >> 18));	/* 10xxxxxx */
		s[3] = 0x80 | (0x3f & (rune >> 12));	/* 10xxxxxx */
		s[4] = 0x80 | (0x3f & (rune >> 6));	/* 10xxxxxx */
		s[5] = 0x80 | (0x3f & (rune));		/* 10xxxxxx */
		s[6] = '\0';
		return 6;
	default:
		s[0] = '\0';
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
 * Return a index of the first byte of a character of `s' that would be rendered
 * at the `col'-th column in a terminal, or NULL if the whole string fit.  In
 * order to format tabs properly, the string must start with an offset of `off'
 * columns.
 */
int
utf8_col(char *str, int col, int off)
{
	long rune;
	char *pos, *s;

	for (s = str; off < col;) {
		pos = s;
		if (*s == '\0')
			break;

		s += utf8_torune(&rune, s);
		if (rune == '\t')
			off += 7 - (off) % 8;
		else
			off += mk_wcwidth(rune);
	}

	return pos - str;
}

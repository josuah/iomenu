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
			off += utf8_wcwidth(rune);
	}

	return pos - str;
}

/*
 * Markus Kuhn -- 2007-05-26 (Unicode 5.0)
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose and without fee is hereby granted. The author
 * disclaims all warranties with regard to this software.
 *
 * Latest version: http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 */

struct interval {
	int first;
	int last;
};

/*
 * auxiliary function for binary search in interval table
 */
static int
bisearch(long ucs, const struct interval *table, int max)
{
	int min = 0;
	int mid;

	if (ucs < table[0].first || ucs > table[max].last)
		return 0;
	while (max >= min) {
		mid = (min + max) / 2;
		if (ucs > table[mid].last)
			min = mid + 1;
		else if (ucs < table[mid].first)
			max = mid - 1;
		else
			return 1;
	}

	return 0;
}

/* The following two functions define the column width of an ISO 10646
 * character as follows:
 *
 *    - The null character (U+0000) has a column width of 0.
 *
 *    - Other C0/C1 control characters and DEL will lead to a return
 *      value of -1.
 *
 *    - Non-spacing and enclosing combining characters (general
 *      category code Mn or Me in the Unicode database) have a
 *      column width of 0.
 *
 *    - SOFT HYPHEN (U+00AD) has a column width of 1.
 *
 *    - Other format characters (general category code Cf in the Unicode
 *      database) and ZERO WIDTH SPACE (U+200B) have a column width of 0.
 *
 *    - Hangul Jamo medial vowels and final consonants (U+1160-U+11FF)
 *      have a column width of 0.
 *
 *    - Spacing characters in the East Asian Wide (W) or East Asian
 *      Full-width (F) category as defined in Unicode Technical
 *      Report #11 have a column width of 2.
 *
 *    - All remaining characters (including all printable
 *      ISO 8859-1 and WGL4 characters, Unicode control characters,
 *      etc.) have a column width of 1.
 *
 * This implementation was assuming that wchar_t characters are encoded
 * in ISO 10646, but wchar_t have been replaced by long.
 */
int
utf8_wcwidth(long ucs)
{
	/* sorted list of non-overlapping intervals of non-spacing characters */
	/* generated by "uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B c" */
	static const struct interval combining[] = {
		{ 0x0300, 0x036F }, { 0x0483, 0x0486 }, { 0x0488, 0x0489 },
		{ 0x0591, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
		{ 0x05C4, 0x05C5 }, { 0x05C7, 0x05C7 }, { 0x0600, 0x0603 },
		{ 0x0610, 0x0615 }, { 0x064B, 0x065E }, { 0x0670, 0x0670 },
		{ 0x06D6, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
		{ 0x070F, 0x070F }, { 0x0711, 0x0711 }, { 0x0730, 0x074A },
		{ 0x07A6, 0x07B0 }, { 0x07EB, 0x07F3 }, { 0x0901, 0x0902 },
		{ 0x093C, 0x093C }, { 0x0941, 0x0948 }, { 0x094D, 0x094D },
		{ 0x0951, 0x0954 }, { 0x0962, 0x0963 }, { 0x0981, 0x0981 },
		{ 0x09BC, 0x09BC }, { 0x09C1, 0x09C4 }, { 0x09CD, 0x09CD },
		{ 0x09E2, 0x09E3 }, { 0x0A01, 0x0A02 }, { 0x0A3C, 0x0A3C },
		{ 0x0A41, 0x0A42 }, { 0x0A47, 0x0A48 }, { 0x0A4B, 0x0A4D },
		{ 0x0A70, 0x0A71 }, { 0x0A81, 0x0A82 }, { 0x0ABC, 0x0ABC },
		{ 0x0AC1, 0x0AC5 }, { 0x0AC7, 0x0AC8 }, { 0x0ACD, 0x0ACD },
		{ 0x0AE2, 0x0AE3 }, { 0x0B01, 0x0B01 }, { 0x0B3C, 0x0B3C },
		{ 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 }, { 0x0B4D, 0x0B4D },
		{ 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 }, { 0x0BC0, 0x0BC0 },
		{ 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 }, { 0x0C46, 0x0C48 },
		{ 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 }, { 0x0CBC, 0x0CBC },
		{ 0x0CBF, 0x0CBF }, { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD },
		{ 0x0CE2, 0x0CE3 }, { 0x0D41, 0x0D43 }, { 0x0D4D, 0x0D4D },
		{ 0x0DCA, 0x0DCA }, { 0x0DD2, 0x0DD4 }, { 0x0DD6, 0x0DD6 },
		{ 0x0E31, 0x0E31 }, { 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E },
		{ 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EB9 }, { 0x0EBB, 0x0EBC },
		{ 0x0EC8, 0x0ECD }, { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 },
		{ 0x0F37, 0x0F37 }, { 0x0F39, 0x0F39 }, { 0x0F71, 0x0F7E },
		{ 0x0F80, 0x0F84 }, { 0x0F86, 0x0F87 }, { 0x0F90, 0x0F97 },
		{ 0x0F99, 0x0FBC }, { 0x0FC6, 0x0FC6 }, { 0x102D, 0x1030 },
		{ 0x1032, 0x1032 }, { 0x1036, 0x1037 }, { 0x1039, 0x1039 },
		{ 0x1058, 0x1059 }, { 0x1160, 0x11FF }, { 0x135F, 0x135F },
		{ 0x1712, 0x1714 }, { 0x1732, 0x1734 }, { 0x1752, 0x1753 },
		{ 0x1772, 0x1773 }, { 0x17B4, 0x17B5 }, { 0x17B7, 0x17BD },
		{ 0x17C6, 0x17C6 }, { 0x17C9, 0x17D3 }, { 0x17DD, 0x17DD },
		{ 0x180B, 0x180D }, { 0x18A9, 0x18A9 }, { 0x1920, 0x1922 },
		{ 0x1927, 0x1928 }, { 0x1932, 0x1932 }, { 0x1939, 0x193B },
		{ 0x1A17, 0x1A18 }, { 0x1B00, 0x1B03 }, { 0x1B34, 0x1B34 },
		{ 0x1B36, 0x1B3A }, { 0x1B3C, 0x1B3C }, { 0x1B42, 0x1B42 },
		{ 0x1B6B, 0x1B73 }, { 0x1DC0, 0x1DCA }, { 0x1DFE, 0x1DFF },
		{ 0x200B, 0x200F }, { 0x202A, 0x202E }, { 0x2060, 0x2063 },
		{ 0x206A, 0x206F }, { 0x20D0, 0x20EF }, { 0x302A, 0x302F },
		{ 0x3099, 0x309A }, { 0xA806, 0xA806 }, { 0xA80B, 0xA80B },
		{ 0xA825, 0xA826 }, { 0xFB1E, 0xFB1E }, { 0xFE00, 0xFE0F },
		{ 0xFE20, 0xFE23 }, { 0xFEFF, 0xFEFF }, { 0xFFF9, 0xFFFB },
		{ 0x10A01, 0x10A03 }, { 0x10A05, 0x10A06 }, { 0x10A0C, 0x10A0F },
		{ 0x10A38, 0x10A3A }, { 0x10A3F, 0x10A3F }, { 0x1D167, 0x1D169 },
		{ 0x1D173, 0x1D182 }, { 0x1D185, 0x1D18B }, { 0x1D1AA, 0x1D1AD },
		{ 0x1D242, 0x1D244 }, { 0xE0001, 0xE0001 }, { 0xE0020, 0xE007F },
		{ 0xE0100, 0xE01EF }
	};

	/* test for 8-bit control characters */
	if (ucs == 0)
		return 0;
	if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))
		return -1;

	/* binary search in table of non-spacing characters */
	if (bisearch(ucs, combining, sizeof(combining) /
	    sizeof(struct interval) - 1))
		return 0;

	/* if we arrive here, ucs is not a combining or C0/C1 control character */

	return 1 + (ucs >= 0x1100 &&
	    (ucs <= 0x115f ||			/* Hangul Jamo init. consonants */
	    ucs == 0x2329 || ucs == 0x232a ||
	    (ucs >= 0x2e80 && ucs <= 0xa4cf &&
	    ucs != 0x303f) ||			/* CJK ... Yi */
	    (ucs >= 0xac00 && ucs <= 0xd7a3) ||	/* Hangul Syllables */
	    (ucs >= 0xf900 && ucs <= 0xfaff) ||	/* CJK Compatibility Ideographs */
	    (ucs >= 0xfe10 && ucs <= 0xfe19) ||	/* Vertical forms */
	    (ucs >= 0xfe30 && ucs <= 0xfe6f) ||	/* CJK Compatibility Forms */
	    (ucs >= 0xff00 && ucs <= 0xff60) ||	/* Fullwidth Forms */
	    (ucs >= 0xffe0 && ucs <= 0xffe6) ||
	    (ucs >= 0x20000 && ucs <= 0x2fffd) ||
	    (ucs >= 0x30000 && ucs <= 0x3fffd)));
}

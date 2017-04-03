/*
 * Functions handling UTF-8 strings:
 *
 * stdin  -> buffer -> stdout
 * UTF-8  ->  rune  -> UTF-8
 * char[] -> long[] -> char[]
 *
 * Thanks to Connor Lane Smith for the idea of combining switches and
 * binary masks.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "utf.h"


/* --- lengths -------------------------------------------------------------- */


/*
 * Return the number of bytes in rune for the `n` next char in `s`,
 * or 0 if ti is misencoded.
 */
int
utflen(char *s, int n)
{
	int len = 1;
	int continuation_bytes =
		(s[0] & 0x80) == 0x00 ? 0 :  /* 0xxxxxxx */
		(s[0] & 0xc0) == 0x80 ? 1 :  /* 10xxxxxx */
		(s[0] & 0xe0) == 0xc0 ? 2 :  /* 110xxxxx */
		(s[0] & 0xf0) == 0xe0 ? 3 :  /* 1110xxxx */
		(s[0] & 0xf8) == 0xf0 ? 4 :  /* 11110xxx */
		(s[0] & 0xfc) == 0xf8 ? 5 :  /* 111110xx */
		(s[0] & 0xfe) == 0xfc ? 6 :  /* 1111110x */
		(s[0] & 0xff) == 0xfe ? 7 :  /* 11111110 */
		                        8;   /* 11111111 */

	if (continuation_bytes > 6 || continuation_bytes > n)
		return 0;

	/* check if continuation bytes are 10xxxxxx and increment `len` */
	switch (continuation_bytes) {  /* FALLTHROUGH */
	case 7:	if ((s[6] & 0xc0) != 0x80) return 0; else len++;
	case 6:	if ((s[5] & 0xc0) != 0x80) return 0; else len++;
	case 5:	if ((s[4] & 0xc0) != 0x80) return 0; else len++;
	case 4:	if ((s[3] & 0xc0) != 0x80) return 0; else len++;
	case 3:	if ((s[2] & 0xc0) != 0x80) return 0; else len++;
	case 2:	if ((s[1] & 0xc0) != 0x80) return 0; else len++;
	case 0:	return len;
	default: return 0;
	}
}


/*
 * Return the number of bytes required to encode `rune` into UTF-8, or
 * 0 if rune is too long.
 */
int
runelen(long r)
{
	if (r <= 0x0000007f) return 1;
	if (r <= 0x000007ff) return 2;
	if (r <= 0x0000ffff) return 3;
	if (r <= 0x001fffff) return 4;
	if (r <= 0x03ffffff) return 5;
	if (r <= 0x7fffffff) return 6;
	return 0;
}


/* --- conversions ---------------------------------------------------------- */


/*
 * Sets `r` to a rune corresponding to the firsts `n` bytes of `s`.
 * If `s` is misencoded, the rune is stored as a negative value.
 *
 * Return the number of bytes read.
 */
int
utftorune(long *r, char *s, int n)
{
	int len = utflen(s, n), i;

	/* first byte */
	switch (len) {
	case 1: *r = s[0]; return 1;      /* 0xxxxxxx */
	case 2: *r = s[0] & 0x1f; break;  /* 110xxxxx */
	case 3: *r = s[0] & 0x0f; break;  /* 1110xxxx */
	case 4: *r = s[0] & 0x07; break;  /* 11110xxx */
	case 5: *r = s[0] & 0x03; break;  /* 111110xx */
	case 6: *r = s[0] & 0x01; break;  /* 1111110x */
	default: *r = -(unsigned char) s[0]; return 1;  /* misencoded */
	}

	/* continuation bytes */
	for (i = 1; i < len; i++)
		*r = (*r << 6) | (s[i] & 0x3f);  /* 10xxxxxx */

	/* overlong sequences */
	if (runelen(*r) != len) {
		*r = -(unsigned char) s[0];
		return 1;
	}

	return len;
}


/*
 * Convert the utf char sring `src` of size `n` to a long string
 * `dest`.
 *
 * Return the length of `i`.
 */
size_t
utftorunes(long *runev, char *utf, size_t n)
{
	size_t i, j;

	for (i = 0, j = 0; n > 0; i++)
		j += utftorune(runev + i, utf[j], n - j);

	runev[i] = '\0';
	return i;
}


/*
 * Encode the rune `r` in utf-8 in `s`, null-terminated.
 *
 * Return the number of bytes written, 0 if `r` is invalid.
 */
int
runetoutf(char *s, long r)
{
	switch (runelen(r)) {
	case 1:
		s[0] = r;                          /* 0xxxxxxx */
		s[1] = '\0';
		return 1;
	case 2:
		s[0] = 0xc0 | (0x1f & (r >> 6));   /* 110xxxxx */
		s[1] = 0x80 | (0x3f & (r));        /* 10xxxxxx */
		s[2] = '\0';
		return 2;
	case 3:
		s[0] = 0xe0 | (0x0f & (r >> 12));  /* 1110xxxx */
		s[1] = 0x80 | (0x3f & (r >> 6));   /* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (r));        /* 10xxxxxx */
		s[3] = '\0';
		return 3;
	case 4:
		s[0] = 0xf0 | (0x07 & (r >> 18));  /* 11110xxx */
		s[1] = 0x80 | (0x3f & (r >> 12));  /* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (r >> 6));   /* 10xxxxxx */
		s[3] = 0x80 | (0x3f & (r));        /* 10xxxxxx */
		s[4] = '\0';
		return 4;
	case 5:
		s[0] = 0xf8 | (0x03 & (r >> 24));  /* 111110xx */
		s[1] = 0x80 | (0x3f & (r >> 18));  /* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (r >> 12));  /* 10xxxxxx */
		s[3] = 0x80 | (0x3f & (r >> 6));   /* 10xxxxxx */
		s[4] = 0x80 | (0x3f & (r));        /* 10xxxxxx */
		s[5] = '\0';
		return 5;
	case 6:
		s[0] = 0xfc | (0x01 & (r >> 30));  /* 1111110x */
		s[1] = 0x80 | (0x3f & (r >> 24));  /* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (r >> 18));  /* 10xxxxxx */
		s[3] = 0x80 | (0x3f & (r >> 12));  /* 10xxxxxx */
		s[4] = 0x80 | (0x3f & (r >> 6));   /* 10xxxxxx */
		s[5] = 0x80 | (0x3f & (r));        /* 10xxxxxx */
		s[6] = '\0';
		return 6;
	default:
		s[0] = '\0';
		return 0;
	}
}


/*
 * Fill `s` with a printable representation of `r`.
 *
 * Return the width of the character.
 */
int
runetoprint(char *s, long r)
{
	if (r < 0) {
		return sprintf(s, "[%02x]", (unsigned char) -r);

	} else if (r == 0x7f || r < ' ') {
		return sprintf(s, "[%02lx]", r);

	} else if (!isprintrune(r)) {
		return sprintf(s, "[%04lx]", r);

	} else {
		runetoutf(s, r);
		return 1;
	}

	return 0;
}


/* --- standard library ----------------------------------------------------- */


/*
 * Returns 1 if the rune is a printable character and 0 if not.
 */
int
isprintrune(long r)
{
	return !(
		(r < ' ' || r == 0x7f)           ||  /* ascii control */

		(0x80 <= r && r < 0xa0)          ||  /* unicode control */

		(r > 0x10ffff)                   ||  /* outside range */

		((r & 0x00fffe) == 0x00fffe)     ||  /* noncharacters */
		(0x00fdd0 <= r && r <= 0x00fdef) ||

		(0x00e000 <= r && r <= 0x00f8ff) ||  /* private use */
		(0x0f0000 <= r && r <= 0x0ffffd) ||
		(0x100000 <= r && r <= 0x10fffd) ||

		(0x00d800 <= r && r <= 0x00dfff)     /* surrogates */
	);
}


/*
 * Read an utf string from `f` up to the first '\n' character or the
 * end of the file.  It is stored as a rune array into the newly
 * allocated `r`.
 * 
 * Return the length of `r`, or -1 if malloc fails or if the end of
 * `f` is reached.
 */
size_t
getrunes(long **r, FILE *f)
{
	size_t slen, rlen = 0, size = BUFSIZ, i;
	int c;
	char *s;

	if (!(s = malloc(size))) return -1;
	for (slen = 0; (c = fgetc(f)) != EOF && (c != '\n'); slen++) {
		if (slen > size && !(s = realloc(s, ++size))) return -1;
		s[slen] = c;
	}

	if (!(*r = malloc(size * sizeof (long)))) return -1;
	for (i = 0; i < slen; rlen++)
		i += utftorune(*r + rlen, s + i, slen - i);
	(*r)[rlen] = '\0';

	free(s);
	return feof(f) ? -1 : rlen;
}


long *
runescpy(long *dest, long *src)
{
	size_t i;

	for (i = 0; src[i] != '\0'; i++)
		dest[i] = src[i];
	dest[i] = '\0';

	return dest;	
}


long *
runeschr(long *s, long r)
{
	size_t i;

	for (i = 0; s[i] != '\0'; i++)
		if (s[i] == r) return s + i;

	return NULL;
}


long *
runescat(long *s1, long *s2)
{
	size_t i, j;

	for (i = 0; s1[i] != '\0'; i++);
	for (j = 0; s2[j] != '\0'; j++)
		s1[i + j] = s2[j];
	s1[i + j] = '\0';

	return s1;
}


int
main()
{
	char s[BUFSIZ];
	long *r;
	int len, i;

	for (len = 0; (len = getutf(&r, stdin)) >= 0 && !feof(stdin); free(r)) {
		for (i = 0; i < len; i++) {
			runetoprint(s, r[i]);
			fputs(s, stdout);
		}

		putchar('\n');
	}
	free(r);

	return 0;
}

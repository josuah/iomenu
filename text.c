/*
 * Functions handling UTF-8 srings:
 *
 * stdin  -> buffer -> stdout
 * char[] -> long[] -> char[]
 * UTF-8  ->  rune  -> UTF-8
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "text.h"


/*
 * Return the number of bytes in rune for the `len` next char in `s`,
 * or 0 if `utf` is misencoded.
 *
 * Thanks to Connor Lane Smith for some ideas.
 */
int
utflen(char *s, int n) {
	int len = 1;
	int contiunation_bytes =
		(s[0] & 0x80) == 0x00 ? 0 :  /* 0xxxxxxx */
		(s[0] & 0xc0) == 0x80 ? 1 :  /* 10xxxxxx */
		(s[0] & 0xe0) == 0xc0 ? 2 :  /* 110xxxxx */
		(s[0] & 0xf0) == 0xe0 ? 3 :  /* 1110xxxx */
		(s[0] & 0xf8) == 0xf0 ? 4 :  /* 11110xxx */
		(s[0] & 0xfc) == 0xf8 ? 5 :  /* 111110xx */
		(s[0] & 0xfe) == 0xfc ? 6 :  /* 1111110x */
		(s[0] & 0xff) == 0xfe ? 7 :  /* 11111110 */
		                        8;   /* 11111111 */

	if (contiunation_bytes > 6 || contiunation_bytes > n)
		return 0;

	/* check if continuation bytes are 10xxxxxx and increment `len` */
	switch (contiunation_bytes) {  /* FALLTHROUGH */
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
 * return the number of bytes required to display `rune`
 */
int
runelen(long r) {
	if (r <= 0x0000007f) return 1;
	if (r <= 0x000007ff) return 2;
	if (r <= 0x0000ffff) return 3;
	if (r <= 0x001fffff) return 4;
	if (r <= 0x03ffffff) return 5;
	if (r <= 0x7fffffff) return 6;
	return 0;
}


/*
 * return the firsts `len` bytes in the sring poined by `utf` to a rune.
 * if the `utf` is misencoded, the first char is returned as a
 * negative value.
 */
int
utftorune(long *r, char *s, int n) {
	int len = utflen(s, n);

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
	for (int i = 1; i < len; i++)
		*r = (*r << 6) | (s[i] & 0x3f);  /* 10xxxxxx */

	/* overlong sequences */
	if (runelen(*r) != len) {
		*r = -(unsigned char) s[0];
		return 1;
	}
 
	return len;
}


/*
 * return the next rune in the `len` next `utf`, or 0 if
 * `utf` is misencoded.
 */
int
runetoutf(char *s, long r) {
	switch (runelen(r)) {
	case 1:
		s[0] = r;                         /* 0xxxxxxx */
		s[1] = '\0';
		return 1;
	case 2:
		s[0] = 0xc0 | (0x3f & (r >> 6));  /* 110xxxxx */
		s[1] = 0x80 | (0x3f & (r));       /* 10xxxxxx */
		s[2] = '\0';
		return 2;
	case 3:
		s[0] = 0xe0 | (0x3f & (r >> 12)); /* 1110xxxx */
		s[1] = 0x80 | (0x3f & (r >> 6));  /* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (r));       /* 10xxxxxx */
		s[3] = '\0';
		return 3;
	case 4:
		s[0] = 0xf0 | (0x3f & (r >> 6));  /* 11110xxx */
		s[1] = 0x80 | (0x3f & (r >> 6));  /* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (r >> 6));  /* 10xxxxxx */
		s[3] = 0x80 | (0x3f & (r));       /* 10xxxxxx */
		s[4] = '\0';
		return 4;
	case 5:
		s[0] = 0xf8 | (0x3f & (r >> 24));  /* 111110xx */
		s[1] = 0x80 | (0x3f & (r >> 18));  /* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (r >> 12));  /* 10xxxxxx */
		s[3] = 0x80 | (0x3f & (r >> 6));   /* 10xxxxxx */
		s[4] = 0x80 | (0x3f & (r));        /* 10xxxxxx */
		s[5] = '\0';
		return 5;
	case 6:
		s[0] = 0xfc | (0x3f & (r >> 30));  /* 1111110x */
		s[1] = 0x80 | (0x3f & (r >> 24));  /* 10xxxxxx */
		s[2] = 0x80 | (0x3f & (r >> 18));  /* 10xxxxxx */
		s[3] = 0x80 | (0x3f & (r >> 12));  /* 10xxxxxx */
		s[4] = 0x80 | (0x3f & (r >> 6));   /* 10xxxxxx */
		s[5] = 0x80 | (0x3f & (r));        /* 10xxxxxx */
		s[6] = '\0';
		return 6;
	}

	return 0;
}


/*
 * Fill `s` with a printable representation of `r` and return the
 * width of the character
 */
int
runetoprint(char *s, long r, int col)
{
	/* ASCII control characters and invalid characters */
	if (r == '\t') {
		int i;
		for (i = 0; i < (col + 1) % 8 - 1; i++)
			s[i] = ' ';
		s[i] = '\0';

	} else if (r < ' ' || r == 0x7f) {
		sprintf(s, "[%02x]", (char) r);

	/* non-breaking space */
	} else if (r == 0xa0) {
		sprintf(s, "[ ]");

	/* soft hyphen */
	} else if (r == 0xad) {
		sprintf(s, "[-]");

	/* valid UTF-8 but not printable Unicode code points */
	} else if (
		/* unicode control */
		(0x80 <= r && r < 0xa0)          ||

		/* outside range */
		(r > 0x10ffff)                   ||

		/* noncharacters */
		(r % 0x010000 == 0x00fffe)       ||
		(r % 0x010000 == 0x00ffff)       ||
		(0x00fdd0 <= r && r <= 0x00fdef) ||

		/* private use */
		(0x00e000 <= r && r <= 0x00f8ff) ||
		(0x0f0000 <= r && r <= 0x0ffffd) ||
		(0x100000 <= r && r <= 0x10fffd) ||

		/* surrogates */
		(0x00d800 <= r && r <= 0x00dfff)
	) {
		sprintf(s, "[%04x]", (unsigned int) r);

	/* valid unicode characters */
	} else {
		runetoutf(s, r);
		return 1;
	}

	return 0;
}


/*
 * Read a newly allocated string `s` from `file` up to the first '\n'
 * character or the end of the file.
 */
int
getutf(char **s, FILE *file)
{
	int i; int c;

	*s = malloc(BUFSIZ);

	for (i = 0; (c = fgetc(file)) != EOF && (c != '\n'); i++) {
		(*s)[i] = c;

		if ((size_t) i + 16 >= sizeof(s))
			*s = realloc(*s, sizeof(s) + BUFSIZ);
	}

	return i;
}


int
main()
{
	char s[7];
	long r;

	for (int i = 0; i < 9000; i++) {
		runetoutf(s, i);
		utftorune(&r, s, 7);
		runetoutf(s, r);
		utftorune(&r, s, 7);
		runetoprint(s, r, 0);

		printf("%5X: ", r);
		printf("'%s'\t", s);

		if (i % 8 == 0)
			puts("");
	}

	return 0;
}

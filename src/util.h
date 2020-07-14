#ifndef UTIL_H
#define UTIL_H

#define ESC		0x1b			/* Esc key */
#define CTL(c)		((c) & ~0x40)		/* Ctr + (c) key */
#define ALT(c)		((c) + 0x80)		/* Alt + (c) key */
#define CSI(c)		((c) + 0x80 + 0x80)	/* Escape + '[' + (c) code */
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))
#define MAX(x, y)	(((x) > (y)) ? (x) : (y))
#define LEN(x)		(sizeof(x) / sizeof(*(x)))

#endif

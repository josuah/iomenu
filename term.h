#ifndef TERM_H
#define TERM_H

#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define TERM_KEY_CTRL(c)    ((c) & ~0x40)
#define TERM_KEY_ALT(c)	    (0x100 + (c))
#define TERM_KEY_CSI(c, i)  (0x100 + (c) * 0x100 + (i))

enum term_key {
	TERM_KEY_ESC        = 0x1b,
	TERM_KEY_DELETE     = 127,
	TERM_KEY_BACKSPACE  = TERM_KEY_CTRL('H'),
	TERM_KEY_TAB        = TERM_KEY_CTRL('I'),
	TERM_KEY_ENTER      = TERM_KEY_CTRL('J'),
	TERM_KEY_ARROW_UP   = TERM_KEY_CSI('A', 0),
	TERM_KEY_ARROW_DOWN = TERM_KEY_CSI('B', 0),
	TERM_KEY_PAGE_UP    = TERM_KEY_CSI('~', 5),
	TERM_KEY_PAGE_DOWN  = TERM_KEY_CSI('~', 6),
};

struct term {
	struct winsize winsize;
	struct termios old_termios;
};

struct term term;
int term_width_at_pos(uint32_t codepoint, int pos);
int term_at_width(char const *s, int width, int pos);
int term_raw_on(int fd);
int term_raw_off(int fd);
int term_get_key(FILE *fp);

#endif

#include "term.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "compat.h"
#include "utf8.h"

struct term term;

static int
term_codepoint_width(uint32_t codepoint, int pos)
{
	if (codepoint == '\t')
		return 8 - pos % 8;
	return wcwidth(codepoint);
}

int
term_at_width(char const *s, int width, int pos)
{
	char const *beg = s;

	for (uint32_t state = 0, codepoint; *s != '\0'; s++) {
		if (utf8_decode(&state, &codepoint, *s) == UTF8_ACCEPT) {
			pos += term_codepoint_width(codepoint, pos);
			if (pos > width)
				break;
		}
	}
	return s - beg;
}

int
term_raw_on(int fd)
{
	struct termios new_termios = {0};
	char *seq = "\x1b[s\x1b[?1049h\x1b[H";
	ssize_t len = strlen(seq);

	if (write(fd, seq, len) < len)
		return -1;

	if (tcgetattr(fd, &term.old_termios) < 0)
		return -1;
	memcpy(&new_termios, &term.old_termios, sizeof new_termios);

	new_termios.c_lflag &= ~(ICANON | ECHO | IEXTEN | IGNBRK | ISIG);
	if (tcsetattr(fd, TCSANOW, &new_termios) == -1)
		return -1;
	return 0;
}

int
term_raw_off(int fd)
{
	char *seq = "\x1b[2J\x1b[u\033[?1049l";
	ssize_t len = strlen(seq);

	if (tcsetattr(fd, TCSANOW, &term.old_termios) < 0)
		return -1;
	if (write(fd, seq, len) < len)
		return -1;
	return 0;
}

int
term_get_key(FILE *fp)
{
	int key, num;

	key = fgetc(fp);
top:
	switch (key) {
	case EOF:
		return -1;
	case TERM_KEY_ALT('['):
		key = getc(fp);
		if (key == EOF)
			return -1;

		for (num = 0; isdigit(key);) {
			num *= 10;
			num += key - '0';

			key = fgetc(fp);
			if (key == EOF)
				return -1;
		}

		key = TERM_KEY_CSI(key, num);

		goto top;
	case TERM_KEY_ESC:
		key = getc(fp);
		if (key == EOF)
			return -1;
		key = TERM_KEY_ALT(key);
		goto top;
	default:
		return key;
	}
}

#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "iomenu.h"
#include "utf8.h"
#include "control.h"
#include "display.h"

static char *
format(char *str, int cols)
{
	int   col = 0;
	long  rune = 0;
	char *fmt = formatted;

	while (*str && col < cols) {
		if (*str == '\t') {
			int t = 8 - col % 8;
			while (t-- && col < cols) {
				*fmt++ = ' ';
				col++;
			}
			str++;
		} else if (utf8_to_rune(&rune, str) && utf8_is_print(rune)) {
			int i = utf8_len(str);
			while (i--)
				*fmt++ = *str++;
			col++;
		} else {
			*fmt++ = '?';
			col++;
			str++;
		}
	}
	*fmt = '\0';

	return formatted;
}

static void
print_line(char *line, int cur)
{
	if (opt['#'] && line[0] == '#') {
		format(line + 1, ws.ws_col - 1);
		fprintf(stderr, "\n\x1b[1m %s\x1b[m", formatted);
	} else if (cur) {
		format(line, ws.ws_col - 1);
		fprintf(stderr, "\n\x1b[47;30m\x1b[K %s\x1b[m", formatted);
	} else {
		format(line, ws.ws_col - 1);
		fprintf(stderr, "\n %s", formatted);
	}
}

void
print_screen(void)
{
	char **m;
	int p;
	int i;
	int cols = ws.ws_col - 1;

	p = 0;
	i = current - current % rows;
	m = matchv + i;
	fputs("\x1b[2J", stderr);
	while (p < rows && i < matchc) {
		print_line(*m, i == current);
		p++, i++, m++;
	}
	fputs("\x1b[H", stderr);
	if (*prompt) {
		format(prompt, cols - 2);
		fprintf(stderr, "\x1b[30;47m %s \x1b[m", formatted);
		cols -= strlen(formatted) + 2;
	}
	fputc(' ', stderr);
	fputs(format(input, cols), stderr);
	fflush(stderr);
}

void
print_selection(void)
{
	char **match;

	if (opt['#']) {
		match = matchv + current;
		while (--match >= matchv) {
			if ((*match)[0] == '#') {
				fputs(*match + 1, stdout);
				break;
			}
		}
		putchar('\t');
	}
	if (matchc == 0 || (opt['#'] && matchv[current][0] == '#'))
		puts(input);
	else
		puts(matchv[current]);
}

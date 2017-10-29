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
		} else if (utf8_to_rune(&rune, str) && rune_is_print(rune)) {
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
print_lines(void)
{
	int printed = 0, i = current - current % rows;

	for (; printed < rows && i < matchc; i++, printed++) {
		fprintf(stderr,
			opt['#'] && matchv[i][0] == '#' ?
			"\n\x1b[1m\x1b[K %s\x1b[m"      :
			i == current                    ?
			"\n\x1b[47;30m\x1b[K %s\x1b[m"      :
			"\n\x1b[K %s",
			format(matchv[i], ws.ws_col - 1)
		);
	}
	while (printed++ < rows)
		fputs("\n\x1b[K", stderr);
	fprintf(stderr, "\x1b[%dA\r\x1b[K", rows);
}

static void
print_segments(void)
{
	int i;

	if (current < offset) {
		next   = offset;
		offset = prev_page(offset);
	} else if (current >= next) {
		offset = next;
		next   = next_page(offset);
	}
	fprintf(stderr, "\r\x1b[K\x1b[%dC", MARGIN);
	fputs(offset > 0 ? "< " : "  ", stderr);
	for (i = offset; i < next && i < matchc; i++) {
		fprintf(stderr,
			opt['#'] && matchv[i][0] == '#' ? "\x1b[1m %s \x1b[m" :
			i == current ? "\x1b[7m %s \x1b[m" : " %s ",
			format(matchv[i], ws.ws_col - 1)
		);
	}
	if (next < matchc)
		fprintf(stderr, "\x1b[%dC\b>", ws.ws_col - MARGIN);
	fputc('\r', stderr);
}

void
print_screen(void)
{
	int cols = ws.ws_col - 1;

	if (opt['l'] > 0)
		print_lines();
	else
		print_segments();
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
	fputs("\r\x1b[K", stderr);
}

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include "log.h"
#include "mem.h"
#include "term.h"
#include "utf8.h"

struct {
	FILE *tty;
	char input[LINE_MAX];
	size_t cur;

	char **lines_buf;
	size_t lines_len;

	char **match_buf;
	size_t match_len;
} ctx;

int opt_comment;

/*
 * Keep the line if it match every token (in no particular order,
 * and allowed to be overlapping).
 */
static int
match_line(char *line, char **tokv)
{
	if (opt_comment && line[0] == '#')
		return 2;
	for (; *tokv != NULL; tokv++)
		if (strcasestr(line, *tokv) == NULL)
			return 0;
	return 1;
}

/*
 * Free the structures, reset the terminal state and exit with an
 * error message.
 */
static void
goodbye(const char *s)
{
	int e = errno;

	term_raw_off(2);
	errno = e;
	die("%s", s);
}

static void
do_move(int sign)
{
	/* integer overflow will do what we need */
	for (size_t i = ctx.cur + sign; i < ctx.match_len; i += sign) {
		if (opt_comment == 0 || ctx.match_buf[i][0] != '#') {
			ctx.cur = i;
			break;
		}
	}
}

/*
 * First split input into token, then match every token independently against
 * every line.  The matching lines fills matches.  Matches are searched inside
 * of `searchv' of size `searchc'
 */
static void
do_filter(char **search_buf, size_t search_len)
{
	char **t, *tokv[(sizeof ctx.input + 1) * sizeof(char *)];
	char *b, buf[sizeof ctx.input];

	strlcpy(buf, ctx.input, sizeof buf);

	for (b = buf, t = tokv; (*t = strsep(&b, " \t")) != NULL; t++)
		continue;
	*t = NULL;

	ctx.cur = ctx.match_len = 0;
	for (size_t n = 0; n < search_len; n++)
		if (match_line(search_buf[n], tokv))
			ctx.match_buf[ctx.match_len++] = search_buf[n];
	if (opt_comment && ctx.match_buf[ctx.cur][0] == '#')
		do_move(+1);
}

static void
do_move_page(signed int sign)
{
	int rows = term.winsize.ws_row - 1;
	size_t i = ctx.cur - ctx.cur % rows + rows * sign;

	if (i >= ctx.match_len)
		return;
	ctx.cur = i - 1;

	do_move(+1);
}

static void
do_move_header(signed int sign)
{
	do_move(sign);

	if (opt_comment == 0)
		return;
	for (ctx.cur += sign;; ctx.cur += sign) {
		char *cur = ctx.match_buf[ctx.cur];

		if (ctx.cur >= ctx.match_len) {
			ctx.cur--;
			break;
		}
		if (cur[0] == '#')
			break;
	}

	do_move(+1);
}

static void
do_remove_word(void)
{
	int len, i;

	len = strlen(ctx.input) - 1;
	for (i = len; i >= 0 && isspace(ctx.input[i]); i--)
		ctx.input[i] = '\0';
	len = strlen(ctx.input) - 1;
	for (i = len; i >= 0 && !isspace(ctx.input[i]); i--)
		ctx.input[i] = '\0';
	do_filter(ctx.lines_buf, ctx.lines_len);
}

static void
do_add_char(char c)
{
	int len;

	len = strlen(ctx.input);
	if (len + 1 == sizeof ctx.input)
		return;
	if (isprint(c)) {
		ctx.input[len] = c;
		ctx.input[len + 1] = '\0';
	}
	do_filter(ctx.match_buf, ctx.match_len);
}

static void
do_print_selection(void)
{
	if (opt_comment) {
		char **match = ctx.match_buf + ctx.cur;

		while (--match >= ctx.match_buf) {
			if ((*match)[0] == '#') {
				fprintf(stdout, "%s", *match + 1);
				break;
			}
		}
		fprintf(stdout, "%c", '\t');
	}
	term_raw_off(2);
	if (ctx.match_len == 0
	  || (opt_comment && ctx.match_buf[ctx.cur][0] == '#'))
		fprintf(stdout, "%s\n", ctx.input);
	else
		fprintf(stdout, "%s\n", ctx.match_buf[ctx.cur]);
	term_raw_on(2);
}

/*
 * Big case table, that calls itself back for with TERM_KEY_ALT (aka Esc), TERM_KEY_CSI
 * (aka Esc + [).  These last two have values above the range of ASCII.
 */
static int
key_action(void)
{
	int key;

	key = term_get_key(stderr);
	switch (key) {
	case TERM_KEY_CTRL('Z'):
		term_raw_off(2);
		kill(getpid(), SIGSTOP);
		term_raw_on(2);
		break;
	case TERM_KEY_CTRL('C'):
	case TERM_KEY_CTRL('D'):
		return -1;
	case TERM_KEY_CTRL('U'):
		ctx.input[0] = '\0';
		do_filter(ctx.lines_buf, ctx.lines_len);
		break;
	case TERM_KEY_CTRL('W'):
		do_remove_word();
		break;
	case TERM_KEY_DELETE:
	case TERM_KEY_BACKSPACE:
		ctx.input[strlen(ctx.input) - 1] = '\0';
		do_filter(ctx.lines_buf, ctx.lines_len);
		break;
	case TERM_KEY_ARROW_UP:
	case TERM_KEY_CTRL('P'):
		do_move(-1);
		break;
	case TERM_KEY_ALT('p'):
		do_move_header(-1);
		break;
	case TERM_KEY_ARROW_DOWN:
	case TERM_KEY_CTRL('N'):
		do_move(+1);
		break;
	case TERM_KEY_ALT('n'):
		do_move_header(+1);
		break;
	case TERM_KEY_PAGE_UP:
	case TERM_KEY_ALT('v'):
		do_move_page(-1);
		break;
	case TERM_KEY_PAGE_DOWN:
	case TERM_KEY_CTRL('V'):
		do_move_page(+1);
		break;
	case TERM_KEY_TAB:
		if (ctx.match_len == 0)
			break;
		strlcpy(ctx.input, ctx.match_buf[ctx.cur], sizeof(ctx.input));
		do_filter(ctx.match_buf, ctx.match_len);
		break;
	case TERM_KEY_ENTER:
	case TERM_KEY_CTRL('M'):
		do_print_selection();
		return 0;
	default:
		do_add_char(key);
	}

	return 1;
}

static void
print_line(char *line, int highlight)
{
	if (opt_comment && line[0] == '#') {
		fprintf(stderr, "\n\x1b[1m\r%.*s\x1b[m",
		  term_at_width(line + 1, term.winsize.ws_col, 0), line + 1);
	} else if (highlight) {
		fprintf(stderr, "\n\x1b[47;30m\x1b[K\r%.*s\x1b[m",
		  term_at_width(line, term.winsize.ws_col, 0), line);
	} else {
		fprintf(stderr, "\n%.*s",
		  term_at_width(line, term.winsize.ws_col, 0), line);
	}
}

static void
do_print_screen(void)
{
	char **m;
	int p, c, cols, rows;
	size_t i;

	cols = term.winsize.ws_col;
	rows = term.winsize.ws_row - 1; /* -1 to keep one line for user input */
	p = c = 0;
	i = ctx.cur - ctx.cur % rows;
	m = ctx.match_buf + i;
	fprintf(stderr, "\x1b[2J");
	while (p < rows && i < ctx.match_len) {
		print_line(*m, i == ctx.cur);
		p++, i++, m++;
	}
	fprintf(stderr, "\x1b[H%.*s",
	  term_at_width(ctx.input, cols, c), ctx.input);
	fflush(stderr);
}

static void
sig_winch(int sig)
{
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &term.winsize) == -1)
		goodbye("ioctl");
	do_print_screen();
	signal(sig, sig_winch);
}

static void
usage(char const *arg0)
{
	fprintf(stderr, "usage: %s [-#] <lines\n", arg0);
	exit(1);
}

static void
read_stdin(char **buf, struct mem_pool *pool)
{
	if (mem_read((void **)buf, pool) < 0)
		goodbye("reading standard input");
	if (memchr(*buf, '\0', mem_length(*buf)) != NULL)
		goodbye("'\\0' byte in input");
	if (mem_append((void **)buf, "", 1) < 0)
		goodbye("adding '\\0' terminator");
}

/*
 * Split a buffer into an array of lines, without allocating memory for every
 * line, but using the input buffer and replacing '\n' by '\0'.
 */
static void
split_lines(char *s, struct mem_pool *pool)
{
	ctx.lines_buf = mem_alloc(pool, 0);
	if (ctx.lines_buf == NULL)
		goodbye("initializing full lines buffer");

	ctx.lines_len = 0;
	for (;;) {
		if (mem_append((void **)&ctx.lines_buf, &s, sizeof s) < 0)
			goodbye("adding line to array");
		ctx.lines_len++;

		s = strchr(s, '\n');
		if (s == NULL)
			break;
		*s++ = '\0';
	}

	ctx.match_buf = mem_alloc(pool, mem_length(ctx.lines_buf));
	if (ctx.match_buf == NULL)
		goodbye("initializing matching lines buffer");
}

/*
 * Read stdin in a buffer, filling a table of lines, then re-open stdin to
 * /dev/tty for an interactive (raw) session to let the user filter and select
 * one line by searching words within stdin.  This was inspired from dmenu.
 */
int
main(int argc, char *argv[])
{
	struct mem_pool pool = {0};
	char *buf;

	arg0 = *argv;
	for (int opt; (opt = getopt(argc, argv, "#v")) > 0;) {
		switch (opt) {
		case 'v':
			fprintf(stdout, "%s\n", VERSION);
			exit(0);
		case '#':
			opt_comment = 1;
			break;
		default:
			usage(arg0);
		}
	}
	argc -= optind;
	argv += optind;

	read_stdin(&buf, &pool);
	split_lines(buf, &pool);

	do_filter(ctx.lines_buf, ctx.lines_len);

	if (!isatty(2))
		goodbye("file descriptor 2 (stderr)");

	freopen("/dev/tty", "w+", stderr);
	if (stderr == NULL)
		goodbye("re-opening standard error read/write");

	term_raw_on(2);
	sig_winch(SIGWINCH);

#ifdef __OpenBSD__
	pledge("stdio tty", NULL);
#endif

	while (key_action() > 0)
		do_print_screen();

	term_raw_off(2);

	mem_free(&pool);
	return 0;
}

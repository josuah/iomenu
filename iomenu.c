#include <sys/ioctl.h>

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "utf8.h"
#include "util.h"
#include "arg.h"

#ifndef SIGWINCH
#define SIGWINCH	28
#endif

static struct termios	termios;
struct winsize		ws;
static int		linec = 0, matchc = 0, cur = 0;
static char		**linev = NULL, **matchv = NULL;
static char		input[LINE_MAX];
static int		hsflag = 0;
char			*argv0;

/*
** Keep the line if it match every token (in no particular order,
** and allowed to be overlapping).
*/
static int
match_line(char *line, char **tokv)
{
	if (hsflag && line[0] == '#')
		return 2;
	for (; *tokv != NULL; tokv++)
		if (strcasestr(line, *tokv) == NULL)
			return 0;
	return 1;
}

/*
** Free the structures, reset the terminal state and exit with an
** error message.
*/
static void
die(const char *s)
{
	if (tcsetattr(STDERR_FILENO, TCSANOW, &termios) == -1)
		perror("tcsetattr while dying");
	close(STDERR_FILENO);
	perror(s);
	exit(EXIT_FAILURE);
}

/*
** Read one key from stdin and die if it failed to prevent to read
** in an endless loop.  This caused the load average to go over 10
** at work.  :S
*/
int
getkey(void)
{
	int c;

	if ((c = fgetc(stdin)) == EOF)
		die("getting a key");
	return c;
}

/*
** Split a buffer into an array of lines, without allocating memory for every
** line, but using the input buffer and replacing '\n' by '\0'.
*/
static void
split_lines(char *buf)
{
	char	*b, **lv, **mv;

	linec = 1;
	for (b = buf; (b = strchr(b, '\n')) != NULL && b[1] != '\0'; b++)
		linec++;
	if ((lv = linev = calloc(linec + 1, sizeof(char **))) == NULL)
		die("calloc");
	if ((mv = matchv = calloc(linec + 1, sizeof(char **))) == NULL)
		die("calloc");
	*mv = *lv = b = buf;
	while ((b = strchr(b, '\n')) != NULL) {
		*b = '\0';
		*++mv = *++lv = ++b;
	}
}

/*
** Read stdin in a single malloc-ed buffer, realloc-ed to twice its size every
** time the previous buffer is filled.
*/
static void
read_stdin(void)
{
	size_t size, len, off;
	char *buf;

	size = BUFSIZ;
	off = 0;
	if ((buf = malloc(size)) == NULL)
		die("malloc");
	while ((len = read(STDIN_FILENO, buf + off, size - off)) > 0) {
		off += len;
		if (off == size) {
			size *= 2;
			if ((buf = realloc(buf, size + 1)) == NULL)
				die("realloc");
		}
	}
	buf[off] = '\0';
	split_lines(buf);
}

static void
move(int sign)
{
	int i;

	for (i = cur + sign; 0 <= i && i < matchc; i += sign) {
		if (hsflag == 0 || matchv[i][0] != '#') {
			cur = i;
			break;
		}
	}
}

static void
tokenize(char **tokv, char *str)
{
	while ((*tokv = strsep(&str, " \t")) != NULL) {
		if (**tokv != '\0')
			tokv++;
	}
	*tokv = NULL;
}

/*
** First split input into token, then match every token independently against
** every line.  The matching lines fills matchv.  Matches are searched inside
** of `searchv' of size `searchc'
*/
static void
filter(int searchc, char **searchv)
{
	int n;
	char *tokv[sizeof(input) * sizeof(char *) + sizeof(NULL)];
	char buf[sizeof(input)];

	strncpy(buf, input, sizeof(input));
	buf[sizeof(input) - 1] = '\0';
	tokenize(tokv, buf);

	cur = matchc = 0;
	for (n = 0; n < searchc; n++)
		if (match_line(searchv[n], tokv))
			matchv[matchc++] = searchv[n];
	if (hsflag && matchv[cur][0] == '#')
		move(+1);
}

static void
move_page(signed int sign)
{
	int i, rows;

	rows = ws.ws_row - 1;
	i = cur - cur % rows + rows * sign;
	if (!(0 <= i && i < matchc))
		return;
	cur = i - 1;
	move(+1);
}

static void
move_header(signed int sign)
{
	move(sign);
	if (hsflag == 0)
		return;
	for (cur += sign; 0 <= cur; cur += sign) {
		if (cur >= matchc) {
			cur--;
			break;
		}
		if (matchv[cur][0] == '#')
			break;
	}
	move(+1);
}

static void
remove_word()
{
	int len, i;

	len = strlen(input) - 1;
	for (i = len; i >= 0 && isspace(input[i]); i--)
		input[i] = '\0';
	len = strlen(input) - 1;
	for (i = len; i >= 0 && !isspace(input[i]); i--)
		input[i] = '\0';
	filter(linec, linev);
}

static void
add_char(char c)
{
	int len;

	len = strlen(input);
	if (isprint(c)) {
		input[len]     = c;
		input[len + 1] = '\0';
	}
	filter(matchc, matchv);
}

static void
print_selection(void)
{
	char **match;

	if (hsflag) {
		match = matchv + cur;
		while (--match >= matchv) {
			if ((*match)[0] == '#') {
				fputs(*match + 1, stdout);
				break;
			}
		}
		putchar('\t');
	}
	if (matchc == 0 || (hsflag && matchv[cur][0] == '#'))
		puts(input);
	else
		puts(matchv[cur]);
}

/*
 * Big case table, that calls itself back for with ALT (aka Esc), CSI
 * (aka Esc + [).  These last two have values above the range of ASCII.
 */
int
key(void)
{
	int k;
	k = getkey();
top:
	switch (k) {
	case CTL('C'):
		return -1;
	case CTL('U'):
		input[0] = '\0';
		filter(linec, linev);
		break;
	case CTL('W'):
		remove_word();
		break;
	case 127:
	case CTL('H'):	/* backspace */
		input[strlen(input) - 1] = '\0';
		filter(linec, linev);
		break;
	case CSI('A'):	/* up */
	case CTL('P'):
		move(-1);
		break;
	case ALT('p'):
		move_header(-1);
		break;
	case CSI('B'):	/* down */
	case CTL('N'):
		move(+1);
		break;
	case ALT('n'):
		move_header(+1);
		break;
	case CSI('5'):	/* page up */
		if (getkey() != '~')
			break;
		/* FALLTHROUGH */
	case ALT('v'):
		move_page(-1);
		break;
	case CSI('6'):	/* page down */
		if (getkey() != '~')
			break;
		/* FALLTHROUGH */
	case CTL('V'):
		move_page(+1);
		break;
	case CTL('I'):	/* tab */
		if (linec > 0) {
			strncpy(input, matchv[cur], sizeof(input));
			input[sizeof(input) - 1] = '\0';
		}
		filter(matchc, matchv);
		break;
	case CTL('J'):	/* enter */
	case CTL('M'):
		print_selection();
		return 0;
	case ALT('['):
		k = CSI(getkey());
		goto top;
	case ESC:
		k = ALT(getkey());
		goto top;
	default:
		add_char((char) k);
	}

	return 1;
}

static void
print_line(char *line, int highlight)
{
	if (hsflag && line[0] == '#')
		fprintf(stderr, "\n\x1b[1m\r%.*s\x1b[m",
		    utf8_col(line + 1, ws.ws_col, 0), line + 1);
	else if (highlight)
		fprintf(stderr, "\n\x1b[47;30m\x1b[K\r%.*s\x1b[m",
		    utf8_col(line, ws.ws_col, 0), line);
	else
		fprintf(stderr, "\n%.*s",
		    utf8_col(line, ws.ws_col, 0), line);
}

static void
print_screen(void)
{
	char **m;
	int p, i, c, cols, rows;

	cols = ws.ws_col;
	rows = ws.ws_row - 1;	/* -1 to keep one line for user input */
	p = c = 0;
	i = cur - cur % rows;
	m = matchv + i;
	fputs("\x1b[2J", stderr);
	while (p < rows && i < matchc) {
		print_line(*m, i == cur);
		p++, i++, m++;
	}
	fputs("\x1b[H", stderr);
	fprintf(stderr, "%.*s", utf8_col(input, cols, c), input);
	fflush(stderr);
}

/*
 * Set terminal to raw mode.
 */
static void
term_set(void)
{
	struct termios new;

	fputs("\x1b[s\x1b[?1049h\x1b[H", stderr);
	if (tcgetattr(STDERR_FILENO, &termios) == -1 ||
	    tcgetattr(STDERR_FILENO, &new) == -1)
		die("setting terminal");
	new.c_lflag &= ~(ICANON | ECHO | IEXTEN | IGNBRK | ISIG);
	if (tcsetattr(STDERR_FILENO, TCSANOW, &new) == -1)
		die("tcsetattr");
}

/*
 * Take terminal out of raw mode.
 */
static void
term_reset(void)
{
	fputs("\x1b[2J\x1b[u\033[?1049l", stderr);
	if (tcsetattr(STDERR_FILENO, TCSANOW, &termios))
		die("resetting terminal");
}

static void
sigwinch(int sig)
{
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1)
		die("ioctl");
	print_screen();
	signal(sig, sigwinch);
}

static void
usage(void)
{
	fputs("usage: iomenu [-#]\n", stderr);
	exit(EXIT_FAILURE);
}

/*
** Read stdin in a buffer, filling a table of lines, then re-open stdin to
** /dev/tty for an interactive (raw) session to let the user filter and select
** one line by searching words within stdin.  This was inspired from dmenu.
*/
int
main(int argc, char *argv[])
{
	ARGBEGIN {
	case '#':
		hsflag = 1;
		break;
	default:
		usage();
	} ARGEND

	input[0] = '\0';
	read_stdin();
	filter(linec, linev);
	if (freopen("/dev/tty", "r", stdin) == NULL)
		die("reopening /dev/tty as stdin");
	term_set();
	sigwinch(SIGWINCH);

#ifdef __OpenBSD__
	pledge("stdio tty", NULL);
#endif

	while (key() > 0)
		print_screen();
	term_reset();

	return 0;
}

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
#include "str.h"

#ifndef SIGWINCH
#define SIGWINCH	28
#endif

#define MIN(X, Y)	(((X) < (Y)) ? (X) : (Y))
#define CTL(char)	((char) & ~0x40)
#define ALT(char)	((char) + 0x80)
#define CSI(char)	((char) + 0x80 + 0x80)

static struct termios	termios;
struct winsize		ws;
static int		ttyfd;

static int		linec = 0, matchc = 0, cur = 0;
static char		**linev = NULL, **matchv = NULL;
static char		input[LINE_MAX], formatted[LINE_MAX * 8];

static int		flag_hs = 0;

/*
** Keep the line if it match every token (in no particular order, and allowed to
** be overlapping).
*/
static int
match_line(char *line, char **tokv)
{
	if (flag_hs && line[0] == '#')
		return 2;
	for (; *tokv != NULL; tokv++)
		if (strcasestr(line, *tokv) == NULL)
			return 0;
	return 1;
}

/*
** Free the structures, reset the terminal state and exit with an error message.
*/
static void
err(const char *s)
{
	tcsetattr(ttyfd, TCSANOW, &termios);
	close(ttyfd);
	perror(s);
	exit(EXIT_FAILURE);
}

/*
** Split a buffer into an array of lines, without allocating memory for every
** line, but using the input buffer and replacing characters.
*/
static void
split_lines(char *buf)
{
	extern char	**linev, **matchv;
	extern int	linec;

	char	*b, **lv, **mv;

	linec = 1;
	for (b = buf; (b = strchr(b, '\n')) != NULL && b[1] != '\0'; b++)
		linec++;
	if ((lv = linev = calloc(linec + 1, sizeof(char **))) == NULL)
		err("calloc");
	if ((mv = matchv = calloc(linec + 1, sizeof(char **))) == NULL)
		err("calloc");
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
	size_t	size, len, off;
	char	*buf;

	size = BUFSIZ;
	off = 0;
	if ((buf = malloc(size)) == NULL)
		err("malloc");
	while ((len = read(STDIN_FILENO, buf + off, size - off)) > 0) {
		off += len;
		if (off == size) {
			size *= 2;
			if ((buf = realloc(buf, size + 1)) == NULL)
				err("realloc");
		}
	}
	buf[off] = '\0';
	split_lines(buf);
}

static void
move(int direction)
{
	extern char	**matchv;
	extern int	matchc;

	int	i;

	for (i = cur + direction; 0 <= i && i < matchc; i += direction) {
		if (!flag_hs || matchv[i][0] != '#') {
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
	extern char	**matchv;
	extern int	matchc, cur;

	int	n;
	char	*tokv[sizeof(input) * sizeof(char *) + sizeof(NULL)];
	char	*s, buf[sizeof(input)];

	strncpy(buf, input, sizeof(input));
	buf[sizeof(input) - 1] = '\0';
	tokenize(tokv, buf);

	cur = matchc = 0;
	for (n = 0; n < searchc; n++)
		if (match_line(searchv[n], tokv))
			matchv[matchc++] = searchv[n];
	if (flag_hs && matchv[cur][0] == '#')
		move(+1);
}

static void
move_page(signed int sign)
{
	extern	struct	winsize ws;
	extern	int	matchc;

	int	i, rows;

	rows = ws.ws_row - 1;
	i = cur - cur % rows + rows * sign;
	if (!(0 <= i && i < matchc))
		return;
	cur = i - 1;
	move(+1);
}

static void
remove_word()
{
	extern	char	input[LINE_MAX];

	int	len, i;

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
	extern	char	input[LINE_MAX];

	int	len;

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
	extern	char	**matchv, input[LINE_MAX];
	extern	int	  matchc;

	char	**match;

	if (flag_hs) {
		match = matchv + cur;
		while (--match >= matchv) {
			if ((*match)[0] == '#') {
				fputs(*match + 1, stdout);
				break;
			}
		}
		putchar('\t');
	}
	if (matchc == 0 || (flag_hs && matchv[cur][0] == '#'))
		puts(input);
	else
		puts(matchv[cur]);
}

/*
** Big case table, that calls itself back for with ALT (aka ESC), CSI
** (aka ESC + [).  These last two have values above the range of ASCII.
*/
int
key(int k)
{
	extern	char	**matchv, input[LINE_MAX];
	extern	int	  linec;

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
	case CTL('H'):  /* backspace */
		input[strlen(input) - 1] = '\0';
		filter(linec, linev);
		break;
	case CSI('A'):  /* up */
	case CTL('P'):
		move(-1);
		break;
	case CSI('B'):  /* down */
	case CTL('N'):
		move(+1);
		break;
	case CSI('5'):  /* page up */
		if (fgetc(stdin) != '~')
			break;
		/* FALLTHROUGH */
	case ALT('v'):
		move_page(-1);
		break;
	case CSI('6'):  /* page down */
		if (fgetc(stdin) != '~')
			break;
		/* FALLTHROUGH */
	case CTL('V'):
		move_page(+1);
		break;
	case CTL('I'):  /* tab */
		if (linec > 0) {
			strncpy(input, matchv[cur], sizeof(input));
			input[sizeof(input) - 1] = '\0';
		}
		filter(matchc, matchv);
		break;
	case CTL('J'):  /* enter */
	case CTL('M'):
		print_selection();
		return 0;
	case ALT('['):
		k = CSI(fgetc(stdin));
		goto top;
	case 0x1b: /* escape / alt */
		k = ALT(fgetc(stdin));
		goto top;
	default:
		add_char((char) k);
	}

	return 1;
}

static void
print_line(char *line, int highlight)
{
	extern	struct	winsize ws;

	if (flag_hs && line[0] == '#')
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
	extern struct winsize	ws;
	extern char		**matchv, input[LINE_MAX];
	extern int		matchc;

	char	**m;
	int	  p, i, c, cols, rows;

	cols = ws.ws_col;
	rows = ws.ws_row - 1;	/* keep one line for user input */
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
** Set terminal in raw mode.
*/
static void
set_terminal(void)
{
	struct termios	new;

	fputs("\x1b[s\x1b[?1049h\x1b[H", stderr);
	if (tcgetattr(ttyfd, &termios) < 0 || tcgetattr(ttyfd, &new) < 0) {
		perror("tcgetattr");
		exit(EXIT_FAILURE);
	}
	new.c_lflag &= ~(ICANON | ECHO | IEXTEN | IGNBRK);
	tcsetattr(ttyfd, TCSANOW, &new);
}

/*
** Take terminal out of raw mode.
*/
static void
reset_terminal(void)
{
	fputs("\x1b[2J\x1b[u\033[?1049l", stderr);
	tcsetattr(ttyfd, TCSANOW, &termios);
}

static void
sighandle(int sig)
{
	extern struct winsize	ws;

	switch (sig) {
	case SIGWINCH:
		if (ioctl(ttyfd, TIOCGWINSZ, &ws) < 0)
			err("ioctl");
		print_screen();
		break;
	}
	signal(sig, sighandle);
}

static void
usage(void)
{
	fputs("usage: iomenu [-#]\n", stderr);
	exit(EXIT_FAILURE);
}

/*
** XXX: switch to getopt.
*/
static void
parse_opt(int argc, char *argv[])
{
	for (argv++, argc--; argc > 0; argv++, argc--) {
		if (argv[0][0] != '-')
			usage();
		switch ((*argv)[1]) {
		case '#':
			flag_hs = 1;
			break;
		default:
			usage();
		}
	}
}

void
init(void)
{
	extern char	input[LINE_MAX];

	input[0] = '\0';
	read_stdin();
	filter(linec, linev);

	if (freopen("/dev/tty", "r", stdin) == NULL)
		err("freopen /dev/tty");
	if (freopen("/dev/tty", "w", stderr) == NULL)
		err("freopen /dev/tty");
	if ((ttyfd = open("/dev/tty", O_RDWR)) < 0)
		err("open /dev/tty");

	set_terminal();
	sighandle(SIGWINCH);
}

/*
** Read stdin in a buffer, filling a table of lines, then re-open stdin to
** /dev/tty for an interactive (raw) session to let the user filter and select
** one line by searching words within stdin.  This was inspired from dmenu.
*/
int
main(int argc, char *argv[])
{
	int		exit_code;

	parse_opt(argc, argv);
	init();

#ifdef __OpenBSD__
	pledge("stdio tty", NULL);
#endif

	while ((exit_code = key(fgetc(stdin))) > 0)
		print_screen();

	print_screen();
	reset_terminal();
	close(ttyfd);

	return exit_code;
}

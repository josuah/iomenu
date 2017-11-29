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

#ifndef SIGWINCH
#define SIGWINCH	28
#endif

#define MIN(X, Y)	(((X) < (Y)) ? (X) : (Y))
#define CTL(char)	((char) ^ 0x40)
#define ALT(char)	((char) + 0x80)
#define CSI(char)	((char) + 0x80 + 0x80)

static	struct	termios termios;
static	int	ttyfd;

struct winsize	ws;
int		linec = 0, matchc = 0, current = 0;
char		**linev = NULL, **matchv = NULL;
char		input[LINE_MAX], formatted[LINE_MAX * 8];

char	*flag_p = "";
int	flag_hs = 0;

/*
 * Keep the line if it match every token (in no particular order, and allowed to
 * be overlapping).
 */
static int
match_line(char *line, char **tokv, int tokc)
{
	if (flag_hs && line[0] == '#')
		return 2;
	while (tokc-- > 0)
		if (strstr(line, tokv[tokc]) == NULL)
			return 0;
	return 1;
}

/*
 * As we use a single buffer for the whole stdin, we only need to free it once
 * and it will free all the lines.
 */
static void
free_lines(void)
{
	extern	char	**linev;
	extern	char	**matchv;

	if (linev)
		free(linev[0]);
	free(linev);
	free(matchv);
}

/*
 * Free the structures, reset the terminal state and exit with an error message.
 */
static void
die(const char *s)
{
	tcsetattr(ttyfd, TCSANOW, &termios);
	close(ttyfd);
	free_lines();
	perror(s);
	exit(EXIT_FAILURE);
}

/*
 * Split a buffer into an array of lines, without allocating memory for every
 * line, but using the input buffer and replacing characters.
 */
static void
split_lines(char *buf)
{
	extern char	**linev, **matchv;
	extern int	linec;

	char	*b, **lv, **mv;

	linec = 0;
	b = buf;
	while ((b = strchr(b + 1, '\n')))
		linec++;
	if (!linec)
		linec = 1;
	if (!(lv = linev = calloc(linec + 1, sizeof (char **))))
		die("calloc");
	if (!(mv = matchv = calloc(linec + 1, sizeof (char **))))
		die("calloc");
	*mv = *lv = b = buf;
	while ((b = strchr(b, '\n'))) {
		*b = '\0';
		mv++, lv++;
		*mv = *lv = ++b;
	}
}

/*
 * Read stdin in a single malloc-ed buffer, realloc-ed to twice its size every
 * time the previous buffer is filled.
 */
static void
read_stdin(void)
{
	size_t	size, len, off;
	char	*buf;

	size = BUFSIZ;
	off = 0;
	if (!(buf = malloc(size)))
		die("malloc");
	while ((len = read(STDIN_FILENO, buf + off, size - off)) > 0) {
		off += len;
		if (off >= size >> 1) {
			size <<= 1;
			if (!(buf = realloc(buf, size + 1)))
				die("realloc");
		}
	}
	buf[off] = '\0';
	split_lines(buf);
}

static void
move(signed int sign)
{
	extern	char	**matchv;
	extern	int	  matchc;

	int	i;

	for (i = current + sign; 0 <= i && i < matchc; i += sign) {
		if (!flag_hs || matchv[i][0] != '#') {
			current = i;
			break;
		}
	}
}

/*
 * First split input into token, then match every token independently against
 * every line.  The matching lines fills matchv.
 */
static void
filter(void)
{
	extern char	**linev, **matchv;
	extern int	linec, matchc, current;

	int	tokc, n;
	char	**tokv, *s, buf[sizeof (input)];

	tokv = NULL;
	current = 0;
	strcpy(buf, input);
	tokc = 0;
	n = 0;
	s = strtok(buf, " ");
	while (s) {
		if (tokc >= n) {
			tokv = realloc(tokv, ++n * sizeof (*tokv));
			if (tokv == NULL)
				die("realloc");
		}
		tokv[tokc] = s;
		s = strtok(NULL, " ");
		tokc++;
	}
	matchc = 0;
	for (n = 0; n < linec; n++)
		if (match_line(linev[n], tokv, tokc))
			matchv[matchc++] = linev[n];
	free(tokv);
	if (flag_hs && matchv[current][0] == '#')
		move(+1);
}

static void
move_page(signed int sign)
{
	extern	struct	winsize ws;
	extern	int	matchc;

	int	i;
	int	rows;

	rows = ws.ws_row - 1;
	i = current - current % rows + rows * sign;
	if (!(0 <= i && i < matchc))
		return;
	current = i - 1;
	move(+1);
}

static void
remove_word()
{
	extern	char	input[LINE_MAX];

	int len;
	int i;

	len = strlen(input) - 1;
	for (i = len; i >= 0 && isspace(input[i]); i--)
		input[i] = '\0';
	len = strlen(input) - 1;
	for (i = len; i >= 0 && !isspace(input[i]); i--)
		input[i] = '\0';
	filter();
}

static void
add_char(char c)
{
	extern	char	input[LINE_MAX];

	int len;

	len = strlen(input);
	if (isprint(c)) {
		input[len]     = c;
		input[len + 1] = '\0';
	}
	filter();
}

static void
print_selection(void)
{
	extern	char	**matchv;
	extern	char	  input[LINE_MAX];
	extern	int	  matchc;

	char **match;

	if (flag_hs) {
		match = matchv + current;
		while (--match >= matchv) {
			if ((*match)[0] == '#') {
				fputs(*match + 1, stdout);
				break;
			}
		}
		putchar('\t');
	}
	if (matchc == 0 || (flag_hs && matchv[current][0] == '#'))
		puts(input);
	else
		puts(matchv[current]);
}

/*
 * Big case table, that calls itself back for with ALT (aka ESC), CSI
 * (aka ESC + [).  These last two have values above the range of ASCII.
 */
int
key(int k)
{
	extern	char	**matchv;
	extern	char	  input[LINE_MAX];
	extern	int	  linec;

top:
	switch (k) {
	case CTL('C'):
		return -1;
	case CTL('U'):
		input[0] = '\0';
		filter();
		break;
	case CTL('W'):
		remove_word();
		break;
	case 127:
	case CTL('H'):  /* backspace */
		input[strlen(input) - 1] = '\0';
		filter();
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
		if (linec > 0)
			strcpy(input, matchv[current]);
		filter();
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

static char *
format(char *str, int cols)
{
	extern	struct	winsize ws;

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
	extern	struct	winsize ws;

	if (flag_hs && line[0] == '#') {
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

static void
print_screen(void)
{
	extern	struct	winsize ws;
	extern	char	**matchv;
	extern	char	 *flag_p;
	extern	char	  input[LINE_MAX];
	extern	int	  matchc;

	char	**m;
	int	  p;
	int	  i;
	int	  cols;
	int	  rows;

	cols = ws.ws_col - 1;
	rows = ws.ws_row - 1;
	p = 0;
	i = current - current % rows;
	m = matchv + i;
	fputs("\x1b[2J", stderr);
	while (p < rows && i < matchc) {
		print_line(*m, i == current);
		p++, i++, m++;
	}
	fputs("\x1b[H", stderr);
	if (*flag_p) {
		format(flag_p, cols - 2);
		fprintf(stderr, "\x1b[30;47m %s \x1b[m", formatted);
		cols -= strlen(formatted) + 2;
	}
	fputc(' ', stderr);
	fputs(format(input, cols), stderr);
	fflush(stderr);
}

/*
 * Set terminal in raw mode.
 */
static void
set_terminal(void)
{
	struct	termios new;

	fputs("\x1b[s\x1b[?1049h\x1b[H", stderr);
	if (tcgetattr(ttyfd, &termios) < 0 || tcgetattr(ttyfd, &new) < 0) {
		perror("tcgetattr");
		exit(EXIT_FAILURE);
	}
	new.c_lflag &= ~(ICANON | ECHO | IGNBRK | IEXTEN | ISIG);
	tcsetattr(ttyfd, TCSANOW, &new);
}

/*
 * Take terminal out of raw mode.
 */
static void
reset_terminal(void)
{
	fputs("\x1b[u\033[?1049l", stderr);
	tcsetattr(ttyfd, TCSANOW, &termios);
}

/*
 * Redraw the whole screen on window resize.
 */
static void
sigwinch()
{
	extern struct winsize	ws;

	if (ioctl(ttyfd, TIOCGWINSZ, &ws) < 0)
		die("ioctl");
	print_screen();
	signal(SIGWINCH, sigwinch);
}

static void
usage(void)
{
	fputs("usage: iomenu [-#] [-p flag_p]\n", stderr);
	exit(EXIT_FAILURE);
}

/*
 * XXX: switch to getopt.
 */
static void
parse_opt(int argc, char *argv[])
{
	extern char	*flag_p;

	for (argv++, argc--; argc > 0; argv++, argc--) {
		if (argv[0][0] != '-')
			usage();
		switch ((*argv)[1]) {
		case 'p':
			if (!--argc)
				usage();
			flag_p = *++argv;
			break;
		case '#':
			flag_hs = 1;
			break;
		default:
			usage();
		}
	}
}

/*
 * Read stdin in a buffer, filling a table of lines, then re-open stdin to
 * /dev/tty for an interactive (raw) session to let the user filter and select
 * one line by searching words within stdin.  This was inspired from dmenu.
 */
int
main(int argc, char *argv[])
{
	extern char	input[LINE_MAX];

	int		exit_code;

	parse_opt(argc, argv);
	read_stdin();
	filter();
	if (!freopen("/dev/tty", "r", stdin))
		die("freopen /dev/tty");
	if (!freopen("/dev/tty", "w", stderr))
		die("freopen /dev/tty");
	ttyfd =  open("/dev/tty", O_RDWR);
	set_terminal();
	sigwinch();
	input[0] = '\0';
	while ((exit_code = key(fgetc(stdin))) > 0)
		print_screen();
	print_screen();
	reset_terminal();
	close(ttyfd);
	free_lines();

	return exit_code;
}

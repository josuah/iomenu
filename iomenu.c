#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include "utf8.h"

#define CONTINUE  2   /* as opposed to EXIT_SUCCESS and EXIT_FAILURE */

#define  CTL(char) (char ^ 0x40)
#define  ALT(char) (char + 0x80)
#define  CSI(char) (char + 0x80 + 0x80)
#define  MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static struct winsize ws;
static struct termios termios;
static int            ttyfd;
static int            current = 0, offset = 0, prev = 0, next = 0;
static int            linec = 0,      matchc = 0;
static char         **linev = NULL, **matchv = NULL;
static char           input[LINE_MAX], formatted[LINE_MAX * 8];
static int            opt[128], rows = 0;
static char          *prompt = "";

static void
free_lines(void)
{
	if (linev) {
		for (; linec > 0; linec--)
			free(linev[linec - 1]);
		free(linev);
	}
	if (matchv)
		free(matchv);
}

static void
die(const char *s)
{
	tcsetattr(ttyfd, TCSANOW, &termios);
	close(ttyfd);
	free_lines();
	perror(s);
	exit(EXIT_FAILURE);
}

static void
set_terminal(void)
{
	struct termios new;

	/* save cursor postition */
	fputs("\033[s", stderr);

	/* save attributes to `termios` */
	if (tcgetattr(ttyfd, &termios) < 0 || tcgetattr(ttyfd, &new) < 0) {
		perror("tcgetattr");
		exit(EXIT_FAILURE);
	}

	/* change to raw mode */
	new.c_lflag &= ~(ICANON | ECHO | IGNBRK);
	tcsetattr(ttyfd, TCSANOW, &new);
}

static void
reset_terminal(void)
{
	int i;

	/* clear terminal */
	for (i = 0; i < rows + 1; i++)
		fputs("\r\033[K\n", stderr);

	/* reset cursor position */
	fputs("\033[u", stderr);

	/* set terminal back to normal mode */
	tcsetattr(ttyfd, TCSANOW, &termios);
}

static void
read_lines(void)
{
	int    size = 0;
	size_t len;

	do {
		if (linec >= size) {
			size += BUFSIZ;
			linev  = realloc(linev,  sizeof (char **) * size);
			matchv = realloc(matchv, sizeof (char **) * size);
			if (!linev || !matchv)
				die("realloc");
		}

		linev[linec] = malloc(LINE_MAX + 1);
		if (!(fgets(linev[linec], LINE_MAX, stdin))) {
			free(linev[linec]);
			break;
		}

		len = strlen(linev[linec]);
		if (len > 0 && linec[linev][len - 1] == '\n')
			linev[linec][len - 1] = '\0';

	} while (++linec, ++matchc);
}

static size_t
string_width(char *s)
{
	int width = 0;

	while (*s)
		if (*s++ == '\t')
			width += (width + 7) % 8;

	return width;
}

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

	while (printed < rows && i < matchc) {

		char *s = format(matchv[i], ws.ws_col - 1);

		if (opt['#'] && matchv[i][0] == '#')
			fprintf(stderr, "\n\033[1m\033[K %s\033[m",     s);
		else if (i == current)
			fprintf(stderr, "\n\033[30;47m\033[K %s\033[m", s);
		else
			fprintf(stderr, "\n\033[K %s",                  s);

		i++; printed++;
	}

	while (printed++ < rows)
		fputs("\n\033[K", stderr);
}

static void
print_screen(void)
{
	int cols = ws.ws_col - 1;

	fputs("\r\033[K", stderr);

	print_lines();
	fprintf(stderr, "\033[%dA\r", rows);

	if (*prompt) {
		format(prompt, cols - 2);
		fprintf(stderr, "\033[30;47m %s \033[m", formatted);
		cols -= strlen(formatted) + 2;
	}

	fputc(' ', stderr);
	fputs(format(input, cols), stderr);

	fflush(stderr);
}

static int
match_line(char *line, char **tokv, int tokc)
{
	if (opt['#'] && line[0] == '#')
		return 2;

	while (tokc-- > 0)
		if (strstr(line, tokv[tokc]) == NULL)
			return 0;

	return 1;
}

static void
move(signed int sign)
{
	int i;

	for (i = current + sign; 0 <= i && i < matchc; i += sign) {
		if (!opt['#'] || matchv[i][0] != '#') {
			current = i;
			break;
		}
	}
}

static void
move_page(signed int sign)
{
	int i = current - current % rows + rows * sign;

	if (!opt['l'])
		return;

	if (0 > i || i > matchc)
		return;

	current = i - 1;
	move(+1);
}

static void
filter(void)
{
	char **tokv = NULL, *s, buffer[sizeof (input)];
	int       tokc = 0, n = 0, i;

	current = offset = prev = next = 0;

	strcpy(buffer, input);

	for (s = strtok(buffer, " "); s; s = strtok(NULL, " "), tokc++) {

		if (tokc >= n) {
			tokv = realloc(tokv, ++n * sizeof (*tokv));

			if (tokv == NULL)
				die("realloc");
		}

		tokv[tokc] = s;
	}

	matchc = 0;
	for (i = 0; i < linec; i++)
		if (match_line(linev[i], tokv, tokc))
			matchv[matchc++] = linev[i];

	free(tokv);

	if (opt['#'] && matchv[current][0] == '#')
		move(+1);
}

static void
remove_word()
{
	int len = strlen(input) - 1, i;

	for (i = len; i >= 0 && isspace(input[i]); i--)
		input[i] = '\0';

	len = strlen(input) - 1;
	for (i = len; i >= 0 && !isspace(input[i]); i--)
		input[i] = '\0';

	filter();
}

static void
add_char(char key)
{
	int len = strlen(input);

	if (isprint(key)) {
		input[len]     = key;
		input[len + 1] = '\0';
	}

	filter();
}

static void
print_selection(void)
{
	if (opt['#']) {
		char **match = matchv + current;

		while (--match >= matchv) {
			if ((*match)[0] == '#') {
				fputs(*match, stdout);
				break;
			}
		}

		putchar('\t');
	}

	if (matchc == 0 || (opt['#'] && matchv[current][0] == '#')) {
		puts(input);
	} else {
		puts(matchv[current]);
	}

	fputs("\r\033[K", stderr);
}

static int
key(void)
{
	int key = fgetc(stdin);

top:
	switch (key) {

	case CTL('C'):
		return EXIT_FAILURE;

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
		if (fgetc(stdin) != '~') break;
		/* FALLTHROUGH */
	case ALT('v'):
		move_page(-1);
		break;

	case CSI('6'):  /* page down */
		if (fgetc(stdin) != '~') break;
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
		return EXIT_SUCCESS;

	case ALT('['):
		key = CSI(fgetc(stdin));
		goto top;

	case 033: /* escape / alt */
		key = ALT(fgetc(stdin));
		goto top;

	default:
		add_char((char) key);
	}

	return CONTINUE;
}

static void
sigwinch()
{
	if (ioctl(ttyfd, TIOCGWINSZ, &ws) < 0)
		die("ioctl");

	rows = MIN(opt['l'], ws.ws_row - 1);
	print_screen();

	signal(SIGWINCH, sigwinch);
}

static void
usage(void)
{
	fputs("iomenu [-#] [-l lines] [-p prompt]\n", stderr);
	exit(EXIT_FAILURE);
}

static void
parse_opt(int argc, char *argv[])
{
	memset(opt, 0, 128 * sizeof (int));

	opt['l'] = 255;
	for (argv++, argc--; argc > 0; argv++, argc--) {
		if (argv[0][0] != '-')
			usage();

		switch ((*argv)[1]) {

		case 'l':
			if (!--argc || sscanf(*++argv, "%d", &opt['l']) <= 0)
				usage();
			break;

		case 'p':
			if (!--argc)
				usage();
			prompt = *++argv;
			break;

		case '#':
			opt['#'] = 1;
			break;

		case 's':
			if (!--argc)
				usage();
			opt['s'] = (int) **++argv;
			break;

		default:
			usage();
		}
	}
}

int
main(int argc, char *argv[])
{
	int exit_code;

	parse_opt(argc, argv);

	read_lines();
	filter();

	if (!freopen("/dev/tty", "r", stdin) ||
	    !freopen("/dev/tty", "w", stderr))
		die("freopen");
	ttyfd =  open("/dev/tty", O_RDWR);

	set_terminal();
	sigwinch();

	input[0] = '\0';
	while ((exit_code = key()) == CONTINUE)
		print_screen();
	print_screen();

	reset_terminal();
	close(ttyfd);
	free_lines();

	return exit_code;
}

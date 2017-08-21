#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/ioctl.h>

#define CONTINUE  2   /* as opposed to EXIT_SUCCESS and EXIT_FAILURE */

#define  CONTROL(char) (char ^ 0x40)
#define  ALT(char) (char + 0x80)
#define  MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static struct winsize ws;
static struct termios termios;
static int    ttyfd;
static int    current = 0, offset = 0, prev = 0, next = 0;
static int    linec = 0,      matchc = 0;
static char **linev = NULL, **matchv = NULL;
static char   input[BUFSIZ], formatted[BUFSIZ * 8];
static int    opt[128];
static char  *prompt = "";

static void
freelines(void)
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
	freelines();
	perror(s);
	exit(EXIT_FAILURE);
}

static void
setterminal(void)
{
	struct termios new;

	/* save cursor postition */
	fputs("\x1b[s", stderr);

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
resetterminal(void)
{
	int i;

	/* clear terminal */
	for (i = 0; i < opt['l'] + 1; i++)
		fputs("\r\x1b[K\n", stderr);

	/* reset cursor position */
	fputs("\x1b[u", stderr);

	/* set terminal back to normal mode */
	tcsetattr(ttyfd, TCSANOW, &termios);
}

static void
readlines(void)
{
	char buffer[BUFSIZ];
	int size = 1 << 6;

	linev  = malloc(sizeof (char **) * size);
	matchv = malloc(sizeof (char **) * size);
	if (linev == NULL || matchv == NULL)
		die("malloc");

	linev[0] = matchv[0] = NULL;

	/* read the file into an array of lines as the lines never change */
	for (; fgets(buffer, sizeof buffer, stdin); linec++, matchc++) {
		int len = strlen(buffer);

		if (len > 0 && buffer[len - 1] == '\n')
			buffer[len - 1] = '\0';

		if (linec >= size) {
			size *= 2;
			linev  = realloc(linev,  sizeof (char **) * size);
			matchv = realloc(matchv, sizeof (char **) * size);
			if (linev == NULL || matchv == NULL)
				die("realloc");
		}

		linev[linec] = matchv[matchc] = malloc(len + 1);
		if (linev[linec] == NULL)
			die("malloc");

		strcpy(linev[linec], buffer);
	}
}

static char *
format(char *str, int cols)
{
	int i, j;

	for (i = j = 0; str[i] && j < cols; i++) {

		if (str[i] == '\t') {
			int t = 8 - j % 8;
			while (t-- > 0 && j < cols)
				formatted[j++] = ' ';

		} else if (isprint(str[i])) {
			formatted[j++] = str[i];

		} else {
			formatted[j++] = '?';
		}
	}

	formatted[j] = '\0';

	return formatted;
}

static void
printlines(int count)
{
	int printed = 0, i = current / count * count;

	while (printed < count && i < matchc) {

		if (opt['#'] && matchv[i][0] == '#') {
			char *s = format(matchv[i], ws.ws_col);
			fprintf(stderr, "\n\x1b[1m\x1b[K%s\x1b[m", s + 1);

		} else if (i == current) {
			char *s = format(matchv[i], ws.ws_col - 3);
			fprintf(stderr, "\n\x1b[30;47m\x1b[K   %s\x1b[m", s);

		} else {
			char *s = format(matchv[i], ws.ws_col - 3);
			fprintf(stderr, "\n\x1b[K   %s", s);
		}

		i++; printed++;
	}

	while (printed++ < count)
		fputs("\n\x1b[K", stderr);
}

static void
printscreen(void)
{
	int cols = ws.ws_col - 1, i;
	int count = MIN(opt['l'], ws.ws_row - 1);

	fputs("\r\x1b[K", stderr);

	printlines(count);
	fprintf(stderr, "\x1b[%dA\r", count);

	if (*prompt) {
		format(prompt, cols);
		fputs("\x1b[30;47m ", stderr);
		for (i = 0; formatted[i]; i++)
			fputc(formatted[i], stderr);
		fputs(" \x1b[m", stderr);
		cols -= strlen(formatted) + 1;
	}

	fputc(' ', stderr);
	fputs(format(input, cols), stderr);

	fflush(stderr);
}

static int
matchline(char *line, char **tokv, int tokc)
{
	if (opt['#'] && line[0] == '#')
		return 2;

	while (tokc-- > 0)
		if (strstr(line, tokv[tokc]) == NULL)
			return 0;

	return 1;
}

static void
move(signed int n)
{
	int i;

	for (i = current + n; 0 <= i && i < matchc; i += n) {
		if (!opt['#'] || matchv[i][0] != '#') {
			current = i;
			break;
		}
	}
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
		if (matchline(linev[i], tokv, tokc))
			matchv[matchc++] = linev[i];

	free(tokv);

	if (opt['#'] && matchv[current][0] == '#')
		move(+1);
}

static void
removeword()
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
addchar(char key)
{
	int len = strlen(input);

	if (isprint(key)) {
		input[len]     = key;
		input[len + 1] = '\0';
	}

	filter();
}

static void
printselection(void)
{
	/* header */
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

	/* input or selection */
	if (matchc == 0 || (opt['#'] && matchv[current][0] == '#')) {
		puts(input);
	} else {
		puts(matchv[current]);
	}

	fputs("\r\x1b[K", stderr);
}

static int
key(void)
{
	int key = fgetc(stdin);

top:
	switch (key) {

	case CONTROL('C'):
		return EXIT_FAILURE;

	case CONTROL('U'):
		input[0] = '\0';
		filter();
		break;

	case CONTROL('W'):
		removeword();
		break;

	case 127:
	case CONTROL('H'):  /* backspace */
		input[strlen(input) - 1] = '\0';
		filter();
		break;

	case CONTROL('N'):
		move(+1);
		break;

	case CONTROL('P'):
		move(-1);
		break;

	case CONTROL('V'):
		move(ws.ws_row - 1);
		break;

	case ALT('v'):
		move(-ws.ws_row + 1);
		break;

	case CONTROL('I'):  /* tab */
		if (linec > 0)
			strcpy(input, matchv[current]);
		filter();
		break;

	case CONTROL('J'):  /* enter */
	case CONTROL('M'):
		printselection();
		return EXIT_SUCCESS;

	case 0x1b: /* escape / alt */
		key = ALT(fgetc(stdin));
		goto top;

	default:
		addchar((char) key);
	}

	return CONTINUE;
}

static void
sigwinch()
{
	if (ioctl(ttyfd, TIOCGWINSZ, &ws) < 0)
		die("ioctl");
	printscreen();

	signal(SIGWINCH, sigwinch);
}

static void
usage(void)
{
	fputs("iomenu [-#] [-l lines] [-p prompt]\n", stderr);
	exit(EXIT_FAILURE);
}

static void
parseopt(int argc, char *argv[])
{
	memset(opt, 0, 128 * sizeof (int));

	opt['l'] = 255;
	for (argv++, argc--; argc > 0; argv++, argc--) {
		if (argv[0][0] != '-')
			usage();

		switch ((*argv)[1]) {
		case 'l':
			argv++; argc--;
			if (argc == 0 || sscanf(*argv, "%d", &opt['l']) <= 0)
				usage();
			break;

		case 'p':
			argv++; argc--;
			if (argc == 0)
				usage();
			prompt = *argv;
			break;

		case '#':
			opt['#'] = 1;
			break;

		default:
			usage();
		}
	}
}

int
main(int argc, char *argv[])
{
	int exitcode;

	parseopt(argc, argv);

	readlines();
	filter();

	if (!freopen("/dev/tty", "r", stdin) ||
	    !freopen("/dev/tty", "w", stderr))
		die("freopen");
	ttyfd =  open("/dev/tty", O_RDWR);

	setterminal();
	sigwinch();

	input[0] = '\0';
	while ((exitcode = key()) == CONTINUE)
		printscreen();

	resetterminal();
	close(ttyfd);
	freelines();

	return exitcode;
}

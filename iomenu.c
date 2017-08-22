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
static char           input[BUFSIZ], formatted[BUFSIZ * 8];
static int            opt[128], rows = 0;
static char          *prompt = "";

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
resetterminal(void)
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
printlines(void)
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
printscreen(void)
{
	int cols = ws.ws_col - 1;

	fputs("\r\033[K", stderr);

	printlines();
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
movepg(signed int sign)
{
	int i = current - current % rows + rows * sign;

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
		removeword();
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
		movepg(-1);
		break;

	case CSI('6'):  /* page down */
		if (fgetc(stdin) != '~') break;
		/* FALLTHROUGH */
	case CTL('V'):
		movepg(+1);
		break;

	case CTL('I'):  /* tab */
		if (linec > 0)
			strcpy(input, matchv[current]);
		filter();
		break;

	case CTL('J'):  /* enter */
	case CTL('M'):
		printselection();
		return EXIT_SUCCESS;

	case ALT('['):
		key = CSI(fgetc(stdin));
		goto top;

	case 033: /* escape / alt */
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

	rows = MIN(opt['l'], ws.ws_row - 1);
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
	printscreen();

	resetterminal();
	close(ttyfd);
	freelines();

	return exitcode;
}

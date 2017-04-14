#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/ioctl.h>


#define MARGIN    4   /* amount of space at the left and right of the feed */
#define CONTINUE  2   /* as opposed to EXIT_SUCCESS and EXIT_FAILURE */

#define CONTROL(char) (char ^ 0x40)
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static struct winsize ws;
static struct termios termios;
int   tty_fd;

static int     current = 0, offset = 0, prev = 0, next = 0;
static int     linec = 0,      matchc = 0;
static char  **linev = NULL, **matchv = NULL;
static char    input[BUFSIZ], formatted[BUFSIZ * 8];
static int     opt_tb = 0, opt_l = 255;
static char   *opt_p = "", opt_s = '\0';


static void
free_all(void)
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
	tcsetattr(tty_fd, TCSANOW, &termios);
	close(tty_fd);
	free_all();
	perror(s);
	exit(EXIT_FAILURE);
}


static void
set_terminal(void)
{
	struct termios new;

	/* get window size */
	if (ioctl(tty_fd, TIOCGWINSZ, &ws) < 0)
		die("ioctl");

	/* save cursor postition */
	fputs("\033[s", stderr);

	/* put cursor at the top / bottom */
	switch (opt_tb) {
	case 't': fputs("\033[H", stderr);                        break;
	case 'b': fprintf(stderr, "\033[%dH", ws.ws_row - opt_l); break;
	}

	/* save attributes to `termios` */
	if (tcgetattr(tty_fd, &termios) < 0 || tcgetattr(tty_fd, &new) < 0) {
		perror("tcgetattr");
		exit(EXIT_FAILURE);
	}

	/* change to raw mode */
	new.c_lflag &= ~(ICANON | ECHO | IGNBRK);
	tcsetattr(tty_fd, TCSANOW, &new);
}


static void
reset_terminal(void)
{
	extern struct termios termios;
	extern struct winsize ws;

	/* clear terminal */
	for (int i = 0; i < opt_l + 1; i++)
		fputs("\r\033[K\n", stderr);

	/* reset cursor position */
	fputs("\033[u", stderr);

	/* set terminal back to normal mode */
	tcsetattr(tty_fd, TCSANOW, &termios);
}


static void
read_lines(void)
{
	char buffer[BUFSIZ];
	int size = 1 << 6;

	linev  = malloc(sizeof (char **) * size);
	matchv = malloc(sizeof (char **) * size);
	if (linev == NULL || matchv == NULL)
		die("malloc");

	linev[0] = matchv[0] = NULL;

	/* read the file into an array of lines */
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


/*
 * Prepare a string for printing.
 */
static char *
format(char *str, int cols)
{
	extern char formatted[BUFSIZ * 8];

	int j = 0;

	for (int i = 0; str[i] && j < cols; i++) {

		if (str[i] == '\t') {
			for (int t = (j + 7) % 8 + 1; t > 0 && j < cols; t--)
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
print_lines(int count)
{
	extern int opt_l;
	extern char opt_s;

	int p = 0;  /* amount of lines printed */
	offset = current / count * count;

	for (int i = offset; p < count && i < matchc; p++, i++) {
		fputc('\n', stderr);

		fputs(i == current ? "\033[30;47m\033[K" : "\033[K", stderr);

		fprintf(stderr,
		        opt_s && matchv[i][0] == '#' ? "\033[1m%s" : "    %s",
		        format(matchv[i], ws.ws_col - 2 * MARGIN));

		fputs(" \033[m", stderr);
	}

	while (p++ < count)
		fputs("\n\033[K", stderr);
}


static void
print_screen(void)
{
	extern char formatted[BUFSIZ * 8];

	int cols = ws.ws_col - MARGIN;

	fputs("\r\033[K", stderr);

	/* items */
	int count = MIN(opt_l, ws.ws_row - 1);
	print_lines(count);
	fprintf(stderr, "\033[%dA", count);

	fputs("\r", stderr);

	/* prompt */
	if (opt_p[0] != '\0') {
		format(opt_p, cols);
		fputs("\033[30;47m ", stderr);
		for (int i = 0; formatted[i]; i++)
			fputc(formatted[i], stderr);
		fputs(" \033[m", stderr);
		cols -= strlen(formatted) + 1;
	}

	fputc(' ', stderr);

	/* input */
	fputs(format(input, cols), stderr);

	fflush(stderr);
}


static int
match_line(char *line, char **tokv, int tokc)
{
	if (opt_s && line[0] == opt_s)
		return 2;

	for (int i = 0; i < tokc; i++)
		if (strstr(line, tokv[i]) == NULL)
			return 0;

	return 1;
}


static void
move_line(signed int count)
{
	extern int    current;
	extern char **matchv;

	for (int i = current + count; 0 <= i && i < matchc; i += count) {
		if (!opt_s || matchv[i][0] != opt_s) {
			current = i;
			break;
		}
	}
}


static void
filter_lines(void)
{
	char **tokv = NULL, *s, buffer[sizeof (input)];
	int       tokc = 0, n = 0;

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
	for (int i = 0; i < linec; i++)
		if (match_line(linev[i], tokv, tokc))
			matchv[matchc++] = linev[i];

	free(tokv);

	if (opt_s && matchv[current][0] == opt_s)
		move_line(+1);
}


static void
remove_word_input()
{
	int len = strlen(input) - 1;

	for (int i = len; i >= 0 && isspace(input[i]); i--)
		input[i] = '\0';

	len = strlen(input) - 1;
	for (int i = len; i >= 0 && !isspace(input[i]); i--)
		input[i] = '\0';

	filter_lines();
}


static void
add_character(char key)
{
	int len = strlen(input);

	if (isprint(key)) {
		input[len]     = key;
		input[len + 1] = '\0';
	}

	filter_lines();
}


static void
print_selection(void)
{
	extern int    current;
	extern char **matchv, input[BUFSIZ];

	fputs("\r\033[K", stderr);

	if (matchc == 0 || (opt_s && matchv[current][0] == opt_s)) {
		puts(input);
	} else {
		puts(matchv[current]);
	}
}


static int
input_key(void)
{
	char key = fgetc(stdin);

	switch (key) {

	case CONTROL('C'):
		return EXIT_FAILURE;

	case CONTROL('U'):
		input[0] = '\0';
		filter_lines();
		break;

	case CONTROL('W'):
		remove_word_input();
		break;

	case 127:
	case CONTROL('H'):  /* backspace */
		input[strlen(input) - 1] = '\0';
		filter_lines();
		break;

	case CONTROL('N'):
		move_line(+1);
		break;

	case CONTROL('P'):
		move_line(-1);
		break;

	case CONTROL('I'):  /* tab */
		if (linec > 0)
			strcpy(input, matchv[current]);
		filter_lines();
		break;

	case CONTROL('Y'):
		print_selection();
		break;

	case CONTROL('J'):  /* enter */
	case CONTROL('M'):
		print_selection();
		return EXIT_SUCCESS;

	default:
		add_character(key);
	}

	return CONTINUE;
}


/*
 * Listen for the user input and call the appropriate functions.
 */
static int
input_get(void)
{
	int   exit_code;

	input[0] = '\0';

	print_screen();
	while ((exit_code = input_key()) == CONTINUE)
		print_screen();

	tcsetattr(tty_fd, TCSANOW, &termios);

	return exit_code;
}


static void
usage(void)
{
	fputs("usage: iomenu [-b] [-t] [-s] [-l lines] [-p prompt]\n", stderr);

	exit(EXIT_FAILURE);
}


int
main(int argc, char *argv[])
{
	extern char *opt_p;
	extern int opt_l;

	int exit_code;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || strlen(argv[i]) != 2)
			usage();

		switch (argv[i][1]) {
		case 'l':
			if (++i >= argc || sscanf(argv[i], "%d", &opt_l) <= 0)
				usage();
			break;

		case 't': opt_tb = 't'; break;
		case 'b': opt_tb = 'b'; break;

		case 'p':
			if (++i >= argc)
				usage();
			opt_p = argv[i];
			break;

		case 's':
			opt_s = '#';
			break;

		default:
			usage();
		}
	}

	setlocale(LC_ALL, "");
	read_lines();
	filter_lines();

	if (!freopen("/dev/tty", "r", stdin) ||
	    !freopen("/dev/tty", "w", stderr))
		die("freopen");
	tty_fd =  open("/dev/tty", O_RDWR);

	set_terminal();
	exit_code = input_get();  /* main loop */
	reset_terminal();
	close(tty_fd);
	free_all();

	return exit_code;
}

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>


#define OFFSET     5
#define CONTINUE   2  /* as opposed to EXIT_SUCCESS and EXIT_FAILURE */

#define CONTROL(char) (char ^ 0x40)
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


char     input[BUFSIZ];
size_t   current = 0, offset = 0, last = 0;
size_t   linec = 0,      matchc = 0;
char   **linev = NULL, **matchv = NULL;
char    *opt_prompt = "";
int      opt_lines = 0;


void
free_v(char **v, size_t c)
{
	for (; c > 0; c--)
		free(v[c - 1]);

	free(v);
}


void
die(const char *s)
{
	/* tcsetattr(STDIN_FILENO, TCSANOW, &termio_old); */
	fprintf(stderr, "%s\n", s);
	exit(EXIT_FAILURE);
}


struct termios
set_terminal(int tty_fd)
{
	struct termios termio_old;
	struct termios termio_new;

	if (tcgetattr(tty_fd, &termio_old) < 0)
		die("Can not get terminal attributes with tcgetattr().");

	termio_new          = termio_old;
	termio_new.c_lflag &= ~(ICANON | ECHO | IGNBRK);

	tcsetattr(tty_fd, TCSANOW, &termio_new);

	return termio_old;
}


void
read_lines(void)
{
	char buffer[BUFSIZ];
	size_t size = 1 << 6;

	linev  = malloc(sizeof (char **) * size);
	matchv = malloc(sizeof (char **) * size);
	if (linev == NULL || matchv == NULL)
		die("malloc");

	linev[0] = matchv[0] = NULL;

	/* read the file into an array of lines */
	for (; fgets(buffer, sizeof buffer, stdin); linec++, matchc++) {
		size_t len = strlen(buffer);

		if (len > 0 && buffer[len - 1] == '\n')
			buffer[len - 1] = '\0';

		if (linec >= size) {
			size *= 2;
			linev  = realloc(linev,  sizeof (char **) * size);
			matchv = realloc(matchv, sizeof (char **) * size);
			if (linev == NULL || matchv == NULL)
				die("realloc");
		}

		linev[linec] = matchv[matchc] = malloc(len);
		if (linev[linec] == NULL)
			die("malloc");

		strcpy(linev[linec], buffer);
	}
}


int
match_line(char *line, char **tokv, size_t tokc)
{
	for (size_t i = 0; i < tokc; i++)
		if (strstr(line, tokv[i]) == NULL)
			return 0;

	return 1;
}


void
filter_lines(void)
{
	char   **tokv = NULL, *s, buffer[sizeof (input)];
	size_t   tokc = 0, n = 0;

	/* tokenize input from space characters, this comes from dmenu */
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
	for (size_t i = 0; i < linec; i++)
		if (match_line(linev[i], tokv, tokc))
			matchv[matchc++] = linev[i];

	free(tokv);
}


size_t
print_string(char *str, size_t limit, int current)
{
	size_t i, col = 1;

	if (col >= limit)
		return 0;

	fputs(current  ? "\033[30;47m" : "", stderr);
	fputs(opt_lines ? "\033[K " : " ", stderr);

	for (i = 0; str[i] && col < limit; i++, col++)
		fputc(str[i], stderr);

	if (col < limit) {
		fputc(' ', stderr);
		col++;
	}

	fputs("\033[m", stderr);

	return col;
}


void
print_lines(size_t count, size_t cols)
{
	size_t p = 0;  /* amount of lines printed */
	offset = current / count * count;

	for (size_t i = offset; p < count && i < matchc; p++, i++) {
		fputc('\n', stderr);
		print_string(matchv[i], cols, i == current);
	}

	while (p++ <= count)
		fputs("\n\033[K", stderr);
}


void
print_columns(size_t cols)
{
	size_t col = 30;

	for (size_t i = offset; col < cols && i < matchc; i++)
		col += print_string(matchv[i], cols - col, i == current);
}


void
print_prompt(size_t cols)
{
	size_t limit = opt_lines ? cols : 30;

	fputc('\r', stderr);
	for (size_t i = 0; i < limit; i++)
		fputc(' ', stderr);

	fprintf(stderr, "\r%s %s", opt_prompt, input);
}


void
print_screen(int tty_fd)
{
	struct winsize w;
	size_t count;

	if (ioctl(tty_fd, TIOCGWINSZ, &w) < 0)
		die("could not get terminal size");

	count = MIN(opt_lines, w.ws_row - 2);

	fputs("\r\033[K", stderr);

	if (opt_lines) {
		print_lines(count, w.ws_col);
		fprintf(stderr, "\033[%ldA", count + 1);
	} else {
		fputs("\033[30C", stderr);
		print_columns(w.ws_col);
	}

	print_prompt(w.ws_col);
}


void
print_clear(size_t lines)
{
	for (size_t i = 0; i < lines + 1; i++)
		fputs("\r\033[K\n", stderr);
	fprintf(stderr, "\033[%ldA", lines + 1);
}


void
remove_word_input()
{
	size_t len = strlen(input) - 1;

	for (int i = len; i >= 0 && isspace(input[i]); i--)
		input[i] = '\0';

	len = strlen(input) - 1;
	for (int i = len; i >= 0 && !isspace(input[i]); i--)
		input[i] = '\0';
}


void
add_character(char key)
{
	size_t len = strlen(input);

	if (isprint(key)) {
		input[len]     = key;
		input[len + 1] = '\0';
	}

	filter_lines();

	current = 0;
}


/*
 * Send the selection to stdout.
 */
void
print_selection(void)
{
	fputs("\r\033[K", stderr);

	if (matchc > 0) {
		puts(matchv[current]);
	} else {
		puts(input);
	}
}


/*
 * Perform action associated with key
 */
int
input_key(FILE *tty_fp)
{
	char key = fgetc(tty_fp);

	switch (key) {

	case CONTROL('C'):
		print_clear(opt_lines);
		return EXIT_FAILURE;

	case CONTROL('U'):
		input[0] = '\0';
		current = 0;
		filter_lines();
		break;

	case CONTROL('W'):
		remove_word_input();
		filter_lines();
		break;

	case 127:
	case CONTROL('H'):  /* backspace */
		input[strlen(input) - 1] = '\0';
		filter_lines();
		current = 0;
		break;

	case CONTROL('N'):
		current += current < matchc - 1 ? 1 : 0;
		break;

	case CONTROL('P'):
		current -= current > 0 ? 1 : 0;
		break;

	case CONTROL('I'):  /* tab */
		if (linec > 0)
			strcpy(input, matchv[current]);
		filter_lines();
		break;

	case CONTROL('J'):
	case CONTROL('M'):  /* enter */
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
int
input_get(int tty_fd)
{
	FILE *tty_fp = fopen("/dev/tty", "r");
	int   exit_code;
	struct termios termio_old = set_terminal(tty_fd);

	input[0] = '\0';

	while ((exit_code = input_key(tty_fp)) == CONTINUE)
		print_screen(tty_fd);

	/* resets the terminal to the previous state. */
	tcsetattr(tty_fd, TCSANOW, &termio_old);

	fclose(tty_fp);

	return exit_code;
}


void
usage(void)
{
	fputs("usage: iomenu [-l lines] [-p prompt]\n", stderr);

	exit(EXIT_FAILURE);
}


int
main(int argc, char *argv[])
{
	int i, exit_code, tty_fd = open("/dev/tty", O_RDWR);

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || strlen(argv[i]) != 2)
			usage();

		switch (argv[i][1]) {
		case 'l':
			if (++i >= argc || sscanf(argv[i], "%d", &opt_lines) <= 0)
				usage();
			break;
		case 'p':
			if (++i >= argc)
				usage();
			opt_prompt = argv[i];
			break;
		default:
			usage();
		}
	}

	read_lines();

	print_screen(tty_fd);
	exit_code = input_get(tty_fd);

	print_clear(opt_lines);
	close(tty_fd);
	free_v(linev, linec);
	free(matchv);

	return exit_code;
}

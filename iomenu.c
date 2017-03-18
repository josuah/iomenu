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


struct line {
	char text[BUFSIZ];
	int  match;
};


char          input[BUFSIZ];
size_t        current = 0, matching = 0, linec = 0, offset = 0;
struct line **linev = NULL;
int           opt_lines = 0;
char         *opt_prompt = "";


void
free_linev(struct line **linev)
{
	for (; linec > 0; linec--)
		free(linev[linec - 1]);

	free(linev);
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
	extern struct line **linev;
	extern size_t linec, matching;

	char buffer[BUFSIZ];
	size_t size = 1 << 6;

	if (!(linev = malloc(sizeof(struct line *) * size)))
		die("malloc");
	linev[0] = NULL;

	/* read the file into an array of lines */
	for (; fgets(buffer, sizeof buffer, stdin); linec++, matching++) {
		size_t len = strlen(buffer);

		if (len > 0 && buffer[len - 1] == '\n')
			buffer[len - 1] = '\0';

		if (linec > size) {
			size *= 2;
			linev = realloc(linev, sizeof(struct line *) * size);

			if (linev == NULL)
				die("realloc");
		}

		if (!(linev[linec] = malloc(sizeof(struct line))))
			die("malloc");

		strcpy(linev[linec]->text, buffer);
		linev[linec]->match = 1;
	}
}


int
match_line(struct line *line, char **tokv, size_t tokc)
{
	for (size_t i = 0; i < tokc; i++)
		if (strstr(line->text, tokv[i]) == NULL)
			return 0;

	return 1;
}


void
filter_lines(void)
{
	char   **tokv = NULL;
	char    *s, buffer[sizeof(input)];
	size_t   n = 0, tokc = 0;

	/* tokenize input from space characters, this comes from dmenu */
	strcpy(buffer, input);
	for (s = strtok(buffer, " "); s; s = strtok(NULL, " "), tokc++) {

		if (tokc >= n) {
			tokv = realloc(tokv, ++n * sizeof(*tokv));

			if (tokv == NULL)
				die("realloc");
		}

		tokv[tokc] = s;
	}

	for (size_t i = 0, matching = 0; i < linec; i++)
		matching += linev[i]->match = match_line(linev[i], tokv, tokc);
}


int
matching_prev(int pos)
{
	for (int i = pos - 1; i >= 0; i--)
		if (linev[i]->match)
			return i;
	return pos;
}


int
matching_next(size_t pos)
{
	for (size_t i = pos + 1; i < linec; i++)
		if (linev[i]->match)
			return i;

	return pos;
}


void
print_line(size_t pos, const size_t cols)
{
	fprintf(stderr, pos == current ?
		"\n\033[30;47m\033[K%s\033[m" : "\n\033[K%s",
		linev[pos]->text
	);
}


void
print_lines(size_t count, size_t cols)
{
	size_t printed = 0;

	for (size_t i = offset; printed < count && i < linec; i++) {
		if (linev[i]->match) {
			print_line(i, cols);
			printed++;
		}
	}

	while (printed++ < count)
		fputs("\n\033[K", stderr);
}


int
print_column(size_t pos, size_t col, size_t cols)
{
	fputs(pos == current ? "\033[30;47m " : " ", stderr);

	for (size_t i = 0; col < cols ;) {
		int len = mblen(linev[pos]->text + i, BUFSIZ - i);

		if (len < 0) {
			i++;
			continue;
		} else if (len == 0) {
			break;
		}

		col += linev[pos]->text[i] = '\t' ? pos + 1 % 8 : 1;

		for (; len > 0; len--, i++)
			fputc(linev[pos]->text[i], stderr);
	}

	fputs(pos == current ? " \033[m" : " ", stderr);

	return col;
}


void
print_columns(size_t cols)
{
	size_t col = 20;

	for (size_t i = offset; col < cols; i++)
		col = print_column(i, col, cols);
}


void
print_prompt(size_t cols)
{
	size_t limit = opt_lines ? cols : 20;

	fputc('\r', stderr);
	for (size_t i = 0; i < limit; i++)
		fputc(' ', stderr);

	fprintf(stderr, "\r\033[1m%s%s\033[m", opt_prompt, input);
}


void
print_screen(int tty_fd)
{
	struct winsize w;
	size_t count;

	if (ioctl(tty_fd, TIOCGWINSZ, &w) < 0)
		die("could not get terminal size");

	count = MIN(opt_lines, w.ws_row - 2);

	if (opt_lines) {
		print_lines(count, w.ws_col);
		fprintf(stderr, "\033[%ldA", count);
	} else {
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

	current = linec == 0 || linev[0]->match ? 0 : matching_next(0);
}


/*
 * Send the selection to stdout.
 */
void
print_selection(void)
{
	fputs("\r\033[K", stderr);

	if (matching > 0) {
		puts(linev[current]->text);
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
	extern char input[];

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
		current = linec == 0 || linev[0]->match ? 0 : matching_next(0);
		break;

	case CONTROL('N'):
		current = matching_next(current);
		break;

	case CONTROL('P'):
		current = matching_prev(current);
		break;

	case CONTROL('I'):  /* tab */
		if (linec > 0)
			strcpy(input, linev[current]->text);
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
	free_linev(linev);

	return exit_code;
}

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


typedef struct Line {
	char *text;  /* sent as output and matched by input */
	int   match;   /* whether it matches buffer's input */
} Line;


Line **buffer;
size_t current = 0, matching = 0, total = 0;
char  *input = "";
int    opt_lines         = 30;
char  *opt_prompt        = "";


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

	/* set the terminal to send one key at a time. */

	/* get the terminal's state */
	if (tcgetattr(tty_fd, &termio_old) < 0)
		die("Can not get terminal attributes with tcgetattr().");

	/* create a new modified state by switching the binary flags */
	termio_new          = termio_old;
	termio_new.c_lflag &= ~(ICANON | ECHO | IGNBRK);

	/* apply this state to buffer[current] terminal now (TCSANOW) */
	tcsetattr(tty_fd, TCSANOW, &termio_new);

	return termio_old;
}


void
fill_buffer(void)
{
	extern Line **buffer;

	char  s[BUFSIZ];
	size_t size = 2 << 4;

	buffer = malloc(sizeof(Line) * size);

	input[0] = '\0';
	total = matching = 1;

	/* read the file into an array of lines */
	for (; fgets(s, BUFSIZ, stdin); total++, matching++) {
		if (total > size) {
			size *= 2;
			if (!realloc(buffer, sizeof(Line) * size))
				die("realloc");
		}

		/* empty input match everything */
		buffer[total]->matches = 1;
		buffer[total]->text[strlen(s) - 1] = '\0';
	}
}


void
free_buffer(Line **buffer)
{
	for (; total > 0; total--)
		free(buffer[total - 1]->text);

	free(buffer);
}


int
line_matches(Line *line, char **tokv, size_t tokc)
{
	for (size_t i = 0; i < tokc; i++)
		if (strstr(line->text, tokv[i]) != 0)
			return 0;

	return 1;
}


/*
 * If inc is 1, it will only check already matching lines.
 * If inc is 0, it will only check non-matching lines.
 */
void
filter_lines(int inc)
{
	char   **tokv = NULL;
	char    *s, buf[sizeof(input)];
	size_t   n = 0, tokc = 0;

	/* tokenize input from space characters, this comes from dmenu */
	strcpy(buf, input);
	for (s = strtok(buf, " "); s; s = strtok(NULL, " ")) {
		if (++tokc > n && !(tokv = realloc(tokv, ++n * sizeof(*tokv))))
			die("realloc");

		tokv[tokc - 1] = s;
	}

	/* match lines */
	matching = 0;
	for (size_t i = 0; i < total; i++) {

		if (input[0] && strcmp(input, buffer[i]->text) == 0) {
			buffer[i]->match = 1;

		} else if ((inc && buffer[i]->match) || (!inc && !buffer[i]->match)) {
			buffer[i]->match = line_matches(buffer[i], tokv, tokc);
			matching += buffer[i]->match;
		}
	}
}


int
matching_prev(int pos)
{
	for (size_t i = pos; i > 0; i--)
		if (buffer[i]->match)
			return i;
	return pos;
}


int
matching_next(size_t pos)
{
	for (size_t i = pos; i < total; i++)
		if (buffer[i]->match)
			return i;

	return pos;
}


int
matching_close(size_t pos)
{
	if (buffer[pos]->match)
		return pos;

	for (size_t i = 0; i + pos < total && i <= pos; i++) {
		if (buffer[pos - i]->match)
			return pos - i;

		if (buffer[pos + i]->match)
			return pos + i;
	}

	return pos;
}


void
draw_line(size_t pos, const size_t cols)
{
	fprintf(stderr, pos == current ? "\033[1;31m%s" : "%s", buffer[pos]->text);
}


void
draw_lines(size_t count, size_t cols)
{
	size_t i;
	for (i = MAX(current, 0); i < total && i < count;) {
		if (buffer[i]->match) {
			draw_line(i, cols);
			i++;
		}
	}

	/* continue up to the end of the screen clearing it */
	for (; i < count; i++)
		fputs("\n\033[K", stderr);
}


void
draw_prompt(int cols)
{
	fprintf(stderr, "\r\033[K\033[1m%7s %s", opt_prompt, input);
}


void
draw_screen( int tty_fd)
{
	struct winsize w;
	int count;

	if (ioctl(tty_fd, TIOCGWINSZ, &w) < 0)
		die("could not get terminal size");

	count = MIN(opt_lines, w.ws_row - 2);

	fputs("\n", stderr);
	draw_lines(count, w.ws_col);

	/* go up to the prompt position and update it */
	fprintf(stderr, "\033[%dA", count + 1);
	draw_prompt(w.ws_col);
}


void
draw_clear(int lines)
{
	int i;

	for (i = 0; i < lines + 1; i++)
		fputs("\r\033[K\n", stderr);
	fprintf(stderr, "\033[%dA", lines + 1);
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

	filter_lines(1);
	current = matching_close(current);
}


/*
 * Send the selection to stdout.
 */
void
print_selection(int return_input)
{
	fputs("\r\033[K", stderr);

	if (return_input || !matching) {
		puts(input);

	} else if (matching > 0) {
		puts(buffer[current]->text);
	}
}


/*
 * Perform action associated with key
 */
int
input_key(FILE *tty_fp)
{
	extern char *input;

	char key = fgetc(tty_fp);

	if (key == '\n') {
		print_selection(0);
		return EXIT_SUCCESS;
	}

	switch (key) {

	case CONTROL('C'):
		draw_clear(opt_lines);
		return EXIT_FAILURE;

	case CONTROL('U'):
		input[0] = '\0';
		current = 0;
		filter_lines(0);
		break;

	case CONTROL('W'):
		remove_word_input();
		filter_lines(0);
		break;

	case 127:
	case CONTROL('H'):  /* backspace */
		input[strlen(input) - 1] = '\0';
		filter_lines(0);
		current = matching_close(current);
		break;

	case CONTROL('N'):
		current = matching_next(current);
		break;

	case CONTROL('P'):
		matching_prev(current);
		break;

	case CONTROL('I'):  /* tab */
		strcpy(input, buffer[current]->text);
		filter_lines(1);
		break;

	case CONTROL('J'):
	case CONTROL('M'):  /* enter */
		print_selection(0);
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

	/* receive one character at a time from the terminal */
	struct termios termio_old = set_terminal(tty_fd);

	while ((exit_code = input_key(tty_fp)) == CONTINUE)
		draw_screen(tty_fd);

	/* resets the terminal to the previous state. */
	tcsetattr(tty_fd, TCSANOW, &termio_old);

	fclose(tty_fp);

	return exit_code;
}


void
usage(void)
{
	fputs("usage: iomenu [-n] [-p prompt] [-l lines]\n", stderr);

	exit(EXIT_FAILURE);
}


int
main(int argc, char *argv[])
{
	int i, exit_code, tty_fd = open("/dev/tty", O_RDWR);


	/* command line arguments */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || strlen(argv[i]) != 2)
			usage();

		switch (argv[i][1]) {
		case 'l':
			if (sscanf(argv[++i], "%d", &opt_lines) <= 0)
				die("wrong number format after -l");
			break;
		case 'p':
			if (++i >= argc)
				die("missing string after -p");
			opt_prompt = argv[i];
			break;
		default:
			usage();
		}
	}

	/* command line arguments */
	fill_buffer();

	/* set the interface */
	draw_screen(tty_fd);

	/* listen and interact to input */
	exit_code = input_get(tty_fd);

	draw_clear(opt_lines);

	/* close files descriptors and pointers, and free memory */
	close(tty_fd);
	free_buffer(buffer);

	return exit_code;
}

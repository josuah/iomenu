#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>


/*--- constants --------------------------------------------------------------*/

#define LINE_SIZE  1024
#define OFFSET     5
#define CONTINUE   2  /* as opposed to EXIT_SUCCESS and EXIT_FAILURE */


/*--- macros -----------------------------------------------------------------*/

#define CONTROL(char) (char ^ 0x40)
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


/*--- structures -------------------------------------------------------------*/

typedef struct Line {
	char *text;  /* sent as output and matched by input */
	int   match;   /* whether it matches buffer's input */
} Line;


/*--- variables --------------------------------------------------------------*/

Line **buffer;
int    current, matching, total;
int    opt_lines;
char  *opt_prompt, *input;


/*--- functions --------------------------------------------------------------*/

/*
 * Fill the buffer apropriately with the lines
 */
void
fill_buffer(void)
{
	extern Line **buffer;

	char  s[LINE_SIZE];
	size_t size = 1;

	buffer = malloc(sizeof(Line) * 2 << 4);

	input[0] = '\0';
	total = matching = 1;

	/* read the file into an array of lines */
	for (; fgets(s, LINE_SIZE, stdin); total++, matching++) {
		if (total > size) {
			size *= 2;
			if (!realloc(buffer, size * sizeof(Line)))
				die("realloc");
		}

		buffer[total]->text[strlen(s) - 1] = '\0';
		buffer[total]->match = 1;  /* empty input match everything */
	}
}


void
free_buffer(Line **buffer)
{
	Line *next = NULL;

	for (; total > 0; total--)
		free(buffer[total - 1]->text);

	free(buffer);
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
	for (int i = 0; i < total; i++) {

		if (input[0] && strcmp(input, buffer[i]->text) == 0) {
			buffer[i]->match = 1;

		} else if ((inc && buffer[i]->match) || (!inc && !buffer[i]->match)) {
			buffer[i]->match = match_line(buffer[i], tokv, tokc);
			matching += buffer[i]->match;
		}
	}
}


/*
 * Return whecher the line matches every string from tokv.
 */
int
match_line(Line *line, char **tokv, size_t tokc)
{
	for (int i = 0; i < tokc; i++)
		if (!!strstr(buffer[i]->text, tokv[i]))
			return 0;

	return 1;
}


/*
 * Seek the previous matching line, or NULL if none matches.
 */
Line *
matching_prev(int pos)
{
	for (; pos > 0 && !buffer[pos]->match; pos--);
	return buffer[pos];
}


/*
 * Seek the next matching line, or NULL if none matches.
 */
Line *
matching_next(int pos)
{
	for (; pos < total && !buffer[pos]->match; pos++);
	return buffer[pos];
}


/*
 * Print a line to stderr.
 */
void
draw_line(Line *line, const int cols)
{
	char  output[LINE_SIZE] = "\033[K";
	int n = 0;

	if (opt_line_numbers) {
		strcat(output, buffer[current] ? "\033[1;37m" : "\033[1;30m");
		sprintf(output + strlen(output), "%7d\033[m ", line->number);
	} else {
		strcat(output, buffer[current] ? "\033[1;31m      > " : "        ");
	}
	n += 8;

	/* highlight buffer[current] line */
	if (buffer[current])
		strcat(output, "\033[1;33m");

	/* content */
	strncat(output, line->content, cols - n);
	n += strlen(line->content);

	/* MAX with '1' as \033[0C still move 1 to the right */
	sprintf(output + strlen(output), "\033[%dC",
		MAX(1, 40 - n));
	n += MAX(1, 40 - n);
	strcat(output, "\033[m\n");

	fputs(output, stderr);

}


/*
 * Print all the lines from an array of pointer to lines.
 *
 * The total number oflines printed shall not excess 'count'.
 */
void
draw_lines( int count, int cols)
{
	Line *line = buffer[current];
	int i = 0;
	int j = 0;

	/* seek back from buffer[current] line to the first line to print */
	while (line && i < count - OFFSET) {
		i    = line->matches ? i + 1 : i;
		line = line->prev;
	}
	line = line ? line : first;

	/* print up to count lines that match the input */
	while (line && j < count) {
		if (line->matches) {
			draw_line(line, line == buffer[current], cols);
			j++;
		}

		line = line->next;
	}

	/* continue up to the end of the screen clearing it */
	for (; j < count; j++)
		fputs("\r\033[K\n", stderr);
}


/*
 * Update the screen interface and print all candidates.
 *
 * This also has to clear the previous lines.
 */
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


/*
 * Print the prompt, before the input, with the number of candidates that
 * match.
 */
void
draw_prompt(int cols)
{
	size_t  i;
	int     matching = matching;
	int     total    = total;

	/* for the '/' separator between the numbers */
	cols--;

	/* number of digits */
	for (i = matching; i; i /= 10, cols--);
	for (i = total;    i; i /= 10, cols--);
	cols -= !matching ? 1 : 0;  /* 0 also has one digit*/

	/* actual prompt */
	fprintf(stderr, "\r%-6s\033[K\033[1m>\033[m ", opt_prompt);
	cols -= 2 + MAX(strlen(opt_prompt), 6);

	/* input without overflowing terminal width */
	for (i = 0; i < strlen(input) && cols > 0; cols--, i++)
		fputc(input[i], stderr);

	/* save the cursor position at the end of the input */
	fputs("\033[s", stderr);

	/* grey */
	fputs("\033[1;30m", stderr);

	/* go to the end of the line */
	fprintf(stderr, "\033[%dC", cols);

	/* total match and line count at the end of the line */
	fprintf(stderr, "%d/%d", matching, total);

	/* restore cursor position at the end of the input */
	fputs("\033[m\033[u", stderr);

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


/*
 * Perform action associated with key
 */
int
input_key(FILE *tty_fp)
{
	extern char *input;

	char key = fgetc(tty_fp);

	if (key == '\n') {
		action_print_selection(0);
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
		action_jump(1);
		action_jump(-1);
		break;

	case CONTROL('W'):
		action_remove_word_input();
		filter_lines(0);
		break;

	case 127:
	case CONTROL('H'):  /* backspace */
		input[strlen(input) - 1] = '\0';
		filter_lines(0);
		action_jump(0);
		break;

	case CONTROL('N'):
		action_jump(1);
		break;

	case CONTROL('P'):
		action_jump(-1);
		break;

	case CONTROL('I'):  /* tab */
		strcpy(input, buffer[current]->text);
		filter_lines(1);
		break;

	case CONTROL('J'):
	case CONTROL('M'):  /* enter */
		action_print_selection(0);
		return EXIT_SUCCESS;

	case CONTROL('@'):  /* ctrl + space */
		action_print_selection(1);
		return EXIT_SUCCESS;

	case CONTROL('['):  /* escape */
		switch (fgetc(tty_fp)) {

		case 'O':  /* arrow keys */
			switch (fgetc(tty_fp)) {

			case 'A':  /* up */
				action_jump(-1);
				break;

			case 'B':  /* Down */
				action_jump(1);
				break;
			}
			break;

		case '[':  /* page control */
			key = fgetc(tty_fp);
			switch(fgetc(tty_fp)) {

			case '~':
				switch (key) {

				case '5': /* page up */
					action_jump(-10);
					break;

				case '6': /* page down */
					action_jump(10);
					break;
				}
				break;
			}
			break;
		}
		break;

	default:
		action_add_character(key);
	}

	return CONTINUE;
}


/*
 * Set the buffer[current] line to next/previous/any matching line.
 */
void
action_jump(int direction)
{
	Line * line   = buffer[current];
	Line * result = line;

	if (direction == 0 && !buffer[current]->match) {
		line   =               matching_next(current);
		line   = line ? line : matching_prev(current);
		result = line ? line : result;
	}

	for (; direction < 0 && line; direction++) {
		line   = matching_prev(line);
		result = line ? line : result;
	}

	for (; direction > 0 && line; direction--) {
		line   = matching_next(line);
		result = line ? line : result;
	}

	buffer[current] = result;
}


/*
 * Remove the last word from the buffer's input
 */
void
action_remove_word_input()
{
	size_t len = strlen(input) - 1;

	for (int i = len; i >= 0 && isspace(input[i]); i--)
		input[i] = '\0';

	len = strlen(input) - 1;
	for (int i = len; i >= 0 && !isspace(input[i]); i--)
		input[i] = '\0';
}


/*
 * Add a character to the buffer input and filter lines again.
 */
void
action_add_character(char key)
{
	size_t len = strlen(input);

	if (isprint(key)) {
		input[len]     = key;
		input[len + 1] = '\0';
	}

	filter_lines(1);

	action_jump(0);
}


/*
 * Send the selection to stdout.
 */
void
action_print_selection(int return_input)
{
	fputs("\r\033[K", stderr);

	if (return_input || !matching) {
		puts(input);

	} else if (matching > 0) {
		puts(buffer[current]->text);
	}
}


opt_line_numbers  = 0;
opt_print_number  = 0;
opt_lines         = 30;
opt_prompt        = "";


/*
 * Reset the terminal state and exit with error.
 */
void
die(const char *s)
{
	/* tcsetattr(STDIN_FILENO, TCSANOW, &termio_old); */
	fprintf(stderr, "%s\n", s);
	exit(EXIT_FAILURE);
}


/*
 * Set terminal to send one char at a time for interactive mode, and return the
 * last terminal state.
 */
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
usage(void)
{
	fputs("usage: iomenu [-n] [-p prompt] [-l lines]\n", stderr);

	exit(EXIT_FAILURE);
}


int
main(int argc, char *argv[])
{
	int i, exit_code, tty_fd = open("/dev/tty", O_RDWR);
	Buffer *buffer = NULL;
	Opt    *opt    = malloc(sizeof(Opt));


	/* command line arguments */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || strlen(argv[i]) != 2)
			usage();

		switch (argv[i][1]) {
		case 'n':
			opt_line_numbers = 1;
			break;
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
	buffer = fill_buffer();

	/* set the interface */
	draw_screen(tty_fd);

	/* listen and interact to input */
	exit_code = input_get(tty_fd);

	draw_clear(opt_lines);

	/* close files descriptors and pointers, and free memory */
	close(tty_fd);
	free(opt);
	free_buffer(buffer);

	return exit_code;
}

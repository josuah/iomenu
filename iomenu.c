#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>


#define OFFSET    30   /* in horizontal mode, amount of space kept for writing */
#define CONTINUE  2    /* as opposed to EXIT_SUCCESS and EXIT_FAILURE */

#define CONTROL(char) (char ^ 0x40)
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


static struct winsize ws;
static struct termios termios;
FILE *tty_fp = NULL;
int   tty_fd;

static char   input[BUFSIZ], formatted[BUFSIZ * 8];
static int    current = 0, offset = 0, prev = 0, next = 0;
static int    linec = 0,      matchc = 0;
static char **linev = NULL, **matchv = NULL;
static int    opt_l = 0;


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
	fclose(tty_fp);
	 close(tty_fd);
	free_all();
	perror(s);
	exit(EXIT_FAILURE);
}


static void
set_terminal(void)
{
	struct termios new;

	if (tcgetattr(tty_fd, &termios) < 0 || tcgetattr(tty_fd, &new) < 0) {
		perror("tcgetattr");
		exit(EXIT_FAILURE);
	}

	new.c_lflag &= ~(ICANON | ECHO | IGNBRK);

	tcsetattr(tty_fd, TCSANOW, &new);
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

		linev[linec] = matchv[matchc] = malloc(len);
		if (linev[linec] == NULL)
			die("malloc");

		strcpy(linev[linec], buffer);
	}
}


static int
match_line(char *line, char **tokv, int tokc)
{
	for (int i = 0; i < tokc; i++)
		if (strstr(line, tokv[i]) == NULL)
			return 0;

	return 1;
}


int
screen_width(char *str)
{
	int len = 0;

	for (int i = 0; str[i]; i++, len++)
		if (str[i] == '\t')
			len += (len + 7) % 8;

	return len;
}


static char *
format(char *str, int cols)
{
	int j = 0;

	for (int i = 0; str[i] && j < cols; i++) {

		if (str[i] == '\t') {
			for (int t = (j + 7) % 8 + 1; t > 0 && j < cols; t--)
				formatted[j++] = ' ';
		} else {
			formatted[j++] = str[i];
		}
	}

	formatted[j] = '\0';

	return formatted;
}


static void
print_string(char *str, int current)
{
	fputs(current   ? "\033[30;47m" : "", stderr);
	fputs(opt_l ? "\033[K " : " ", stderr);
	fprintf(stderr, "%s \033[m", format(str, ws.ws_col - 2));
}


static void
print_lines(int count)
{
	int p = 0;  /* amount of lines printed */
	offset = current / count * count;

	for (int i = offset; p < count && i < matchc; p++, i++) {
		fputc('\n', stderr);
		print_string(matchv[i], i == current);
	}

	while (p++ <= count)
		fputs("\n\033[K", stderr);
}


static int
prev_page(int pos, int cols)
{
	pos -= pos > 0 ? 1 : 0;
	for (int col = 0; pos > 0; pos--)
		if ((col += screen_width(matchv[pos]) + 2) > cols)
			return pos + 1;
	return pos;
}


static int
next_page(int pos, int cols)
{
	for (int col = 0; pos < matchc; pos++)
		if ((col += screen_width(matchv[pos]) + 2) > cols)
			return pos;
	return pos;
}


static void
print_columns(void)
{
	if (current < offset) {
		next = offset;
		offset = prev_page(offset, ws.ws_col - OFFSET - 4);

	} else if (current >= next) {
		offset = next;
		next = next_page(offset, ws.ws_col - OFFSET - 4);
	}

	fputs(offset > 0 ? "< " : "  ", stderr);

	for (int i = offset; i < next && i < matchc; i++)
		print_string(matchv[i], i == current);

	if (next < matchc)
		fprintf(stderr, "\033[%dC>", ws.ws_col - OFFSET);
}


static void
print_screen(void)
{
	int count;

	if (ioctl(tty_fd, TIOCGWINSZ, &ws) < 0)
		die("ioctl");

	count = MIN(opt_l, ws.ws_row - 2);

	fputs("\r\033[K", stderr);

	if (opt_l) {
		print_lines(count);
		fprintf(stderr, "\033[%dA", count + 1);

	} else {
		fputs("\033[30C", stderr);
		print_columns();
	}

	format(input, opt_l || matchc == 0 ? ws.ws_col : OFFSET - 3);
	fprintf(stderr, "\r %s", formatted);
}


static void
print_clear(int lines)
{
	for (int i = 0; i < lines + 1; i++)
		fputs("\r\033[K\n", stderr);
	fprintf(stderr, "\033[%dA", lines + 1);
}


static void
filter_lines(void)
{
	char **tokv = NULL, *s, buffer[sizeof (input)];
	int    tokc = 0, n = 0;

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
	fputs("\r\033[K", stderr);
	puts(matchc > 0 ? matchv[current] : input);
}


/*
 * Perform action associated with key
 */
static int
input_key(void)
{
	char key = fgetc(tty_fp);

	switch (key) {

	case CONTROL('C'):
		print_clear(opt_l);
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

	case CONTROL('Y'):
		print_selection();
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
static int
input_get(void)
{
	int   exit_code;

	input[0] = '\0';

	set_terminal();

	while ((exit_code = input_key()) == CONTINUE)
		print_screen();

	tcsetattr(tty_fd, TCSANOW, &termios);

	return exit_code;
}


static void
usage(void)
{
	fputs("usage: iomenu [-l lines]\n", stderr);

	exit(EXIT_FAILURE);
}


int
main(int argc, char *argv[])
{
	int exit_code;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || strlen(argv[i]) != 2)
			usage();

		switch (argv[i][1]) {
		case 'l':
			if (++i >= argc || sscanf(argv[i], "%d", &opt_l) <= 0)
				usage();
			break;
		default:
			usage();
		}
	}

	read_lines();

	tty_fp = fopen("/dev/tty", "r");
	tty_fd =  open("/dev/tty", O_RDWR);

	print_screen();
	exit_code = input_get();

	tcsetattr(tty_fd, TCSANOW, &termios);
	print_clear(opt_l);
	fclose(tty_fp);
	 close(tty_fd);
	free_all();

	return exit_code;
}

#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include <sys/ioctl.h>


#define OFFSET    30  /* in horizontal mode, amount of space kept for writing */
#define CONTINUE  2   /* as opposed to EXIT_SUCCESS and EXIT_FAILURE */

#define CONTROL(char) (char ^ 0x40)
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


static struct winsize ws;
static struct termios termios;
int   tty_fd;

static int       current = 0, offset = 0, prev = 0, next = 0;
static int       linec = 0,      matchc = 0;
static wchar_t **linev = NULL, **matchv = NULL;
static wchar_t   input[BUFSIZ], formatted[BUFSIZ * 8];
static int       opt_l = 0, opt_tb = 0;
static wchar_t   opt_p[BUFSIZ];


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
	fputws(L"\033[s", stderr);

	/* put cursor at the top / bottom */
	switch (opt_tb) {
	case 't': fputws(L"\033[H", stderr);                        break;
	case 'b': fwprintf(stderr, L"\033[%dH", ws.ws_row - opt_l); break;
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
		fputws(L"\r\033[K\n", stderr);

	/* reset cursor position */
	fputws(L"\033[u", stderr);

	/* set terminal back to normal mode */
	tcsetattr(tty_fd, TCSANOW, &termios);
}


static void
read_lines(void)
{
	wchar_t buffer[BUFSIZ];
	int size = 1 << 6;

	linev  = malloc(sizeof (wchar_t **) * size);
	matchv = malloc(sizeof (wchar_t **) * size);
	if (linev == NULL || matchv == NULL)
		die("malloc");

	linev[0] = matchv[0] = NULL;

	/* read the file into an array of lines */
	for (; fgetws(buffer, sizeof buffer, stdin); linec++, matchc++) {
		int len = wcslen(buffer);

		if (len > 0 && buffer[len - 1] == '\n')
			buffer[len - 1] = '\0';

		if (linec >= size) {
			size *= 2;
			linev  = realloc(linev,  sizeof (wchar_t **) * size);
			matchv = realloc(matchv, sizeof (wchar_t **) * size);
			if (linev == NULL || matchv == NULL)
				die("realloc");
		}

		linev[linec] = matchv[matchc] = malloc(len * sizeof (wchar_t));
		if (linev[linec] == NULL)
			die("malloc");

		wcscpy(linev[linec], buffer);
	}
}


static int
string_width(wchar_t *wcs)
{
	int len = 0;

	for (int i = 0; wcs[i]; i++, len++)
		if (wcs[i] == '\t')
			len += (len + 7) % 8;

	return len;
}


/*
 * Prepare a string for printing.
 */
static wchar_t *
format(wchar_t *wcs, int cols)
{
	int j = 0;

	for (int i = 0; wcs[i] && j < cols; i++) {

		if (wcs[i] == L'\t') {
			for (int t = (j + 7) % 8 + 1; t > 0 && j < cols; t--)
				formatted[j++] = L' ';
		} else {
			formatted[j++] = wcs[i];
		}
	}

	formatted[j] = L'\0';

	return formatted;
}


static void
print_string(wchar_t *wcs, int current)
{
	fputws(current   ? L"\033[30;47m" : L"", stderr);
	fputws(opt_l ? L"\033[K " : L" ", stderr);
	fputws(format(wcs, ws.ws_col - 2), stderr);
	fputws(L" \033[m", stderr);
}


static void
print_lines(int count)
{
	int p = 0;  /* amount of lines printed */
	offset = current / count * count;

	for (int i = offset; p < count && i < matchc; p++, i++) {
		fputwc(L'\n', stderr);
		print_string(matchv[i], i == current);
	}

	while (p++ <= count)
		fputws(L"\n\033[K", stderr);
}


static int
prev_page(int pos, int cols)
{
	pos -= pos > 0 ? 1 : 0;
	for (int col = 0; pos > 0; pos--)
		if ((col += string_width(matchv[pos]) + 2) > cols)
			return pos + 1;
	return pos;
}


static int
next_page(int pos, int cols)
{
	for (int col = 0; pos < matchc; pos++)
		if ((col += string_width(matchv[pos]) + 2) > cols)
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

	fputws(offset > 0 ? L"< " : L"  ", stderr);

	for (int i = offset; i < next && i < matchc; i++)
		print_string(matchv[i], i == current);

	if (next < matchc)
		fwprintf(stderr, L"\033[%dC>", ws.ws_col - OFFSET);
}


static void
print_screen(void)
{
	extern wchar_t formatted[BUFSIZ * 8];

	int cols = opt_l || matchc == 0 ? ws.ws_col : OFFSET - 3;

	fputws(L"\r\033[K", stderr);

	/* items */
	if (opt_l) {
		int count = MIN(opt_l, ws.ws_row - 2);
		print_lines(count);
		fwprintf(stderr, L"\033[%dA", count + 1);
	} else {
		fwprintf(stderr, L"\033[%dC", OFFSET);
		print_columns();
	}

	fputws(L"\r", stderr);

	/* prompt */
	if (opt_p[0] != '\0') {
		format(opt_p, cols);
		fputws(L"\033[30;47m ", stderr);
		for (int i = 0; formatted[i]; i++)
			fputwc(formatted[i], stderr);
		fputws(L" \033[m", stderr);
		cols -= wcslen(formatted) + 3;
	}

	fputwc(L' ', stderr);

	/* input */
	cols = (opt_l || matchc == 0 ? cols : MAX(OFFSET, cols) - 3);
	format(input, cols);
	fputws(formatted, stderr);

	fflush(stderr);
}


static int
match_line(wchar_t *line, wchar_t **tokv, int tokc)
{
	for (int i = 0; i < tokc; i++)
		if (wcsstr(line, tokv[i]) == NULL)
			return 0;

	return 1;
}


static void
filter_lines(void)
{
	wchar_t **tokv = NULL, *tok, *s, buffer[sizeof (input)];
	int       tokc = 0, n = 0;

	current = offset = prev = next = 0;

	wcscpy(buffer, input);

	for (s = wcstok(buffer, L" ", &tok); s; s = wcstok(NULL, L" ", &tok), tokc++) {

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
	int len = wcslen(input) - 1;

	for (int i = len; i >= 0 && iswspace(input[i]); i--)
		input[i] = '\0';

	len = wcslen(input) - 1;
	for (int i = len; i >= 0 && !iswspace(input[i]); i--)
		input[i] = '\0';

	filter_lines();
}


static void
add_character(char key)
{
	int len = wcslen(input);

	if (iswprint(key)) {
		input[len]     = key;
		input[len + 1] = '\0';
	}

	filter_lines();
}


static void
print_selection(void)
{
	fputws(L"\r\033[K", stderr);
	fputws(matchc > 0 ? matchv[current] : input, stdout);
	fputwc(L'\n', stdout);
}


static int
input_key(void)
{
	wchar_t key = fgetwc(stdin);

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
		input[wcslen(input) - 1] = '\0';
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
			wcscpy(input, matchv[current]);
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

	print_screen();
	while ((exit_code = input_key()) == CONTINUE)
		print_screen();

	tcsetattr(tty_fd, TCSANOW, &termios);

	return exit_code;
}


static void
usage(void)
{
	fputws(L"usage: iomenu [-b] [-t] [-l lines] [-p prompt]\n", stderr);

	exit(EXIT_FAILURE);
}


int
main(int argc, char *argv[])
{
	extern wchar_t opt_p[BUFSIZ];

	int exit_code;

	opt_p[0] = '\0';

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
			mbstowcs(opt_p, argv[i], BUFSIZ);
			break;

		default:
			usage();
		}
	}

	setlocale(LC_ALL, "");
	read_lines();

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

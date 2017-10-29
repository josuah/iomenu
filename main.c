#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "iomenu.h"
#include "main.h"
#include "buffer.h"
#include "control.h"
#include "display.h"

static struct termios termios;
static int            ttyfd;

struct winsize   ws;
char           **linev = NULL;
int              linec = 0;
char           **matchv = NULL;
int              matchc = 0;
char            *prompt = "";
char             input[LINE_MAX];
char             formatted[LINE_MAX * 8];
int              current = 0;
int              offset = 0;
int              rows = 0;
int              next = 0;
int              opt[128];

void
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

	/* save currentsor postition */
	fputs("\x1b[s", stderr);

	/* save attributes to `termios` */
	if (tcgetattr(ttyfd, &termios) < 0 || tcgetattr(ttyfd, &new) < 0) {
		perror("tcgetattr");
		exit(EXIT_FAILURE);
	}

	/* change to raw mode */
	new.c_lflag &= ~(ICANON | ECHO | IGNBRK | IEXTEN | ISIG);
	tcsetattr(ttyfd, TCSANOW, &new);
}

static void
reset_terminal(void)
{
	int i;

	/* clear terminal */
	for (i = 0; i < rows + 1; i++)
		fputs("\r\x1b[K\n", stderr);

	/* reset currentsor position */
	fputs("\x1b[u", stderr);

	tcsetattr(ttyfd, TCSANOW, &termios);
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
	read_stdin();
	filter();
	if (!freopen("/dev/tty", "r", stdin))
		die("freopen /dev/tty");
	if (!freopen("/dev/tty", "w", stderr))
		die("freopen /dev/tty");
	ttyfd =  open("/dev/tty", O_RDWR);
	set_terminal();
	sigwinch();
	input[0] = '\0';
	while ((exit_code = key(fgetc(stdin))) > 0)
		print_screen();
	print_screen();
	reset_terminal();
	close(ttyfd);
	free_lines();

	return exit_code;
}

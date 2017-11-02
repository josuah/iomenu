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

static	struct	termios termios;
static	int	ttyfd;

struct	winsize ws;
char	**linev = NULL;
int	  linec = 0;
char	**matchv = NULL;
int	  matchc = 0;
char	 *prompt = "";
char	  input[LINE_MAX];
char	  formatted[LINE_MAX * 8];
int	  current = 0;
int	  opt[128];

/*
 * Free the structures, reset the terminal state and exit with an error message.
 */
void
die(const char *s)
{
	tcsetattr(ttyfd, TCSANOW, &termios);
	close(ttyfd);
	free_lines();
	perror(s);
	exit(EXIT_FAILURE);
}

/*
 * Set terminal in raw mode.
 */
static void
set_terminal(void)
{
	struct	termios new;

	fputs("\x1b[s\x1b[?1049h\x1b[H", stderr);
	if (tcgetattr(ttyfd, &termios) < 0 || tcgetattr(ttyfd, &new) < 0) {
		perror("tcgetattr");
		exit(EXIT_FAILURE);
	}
	new.c_lflag &= ~(ICANON | ECHO | IGNBRK | IEXTEN | ISIG);
	tcsetattr(ttyfd, TCSANOW, &new);
}

/*
 * Take terminal out of raw mode.
 */
static void
reset_terminal(void)
{
	fputs("\x1b[u\033[?1049l", stderr);
	tcsetattr(ttyfd, TCSANOW, &termios);
}

/*
 * Redraw the whole screen on window resize.
 */
static void
sigwinch()
{
	extern	struct	winsize ws;

	if (ioctl(ttyfd, TIOCGWINSZ, &ws) < 0)
		die("ioctl");
	print_screen();
	signal(SIGWINCH, sigwinch);
}

static void
usage(void)
{
	fputs("usage: iomenu [-#] [-p prompt]\n", stderr);
	exit(EXIT_FAILURE);
}

/*
 * XXX: switch to getopt.
 */
static void
parse_opt(int argc, char *argv[])
{
	extern	char	*prompt;

	memset(opt, 0, 128 * sizeof (int));
	for (argv++, argc--; argc > 0; argv++, argc--) {
		if (argv[0][0] != '-')
			usage();
		switch ((*argv)[1]) {
		case 'p':
			if (!--argc)
				usage();
			prompt = *++argv;
			break;
		case '#':
			opt['#'] = 1;
			break;
		default:
			usage();
		}
	}
}

/*
 * Read stdin in a buffer, filling a table of lines, then re-open stdin to
 * /dev/tty for an interactive (raw) session to let the user filter and select
 * one line by searching words within stdin.  This was inspired from dmenu.
 */
int
main(int argc, char *argv[])
{
	extern	char	input[LINE_MAX];

	int	exit_code;

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

opt_line_numbers  = 0;
opt_print_number  = 0;
opt_lines         = 30;
opt_prompt        = "";

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include "iomenu.h"


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

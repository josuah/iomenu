#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "main.h"


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

	/* apply this state to current terminal now (TCSANOW) */
	tcsetattr(tty_fd, TCSANOW, &termio_new);

	return termio_old;
}


/*
 * Replace tab as a multiple of 8 spaces in a line.
 *
 * Allocates memory.
 */
char *
expand_tabs(char *line)
{
	size_t i, n;
	char *converted = malloc(sizeof(char) * (strlen(line) * 8 + 1));

	for (i = 0, n = 0; i < strlen(line); i++, n++) {
		if (line[i] == '\t') {
			for (; n == 0 || n % 8 != 0; n++)
				converted[n] = ' ';
			n--;
		} else {
			converted[n] = line[i];
		}
	}

	converted[n] = '\0';

	return converted;
}

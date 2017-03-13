#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "iomenu.h"


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

	/* get input char by char from the keyboard. */
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
		buffer[current] = first;
		filter_lines(0);
		action_jump(1);
		action_jump(-1);
		break;

	case CONTROL('W'):
		action_remove_word_input(buffer);
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
	extern char *input;
		break;

	case CONTROL('P'):
		action_jump(-1);
		break;

	case CONTROL('I'):  /* tab */
		strcpy(input, buffer[current]->content);
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

	if (direction == 0 && !buffer[current]->matches) {
		line   =               matching_next(buffer[current]);
		line   = line ? line : matching_prev(buffer[current]);
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
	size_t length = strlen(input) - 1;
	int i;

	for (i = length; i >= 0 && isspace(input[i]); i--)
		input[i] = '\0';

	length = strlen(input) - 1;
	for (i = length; i >= 0 && !isspace(input[i]); i--)
		input[i] = '\0';
}


/*
 * Add a character to the buffer input and filter lines again.
 */
void
action_add_character(char key)
{
	size_t length = strlen(input);

	if (isprint(key)) {
		input[length]     = key;
		input[length + 1] = '\0';
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
	Line *line = NULL;

	fputs("\r\033[K", stderr);

	if (return_input || !matching) {
		puts(input);

	} else if (matching > 0) {
		puts(buffer[current]->content);
	}
}

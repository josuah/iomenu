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
input_get(Buffer *buffer, int tty_fd, Opt *opt)
{
	FILE *tty_fp = fopen("/dev/tty", "r");
	int   exit_code;

	/* receive one character at a time from the terminal */
	struct termios termio_old = set_terminal(tty_fd);

	/* get input char by char from the keyboard. */
	while ((exit_code = input_key(tty_fp, buffer, opt)) == CONTINUE)
		draw_screen(buffer, tty_fd, opt);

	/* resets the terminal to the previous state. */
	tcsetattr(tty_fd, TCSANOW, &termio_old);

	fclose(tty_fp);

	return exit_code;
}


/*
 * Perform action associated with key
 */
int
input_key(FILE *tty_fp, Buffer *buffer, Opt *opt)
{
	char key = fgetc(tty_fp);

	if (key == opt->validate_key) {
		action_print_selection(buffer, 0, opt);
		return EXIT_SUCCESS;
	}

	switch (key) {

	case CONTROL('C'):
		draw_clear(opt->lines);
		return EXIT_FAILURE;

	case CONTROL('U'):
		buffer->input[0] = '\0';
		buffer->current  = buffer->first;
		filter_lines(buffer, 0);
		action_jump(buffer, 1);
		action_jump(buffer, -1);
		break;

	case CONTROL('W'):
		action_remove_word_input(buffer);
		filter_lines(buffer, 0);
		break;

	case 127:
	case CONTROL('H'):  /* backspace */
		buffer->input[strlen(buffer->input) - 1] = '\0';
		filter_lines(buffer, 0);
		action_jump(buffer, 0);
		break;

	case CONTROL('N'):
		action_jump(buffer, 1);
		break;

	case CONTROL('P'):
		action_jump(buffer, -1);
		break;

	case CONTROL('I'):  /* tab */
		strcpy(buffer->input, buffer->current->content);
		filter_lines(buffer, 1);
		break;

	case CONTROL('J'):
	case CONTROL('M'):  /* enter */
		action_print_selection(buffer, 0, opt);
		return EXIT_SUCCESS;

	case CONTROL('@'):  /* ctrl + space */
		action_print_selection(buffer, 1, opt);
		return EXIT_SUCCESS;

	case CONTROL('['):  /* escape */
		switch (fgetc(tty_fp)) {

		case 'O':  /* arrow keys */
			switch (fgetc(tty_fp)) {

			case 'A':  /* up */
				action_jump(buffer, -1);
				break;

			case 'B':  /* Down */
				action_jump(buffer, 1);
				break;
			}
			break;

		case '[':  /* page control */
			key = fgetc(tty_fp);
			switch(fgetc(tty_fp)) {

			case '~':
				switch (key) {

				case '5': /* page up */
					action_jump(buffer, -10);
					break;

				case '6': /* page down */
					action_jump(buffer, 10);
					break;
				}
				break;
			}
			break;
		}
		break;

	default:
		action_add_character(buffer, key);
	}

	return CONTINUE;
}


/*
 * Set the current line to next/previous/any matching line.
 */
void
action_jump(Buffer *buffer, int direction)
{
	Line * line   = buffer->current;
	Line * result = line;

	if (direction == 0 && !buffer->current->matches) {
		line   =               matching_next(buffer->current);
		line   = line ? line : matching_prev(buffer->current);
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

	buffer->current = result;
}


/*
 * Remove the last word from the buffer's input
 */
void
action_remove_word_input(Buffer *buffer)
{
	size_t length = strlen(buffer->input) - 1;
	int i;

	for (i = length; i >= 0 && isspace(buffer->input[i]); i--)
		buffer->input[i] = '\0';

	length = strlen(buffer->input) - 1;
	for (i = length; i >= 0 && !isspace(buffer->input[i]); i--)
		buffer->input[i] = '\0';
}


/*
 * Add a character to the buffer input and filter lines again.
 */
void
action_add_character(Buffer *buffer, char key)
{
	size_t length = strlen(buffer->input);

	if (isprint(key)) {
		buffer->input[length]     = key;
		buffer->input[length + 1] = '\0';
	}

	filter_lines(buffer, 1);

	action_jump(buffer, 0);
}


/*
 * Send the selection to stdout.
 */
void
action_print_selection(Buffer *buffer, int return_input, Opt *opt)
{
	Line *line = NULL;

	fputs("\r\033[K", stderr);

	if (opt->print_number) {
		if (buffer->matching > 0)
			printf("%d\n", buffer->current->number);

	} else if (return_input || !buffer->matching) {
		puts(buffer->input);

	} else if (buffer->matching > 0) {
		puts(buffer->current->content);
	}
}

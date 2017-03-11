#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>

#include "main.h"


/*
 * Print a line to stderr.
 */
void
draw_line(Line *line, int current, const int cols, Opt *opt)
{
	char *content = expand_tabs(line->content);
	char *comment = expand_tabs(line->comment);
	char  output[LINE_SIZE * sizeof(char)] = "\033[K";
	int n = 0;

	if (opt->line_numbers && !line->header) {
		strcat(output, current ? "\033[1;37m" : "\033[1;30m");
		sprintf(output + strlen(output), "%7d\033[m ", line->number);
	} else {
		strcat(output, current ? "\033[1;31m      > " : "        ");
	}
	n += 8;


	/* highlight current line */
	if (current)
		strcat(output, "\033[1;33m");

	/* content */
	strncat(output, content, cols - n);
	n += strlen(content);

	/* align comment */
	if (!line->header && line->comment[0] != '\0') {
		/* MAX with '1' as \033[0C still move 1 to the right */
		sprintf(output + strlen(output), "\033[%dC",
			MAX(1, 40 - n));
		n += MAX(1, 40 - n);
	} else if (line->header)

	/* comment */
	strcat(output, "\033[1;30m");
	strncat(output, comment, cols - n);
	n += strlen(comment);

	strcat(output, "\033[m\n");

	fputs(output, stderr);

	free(content);
	free(comment);
}


/*
 * Print all the lines from an array of pointer to lines.
 *
 * The total number oflines printed shall not excess 'count'.
 */
void
draw_lines(Buffer *buffer, int count, int cols, Opt *opt)
{
	Line *line = buffer->current;
	int i = 0;
	int j = 0;

	/* seek back from current line to the first line to print */
	while (line && i < count - OFFSET) {
		i    = line->matches ? i + 1 : i;
		line = line->prev;
	}
	line = line ? line : buffer->first;

	/* print up to count lines that match the input */
	while (line && j < count) {
		if (line->matches) {
			draw_line(line, line == buffer->current, cols, opt);
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
draw_screen(Buffer *buffer, int tty_fd, Opt *opt)
{
	struct winsize w;
	int count;

	if (ioctl(tty_fd, TIOCGWINSZ, &w) < 0)
		die("could not get terminal size");

	count = MIN(opt->lines, w.ws_row - 2);

	fputs("\n", stderr);
	draw_lines(buffer, count, w.ws_col, opt);

	/* go up to the prompt position and update it */
	fprintf(stderr, "\033[%dA", count + 1);
	draw_prompt(buffer, w.ws_col, opt);
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
draw_prompt(Buffer *buffer, int cols, Opt *opt)
{
	size_t  i;
	int     matching = buffer->matching;
	int     total    = buffer->total;
	char   *input    = expand_tabs(buffer->input);
	char   *suggest  = expand_tabs(buffer->current->content);

	/* for the '/' separator between the numbers */
	cols--;

	/* number of digits */
	for (i = matching; i; i /= 10, cols--);
	for (i = total;    i; i /= 10, cols--);
	cols -= !matching ? 1 : 0;  /* 0 also has one digit*/

	/* actual prompt */
	fprintf(stderr, "\r%-6s\033[K\033[1m>\033[m ", opt->prompt);
	cols -= 2 + MAX(strlen(opt->prompt), 6);

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

	free(input);
	free(suggest);
}

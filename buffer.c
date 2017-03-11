#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"


/*
 * Fill the buffer apropriately with the lines and headers.
 */
Buffer *
fill_buffer(char *separator)
{
	/* fill buffer with string */
	char    s[LINE_SIZE];
	Buffer *buffer = malloc(sizeof(Buffer));
	FILE   *fp     = stdin;
	int     l;

	if (!fp)
		die("Can not open file for reading.");

	buffer->input[0] = '\0';
	buffer->total = buffer->matching = 1;

	/* empty line in case no line come from stdin */
	buffer->first = buffer->current = malloc(sizeof(Line));
	buffer->first->content = buffer->first->comment = "";
	buffer->first->next = buffer->first->prev = NULL;
	buffer->last = NULL;

	/* read the file into a doubly linked list of lines */
	for (l = 1; fgets(s, LINE_SIZE, fp); buffer->total++, l++) {
		buffer->last = add_line(buffer, l, s, separator, buffer->last);

		l = buffer->last->header ? 0 : l;
	}

	/* prevent initial current line to be a header */
	buffer->current = buffer->first;
	while (buffer->current->next && buffer->current->header)
		buffer->current = buffer->current->next;

	return buffer;
}


/*
 * Add a line to the end of the current buffer.
 */
Line *
add_line(Buffer *buffer, int number, char *s, char *separator, Line *prev)
{
	/* allocate new line */
	buffer->last          = new_line(s, separator);
	buffer->last->number  = number;
	buffer->last->matches = 1;  /* matches by default */
	buffer->matching++;

	/* interlink with previous line if exists */
	if (prev) {
		prev->next = buffer->last;
		buffer->last->prev = prev;
	} else {
		buffer->first = buffer->last;
	}

	return buffer->last;
}


/*
 * Parse the line content to determine if it is a header and identify the
 * separator if any.
 */
Line *
new_line(char *s, char *separator)
{
	Line *line = malloc(sizeof(Line));
	char *sep  = separator ? strstr(s, separator) : NULL;
	int   pos  = sep ? (int) (sep - s) : (int) strlen(s) - 1;

	/* header is when separator is the first character of the line */
	line->header = (sep == s);

	/* strip trailing newline */
	s[strlen(s) - 1] = '\0';

	/* fill line->content */
	line->content = malloc((pos + 1) * sizeof(char));
	strncpy(line->content, s, pos);

	/* fill line->comment */
	line->comment = malloc((strlen(s) - pos) * sizeof(char));
	if (sep) {
		strcpy(line->comment, s + pos + strlen(separator));
	}

	/* strip trailing whitespaces from line->content */
	for (pos--; pos > 0 && isspace(line->content[pos]); pos--)
		line->content[pos] = '\0';

	/* strip leading  whitespaces from line->comment */
	for (pos = 0; isspace(line->comment[pos]); pos++);
	line->comment += pos;

	return line;
}


/*
 * Free the buffer, also recursing the doubly linked list.
 */
void
free_buffer(Buffer *buffer)
{
	Line *next = NULL;

	while (buffer->first) {
		next = buffer->first->next;

		free(buffer->first);

		buffer->first = next;
	}

	free(buffer);
}


/*
 * Set the line->matching state according to the return value of match_line,
 * and buffer->matching to number of matching candidates.
 *
 * The incremental parameter sets whether check already matching or
 * non-matching lines only.  This is for performance concerns.
 */
void
filter_lines(Buffer *buffer, int inc)
{
	Line    *line = buffer->first;
	char   **tokv = NULL;
	char    *s, buf[sizeof buffer->input];
	size_t   n = 0, tokc = 0;

	/* tokenize input from space characters, this comes from dmenu */
	strcpy(buf, buffer->input);
	for (s = strtok(buf, " "); s; s = strtok(NULL, " ")) {
		if (++tokc > n && !(tokv = realloc(tokv, ++n * sizeof(*tokv))))
			die("cannot realloc memory for tokv\n");

		tokv[tokc - 1] = s;
	}

	/* match lines */
	buffer->matching = 0;
	while (line) {
		if (buffer->input[0] && !strcmp(buffer->input, line->content)) {
			line->matches = 1;
			buffer->current = line;
		} else if ((inc && line->matches) || (!inc && !line->matches)) {
			line->matches     = match_line(line, tokv, tokc);
			buffer->matching += line->header ? 0 : line->matches;
		}

		line = line->next;
	}
}


/*
 * Return whecher the line matches every string from tokv.
 */
int
match_line(Line *line, char **tokv, size_t tokc)
{
	size_t i, match = 1, offset = 0;

	if (line->header)
		return 1;

	for (i = 0; i < tokc && match; i++)
		match = !!strstr(line->content + offset, tokv[i]);

	return match;
}


/*
 * Seek the previous matching line, or NULL if none matches.
 */
Line *
matching_prev(Line *line)
{
	while ((line = line->prev) && (!line->matches || line->header));
	return line;
}


/*
 * Seek the next matching line, or NULL if none matches.
 */
Line *
matching_next(Line *line)
{
	while ((line = line->next) && (!line->matches || line->header));
	return line;
}

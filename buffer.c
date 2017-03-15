#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iomenu.h"


/*
 * Fill the buffer apropriately with the lines
 */
void
fill_buffer(void)
{
	extern Line **buffer;

	char  s[LINE_SIZE];
	size_t size = 1;

	buffer = malloc(sizeof(Line) * 2 << 4);

	input[0] = '\0';
	total = matching = 1;

	/* read the file into an array of lines */
	for (; fgets(s, LINE_SIZE, stdin); total++, matching++) {
		if (total > size) {
			size *= 2;
			if (!realloc(buffer, size * sizeof(Line)))
				die("realloc");
		}

		buffer[total]->text[strlen(s) - 1] = '\0';
		buffer[total]->match = 1;  /* empty input match everything */
	}
}


void
free_buffer(Line **buffer)
{
	Line *next = NULL;

	for (; total > 0; total--)
		free(buffer[total - 1]->text);

	free(buffer);
}


/*
 * If inc is 1, it will only check already matching lines.
 * If inc is 0, it will only check non-matching lines.
 */
void
filter_lines(int inc)
{
	char   **tokv = NULL;
	char    *s, buf[sizeof(input)];
	size_t   n = 0, tokc = 0;

	/* tokenize input from space characters, this comes from dmenu */
	strcpy(buf, input);
	for (s = strtok(buf, " "); s; s = strtok(NULL, " ")) {
		if (++tokc > n && !(tokv = realloc(tokv, ++n * sizeof(*tokv))))
			die("realloc");

		tokv[tokc - 1] = s;
	}

	/* match lines */
	matching = 0;
	for (int i = 0; i < total; i++) {

		if (input[0] && strcmp(input, buffer[i]->text) == 0) {
			buffer[i]->match = 1;

		} else if ((inc && buffer[i]->match) || (!inc && !buffer[i]->match)) {
			buffer[i]->match = match_line(buffer[i], tokv, tokc);
			matching += buffer[i]->match;
		}
	}
}


/*
 * Return whecher the line matches every string from tokv.
 */
int
match_line(Line *line, char **tokv, size_t tokc)
{
	for (int i = 0; i < tokc; i++)
		if (!!strstr(buffer[i]->text, tokv[i]))
			return 0;

	return 1;
}


/*
 * Seek the previous matching line, or NULL if none matches.
 */
Line *
matching_prev(int pos)
{
	for (; pos > 0 && !buffer[pos]->match; pos--);
	return buffer[pos];
}


/*
 * Seek the next matching line, or NULL if none matches.
 */
Line *
matching_next(int pos)
{
	for (; pos < total && !buffer[pos]->match; pos++);
	return buffer[pos];
}

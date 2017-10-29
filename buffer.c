#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "iomenu.h"
#include "buffer.h"
#include "main.h"
#include "control.h"

static char *
read_line(FILE *fp)
{
	char *line;
	size_t len;

	line = malloc(LINE_MAX + 1);
	if (!(fgets(line, LINE_MAX, fp))) {
		free(line);
		return NULL;
	}

	len = strlen(line);
	if (len > 0 && line[len - 1] == '\n')
		line[len - 1] = '\0';

	return (line);
}

static int
match_line(char *line, char **tokv, int tokc)
{
	if (opt['#'] && line[0] == '#')
		return 2;
	while (tokc-- > 0)
		if (strstr(line, tokv[tokc]) == NULL)
			return 0;

	return 1;
}

void
free_lines(void)
{
	extern char **linev;

	if (linev) {
		for (; linec > 0; linec--)
			free(linev[linec - 1]);
		free(linev);
	}
	if (matchv)
		free(matchv);
}

void
read_stdin(void)
{
	int size = 0;
	extern char **linev;

	while (1) {
		if (linec >= size) {
			size += BUFSIZ;
			linev = realloc(linev,  sizeof (char **) * size);
			matchv = realloc(matchv, sizeof (char **) * size);
			if (!linev || !matchv)
				die("realloc");
		}
		if ((linev[linec] = read_line(stdin)) == NULL)
			break;
		linec++;
		matchc++;
	}
}

void
filter(void)
{
	int tokc = 0;
	int n = 0;
	int i;
	char **tokv = NULL;
	char *s;
	char buffer[sizeof (input)];
	extern char **linev;
	extern current;

	current = offset = next = 0;
	strcpy(buffer, input);
	for (s = strtok(buffer, " "); s; s = strtok(NULL, " "), tokc++) {
		if (tokc >= n) {
			tokv = realloc(tokv, ++n * sizeof (*tokv));
			if (tokv == NULL)
				die("realloc");
		}
		tokv[tokc] = s;
	}
	matchc = 0;
	for (i = 0; i < linec; i++)
		if (match_line(linev[i], tokv, tokc))
			matchv[matchc++] = linev[i];
	free(tokv);
	if (opt['#'] && matchv[current][0] == '#')
		move(+1);
}

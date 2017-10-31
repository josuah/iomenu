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
	extern char **linev;
	extern int    current;
	int           tokc;
	int           n;
	char        **tokv = NULL;
	char         *s;
	char          buf[sizeof (input)];

	current = offset = next = 0;
	strcpy(buf, input);
	tokc = 0;
	n = 0;
	s = strtok(buf, " ");
	while (s) {
		if (tokc >= n) {
			tokv = realloc(tokv, ++n * sizeof (*tokv));
			if (tokv == NULL)
				die("realloc");
		}
		tokv[tokc] = s;
		s = strtok(NULL, " ");
		tokc++;
	}
	matchc = 0;
	for (n = 0; n < linec; n++)
		if (match_line(linev[n], tokv, tokc))
			matchv[matchc++] = linev[n];
	free(tokv);
	if (opt['#'] && matchv[current][0] == '#')
		move(+1);
}

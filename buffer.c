#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "iomenu.h"
#include "buffer.h"
#include "main.h"
#include "control.h"

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
		free(linev[0]);
		free(linev);
	}
	if (matchv)
		free(matchv);
}

void
split_lines(char *buf)
{
	extern char **linev;
	extern int linec;
	char *b;
	char **lv;
	char **mv;

	linec = 0;
	b = buf;
	while ((b = strchr(b + 1, '\n')))
		linec++;
	if (!linec)
		linec = 1;
	if (!(lv = linev = calloc(linec, sizeof (char **))))
		die("calloc");
	if (!(mv = matchv = calloc(linec, sizeof (char **)))) {
		free(linev);
		die("calloc");
	}
	*mv = *lv = b = buf;
	while ((b = strchr(b, '\n'))) {
		*b++ = '\0';
		mv++, lv++;
		*mv = *lv = b;
	}
}

void
read_stdin(void)
{
	size_t size = BUFSIZ;
	size_t len;
	size_t off;
	char *buf;
	char *b;

	off = 0;
	buf = malloc(size);
	while ((len = read(STDIN_FILENO, buf + off, size - off)) > 0) {
		off += len;
		if (off >= size >> 1) {
			size <<= 1;
			if (!(b = realloc(buf, size + 1))) {
				free(buf);
				die("realloc");
			}
			buf = b;
		}
	}
	buf[off] = '\0';
	split_lines(buf);
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

	current = 0;
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

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

/*
 * Keep the line if it match every token (in no particular order, and allowed to
 * be overlapping).
 */
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

/*
 * As we use a single buffer for the whole stdin, we only need to free it once
 * and it will free all the lines.
 */
void
free_lines(void)
{
	extern	char	**linev;
	extern	char	**matchv;

	if (linev) {
		free(linev[0]);
		free(linev);
	}
	if (matchv)
		free(matchv);
}

/*
 * Split a buffer into an array of lines, without allocating memory for every
 * line, but using the input buffer and replacing characters.
 */
void
split_lines(char *buf)
{
	extern	char	**linev;
	extern	char	**matchv;
	extern	int	  linec;

	char	 *b;
	char	**lv;
	char	**mv;

	linec = 0;
	b = buf;
	while ((b = strchr(b + 1, '\n')))
		linec++;
	if (!linec)
		linec = 1;
	if (!(lv = linev = calloc(linec + 1, sizeof (char **))))
		die("calloc");
	if (!(mv = matchv = calloc(linec + 1, sizeof (char **)))) {
		free(linev);
		die("calloc");
	}
	*mv = *lv = b = buf;
	while ((b = strchr(b, '\n'))) {
		*b = '\0';
		mv++, lv++;
		*mv = *lv = ++b;
	}
}

/*
 * Read stdin in a single malloc-ed buffer, realloc-ed to twice its size every
 * time the previous buffer is filled.
 */
void
read_stdin(void)
{
	size_t	 size;
	size_t	 len;
	size_t	 off;
	char	*buf;
	char	*b;

	size = BUFSIZ;
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

/*
 * First split input into token, then match every token independently against
 * every line.  The matching lines fills matchv.
 */
void
filter(void)
{
	extern	char	**linev;
	extern	char	**matchv;
	extern	int	  linec;
	extern	int	  matchc;
	extern	int	  current;

	int	  tokc;
	int	  n;
	char	**tokv;
	char	 *s;
	char	  buf[sizeof (input)];

	tokv = NULL;
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

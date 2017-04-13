/* MIT/X Consortium License
 *
 * copyright (c) 2006-2014 Anselm R Garbe <anselm@garbe.us>
 * copyright (c) 2010-2012 Connor Lane Smith <cls@lubutu.com>
 * copyright (c) 2009 Gottox <gottox@s01.de>
 * copyright (c) 2009 Markus Schnalke <meillo@marmaro.de>
 * copyright (c) 2009 Evan Gates <evan.gates@gmail.com>
 * copyright (c) 2006-2008 Sander van Dijk <a dot h dot vandijk at gmail dot com>
 * copyright (c) 2006-2007 Michał Janeczek <janeczek at gmail dot com>
 * copyright (c) 2014-2015 Hiltjo Posthuma <hiltjo@codemadness.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define _POSIX_C_SOURCE 200809L

#include <sys/stat.h>

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *argv0;

#define FLAG(x)  (flag[(x)-'a'])

static void test(const char *, const char *);
static void usage(void);

static int match = 0;
static int flag[26];
static struct stat old, new;

static void
test(const char *path, const char *name)
{
	struct stat st, ln;

	if ((!stat(path, &st) && (FLAG('a') || name[0] != '.')        /* hidden files      */
	&& (!FLAG('b') || S_ISBLK(st.st_mode))                        /* block special     */
	&& (!FLAG('c') || S_ISCHR(st.st_mode))                        /* character special */
	&& (!FLAG('d') || S_ISDIR(st.st_mode))                        /* directory         */
	&& (!FLAG('e') || access(path, F_OK) == 0)                    /* exists            */
	&& (!FLAG('f') || S_ISREG(st.st_mode))                        /* regular file      */
	&& (!FLAG('g') || st.st_mode & S_ISGID)                       /* set-group-id flag */
	&& (!FLAG('h') || (!lstat(path, &ln) && S_ISLNK(ln.st_mode))) /* symbolic link     */
	&& (!FLAG('n') || st.st_mtime > new.st_mtime)                 /* newer than file   */
	&& (!FLAG('o') || st.st_mtime < old.st_mtime)                 /* older than file   */
	&& (!FLAG('p') || S_ISFIFO(st.st_mode))                       /* named pipe        */
	&& (!FLAG('r') || access(path, R_OK) == 0)                    /* readable          */
	&& (!FLAG('s') || st.st_size > 0)                             /* not empty         */
	&& (!FLAG('u') || st.st_mode & S_ISUID)                       /* set-user-id flag  */
	&& (!FLAG('w') || access(path, W_OK) == 0)                    /* writable          */
	&& (!FLAG('x') || access(path, X_OK) == 0)) != FLAG('v')) {   /* executable        */
		if (FLAG('q'))
			exit(0);
		match = 1;
		puts(name);
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-abcdefghlpqrsuvwx] "
	        "[-n file] [-o file] [file...]\n", argv0);
	exit(2); /* like test(1) return > 1 on error */
}

int
main(int argc, char *argv[])
{
	struct dirent *d;
	char path[PATH_MAX], *line = NULL, *file;
	size_t linesiz = 0;
	ssize_t n;
	DIR *dir;
	int r;

	for (
		argv0 = *argv, argv++, argc--;
		argv[0] && argv[0][0] == '-' && argv[0][1];
		argv++, argc--
	) {
		int brk = 0;

		if (argv[0][1] == '-' && argv[0][2] == '\0') {
			argv++,	argc--;
			break;
		}

		for (argv[0]++; !brk && argv[0][0]; argv[0]++) {
			char f = argv[0][1];

			switch (f) {

			case 'n': /* newer than file */
			case 'o': /* older than file */
				if (argv[0][1] == '\0' && argv[1] == NULL)
					usage();

				file = (brk = 1, (argv[0][1] != '\0') ?
					(&argv[0][1]) :	(argc--, argv++, argv[0]));

				if (!(FLAG(f) = !stat(file, (f == 'n' ? &new : &old))))
					perror(file);
				break;
			default:
				/* miscellaneous operators */
				if (strchr("abcdefghlpqrsuvwx", f))
					FLAG(f) = 1;
				else
					usage(); /* unknown flag */
			}
		}
	}

	if (!argc) {
		/* read list from stdin */
		while ((n = getline(&line, &linesiz, stdin)) > 0) {
			if (n && line[n - 1] == '\n')
				line[n - 1] = '\0';
			test(line, line);
		}
		free(line);
	} else {
		for (; argc; argc--, argv++) {
			if (FLAG('l') && (dir = opendir(*argv))) {
				/* test directory contents */
				while ((d = readdir(dir))) {
					r = snprintf(path, sizeof path, "%s/%s",
					             *argv, d->d_name);
					if (r >= 0 && (size_t)r < sizeof path)
						test(path, d->d_name);
				}
				closedir(dir);
			} else {
				test(*argv, *argv);
			}
		}
	}
	return match ? 0 : 1;
}

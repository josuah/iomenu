#include "log.h"

#include <assert.h>
#include <string.h>

/*
 * log.c - log to standard error according to the log level
 *
 * Instead of logging to syslog, delegate logging to a separate
 * tool, such as FreeBSD's daemon(8), POSIX's logger(1).
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_DEFAULT 3 /* info */

char *arg0 = NULL;

static int log_level = -1;

void
log_print(int level, char const *flag, char const *fmt, ...)
{
	va_list va;
	char *env;
	int old_errno = errno;

	va_start(va, fmt);

	if (log_level < 0) {
		env = getenv("LOG");
		log_level = (env == NULL) ? 0 : atoi(env);
		if (log_level == 0)
			log_level = LOG_DEFAULT;
	}

	if (log_level < level)
		return;

	if (arg0 != NULL)
		fprintf(stderr, "%s: ", arg0);

	fprintf(stderr, "%s: ", flag);
	vfprintf(stderr, fmt, va);

	if (old_errno != 0)
		fprintf(stderr, ": %s", strerror(old_errno));

	fprintf(stderr, "\n");
	fflush(stderr);
	va_end(va);
}

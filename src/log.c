#include "log.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LOG_DEFAULT
#define LOG_DEFAULT 3 /* info */
#endif

char *arg0 = NULL;
static int log_level = -1;

void
log_vprintf(int level, char const *flag, char const *fmt, va_list va)
{
	char *env;
	int old_errno = errno;

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
}

void
die(char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt); log_vprintf(1, "error", fmt, va); va_end(va);
	exit(1);
}

void
warn(char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt); log_vprintf(2, "warn", fmt, va); va_end(va);
}

void
info(char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt); log_vprintf(3, "info", fmt, va); va_end(va);
}

void
debug(char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt); log_vprintf(4, "debug", fmt, va); va_end(va);
}

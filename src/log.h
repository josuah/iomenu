#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

/** src/log.c **/
char *arg0;
void log_print(int level, char const *flag, char const *fmt, ...);

#define die(...) (log_print(1, "error", __VA_ARGS__), exit(1))
#define warn(...) log_print(2, "warn", __VA_ARGS__)
#define info(...) log_print(3, "info", __VA_ARGS__)
#define debug(...) log_print(4, "debug", __VA_ARGS__)

#endif

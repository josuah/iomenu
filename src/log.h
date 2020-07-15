#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

/** src/log.c **/
char *arg0;
void log_vprintf(int level, char const *flag, char const *fmt, va_list va);
void die(char const *fmt, ...);
void warn(char const *fmt, ...);
void info(char const *fmt, ...);
void debug(char const *fmt, ...);

#endif

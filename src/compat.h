#ifndef COMPAT_H
#define COMPAT_H

#define wcwidth(c) mk_wcwidth_cjk(c)
#define wcswidth(s, n) mk_wcwidth_cjk(s, n)

/** src/compat/?*.c **/
char * strcasestr(const char *str1, const char *str2);
char * strsep(char **str_p, char const *sep);

#endif

#ifndef COMPAT_H
#define COMPAT_H

#include <stddef.h>
#include <wchar.h>

#define wcwidth(c) mk_wcwidth_cjk(c)

char	*strcasestr(const char *str1, const char *str2);
size_t	 strlcpy(char *buf, char const *str, size_t sz);
char	*strsep(char **str_p, char const *sep);
int	 mk_wcwidth(wchar_t ucs);
int	 mk_wcswidth(const wchar_t *pwcs, size_t n);
int	 mk_wcwidth_cjk(wchar_t ucs);
int	 mk_wcswidth_cjk(const wchar_t *pwcs, size_t n);

#endif

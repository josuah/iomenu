#ifndef WCWIDTH_H
#define WCWIDTH_H

#include <wchar.h>

#define wcwidth(c) mk_wcwidth_cjk(c)

int mk_wcwidth(wchar_t ucs);
int mk_wcswidth(const wchar_t *pwcs, size_t n);
int mk_wcwidth_cjk(wchar_t ucs);
int mk_wcswidth_cjk(const wchar_t *pwcs, size_t n);

#endif

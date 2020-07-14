#ifndef UTF8_H
#define UTF8_H

/** src/utf8.c **/
int utf8_runelen(long rune);
int utf8_utflen(char const *str);
long utf8_torune(char const **str);
int utf8_tostr(char *str, long rune);
int utf8_isprint(long rune);
char const * utf8_nextrune(char const *str);
char const * utf8_prevrune(char const *str);

#endif

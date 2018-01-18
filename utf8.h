size_t	utf8_len(char *);
size_t	rune_len(long);
size_t	utf8_torune(long *, char *);
int	utf8_isunicode(long);
int	utf8_check(char *);
int	utf8_isprint(long);
int	utf8_col(char *, int, int);
int	utf8_wcwidth(long);

size_t	utf8_len(char *);
size_t	rune_len(long);
size_t	utf8_torune(long *, char *);
int	utf8_isunicode(long);
int	utf8_check(char *);
int	utf8_isprint(long);
char	*utf8_col();

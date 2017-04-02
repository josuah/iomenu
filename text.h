/* rune / utf length */
int utflen(char *, int);
int runelen(long);

/* decode / encode */
int utftorune(long *, char *, int);
int runetoutf(char *, long);

/* stdin / stdout */
int getutf(long **, FILE *);
int runetoprint(char *, long, int);

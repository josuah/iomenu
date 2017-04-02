/* rune / utf length */
int utflen(char *, int);
int runelen(long);

/* decode / encode */
int utftorune(long *, char *, int);
int runetoutf(char *, long);

/* rune class */
int isprintrune(long);

/* stdin / stdout */
int runetoprint(char *, long, int);
int getutf(long **, FILE *);

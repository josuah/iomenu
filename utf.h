/* rune / utf length */
int    utflen(char *, int);
int    runelen(long);

/* conversion */
int    utftorune(long *, char *, int);
int    runetoutf(char *, long);
int    runetoprint(char *, long);

/* input/output */

size_t getutf(long **, FILE *);

/* standard library */
int    runeisprint(long);
long  *runestrcpy();

/* lengths */
int    utflen(char *, int);
int    runelen(long);

/* conversions */
int    utftorune(long *, char *, int);
int    utftorune(long *, char *, int);
int    runetoutf(char *, long);
int    runetoprint(char *, long);


/* standard library */

int    runeisprint(long);
size_t getrunes(long **, FILE *);
long * runescpy(long *, long *);
long * runeschr(long *, long);
long * runescat(long *, long *);

#ifndef SIGWINCH
#define SIGWINCH 28
#endif

#define MARGIN   30

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

extern struct winsize   ws;
extern char           **linev;
extern int              linec;
extern char           **matchv;
extern int              matchc;
extern char            *prompt;
extern char             input[LINE_MAX];
extern char             formatted[LINE_MAX * 8];
extern int              current;
extern int              opt[128];
extern int              rows;

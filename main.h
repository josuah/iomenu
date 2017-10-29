#ifndef SIGWINCH
#define SIGWINCH 28
#endif

#define CONTINUE (EXIT_SUCCESS + EXIT_FAILURE + 1)
#define MARGIN   30

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

winsize   ws;
char    **linev   = NULL;
int       linec   = 0;
char    **matchv  = NULL;
int       matchc  = 0;
char     *prompt  = "";
char      input[LINE_MAX];
char      formatted[LINE_MAX * 8];
int       current = 0;
int       offset  = 0;
int       next    = 0;
int       opt[128];
int       rows    = 0;

size_t utf8_len        (char *);
size_t rune_len        (long);
size_t utf8_to_rune    (long *, char *);
int    utf8_is_unicode (long);
int    utf8_check      (char *);
int    utf8_is_print   (long);

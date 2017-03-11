#define LINE_SIZE  1024
#define OFFSET     5
#define CONTINUE   2  /* as opposed to EXIT_SUCCESS and EXIT_FAILURE */

#define CONTROL(char) (char ^ 0x40)
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


/*
 * Options from the command line, to pass to each function that need some
 */
typedef struct Opt {
	int   line_numbers;
	int   print_number;
	int   print_header;
	char  validate_key;
	char *separator;
	int   lines;
	char *prompt;
} Opt;


/*
 * Line coming from stdin, wrapped in a header.
 */
typedef struct Line {
	char *content;               /* sent as output and matched by input */
	char *comment;               /* displayed at the right of the content */

	int  number;                 /* set here as order will not change */
	int  matches;                /* whether it matches buffer's input */
	int  header;                 /* whether the line is a header */

	struct Line *prev;           /* doubly linked list structure */
	struct Line *next;
} Line;


/*
 * Buffer containing a doubly linked list of headers
 */
typedef struct Buffer {
	int total;                   /* total number of line in buffer */
	int matching;                /* number lines matching the input */

	char    input[LINE_SIZE];    /* string from user's keyboard */

	Line   *current;             /* selected line, highlighted */
	Line   *first;               /* boundaries of the linked list */
	Line   *last;
} Buffer;


/* main */

void     usage(void);


/* buffer */

Buffer * fill_buffer(char *);
void     free_buffer(Buffer *);
Line   * add_line(Buffer *, int, char *, char *, Line *);
Line   * new_line(char *, char *);
Line   * matching_next(Line *);
Line   * matching_prev(Line *);
int      match_line(Line *, char **, size_t);
void     filter_lines(Buffer *, int);


/* draw */

void     draw_screen(Buffer *, int, Opt *);
void     draw_clear(int);
void     draw_line(Line *, int, int, Opt *);
void     draw_lines(Buffer *, int, int, Opt *);
void     draw_prompt(Buffer *, int, Opt *);


/* input */

int      input_get(Buffer *, int, Opt *);
int      input_key(FILE *, Buffer *, Opt *);
void     action_jump(Buffer *, int);
void     action_print_selection(Buffer *,int, Opt *);
void     action_remove_word_input(Buffer *);
void     action_add_character(Buffer *, char);


/* util */

void             die(const char *);
struct termios   set_terminal(int);
char           * expand_tabs(char *);

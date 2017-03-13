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
	char  validate_key;
	char *separator;
	int   lines;
	char *prompt;
} Opt;


/*
 * Line coming from stdin
 */
typedef struct Line {
	char *content;               /* sent as output and matched by input */
	int   matches;                /* whether it matches buffer's input */
} Line;


/* iomenu */

void             die(const char *);
struct  termios set_terminal(int);
void            usage(void);


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

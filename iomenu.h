/*--- constants --------------------------------------------------------------*/

#define LINE_SIZE  1024
#define OFFSET     5
#define CONTINUE   2  /* as opposed to EXIT_SUCCESS and EXIT_FAILURE */


/*--- macros -----------------------------------------------------------------*/

#define CONTROL(char) (char ^ 0x40)
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


/*--- structures -------------------------------------------------------------*/

typedef struct Line {
	char *text;  /* sent as output and matched by input */
	int   match;   /* whether it matches buffer's input */
} Line;


/*--- variables --------------------------------------------------------------*/

Line **buffer;
int    current, matching, total;
int    opt_lines;
char  *opt_prompt, *input;


/*--- functions --------------------------------------------------------------*/

/* iomenu */
void            die(const char *);
struct termios set_terminal(int);
void           usage(void);

/* buffer */
void   fill_buffer(void);
void   free_buffer();
Line * add_line(int, char *, char *, Line *);
Line * new_line(char *, char *);
Line * matching_next(int);
Line * matching_prev(int);
int    match_line(Line *, char **, size_t);
void   filter_lines(int);

/* draw */
void draw_screen(int);
void draw_clear(int);
void draw_line(Line *, int);
void draw_lines(int, int);
void draw_prompt(int);

/* input */
int  input_get(int);
int  input_key(FILE *);
void action_jump(int);
void action_print_selection(int);
void action_remove_word_input();
void action_add_character(char);

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "main.h"


void
usage(void)
{
	fputs("usage: iomenu [-n] [-N] [-k key] [-s separator] ", stderr);
	fputs("[-p prompt] [-l lines]\n", stderr);

	exit(EXIT_FAILURE);
}


int
main(int argc, char *argv[])
{
	int i, exit_code, tty_fd = open("/dev/tty", O_RDWR);
	Buffer *buffer = NULL;
	Opt    *opt    = malloc(sizeof(Opt));

	opt->line_numbers  = 0;
	opt->print_number  = 0;
	opt->validate_key  = CONTROL('M');
	opt->separator     = NULL;
	opt->lines         = 30;
	opt->prompt        = "";

	/* command line arguments */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || strlen(argv[i]) != 2)
			usage();

		switch (argv[i][1]) {
		case 'n':
			opt->line_numbers = 1;
			break;
		case 'N':
			opt->print_number = 1;
			opt->line_numbers = 1;
			break;
		case 'H':
			opt->print_header = 1;
			break;
		case 'k':
			opt->validate_key = (argv[++i][0] == '^') ?
				CONTROL(toupper(argv[i][1])): argv[i][0];
			break;
		case 's':
			opt->separator = argv[++i];
			break;
		case 'l':
			if (sscanf(argv[++i], "%d", &opt->lines) <= 0)
				die("wrong number format after -l");
			break;
		case 'p':
			if (++i >= argc)
				die("wrong string format after -p");
			opt->prompt = argv[i];
			break;
		default:
			usage();
		}
	}

	/* command line arguments */
	buffer = fill_buffer(opt->separator);

	/* set the interface */
	draw_screen(buffer, tty_fd, opt);

	/* listen and interact to input */
	exit_code = input_get(buffer, tty_fd, opt);

	draw_clear(opt->lines);

	/* close files descriptors and pointers, and free memory */
	close(tty_fd);
	free(opt);
	free_buffer(buffer);

	return exit_code;
}

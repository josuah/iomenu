#include <sys/ioctl.h>

#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "iomenu.h"
#include "buffer.h"
#include "control.h"
#include "display.h"

#define CTL(char) ((char) ^ 0x40)
#define ALT(char) ((char) + 0x80)
#define CSI(char) ((char) + 0x80 + 0x80)

void
move(signed int sign)
{
	int i;

	for (i = current + sign; 0 <= i && i < matchc; i += sign) {
		if (!opt['#'] || matchv[i][0] != '#') {
			current = i;
			break;
		}
	}
}

static void
move_page(signed int sign)
{
	int i;

	i = current - current % rows + rows * sign;
	if (!(0 < i && i < matchc))
		return;
	current = i - 1;
	move(+1);
}

static void
remove_word()
{
	int len;
	int i;

	len = strlen(input) - 1;
	for (i = len; i >= 0 && isspace(input[i]); i--)
		input[i] = '\0';
	len = strlen(input) - 1;
	for (i = len; i >= 0 && !isspace(input[i]); i--)
		input[i] = '\0';
	filter();
}

static void
add_char(char c)
{
	int len;

	len = strlen(input);
	if (isprint(c)) {
		input[len]     = c;
		input[len + 1] = '\0';
	}
	filter();
}

int
key(int k)
{
top:
	switch (k) {
	case CTL('C'):
		return -1;
	case CTL('U'):
		input[0] = '\0';
		filter();
		break;
	case CTL('W'):
		remove_word();
		break;
	case 127:
	case CTL('H'):  /* backspace */
		input[strlen(input) - 1] = '\0';
		filter();
		break;
	case CSI('A'):  /* up */
	case CTL('P'):
		move(-1);
		break;
	case CSI('B'):  /* down */
	case CTL('N'):
		move(+1);
		break;
	case CSI('5'):  /* page up */
		if (fgetc(stdin) != '~')
			break;
		/* fallthrough */
	case ALT('v'):
		move_page(-1);
		break;
	case CSI('6'):  /* page down */
		if (fgetc(stdin) != '~')
			break;
		/* fallthrough */
	case CTL('V'):
		move_page(+1);
		break;
	case CTL('I'):  /* tab */
		if (linec > 0)
			strcpy(input, matchv[current]);
		filter();
		break;
	case CTL('J'):  /* enter */
	case CTL('M'):
		print_selection();
		return 0;
	case ALT('['):
		k = CSI(fgetc(stdin));
		goto top;
	case 0x1b: /* escape / alt */
		k = ALT(fgetc(stdin));
		goto top;
	default:
		add_char((char) k);
	}

	return 1;
}

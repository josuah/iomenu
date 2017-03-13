CFLAGS    = -std=c89 -pedantic -Wall -Wextra -g -static
OBJ       = ${SRC:.c=.o}

MANPREFIX = $(PREFIX)

all: clean iomenu

iomenu: buffer.c draw.c input.c

clean:
	rm -f iomenu ${OBJ}

install: iomenu
	mkdir -p  $(PREFIX)/bin $(MANPREFIX)/man/man1
	cp *.1 $(MANPREFIX)/man/man1/
	cp iomenu $(PREFIX)/bin/

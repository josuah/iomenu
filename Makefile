CFLAGS    = -std=c89 -pedantic -Wall -Wextra -g -static
SRC       = main.c buffer.c util.c draw.c input.c
OBJ       = ${SRC:.c=.o}

MANPREFIX = $(PREFIX)

all: clean iomenu

.c.o:
	${CC} -c ${CFLAGS} $<

iomenu: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}
	rm -f *.o

clean:
	rm -f iomenu ${OBJ}

install: iomenu
	mkdir -p  $(PREFIX)/bin $(MANPREFIX)/man/man1
	cp *.1 $(MANPREFIX)/man/man1/
	cp iomenu io-* $(PREFIX)/bin/

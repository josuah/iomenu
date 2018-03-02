CFLAGS = -std=c89 -pedantic -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L

SRC = iomenu.c utf8.c strcasestr.c
OBJ = ${SRC:.o=.c}

all: iomenu

.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

iomenu: ${OBJ} utf8.h str.h
	${CC} -o $@ ${LDFLAGS} ${OBJ}

utf8.c: utf8.h

strcasestr.c: str.h

test: test.c
	${CC} -o $@ ${LDFLAGS} test.c utf8.c
	./$@

clean:
	rm -f *.o *.core iomenu test

install: iomenu
	mkdir -p  ${PREFIX}/share/man/man1
	cp *.1    ${PREFIX}/share/man/man1
	mkdir -p  ${PREFIX}/bin
	cp iomenu ${PREFIX}/bin

.PHONY: all test clean install

CFLAGS = -std=c89 -pedantic -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L

SRC = iomenu.c utf8.c
OBJ = ${SRC:.o=.c}

all: iomenu

.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

iomenu: ${OBJ}
	${CC} -o $@ ${LDFLAGS} ${OBJ}
${OBJ}: utf8.h

test:
	${CC} -o $@ ${LDFLAGS} test_utf8.c utf8.c
	./$@

clean:
	rm -f *.o *.core iomenu

install: iomenu
	mkdir -p  ${PREFIX}/share/man/man1
	cp *.1    ${PREFIX}/share/man/man1
	mkdir -p  ${PREFIX}/bin
	cp iomenu ${PREFIX}/bin

.PHONY: all test clean install

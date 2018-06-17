CFLAGS = -std=c89 -pedantic -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L

SRC = iomenu.c strcasestr.c strsep.c utf8.c
OBJ = ${SRC:.c=.o}

.PHONY: all
all: iomenu

.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

iomenu: ${OBJ}
	${CC} -o $@ ${LDFLAGS} ${OBJ}

iomenu.o: iomenu.c util.h
strcasestr.o: strcasestr.c util.h
strsep.o: strsep.c util.h
test.o: test.c util.h
utf8.o: utf8.c utf8.h

.PHONY: test
test: test.c
	${CC} -o $@ ${LDFLAGS} test.c utf8.c
	./$@

.PHONY: clean
clean:
	rm -f *.o *.core iomenu test

.PHONY: install
install: iomenu
	mkdir -p	${PREFIX}/share/man/man1
	cp *.1		${PREFIX}/share/man/man1
	mkdir -p	${PREFIX}/bin
	cp iomenu bin/*	${PREFIX}/bin

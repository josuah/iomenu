include config.mk

SRC = iomenu.c strcasestr.c strsep.c utf8.c
OBJ = ${SRC:.c=.o}

all: iomenu

.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

iomenu: ${OBJ}
	${CC} -o $@ ${LFLAGS} ${OBJ}

iomenu.o: iomenu.c util.h
strcasestr.o: strcasestr.c util.h
strsep.o: strsep.c util.h
test.o: test.c util.h
utf8.o: utf8.c utf8.h

.PHONY: test
test: test.c
	${CC} -o $@ ${LFLAGS} test.c utf8.c
	./$@

clean:
	rm -f *.o *.core iomenu test

install: iomenu
	mkdir -p	${MANPREFIX}/man1
	cp *.1		${MANPREFIX}/man1
	mkdir -p	${PREFIX}/bin
	cp iomenu bin/*	${PREFIX}/bin

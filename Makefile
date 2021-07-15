NAME = iomenu
VERSION = 0.1

CFLAGS = -DVERSION='"${VERSION}"' -I./src  -Wall -Wextra -std=c99 --pedantic -g
LDFLAGS = -static
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/man

SRC = utf8.c compat.c wcwidth.c term.c
HDR = utf8.h compat.h term.h
OBJ = ${SRC:.c=.o}
BIN = iomenu
MAN1 = iomenu.1

all: ${BIN}

.c.o:
	${CC} -c ${CFLAGS} -o $@ $<

${OBJ}: ${HDR}
${BIN}: ${OBJ} ${BIN:=.o}
	${CC} ${LDFLAGS} -o $@ $@.o ${OBJ} ${LIB}

clean:
	rm -rf *.o ${BIN} ${NAME}-${VERSION} *.gz

install:
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -rf bin/* ${BIN} ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -rf ${MAN1} ${DESTDIR}${MANPREFIX}/man1

dist: clean
	mkdir -p ${NAME}-${VERSION}
	cp -r README Makefile bin ${MAN1} ${SRC} ${NAME}-${VERSION}
	tar -cf - ${NAME}-${VERSION} | gzip -c >${NAME}-${VERSION}.tar.gz

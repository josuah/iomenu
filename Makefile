NAME = iomenu
VERSION = 0.1

SRC = src/utf8.c src/log.c src/mem.c src/compat/strcasestr.c \
  src/compat/strsep.c src/compat/strlcpy.c src/compat/wcwidth.c src/term.c
HDR = src/mem.h src/compat.h src/log.h src/term.h src/utf8.h
BIN = iomenu
MAN1 = ${BIN:=.1}
OBJ = ${SRC:.c=.o}
LIB =

W = -Wall -Wextra -std=c99 --pedantic
I = -I./src
L =
D = -DVERSION='"${VERSION}"'
CFLAGS = $I $D $W -g
LDFLAGS = $L -static
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/man

all: ${BIN}

.c.o:
	${CC} -c ${CFLAGS} -o $@ $<

${OBJ}: ${HDR}
${BIN}: ${OBJ} ${BIN:=.o}
	${CC} ${LDFLAGS} -o $@ $@.o ${OBJ} ${LIB}

clean:
	rm -rf *.o */*.o ${BIN} ${NAME}-${VERSION} *.gz

install:
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -rf bin/* ${BIN} ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -rf ${MAN1} ${DESTDIR}${MANPREFIX}/man1

dist: clean
	mkdir -p ${NAME}-${VERSION}
	cp -r README Makefile ${MAN1} ${SRC} ${NAME}-${VERSION}
	tar -cf - ${NAME}-${VERSION} | gzip -c >${NAME}-${VERSION}.tar.gz

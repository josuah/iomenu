CFLAGS = -std=c89 -pedantic -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L

OBJ = iomenu.o utf8.o

all: iomenu

.c.o:
	${CC} -c -o $@ ${CFLAGS} $<

iomenu: ${OBJ}
	${CC} -o $@ ${LDFLAGS} ${OBJ}

${OBJ}: utf8.h

clean:
	rm -f *.o *.core iomenu

install: iomenu
	mkdir -p  ${PREFIX}/share/man/man1
	cp *.1    ${PREFIX}/share/man/man1
	mkdir -p  ${PREFIX}/bin
	cp iomenu ${PREFIX}/bin

.PHONY: all clean install

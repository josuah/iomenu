CFLAGS = -std=c89 -Wpedantic -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L

all: iomenu

iomenu: iomenu.o utf8.o

clean:
	rm -f *.o iomenu

install: iomenu
	mkdir -p  $(PREFIX)/share/man/man1
	cp *.1    $(PREFIX)/share/man/man1
	mkdir -p  $(PREFIX)/bin
	cp iomenu $(PREFIX)/bin

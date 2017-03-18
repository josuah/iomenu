CFLAGS    = -std=c99 -Wpedantic -Wall -Wextra -g # -static 
OBJ       = ${SRC:.c=.o}

all: clean iomenu

clean:
	rm -f iomenu ${OBJ}

install: iomenu
	mkdir -p  $(PREFIX)/bin $(PREFIX)/man/man1
	# cp *.1 $(PREFIX)/man/man1/
	cp iomenu $(PREFIX)/bin/

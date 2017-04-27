CFLAGS    = -std=c89 -Wpedantic -Wall -Wextra -g # -static 

all: clean iomenu

clean:
	rm -f *.o iomenu

install: iomenu
	mkdir -p $(PREFIX)/bin $(PREFIX)/man/man1
	cp *.1    $(PREFIX)/man/man1/
	cp iomenu $(PREFIX)/bin/

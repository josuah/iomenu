CFLAGS = -std=c89 -Wpedantic -Wall -Wextra -g

all: clean iomenu

clean:
	rm -f *.o iomenu

install: iomenu
	mkdir -p $(PREFIX)/bin $(PREFIX)/share/man/man1
	cp *.1    $(PREFIX)/share/man/man1/
	cp iomenu $(PREFIX)/bin/

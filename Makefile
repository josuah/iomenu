CFLAGS = -std=c89 -Wpedantic -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L

OBJ = buffer.o control.o display.o main.o utf8.o

all: iomenu

iomenu: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@

clean:
	rm -f *.o iomenu

install: iomenu
	mkdir -p  $(PREFIX)/share/man/man1
	cp *.1    $(PREFIX)/share/man/man1
	mkdir -p  $(PREFIX)/bin
	cp iomenu $(PREFIX)/bin

.PHONY: all clean install

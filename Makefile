CFLAGS = -std=c89 -pedantic -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L

OBJ = buffer.o control.o display.o main.o utf8.o
INC = buffer.h control.h display.h main.h utf8.h iomenu.h

all: iomenu

iomenu: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@

$(OBJ): $(INC)

clean:
	rm -f *.o *.core iomenu

install: iomenu
	mkdir -p  $(PREFIX)/share/man/man1
	cp *.1    $(PREFIX)/share/man/man1
	mkdir -p  $(PREFIX)/bin
	cp iomenu $(PREFIX)/bin

.PHONY: all clean install

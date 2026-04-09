# pterminal
# See LICENSE file for copyright and license details.
.POSIX:


CC = cc

LIBS = -lm -lGL -llodepng -lpway -lpfonts
LIBS += -lEGL -lwayland-client -lwayland-egl
LIBS += -lxkbcommon

FLAGS = -g
LDFLAGS = -L lib/ $(LIBS)


SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: pterminal

.c.o:
	$(CC) $(FLAGS) -c $<

$(OBJ): config.h

font.o: font.png
	ld -r -b binary -o font.o font.png
	objcopy --add-section .note.GNU-stack=/dev/null font.o

pterminal: $(OBJ) font.o
	$(CC) -o $@ $(OBJ) font.o $(LDFLAGS)

clean:
	rm -f pterminal $(OBJ) font.o

install: pterminal
	cp -f pterminal /bin


.PHONY: all clean install

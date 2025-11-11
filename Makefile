# pterminal
# See LICENSE file for copyright and license details.
.POSIX:


CC = cc

X11INC = /usr/include
X11LIB = /usr/lib

INCS = -I$(X11INC) -I/usr/include/freetype2

LIBS = -L/usr/lib -lm -lrt -lX11 -lutil -lGL -llodepng

STCFLAGS = $(INCS) $(CFLAGS) -g
STLDFLAGS = $(LIBS) $(LDFLAGS)


SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: pterminal

.c.o:
	$(CC) $(STCFLAGS) -c $<


$(OBJ): config.h

pterminal: $(OBJ)
	$(CC) -o $@ $(OBJ) $(STLDFLAGS)

clean:
	rm -f pterminal $(OBJ) 

install: pterminal
	cp -f pterminal /bin

test: all
	./pterminal -c "pterminal-test" -t "pterminal" -g 100x30

.PHONY: all clean install

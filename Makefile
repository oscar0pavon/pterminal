# pterminal
# See LICENSE file for copyright and license details.
.POSIX:

VERSION = 0.9.2

CC = cc

X11INC = /usr/include
X11LIB = /usr/lib

INCS = -I$(X11INC) -I/usr/include/freetype2

LIBS = -L/usr/lib -lm -lrt -lX11 -lutil -lXft -lfreetype -lfontconfig

STCPPFLAGS = -DVERSION=\"$(VERSION)\" -D_XOPEN_SOURCE=600
STCFLAGS = $(INCS) $(STCPPFLAGS) $(CPPFLAGS) $(CFLAGS)
STLDFLAGS = $(LIBS) $(LDFLAGS)


SRC = st.c main.c
OBJ = $(SRC:.c=.o)

all: pterminal

.c.o:
	$(CC) $(STCFLAGS) -c $<

st.o: config.h st.h win.h
main.o: arg.h config.h st.h win.h

$(OBJ): config.h

pterminal: $(OBJ)
	$(CC) -o $@ $(OBJ) $(STLDFLAGS)

clean:
	rm -f pterminal $(OBJ) 

install: pterminal
	cp -f pterminal /bin

.PHONY: all clean install

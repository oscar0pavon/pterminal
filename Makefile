# pterminal
# See LICENSE file for copyright and license details.
.POSIX:


CC = cc

LIBS = -lm -lGL -llodepng
LIBS += -lEGL -lwayland-client -lwayland-egl
LIBS += -lxkbcommon

FLAGS = -g
LDFLAGS = -L lib/ $(LIBS)

wayland_src := $(wildcard ./wayland/*.c)
wayland_objs := $(wayland_src:%.c=%.o)

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
OBJ += $(wayland_objs)

all: pterminal

.c.o:
	$(CC) $(STCFLAGS) -c $<


$(OBJ): config.h

pterminal: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

./wayland/%.o: ./wayland/%.c
	cc $(FLAGS) -o $@ -c $<

clean:
	rm -f pterminal $(OBJ) 

install: pterminal
	cp -f pterminal /bin

EGL = WAYLAND_DISPLAY=wayland-0 EGL_LOG_LEVEL=debug MESA_DEBUG=egl MESA_LOADER_DRIVER_OVERRIDE=llvmpipe LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe


test: all
	./pterminal -c "pterminal-test" -t "pterminal" -g 30x10


.PHONY: all clean install

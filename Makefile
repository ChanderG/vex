LDLIBS += -lX11 -lXft \
	`pkg-config --libs x11` \
	`pkg-config --libs fontconfig`

CFLAGS += -g -std=c99 -Wall -Wextra \
	`pkg-config --cflags x11` \
	`pkg-config --cflags fontconfig` \
	-I./deps/libvterm/include

.PHONY: all clean

all: vex

vex: vex.c unicode.c
	gcc $(CFLAGS) $(LDLIBS) -o vex vex.c unicode.c ./deps/libvterm/.libs/libvterm.a

clean:
	rm vex

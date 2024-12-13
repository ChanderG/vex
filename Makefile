LDLIBS += -lX11 -lXft \
	`pkg-config --libs x11` \
	`pkg-config --libs fontconfig` \
	`pkg-config --libs vterm`
CFLAGS += -g -std=c99 -Wall -Wextra \
	`pkg-config --cflags x11` \
	`pkg-config --cflags fontconfig` \
	`pkg-config --cflags vterm`

.PHONY: all clean

all: vex

vex: vex.c

clean:
	rm vex

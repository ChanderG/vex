LDLIBS += -lX11 -lXft \
	`pkg-config --libs x11` \
	`pkg-config --libs fontconfig`
CFLAGS += -g -std=c99 -Wall -Wextra \
	`pkg-config --cflags x11` \
	`pkg-config --cflags fontconfig` \

.PHONY: all clean

all: vex

vex: vex.c

clean:
	rm vex

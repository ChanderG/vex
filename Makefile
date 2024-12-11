LDLIBS += `pkg-config --libs x11`
CFLAGS += -g -std=c99 -Wall -Wextra `pkg-config --cflags x11`

.PHONY: all clean

all: vex

vex: vex.c

clean:
	rm vex

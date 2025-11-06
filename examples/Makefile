# OCFX Examples Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
CFLAGS += -I../include
CFLAGS += $(shell pkg-config --cflags wayland-client wayland-egl xkbcommon egl glesv2 freetype2)

LIBS = -L../lib -locfx
LIBS += $(shell pkg-config --libs wayland-client wayland-egl xkbcommon egl glesv2 freetype2)
LIBS += -lm

# Wayland protocols
PROTOCOL_DIR = ../protocols
CFLAGS += -I$(PROTOCOL_DIR)

all: hello

hello: hello.c ../lib/libocfx.a
	$(CC) $(CFLAGS) hello.c $(LIBS) -o hello
	@echo "Built example: hello"

clean:
	rm -f hello

run: hello
	./hello

.PHONY: all clean run

# Makefile for the Helio 210 emulator suite
CC		?= gcc
CFLAGS	?= -O2 -Wall -Wextra -std=c11

# SDL flags for SDL 2
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

# Common sources
COM_SRCS   = helio210.c AY3-8910.c AY3-8910_sdl.c

# Full vs No-uart builds
FULL_SRCS  = $(COM_SRCS) uart.c
NUART_SRCS = $(COM_SRCS) uart-stub.c 

# Default build includes uart.c
all: helio210 asm studio

helio210: $(FULL_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL_LIBS)


helio210-nouart: $(NUART_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL_LIBS)

asm: asm.c 
	$(CC) $(CFLAGS) -o $@ $^

studio: studio.c
	$(CC) $(CFLAGS) -o $@ $^ $(SDL_LIBS)


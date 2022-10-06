CC = gcc

CFLAGS_DEBUG = -pedantic -g3 -D DEBUG
CFLAGS_RELEASE = -O3 -march=native

LIBS = $(shell pkg-config --cflags --libs libavcodec libavformat libavutil)

FILES = src/*.c

BIN = pixie

.PHONY = release

release:
	$(CC) $(FILES) $(CFLAGS_RELEASE) $(LIBS) -o $(BIN)

debug:
	$(CC) $(FILES) $(CFLAGS_DEBUG) $(LIBS) -o $(BIN)

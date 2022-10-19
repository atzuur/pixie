CC = gcc
BIN = pixie
CFLAGS_DEBUG = -pedantic -g3
CFLAGS_RELEASE = -O3 -march=native

LIBS = $(shell pkg-config --cflags --libs libavcodec libavformat libavutil)
FILES = src/*.c

.PHONY = release

release:
	$(CC) $(FILES) $(CFLAGS_RELEASE) $(LIBS) -o $(BIN)

debug:
	$(CC) $(FILES) $(CFLAGS_DEBUG) $(LIBS) -o $(BIN)

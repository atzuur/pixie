CC = gcc
BIN = pixie
CFLAGS = -Wall -Wextra -pedantic
CFLAGS_DEBUG = -g3 -fsanitize=address,undefined
CFLAGS_RELEASE = -O3 -flto

LIBS = $(shell pkg-config --cflags --libs libavcodec libavformat libavutil)
FILES = src/*.c src/util/*.c

.PHONY = release

release:
	$(CC) $(FILES) $(CFLAGS) $(CFLAGS_RELEASE) $(LIBS) -o $(BIN)

debug:
	$(CC) $(FILES) $(CFLAGS) $(CFLAGS_DEBUG) $(LIBS) -o $(BIN).out

clean:
	rm -f $(BIN)

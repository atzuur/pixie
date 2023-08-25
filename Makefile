CC = gcc
BIN = pixie
CFLAGS = -Wall -Wextra -Wpedantic -Wno-gnu-zero-variadic-macro-arguments
CFLAGS_DEBUG = -ggdb -fsanitize=address,undefined -DDEBUG
CFLAGS_RELEASE = -O3 -flto -DNDEBUG

LIBS = $(shell pkg-config --cflags --libs libavcodec libavformat libavutil) -lm
FILES = src/*.c src/util/*.c

.PHONY = release

release:
	$(CC) $(FILES) $(CFLAGS) $(CFLAGS_RELEASE) $(LIBS) -o $(BIN)

debug:
	$(CC) $(FILES) $(CFLAGS) $(CFLAGS_DEBUG) $(LIBS) -o $(BIN).out

clean:
	rm -f $(BIN)

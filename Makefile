CC = gcc
BIN = pixie
CFLAGS = -Wall -Wextra
CFLAGS_DEBUG = -pedantic -g3 -Og
CFLAGS_RELEASE = -O3 -march=native

LIBS = $(shell pkg-config --cflags --libs libavcodec libavformat libavutil)
FILES = src/*.c src/util/*.c src/util/containers/libcontainers.a

SUB = src/util/containers

.PHONY = release

release: submodules
	$(CC) $(FILES) $(CFLAGS) $(CFLAGS_RELEASE) $(LIBS) -o $(BIN)

debug: submodules_debug
	$(CC) $(FILES) $(CFLAGS) $(CFLAGS_DEBUG) $(LIBS) -o $(BIN)

clean:
	rm -f $(BIN)
	$(foreach mod, $(SUB), +$(MAKE) -C $(mod) clean)

submodules:
	$(foreach mod, $(SUB), +$(MAKE) -C $(mod))

submodules_debug:
	$(foreach mod, $(SUB), +$(MAKE) -C $(mod) debug)

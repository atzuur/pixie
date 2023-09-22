CC = gcc
BIN = pixie
CFLAGS = -Wall -Wextra -Wpedantic -Wno-gnu-zero-variadic-macro-arguments -Wno-dollar-in-identifier-extension -Wno-for-loop-analysis -Iincl
CFLAGS_DEBUG = -ggdb -fsanitize=address,undefined -DDEBUG
CFLAGS_RELEASE = -O3 -flto -DNDEBUG

LIBS = -lm
ifdef OS # windows
FF_LIBS = -Lbuild\ffmpeg\bin -Lbuild\ffmpeg\lib -lavcodec -lavformat -lavutil -Ibuild\ffmpeg\include # assume there's a shared build in build/ffmpeg/
else
FF_LIBS = $(shell sh ./scripts/get-ffmpeg.sh)
ifeq ($(FF_LIBS),)
	exit 1
endif
endif

LIBS += $(FF_LIBS)

FILES = src/*.c src/util/*.c app/*.c

.PHONY = release

release:
	$(CC) $(FILES) $(CFLAGS) $(CFLAGS_RELEASE) $(LIBS) -o $(BIN)

debug:
	$(CC) $(FILES) $(CFLAGS) $(CFLAGS_DEBUG) $(LIBS) -o $(BIN)

clean:
ifdef OS
	del /f $(BIN).exe
	rmdir /s /q build
else
	rm -f $(BIN)
	rm -rf build
endif

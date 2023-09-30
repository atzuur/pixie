CC = gcc
BIN = pixie

CFLAGS = -Wall -Wextra -Wpedantic -Wshadow -Wno-gnu-zero-variadic-macro-arguments -Wno-dollar-in-identifier-extension -Wno-for-loop-analysis -std=c2x
CFLAGS_DEBUG = -ggdb -DDEBUG
CFLAGS_RELEASE = -O3 -flto -DNDEBUG
CFLAGS_EXTRA =

INCLUDES = -Iincl
LIBS = -lm

ifdef OS # windows
# assume there's a shared build in build/ffmpeg/
FF_LIBS = -Lbuild/ffmpeg/lib -lavcodec -lavformat -lavutil
INCLUDES += -Ibuild/ffmpeg/include
override CFLAGS_EXTRA += -D__USE_MINGW_ANSI_STDIO # https://stackoverflow.com/a/68902645/18756717
else
CFLAGS_DEBUG += -fsanitize=address,undefined
FF_LIBS = $(shell sh ./scripts/get-ff-libs.sh)
ifeq ($(FF_LIBS), )
	exit 1
endif
endif

ifeq ($(shell basename "$(CC)" .exe), clang)
override CFLAGS_EXTRA += -fuse-ld=lld
endif

LIBS += $(FF_LIBS)
FILES = src/*.c src/util/*.c app/*.c

default: gen-compile_flags-txt release

release:
	$(CC) $(FILES) $(INCLUDES) $(CFLAGS) $(CFLAGS_RELEASE) $(CFLAGS_EXTRA) $(LIBS) -o build/$(BIN)

debug:
	$(CC) $(FILES) $(INCLUDES) $(CFLAGS) $(CFLAGS_DEBUG) $(CFLAGS_EXTRA) $(LIBS) -o build/$(BIN)

clean:
	rm -rf build &

gen-compile_flags-txt:
	@sh ./scripts/gen-compile_flags-txt.sh "$(INCLUDES) $(CFLAGS)"

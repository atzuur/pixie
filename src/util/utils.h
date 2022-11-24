#pragma once
#include <assert.h>
#include <stdio.h>

// should be max(1, nproc)
int get_available_threads();

// dest should be at least 256 bytes
void last_errstr(char* dest, int err);

// not recursive! (no parent folders are created)
int create_folder(char* path);

// sleep for `ms` milliseconds
void sleep_ms(int ms);

// assert with optional block on failure
#define assert__(x) for (; !(x); assert(x))

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include <errno.h>
#define last_errcode() errno

#elif defined(_WIN32)
#include <windows.h>
#define last_errcode() GetLastError()
#endif

// automated error message
#define throw_msg(func, err)                                                                  \
    do {                                                                                      \
        char errstr[256];                                                                     \
        last_errstr(errstr, 0);                                                               \
        fprintf(stderr, "%s() failed at %s:%d : %s (%d)\n", func, __FILE__, __LINE__, errstr, \
                err);                                                                         \
    } while (0)

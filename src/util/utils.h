#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// should be max(1, nproc)
int get_available_threads();

/**
 * convert the last error code to a string
 * uses strerror on unix and FormatMessage on windows
 *
 * @param dest buffer to write to, should be at least 256 bytes
 * @param err error code to convert, if 0, the errno or GetLastError() is used
 */
void last_errstr(char* dest, int err);

// not recursive! (no parent folders are created)
int create_folder(char* path);

// sleep for `ms` milliseconds
void sleep_ms(int ms);

// assert with block on failure
#define assert_or(x) for (; !(x); assert(x))

void oom(size_t alloc_size);

static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool is_int(char* str) {

    while (*str) {
        if (!is_digit(*str++)) {
            return false;
        }
    }

    return true;
}

static inline int ceil_div(int a, int b) {
    return 1 + (a - 1) / b;
}

// if `*ptr` is not NULL, free it and set it to NULL
static inline void free_s(void* ptr) {

    void** p = (void**)ptr;

    if (*p) {
        free(*p);
        *p = NULL;
    }
}

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include <errno.h>
#include <unistd.h>

#define PATH_SEP "/"
#define last_errcode() errno

#elif defined(_WIN32)
#include <io.h> // _access
#include <windows.h>
#define F_OK 0

#define PATH_SEP "\\"
#define last_errcode() GetLastError()

#else
#error "Unsupported platform"
#endif

// automated error message
// usage: throw_msg("function_name", returned_error_code);
// use for os-related functions that return standard error codes
static inline void throw_msg(const char* func, int err) {

    char errstr[256];
    last_errstr(errstr, err);

    fprintf(stderr, "%s() failed at %s:%d : %s (%d)\n", func, __FILE__, __LINE__, errstr, err);
}

static inline bool file_exists(char* path) {
    return access(path, F_OK) == 0;
}
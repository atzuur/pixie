#pragma once

#include <assert.h>
#include <stdbool.h>

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include <errno.h>
#include <unistd.h>

#define PATH_SEP "/"
#define $last_errcode() errno

#elif defined(_WIN32)

#include <io.h> // _access
#include <windows.h>

#define PATH_SEP "\\"
#define $last_errcode() GetLastError()

#define access _access
#define F_OK 0

#else
#error "Unsupported platform"
#endif

// should be max(1, nproc)
int get_available_threads(void);

/**
 * convert the last error code to a string
 * uses strerror_r on unix and FormatMessage on windows
 *
 * @param dest buffer to write to, should be at least 256 bytes
 * @param err error code to convert, if 0, errno or GetLastError() is used
 * @return `dest`
 */
char* last_errstr(char* dest, int err);

// not recursive! (no parent folders are created)
int create_folder(const char* path);

/**
 * get the basename of `path`
 * @return pointer to the start of the basename in the path
 */
char* get_basename(const char* path);

// sleep for `ms` milliseconds
void sleep_ms(int ms);

// assert with block on failure
#define $assert_or(x) for (; !(x); assert(x))

// print an error message about a failed allocation
void oom(size_t alloc_size);

// ceil(`a` / `b`)
int ceil_div(int a, int b);

// if `*ptr` is not NULL, free it and set it to NULL
void free_s(void* ptr);

// check if `path` exists
bool file_exists(const char* path);

/**
 * print the last error location, code and message to stderr
 * @param func (char*) name of the function that failed
 * @param err (int) error code, if 0, the last error code is used
 */
#define $throw_msg(func, err)                                                          \
    fprintf(stderr, "%s() failed at %s:%d : %s (code %d)\n", __FILE__, __LINE__, func, \
            last_errstr(&(char[256]), err), err)

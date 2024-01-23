#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

// same as ffmpeg's AVERROR(x) for consistency
#define PXERROR(x) (-(x))

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include <errno.h>
#include <unistd.h>

#define PX_PATH_SEP "/"
#define PX_LAST_OS_ERR() errno

#define aligned_free(ptr) free(ptr)

#elif defined(_WIN32)

#include <io.h> // _access
#include <windows.h>

#define PX_PATH_SEP "\\"
#define PX_LAST_OS_ERR() (int)GetLastError()

#define access _access
#define F_OK 0

#define aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
#define aligned_free(ptr) _aligned_free(ptr)

#else
#error "Unsupported platform"
#endif

// should be max(1, nproc)
int px_get_available_threads(void);

/**
 * convert the last error code to a string
 * uses strerror_r on unix and FormatMessage on windows
 *
 * @param dest buffer to write to, should be at least 256 bytes
 * @param err error code to convert, if 0, errno or GetLastError() is used
 * @return `dest`
 */
char* px_last_os_errstr(char* dest, int err);

// not recursive! (no parent folders are created)
int px_create_folder(const char* path);

/**
 * get the basename of `path`
 * @return pointer to the start of the basename in the path
 */
char* px_get_basename(const char* path);

// sleep for `ms` milliseconds
void px_sleep_ms(int ms);

// print an error message about a failed allocation
void px_oom_msg(size_t alloc_size);

// ceil(`a` / `b`)
int px_ceil_div(int a, int b);

// if `*ptr` is not NULL, free it and set it to NULL
void px_free(void* ptr);

// check if `path` exists
bool px_file_exists(const char* path);

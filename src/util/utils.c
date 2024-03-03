#include "../internals.h"

#include <pixie/util/utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: change to px_log maybe?
void px_oom_msg(size_t alloc_size) {
    fprintf(stderr, "Memory allocation of %zu bytes failed\n", alloc_size);
}

char* px_get_basename(const char* path) {
    return strrchr(path, *PX_PATH_SEP);
}

void px_strip_str_end(char* str) {
    char* end = str + strlen(str) - 1;
    while (end >= str && isspace(*end)) {
        *end-- = '\0';
    }
}

int px_ceil_div(int a, int b) {
    return 1 + (a - 1) / b;
}

void px_free(void* ptr) {
    void** p = (void**)ptr;
    if (*p) {
        free(*p);
        *p = NULL;
    }
}

bool px_file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

#ifdef PX_PLATFORM_UNIX
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

int nanosleep(const struct timespec* req, struct timespec* rem);

int px_get_available_threads(void) {
    int ret = (int)sysconf(_SC_NPROCESSORS_ONLN);
    return ret < 1 ? 1 : ret;
}

char* px_last_os_errstr(char* dest, int err) {
    int errcode = err ? err : errno;
    strncpy(dest, strerror(errcode), 256);
    return dest;
}

int px_create_folder(const char* path) {
    int ret = mkdir(path, 0777);
    if (ret < 0) {
        if (errno == EEXIST)
            return 0;

        OS_THROW_MSG("mkdir", errno);
        return PXERROR(errno);
    }
    return 0;
}

void px_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, &ts);
}

void* px_aligned_alloc(size_t align, size_t size) {
    return aligned_alloc(align, size);
}

void px_aligned_free(void* ptr) {
    free(ptr);
}

#elif defined(PX_PLATFORM_WINDOWS)

int px_get_available_threads(void) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int)info.dwNumberOfProcessors;
}

char* px_last_os_errstr(char* dest, int err) {
    DWORD errcode = err ? (DWORD)err : GetLastError();
    DWORD written = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errcode,
                                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), dest, 256, NULL);
    if (!written) {
        strncpy(dest, "(message unavailable)", 256);
    } else {
        px_strip_str_end(dest);
    }

    return dest;
}

int px_create_folder(const char* path) {
    if (!CreateDirectoryA(path, 0)) {
        int err = (int)GetLastError();
        if (err == ERROR_ALREADY_EXISTS)
            return 0;

        OS_THROW_MSG("CreateDirectoryA", err);
        return PXERROR(err);
    }
    return 0;
}

void px_sleep_ms(int ms) {
    assert(ms >= 0);
    Sleep((DWORD)ms);
}

void* px_aligned_alloc(size_t align, size_t size) {
    return _aligned_malloc(size, align);
}

void px_aligned_free(void* ptr) {
    _aligned_free(ptr);
}

char* strndup(const char* src, size_t len) {
    char* dest = malloc(len + 1);
    if (!dest)
        return NULL;

    memcpy(dest, src, len);
    dest[len] = '\0';
    return dest;
}
#endif

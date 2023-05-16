#include "utils.h"
#include <string.h>

void oom(size_t alloc_size) {
    fprintf(stderr, "Memory allocation of %zu bytes failed\n", alloc_size);
}

char* get_basename(const char* path) {
    return strrchr(path, *PATH_SEP);
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_int(char* str) {

    while (*str) {
        if (!is_digit(*str++)) {
            return false;
        }
    }

    return true;
}

int ceil_div(int a, int b) {
    return 1 + (a - 1) / b;
}

// if `*ptr` is not NULL, free it and set it to NULL
void free_s(void* ptr) {

    void** p = (void**)ptr;

    if (*p) {
        free(*p);
        *p = NULL;
    }
}

bool file_exists(char* path) {
    return access(path, F_OK) == 0;
}

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

int nanosleep(const struct timespec* req, struct timespec* rem);

int get_available_threads(void) {
    long ret = sysconf(_SC_NPROCESSORS_ONLN);
    return ret < 1 ? 1 : ret;
}

char* last_errstr(char* dest, int err) {
    int errcode = err ? err : errno;
    strerror_r(errcode, dest, 256);
    return dest;
}

int create_folder(char* path) {
    int ret = mkdir(path, 0777);
    if (ret < 0)
        return errno != EEXIST;
    return 1;
}

void sleep_ms(int ms) {
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = (ms % 1000) * 1000000;

    nanosleep(&ts, &ts);
}

#elif defined(_WIN32)

int get_available_threads(void) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

char* last_errstr(char* dest, int err) {
    int errcode = err ? err : GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errcode,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), dest, 256, NULL);
    return dest;
}

int create_folder(char* path) {
    if (!CreateDirectoryA(path, 0))
        return GetLastError() != ERROR_ALREADY_EXISTS;
    return 1;
}

void sleep_ms(int ms) {
    Sleep(ms);
}
#endif

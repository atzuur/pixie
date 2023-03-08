#include "utils.h"
#include <string.h>

void oom(size_t alloc_size) {
    fprintf(stderr, "Memory allocation of %zu bytes failed\n", alloc_size);
}

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

int nanosleep(const struct timespec* req, struct timespec* rem);

int get_available_threads() {
    long ret = sysconf(_SC_NPROCESSORS_ONLN);
    return ret < 1 ? 1 : ret;
}

void last_errstr(char* dest, int err) {
    int errcode = err ? err : errno;
    strerror_r(errcode, dest, 256);
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

int get_available_threads() {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

void last_errstr(char* dest, int err) {
    int errcode = err ? err : GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errcode,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), dest, 256, NULL);
}

int create_folder(char* path) {
    if (!CreateDirectoryA(path, 0))
        return GetLastError() != ERROR_ALREADY_EXISTS;
    return 1;
}

void sleep_milli(int ms) {
    Sleep(ms);
}
#endif

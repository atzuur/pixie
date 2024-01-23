#include <pixie/util/utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void px_oom_msg(size_t alloc_size) {
    fprintf(stderr, "Memory allocation of %zu bytes failed\n", alloc_size);
}

char* px_get_basename(const char* path) {
    return strrchr(path, *PX_PATH_SEP);
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

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

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
    if (ret < 0)
        return errno != EEXIST;
    return 1;
}

void px_sleep_ms(int ms) {
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = (ms % 1000) * 1000000;

    nanosleep(&ts, &ts);
}

#elif defined(_WIN32)

int px_get_available_threads(void) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int)info.dwNumberOfProcessors;
}

char* px_last_os_errstr(char* dest, int err) {
    DWORD errcode = err ? (DWORD)err : GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errcode,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), dest, 256, NULL);
    return dest;
}

int px_create_folder(const char* path) {
    if (!CreateDirectoryA(path, 0))
        return GetLastError() != ERROR_ALREADY_EXISTS;
    return 1;
}

void px_sleep_ms(int ms) {
    Sleep((DWORD)ms);
}
#endif

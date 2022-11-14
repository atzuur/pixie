#include "utils.h"
#include <string.h>

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int nanosleep(const struct timespec* req, struct timespec* rem);

int get_available_threads() {
    long ret = sysconf(_SC_NPROCESSORS_ONLN);
    return ret < 1 ? 1 : ret;
}

// dest should be at least 256 bytes
void get_os_error(char* dest) {
    strcpy(dest, strerror(errno));
}

// return 0 on failure
int create_folder(char* path) {
    int ret = mkdir(path, 0777);
    if (ret < 0)
        return errno == EEXIST;
    return 1;
}

void sleep_ms(int ms) {
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = (ms % 1000) * 1000000;

    nanosleep(&ts, &ts);
}

#elif defined(_WIN32)

#include <windows.h>

int get_available_threads() {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

// dest should be at least 256 bytes
void get_os_error(char* dest) {
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), dest, 256, NULL);
}

// return 0 on failure
int create_folder(char* path) {
    if (!CreateDirectoryA(path, 0))
        return GetLastError() == ERROR_ALREADY_EXISTS;
    return 1;
}

void sleep_milli(int ms) {
    Sleep(ms);
}

#endif

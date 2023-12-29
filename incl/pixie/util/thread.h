// s/o luminance
// https://github.com/unknownopponent/C_Simple_Thread

#pragma once

#include <stdbool.h>

#ifdef _WIN32
#define WIN32_THREADS
#include <windows.h>

#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#define POSIX_THREADS
#include <pthread.h>

#elif __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#define C11_THREADS
#include <stdio.h>
#include <threads.h>
#include <errno.h>

static inline const char* c11_thrd_strerror(int err) {
    switch (err) {
        case thrd_success:
            return strerror(0);
        case thrd_nomem:
            return strerror(ENOMEM);
        case thrd_timedout:
            return strerror(ETIMEDOUT);
        case thrd_busy:
            return strerror(EBUSY);
        case thrd_error: // fallthrough
        default:
            return "Unspecified thread error";
    }
}

#define $c11_thrd_throw_msg(func, err)                                                 \
    fprintf(stderr, "%s() failed at %s:%d : %s (code %d)\n", func, __FILE__, __LINE__, \
            c11_thrd_strerror(err), err)

#else
#error No thread implementation available!
#endif

typedef int (*PXThreadFunc)(void*);

typedef struct PXThread {
    PXThreadFunc func;
    void* args;

    bool done;

#ifdef C11_THREADS
    thrd_t thrd;
#elif defined(WIN32_THREADS)
    HANDLE thrd;
#elif defined(POSIX_THREADS)
    pthread_t thrd;
#endif

} PXThread;

// launch thread with function `thread->func` and arguments `thread->args`
int px_thrd_launch(PXThread* thread);

// wait for `thread` to terminate, thread return value is stored in `*return_code`
int px_thrd_join(PXThread* thread, int* return_code);

// terminate calling thread with code `ret`
void px_thrd_exit(int ret);

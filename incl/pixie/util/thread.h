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

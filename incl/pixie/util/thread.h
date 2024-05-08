// s/o luminance
// https://github.com/unknownopponent/C_Simple_Thread

#pragma once

#include <pixie/util/platform.h>

#include <stdatomic.h>

#ifdef PX_PLATFORM_WINDOWS
#define PX_THREADS_WIN32
#include <windows.h>

#elif defined(PX_PLATFORM_UNIX)
#define PX_THREADS_POSIX
#include <pthread.h>

#elif __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#define PX_THREADS_C11
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

    atomic_bool done;

#ifdef PX_THREADS_C11
    thrd_t thrd;
#elif defined(PX_THREADS_WIN32)
    HANDLE thrd;
#elif defined(PX_THREADS_POSIX)
    pthread_t thrd;
#endif

} PXThread;

// launch thread with function `thread->func` and arguments `thread->args`
int px_thrd_launch(PXThread* thread);

// wait for `thread` to terminate, thread return value is stored in `*return_code`
int px_thrd_join(PXThread* thread, int* return_code);

// terminate calling thread with code `ret`
void px_thrd_exit(int ret);

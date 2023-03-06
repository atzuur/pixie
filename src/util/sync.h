// s/o luminance
// https://github.com/unknownopponent/C_Simple_Thread
//
// all of these throw because propagating would've been a pain
// they should all be fatal anyways

#pragma once

#include "utils.h"

#include <stdbool.h>
#include <stdint.h> // intptr_t
#include <string.h> // memset

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#define C11_THREADS
#include <threads.h>

// fuck c11 for not using errno
void c11thrd_err_strerror_r(int err, char* dest, size_t len);

static inline void c11thrd_throw_msg(const char* func, int err) {

    char errstr[256];
    c11thrd_err_strerror_r(err, errstr, 256u);

    fprintf(stderr, "%s() failed at %s:%d : %s (%d)\n", func, __FILE__, __LINE__, errstr, err);
}

#elif defined(_WIN32) && !defined(C11_THREADS)
#define WIN32_THREADS
#include <windows.h>

#elif defined(__unix__) && !defined(C11_THREADS)
#define POSIX_THREADS
#include <pthread.h>

#elif !defined(C11_THREADS) && !defined(_WIN32) && !defined(__unix__)
#error No thread implementation available!
#endif

typedef int (*PXThreadFunc)(void*);

typedef struct PXThread {
    PXThreadFunc func;
    void* args;

    bool done;

#ifdef C11_THREADS
    thrd_t thrd;
#endif
#ifdef WIN32_THREADS
    HANDLE thrd;
#endif
#ifdef POSIX_THREADS
    pthread_t thrd;
#endif

} PXThread;

typedef struct PXMutex {
#ifdef C11_THREADS
    mtx_t mtx;
#endif
#ifdef WIN32_THREADS
    HANDLE mtx;
#endif
#ifdef POSIX_THREADS
    pthread_mutex_t mtx;
#endif
} PXMutex;

typedef struct PXCond {
#ifdef C11_THREADS
    cnd_t cond;
#endif
#ifdef WIN32_THREADS
    HANDLE cond;
#endif
#ifdef POSIX_THREADS
    pthread_cond_t cond;
#endif
} PXCond;

// launch thread with function `thread->func` and arguments `thread->args`
void px_thrd_launch(PXThread* thread);

// wait for `thread` to terminate, thread return value is stored in `*return_code`
void px_thrd_join(PXThread* thread, int* return_code);

// terminate calling thread with code `ret`
void px_thrd_exit(int ret);

// initialize mutex
void px_mtx_create(PXMutex* mutex);

// deinitialize mutex
void px_mtx_destroy(PXMutex* mutex);

// lock mutex, block until available
void px_mtx_lock(PXMutex* mutex);

// try to lock mutex, always return immediately
void px_mtx_try_lock(PXMutex* mutex);

// release mutex
void px_mtx_unlock(PXMutex* mutex);

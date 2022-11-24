// s/o luminance
// https://github.com/unknownopponent/C_Simple_Thread
//
// all of these throw because propagating would've been a pain
// they should all be fatal anyways

#pragma once

#include "utils.h"

#include <stdint.h> // intptr_t
#include <string.h> // memset

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#define C11_THREADS
#include <threads.h>

#elif defined(_WIN32) && !defined(C11_THREADS)
#define WIN32_THREADS
#include <windows.h>

#elif defined(__unix__) && !defined(C11_THREADS)
#define POSIX_THREADS
#include <pthread.h>

#elif !defined(C11_THREADS) && !defined(_WIN32) && !defined(__unix__)
#error No thread implementation available!
#endif

typedef struct PXThread {
    void* (*func)(void*);
    void* args;

#ifdef C11_THREADS
    thrd_t thread_handle;
#endif
#ifdef WIN32_THREADS
    HANDLE thread_handle;
#endif
#ifdef POSIX_THREADS
    pthread_t thread_handle;
#endif

} PXThread;

void pxt_create(PXThread* thread);

// thread return value is stored in `*return_code`
void pxt_join(PXThread* thread, int* return_code);

// terminate calling thread with code `ret`
void pxt_exit(int ret);

inline void pxt_create(PXThread* thread) {
    assert(thread->func);

#ifdef C11_THREADS

    int res = thrd_create(&thread->thread_handle, thread->func, thread->args);
    assert__(res == thrd_success) {
        throw_msg("thrd_create", res);
    }

#endif
#ifdef WIN32_THREADS

    thread->thread_handle =
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)thread->func, thread->args, 0, 0);

    assert__(thread->thread_handle) {
        throw_msg("CreateThread", last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_create(&thread->thread_handle, 0, thread->func, thread->args);
    assert__(res == 0) {
        throw_msg("pthread_create", res);
    }

#endif
}

inline void pxt_join(PXThread* thread, int* return_code) {
    assert(thread->thread_handle);

#ifdef C11_THREADS

    int res = thrd_join(thread->thread_handle, return_code);
    assert__(res == thrd_success) {
        throw_msg("thrd_join", res);
    }

    memset(&thread->thread_handle, '\0', sizeof(thrd_t));

#endif
#ifdef WIN32_THREADS

    DWORD res = WaitForSingleObject(thread->thread_handle, INFINITE);
    assert__(res == WAIT_OBJECT_0) {
        throw_msg("WaitForSingleObject", last_errcode());
    }

    DWORD ret;
    BOOL res2 = GetExitCodeThread(thread->thread_handle, &ret);
    assert__(res2) {
        throw_msg("GetExitCodeThread", last_errcode());
    }

    res2 = CloseHandle(thread->thread_handle);
    assert__(res2) {
        throw_msg("CloseHandle", last_errcode());
    }

    memset(&thread->thread_handle, '\0', sizeof(HANDLE));

    *return_code = (int)ret;

#endif
#ifdef POSIX_THREADS

    void* ret;
    int res = pthread_join(thread->thread_handle, &ret);
    assert__(res == 0) {
        throw_msg("pthread_join", res);
    }

    memset(&thread->thread_handle, '\0', sizeof(pthread_t));

    *return_code = (int)(intptr_t)ret;

#endif
}

inline void pxt_exit(int ret) {
#ifdef C11_THREADS
    thrd_exit(ret);
#endif
#ifdef WIN32_THREADS
    ExitThread(ret);
#endif
#ifdef POSIX_THREADS
    pthread_exit((void*)(intptr_t)ret);
#endif
}

#pragma once

#include "utils.h"

#include <string.h>

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#define C11_THREADS
#include <threads.h>
#endif

#if defined(_WIN32) && !defined(C11_THREADS)
#define WIN32_THREADS
#include <windows.h>
#endif

#if defined(__unix__) && !defined(C11_THREADS)
#define POSIX_THREADS
#include <pthread.h>
#endif

#if !defined(C11_THREADS) && !defined(_WIN32) && !defined(__unix__)
#error No thread implementation available!
#endif

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

void pxm_create(PXMutex* mutex);
void pxm_destroy(PXMutex* mutex);

void pxm_lock(PXMutex* mutex);
void pxm_try_lock(PXMutex* mutex);
void pxm_unlock(PXMutex* mutex);

inline void pxm_create(PXMutex* mutex) {
#ifdef C11_THREADS

    int res = mtx_init(&mutex->mtx, mtx_plain);
    assert__(res == thrd_success) {
        throw_msg("mtx_init", res);
    }

#endif
#ifdef WIN32_THREADS

    mutex->mtx = CreateMutexA(0, 0, 0);
    assert__(mutex->mtx) {
        throw_msg("CreateMutexA", last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_mutex_init(&mutex->mtx, 0);
    assert__(res == 0) {
        throw_msg("pthread_mutex_init", res);
    }

#endif
}

inline void pxm_destroy(PXMutex* mutex) {
#ifdef C11_THREADS

    mtx_destroy(&mutex->mtx);

#endif
#ifdef WIN32_THREADS

    BOOL res = CloseHandle(mutex->mtx);
    assert__(res) {
        throw_msg("CloseHandle", last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_mutex_destroy(&mutex->mtx);
    assert__(res == 0) {
        throw_msg("pthread_mutex_destroy", res);
    }

#endif
    memset(mutex, '\0', sizeof(PXMutex));
}

inline void pxm_lock(PXMutex* mutex) {
#ifdef C11_THREADS

    int res = mtx_lock(&mutex->mtx);
    assert__(res == thrd_success) {
        throw_msg("mtx_lock", res);
    }

#endif
#ifdef WIN32_THREADS

    DWORD res = WaitForSingleObject(mutex->mtx, INFINITE);
    assert__(res == WAIT_OBJECT_0) {
        throw_msg("WaitForSingleObject", last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_mutex_lock(&mutex->mtx);
    assert__(res == 0) {
        throw_msg("pthread_mutex_lock", res);
    }

#endif
}

inline void pxm_try_lock(PXMutex* mutex) {
#ifdef C11_THREADS

    int res = mtx_trylock(&mutex->mtx);
    assert__(res == thrd_success || res == thrd_busy) {
        throw_msg("mtx_trylock", res);
    }

#endif
#ifdef WIN32_THREADS

    DWORD res = WaitForSingleObject(mutex->mtx, 0);
    assert__(res == WAIT_OBJECT_0 || res == WAIT_TIMEOUT) {
        throw_msg("WaitForSingleObject", last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_mutex_trylock(&mutex->mtx);
    assert__(res == 0) {
        throw_msg("pthread_mutex_trylock", res);
    }

#endif
}

void pxm_unlock(PXMutex* mutex) {
#ifdef C11_THREADS

    int res = mtx_unlock(&mutex->mtx);
    assert__(res == thrd_success) {
        throw_msg("mtx_unlock", res);
    }

#endif
#ifdef WIN32_THREADS

    BOOL res = ReleaseMutex(mutex->mtx);
    assert__(res) {
        throw_msg("ReleaseMutex", last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_mutex_unlock(&mutex->mtx);
    assert__(res == 0) {
        throw_msg("pthread_mutex_unlock", res);
    }

#endif
}

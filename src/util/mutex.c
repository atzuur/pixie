#include "utils.h"

#include <pixie/util/sync.h>

void px_mtx_create(PXMutex* mutex) {
#ifdef C11_THREADS

    int res = mtx_init(&mutex->mtx, mtx_plain);
    $assert_or(res == thrd_success) {
        $c11_thrd_throw_msg("mtx_init", res);
    }

#endif
#ifdef WIN32_THREADS

    mutex->mtx = CreateMutexA(0, 0, 0);
    $assert_or(mutex->mtx) {
        $throw_msg("CreateMutexA", $last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_mutex_init(&mutex->mtx, 0);
    $assert_or(res == 0) {
        $throw_msg("pthread_mutex_init", res);
    }

#endif
}

void px_mtx_destroy(PXMutex* mutex) {
#ifdef C11_THREADS

    mtx_destroy(&mutex->mtx);

#endif
#ifdef WIN32_THREADS

    BOOL res = CloseHandle(mutex->mtx);
    $assert_or(res) {
        $throw_msg("CloseHandle", $last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_mutex_destroy(&mutex->mtx);
    $assert_or(res == 0) {
        $throw_msg("pthread_mutex_destroy", res);
    }

#endif
    memset(mutex, '\0', sizeof(PXMutex));
}

void px_mtx_lock(PXMutex* mutex) {
#ifdef C11_THREADS

    int res = mtx_lock(&mutex->mtx);
    $assert_or(res == thrd_success) {
        $c11_thrd_throw_msg("mtx_lock", res);
    }

#endif
#ifdef WIN32_THREADS

    DWORD res = WaitForSingleObject(mutex->mtx, INFINITE);
    $assert_or(res == WAIT_OBJECT_0) {
        $throw_msg("WaitForSingleObject", $last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_mutex_lock(&mutex->mtx);
    $assert_or(res == 0) {
        $throw_msg("pthread_mutex_lock", res);
    }

#endif
}

void px_mtx_try_lock(PXMutex* mutex) {
#ifdef C11_THREADS

    int res = mtx_trylock(&mutex->mtx);
    $assert_or(res == thrd_success || res == thrd_busy) {
        $c11_thrd_throw_msg("mtx_trylock", res);
    }

#endif
#ifdef WIN32_THREADS

    DWORD res = WaitForSingleObject(mutex->mtx, 0);
    $assert_or(res == WAIT_OBJECT_0 || res == WAIT_TIMEOUT) {
        $throw_msg("WaitForSingleObject", $last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_mutex_trylock(&mutex->mtx);
    $assert_or(res == 0) {
        $throw_msg("pthread_mutex_trylock", res);
    }

#endif
}

void px_mtx_unlock(PXMutex* mutex) {
#ifdef C11_THREADS

    int res = mtx_unlock(&mutex->mtx);
    $assert_or(res == thrd_success) {
        $c11_thrd_throw_msg("mtx_unlock", res);
    }

#endif
#ifdef WIN32_THREADS

    BOOL res = ReleaseMutex(mutex->mtx);
    $assert_or(res) {
        $throw_msg("ReleaseMutex", $last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_mutex_unlock(&mutex->mtx);
    $assert_or(res == 0) {
        $throw_msg("pthread_mutex_unlock", res);
    }

#endif
}

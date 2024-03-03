#include "../internals.h"

#include <pixie/util/utils.h>
#include <pixie/util/thread.h>

#include <stdint.h>
#include <string.h>

#ifdef PX_THREADS_C11
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

#define CTHRD_THROW_MSG(func, err)                                                    \
    fprintf(stderr, "%s() failed at %s:%d: %s (code %d)\n", func, __FILE__, __LINE__, \
            c11_thrd_strerror(err), err)
#endif

int px_thrd_launch(PXThread* thread) {
    assert(thread->func);

#ifdef PX_THREADS_C11
    int res = thrd_create(&thread->thrd, thread->func, thread->args);
    if (res != thrd_success)
        CTHRD_THROW_MSG("thrd_create", res);
    return res;
#endif
#ifdef PX_THREADS_WIN32
    thread->thrd = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)thread->func, thread->args, 0, 0);
    if (!thread->thrd) {
        OS_THROW_MSG("CreateThread", 0);
        return 1;
    }
    return 0;
#endif
#ifdef PX_THREADS_POSIX
    typedef void* (*pthread_func)(void*);
    int res = pthread_create(&thread->thrd, 0, (pthread_func)thread->func, thread->args);
    if (res != 0)
        OS_THROW_MSG("pthread_create", res);
    return res;
#endif
}

int px_thrd_join(PXThread* thread, int* return_code) {
    assert(thread->thrd);

#ifdef PX_THREADS_C11
    int res = thrd_join(thread->thrd, return_code);
    if (res != thrd_success) {
        CTHRD_THROW_MSG("thrd_join", res);
        goto end;
    }
#endif
#ifdef PX_THREADS_WIN32
    DWORD res = WaitForSingleObject(thread->thrd, INFINITE);
    if (res != WAIT_OBJECT_0) {
        OS_THROW_MSG("WaitForSingleObject", 0);
        goto end;
    }

    DWORD ret;
    res = !(DWORD)GetExitCodeThread(thread->thrd, &ret);
    if (res) {
        OS_THROW_MSG("GetExitCodeThread", 0);
        goto end;
    }

    res = !(DWORD)CloseHandle(thread->thrd);
    if (res) {
        OS_THROW_MSG("CloseHandle", 0);
        goto end;
    }

    *return_code = (int)ret;
#endif
#ifdef PX_THREADS_POSIX
    void* ret;
    int res = pthread_join(thread->thrd, &ret);
    if (res != 0) {
        OS_THROW_MSG("pthread_join", res);
        goto end;
    }

    *return_code = (int)(intptr_t)ret;
#endif

end:
    memset(&thread->thrd, 0, sizeof thread->thrd);
    return (int)res;
}

void px_thrd_exit(int ret) {
#ifdef PX_THREADS_C11
    thrd_exit(ret);
#endif
#ifdef PX_THREADS_WIN32
    ExitThread((DWORD)ret);
#endif
#ifdef PX_THREADS_POSIX
    pthread_exit((void*)(intptr_t)ret);
#endif
}

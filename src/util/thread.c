#include "utils.h"

#include <pixie/util/thread.h>

int px_thrd_launch(PXThread* thread) {
    assert(thread->func);

#ifdef C11_THREADS
    int res = thrd_create(&thread->thrd, thread->func, thread->args);
    if (res != thrd_success)
        $c11_thrd_throw_msg("thrd_create", res);
    return res;
#endif
#ifdef WIN32_THREADS
    thread->thrd = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)thread->func, thread->args, 0, 0);
    if (!thread->thrd) {
        $throw_msg("CreateThread", $last_errcode());
        return 1;
    }
    return 0;
#endif
#ifdef POSIX_THREADS
    typedef void* (*pthread_func)(void*);
    int res = pthread_create(&thread->thrd, 0, (pthread_func)thread->func, thread->args);
    if (res != 0)
        $throw_msg("pthread_create", res);
    return res;
#endif
}

int px_thrd_join(PXThread* thread, int* return_code) {
    assert(thread->thrd);

#ifdef C11_THREADS
    int res = thrd_join(thread->thrd, return_code);
    if (res != thrd_success) {
        $c11_thrd_throw_msg("thrd_join", res);
        goto end;
    }
#endif
#ifdef WIN32_THREADS
    DWORD res = WaitForSingleObject(thread->thrd, INFINITE);
    if (res != WAIT_OBJECT_0) {
        $throw_msg("WaitForSingleObject", $last_errcode());
        goto end;
    }

    DWORD ret;
    res = (DWORD)GetExitCodeThread(thread->thrd, &ret);
    if (!res) {
        $throw_msg("GetExitCodeThread", $last_errcode());
        goto end;
    }

    res = (DWORD)CloseHandle(thread->thrd);
    if (!res) {
        $throw_msg("CloseHandle", $last_errcode());
        goto end;
    }

    *return_code = (int)ret;
#endif
#ifdef POSIX_THREADS
    void* ret;
    int res = pthread_join(thread->thrd, &ret);
    if (res != 0) {
        $throw_msg("pthread_join", res);
        goto end;
    }

    *return_code = (int)(intptr_t)ret;
#endif

end:
    memset(&thread->thrd, 0, sizeof thread->thrd);
    return res;
}

void px_thrd_exit(int ret) {
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

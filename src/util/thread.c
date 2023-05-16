#include "sync.h"

void px_thrd_launch(PXThread* thread) {
    assert(thread->func);

#ifdef C11_THREADS

    int res = thrd_create(&thread->thrd, thread->func, thread->args);
    assert_or(res == thrd_success) {
        c11_thrd_throw_msg("thrd_create", res);
    }

#endif
#ifdef WIN32_THREADS

    thread->thrd = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)thread->func, thread->args, 0, 0);

    assert_or(thread->thrd) {
        throw_msg("CreateThread", last_errcode());
    }

#endif
#ifdef POSIX_THREADS

    int res = pthread_create(&thread->thrd, 0, thread->func, thread->args);
    assert_or(res == 0) {
        throw_msg("pthread_create", res);
    }

#endif
}

void px_thrd_join(PXThread* thread, int* return_code) {
    assert(thread->thrd);

#ifdef C11_THREADS

    int res = thrd_join(thread->thrd, return_code);
    assert_or(res == thrd_success) {
        c11_thrd_throw_msg("thrd_join", res);
    }

    memset(&thread->thrd, 0, sizeof(thrd_t));

#endif
#ifdef WIN32_THREADS

    DWORD res = WaitForSingleObject(thread->thrd, INFINITE);
    assert_or(res == WAIT_OBJECT_0) {
        throw_msg("WaitForSingleObject", last_errcode());
    }

    DWORD ret;
    BOOL res2 = GetExitCodeThread(thread->thrd, &ret);
    assert_or(res2) {
        throw_msg("GetExitCodeThread", last_errcode());
    }

    res2 = CloseHandle(thread->thrd);
    assert_or(res2) {
        throw_msg("CloseHandle", last_errcode());
    }

    memset(&thread->thrd, 0, sizeof(HANDLE));

    *return_code = (int)ret;

#endif
#ifdef POSIX_THREADS

    void* ret;
    int res = pthread_join(thread->thrd, &ret);
    assert_or(res == 0) {
        throw_msg("pthread_join", res);
    }

    memset(&thread->thrd, 0, sizeof(pthread_t));

    *return_code = (int)(intptr_t)ret;

#endif
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

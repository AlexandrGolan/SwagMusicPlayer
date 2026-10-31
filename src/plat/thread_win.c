/*
 * SMP - Swag Music Player
 * Thread abstraction for Win32.
 *
 * SPDX-License-Identifier: MIT
 */
#ifdef _WIN32

#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "smp/thread.h"

struct SmpMutex {
    CRITICAL_SECTION handle;
};

struct SmpCond {
    CONDITION_VARIABLE handle;
};

struct SmpThread {
    HANDLE handle;
    SmpThreadFn function;
    void *argument;
};

SmpMutex *smp_mutex_create(void)
{
    SmpMutex *mutex = (SmpMutex *)calloc(1, sizeof(*mutex));

    if (mutex == NULL) {
        return NULL;
    }
    InitializeCriticalSection(&mutex->handle);
    return mutex;
}

void smp_mutex_destroy(SmpMutex *mutex)
{
    if (mutex == NULL) {
        return;
    }
    DeleteCriticalSection(&mutex->handle);
    free(mutex);
}

void smp_mutex_lock(SmpMutex *mutex)
{
    if (mutex != NULL) {
        EnterCriticalSection(&mutex->handle);
    }
}

void smp_mutex_unlock(SmpMutex *mutex)
{
    if (mutex != NULL) {
        LeaveCriticalSection(&mutex->handle);
    }
}

SmpCond *smp_cond_create(void)
{
    SmpCond *cond = (SmpCond *)calloc(1, sizeof(*cond));

    if (cond == NULL) {
        return NULL;
    }
    InitializeConditionVariable(&cond->handle);
    return cond;
}

void smp_cond_destroy(SmpCond *cond)
{
    free(cond);
}

void smp_cond_wait(SmpCond *cond, SmpMutex *mutex)
{
    if (cond != NULL && mutex != NULL) {
        (void)SleepConditionVariableCS(&cond->handle, &mutex->handle, INFINITE);
    }
}

void smp_cond_signal(SmpCond *cond)
{
    if (cond != NULL) {
        WakeConditionVariable(&cond->handle);
    }
}

void smp_cond_broadcast(SmpCond *cond)
{
    if (cond != NULL) {
        WakeAllConditionVariable(&cond->handle);
    }
}

bool smp_cond_wait_timeout(SmpCond *cond, SmpMutex *mutex, int timeout_ms)
{
    BOOL signaled;

    if (cond == NULL || mutex == NULL || timeout_ms < 0) {
        return false;
    }

    signaled = SleepConditionVariableCS(&cond->handle, &mutex->handle, (DWORD)timeout_ms);
    return signaled != FALSE;
}

static DWORD WINAPI thread_trampoline(LPVOID argument)
{
    SmpThread *thread = (SmpThread *)argument;

    (void)thread->function(thread->argument);
    return 0;
}

SmpThread *smp_thread_start(SmpThreadFn function, void *argument)
{
    SmpThread *thread;

    if (function == NULL) {
        return NULL;
    }

    thread = (SmpThread *)calloc(1, sizeof(*thread));
    if (thread == NULL) {
        return NULL;
    }

    thread->function = function;
    thread->argument = argument;
    thread->handle = CreateThread(NULL, 0, thread_trampoline, thread, 0, NULL);
    if (thread->handle == NULL) {
        free(thread);
        return NULL;
    }
    return thread;
}

void smp_thread_join(SmpThread *thread)
{
    if (thread == NULL) {
        return;
    }

    if (thread->handle != NULL) {
        (void)WaitForSingleObject(thread->handle, INFINITE);
        CloseHandle(thread->handle);
    }
    free(thread);
}

#endif /* _WIN32 */

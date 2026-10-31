/*
 * SMP - Swag Music Player
 * Thread abstraction for POSIX platforms.
 *
 * SPDX-License-Identifier: MIT
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _WIN32

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "smp/thread.h"

struct SmpMutex {
    pthread_mutex_t handle;
};

struct SmpCond {
    pthread_cond_t handle;
};

struct SmpThread {
    pthread_t handle;
    SmpThreadFn function;
    void *argument;
    bool started;
};

SmpMutex *smp_mutex_create(void)
{
    SmpMutex *mutex = (SmpMutex *)calloc(1, sizeof(*mutex));

    if (mutex == NULL) {
        return NULL;
    }
    if (pthread_mutex_init(&mutex->handle, NULL) != 0) {
        free(mutex);
        return NULL;
    }
    return mutex;
}

void smp_mutex_destroy(SmpMutex *mutex)
{
    if (mutex == NULL) {
        return;
    }
    pthread_mutex_destroy(&mutex->handle);
    free(mutex);
}

void smp_mutex_lock(SmpMutex *mutex)
{
    if (mutex != NULL) {
        pthread_mutex_lock(&mutex->handle);
    }
}

void smp_mutex_unlock(SmpMutex *mutex)
{
    if (mutex != NULL) {
        pthread_mutex_unlock(&mutex->handle);
    }
}

SmpCond *smp_cond_create(void)
{
    SmpCond *cond = (SmpCond *)calloc(1, sizeof(*cond));

    if (cond == NULL) {
        return NULL;
    }

    {
        pthread_condattr_t attributes;

        pthread_condattr_init(&attributes);
        pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
        if (pthread_cond_init(&cond->handle, &attributes) != 0) {
            pthread_condattr_destroy(&attributes);
            free(cond);
            return NULL;
        }
        pthread_condattr_destroy(&attributes);
    }

    return cond;
}

void smp_cond_destroy(SmpCond *cond)
{
    if (cond == NULL) {
        return;
    }
    pthread_cond_destroy(&cond->handle);
    free(cond);
}

void smp_cond_wait(SmpCond *cond, SmpMutex *mutex)
{
    if (cond != NULL && mutex != NULL) {
        pthread_cond_wait(&cond->handle, &mutex->handle);
    }
}

void smp_cond_signal(SmpCond *cond)
{
    if (cond != NULL) {
        pthread_cond_signal(&cond->handle);
    }
}

void smp_cond_broadcast(SmpCond *cond)
{
    if (cond != NULL) {
        pthread_cond_broadcast(&cond->handle);
    }
}

bool smp_cond_wait_timeout(SmpCond *cond, SmpMutex *mutex, int timeout_ms)
{
    struct timespec deadline;
    int result;

    if (cond == NULL || mutex == NULL || timeout_ms < 0) {
        return false;
    }

    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }

    result = pthread_cond_timedwait(&cond->handle, &mutex->handle, &deadline);
    return result == 0;
}

static void *thread_trampoline(void *argument)
{
    SmpThread *thread = (SmpThread *)argument;

    (void)thread->function(thread->argument);
    return NULL;
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

    if (pthread_create(&thread->handle, NULL, thread_trampoline, thread) != 0) {
        free(thread);
        return NULL;
    }

    thread->started = true;
    return thread;
}

void smp_thread_join(SmpThread *thread)
{
    if (thread == NULL) {
        return;
    }

    if (thread->started) {
        pthread_join(thread->handle, NULL);
    }
    free(thread);
}

#endif /* !_WIN32 */

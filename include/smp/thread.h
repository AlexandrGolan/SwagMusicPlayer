/*
 * SMP - Swag Music Player
 * Minimal thread abstraction (POSIX pthreads / Win32 threads).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_THREAD_H
#define SMP_THREAD_H

#include <stdbool.h>
#include <stddef.h>

#define SMP_WAIT_FOREVER (-1)

typedef struct SmpThread SmpThread;
typedef struct SmpMutex SmpMutex;
typedef struct SmpCond SmpCond;

typedef int (*SmpThreadFn)(void *arg);

SmpMutex *smp_mutex_create(void);
void smp_mutex_destroy(SmpMutex *mutex);
void smp_mutex_lock(SmpMutex *mutex);
void smp_mutex_unlock(SmpMutex *mutex);

SmpCond *smp_cond_create(void);
void smp_cond_destroy(SmpCond *cond);
void smp_cond_wait(SmpCond *cond, SmpMutex *mutex);
void smp_cond_signal(SmpCond *cond);
void smp_cond_broadcast(SmpCond *cond);

/* Waits for a signal or the timeout. Returns true when signalled. */
bool smp_cond_wait_timeout(SmpCond *cond, SmpMutex *mutex, int timeout_ms);

/* Starts a thread. Returns NULL on failure. */
SmpThread *smp_thread_start(SmpThreadFn function, void *argument);

/* Blocks until the thread finished and releases its resources. */
void smp_thread_join(SmpThread *thread);

#endif /* SMP_THREAD_H */

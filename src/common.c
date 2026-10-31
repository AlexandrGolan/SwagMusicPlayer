/*
 * SMP - Swag Music Player
 * Common runtime helpers.
 *
 * SPDX-License-Identifier: MIT
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "smp/common.h"

#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <time.h>
#endif

const char *smp_status_str(SmpStatus status)
{
    switch (status) {
    case SMP_OK:
        return "ok";
    case SMP_ERR_INVALID_ARG:
        return "invalid argument";
    case SMP_ERR_OUT_OF_MEMORY:
        return "out of memory";
    case SMP_ERR_IO:
        return "I/O error";
    case SMP_ERR_NOT_FOUND:
        return "not found";
    case SMP_ERR_UNSUPPORTED:
        return "unsupported";
    case SMP_ERR_DECODE:
        return "decode error";
    case SMP_ERR_AUDIO:
        return "audio device error";
    case SMP_ERR_TERMINAL:
        return "terminal error";
    }
    return "unknown error";
}

uint64_t smp_now_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}

#ifdef _WIN32
wchar_t *smp_utf8_to_wide(const char *utf8)
{
    int length;
    wchar_t *wide;

    if (utf8 == NULL) {
        return NULL;
    }

    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    if (length <= 0) {
        return NULL;
    }

    wide = (wchar_t *)malloc((size_t)length * sizeof(wchar_t));
    if (wide == NULL) {
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wide, length) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

char *smp_wide_to_utf8(const wchar_t *wide)
{
    int length;
    char *utf8;

    if (wide == NULL) {
        return NULL;
    }

    length = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (length <= 0) {
        return NULL;
    }

    utf8 = (char *)malloc((size_t)length);
    if (utf8 == NULL) {
        return NULL;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, length, NULL, NULL) <= 0) {
        free(utf8);
        return NULL;
    }
    return utf8;
}
#endif /* _WIN32 */

void smp_sleep_ms(int ms)
{
    if (ms <= 0) {
        return;
    }

#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req;
    struct timespec rem;
    long remaining = (long)ms;

    req.tv_sec = remaining / 1000;
    req.tv_nsec = (remaining % 1000) * 1000000L;

    while (nanosleep(&req, &rem) != 0) {
        if (errno != EINTR) {
            break;
        }
        req = rem;
    }
#endif
}

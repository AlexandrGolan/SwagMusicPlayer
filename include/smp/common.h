/*
 * SMP - Swag Music Player
 * Common definitions: status codes, small helpers, time utilities.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_COMMON_H
#define SMP_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SMP_ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

/* The player assumes 64-bit pointers and 64-bit file offsets. */
#if defined(SMP_REQUIRE_64BIT)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(void *) >= 8, "SMP requires a 64-bit build");
#else
#error "SMP requires a 64-bit build"
#endif
#endif

#define SMP_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define SMP_MAX(a, b) (((a) > (b)) ? (a) : (b))

/* Mark a parameter intentionally unused (keeps -Wunused-parameter quiet). */
#define SMP_UNUSED(value) ((void)(value))

/* Result of an operation. Callers must handle every value they can receive. */
typedef enum {
    SMP_OK = 0,
    SMP_ERR_INVALID_ARG,
    SMP_ERR_OUT_OF_MEMORY,
    SMP_ERR_IO,
    SMP_ERR_NOT_FOUND,
    SMP_ERR_UNSUPPORTED,
    SMP_ERR_DECODE,
    SMP_ERR_AUDIO,
    SMP_ERR_TERMINAL,
} SmpStatus;

/* Human readable name of a status code (never returns NULL). */
const char *smp_status_str(SmpStatus status);

/* Monotonic clock in milliseconds. Never goes backwards. */
uint64_t smp_now_ms(void);

/* Sleep for at least `ms` milliseconds. */
void smp_sleep_ms(int ms);

#ifdef _WIN32
/*
 * UTF-8 <-> UTF-16 conversion for Win32 APIs. Both return freshly allocated
 * memory (free with free()) or NULL on failure.
 */
wchar_t *smp_utf8_to_wide(const char *utf8);
char *smp_wide_to_utf8(const wchar_t *wide);
#endif

#endif /* SMP_COMMON_H */

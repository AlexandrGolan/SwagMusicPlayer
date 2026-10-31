/*
 * SMP - Swag Music Player
 * UTF-8 strings, dynamic string buffers and natural ordering.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_STR_H
#define SMP_STR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smp/common.h"

/* Growable NUL terminated string. `len` excludes the terminator. */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

void sb_init(StrBuf *sb);
void sb_free(StrBuf *sb);
void sb_clear(StrBuf *sb);

/* Ensures room for `extra` more bytes plus the terminator. */
bool sb_reserve(StrBuf *sb, size_t extra);

bool sb_append(StrBuf *sb, const char *text);
bool sb_append_n(StrBuf *sb, const char *text, size_t len);
bool sb_append_char(StrBuf *sb, char c);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
bool sb_appendf(StrBuf *sb, const char *fmt, ...);

/* Detaches the buffer (caller owns it) and resets `sb`. */
char *sb_steal(StrBuf *sb);

/* Never NULL and always NUL terminated. */
const char *sb_cstr(const StrBuf *sb);

/* strdup for UTF-8 strings. Caller frees. */
char *smp_strdup(const char *text);
char *smp_strdup_n(const char *text, size_t len);

/* Replaces every control character in `text` (in place) with a space. */
void smp_str_sanitize(char *text);

/*
 * UTF-8 decoding. Returns the number of bytes consumed (>= 1); invalid
 * sequences yield U+FFFD and consume one byte.
 */
size_t smp_utf8_decode(const char *text, uint32_t *out_codepoint);

/* Encodes `codepoint`; returns the number of bytes written (0 when invalid). */
size_t smp_utf8_encode(uint32_t codepoint, char out[4]);

/* Number of terminal columns occupied by one code point (0, 1 or 2). */
int smp_utf8_codepoint_width(uint32_t codepoint);

/* Number of terminal columns occupied by a string. */
int smp_utf8_width(const char *text);

/* Bytes of `text` that fit into `max_columns` terminal columns. */
size_t smp_utf8_fit_columns(const char *text, int max_columns);

/*
 * Copies at most `max_columns` columns of `text` into `out` and appends an
 * ellipsis when something was dropped. `out` is always NUL terminated.
 */
void smp_utf8_ellipsize(const char *text, int max_columns, char *out, size_t out_size);

/* Case-insensitive, digit-aware comparison used for natural track ordering. */
int smp_strnatcmp(const char *left, const char *right);

#endif /* SMP_STR_H */

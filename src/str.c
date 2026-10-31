/*
 * SMP - Swag Music Player
 * Dynamic strings, UTF-8 helpers and natural ordering.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/str.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SB_INITIAL_CAPACITY 64u

void sb_init(StrBuf *sb)
{
    if (sb == NULL) {
        return;
    }
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

void sb_free(StrBuf *sb)
{
    if (sb == NULL) {
        return;
    }
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

void sb_clear(StrBuf *sb)
{
    if (sb == NULL) {
        return;
    }
    sb->len = 0;
    if (sb->data != NULL) {
        sb->data[0] = '\0';
    }
}

bool sb_reserve(StrBuf *sb, size_t extra)
{
    size_t needed;
    size_t capacity;
    char *grown;

    if (sb == NULL) {
        return false;
    }

    needed = sb->len + extra + 1u;
    if (sb->data != NULL && needed <= sb->cap) {
        return true;
    }

    capacity = sb->cap > 0 ? sb->cap : SB_INITIAL_CAPACITY;
    while (capacity < needed) {
        if (capacity > (size_t)-1 / 2u) {
            return false;
        }
        capacity *= 2u;
    }

    grown = (char *)realloc(sb->data, capacity);
    if (grown == NULL) {
        return false;
    }

    if (sb->data == NULL) {
        grown[0] = '\0';
    }
    sb->data = grown;
    sb->cap = capacity;
    return true;
}

bool sb_append_n(StrBuf *sb, const char *text, size_t len)
{
    if (sb == NULL || (text == NULL && len > 0)) {
        return false;
    }
    if (!sb_reserve(sb, len)) {
        return false;
    }

    if (len > 0) {
        memcpy(sb->data + sb->len, text, len);
        sb->len += len;
    }
    sb->data[sb->len] = '\0';
    return true;
}

bool sb_append(StrBuf *sb, const char *text)
{
    if (text == NULL) {
        return false;
    }
    return sb_append_n(sb, text, strlen(text));
}

bool sb_append_char(StrBuf *sb, char c)
{
    if (sb == NULL) {
        return false;
    }
    if (!sb_reserve(sb, 1)) {
        return false;
    }
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
    return true;
}

bool sb_appendf(StrBuf *sb, const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int needed;

    if (sb == NULL || fmt == NULL) {
        return false;
    }

    va_start(args, fmt);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (needed < 0) {
        va_end(args);
        return false;
    }

    if (!sb_reserve(sb, (size_t)needed)) {
        va_end(args);
        return false;
    }

    (void)vsnprintf(sb->data + sb->len, (size_t)needed + 1u, fmt, args);
    va_end(args);
    sb->len += (size_t)needed;
    return true;
}

char *sb_steal(StrBuf *sb)
{
    char *data;

    if (sb == NULL) {
        return NULL;
    }

    data = sb->data;
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
    return data;
}

const char *sb_cstr(const StrBuf *sb)
{
    if (sb == NULL || sb->data == NULL) {
        return "";
    }
    return sb->data;
}

char *smp_strdup(const char *text)
{
    if (text == NULL) {
        return NULL;
    }
    return smp_strdup_n(text, strlen(text));
}

char *smp_strdup_n(const char *text, size_t len)
{
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    copy = (char *)malloc(len + 1u);
    if (copy == NULL) {
        return NULL;
    }
    if (len > 0) {
        memcpy(copy, text, len);
    }
    copy[len] = '\0';
    return copy;
}

void smp_str_sanitize(char *text)
{
    size_t index;

    if (text == NULL) {
        return;
    }

    for (index = 0; text[index] != '\0'; index++) {
        unsigned char byte = (unsigned char)text[index];

        if (byte < 0x20u || byte == 0x7fu) {
            text[index] = ' ';
        }
    }
}

size_t smp_utf8_decode(const char *text, uint32_t *out_codepoint)
{
    const unsigned char *bytes = (const unsigned char *)text;
    unsigned char lead;
    size_t length;
    size_t index;
    uint32_t codepoint;
    uint32_t minimum;

    if (text == NULL || bytes[0] == 0) {
        if (out_codepoint != NULL) {
            *out_codepoint = 0;
        }
        return 0;
    }

    lead = bytes[0];
    if (lead < 0x80u) {
        if (out_codepoint != NULL) {
            *out_codepoint = lead;
        }
        return 1;
    }

    if ((lead & 0xe0u) == 0xc0u) {
        length = 2;
        codepoint = lead & 0x1fu;
        minimum = 0x80u;
    } else if ((lead & 0xf0u) == 0xe0u) {
        length = 3;
        codepoint = lead & 0x0fu;
        minimum = 0x800u;
    } else if ((lead & 0xf8u) == 0xf0u) {
        length = 4;
        codepoint = lead & 0x07u;
        minimum = 0x10000u;
    } else {
        if (out_codepoint != NULL) {
            *out_codepoint = 0xfffdu;
        }
        return 1;
    }

    for (index = 1; index < length; index++) {
        if ((bytes[index] & 0xc0u) != 0x80u) {
            if (out_codepoint != NULL) {
                *out_codepoint = 0xfffdu;
            }
            return 1;
        }
        codepoint = (codepoint << 6) | (bytes[index] & 0x3fu);
    }

    if (codepoint < minimum || codepoint > 0x10ffffu
        || (codepoint >= 0xd800u && codepoint <= 0xdfffu))
    {
        if (out_codepoint != NULL) {
            *out_codepoint = 0xfffdu;
        }
        return 1;
    }

    if (out_codepoint != NULL) {
        *out_codepoint = codepoint;
    }
    return length;
}

size_t smp_utf8_encode(uint32_t codepoint, char out[4])
{
    if (out == NULL) {
        return 0;
    }

    if (codepoint < 0x80u) {
        out[0] = (char)codepoint;
        return 1;
    }
    if (codepoint < 0x800u) {
        out[0] = (char)(0xc0u | (codepoint >> 6));
        out[1] = (char)(0x80u | (codepoint & 0x3fu));
        return 2;
    }
    if (codepoint < 0x10000u) {
        if (codepoint >= 0xd800u && codepoint <= 0xdfffu) {
            return 0;
        }
        out[0] = (char)(0xe0u | (codepoint >> 12));
        out[1] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
        out[2] = (char)(0x80u | (codepoint & 0x3fu));
        return 3;
    }
    if (codepoint <= 0x10ffffu) {
        out[0] = (char)(0xf0u | (codepoint >> 18));
        out[1] = (char)(0x80u | ((codepoint >> 12) & 0x3fu));
        out[2] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
        out[3] = (char)(0x80u | (codepoint & 0x3fu));
        return 4;
    }
    return 0;
}

int smp_utf8_codepoint_width(uint32_t codepoint)
{
    /* Control characters and combining marks take no columns. */
    if (codepoint == 0) {
        return 0;
    }
    if (codepoint < 0x20u || (codepoint >= 0x7fu && codepoint < 0xa0u)) {
        return 0;
    }
    if (codepoint == 0x200bu || codepoint == 0x200cu || codepoint == 0x200du) {
        return 0;
    }
    if (codepoint >= 0x0300u && codepoint <= 0x036fu) {
        return 0;
    }
    if (codepoint >= 0x0483u && codepoint <= 0x0489u) {
        return 0;
    }
    if (codepoint >= 0x1ab0u && codepoint <= 0x1affu) {
        return 0;
    }
    if (codepoint >= 0x1dc0u && codepoint <= 0x1dffu) {
        return 0;
    }
    if (codepoint >= 0x20d0u && codepoint <= 0x20f0u) {
        return 0;
    }
    if (codepoint >= 0xfe00u && codepoint <= 0xfe0fu) {
        return 0;
    }
    if (codepoint >= 0xfe20u && codepoint <= 0xfe2fu) {
        return 0;
    }
    if (codepoint >= 0xe0100u && codepoint <= 0xe01efu) {
        return 0;
    }

    /* East Asian wide characters and most emoji occupy two columns. */
    if (codepoint >= 0x1100u && codepoint <= 0x115fu) {
        return 2;
    }
    if (codepoint >= 0x2e80u && codepoint <= 0x303eu) {
        return 2;
    }
    if (codepoint >= 0x3041u && codepoint <= 0x33ffu) {
        return 2;
    }
    if (codepoint >= 0x3400u && codepoint <= 0x4dbfu) {
        return 2;
    }
    if (codepoint >= 0x4e00u && codepoint <= 0x9fffu) {
        return 2;
    }
    if (codepoint >= 0xa000u && codepoint <= 0xa4cfu) {
        return 2;
    }
    if (codepoint >= 0xa960u && codepoint <= 0xa97fu) {
        return 2;
    }
    if (codepoint >= 0xac00u && codepoint <= 0xd7a3u) {
        return 2;
    }
    if (codepoint >= 0xf900u && codepoint <= 0xfaffu) {
        return 2;
    }
    if (codepoint >= 0xfe10u && codepoint <= 0xfe19u) {
        return 2;
    }
    if (codepoint >= 0xfe30u && codepoint <= 0xfe6fu) {
        return 2;
    }
    if (codepoint >= 0xff00u && codepoint <= 0xff60u) {
        return 2;
    }
    if (codepoint >= 0xffe0u && codepoint <= 0xffe6u) {
        return 2;
    }
    if (codepoint >= 0x1f300u && codepoint <= 0x1f64fu) {
        return 2;
    }
    if (codepoint >= 0x1f680u && codepoint <= 0x1f6ffu) {
        return 2;
    }
    if (codepoint >= 0x1f900u && codepoint <= 0x1f9ffu) {
        return 2;
    }
    if (codepoint >= 0x1fa70u && codepoint <= 0x1faffu) {
        return 2;
    }
    if (codepoint >= 0x20000u && codepoint <= 0x3fffdu) {
        return 2;
    }

    return 1;
}

int smp_utf8_width(const char *text)
{
    int width = 0;
    size_t offset = 0;

    if (text == NULL) {
        return 0;
    }

    while (text[offset] != '\0') {
        uint32_t codepoint = 0;
        size_t consumed = smp_utf8_decode(text + offset, &codepoint);

        if (consumed == 0) {
            break;
        }
        width += smp_utf8_codepoint_width(codepoint);
        offset += consumed;
    }

    return width;
}

size_t smp_utf8_fit_columns(const char *text, int max_columns)
{
    size_t offset = 0;
    int width = 0;

    if (text == NULL || max_columns <= 0) {
        return 0;
    }

    while (text[offset] != '\0') {
        uint32_t codepoint = 0;
        size_t consumed = smp_utf8_decode(text + offset, &codepoint);
        int codepoint_width;

        if (consumed == 0) {
            break;
        }

        codepoint_width = smp_utf8_codepoint_width(codepoint);
        if (width + codepoint_width > max_columns) {
            break;
        }

        width += codepoint_width;
        offset += consumed;
    }

    return offset;
}

void smp_utf8_ellipsize(const char *text, int max_columns, char *out, size_t out_size)
{
    size_t fit;
    size_t copy_len;
    static const char ellipsis[] = "\xe2\x80\xa6"; /* U+2026 */

    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (text == NULL || max_columns <= 0) {
        return;
    }

    if (smp_utf8_width(text) <= max_columns) {
        snprintf(out, out_size, "%s", text);
        return;
    }

    fit = smp_utf8_fit_columns(text, max_columns - 1);
    copy_len = fit;
    if (copy_len > out_size - 1u - (sizeof(ellipsis) - 1u)) {
        copy_len = out_size - 1u - (sizeof(ellipsis) - 1u);
    }

    memcpy(out, text, copy_len);
    memcpy(out + copy_len, ellipsis, sizeof(ellipsis) - 1u);
    out[copy_len + sizeof(ellipsis) - 1u] = '\0';
}

static bool is_ascii_digit(char c)
{
    return c >= '0' && c <= '9';
}

static char ascii_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

int smp_strnatcmp(const char *left, const char *right)
{
    size_t li = 0;
    size_t ri = 0;

    if (left == right) {
        return 0;
    }
    if (left == NULL) {
        return -1;
    }
    if (right == NULL) {
        return 1;
    }

    while (left[li] != '\0' && right[ri] != '\0') {
        if (is_ascii_digit(left[li]) && is_ascii_digit(right[ri])) {
            size_t left_end;
            size_t right_end;
            size_t left_digits;
            size_t right_digits;
            int comparison;

            while (left[li] == '0' && is_ascii_digit(left[li + 1])) {
                li++;
            }
            while (right[ri] == '0' && is_ascii_digit(right[ri + 1])) {
                ri++;
            }

            left_end = li;
            while (is_ascii_digit(left[left_end])) {
                left_end++;
            }
            right_end = ri;
            while (is_ascii_digit(right[right_end])) {
                right_end++;
            }

            left_digits = left_end - li;
            right_digits = right_end - ri;

            if (left_digits != right_digits) {
                return left_digits < right_digits ? -1 : 1;
            }

            comparison = strncmp(left + li, right + ri, left_digits);
            if (comparison != 0) {
                return comparison < 0 ? -1 : 1;
            }

            li = left_end;
            ri = right_end;
            continue;
        }

        {
            char left_char = ascii_lower(left[li]);
            char right_char = ascii_lower(right[ri]);

            if (left_char != right_char) {
                return (unsigned char)left_char < (unsigned char)right_char ? -1 : 1;
            }
        }

        li++;
        ri++;
    }

    if (left[li] == '\0' && right[ri] == '\0') {
        return 0;
    }
    return left[li] == '\0' ? -1 : 1;
}

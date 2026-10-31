/*
 * SMP - Swag Music Player
 * Tests for UTF-8 handling, dynamic strings and natural ordering.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/str.h"
#include "test.h"

static void test_string_buffer(void)
{
    StrBuf sb;

    smp_test_begin("dynamic buffer append/steal");
    sb_init(&sb);

    SMP_CHECK(sb_append(&sb, "hello"));
    SMP_CHECK(sb_append_char(&sb, ' '));
    SMP_CHECK(sb_appendf(&sb, "%s-%d", "world", 42));
    SMP_CHECK_STR(sb_cstr(&sb), "hello world-42");
    SMP_CHECK_INT(sb.len, 14);

    sb_clear(&sb);
    SMP_CHECK_INT(sb.len, 0);
    SMP_CHECK_STR(sb_cstr(&sb), "");

    SMP_CHECK(sb_append_n(&sb, "abcdef", 3));
    SMP_CHECK_STR(sb_cstr(&sb), "abc");

    {
        char *stolen = sb_steal(&sb);
        SMP_CHECK_STR(stolen, "abc");
        SMP_CHECK_INT(sb.len, 0);
        free(stolen);
    }

    sb_free(&sb);
    smp_test_end();
}

static void test_long_append(void)
{
    StrBuf sb;
    int index;

    smp_test_begin("dynamic buffer grows beyond capacity");
    sb_init(&sb);
    for (index = 0; index < 5000; index++) {
        SMP_CHECK(sb_append_char(&sb, 'x'));
    }
    SMP_CHECK_INT(sb.len, 5000);
    SMP_CHECK_INT(strlen(sb_cstr(&sb)), 5000);
    sb_free(&sb);
    smp_test_end();
}

static void test_utf8_decode(void)
{
    uint32_t codepoint = 0;

    smp_test_begin("utf8 decode ascii/2/3/4 bytes");
    SMP_CHECK_INT(smp_utf8_decode("A", &codepoint), 1);
    SMP_CHECK_INT(codepoint, 'A');

    SMP_CHECK_INT(smp_utf8_decode("\xd0\x9f", &codepoint), 2);
    SMP_CHECK_INT(codepoint, 0x41f); /* П */

    SMP_CHECK_INT(smp_utf8_decode("\xe4\xb8\xad", &codepoint), 3);
    SMP_CHECK_INT(codepoint, 0x4e2d); /* 中 */

    SMP_CHECK_INT(smp_utf8_decode("\xf0\x9f\x8e\xb5", &codepoint), 4);
    SMP_CHECK_INT(codepoint, 0x1f3b5); /* 🎵 */
    smp_test_end();
}

static void test_utf8_invalid(void)
{
    uint32_t codepoint = 0;

    smp_test_begin("utf8 rejects invalid sequences");
    SMP_CHECK_INT(smp_utf8_decode("\xff", &codepoint), 1);
    SMP_CHECK_INT(codepoint, 0xfffd);

    SMP_CHECK_INT(smp_utf8_decode("\xc3\x28", &codepoint), 1);
    SMP_CHECK_INT(codepoint, 0xfffd);

    SMP_CHECK_INT(smp_utf8_decode("\xed\xa0\x80", &codepoint), 1); /* surrogate */
    SMP_CHECK_INT(codepoint, 0xfffd);

    SMP_CHECK_INT(smp_utf8_decode("\xc0\xaf", &codepoint), 1); /* overlong */
    SMP_CHECK_INT(codepoint, 0xfffd);
    smp_test_end();
}

static void test_utf8_width(void)
{
    smp_test_begin("utf8 display width");
    SMP_CHECK_INT(smp_utf8_width("hello"), 5);
    SMP_CHECK_INT(smp_utf8_width("Привет"), 6);
    SMP_CHECK_INT(smp_utf8_width("中文"), 4);
    SMP_CHECK_INT(smp_utf8_width("a中b"), 4);
    SMP_CHECK_INT(smp_utf8_width("\xf0\x9f\x8e\xb5"), 2);
    SMP_CHECK_INT(smp_utf8_width(""), 0);
    smp_test_end();
}

static void test_utf8_fit(void)
{
    smp_test_begin("utf8 fit columns respects boundaries");
    SMP_CHECK_INT(smp_utf8_fit_columns("abcdef", 3), 3);
    /* Each Cyrillic letter is 2 bytes, 1 column. */
    SMP_CHECK_INT(smp_utf8_fit_columns("Привет", 3), 6);
    /* A wide character that does not fit is not split. */
    SMP_CHECK_INT(smp_utf8_fit_columns("a中", 2), 1);
    SMP_CHECK_INT(smp_utf8_fit_columns("abc", 0), 0);
    smp_test_end();
}

static void test_ellipsize(void)
{
    char buffer[32];

    smp_test_begin("utf8 ellipsize");
    smp_utf8_ellipsize("abcdef", 10, buffer, sizeof(buffer));
    SMP_CHECK_STR(buffer, "abcdef");

    smp_utf8_ellipsize("abcdef", 4, buffer, sizeof(buffer));
    SMP_CHECK_STR(buffer, "abc\xe2\x80\xa6");

    smp_utf8_ellipsize("Привет мир", 8, buffer, sizeof(buffer));
    SMP_CHECK_STR(buffer, "Привет \xe2\x80\xa6");

    smp_utf8_ellipsize("abc", 0, buffer, sizeof(buffer));
    SMP_CHECK_STR(buffer, "");
    smp_test_end();
}

static void test_natural_compare(void)
{
    smp_test_begin("natural comparison of track names");
    SMP_CHECK(smp_strnatcmp("track2.mp3", "track10.mp3") < 0);
    SMP_CHECK(smp_strnatcmp("track10.mp3", "track2.mp3") > 0);
    SMP_CHECK(smp_strnatcmp("Track2.mp3", "track02.mp3") == 0);
    SMP_CHECK(smp_strnatcmp("a", "a") == 0);
    SMP_CHECK(smp_strnatcmp("a", "b") < 0);
    SMP_CHECK(smp_strnatcmp("A", "a") == 0);
    SMP_CHECK(smp_strnatcmp("", "a") < 0);
    smp_test_end();
}

static void test_sanitize(void)
{
    char text[] = "a\tb\nc" "\x01" "d";

    smp_test_begin("control characters sanitized");
    smp_str_sanitize(text);
    SMP_CHECK_STR(text, "a b c d");
    smp_test_end();
}

void suite_str(void)
{
    smp_test_suite("str");
    test_string_buffer();
    test_long_append();
    test_utf8_decode();
    test_utf8_invalid();
    test_utf8_width();
    test_utf8_fit();
    test_ellipsize();
    test_natural_compare();
    test_sanitize();
}

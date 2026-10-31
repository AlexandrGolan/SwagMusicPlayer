/*
 * SMP - Swag Music Player
 * Tests for the render frame.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdlib.h>
#include <string.h>

#include "smp/frame.h"
#include "test.h"

static void test_put_text(void)
{
    SmpFrame frame;
    StrBuf rendered;
    int used;

    smp_test_begin("frame writes text cells");
    smp_frame_init(&frame);
    SMP_CHECK(smp_frame_begin(&frame, 20, 3));

    used = smp_frame_put(&frame, 2, 1, "hello");
    SMP_CHECK_INT(used, 5);
    SMP_CHECK_INT(frame.cells[1 * 20 + 2].codepoint, 'h');
    SMP_CHECK_INT(frame.cells[1 * 20 + 6].codepoint, 'o');
    SMP_CHECK_INT(frame.cells[1 * 20 + 7].codepoint, ' ');

    sb_init(&rendered);
    smp_frame_render(&frame, &rendered);
    SMP_CHECK(strstr(sb_cstr(&rendered), "hello") != NULL);
    SMP_CHECK(strstr(sb_cstr(&rendered), "\x1b[2;1H") != NULL);
    sb_free(&rendered);

    smp_frame_free(&frame);
    smp_test_end();
}

static void test_clipping(void)
{
    SmpFrame frame;

    smp_test_begin("frame clips at the right edge");
    smp_frame_init(&frame);
    SMP_CHECK(smp_frame_begin(&frame, 5, 2));

    SMP_CHECK_INT(smp_frame_put(&frame, 3, 0, "abcdef"), 2);
    SMP_CHECK_INT(frame.cells[0 * 5 + 3].codepoint, 'a');
    SMP_CHECK_INT(frame.cells[0 * 5 + 4].codepoint, 'b');

    /* Writing outside the frame must be ignored. */
    SMP_CHECK_INT(smp_frame_put(&frame, 0, 99, "x"), 0);
    SMP_CHECK_INT(smp_frame_put(&frame, -3, 0, "x"), 0);

    smp_frame_free(&frame);
    smp_test_end();
}

static void test_wide_characters(void)
{
    SmpFrame frame;

    smp_test_begin("frame handles two column characters");
    smp_frame_init(&frame);
    SMP_CHECK(smp_frame_begin(&frame, 10, 1));

    /* "中" is wide: it occupies two cells, the second one is a continuation. */
    SMP_CHECK_INT(smp_frame_put(&frame, 0, 0, "\xe4\xb8\xad"), 2);
    SMP_CHECK_INT(frame.cells[0].codepoint, 0x4e2du);
    SMP_CHECK_INT(frame.cells[1].codepoint, SMP_CELL_CONTINUATION);

    /* A wide character that would not fit is not drawn. */
    SMP_CHECK_INT(smp_frame_put(&frame, 9, 0, "\xe4\xb8\xad"), 0);

    smp_frame_free(&frame);
    smp_test_end();
}

static void test_cyrillic(void)
{
    SmpFrame frame;
    StrBuf rendered;

    smp_test_begin("frame writes cyrillic text");
    smp_frame_init(&frame);
    SMP_CHECK(smp_frame_begin(&frame, 20, 1));

    SMP_CHECK_INT(smp_frame_put(&frame, 0, 0, "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82"), 6);

    sb_init(&rendered);
    smp_frame_render(&frame, &rendered);
    SMP_CHECK(strstr(sb_cstr(&rendered), "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82") != NULL);
    sb_free(&rendered);

    smp_frame_free(&frame);
    smp_test_end();
}

static void test_fill_and_style(void)
{
    SmpFrame frame;
    StrBuf rendered;
    SmpStyle style;

    smp_test_begin("frame fill and colors");
    smp_frame_init(&frame);
    SMP_CHECK(smp_frame_begin(&frame, 6, 2));

    style = smp_style_fg(10, 20, 30);
    smp_frame_set_style(&frame, style);
    smp_frame_fill(&frame, 1, 1, 3, 1, '-');

    SMP_CHECK_INT(frame.cells[1 * 6 + 1].codepoint, '-');
    SMP_CHECK_INT(frame.cells[1 * 6 + 1].flags & SMP_CELL_HAS_FG, SMP_CELL_HAS_FG);
    SMP_CHECK_INT(frame.cells[1 * 6 + 1].fg[0], 10);

    sb_init(&rendered);
    smp_frame_render(&frame, &rendered);
    SMP_CHECK(strstr(sb_cstr(&rendered), "38;2;10;20;30") != NULL);
    sb_free(&rendered);

    smp_frame_free(&frame);
    smp_test_end();
}

static void test_resize(void)
{
    SmpFrame frame;

    smp_test_begin("frame resize clears content");
    smp_frame_init(&frame);
    SMP_CHECK(smp_frame_begin(&frame, 4, 2));
    SMP_CHECK_INT(smp_frame_put(&frame, 0, 0, "abcd"), 4);

    SMP_CHECK(smp_frame_begin(&frame, 10, 4));
    SMP_CHECK_INT(frame.width, 10);
    SMP_CHECK_INT(frame.height, 4);
    SMP_CHECK_INT(frame.cells[0].codepoint, ' ');

    SMP_CHECK(!smp_frame_begin(&frame, 0, 5));
    smp_frame_free(&frame);
    smp_test_end();
}

void suite_frame(void)
{
    smp_test_suite("frame");
    test_put_text();
    test_clipping();
    test_wide_characters();
    test_cyrillic();
    test_fill_and_style();
    test_resize();
}

/*
 * SMP - Swag Music Player
 * Cell based render frame with ANSI output.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/frame.h"

#include <stdlib.h>
#include <string.h>

#include "smp/str.h"

SmpStyle smp_style_default(void)
{
    SmpStyle style;

    memset(&style, 0, sizeof(style));
    style.codepoint = ' ';
    return style;
}

SmpStyle smp_style_fg(uint8_t r, uint8_t g, uint8_t b)
{
    SmpStyle style = smp_style_default();

    style.has_fg = true;
    style.fg[0] = r;
    style.fg[1] = g;
    style.fg[2] = b;
    return style;
}

SmpStyle smp_style_bold(uint8_t r, uint8_t g, uint8_t b)
{
    SmpStyle style = smp_style_fg(r, g, b);

    style.flags |= SMP_CELL_BOLD;
    return style;
}

SmpStyle smp_style_dim(uint8_t r, uint8_t g, uint8_t b)
{
    SmpStyle style = smp_style_fg(r, g, b);

    style.flags |= SMP_CELL_DIM;
    return style;
}

SmpStyle smp_style_reverse(uint8_t r, uint8_t g, uint8_t b)
{
    SmpStyle style = smp_style_fg(r, g, b);

    style.flags |= SMP_CELL_REVERSE;
    return style;
}

void smp_frame_init(SmpFrame *frame)
{
    if (frame == NULL) {
        return;
    }
    frame->cells = NULL;
    frame->width = 0;
    frame->height = 0;
    frame->style = smp_style_default();
    frame->valid = false;
}

void smp_frame_free(SmpFrame *frame)
{
    if (frame == NULL) {
        return;
    }
    free(frame->cells);
    frame->cells = NULL;
    frame->width = 0;
    frame->height = 0;
    frame->valid = false;
}

bool smp_frame_begin(SmpFrame *frame, int width, int height)
{
    size_t count;

    if (frame == NULL || width <= 0 || height <= 0 || width > 1000 || height > 500) {
        return false;
    }

    count = (size_t)width * (size_t)height;
    if ((size_t)frame->width * (size_t)frame->height != count || frame->cells == NULL) {
        SmpCell *cells = (SmpCell *)realloc(frame->cells, count * sizeof(SmpCell));

        if (cells == NULL) {
            return false;
        }
        frame->cells = cells;
    }

    frame->width = width;
    frame->height = height;
    smp_frame_clear(frame);
    return true;
}

static void clear_cell(SmpCell *cell, SmpStyle style)
{
    cell->codepoint = style.codepoint;
    cell->fg[0] = style.fg[0];
    cell->fg[1] = style.fg[1];
    cell->fg[2] = style.fg[2];
    cell->bg[0] = style.bg[0];
    cell->bg[1] = style.bg[1];
    cell->bg[2] = style.bg[2];
    cell->flags = (uint8_t)(((style.has_fg ? SMP_CELL_HAS_FG : 0u)
                             | (style.has_bg ? SMP_CELL_HAS_BG : 0u) | style.flags));
}

void smp_frame_clear(SmpFrame *frame)
{
    size_t count;
    size_t index;
    SmpStyle style;

    if (frame == NULL || frame->cells == NULL) {
        return;
    }

    style = smp_style_default();
    count = (size_t)frame->width * (size_t)frame->height;
    for (index = 0; index < count; index++) {
        clear_cell(&frame->cells[index], style);
    }
    frame->valid = true;
}

void smp_frame_set_style(SmpFrame *frame, SmpStyle style)
{
    if (frame == NULL) {
        return;
    }
    frame->style = style;
}

void smp_frame_put_cell(SmpFrame *frame,
                        int x,
                        int y,
                        uint32_t codepoint,
                        uint8_t fg_r,
                        uint8_t fg_g,
                        uint8_t fg_b,
                        bool has_bg,
                        uint8_t bg_r,
                        uint8_t bg_g,
                        uint8_t bg_b)
{
    SmpCell *cell;

    if (frame == NULL || frame->cells == NULL) {
        return;
    }
    if (x < 0 || y < 0 || x >= frame->width || y >= frame->height) {
        return;
    }

    cell = &frame->cells[(size_t)y * (size_t)frame->width + (size_t)x];
    cell->codepoint = codepoint;
    cell->fg[0] = fg_r;
    cell->fg[1] = fg_g;
    cell->fg[2] = fg_b;
    cell->bg[0] = bg_r;
    cell->bg[1] = bg_g;
    cell->bg[2] = bg_b;
    cell->flags = (uint8_t)(SMP_CELL_HAS_FG | (has_bg ? SMP_CELL_HAS_BG : 0u));
}

void smp_frame_fill(SmpFrame *frame, int x, int y, int width, int height, uint32_t codepoint)
{
    int row;
    int column;

    if (frame == NULL || frame->cells == NULL) {
        return;
    }

    for (row = y; row < y + height; row++) {
        for (column = x; column < x + width; column++) {
            SmpCell *cell;

            if (column < 0 || row < 0 || column >= frame->width || row >= frame->height) {
                continue;
            }
            cell = &frame->cells[(size_t)row * (size_t)frame->width + (size_t)column];
            clear_cell(cell, frame->style);
            cell->codepoint = codepoint;
        }
    }
}

int smp_frame_put_n(SmpFrame *frame, int x, int y, const char *text, int max_columns)
{
    size_t offset = 0;
    int column = 0;
    int drawn = 0;

    if (frame == NULL || text == NULL || y < 0 || y >= frame->height) {
        return 0;
    }

    while (text[offset] != '\0' && column < max_columns) {
        uint32_t codepoint = 0;
        size_t consumed = smp_utf8_decode(text + offset, &codepoint);
        int width;
        int target;

        if (consumed == 0) {
            break;
        }
        offset += consumed;

        width = smp_utf8_codepoint_width(codepoint);
        if (width == 0) {
            continue; /* combining marks are not rendered by this simple renderer */
        }

        target = x + column;
        if (target >= frame->width || target + width > frame->width) {
            break;
        }
        if (target < 0) {
            /* The character starts left of the frame, so it stays invisible. */
            column += width;
            continue;
        }

        {
            SmpCell *cell = &frame->cells[(size_t)y * (size_t)frame->width + (size_t)target];
            int extra;

            clear_cell(cell, frame->style);
            cell->codepoint = codepoint;

            for (extra = 1; extra < width; extra++) {
                SmpCell *continuation
                    = &frame->cells[(size_t)y * (size_t)frame->width + (size_t)target + (size_t)extra];

                clear_cell(continuation, frame->style);
                continuation->codepoint = SMP_CELL_CONTINUATION;
            }
        }

        column += width;
        drawn += width;
    }

    return drawn;
}

int smp_frame_put(SmpFrame *frame, int x, int y, const char *text)
{
    if (frame == NULL) {
        return 0;
    }
    return smp_frame_put_n(frame, x, y, text, frame->width);
}

static void append_color(StrBuf *out, bool foreground, uint8_t r, uint8_t g, uint8_t b)
{
    (void)sb_appendf(out,
                     "\x1b[%d;2;%u;%u;%um",
                     foreground ? 38 : 48,
                     (unsigned)r,
                     (unsigned)g,
                     (unsigned)b);
}

static bool same_style(const SmpCell *cell, uint8_t flags, const uint8_t fg[3], const uint8_t bg[3])
{
    return cell->flags == flags && cell->fg[0] == fg[0] && cell->fg[1] == fg[1]
           && cell->fg[2] == fg[2] && cell->bg[0] == bg[0] && cell->bg[1] == bg[1]
           && cell->bg[2] == bg[2];
}

void smp_frame_render(const SmpFrame *frame, StrBuf *out)
{
    int row;
    uint8_t current_fg[3] = {0, 0, 0};
    uint8_t current_bg[3] = {0, 0, 0};
    uint8_t current_flags = 0xffu;
    bool have_fg = false;
    bool have_bg = false;

    if (frame == NULL || out == NULL || frame->cells == NULL) {
        return;
    }

    (void)sb_append(out, "\x1b[0m");

    for (row = 0; row < frame->height; row++) {
        int column = 0;

        (void)sb_appendf(out, "\x1b[%d;1H", row + 1);

        while (column < frame->width) {
            const SmpCell *cell = &frame->cells[(size_t)row * (size_t)frame->width + (size_t)column];
            char encoded[4];
            size_t length;

            if (cell->codepoint == SMP_CELL_CONTINUATION) {
                column++;
                continue;
            }

            if (!same_style(cell, current_flags, current_fg, current_bg)) {
                uint8_t flags = cell->flags;

                (void)sb_append(out, "\x1b[0m");
                have_fg = false;
                have_bg = false;

                if ((flags & SMP_CELL_BOLD) != 0u) {
                    (void)sb_append(out, "\x1b[1m");
                }
                if ((flags & SMP_CELL_DIM) != 0u) {
                    (void)sb_append(out, "\x1b[2m");
                }
                if ((flags & SMP_CELL_REVERSE) != 0u) {
                    (void)sb_append(out, "\x1b[7m");
                }
                if ((flags & SMP_CELL_HAS_FG) != 0u) {
                    append_color(out, true, cell->fg[0], cell->fg[1], cell->fg[2]);
                    have_fg = true;
                }
                if ((flags & SMP_CELL_HAS_BG) != 0u) {
                    append_color(out, false, cell->bg[0], cell->bg[1], cell->bg[2]);
                    have_bg = true;
                }

                current_flags = flags;
                memcpy(current_fg, cell->fg, sizeof(current_fg));
                memcpy(current_bg, cell->bg, sizeof(current_bg));
            }

            (void)have_fg;
            (void)have_bg;

            length = smp_utf8_encode(cell->codepoint, encoded);
            if (length == 0) {
                encoded[0] = '?';
                length = 1;
            }
            (void)sb_append_n(out, encoded, length);
            column++;
        }
    }

    (void)sb_append(out, "\x1b[0m");
}

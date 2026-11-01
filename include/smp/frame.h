/*
 * SMP - Swag Music Player
 * Cell based render frame with ANSI output.
 *
 * Every frame is rendered into a grid of cells and written to the terminal in
 * one pass. This removes flicker, per-character printf calls and clipping
 * bugs, and it makes wide (two column) characters safe.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_FRAME_H
#define SMP_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smp/common.h"
#include "smp/str.h"

#define SMP_CELL_CONTINUATION 0xffffffffu

#define SMP_CELL_HAS_FG 0x01u
#define SMP_CELL_HAS_BG 0x02u
#define SMP_CELL_BOLD 0x04u
#define SMP_CELL_DIM 0x08u
#define SMP_CELL_REVERSE 0x10u
#define SMP_CELL_ITALIC 0x20u

typedef struct {
    uint32_t codepoint;
    uint8_t fg[3];
    uint8_t bg[3];
    uint8_t flags;
} SmpCell;

typedef struct {
    uint32_t codepoint; /* filler code point, usually ' ' */
    bool has_fg;
    uint8_t fg[3];
    bool has_bg;
    uint8_t bg[3];
    uint8_t flags;
} SmpStyle;

typedef struct {
    SmpCell *cells;
    int width;
    int height;
    SmpStyle style;
    bool valid;
} SmpFrame;

SmpStyle smp_style_default(void);
SmpStyle smp_style_fg(uint8_t r, uint8_t g, uint8_t b);
SmpStyle smp_style_bold(uint8_t r, uint8_t g, uint8_t b);
SmpStyle smp_style_dim(uint8_t r, uint8_t g, uint8_t b);
SmpStyle smp_style_reverse(uint8_t r, uint8_t g, uint8_t b);
SmpStyle smp_style_italic(uint8_t r, uint8_t g, uint8_t b);

void smp_frame_init(SmpFrame *frame);
void smp_frame_free(SmpFrame *frame);

/* Resizes (when needed) and clears the frame. */
bool smp_frame_begin(SmpFrame *frame, int width, int height);

void smp_frame_clear(SmpFrame *frame);
void smp_frame_set_style(SmpFrame *frame, SmpStyle style);

/* Writes UTF-8 text at (x, y) and returns the number of columns used. */
int smp_frame_put(SmpFrame *frame, int x, int y, const char *text);

/* Same, but never draws more than `max_columns` columns. */
int smp_frame_put_n(SmpFrame *frame, int x, int y, const char *text, int max_columns);

/* Writes a single cell with explicit colors (used for cover art). */
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
                        uint8_t bg_b);

/* Fills a rectangle with the current style. */
void smp_frame_fill(SmpFrame *frame, int x, int y, int width, int height, uint32_t codepoint);

/* Renders the frame into `out` as ANSI escape sequences. */
void smp_frame_render(const SmpFrame *frame, StrBuf *out);

#endif /* SMP_FRAME_H */

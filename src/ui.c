/*
 * SMP - Swag Music Player
 * Terminal user interface.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/str.h"

#define UI_MIN_WIDTH 60
#define UI_MIN_HEIGHT 16
#define UI_MESSAGE_MS 2500u

/* Active palette; pure white unless a configuration file overrides it. */
static SmpUiTheme g_theme;

#define SEPARATOR "\xe2\x94\x80"  /* horizontal rule */
#define VERTICAL "\xe2\x94\x82"   /* vertical rule */
#define JUNCTION_TOP "\xe2\x94\xac"     /* top tee */
#define JUNCTION_LEFT "\xe2\x94\x9c"    /* left tee */
#define JUNCTION_RIGHT "\xe2\x94\xa4"   /* right tee */
#define JUNCTION_BOTTOM "\xe2\x94\xb4"  /* bottom tee */
#define BAR_FULL "\xe2\x96\x88"   /* full block */
#define BAR_EMPTY "\xe2\x96\x91"  /* light shade */
#define MARKER "\xe2\x96\xb6"     /* right pointing triangle */

void smp_ui_theme_defaults(SmpUiTheme *theme)
{
    SmpRgb white = {255, 255, 255};

    if (theme == NULL) {
        return;
    }

    theme->text = white;
    theme->accent = white;
    theme->dim = white;
    theme->selection_bg = white;
    theme->playing = white;
    theme->error = white;
    theme->progress = white;
    theme->volume = white;
    theme->border = white;
    theme->cover = white;
    theme->spectrum = white;
    theme->cover_mode = SMP_COVER_BLOCKS;
    theme->show_spectrum = true;
}

static SmpRgb color_to_rgb(SmpColor color)
{
    SmpRgb rgb;

    rgb.r = color.r;
    rgb.g = color.g;
    rgb.b = color.b;
    return rgb;
}

void smp_ui_theme_from_config(SmpUiTheme *theme, const SmpConfig *config)
{
    if (theme == NULL || config == NULL) {
        return;
    }

    theme->text = color_to_rgb(config->text);
    theme->accent = color_to_rgb(config->accent);
    theme->dim = color_to_rgb(config->dim);
    theme->selection_bg = color_to_rgb(config->selection_bg);
    theme->playing = color_to_rgb(config->playing);
    theme->error = color_to_rgb(config->error);
    theme->progress = color_to_rgb(config->progress);
    theme->volume = color_to_rgb(config->volume_color);
    theme->border = color_to_rgb(config->border);
    theme->cover = color_to_rgb(config->cover);
    theme->spectrum = color_to_rgb(config->spectrum);
    theme->cover_mode = config->cover_mode == 1 ? SMP_COVER_ASCII : SMP_COVER_BLOCKS;
    theme->show_spectrum = config->visualizer;
}

void smp_ui_set_theme(SmpUiState *state, const SmpUiTheme *theme)
{
    if (state == NULL || theme == NULL) {
        return;
    }

    state->theme = *theme;
    g_theme = *theme;
}

void smp_ui_set_spectrum(SmpUiState *state, SmpSpectrum *spectrum)
{
    if (state == NULL) {
        return;
    }
    state->spectrum = spectrum;
}

void smp_ui_init(SmpUiState *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    smp_image_init(&state->cover);

    smp_ui_theme_defaults(&state->theme);
    g_theme = state->theme;
}

void smp_ui_free(SmpUiState *state)
{
    if (state == NULL) {
        return;
    }
    smp_image_free(&state->cover);
    smp_image_free(&state->cover_scaled);
    free(state->cover_ascii);
    state->cover_ascii = NULL;
    state->cover_ascii_columns = 0;
    state->cover_ascii_rows = 0;
}

void smp_ui_set_directory(SmpUiState *state, const char *directory)
{
    if (state == NULL || directory == NULL) {
        return;
    }

    snprintf(state->music_directory, sizeof(state->music_directory), "%s", directory);
    smp_image_free(&state->cover);
    smp_image_free(&state->cover_scaled);
    state->cover_scaled_width = 0;
    state->cover_scaled_height = 0;
    state->cover_path[0] = '\0';
    state->cover_loaded = false;
}

void smp_ui_set_message(SmpUiState *state, const char *text, uint64_t now_ms)
{
    if (state == NULL || text == NULL) {
        return;
    }

    snprintf(state->message, sizeof(state->message), "%s", text);
    state->message_until = now_ms + UI_MESSAGE_MS;
}

static void ensure_cover(SmpUiState *state)
{
    char path[SMP_PATH_MAX];

    if (state->cover_loaded) {
        return;
    }

    state->cover_loaded = true;

    if (state->music_directory[0] == '\0') {
        return;
    }
    if (smp_cover_find(state->music_directory, path, sizeof(path)) != SMP_OK) {
        return;
    }
    if (smp_image_load(path, &state->cover) != SMP_OK) {
        return;
    }

    snprintf(state->cover_path, sizeof(state->cover_path), "%s", path);
}

static void format_time(double seconds, char *buffer, size_t size)
{
    int total;
    int hours;
    int minutes;
    int secs;

    if (seconds < 0.0) {
        seconds = 0.0;
    }

    total = (int)(seconds + 0.5);
    hours = total / 3600;
    minutes = (total % 3600) / 60;
    secs = total % 60;

    if (hours > 0) {
        snprintf(buffer, size, "%d:%02d:%02d", hours, minutes, secs);
    } else {
        snprintf(buffer, size, "%d:%02d", minutes, secs);
    }
}

static void put_styled(SmpFrame *frame, int x, int y, const char *text, SmpRgb color)
{
    smp_frame_set_style(frame, smp_style_fg(color.r, color.g, color.b));
    (void)smp_frame_put(frame, x, y, text);
}

static void put_ellipsized(SmpFrame *frame,
                           int x,
                           int y,
                           int width,
                           const char *text,
                           SmpRgb color)
{
    char buffer[512];

    smp_utf8_ellipsize(text != NULL ? text : "", width, buffer, sizeof(buffer));
    put_styled(frame, x, y, buffer, color);
}

static void draw_header(SmpFrame *frame, const SmpPlayerSnapshot *player)
{
    char volume[64];
    int filled;
    int bar_cells;
    int index;
    int x;

    smp_frame_set_style(frame, smp_style_bold(g_theme.accent.r, g_theme.accent.g, g_theme.accent.b));
    (void)smp_frame_put(frame, 1, 0, "SMP");

    smp_frame_set_style(frame, smp_style_dim(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
    (void)smp_frame_put(frame, 5, 0, "Swag Music Player");

    bar_cells = 16;
    filled = player->volume * bar_cells / 100;

    snprintf(volume, sizeof(volume), "%3d%%", player->volume);
    x = frame->width - bar_cells - 8;
    if (x < 30) {
        return;
    }

    smp_frame_set_style(frame, smp_style_fg(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
    (void)smp_frame_put(frame, x, 0, "vol ");
    (void)smp_frame_put(frame, x + 4, 0, volume + 0);

    smp_frame_set_style(frame, smp_style_fg(g_theme.accent.r, g_theme.accent.g, g_theme.accent.b));
    for (index = 0; index < bar_cells; index++) {
        (void)smp_frame_put(frame, x + 9 + index, 0, index < filled ? BAR_FULL : BAR_EMPTY);
    }
}

static void draw_separator(SmpFrame *frame, int y, int x, int width)
{
    int index;

    smp_frame_set_style(frame, smp_style_dim(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
    for (index = 0; index < width; index++) {
        (void)smp_frame_put(frame, x + index, y, SEPARATOR);
    }
}

/*
 * Own ASCII-art implementation: the cover is averaged down to one character
 * per cell and the luminance picks a glyph from the ramp. No external tool
 * (tiv, chafa, ImageMagick) is involved.
 */
static void draw_cover_ascii(SmpUiState *state,
                             SmpFrame *frame,
                             int x,
                             int y,
                             int width,
                             int height)
{
    int columns = SMP_MIN(width, height * 2);
    int rows;
    size_t stride;
    char *text;
    int offset_x;
    int offset_y;
    int row;
    int column;

    if (columns < 4) {
        return;
    }

    rows = columns / 2;
    if (rows > height) {
        rows = height;
        columns = rows * 2;
    }
    if (rows < 2 || columns > state->cover.width || rows > state->cover.height) {
        return;
    }

    stride = (size_t)columns + 1u;

    if (state->cover_ascii == NULL || state->cover_ascii_columns != columns
        || state->cover_ascii_rows != rows)
    {
        char *fresh = (char *)realloc(state->cover_ascii, stride * (size_t)rows);

        if (fresh == NULL) {
            return;
        }
        state->cover_ascii = fresh;
        state->cover_ascii_stride = stride;
        state->cover_ascii_columns = columns;
        state->cover_ascii_rows = rows;

        if (!smp_image_to_ascii(&state->cover, columns, rows, fresh, stride)) {
            state->cover_ascii_columns = 0;
            return;
        }
    }

    text = state->cover_ascii;
    stride = state->cover_ascii_stride;

    offset_x = x + (width - columns) / 2;
    offset_y = y + (height - rows) / 2;

    for (row = 0; row < rows; row++) {
        for (column = 0; column < columns; column++) {
            char glyph = text[(size_t)row * stride + (size_t)column];

            if (glyph == ' ') {
                continue;
            }
            smp_frame_put_cell(frame,
                               offset_x + column,
                               offset_y + row,
                               (uint32_t)(unsigned char)glyph,
                               g_theme.cover.r,
                               g_theme.cover.g,
                               g_theme.cover.b,
                               false,
                               0,
                               0,
                               0);
        }
    }

}

static const uint8_t *image_pixel(const SmpImage *image, int x, int y)
{
    return image->pixels + ((size_t)y * (size_t)image->width + (size_t)x) * 4u;
}

static void draw_cover(SmpUiState *state, SmpFrame *frame, int x, int y, int width, int height)
{
    int cells_x;
    int cells_y;
    int row;

    ensure_cover(state);

    if (state->cover.pixels == NULL || state->cover.width <= 0 || state->cover.height <= 0) {
        smp_frame_set_style(frame, smp_style_dim(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));

        {
            const char *text = "no cover art";
            int length = (int)strlen(text);
            int text_x = x + (width - length) / 2;

            if (text_x < x) {
                text_x = x;
            }
            (void)smp_frame_put(frame, text_x, y + height / 2, text);
        }
        return;
    }

    if (g_theme.cover_mode == SMP_COVER_ASCII) {
        draw_cover_ascii(state, frame, x, y, width, height);
        return;
    }

    /* Fill the whole panel: the image is cropped, never letterboxed. */
    cells_x = width;
    cells_y = height;
    if (cells_x < 1 || cells_y < 1) {
        return;
    }

    if (state->cover_scaled_width != cells_x || state->cover_scaled_height != cells_y * 2) {
        smp_image_free(&state->cover_scaled);
        if (smp_image_fill_box(&state->cover, &state->cover_scaled, cells_x, cells_y * 2) != SMP_OK) {
            state->cover_scaled_width = 0;
            state->cover_scaled_height = 0;
            return;
        }
        state->cover_scaled_width = cells_x;
        state->cover_scaled_height = cells_y * 2;
    }

    for (row = 0; row < cells_y; row++) {
        int column;

        for (column = 0; column < cells_x; column++) {
            const uint8_t *top = image_pixel(&state->cover_scaled, column, row * 2);
            const uint8_t *bottom = image_pixel(&state->cover_scaled, column, row * 2 + 1);

            (void)smp_frame_put_cell(frame,
                                     x + column,
                                     y + row,
                                     0x2580u, /* upper half block */
                                     top[0],
                                     top[1],
                                     top[2],
                                     true,
                                     bottom[0],
                                     bottom[1],
                                     bottom[2]);
        }
    }
}

/* Cava style bars: wide columns, right aligned, with sub-cell precision. */
static void draw_spectrum(SmpUiState *state, SmpFrame *frame, int x, int y, int width, int height)
{
    /* Unicode block elements from 1/8 to 8/8 of a cell. */
    static const uint32_t partials[8]
        = {0x2581u, 0x2582u, 0x2583u, 0x2584u, 0x2585u, 0x2586u, 0x2587u, 0x2588u};
    size_t bands;
    size_t band;
    int bar_width;
    int block_width;
    int start_x;

    if (state->spectrum == NULL || height < 1 || width < 16) {
        return;
    }

    bands = smp_spectrum_band_count(state->spectrum);
    if (bands == 0) {
        return;
    }

    /* Wide bars with a one column gap, the whole group pushed to the right. */
    bar_width = width / (int)bands;
    if (bar_width < 2) {
        bar_width = 2;
    }
    block_width = bar_width - 1;
    start_x = x + width - (int)bands * bar_width;

    for (band = 0; band < bands; band++) {
        int bar_x = start_x + (int)band * bar_width;
        float value = smp_spectrum_band(state->spectrum, band);
        int eighths;
        int full_rows;
        int remainder;
        int row;

        if (bar_x + block_width > x + width) {
            break;
        }

        eighths = (int)(value * (float)(height * 8) + 0.5f);
        full_rows = eighths / 8;
        remainder = eighths % 8;

        if (full_rows > height) {
            full_rows = height;
            remainder = 0;
        }

        for (row = 0; row < full_rows; row++) {
            int column;

            for (column = 0; column < block_width; column++) {
                smp_frame_put_cell(frame,
                                   bar_x + column,
                                   y + height - 1 - row,
                                   0x2588u,
                                   g_theme.spectrum.r,
                                   g_theme.spectrum.g,
                                   g_theme.spectrum.b,
                                   false,
                                   0,
                                   0,
                                   0);
            }
        }

        if (remainder > 0 && full_rows < height) {
            int column;

            for (column = 0; column < block_width; column++) {
                smp_frame_put_cell(frame,
                                   bar_x + column,
                                   y + height - 1 - full_rows,
                                   partials[remainder - 1],
                                   g_theme.spectrum.r,
                                   g_theme.spectrum.g,
                                   g_theme.spectrum.b,
                                   false,
                                   0,
                                   0,
                                   0);
            }
        }
    }
}

static const char *state_label(SmpPlayerState state)
{
    switch (state) {
    case SMP_PLAYER_PLAYING:
        return "playing";
    case SMP_PLAYER_PAUSED:
        return "paused";
    case SMP_PLAYER_ERROR:
        return "error";
    case SMP_PLAYER_STOPPED:
    default:
        return "stopped";
    }
}

static void draw_now_playing(SmpFrame *frame,
                             int x,
                             int y,
                             int width,
                             const SmpPlayerSnapshot *player,
                             const char *label)
{
    char title[192];
    char subtitle[320];
    char buffer[64];

    smp_frame_set_style(frame, smp_style_dim(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
    (void)smp_frame_put(frame, x, y, label);

    smp_frame_set_style(frame, smp_style_bold(g_theme.text.r, g_theme.text.g, g_theme.text.b));
    smp_utf8_ellipsize(player->title, width, title, sizeof(title));
    (void)smp_frame_put(frame, x, y + 1, title);

    snprintf(subtitle, sizeof(subtitle), "%s", player->artist);

    smp_frame_set_style(frame, smp_style_fg(g_theme.accent.r, g_theme.accent.g, g_theme.accent.b));
    smp_utf8_ellipsize(subtitle, width, buffer, sizeof(buffer));
    (void)smp_frame_put(frame, x, y + 2, buffer);

    smp_frame_set_style(frame, smp_style_dim(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
    snprintf(subtitle, sizeof(subtitle), "%s", player->album);
    smp_utf8_ellipsize(subtitle, width, buffer, sizeof(buffer));
    (void)smp_frame_put(frame, x, y + 3, buffer);
}

static void draw_progress(SmpFrame *frame,
                          int x,
                          int y,
                          int width,
                          const SmpPlayerSnapshot *player)
{
    int bar_width = width - 22;
    int filled = 0;
    int index;
    char elapsed[32];
    char total[32];

    if (bar_width < 10) {
        bar_width = 10;
    }

    if (player->duration > 0.0) {
        double ratio = player->position / player->duration;

        if (ratio < 0.0) {
            ratio = 0.0;
        }
        if (ratio > 1.0) {
            ratio = 1.0;
        }
        filled = (int)(ratio * (double)bar_width);
    }

    smp_frame_set_style(frame, smp_style_fg(g_theme.text.r, g_theme.text.g, g_theme.text.b));
    (void)smp_frame_put(frame, x, y, "[");

    for (index = 0; index < bar_width; index++) {
        if (index < filled) {
            smp_frame_set_style(frame, smp_style_fg(g_theme.accent.r, g_theme.accent.g, g_theme.accent.b));
            (void)smp_frame_put(frame, x + 1 + index, y, BAR_FULL);
        } else {
            smp_frame_set_style(frame, smp_style_fg(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
            (void)smp_frame_put(frame, x + 1 + index, y, BAR_EMPTY);
        }
    }

    smp_frame_set_style(frame, smp_style_fg(g_theme.text.r, g_theme.text.g, g_theme.text.b));
    (void)smp_frame_put(frame, x + 1 + bar_width, y, "]");

    format_time(player->position, elapsed, sizeof(elapsed));
    if (player->duration > 0.0) {
        format_time(player->duration, total, sizeof(total));
    } else {
        snprintf(total, sizeof(total), "--:--");
    }

    smp_frame_set_style(frame, smp_style_dim(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
    (void)smp_frame_put(frame, x + bar_width + 3, y, elapsed);
    (void)smp_frame_put(frame, x + bar_width + 3 + (int)strlen(elapsed), y, " / ");
    (void)smp_frame_put(frame, x + bar_width + 6 + (int)strlen(elapsed), y, total);
}

/* Rough perceived brightness, used to detect an unreadable selection colour. */
static int color_luminance(SmpRgb color)
{
    return (2126 * (int)color.r + 7152 * (int)color.g + 722 * (int)color.b) / 10000;
}

static bool low_contrast(SmpRgb first, SmpRgb second)
{
    int difference = color_luminance(first) - color_luminance(second);

    if (difference < 0) {
        difference = -difference;
    }
    return difference < 48;
}

static void draw_playlist(SmpUiState *state,
                          SmpFrame *frame,
                          int x,
                          int y,
                          int width,
                          int height,
                          const Playlist *playlist,
                          const SmpPlayerSnapshot *player)
{
    char header[128];
    size_t visible = (size_t)height;
    size_t index;

    smp_frame_set_style(frame, smp_style_bold(g_theme.text.r, g_theme.text.g, g_theme.text.b));
    snprintf(header,
             sizeof(header),
             "playlist  %zu %s",
             playlist->count,
             playlist->count == 1u ? "track" : "tracks");
    (void)smp_frame_put(frame, x, y, header);

    if (visible < 1 || playlist->count == 0) {
        smp_frame_set_style(frame, smp_style_dim(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
        (void)smp_frame_put(frame, x, y + 1, "no tracks in this directory");
        return;
    }

    if (state->selection >= playlist->count) {
        state->selection = playlist->count - 1u;
    }

    if (state->selection < state->scroll) {
        state->scroll = state->selection;
    }
    if (state->selection >= state->scroll + visible) {
        state->scroll = state->selection - visible + 1u;
    }

    for (index = 0; index < visible; index++) {
        size_t track_index = state->scroll + index;
        const SmpTrack *track;
        char label[512];
        char title[256];
        char duration[32];
        bool is_current;
        bool is_selected;

        if (track_index >= playlist->count) {
            break;
        }

        track = &playlist->tracks[track_index];
        is_current = track_index == player->track_index;
        is_selected = track_index == state->selection;

        smp_utf8_ellipsize(track->title, width - 12, title, sizeof(title));
        if (track->duration > 0.0) {
            format_time(track->duration, duration, sizeof(duration));
        } else {
            snprintf(duration, sizeof(duration), "--:--");
        }

        snprintf(label, sizeof(label), "%2zu. %s", track_index + 1u, title);

        if (is_selected) {
            SmpStyle style = smp_style_fg(g_theme.text.r, g_theme.text.g, g_theme.text.b);

            if (low_contrast(g_theme.selection_bg, g_theme.text)) {
                /* White on white would be invisible, so highlight by inversion. */
                style.flags |= SMP_CELL_REVERSE;
            } else {
                style.has_bg = true;
                style.bg[0] = g_theme.selection_bg.r;
                style.bg[1] = g_theme.selection_bg.g;
                style.bg[2] = g_theme.selection_bg.b;
            }

            smp_frame_set_style(frame, style);
            smp_frame_fill(frame, x, y + 1 + (int)index, width, 1, ' ');
        } else {
            smp_frame_set_style(frame,
                                smp_style_fg(is_current ? g_theme.playing.r : g_theme.text.r,
                                             is_current ? g_theme.playing.g : g_theme.text.g,
                                             is_current ? g_theme.playing.b : g_theme.text.b));
        }

        (void)smp_frame_put(frame, x, y + 1 + (int)index, label);

        smp_frame_set_style(frame, smp_style_dim(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
        (void)smp_frame_put(frame,
                            x + width - (int)strlen(duration) - 1,
                            y + 1 + (int)index,
                            duration);

        if (is_current) {
            (void)smp_frame_put(frame, x + width - 1, y + 1 + (int)index, MARKER);
        }
    }
}

static void draw_help(SmpFrame *frame)
{
    static const char *const lines[] = {
        "  keyboard shortcuts",
        "",
        "  up / down      move the selection",
        "  enter          play the selected track",
        "  space          play / pause",
        "  left / right   seek 5 seconds",
        "  n / p          next / previous track",
        "  + / -          volume",
        "  page up/down   volume",
        "  r              reload the library",
        "  v              toggle the visualiser",
        "  a              ASCII / block cover art",
        "  h / ?          toggle this help",
        "  q / esc        quit",
    };
    int box_width = 40;
    int box_height = (int)SMP_ARRAY_LEN(lines) + 2;
    int x = (frame->width - box_width) / 2;
    int y = (frame->height - box_height) / 2;
    int index;

    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }

    {
        SmpStyle style = smp_style_fg(g_theme.text.r, g_theme.text.g, g_theme.text.b);

        style.has_bg = true;
        style.bg[0] = 24;
        style.bg[1] = 26;
        style.bg[2] = 33;
        smp_frame_set_style(frame, style);
        smp_frame_fill(frame, x, y, box_width, box_height, ' ');
    }

    for (index = 0; index < (int)SMP_ARRAY_LEN(lines); index++) {
        if (y + 1 + index >= frame->height) {
            break;
        }
        smp_frame_set_style(frame,
                            index == 0 ? smp_style_bold(g_theme.accent.r, g_theme.accent.g, g_theme.accent.b)
                                       : smp_style_fg(g_theme.text.r, g_theme.text.g, g_theme.text.b));
        (void)smp_frame_put_n(frame, x + 1, y + 1 + index, lines[index], box_width - 2);
    }
}

static void draw_footer(SmpUiState *state,
                        SmpFrame *frame,
                        const SmpPlayerSnapshot *player,
                        const char *backend,
                        uint64_t now_ms)
{
    static const char *const keys
        = " \xe2\x86\x91\xe2\x86\x93  \xe2\x86\xb5 play  space pause  \xe2\x86\x90\xe2\x86\x92 seek  "
          "+/- vol  v viz  a art  ? help  q quit";
    int status_y = frame->height - 2;
    char status[320];

    draw_separator(frame, status_y - 1, 0, frame->width);

    if (state->message[0] != '\0' && now_ms < state->message_until) {
        put_ellipsized(frame, 1, status_y, frame->width - 2, state->message, g_theme.accent);
    } else {
        snprintf(status,
                 sizeof(status),
                 "%s  track %zu/%zu  volume %d%%  audio %s",
                 state_label(player->state),
                 player->track_count == 0 ? 0u : player->track_index + 1u,
                 player->track_count,
                 player->volume,
                 backend != NULL ? backend : "?");

        smp_frame_set_style(frame,
                            smp_style_fg(player->state == SMP_PLAYER_ERROR ? g_theme.error.r : g_theme.dim.r,
                                         player->state == SMP_PLAYER_ERROR ? g_theme.error.g : g_theme.dim.g,
                                         player->state == SMP_PLAYER_ERROR ? g_theme.error.b : g_theme.dim.b));
        put_ellipsized(frame, 1, status_y, frame->width - 2, status, g_theme.dim);
    }

    smp_frame_set_style(frame, smp_style_dim(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
    put_ellipsized(frame, 1, frame->height - 1, frame->width - 2, keys, g_theme.dim);
}

void smp_ui_render(SmpUiState *state,
                   SmpFrame *frame,
                   const Playlist *playlist,
                   const SmpPlayerSnapshot *player,
                   const char *backend,
                   uint64_t now_ms)
{
    int left_width;
    int right_x;
    int right_width;
    int playlist_y;
    int panel_top;
    int panel_bottom;
    int spectrum_height;
    int cover_bottom;

    if (state == NULL || frame == NULL || playlist == NULL || player == NULL || frame->cells == NULL) {
        return;
    }

    smp_frame_clear(frame);

    if (frame->width < UI_MIN_WIDTH || frame->height < UI_MIN_HEIGHT) {
        char message[128];

        snprintf(message,
                 sizeof(message),
                 "terminal too small (%dx%d), need %dx%d",
                 frame->width,
                 frame->height,
                 UI_MIN_WIDTH,
                 UI_MIN_HEIGHT);
        smp_frame_set_style(frame, smp_style_fg(g_theme.error.r, g_theme.error.g, g_theme.error.b));
        (void)smp_frame_put(frame, 1, frame->height / 2, message);
        return;
    }

    left_width = frame->width * 45 / 100;
    if (left_width < 24) {
        left_width = 24;
    }
    if (left_width > frame->width - 34) {
        left_width = frame->width - 34;
    }

    right_x = left_width + 1;
    right_width = frame->width - right_x;

    /*
     * While the player is idle the panel describes the highlighted track,
     * which is what the user is about to start.
     */
    {
        SmpPlayerSnapshot inactive = *player;

        if (player->state == SMP_PLAYER_STOPPED && playlist->count > 0
            && state->selection < playlist->count)
        {
            const SmpTrack *track = &playlist->tracks[state->selection];

            snprintf(inactive.title, sizeof(inactive.title), "%s", track->title != NULL ? track->title : "");
            snprintf(inactive.artist, sizeof(inactive.artist), "%s", track->artist != NULL ? track->artist : "");
            snprintf(inactive.album, sizeof(inactive.album), "%s", track->album != NULL ? track->album : "");
            inactive.duration = track->duration;
            inactive.position = 0.0;
        }

        draw_header(frame, &inactive);
        draw_separator(frame, 1, 0, frame->width);

        /* Left panel: the cover fills its half, the visualiser sits right below. */
        {
            panel_top = 2;
            panel_bottom = frame->height - 4;
            spectrum_height = 0;
            cover_bottom = panel_bottom;

            if (state->spectrum != NULL && state->theme.show_spectrum
                && panel_bottom - panel_top > 12)
            {
                spectrum_height = SMP_MIN(5, (panel_bottom - panel_top) / 4);
                cover_bottom = panel_bottom - spectrum_height - 1;
            }

            draw_cover(state, frame, 0, panel_top, left_width, cover_bottom - panel_top + 1);

            if (spectrum_height > 0) {
                draw_separator(frame, cover_bottom + 1, 0, left_width);
                draw_spectrum(state, frame, 0, cover_bottom + 2, left_width, spectrum_height);
            }
        }

        /* Right panel: track info, progress and playlist. */
        draw_now_playing(frame,
                         right_x,
                         2,
                         right_width,
                         &inactive,
                         player->state == SMP_PLAYER_STOPPED ? "selected" : "now playing");
        draw_progress(frame, right_x, 6, right_width, &inactive);
        draw_separator(frame, 7, right_x, right_width);
    }

    playlist_y = 8;
    {
        int item_rows = (frame->height - 4) - (playlist_y + 1) + 1;

        if (item_rows < 1) {
            item_rows = 1;
        }
        draw_playlist(state, frame, right_x, playlist_y, right_width, item_rows, playlist, player);
    }

    /* Vertical separator between the panels. */
    {
        int row;

        smp_frame_set_style(frame, smp_style_dim(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
        for (row = 2; row <= frame->height - 4; row++) {
            (void)smp_frame_put(frame, left_width, row, VERTICAL);
        }
    }

    draw_footer(state, frame, player, backend, now_ms);

    /*
     * Junction glyphs: without them a one cell gap shows up between the
     * horizontal rules and the vertical divider.
     */
    smp_frame_set_style(frame, smp_style_dim(g_theme.dim.r, g_theme.dim.g, g_theme.dim.b));
    (void)smp_frame_put(frame, left_width, 1, JUNCTION_TOP);
    (void)smp_frame_put(frame, left_width, 7, JUNCTION_LEFT);
    (void)smp_frame_put(frame, left_width, frame->height - 3, JUNCTION_BOTTOM);
    if (spectrum_height > 0) {
        (void)smp_frame_put(frame, left_width, cover_bottom + 1, JUNCTION_RIGHT);
    }

    if (state->show_help) {
        draw_help(frame);
    }
}

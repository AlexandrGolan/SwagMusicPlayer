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

/* Colour palette. */
static const uint8_t color_accent[3] = {86, 182, 194};
static const uint8_t color_text[3] = {220, 223, 228};
static const uint8_t color_dim[3] = {120, 126, 138};
static const uint8_t color_selected_bg[3] = {38, 48, 64};
static const uint8_t color_playing[3] = {152, 195, 121};
static const uint8_t color_error[3] = {224, 108, 117};

#define SEPARATOR "\xe2\x94\x80"  /* РІвЂќР‚ */
#define BAR_FULL "\xe2\x96\x88"   /* РІвЂ“в‚¬ */
#define BAR_EMPTY "\xe2\x96\x91"  /* РІвЂ“вЂ */
#define MARKER "\xe2\x96\xb6"     /* РІвЂ“В¶ */

void smp_ui_init(SmpUiState *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    smp_image_init(&state->cover);
}

void smp_ui_free(SmpUiState *state)
{
    if (state == NULL) {
        return;
    }
    smp_image_free(&state->cover);
}

void smp_ui_set_directory(SmpUiState *state, const char *directory)
{
    if (state == NULL || directory == NULL) {
        return;
    }

    snprintf(state->music_directory, sizeof(state->music_directory), "%s", directory);
    smp_image_free(&state->cover);
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

static void put_styled(SmpFrame *frame, int x, int y, const char *text, const uint8_t color[3])
{
    smp_frame_set_style(frame, smp_style_fg(color[0], color[1], color[2]));
    (void)smp_frame_put(frame, x, y, text);
}

static void put_ellipsized(SmpFrame *frame,
                           int x,
                           int y,
                           int width,
                           const char *text,
                           const uint8_t color[3])
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

    smp_frame_set_style(frame, smp_style_bold(color_accent[0], color_accent[1], color_accent[2]));
    (void)smp_frame_put(frame, 1, 0, "SMP");

    smp_frame_set_style(frame, smp_style_dim(color_dim[0], color_dim[1], color_dim[2]));
    (void)smp_frame_put(frame, 5, 0, "Swag Music Player");

    bar_cells = 16;
    filled = player->volume * bar_cells / 100;

    snprintf(volume, sizeof(volume), "%3d%%", player->volume);
    x = frame->width - bar_cells - 8;
    if (x < 30) {
        return;
    }

    smp_frame_set_style(frame, smp_style_fg(color_dim[0], color_dim[1], color_dim[2]));
    (void)smp_frame_put(frame, x, 0, "vol ");
    (void)smp_frame_put(frame, x + 4, 0, volume + 0);

    smp_frame_set_style(frame, smp_style_fg(color_accent[0], color_accent[1], color_accent[2]));
    for (index = 0; index < bar_cells; index++) {
        (void)smp_frame_put(frame, x + 9 + index, 0, index < filled ? BAR_FULL : BAR_EMPTY);
    }
}

static void draw_separator(SmpFrame *frame, int y, int x, int width)
{
    int index;

    smp_frame_set_style(frame, smp_style_dim(color_dim[0], color_dim[1], color_dim[2]));
    for (index = 0; index < width; index++) {
        (void)smp_frame_put(frame, x + index, y, SEPARATOR);
    }
}

static void draw_cover(SmpUiState *state, SmpFrame *frame, int x, int y, int width, int height)
{
    int cells_x;
    int cells_y;
    int offset_x;
    int offset_y;
    int row;

    ensure_cover(state);

    if (state->cover.pixels == NULL || state->cover.width <= 0 || state->cover.height <= 0) {
        smp_frame_set_style(frame, smp_style_dim(color_dim[0], color_dim[1], color_dim[2]));

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

    /* Two vertical pixels per terminal cell; keep the aspect ratio. */
    cells_y = SMP_MIN(height, width / 2);
    cells_x = cells_y * 2;
    if (cells_x > width) {
        cells_x = width;
        cells_y = cells_x / 2;
    }
    if (cells_x < 2 || cells_y < 1) {
        return;
    }

    {
        SmpImage scaled;

        if (smp_image_resize_box(&state->cover, &scaled, cells_x, cells_y * 2) != SMP_OK) {
            return;
        }

        offset_x = x + (width - cells_x) / 2;
        offset_y = y + (height - cells_y) / 2;

        for (row = 0; row < cells_y; row++) {
            int column;

            for (column = 0; column < cells_x; column++) {
                const uint8_t *top
                    = scaled.pixels + (((size_t)(row * 2) * (size_t)scaled.width) + (size_t)column) * 4u;
                const uint8_t *bottom
                    = scaled.pixels
                      + (((size_t)(row * 2 + 1) * (size_t)scaled.width) + (size_t)column) * 4u;

                smp_frame_put_cell(frame,
                                   offset_x + column,
                                   offset_y + row,
                                   0x2580u, /* РІвЂ“Р‚ upper half block */
                                   top[0],
                                   top[1],
                                   top[2],
                                   true,
                                   bottom[0],
                                   bottom[1],
                                   bottom[2]);
            }
        }

        smp_image_free(&scaled);
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

    smp_frame_set_style(frame, smp_style_dim(color_dim[0], color_dim[1], color_dim[2]));
    (void)smp_frame_put(frame, x, y, label);

    smp_frame_set_style(frame, smp_style_bold(color_text[0], color_text[1], color_text[2]));
    smp_utf8_ellipsize(player->title, width, title, sizeof(title));
    (void)smp_frame_put(frame, x, y + 1, title);

    snprintf(subtitle, sizeof(subtitle), "%s", player->artist);

    smp_frame_set_style(frame, smp_style_fg(color_accent[0], color_accent[1], color_accent[2]));
    smp_utf8_ellipsize(subtitle, width, buffer, sizeof(buffer));
    (void)smp_frame_put(frame, x, y + 2, buffer);

    smp_frame_set_style(frame, smp_style_dim(color_dim[0], color_dim[1], color_dim[2]));
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

    smp_frame_set_style(frame, smp_style_fg(color_text[0], color_text[1], color_text[2]));
    (void)smp_frame_put(frame, x, y, "[");

    for (index = 0; index < bar_width; index++) {
        if (index < filled) {
            smp_frame_set_style(frame, smp_style_fg(color_accent[0], color_accent[1], color_accent[2]));
            (void)smp_frame_put(frame, x + 1 + index, y, BAR_FULL);
        } else {
            smp_frame_set_style(frame, smp_style_fg(color_dim[0], color_dim[1], color_dim[2]));
            (void)smp_frame_put(frame, x + 1 + index, y, BAR_EMPTY);
        }
    }

    smp_frame_set_style(frame, smp_style_fg(color_text[0], color_text[1], color_text[2]));
    (void)smp_frame_put(frame, x + 1 + bar_width, y, "]");

    format_time(player->position, elapsed, sizeof(elapsed));
    if (player->duration > 0.0) {
        format_time(player->duration, total, sizeof(total));
    } else {
        snprintf(total, sizeof(total), "--:--");
    }

    smp_frame_set_style(frame, smp_style_dim(color_dim[0], color_dim[1], color_dim[2]));
    (void)smp_frame_put(frame, x + bar_width + 3, y, elapsed);
    (void)smp_frame_put(frame, x + bar_width + 3 + (int)strlen(elapsed), y, " / ");
    (void)smp_frame_put(frame, x + bar_width + 6 + (int)strlen(elapsed), y, total);
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

    smp_frame_set_style(frame, smp_style_bold(color_text[0], color_text[1], color_text[2]));
    snprintf(header,
             sizeof(header),
             "playlist  %zu %s",
             playlist->count,
             playlist->count == 1u ? "track" : "tracks");
    (void)smp_frame_put(frame, x, y, header);

    if (visible < 1 || playlist->count == 0) {
        smp_frame_set_style(frame, smp_style_dim(color_dim[0], color_dim[1], color_dim[2]));
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
            smp_frame_set_style(frame,
                                smp_style_fg(color_text[0], color_text[1], color_text[2]));
            {
                SmpStyle style = smp_style_fg(color_text[0], color_text[1], color_text[2]);

                style.has_bg = true;
                style.bg[0] = color_selected_bg[0];
                style.bg[1] = color_selected_bg[1];
                style.bg[2] = color_selected_bg[2];
                smp_frame_set_style(frame, style);
            }
            smp_frame_fill(frame, x, y + 1 + (int)index, width, 1, ' ');
        } else {
            smp_frame_set_style(frame,
                                smp_style_fg(is_current ? color_playing[0] : color_text[0],
                                             is_current ? color_playing[1] : color_text[1],
                                             is_current ? color_playing[2] : color_text[2]));
        }

        (void)smp_frame_put(frame, x, y + 1 + (int)index, label);

        smp_frame_set_style(frame, smp_style_dim(color_dim[0], color_dim[1], color_dim[2]));
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
        SmpStyle style = smp_style_fg(color_text[0], color_text[1], color_text[2]);

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
                            index == 0 ? smp_style_bold(color_accent[0], color_accent[1], color_accent[2])
                                       : smp_style_fg(color_text[0], color_text[1], color_text[2]));
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
        = " \xe2\x86\x91\xe2\x86\x93 select  \xe2\x86\xb5 play  space pause  \xe2\x86\x90\xe2\x86\x92 seek  "
          "+/- vol  n/p  ? help  q quit";
    int status_y = frame->height - 2;
    char status[320];

    draw_separator(frame, status_y - 1, 0, frame->width);

    if (state->message[0] != '\0' && now_ms < state->message_until) {
        put_ellipsized(frame, 1, status_y, frame->width - 2, state->message, color_accent);
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
                            smp_style_fg(player->state == SMP_PLAYER_ERROR ? color_error[0] : color_dim[0],
                                         player->state == SMP_PLAYER_ERROR ? color_error[1] : color_dim[1],
                                         player->state == SMP_PLAYER_ERROR ? color_error[2] : color_dim[2]));
        put_ellipsized(frame, 1, status_y, frame->width - 2, status, color_dim);
    }

    smp_frame_set_style(frame, smp_style_dim(color_dim[0], color_dim[1], color_dim[2]));
    put_ellipsized(frame, 1, frame->height - 1, frame->width - 2, keys, color_dim);
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
    int playlist_rows;
    int playlist_y;

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
        smp_frame_set_style(frame, smp_style_fg(color_error[0], color_error[1], color_error[2]));
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

    right_x = left_width + 2;
    right_width = frame->width - right_x - 1;

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

        /* Left panel: cover art. */
        draw_cover(state, frame, 1, 3, left_width - 2, frame->height - 7);
        draw_separator(frame, 2, 0, frame->width);

        /* Right panel: track info and playlist. */
        draw_now_playing(frame,
                         right_x,
                         3,
                         right_width,
                         &inactive,
                         player->state == SMP_PLAYER_STOPPED ? "selected" : "now playing");
        draw_progress(frame, right_x, 7, right_width, &inactive);
        draw_separator(frame, 8, right_x, right_width);
    }

    playlist_y = 9;

    playlist_y = 9;
    playlist_rows = frame->height - playlist_y - 3;
    if (playlist_rows < 1) {
        playlist_rows = 1;
    }
    draw_playlist(state, frame, right_x, playlist_y, right_width, playlist_rows - 1, playlist, player);

    /* Vertical separator between the panels. */
    {
        int row;

        smp_frame_set_style(frame, smp_style_dim(color_dim[0], color_dim[1], color_dim[2]));
        for (row = 2; row < frame->height - 2; row++) {
            (void)smp_frame_put(frame, left_width, row, "\xe2\x94\x82");
        }
    }

    draw_footer(state, frame, player, backend, now_ms);

    if (state->show_help) {
        draw_help(frame);
    }
}

/*
 * SMP - Swag Music Player
 * Terminal user interface.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_UI_H
#define SMP_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smp/config.h"
#include "smp/cover.h"
#include "smp/fs.h"
#include "smp/frame.h"
#include "smp/playlist.h"
#include "smp/player.h"
#include "smp/spectrum.h"

/* One interface colour. */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} SmpRgb;

typedef enum {
    SMP_COVER_BLOCKS = 0, /* true colour half blocks */
    SMP_COVER_ASCII = 1,  /* luminance ramp ASCII art */
} SmpCoverMode;

/* Palette and layout options, normally mapped from the configuration file. */
typedef struct {
    SmpRgb text;
    SmpRgb accent;
    SmpRgb dim;
    SmpRgb selection_bg;
    SmpRgb playing;
    SmpRgb error;
    SmpRgb progress;
    SmpRgb volume;
    SmpRgb border;
    SmpRgb cover;
    SmpRgb spectrum;

    SmpCoverMode cover_mode;
    bool show_spectrum;
} SmpUiTheme;

void smp_ui_theme_defaults(SmpUiTheme *theme);

/* Maps the configuration file colours onto the interface palette. */
void smp_ui_theme_from_config(SmpUiTheme *theme, const SmpConfig *config);

typedef struct {
    SmpUiTheme theme;
    SmpSpectrum *spectrum;
    size_t selection;
    size_t scroll;
    bool show_help;

    char music_directory[SMP_PATH_MAX];
    char message[160];
    uint64_t message_until;

    char cover_path[SMP_PATH_MAX];
    SmpImage cover;
    bool cover_loaded;

    /* Scaled cover and rasterised ASCII art, rebuilt only when the size changes. */
    SmpImage cover_scaled;
    int cover_scaled_width;
    int cover_scaled_height;

    char *cover_ascii;
    size_t cover_ascii_stride;
    int cover_ascii_columns;
    int cover_ascii_rows;
} SmpUiState;

void smp_ui_init(SmpUiState *state);

/* Applies a palette; the default theme is pure white. */
void smp_ui_set_theme(SmpUiState *state, const SmpUiTheme *theme);

/* Attaches the analyser drawn at the bottom of the cover panel. */
void smp_ui_set_spectrum(SmpUiState *state, SmpSpectrum *spectrum);
void smp_ui_free(SmpUiState *state);

/* Sets the album directory used for cover art. */
void smp_ui_set_directory(SmpUiState *state, const char *directory);

/* Shows a transient message for a couple of seconds. */
void smp_ui_set_message(SmpUiState *state, const char *text, uint64_t now_ms);

/* Renders one complete frame. */
void smp_ui_render(SmpUiState *state,
                   SmpFrame *frame,
                   const Playlist *playlist,
                   const SmpPlayerSnapshot *player,
                   const char *backend,
                   uint64_t now_ms);

#endif /* SMP_UI_H */

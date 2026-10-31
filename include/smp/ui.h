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

#include "smp/cover.h"
#include "smp/fs.h"
#include "smp/frame.h"
#include "smp/playlist.h"
#include "smp/player.h"

typedef struct {
    size_t selection;
    size_t scroll;
    bool show_help;

    char music_directory[SMP_PATH_MAX];
    char message[160];
    uint64_t message_until;

    char cover_path[SMP_PATH_MAX];
    SmpImage cover;
    bool cover_loaded;
} SmpUiState;

void smp_ui_init(SmpUiState *state);
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

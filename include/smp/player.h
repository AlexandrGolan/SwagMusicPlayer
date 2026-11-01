/*
 * SMP - Swag Music Player
 * Playback engine: state machine and a single worker thread.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_PLAYER_H
#define SMP_PLAYER_H

#include <stdbool.h>
#include <stddef.h>

#include "smp/audio.h"
#include "smp/common.h"
#include "smp/playlist.h"
#include "smp/spectrum.h"

typedef enum {
    SMP_PLAYER_STOPPED = 0,
    SMP_PLAYER_PLAYING,
    SMP_PLAYER_PAUSED,
    SMP_PLAYER_ERROR,
} SmpPlayerState;

typedef struct {
    SmpPlayerState state;
    size_t track_index;
    size_t track_count;
    double position;
    double duration;
    int volume;
    char title[192];
    char artist[192];
    char album[192];
} SmpPlayerSnapshot;

typedef struct SmpPlayer SmpPlayer;

/*
 * Creates the playback engine. The playlist must outlive the player and must
 * not be modified while the player exists.
 */
SmpPlayer *smp_player_create(const Playlist *playlist, SmpAudioDevice *device);

/* Stops playback and joins the worker thread. */
void smp_player_destroy(SmpPlayer *player);

void smp_player_play(SmpPlayer *player, size_t index);
void smp_player_toggle(SmpPlayer *player);
void smp_player_stop(SmpPlayer *player);
void smp_player_next(SmpPlayer *player);
void smp_player_previous(SmpPlayer *player);

/* Absolute and relative seeks in seconds. */
void smp_player_seek(SmpPlayer *player, double seconds);
void smp_player_seek_by(SmpPlayer *player, double delta);

void smp_player_set_volume(SmpPlayer *player, int volume);

/* Feeds the decoded signal to a visualiser; pass NULL to detach. */
void smp_player_set_spectrum(SmpPlayer *player, SmpSpectrum *spectrum);
int smp_player_volume(SmpPlayer *player);

/* Copies a consistent view of the player state. */
void smp_player_snapshot(SmpPlayer *player, SmpPlayerSnapshot *snapshot);

/* True exactly once per naturally finished track. */
bool smp_player_take_finished(SmpPlayer *player);

#endif /* SMP_PLAYER_H */

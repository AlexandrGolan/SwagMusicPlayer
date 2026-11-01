/*
 * SMP - Swag Music Player
 * Playlist model: scanning, ordering and navigation.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_PLAYLIST_H
#define SMP_PLAYLIST_H

#include <stdbool.h>
#include <stddef.h>

#include "smp/common.h"

typedef struct {
    char *path;
    char *title;
    char *artist;
    char *album;
    double duration; /* seconds, 0 when unknown */
    char format[8];  /* "WAV", "MP3", "FLAC", "OGG" */
} SmpTrack;

typedef struct {
    SmpTrack *tracks;
    size_t count;
    size_t capacity;
    size_t current;
} Playlist;

void smp_playlist_init(Playlist *playlist);
void smp_playlist_free(Playlist *playlist);

/*
 * Scans `directory` (non recursive) for WAV and MP3 files, reads their tags
 * and sorts them naturally by file name.
 */
SmpStatus smp_playlist_load(Playlist *playlist, const char *directory);

/* Sorts the loaded tracks naturally by file name. */
void smp_playlist_sort(Playlist *playlist);

const SmpTrack *smp_playlist_at(const Playlist *playlist, size_t index);
const SmpTrack *smp_playlist_current(const Playlist *playlist);

/* Moves the cursor; both return false when there is nowhere to go. */
bool smp_playlist_advance(Playlist *playlist);
bool smp_playlist_rewind(Playlist *playlist);

/* Moves the cursor by `delta` with wrap-around (used by the UI). */
void smp_playlist_move(Playlist *playlist, int delta);
void smp_playlist_select(Playlist *playlist, size_t index);

#endif /* SMP_PLAYLIST_H */

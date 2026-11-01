/*
 * SMP - Swag Music Player
 * Playlist model: scanning, ordering and navigation.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/playlist.h"

#include <stdlib.h>
#include <string.h>

#include "smp/fs.h"
#include "smp/media.h"
#include "smp/str.h"
#include "smp/tags.h"

void smp_playlist_init(Playlist *playlist)
{
    if (playlist == NULL) {
        return;
    }
    playlist->tracks = NULL;
    playlist->count = 0;
    playlist->capacity = 0;
    playlist->current = 0;
}

static void free_track(SmpTrack *track)
{
    free(track->path);
    free(track->title);
    free(track->artist);
    free(track->album);
    track->path = NULL;
    track->title = NULL;
    track->artist = NULL;
    track->album = NULL;
}

void smp_playlist_free(Playlist *playlist)
{
    size_t index;

    if (playlist == NULL) {
        return;
    }

    for (index = 0; index < playlist->count; index++) {
        free_track(&playlist->tracks[index]);
    }
    free(playlist->tracks);
    playlist->tracks = NULL;
    playlist->count = 0;
    playlist->capacity = 0;
    playlist->current = 0;
}

static bool reserve(Playlist *playlist, size_t needed)
{
    size_t capacity;
    SmpTrack *grown;

    if (needed <= playlist->capacity) {
        return true;
    }

    capacity = playlist->capacity > 0 ? playlist->capacity : 16u;
    while (capacity < needed) {
        capacity *= 2u;
    }

    grown = (SmpTrack *)realloc(playlist->tracks, capacity * sizeof(SmpTrack));
    if (grown == NULL) {
        return false;
    }

    playlist->tracks = grown;
    playlist->capacity = capacity;
    return true;
}

/* Adds one file to the playlist. Returns false on allocation failure. */
static bool append_track(Playlist *playlist, const char *path)
{
    SmpTags tags;
    SmpMedia *media = NULL;
    SmpTrack *track;

    if (!reserve(playlist, playlist->count + 1u)) {
        return false;
    }

    track = &playlist->tracks[playlist->count];
    memset(track, 0, sizeof(*track));

    track->path = smp_strdup(path);
    if (track->path == NULL) {
        return false;
    }

    if (smp_tags_read_file(path, &tags) == SMP_OK) {
        track->title = smp_strdup(tags.title);
        track->artist = smp_strdup(tags.artist);
        track->album = smp_strdup(tags.album);
    }

    if (track->title == NULL) {
        const char *base = smp_path_basename(path);
        const char *dot = smp_path_extension(path);
        size_t length = dot != NULL ? (size_t)(dot - base) : strlen(base);

        track->title = smp_strdup_n(base, length);
    }
    if (track->artist == NULL) {
        track->artist = smp_strdup("Unknown artist");
    }
    if (track->album == NULL) {
        track->album = smp_strdup("Unknown album");
    }

    if (track->title == NULL || track->artist == NULL || track->album == NULL) {
        free_track(track);
        return false;
    }

    {
        const char *extension = smp_path_extension(path);
        size_t index = 0;

        if (extension != NULL) {
            extension++; /* skip the dot */
        }
        while (extension != NULL && extension[index] != '\0' && index < sizeof(track->format) - 1u) {
            char letter = extension[index];

            if (letter >= 'a' && letter <= 'z') {
                letter = (char)(letter - 'a' + 'A');
            }
            track->format[index] = letter;
            index++;
        }
        track->format[index] = '\0';
    }

    if (smp_media_open(path, &media) == SMP_OK) {
        track->duration = smp_media_duration(media);
        smp_media_close(media);
    }

    playlist->count++;
    return true;
}

static int compare_tracks(const void *left, const void *right)
{
    const SmpTrack *a = (const SmpTrack *)left;
    const SmpTrack *b = (const SmpTrack *)right;

    return smp_strnatcmp(smp_path_basename(a->path), smp_path_basename(b->path));
}

void smp_playlist_sort(Playlist *playlist)
{
    if (playlist == NULL || playlist->count < 2u) {
        return;
    }
    qsort(playlist->tracks, playlist->count, sizeof(SmpTrack), compare_tracks);
}

SmpStatus smp_playlist_load(Playlist *playlist, const char *directory)
{
    SmpDir *dir;
    SmpDirEntry entry;
    SmpStatus status = SMP_OK;

    if (playlist == NULL || directory == NULL) {
        return SMP_ERR_INVALID_ARG;
    }

    smp_playlist_free(playlist);

    if (!smp_path_is_dir(directory)) {
        return SMP_ERR_NOT_FOUND;
    }

    dir = smp_dir_open(directory);
    if (dir == NULL) {
        return SMP_ERR_IO;
    }

    while (smp_dir_next(dir, &entry)) {
        char *full;

        if (entry.is_dir || !smp_media_supported(entry.name)) {
            continue;
        }

        full = smp_path_join(directory, entry.name);
        if (full == NULL) {
            status = SMP_ERR_OUT_OF_MEMORY;
            break;
        }

        if (!append_track(playlist, full)) {
            free(full);
            status = SMP_ERR_OUT_OF_MEMORY;
            break;
        }
        free(full);
    }

    smp_dir_close(dir);

    if (status != SMP_OK) {
        smp_playlist_free(playlist);
        return status;
    }

    smp_playlist_sort(playlist);
    playlist->current = 0;
    return SMP_OK;
}

const SmpTrack *smp_playlist_at(const Playlist *playlist, size_t index)
{
    if (playlist == NULL || index >= playlist->count) {
        return NULL;
    }
    return &playlist->tracks[index];
}

const SmpTrack *smp_playlist_current(const Playlist *playlist)
{
    if (playlist == NULL || playlist->count == 0) {
        return NULL;
    }
    return smp_playlist_at(playlist, playlist->current);
}

bool smp_playlist_advance(Playlist *playlist)
{
    if (playlist == NULL || playlist->count == 0) {
        return false;
    }
    if (playlist->current + 1u >= playlist->count) {
        return false;
    }
    playlist->current++;
    return true;
}

bool smp_playlist_rewind(Playlist *playlist)
{
    if (playlist == NULL || playlist->count == 0) {
        return false;
    }
    if (playlist->current == 0) {
        return false;
    }
    playlist->current--;
    return true;
}

void smp_playlist_select(Playlist *playlist, size_t index)
{
    if (playlist == NULL || playlist->count == 0) {
        return;
    }
    playlist->current = index < playlist->count ? index : playlist->count - 1u;
}

void smp_playlist_move(Playlist *playlist, int delta)
{
    long long target;

    if (playlist == NULL || playlist->count == 0) {
        return;
    }

    target = (long long)playlist->current + delta;
    while (target < 0) {
        target += (long long)playlist->count;
    }
    target %= (long long)playlist->count;
    playlist->current = (size_t)target;
}

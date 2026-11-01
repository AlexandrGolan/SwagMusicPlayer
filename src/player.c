/*
 * SMP - Swag Music Player
 * Playback engine: state machine and a single worker thread.
 *
 * The worker owns the decoder and the audio device. All shared state is
 * guarded by `mutex`; the UI thread only ever observes snapshots, so there are
 * no data races and no lock is held while blocking on the audio device.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/media.h"
#include "smp/pcm.h"
#include "smp/str.h"
#include "smp/thread.h"

#define PLAYER_READ_FRAMES 4096u
#define PLAYER_DEFAULT_VOLUME 70

struct SmpPlayer {
    const Playlist *playlist;
    SmpAudioDevice *device;

    SmpMutex *mutex;
    SmpCond *cond;
    SmpThread *thread;

    SmpPlayerState state;
    size_t index;
    int volume;

    bool quit;
    bool finished;

    uint64_t generation;

    /* Seek requests are expressed in seconds so they work before a decoder
     * exists (for example when the user pauses and seeks immediately). */
    uint64_t seek_serial;
    uint64_t seek_applied;
    double seek_seconds;

    uint64_t position_frames;
    double duration;
    int sample_rate;
    int channels;

    SmpSpectrum *spectrum;
    int16_t *mono;
    size_t mono_capacity;
};

static void copy_text(char *destination, size_t size, const char *source)
{
    if (size == 0) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    snprintf(destination, size, "%s", source);
}

static uint64_t seconds_to_frames(double seconds, int sample_rate)
{
    if (seconds <= 0.0 || sample_rate <= 0) {
        return 0;
    }
    return (uint64_t)(seconds * (double)sample_rate);
}

static void lock(SmpPlayer *player)
{
    smp_mutex_lock(player->mutex);
}

static void unlock(SmpPlayer *player)
{
    smp_mutex_unlock(player->mutex);
}

/* Prepares a fresh playback of the current track. Must be called with the lock held. */
static void start_track_locked(SmpPlayer *player)
{
    player->generation++;
    player->seek_seconds = 0.0;
    player->seek_serial++;
    player->seek_applied = player->seek_serial;
    player->position_frames = 0;
    player->finished = false;
    player->state = SMP_PLAYER_PLAYING;
    player->duration = player->index < player->playlist->count
                           ? player->playlist->tracks[player->index].duration
                           : 0.0;
    smp_cond_broadcast(player->cond);
}

static void worker_fail(SmpPlayer *player, uint64_t generation)
{
    lock(player);
    if (player->generation == generation) {
        player->state = SMP_PLAYER_ERROR;
        player->finished = true;
    }
    unlock(player);
}

static int worker(void *argument)
{
    SmpPlayer *player = (SmpPlayer *)argument;

    for (;;) {
        size_t index;
        uint64_t generation;
        const SmpTrack *track;
        SmpMedia *media = NULL;
        PcmConverter *converter = NULL;
        int16_t *input = NULL;
        int media_rate;
        bool ended_naturally = false;
        uint64_t start_frame = 0;

        lock(player);
        while (!player->quit && player->state != SMP_PLAYER_PLAYING) {
            smp_cond_wait(player->cond, player->mutex);
        }
        if (player->quit) {
            unlock(player);
            break;
        }

        index = player->index;
        generation = player->generation;
        unlock(player);

        track = smp_playlist_at(player->playlist, index);
        if (track == NULL || smp_media_open(track->path, &media) != SMP_OK) {
            worker_fail(player, generation);
            continue;
        }

        converter = pcm_converter_create(smp_media_sample_rate(media), smp_media_channels(media));
        input = (int16_t *)malloc(PLAYER_READ_FRAMES * (size_t)smp_media_channels(media)
                                  * sizeof(int16_t));
        if (converter == NULL || input == NULL) {
            free(input);
            pcm_converter_destroy(converter);
            smp_media_close(media);
            worker_fail(player, generation);
            continue;
        }

        media_rate = smp_media_sample_rate(media);

        lock(player);
        player->sample_rate = media_rate;
        player->channels = smp_media_channels(media);
        player->duration = track->duration > 0.0 ? track->duration : smp_media_duration(media);
        start_frame = seconds_to_frames(player->seek_seconds, media_rate);
        player->seek_applied = player->seek_serial;
        player->position_frames = start_frame;
        unlock(player);

        if (start_frame > 0) {
            if (smp_media_seek(media, start_frame)) {
                pcm_converter_seek(converter, start_frame);
            }
        }

        /* The new track owns the device now; drop the mute from the switch. */
        smp_audio_set_muted(player->device, false);

        for (;;) {
            size_t frames_read;
            int16_t *converted;
            size_t converted_frames = 0;
            int volume;

            lock(player);

            if (player->quit) {
                unlock(player);
                break;
            }
            if (player->generation != generation) {
                unlock(player);
                break;
            }

            if (player->seek_applied != player->seek_serial) {
                uint64_t frame;

                player->seek_applied = player->seek_serial;
                frame = seconds_to_frames(player->seek_seconds, media_rate);
                unlock(player);

                if (smp_media_seek(media, frame)) {
                    pcm_converter_seek(converter, frame);
                }

                lock(player);
                if (player->generation == generation) {
                    player->position_frames = frame;
                }
                unlock(player);
                continue;
            }

            if (player->state != SMP_PLAYER_PLAYING) {
                /* Paused: sleep until the state changes again. */
                smp_cond_wait(player->cond, player->mutex);
                unlock(player);
                continue;
            }

            volume = player->volume;
            unlock(player);

            frames_read = smp_media_read(media, input, PLAYER_READ_FRAMES);
            if (frames_read == 0) {
                ended_naturally = true;
                break;
            }

            converted = pcm_converter_process(converter, input, frames_read, &converted_frames);
            if (converted == NULL || converted_frames == 0) {
                continue;
            }

            pcm_apply_gain(converted, converted_frames * SMP_OUTPUT_CHANNELS, volume);

            if (player->spectrum != NULL && player->mono != NULL) {
                size_t offset = 0;

                while (offset < converted_frames) {
                    size_t chunk = SMP_MIN(converted_frames - offset, player->mono_capacity);
                    size_t sample;

                    for (sample = 0; sample < chunk; sample++) {
                        int32_t left = converted[(offset + sample) * 2];
                        int32_t right = converted[(offset + sample) * 2 + 1];

                        player->mono[sample] = (int16_t)((left + right) / 2);
                    }

                    smp_spectrum_feed(player->spectrum, player->mono, chunk);
                    offset += chunk;
                }
            }

            if (smp_audio_write(player->device, converted, converted_frames) != SMP_OK) {
                lock(player);
                if (player->generation == generation) {
                    player->state = SMP_PLAYER_ERROR;
                    player->finished = true;
                }
                unlock(player);
                break;
            }

            lock(player);
            if (player->generation == generation) {
                player->position_frames = smp_media_position(media);
            }
            unlock(player);
        }

        free(input);
        pcm_converter_destroy(converter);
        smp_media_close(media);

        lock(player);
        if (ended_naturally && player->generation == generation) {
            player->state = SMP_PLAYER_STOPPED;
            player->finished = true;
            player->position_frames = seconds_to_frames(player->duration, media_rate);
        }
        unlock(player);
    }

    return 0;
}

SmpPlayer *smp_player_create(const Playlist *playlist, SmpAudioDevice *device)
{
    SmpPlayer *player;

    if (playlist == NULL || device == NULL) {
        return NULL;
    }

    player = (SmpPlayer *)calloc(1, sizeof(*player));
    if (player == NULL) {
        return NULL;
    }

    player->playlist = playlist;
    player->device = device;
    player->state = SMP_PLAYER_STOPPED;
    player->volume = PLAYER_DEFAULT_VOLUME;
    player->index = 0;

    player->mutex = smp_mutex_create();
    player->cond = smp_cond_create();
    if (player->mutex == NULL || player->cond == NULL) {
        smp_mutex_destroy(player->mutex);
        smp_cond_destroy(player->cond);
        free(player);
        return NULL;
    }

    player->mono_capacity = 32768u;
    player->mono = (int16_t *)malloc(player->mono_capacity * sizeof(int16_t));
    if (player->mono == NULL) {
        smp_mutex_destroy(player->mutex);
        smp_cond_destroy(player->cond);
        free(player);
        return NULL;
    }

    player->thread = smp_thread_start(worker, player);
    if (player->thread == NULL) {
        free(player->mono);
        smp_mutex_destroy(player->mutex);
        smp_cond_destroy(player->cond);
        free(player);
        return NULL;
    }

    return player;
}

void smp_player_destroy(SmpPlayer *player)
{
    if (player == NULL) {
        return;
    }

    lock(player);
    player->quit = true;
    player->state = SMP_PLAYER_STOPPED;
    smp_cond_broadcast(player->cond);
    unlock(player);

    smp_audio_set_muted(player->device, true);

    smp_thread_join(player->thread);

    smp_cond_destroy(player->cond);
    smp_mutex_destroy(player->mutex);
    free(player->mono);
    free(player);
}

void smp_player_play(SmpPlayer *player, size_t index)
{
    if (player == NULL) {
        return;
    }

    /* Silence the old track at once instead of playing out its buffer. */
    smp_audio_set_muted(player->device, true);

    lock(player);
    if (index >= player->playlist->count) {
        unlock(player);
        return;
    }
    player->index = index;
    start_track_locked(player);
    unlock(player);
}

void smp_player_toggle(SmpPlayer *player)
{
    bool paused = false;

    if (player == NULL) {
        return;
    }

    lock(player);

    if (player->state == SMP_PLAYER_PLAYING) {
        player->state = SMP_PLAYER_PAUSED;
    } else if (player->state == SMP_PLAYER_PAUSED) {
        player->state = SMP_PLAYER_PLAYING;
        smp_cond_broadcast(player->cond);
    } else if (player->playlist->count > 0) {
        start_track_locked(player);
    }

    paused = player->state == SMP_PLAYER_PAUSED;
    unlock(player);

    /*
     * Pausing mutes the device immediately: the queued frames are still
     * consumed by the callback but never heard, so there is no audible
     * tail-off when the key is pressed.
     */
    smp_audio_set_muted(player->device, paused);
}

void smp_player_stop(SmpPlayer *player)
{
    if (player == NULL) {
        return;
    }

    lock(player);
    player->generation++;
    player->state = SMP_PLAYER_STOPPED;
    player->position_frames = 0;
    player->seek_seconds = 0.0;
    player->seek_serial++;
    player->seek_applied = player->seek_serial;
    player->finished = false;
    smp_cond_broadcast(player->cond);
    unlock(player);

    smp_audio_set_muted(player->device, true);
}

void smp_player_next(SmpPlayer *player)
{
    size_t index;

    if (player == NULL) {
        return;
    }

    lock(player);
    index = player->index + 1u;
    unlock(player);

    if (index < player->playlist->count) {
        smp_player_play(player, index);
    }
}

void smp_player_previous(SmpPlayer *player)
{
    size_t index;
    bool valid;

    if (player == NULL) {
        return;
    }

    lock(player);
    valid = player->index > 0;
    index = valid ? player->index - 1u : 0;
    unlock(player);

    if (valid) {
        smp_player_play(player, index);
    }
}

void smp_player_seek(SmpPlayer *player, double seconds)
{
    double duration;

    if (player == NULL) {
        return;
    }

    lock(player);

    duration = player->duration;
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    if (duration > 0.0 && seconds > duration) {
        seconds = duration;
    }

    player->seek_seconds = seconds;
    player->seek_serial++;
    if (player->sample_rate > 0) {
        player->position_frames = seconds_to_frames(seconds, player->sample_rate);
    }
    smp_cond_broadcast(player->cond);
    unlock(player);
}

static double current_position(SmpPlayer *player)
{
    double position;

    lock(player);
    if (player->sample_rate > 0) {
        position = (double)player->position_frames / (double)player->sample_rate;
    } else {
        position = player->seek_seconds;
    }
    unlock(player);
    return position;
}

void smp_player_seek_by(SmpPlayer *player, double delta)
{
    double target;

    if (player == NULL) {
        return;
    }

    target = current_position(player) + delta;
    if (target < 0.0) {
        target = 0.0;
    }
    smp_player_seek(player, target);
}

void smp_player_set_spectrum(SmpPlayer *player, SmpSpectrum *spectrum)
{
    if (player == NULL) {
        return;
    }

    lock(player);
    player->spectrum = spectrum;
    unlock(player);
}

void smp_player_set_volume(SmpPlayer *player, int volume)
{
    if (player == NULL) {
        return;
    }

    lock(player);
    player->volume = pcm_clamp_volume(volume);
    unlock(player);
}

int smp_player_volume(SmpPlayer *player)
{
    int volume;

    if (player == NULL) {
        return 0;
    }

    lock(player);
    volume = player->volume;
    unlock(player);
    return volume;
}

void smp_player_snapshot(SmpPlayer *player, SmpPlayerSnapshot *snapshot)
{
    const SmpTrack *track;

    if (player == NULL || snapshot == NULL) {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));

    lock(player);

    snapshot->state = player->state;
    snapshot->track_index = player->index;
    snapshot->track_count = player->playlist->count;
    snapshot->duration = player->duration;
    snapshot->volume = player->volume;

    if (player->sample_rate > 0) {
        snapshot->position = (double)player->position_frames / (double)player->sample_rate;
    } else {
        snapshot->position = player->seek_seconds;
    }

    track = smp_playlist_at(player->playlist, player->index);
    if (track != NULL) {
        copy_text(snapshot->title, sizeof(snapshot->title), track->title);
        copy_text(snapshot->artist, sizeof(snapshot->artist), track->artist);
        copy_text(snapshot->album, sizeof(snapshot->album), track->album);
        copy_text(snapshot->format, sizeof(snapshot->format), track->format);
    }

    unlock(player);
}

bool smp_player_take_finished(SmpPlayer *player)
{
    bool finished;

    if (player == NULL) {
        return false;
    }

    lock(player);
    finished = player->finished;
    player->finished = false;
    unlock(player);
    return finished;
}

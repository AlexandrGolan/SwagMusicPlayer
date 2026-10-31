/*
 * SMP - Swag Music Player
 * Tests for the playback engine (with the silent backend).
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_wav.h"
#include "smp/audio.h"
#include "smp/common.h"
#include "smp/fs.h"
#include "smp/playlist.h"
#include "smp/player.h"
#include "test.h"

static bool write_wav(const char *path, int sample_rate, double seconds)
{
    drwav wav;
    drwav_data_format format;
    int frames = (int)(seconds * (double)sample_rate);
    int16_t *samples;
    int index;
    bool ok;

    memset(&format, 0, sizeof(format));
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = 2;
    format.sampleRate = (drwav_uint32)sample_rate;
    format.bitsPerSample = 16;

    if (drwav_init_file_write(&wav, path, &format, NULL) != DRWAV_TRUE) {
        return false;
    }

    samples = (int16_t *)malloc((size_t)frames * 2u * sizeof(int16_t));
    if (samples == NULL) {
        drwav_uninit(&wav);
        return false;
    }

    for (index = 0; index < frames * 2; index++) {
        samples[index] = (int16_t)((index % 100) * 10);
    }

    ok = drwav_write_pcm_frames(&wav, (drwav_uint64)frames, samples) == (drwav_uint64)frames;
    free(samples);
    drwav_uninit(&wav);
    return ok;
}

static void wait_for(bool (*condition)(const SmpPlayerSnapshot *), SmpPlayer *player, int timeout_ms)
{
    uint64_t deadline = smp_now_ms() + (uint64_t)timeout_ms;

    while (smp_now_ms() < deadline) {
        SmpPlayerSnapshot snapshot;

        smp_player_snapshot(player, &snapshot);
        if (condition(&snapshot)) {
            return;
        }
        smp_sleep_ms(5);
    }
}

static bool state_is_playing(const SmpPlayerSnapshot *snapshot)
{
    return snapshot->state == SMP_PLAYER_PLAYING;
}

static bool duration_known(const SmpPlayerSnapshot *snapshot)
{
    return snapshot->duration > 0.0;
}


static void test_state_machine(void)
{
    const char *directory = "smp_test_player";
    Playlist playlist;
    SmpAudioDevice *device = NULL;
    SmpPlayer *player = NULL;
    SmpPlayerSnapshot snapshot;
    SmpAudioFormat format;

    smp_test_begin("player state machine");
    (void)smp_dir_remove(directory);
    SMP_CHECK_INT(smp_dir_create(directory), SMP_OK);
    SMP_CHECK(write_wav("smp_test_player/first.wav", 48000, 1.0));

    smp_playlist_init(&playlist);
    SMP_CHECK_INT(smp_playlist_load(&playlist, directory), SMP_OK);
    SMP_CHECK_INT(playlist.count, 1);

    format.sample_rate = 48000;
    format.channels = 2;
    SMP_CHECK_INT(smp_audio_open("null", &device, &format), SMP_OK);

    player = smp_player_create(&playlist, device);
    SMP_CHECK(player != NULL);

    if (player != NULL) {
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_INT(snapshot.state, SMP_PLAYER_STOPPED);
        SMP_CHECK_INT(snapshot.volume, 70);
        SMP_CHECK_INT(snapshot.track_count, 1);

        smp_player_play(player, 0);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_INT(snapshot.state, SMP_PLAYER_PLAYING);
        SMP_CHECK_INT(snapshot.track_index, 0);

        smp_player_toggle(player);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_INT(snapshot.state, SMP_PLAYER_PAUSED);

        smp_player_toggle(player);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_INT(snapshot.state, SMP_PLAYER_PLAYING);

        smp_player_set_volume(player, 40);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_INT(snapshot.volume, 40);

        smp_player_set_volume(player, 500);
        SMP_CHECK_INT(smp_player_volume(player), 100);
        smp_player_set_volume(player, -10);
        SMP_CHECK_INT(smp_player_volume(player), 0);

        smp_player_stop(player);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_INT(snapshot.state, SMP_PLAYER_STOPPED);

        smp_player_destroy(player);
        player = NULL;
    }

    smp_audio_close(device);

    (void)smp_file_remove("smp_test_player/first.wav");
    (void)smp_dir_remove(directory);
    smp_playlist_free(&playlist);
    smp_test_end();
}

static void test_plays_to_the_end(void)
{
    const char *directory = "smp_test_player_end";
    Playlist playlist;
    SmpAudioDevice *device = NULL;
    SmpPlayer *player = NULL;
    SmpAudioFormat format;
    bool finished = false;
    int attempts;

    smp_test_begin("player reaches the end of a track");
    (void)smp_dir_remove(directory);
    SMP_CHECK_INT(smp_dir_create(directory), SMP_OK);
    SMP_CHECK(write_wav("smp_test_player_end/short.wav", 48000, 0.25));

    smp_playlist_init(&playlist);
    SMP_CHECK_INT(smp_playlist_load(&playlist, directory), SMP_OK);

    format.sample_rate = 48000;
    format.channels = 2;
    SMP_CHECK_INT(smp_audio_open("null", &device, &format), SMP_OK);

    player = smp_player_create(&playlist, device);
    SMP_CHECK(player != NULL);

    if (player != NULL) {
        smp_player_play(player, 0);

        for (attempts = 0; attempts < 400 && !finished; attempts++) {
            smp_sleep_ms(5);
            finished = smp_player_take_finished(player);
        }
        SMP_CHECK(finished);

        {
            SmpPlayerSnapshot snapshot;

            smp_player_snapshot(player, &snapshot);
            SMP_CHECK_INT(snapshot.state, SMP_PLAYER_STOPPED);
            SMP_CHECK_NEAR(snapshot.duration, 0.25, 0.01);
            SMP_CHECK_NEAR(snapshot.position, 0.25, 0.02);
        }

        smp_player_destroy(player);
    }

    smp_audio_close(device);
    (void)smp_file_remove("smp_test_player_end/short.wav");
    (void)smp_dir_remove(directory);
    smp_playlist_free(&playlist);
    smp_test_end();
}

static void test_navigation_and_seek(void)
{
    const char *directory = "smp_test_player_nav";
    Playlist playlist;
    SmpAudioDevice *device = NULL;
    SmpPlayer *player = NULL;
    SmpPlayerSnapshot snapshot;
    SmpAudioFormat format;

    smp_test_begin("player navigation and seeking");
    (void)smp_dir_remove(directory);
    SMP_CHECK_INT(smp_dir_create(directory), SMP_OK);
    SMP_CHECK(write_wav("smp_test_player_nav/a.wav", 48000, 2.0));
    SMP_CHECK(write_wav("smp_test_player_nav/b.wav", 48000, 2.0));

    smp_playlist_init(&playlist);
    SMP_CHECK_INT(smp_playlist_load(&playlist, directory), SMP_OK);
    SMP_CHECK_INT(playlist.count, 2);

    format.sample_rate = 48000;
    format.channels = 2;
    SMP_CHECK_INT(smp_audio_open("null", &device, &format), SMP_OK);

    player = smp_player_create(&playlist, device);
    SMP_CHECK(player != NULL);

    if (player != NULL) {
        smp_player_play(player, 0);
        wait_for(duration_known, player, 1000);

        smp_player_next(player);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_INT(snapshot.track_index, 1);

        smp_player_previous(player);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_INT(snapshot.track_index, 0);

        wait_for(state_is_playing, player, 1000);

        /* Pause first: the silent backend is faster than real time. */
        smp_player_toggle(player);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_INT(snapshot.state, SMP_PLAYER_PAUSED);

        smp_player_seek(player, 1.0);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_NEAR(snapshot.position, 1.0, 0.05);

        smp_player_seek(player, 0.25);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_NEAR(snapshot.position, 0.25, 0.05);

        smp_player_seek_by(player, 0.5);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK_NEAR(snapshot.position, 0.75, 0.10);

        smp_player_seek(player, 0.0);
        smp_player_snapshot(player, &snapshot);
        SMP_CHECK(snapshot.position < 0.05);

        smp_player_destroy(player);
    }

    smp_audio_close(device);
    (void)smp_file_remove("smp_test_player_nav/a.wav");
    (void)smp_file_remove("smp_test_player_nav/b.wav");
    (void)smp_dir_remove(directory);
    smp_playlist_free(&playlist);
    smp_test_end();
}

void suite_player(void)
{
    smp_test_suite("player");
    test_state_machine();
    test_plays_to_the_end();
    test_navigation_and_seek();
}

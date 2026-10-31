/*
 * SMP - Swag Music Player
 * Tests for the playlist model.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/fs.h"
#include "smp/playlist.h"
#include "test.h"

static void touch(const char *directory, const char *name)
{
    char *path = smp_path_join(directory, name);
    FILE *file;

    if (path == NULL) {
        return;
    }
    file = fopen(path, "wb");
    if (file != NULL) {
        fclose(file);
    }
    free(path);
}

static void test_load_and_sort(void)
{
    const char *dir_path = "smp_test_playlist";
    Playlist playlist;

    smp_test_begin("playlist load and natural sort");
    (void)smp_dir_remove(dir_path);
    SMP_CHECK_INT(smp_dir_create(dir_path), SMP_OK);

    touch(dir_path, "track10.mp3");
    touch(dir_path, "track2.mp3");
    touch(dir_path, "notes.txt");
    touch(dir_path, "song.wav");

    smp_playlist_init(&playlist);
    SMP_CHECK_INT(smp_playlist_load(&playlist, dir_path), SMP_OK);
    SMP_CHECK_INT(playlist.count, 3);

    if (playlist.count == 3) {
        SMP_CHECK_STR(smp_path_basename(smp_playlist_at(&playlist, 0)->path), "song.wav");
        SMP_CHECK_STR(smp_path_basename(smp_playlist_at(&playlist, 1)->path), "track2.mp3");
        SMP_CHECK_STR(smp_path_basename(smp_playlist_at(&playlist, 2)->path), "track10.mp3");
        SMP_CHECK_STR(smp_playlist_at(&playlist, 1)->title, "track2");
        SMP_CHECK_STR(smp_playlist_at(&playlist, 1)->artist, "Unknown artist");
        SMP_CHECK_NEAR(smp_playlist_at(&playlist, 1)->duration, 0.0, 0.0001);
    }

    SMP_CHECK_INT(playlist.current, 0);
    SMP_CHECK(smp_playlist_current(&playlist) != NULL);
    SMP_CHECK(smp_playlist_at(&playlist, 99) == NULL);

    smp_playlist_free(&playlist);
    SMP_CHECK_INT(playlist.count, 0);
    SMP_CHECK(playlist.tracks == NULL);

    (void)smp_file_remove("smp_test_playlist/track10.mp3");
    (void)smp_file_remove("smp_test_playlist/track2.mp3");
    (void)smp_file_remove("smp_test_playlist/notes.txt");
    (void)smp_file_remove("smp_test_playlist/song.wav");
    (void)smp_dir_remove(dir_path);
    smp_test_end();
}

static void test_navigation(void)
{
    Playlist playlist;

    smp_test_begin("playlist navigation and wrap");
    smp_playlist_init(&playlist);

    SMP_CHECK(!smp_playlist_advance(&playlist));
    SMP_CHECK(!smp_playlist_rewind(&playlist));

    playlist.count = 3;
    playlist.current = 0;

    SMP_CHECK(smp_playlist_advance(&playlist));
    SMP_CHECK_INT(playlist.current, 1);
    SMP_CHECK(smp_playlist_advance(&playlist));
    SMP_CHECK_INT(playlist.current, 2);
    SMP_CHECK(!smp_playlist_advance(&playlist));
    SMP_CHECK_INT(playlist.current, 2);

    SMP_CHECK(smp_playlist_rewind(&playlist));
    SMP_CHECK_INT(playlist.current, 1);

    smp_playlist_move(&playlist, 5);
    SMP_CHECK_INT(playlist.current, 0);
    smp_playlist_move(&playlist, -1);
    SMP_CHECK_INT(playlist.current, 2);

    smp_playlist_select(&playlist, 10);
    SMP_CHECK_INT(playlist.current, 2);
    smp_playlist_select(&playlist, 1);
    SMP_CHECK_INT(playlist.current, 1);

    smp_test_end();
}

static void test_missing_directory(void)
{
    Playlist playlist;

    smp_test_begin("playlist handles a missing directory");
    smp_playlist_init(&playlist);
    SMP_CHECK_INT(smp_playlist_load(&playlist, "smp_test_no_such_dir"), SMP_ERR_NOT_FOUND);
    SMP_CHECK_INT(playlist.count, 0);
    smp_playlist_free(&playlist);
    smp_test_end();
}

static void test_empty_directory(void)
{
    const char *dir_path = "smp_test_empty_dir";
    Playlist playlist;

    smp_test_begin("playlist handles an empty directory");
    (void)smp_dir_remove(dir_path);
    SMP_CHECK_INT(smp_dir_create(dir_path), SMP_OK);

    smp_playlist_init(&playlist);
    SMP_CHECK_INT(smp_playlist_load(&playlist, dir_path), SMP_OK);
    SMP_CHECK_INT(playlist.count, 0);

    smp_playlist_free(&playlist);
    (void)smp_dir_remove(dir_path);
    smp_test_end();
}

void suite_playlist(void)
{
    smp_test_suite("playlist");
    test_load_and_sort();
    test_navigation();
    test_missing_directory();
    test_empty_directory();
}

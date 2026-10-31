/*
 * SMP - Swag Music Player
 * Tests for path handling and file system helpers.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/fs.h"
#include "test.h"

static void test_path_join(void)
{
    char *joined;

    smp_test_begin("path join handles separators");
    joined = smp_path_join("/music", "track.mp3");
    SMP_CHECK_STR(joined, "/music/track.mp3");
    free(joined);

    joined = smp_path_join("/music/", "track.mp3");
    SMP_CHECK_STR(joined, "/music/track.mp3");
    free(joined);

    joined = smp_path_join("", "track.mp3");
    SMP_CHECK_STR(joined, "track.mp3");
    free(joined);

    joined = smp_path_join("C:\\music", "track.mp3");
    SMP_CHECK_STR(joined, "C:\\music/track.mp3");
    free(joined);

    smp_test_end();
}

static void test_basename_extension(void)
{
    smp_test_begin("basename and extension extraction");
    SMP_CHECK_STR(smp_path_basename("/a/b/c.mp3"), "c.mp3");
    SMP_CHECK_STR(smp_path_basename("c.mp3"), "c.mp3");
    SMP_CHECK_STR(smp_path_basename("C:\\a\\b.mp3"), "b.mp3");
    SMP_CHECK_STR(smp_path_basename("/a/"), "");

    SMP_CHECK_STR(smp_path_extension("/a/b/c.mp3"), ".mp3");
    SMP_CHECK(smp_path_extension("noext") == NULL);
    SMP_CHECK(smp_path_extension(".hidden") == NULL);
    SMP_CHECK_STR(smp_path_extension("archive.tar.gz"), ".gz");

    SMP_CHECK(smp_path_has_extension("A.MP3", ".mp3"));
    SMP_CHECK(smp_path_has_extension("a.wav", ".WAV"));
    SMP_CHECK(!smp_path_has_extension("a.flac", ".mp3"));
    SMP_CHECK(!smp_path_has_extension("a", ".mp3"));
    smp_test_end();
}

static void test_file_read(void)
{
    const char *path = "smp_test_read.txt";
    const char *content = "0123456789abcdef";
    void *data = NULL;
    size_t size = 0;
    FILE *file;

    smp_test_begin("file read and tail");
    file = fopen(path, "wb");
    SMP_CHECK(file != NULL);
    if (file != NULL) {
        SMP_CHECK_INT(fwrite(content, 1, strlen(content), file), strlen(content));
        fclose(file);
    }

    SMP_CHECK_INT(smp_file_read(path, 64, &data, &size), SMP_OK);
    SMP_CHECK_INT(size, 16);
    SMP_CHECK_STR((const char *)data, content);
    free(data);

    SMP_CHECK_INT(smp_file_read(path, 4, &data, &size), SMP_OK);
    SMP_CHECK_INT(size, 4);
    SMP_CHECK_STR((const char *)data, "0123");
    free(data);

    SMP_CHECK_INT(smp_file_read_tail(path, 4, &data, &size), SMP_OK);
    SMP_CHECK_INT(size, 4);
    SMP_CHECK_STR((const char *)data, "cdef");
    free(data);

    {
        uint64_t file_size = 0;

        SMP_CHECK_INT(smp_file_size(path, &file_size), SMP_OK);
        SMP_CHECK_INT(file_size, 16);
    }

    SMP_CHECK_INT(smp_file_read("smp_test_missing.txt", 4, &data, &size), SMP_ERR_NOT_FOUND);

    (void)smp_file_remove(path);
    SMP_CHECK(!smp_path_exists(path));
    smp_test_end();
}

static void test_directory_iteration(void)
{
    const char *dir_path = "smp_test_dir";
    static const char *files[] = {"one.txt", "two.mp3", "three.WAV"};
    SmpDir *dir;
    SmpDirEntry entry;
    int found = 0;
    int index;
    size_t file_index;

    smp_test_begin("directory iteration lists regular files");
    (void)smp_dir_remove(dir_path);
    SMP_CHECK_INT(smp_dir_create(dir_path), SMP_OK);
    SMP_CHECK(smp_path_is_dir(dir_path));

    for (file_index = 0; file_index < SMP_ARRAY_LEN(files); file_index++) {
        char *full = smp_path_join(dir_path, files[file_index]);
        FILE *file = fopen(full, "wb");

        SMP_CHECK(file != NULL);
        if (file != NULL) {
            fputs("x", file);
            fclose(file);
        }
        free(full);
    }

    dir = smp_dir_open(dir_path);
    SMP_CHECK(dir != NULL);
    if (dir != NULL) {
        while (smp_dir_next(dir, &entry)) {
            SMP_CHECK(!entry.is_dir);
            SMP_CHECK_INT(entry.size, 1);
            for (index = 0; index < (int)SMP_ARRAY_LEN(files); index++) {
                if (strcmp(entry.name, files[index]) == 0) {
                    found++;
                }
            }
        }
        smp_dir_close(dir);
    }
    SMP_CHECK_INT(found, 3);

    for (file_index = 0; file_index < SMP_ARRAY_LEN(files); file_index++) {
        char *full = smp_path_join(dir_path, files[file_index]);

        SMP_CHECK_INT(smp_file_remove(full), SMP_OK);
        free(full);
    }
    SMP_CHECK_INT(smp_dir_remove(dir_path), SMP_OK);
    SMP_CHECK(!smp_path_exists(dir_path));
    smp_test_end();
}

static void test_nested_directory(void)
{
    const char *nested = "smp_test_nested/a/b";

    smp_test_begin("nested directory creation");
    (void)smp_dir_remove("smp_test_nested/a/b");
    (void)smp_dir_remove("smp_test_nested/a");
    (void)smp_dir_remove("smp_test_nested");

    SMP_CHECK_INT(smp_dir_create(nested), SMP_OK);
    SMP_CHECK(smp_path_is_dir(nested));

    (void)smp_dir_remove(nested);
    (void)smp_dir_remove("smp_test_nested/a");
    (void)smp_dir_remove("smp_test_nested");
    smp_test_end();
}

void suite_fs(void)
{
    smp_test_suite("fs");
    test_path_join();
    test_basename_extension();
    test_file_read();
    test_directory_iteration();
    test_nested_directory();
}

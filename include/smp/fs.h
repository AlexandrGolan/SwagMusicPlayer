/*
 * SMP - Swag Music Player
 * Portable file system helpers.
 *
 * All paths are UTF-8 encoded. On Windows the implementation converts them to
 * UTF-16 internally so that non-ASCII paths work correctly.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_FS_H
#define SMP_FS_H

#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "smp/common.h"

#define SMP_PATH_MAX 4096

typedef struct {
    char name[SMP_PATH_MAX]; /* file name, UTF-8, NUL terminated */
    uint64_t size;           /* size in bytes (0 for directories) */
    bool is_dir;
} SmpDirEntry;

typedef struct SmpDir SmpDir;

/* Opens a directory for iteration. Returns NULL on failure. */
SmpDir *smp_dir_open(const char *path);

/*
 * Reads the next entry. Returns false when the directory is exhausted.
 * "." and ".." are skipped.
 */
bool smp_dir_next(SmpDir *dir, SmpDirEntry *entry);

void smp_dir_close(SmpDir *dir);

bool smp_path_exists(const char *path);
bool smp_path_is_dir(const char *path);

/* Joins `dir` and `name` with a single separator. Caller frees the result. */
char *smp_path_join(const char *dir, const char *name);

/* Pointer into `path` after the last separator (never NULL). */
const char *smp_path_basename(const char *path);

/* Pointer to the last '.' of the base name, or NULL. Case preserving. */
const char *smp_path_extension(const char *path);

/* Case-insensitive extension test; `ext` must include the dot (".mp3"). */
bool smp_path_has_extension(const char *path, const char *ext);

/*
 * Reads at most `max_bytes` from the beginning of the file.
 * The returned buffer is NUL terminated and must be freed with free().
 */
SmpStatus smp_file_read(const char *path, size_t max_bytes, void **out_data, size_t *out_size);

/* Reads at most `max_bytes` from the end of the file (NUL terminated as well). */
SmpStatus smp_file_read_tail(const char *path, size_t max_bytes, void **out_data, size_t *out_size);

/* Size of a regular file in bytes, or SMP_ERR_NOT_FOUND. */
SmpStatus smp_file_size(const char *path, uint64_t *out_size);

/*
 * Opens a file for binary reading with a UTF-8 path (UTF-16 on Windows).
 * Returns NULL on failure; close with fclose().
 */
FILE *smp_file_open(const char *path);

/* Recursively creates a directory including missing parents. */
SmpStatus smp_dir_create(const char *path);

/* Removes a file. */
SmpStatus smp_file_remove(const char *path);

/* Removes an empty directory. */
SmpStatus smp_dir_remove(const char *path);

#endif /* SMP_FS_H */

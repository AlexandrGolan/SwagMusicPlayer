/*
 * SMP - Swag Music Player
 * Portable file system helpers (POSIX + Win32).
 *
 * SPDX-License-Identifier: MIT
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "smp/fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/str.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

struct SmpDir {
    char *path;
#ifdef _WIN32
    HANDLE handle;
    WIN32_FIND_DATAW data;
    bool pending;
#else
    DIR *dir;
#endif
};

static bool is_separator(char c)
{
    return c == '/' || c == '\\';
}

char *smp_path_join(const char *dir, const char *name)
{
    size_t dir_len;
    size_t name_len;
    bool needs_separator;
    char *joined;

    if (dir == NULL || name == NULL) {
        return NULL;
    }
    if (name[0] != '\0' && is_separator(name[0])) {
        return smp_strdup(name);
    }

    dir_len = strlen(dir);
    name_len = strlen(name);
    needs_separator = dir_len > 0 && !is_separator(dir[dir_len - 1]);

    joined = (char *)malloc(dir_len + (needs_separator ? 1u : 0u) + name_len + 1u);
    if (joined == NULL) {
        return NULL;
    }

    memcpy(joined, dir, dir_len);
    if (needs_separator) {
        joined[dir_len] = '/';
        dir_len++;
    }
    memcpy(joined + dir_len, name, name_len);
    joined[dir_len + name_len] = '\0';
    return joined;
}

const char *smp_path_basename(const char *path)
{
    const char *base = path;
    const char *cursor;

    if (path == NULL) {
        return "";
    }

    for (cursor = path; *cursor != '\0'; cursor++) {
        if (is_separator(*cursor)) {
            base = cursor + 1;
        }
    }
    return base;
}

const char *smp_path_extension(const char *path)
{
    const char *base = smp_path_basename(path);
    const char *dot = strrchr(base, '.');

    if (dot == NULL || dot == base) {
        return NULL;
    }
    return dot;
}

bool smp_path_has_extension(const char *path, const char *extension)
{
    const char *actual = smp_path_extension(path);
    size_t index;

    if (actual == NULL || extension == NULL) {
        return false;
    }
    if (strlen(actual) != strlen(extension)) {
        return false;
    }

    for (index = 0; actual[index] != '\0'; index++) {
        char left = actual[index];
        char right = extension[index];

        if (left >= 'A' && left <= 'Z') {
            left = (char)(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = (char)(right - 'A' + 'a');
        }
        if (left != right) {
            return false;
        }
    }
    return true;
}

bool smp_path_exists(const char *path)
{
    if (path == NULL) {
        return false;
    }

#ifdef _WIN32
    {
        wchar_t *wide = smp_utf8_to_wide(path);
        DWORD attributes;

        if (wide == NULL) {
            return false;
        }
        attributes = GetFileAttributesW(wide);
        free(wide);
        return attributes != INVALID_FILE_ATTRIBUTES;
    }
#else
    {
        struct stat info;

        return stat(path, &info) == 0;
    }
#endif
}

bool smp_path_is_dir(const char *path)
{
    if (path == NULL) {
        return false;
    }

#ifdef _WIN32
    {
        wchar_t *wide = smp_utf8_to_wide(path);
        DWORD attributes;

        if (wide == NULL) {
            return false;
        }
        attributes = GetFileAttributesW(wide);
        free(wide);
        return attributes != INVALID_FILE_ATTRIBUTES
               && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
#else
    {
        struct stat info;

        return stat(path, &info) == 0 && S_ISDIR(info.st_mode) != 0;
    }
#endif
}

SmpStatus smp_file_size(const char *path, uint64_t *out_size)
{
    if (path == NULL || out_size == NULL) {
        return SMP_ERR_INVALID_ARG;
    }

#ifdef _WIN32
    {
        wchar_t *wide = smp_utf8_to_wide(path);
        WIN32_FILE_ATTRIBUTE_DATA data;

        if (wide == NULL) {
            return SMP_ERR_INVALID_ARG;
        }
        if (!GetFileAttributesExW(wide, GetFileExInfoStandard, &data)) {
            free(wide);
            return SMP_ERR_NOT_FOUND;
        }
        free(wide);
        *out_size = ((uint64_t)data.nFileSizeHigh << 32) | (uint64_t)data.nFileSizeLow;
        return SMP_OK;
    }
#else
    {
        struct stat info;

        if (stat(path, &info) != 0) {
            return SMP_ERR_NOT_FOUND;
        }
        *out_size = (uint64_t)info.st_size;
        return SMP_OK;
    }
#endif
}

SmpDir *smp_dir_open(const char *path)
{
    SmpDir *dir;

    if (path == NULL) {
        return NULL;
    }

    dir = (SmpDir *)calloc(1, sizeof(*dir));
    if (dir == NULL) {
        return NULL;
    }

    dir->path = smp_strdup(path);
    if (dir->path == NULL) {
        free(dir);
        return NULL;
    }

#ifdef _WIN32
    {
        wchar_t *wide = smp_utf8_to_wide(path);
        wchar_t *pattern;
        size_t length;

        if (wide == NULL) {
            free(dir->path);
            free(dir);
            return NULL;
        }

        length = wcslen(wide);
        pattern = (wchar_t *)malloc((length + 3u) * sizeof(wchar_t));
        if (pattern == NULL) {
            free(wide);
            free(dir->path);
            free(dir);
            return NULL;
        }
        wcscpy(pattern, wide);
        if (length > 0 && pattern[length - 1] != L'/' && pattern[length - 1] != L'\\') {
            pattern[length] = L'\\';
            pattern[length + 1] = L'\0';
        }
        wcscat(pattern, L"*");

        dir->handle = FindFirstFileW(pattern, &dir->data);
        free(pattern);
        free(wide);

        if (dir->handle == INVALID_HANDLE_VALUE) {
            free(dir->path);
            free(dir);
            return NULL;
        }
        dir->pending = true;
    }
#else
    dir->dir = opendir(path);
    if (dir->dir == NULL) {
        free(dir->path);
        free(dir);
        return NULL;
    }
#endif

    return dir;
}

bool smp_dir_next(SmpDir *dir, SmpDirEntry *entry)
{
    if (dir == NULL || entry == NULL) {
        return false;
    }

#ifdef _WIN32
    for (;;) {
        char *name;

        if (!dir->pending && !FindNextFileW(dir->handle, &dir->data)) {
            return false;
        }
        dir->pending = false;

        name = smp_wide_to_utf8(dir->data.cFileName);
        if (name == NULL) {
            continue;
        }
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            free(name);
            continue;
        }

        snprintf(entry->name, sizeof(entry->name), "%s", name);
        entry->is_dir = (dir->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        entry->size = ((uint64_t)dir->data.nFileSizeHigh << 32) | (uint64_t)dir->data.nFileSizeLow;
        free(name);
        return true;
    }
#else
    for (;;) {
        struct dirent *item = readdir(dir->dir);
        char *full;
        int length;

        if (item == NULL) {
            return false;
        }
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) {
            continue;
        }

        length = snprintf(entry->name, sizeof(entry->name), "%s", item->d_name);
        if (length < 0) {
            continue;
        }

        entry->is_dir = false;
        entry->size = 0;
        full = smp_path_join(dir->path, item->d_name);
        if (full != NULL) {
            struct stat info;

            if (stat(full, &info) == 0) {
                entry->is_dir = S_ISDIR(info.st_mode) != 0;
                entry->size = (uint64_t)info.st_size;
            }
            free(full);
        }
        return true;
    }
#endif
}

void smp_dir_close(SmpDir *dir)
{
    if (dir == NULL) {
        return;
    }

#ifdef _WIN32
    if (dir->handle != INVALID_HANDLE_VALUE) {
        FindClose(dir->handle);
    }
#else
    if (dir->dir != NULL) {
        closedir(dir->dir);
    }
#endif
    free(dir->path);
    free(dir);
}

static FILE *open_utf8(const char *path, const char *mode)
{
#ifdef _WIN32
    wchar_t *wide_path = smp_utf8_to_wide(path);
    wchar_t *wide_mode = smp_utf8_to_wide(mode);
    FILE *file = NULL;

    if (wide_path != NULL && wide_mode != NULL) {
        file = _wfopen(wide_path, wide_mode);
    }
    free(wide_path);
    free(wide_mode);
    return file;
#else
    return fopen(path, mode);
#endif
}

/* 64-bit file positioning (long is 32-bit on Windows). */
static bool file_seek(FILE *file, int64_t offset, int origin)
{
#ifdef _WIN32
    return _fseeki64(file, (__int64)offset, origin) == 0;
#else
    return fseeko(file, (off_t)offset, origin) == 0;
#endif
}

static int64_t file_tell(FILE *file)
{
#ifdef _WIN32
    return (int64_t)_ftelli64(file);
#else
    return (int64_t)ftello(file);
#endif
}

static SmpStatus read_stream(FILE *file, size_t max_bytes, void **out_data, size_t *out_size){
    char *buffer;
    size_t capacity;
    size_t used = 0;

    capacity = max_bytes > 0 && max_bytes < 65536u ? max_bytes : 65536u;
    buffer = (char *)malloc(capacity + 1u);
    if (buffer == NULL) {
        return SMP_ERR_OUT_OF_MEMORY;
    }

    for (;;) {
        size_t request;
        size_t got;

        if (used == capacity) {
            size_t grown_capacity;

            if (max_bytes > 0 && capacity >= max_bytes) {
                break;
            }

            grown_capacity = capacity * 2u;
            if (max_bytes > 0 && grown_capacity > max_bytes) {
                grown_capacity = max_bytes;
            }

            {
                char *grown = (char *)realloc(buffer, grown_capacity + 1u);

                if (grown == NULL) {
                    free(buffer);
                    return SMP_ERR_OUT_OF_MEMORY;
                }
                buffer = grown;
                capacity = grown_capacity;
            }
        }

        request = capacity - used;
        got = fread(buffer + used, 1, request, file);
        used += got;
        if (got < request) {
            break;
        }
    }

    buffer[used] = '\0';
    *out_data = buffer;
    *out_size = used;
    return SMP_OK;
}

FILE *smp_file_open(const char *path)
{
    if (path == NULL) {
        return NULL;
    }
    return open_utf8(path, "rb");
}

SmpStatus smp_file_read(const char *path, size_t max_bytes, void **out_data, size_t *out_size)
{
    FILE *file;
    SmpStatus status;

    if (path == NULL || out_data == NULL || out_size == NULL) {
        return SMP_ERR_INVALID_ARG;
    }

    file = open_utf8(path, "rb");
    if (file == NULL) {
        return SMP_ERR_NOT_FOUND;
    }

    status = read_stream(file, max_bytes, out_data, out_size);
    fclose(file);
    return status;
}

SmpStatus smp_file_read_tail(const char *path, size_t max_bytes, void **out_data, size_t *out_size)
{
    FILE *file;
    int64_t size;
    size_t wanted;
    SmpStatus status;

    if (path == NULL || out_data == NULL || out_size == NULL) {
        return SMP_ERR_INVALID_ARG;
    }

    file = open_utf8(path, "rb");
    if (file == NULL) {
        return SMP_ERR_NOT_FOUND;
    }

    if (!file_seek(file, 0, SEEK_END)) {
        fclose(file);
        return SMP_ERR_IO;
    }
    size = file_tell(file);
    if (size < 0) {
        fclose(file);
        return SMP_ERR_IO;
    }

    wanted = (size_t)size;
    if (max_bytes > 0 && wanted > max_bytes) {
        wanted = max_bytes;
    }
    if (!file_seek(file, size - (int64_t)wanted, SEEK_SET)) {
        fclose(file);
        return SMP_ERR_IO;
    }

    status = read_stream(file, wanted, out_data, out_size);
    fclose(file);
    return status;
}

static int make_single_dir(const char *path)
{
#ifdef _WIN32
    wchar_t *wide = smp_utf8_to_wide(path);
    int result;

    if (wide == NULL) {
        return -1;
    }
    result = _wmkdir(wide);
    free(wide);
    return result;
#else
    return mkdir(path, 0777);
#endif
}

SmpStatus smp_dir_create(const char *path)
{
    char *copy;
    char *cursor;
    size_t length;

    if (path == NULL || path[0] == '\0') {
        return SMP_ERR_INVALID_ARG;
    }
    if (smp_path_is_dir(path)) {
        return SMP_OK;
    }

    copy = smp_strdup(path);
    if (copy == NULL) {
        return SMP_ERR_OUT_OF_MEMORY;
    }

    length = strlen(copy);
    while (length > 0 && is_separator(copy[length - 1])) {
        copy[--length] = '\0';
    }

    for (cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (!is_separator(*cursor)) {
            continue;
        }

        *cursor = '\0';
        if (!smp_path_is_dir(copy)) {
            (void)make_single_dir(copy);
        }
        *cursor = '/';
    }

    free(copy);

    if (smp_path_is_dir(path)) {
        return SMP_OK;
    }
    return make_single_dir(path) == 0 ? SMP_OK : SMP_ERR_IO;
}

SmpStatus smp_file_remove(const char *path)
{
    if (path == NULL) {
        return SMP_ERR_INVALID_ARG;
    }

#ifdef _WIN32
    {
        wchar_t *wide = smp_utf8_to_wide(path);
        int result;

        if (wide == NULL) {
            return SMP_ERR_INVALID_ARG;
        }
        result = _wremove(wide);
        free(wide);
        return result == 0 ? SMP_OK : SMP_ERR_IO;
    }
#else
    return remove(path) == 0 ? SMP_OK : SMP_ERR_IO;
#endif
}

SmpStatus smp_dir_remove(const char *path)
{
    if (path == NULL) {
        return SMP_ERR_INVALID_ARG;
    }

#ifdef _WIN32
    {
        wchar_t *wide = smp_utf8_to_wide(path);
        int result;

        if (wide == NULL) {
            return SMP_ERR_INVALID_ARG;
        }
        result = _wrmdir(wide);
        free(wide);
        return result == 0 ? SMP_OK : SMP_ERR_IO;
    }
#else
    return rmdir(path) == 0 ? SMP_OK : SMP_ERR_IO;
#endif
}

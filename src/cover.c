/*
 * SMP - Swag Music Player
 * Cover art discovery and image scaling.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/cover.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/str.h"
#include "stb_image.h"

#define COVER_FILE_LIMIT (32u * 1024u * 1024u)

static const char *const cover_names[] = {
    "cover", "Cover", "COVER", "album", "Album", "ALBUM", "folder", "Folder", "FOLDER",
    "front", "Front", "FRONT", "art",    "Art",    "ART",
};

static const char *const cover_extensions[] = {
    ".png", ".jpg", ".jpeg", ".bmp", ".gif",
};

void smp_image_init(SmpImage *image)
{
    if (image == NULL) {
        return;
    }
    image->width = 0;
    image->height = 0;
    image->pixels = NULL;
}

void smp_image_free(SmpImage *image)
{
    if (image == NULL) {
        return;
    }
    free(image->pixels);
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
}

SmpStatus smp_image_load(const char *path, SmpImage *out_image)
{
    void *data = NULL;
    size_t size = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *decoded;
    size_t total;

    if (path == NULL || out_image == NULL) {
        return SMP_ERR_INVALID_ARG;
    }
    smp_image_init(out_image);

    if (smp_file_read(path, COVER_FILE_LIMIT, &data, &size) != SMP_OK || size == 0) {
        free(data);
        return SMP_ERR_IO;
    }

    decoded = stbi_load_from_memory((const unsigned char *)data, (int)size, &width, &height, &channels, 4);
    free(data);

    if (decoded == NULL || width <= 0 || height <= 0) {
        stbi_image_free(decoded);
        return SMP_ERR_DECODE;
    }

    total = (size_t)width * (size_t)height * 4u;
    out_image->pixels = (uint8_t *)malloc(total);
    if (out_image->pixels == NULL) {
        stbi_image_free(decoded);
        return SMP_ERR_OUT_OF_MEMORY;
    }

    memcpy(out_image->pixels, decoded, total);
    out_image->width = width;
    out_image->height = height;
    stbi_image_free(decoded);
    return SMP_OK;
}

SmpStatus smp_image_resize_box(const SmpImage *source, SmpImage *out_image, int width, int height)
{
    uint8_t *pixels;
    int x;
    int y;

    if (source == NULL || out_image == NULL || source->pixels == NULL) {
        return SMP_ERR_INVALID_ARG;
    }
    if (width <= 0 || height <= 0 || width > source->width || height > source->height) {
        return SMP_ERR_INVALID_ARG;
    }

    smp_image_init(out_image);

    pixels = (uint8_t *)malloc((size_t)width * (size_t)height * 4u);
    if (pixels == NULL) {
        return SMP_ERR_OUT_OF_MEMORY;
    }

    for (y = 0; y < height; y++) {
        int y0 = (int)((int64_t)y * source->height / height);
        int y1 = (int)((int64_t)(y + 1) * source->height / height);

        if (y1 <= y0) {
            y1 = y0 + 1;
        }

        for (x = 0; x < width; x++) {
            int x0 = (int)((int64_t)x * source->width / width);
            int x1 = (int)((int64_t)(x + 1) * source->width / width);
            uint32_t sum[4] = {0, 0, 0, 0};
            uint32_t count = 0;
            int sx;
            int sy;
            int channel;

            if (x1 <= x0) {
                x1 = x0 + 1;
            }

            for (sy = y0; sy < y1; sy++) {
                for (sx = x0; sx < x1; sx++) {
                    const uint8_t *pixel = source->pixels
                                           + ((size_t)sy * (size_t)source->width + (size_t)sx) * 4u;

                    for (channel = 0; channel < 4; channel++) {
                        sum[channel] += pixel[channel];
                    }
                    count++;
                }
            }

            if (count == 0) {
                count = 1;
            }

            {
                uint8_t *target = pixels + ((size_t)y * (size_t)width + (size_t)x) * 4u;

                for (channel = 0; channel < 4; channel++) {
                    target[channel] = (uint8_t)(sum[channel] / count);
                }
            }
        }
    }

    out_image->width = width;
    out_image->height = height;
    out_image->pixels = pixels;
    return SMP_OK;
}

SmpStatus smp_image_fill_box(const SmpImage *source, SmpImage *out_image, int width, int height)
{
    uint8_t *pixels;
    double target_ratio;
    double source_ratio;
    int crop_w;
    int crop_h;
    int offset_x;
    int offset_y;
    int x;
    int y;

    if (source == NULL || out_image == NULL || source->pixels == NULL) {
        return SMP_ERR_INVALID_ARG;
    }
    if (width <= 0 || height <= 0) {
        return SMP_ERR_INVALID_ARG;
    }

    smp_image_init(out_image);

    target_ratio = (double)width / (double)height;
    source_ratio = (double)source->width / (double)source->height;

    crop_w = source->width;
    crop_h = source->height;
    if (source_ratio > target_ratio) {
        crop_w = (int)((double)crop_h * target_ratio + 0.5);
    } else {
        crop_h = (int)((double)crop_w / target_ratio + 0.5);
    }
    if (crop_w > source->width) {
        crop_w = source->width;
    }
    if (crop_h > source->height) {
        crop_h = source->height;
    }
    if (crop_w <= 0 || crop_h <= 0) {
        return SMP_ERR_INVALID_ARG;
    }

    offset_x = (source->width - crop_w) / 2;
    offset_y = (source->height - crop_h) / 2;

    pixels = (uint8_t *)malloc((size_t)width * (size_t)height * 4u);
    if (pixels == NULL) {
        return SMP_ERR_OUT_OF_MEMORY;
    }

    for (y = 0; y < height; y++) {
        int sy0 = offset_y + (int)((double)y * (double)crop_h / (double)height);
        int sy1 = offset_y + (int)((double)(y + 1) * (double)crop_h / (double)height);

        if (sy1 <= sy0) {
            sy1 = sy0 + 1;
        }
        if (sy1 > source->height) {
            sy1 = source->height;
        }

        for (x = 0; x < width; x++) {
            int sx0 = offset_x + (int)((double)x * (double)crop_w / (double)width);
            int sx1 = offset_x + (int)((double)(x + 1) * (double)crop_w / (double)width);
            uint32_t sum[4] = {0, 0, 0, 0};
            uint32_t count = 0;
            int sample_x;
            int sample_y;
            int channel;

            if (sx1 <= sx0) {
                sx1 = sx0 + 1;
            }
            if (sx1 > source->width) {
                sx1 = source->width;
            }

            for (sample_y = sy0; sample_y < sy1; sample_y++) {
                for (sample_x = sx0; sample_x < sx1; sample_x++) {
                    const uint8_t *pixel
                        = source->pixels
                          + ((size_t)sample_y * (size_t)source->width + (size_t)sample_x) * 4u;

                    for (channel = 0; channel < 4; channel++) {
                        sum[channel] += pixel[channel];
                    }
                    count++;
                }
            }

            if (count == 0) {
                count = 1;
            }

            {
                uint8_t *target = pixels + ((size_t)y * (size_t)width + (size_t)x) * 4u;

                for (channel = 0; channel < 4; channel++) {
                    target[channel] = (uint8_t)(sum[channel] / count);
                }
            }
        }
    }

    out_image->width = width;
    out_image->height = height;
    out_image->pixels = pixels;
    return SMP_OK;
}

const char *smp_ascii_ramp(void)
{
    return " .:-=+*#%@";
}

bool smp_image_to_ascii(const SmpImage *image, int columns, int rows, char *out, size_t stride)
{
    const char *ramp = smp_ascii_ramp();
    int ramp_length = (int)strlen(ramp);
    int row;
    int column;

    if (image == NULL || image->pixels == NULL || out == NULL) {
        return false;
    }
    if (columns <= 0 || rows <= 0 || columns > image->width || rows > image->height) {
        return false;
    }
    if (stride < (size_t)columns + 1u) {
        return false;
    }

    for (row = 0; row < rows; row++) {
        int y0 = row * image->height / rows;
        int y1 = (row + 1) * image->height / rows;

        if (y1 <= y0) {
            y1 = y0 + 1;
        }

        for (column = 0; column < columns; column++) {
            int x0 = column * image->width / columns;
            int x1 = (column + 1) * image->width / columns;
            uint32_t red = 0;
            uint32_t green = 0;
            uint32_t blue = 0;
            uint32_t samples = 0;
            int y;
            int x;

            if (x1 <= x0) {
                x1 = x0 + 1;
            }

            for (y = y0; y < y1; y++) {
                const uint8_t *line = image->pixels + (size_t)y * (size_t)image->width * 4u;

                for (x = x0; x < x1; x++) {
                    const uint8_t *pixel = line + (size_t)x * 4u;

                    red += pixel[0];
                    green += pixel[1];
                    blue += pixel[2];
                    samples++;
                }
            }

            if (samples == 0) {
                samples = 1;
            }

            {
                /* Rec. 709 luminance, 0..255. */
                uint32_t luminance
                    = (2126u * red / samples + 7152u * green / samples + 722u * blue / samples)
                      / 10000u;
                int level = (int)(luminance * (uint32_t)(ramp_length - 1) / 255u);

                if (level < 0) {
                    level = 0;
                }
                if (level >= ramp_length) {
                    level = ramp_length - 1;
                }
                out[(size_t)row * stride + (size_t)column] = ramp[level];
            }
        }

        out[(size_t)row * stride + (size_t)columns] = '\0';
    }

    return true;
}

static bool contains_case_insensitive(const char *text, const char *needle)
{
    size_t needle_length = strlen(needle);
    size_t offset;

    if (needle_length == 0) {
        return true;
    }

    for (offset = 0; text[offset] != '\0'; offset++) {
        size_t index;

        for (index = 0; index < needle_length; index++) {
            char left = text[offset + index];
            char right = needle[index];

            if (left == '\0') {
                return false;
            }
            if (left >= 'A' && left <= 'Z') {
                left = (char)(left - 'A' + 'a');
            }
            if (right >= 'A' && right <= 'Z') {
                right = (char)(right - 'A' + 'a');
            }
            if (left != right) {
                break;
            }
        }

        if (index == needle_length) {
            return true;
        }
    }
    return false;
}

static bool is_cover_extension(const char *path)
{
    size_t index;

    for (index = 0; index < SMP_ARRAY_LEN(cover_extensions); index++) {
        if (smp_path_has_extension(path, cover_extensions[index])) {
            return true;
        }
    }
    return false;
}

SmpStatus smp_cover_find(const char *directory, char *out_path, size_t out_size)
{
    size_t name_index;
    size_t extension_index;
    SmpDir *dir;
    SmpDirEntry entry;

    if (directory == NULL || out_path == NULL || out_size == 0) {
        return SMP_ERR_INVALID_ARG;
    }
    out_path[0] = '\0';

    /* Preferred file names first. */
    for (name_index = 0; name_index < SMP_ARRAY_LEN(cover_names); name_index++) {
        for (extension_index = 0; extension_index < SMP_ARRAY_LEN(cover_extensions);
             extension_index++)
        {
            char file_name[128];
            char *full;

            snprintf(file_name,
                     sizeof(file_name),
                     "%s%s",
                     cover_names[name_index],
                     cover_extensions[extension_index]);
            full = smp_path_join(directory, file_name);
            if (full == NULL) {
                continue;
            }

            if (smp_path_exists(full)) {
                snprintf(out_path, out_size, "%s", full);
                free(full);
                return SMP_OK;
            }
            free(full);
        }
    }

    /* Then any image whose name hints at a cover. */
    dir = smp_dir_open(directory);
    if (dir == NULL) {
        return SMP_ERR_NOT_FOUND;
    }

    while (smp_dir_next(dir, &entry)) {
        char *full;

        if (entry.is_dir || !is_cover_extension(entry.name)) {
            continue;
        }
        if (!contains_case_insensitive(entry.name, "cover")
            && !contains_case_insensitive(entry.name, "album")
            && !contains_case_insensitive(entry.name, "folder")
            && !contains_case_insensitive(entry.name, "front"))
        {
            continue;
        }

        full = smp_path_join(directory, entry.name);
        if (full == NULL) {
            continue;
        }
        snprintf(out_path, out_size, "%s", full);
        free(full);
        smp_dir_close(dir);
        return SMP_OK;
    }

    smp_dir_close(dir);
    return SMP_ERR_NOT_FOUND;
}

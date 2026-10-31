/*
 * SMP - Swag Music Player
 * Cover art discovery and image scaling.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_COVER_H
#define SMP_COVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smp/common.h"
#include "smp/fs.h"

/* Decoded RGBA8 image. */
typedef struct {
    int width;
    int height;
    uint8_t *pixels; /* width * height * 4 bytes */
} SmpImage;

void smp_image_init(SmpImage *image);
void smp_image_free(SmpImage *image);

/* Loads a PNG/JPEG/BMP/GIF file. */
SmpStatus smp_image_load(const char *path, SmpImage *out_image);

/*
 * Scales `source` down to `width` x `height` with an area (box) filter.
 * Upscaling is not supported; it returns SMP_ERR_INVALID_ARG.
 */
SmpStatus smp_image_resize_box(const SmpImage *source, SmpImage *out_image, int width, int height);

/*
 * Finds the cover image of an album directory. The result is the full path
 * inside `out_path`. Returns SMP_ERR_NOT_FOUND when there is none.
 */
SmpStatus smp_cover_find(const char *directory, char *out_path, size_t out_size);

#endif /* SMP_COVER_H */

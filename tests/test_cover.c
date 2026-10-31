/*
 * SMP - Swag Music Player
 * Tests for cover discovery and image scaling.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smp/cover.h"
#include "smp/fs.h"
#include "test.h"

/* Writes a 2x2 24-bit BMP: red/green on the bottom row, blue/white on top. */
static void write_test_bmp(const char *path)
{
    unsigned char header[54];
    unsigned char pixels[16];
    FILE *file;

    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    header[2] = 70;  /* file size */
    header[10] = 54; /* pixel data offset */

    header[14] = 40; /* DIB header size */
    header[18] = 2;  /* width */
    header[22] = 2;  /* height */
    header[26] = 1;  /* planes */
    header[28] = 24; /* bits per pixel */
    header[34] = 16; /* image size */

    memset(pixels, 0, sizeof(pixels));
    /* Bottom row: red (BGR 0,0,255), green (0,255,0) + 2 padding bytes. */
    pixels[0] = 0;
    pixels[1] = 0;
    pixels[2] = 255;
    pixels[3] = 0;
    pixels[4] = 255;
    pixels[5] = 0;
    /* Top row: blue (255,0,0), white (255,255,255). */
    pixels[8] = 255;
    pixels[9] = 0;
    pixels[10] = 0;
    pixels[11] = 255;
    pixels[12] = 255;
    pixels[13] = 255;

    file = fopen(path, "wb");
    if (file == NULL) {
        return;
    }
    fwrite(header, 1, sizeof(header), file);
    fwrite(pixels, 1, sizeof(pixels), file);
    fclose(file);
}

static void test_image_load(void)
{
    const char *path = "smp_test_cover.bmp";
    SmpImage image;

    smp_test_begin("bmp cover decoding");
    write_test_bmp(path);

    SMP_CHECK_INT(smp_image_load(path, &image), SMP_OK);
    SMP_CHECK_INT(image.width, 2);
    SMP_CHECK_INT(image.height, 2);

    if (image.pixels != NULL) {
        /* BMP rows are stored bottom-up, so the first decoded row is the top. */
        SMP_CHECK_INT(image.pixels[0], 0);   /* top left blue   R */
        SMP_CHECK_INT(image.pixels[1], 0);   /* G */
        SMP_CHECK_INT(image.pixels[2], 255); /* B */
        SMP_CHECK_INT(image.pixels[3], 255); /* A */

        /* Top right is white. */
        SMP_CHECK_INT(image.pixels[(0 * 2 + 1) * 4 + 0], 255);
        SMP_CHECK_INT(image.pixels[(0 * 2 + 1) * 4 + 1], 255);
        SMP_CHECK_INT(image.pixels[(0 * 2 + 1) * 4 + 2], 255);

        /* Bottom left is red, bottom right is green. */
        SMP_CHECK_INT(image.pixels[(1 * 2 + 0) * 4 + 0], 255);
        SMP_CHECK_INT(image.pixels[(1 * 2 + 0) * 4 + 2], 0);
        SMP_CHECK_INT(image.pixels[(1 * 2 + 1) * 4 + 1], 255);
    }

    smp_image_free(&image);
    SMP_CHECK(image.pixels == NULL);

    SMP_CHECK_INT(smp_image_load("smp_test_missing.png", &image), SMP_ERR_IO);

    (void)smp_file_remove(path);
    smp_test_end();
}

static void test_image_scaling(void)
{
    uint8_t pixels[4 * 4 * 4];
    SmpImage source;
    SmpImage scaled;
    int index;

    smp_test_begin("box scaling averages pixels");
    for (index = 0; index < 4 * 4 * 4; index += 4) {
        pixels[index] = 100;
        pixels[index + 1] = 150;
        pixels[index + 2] = 200;
        pixels[index + 3] = 255;
    }

    smp_image_init(&source);
    source.width = 4;
    source.height = 4;
    source.pixels = pixels;

    SMP_CHECK_INT(smp_image_resize_box(&source, &scaled, 2, 2), SMP_OK);
    SMP_CHECK_INT(scaled.width, 2);
    SMP_CHECK_INT(scaled.height, 2);
    if (scaled.pixels != NULL) {
        SMP_CHECK_INT(scaled.pixels[0], 100);
        SMP_CHECK_INT(scaled.pixels[1], 150);
        SMP_CHECK_INT(scaled.pixels[2], 200);
        SMP_CHECK_INT(scaled.pixels[3], 255);
    }
    smp_image_free(&scaled);

    SMP_CHECK_INT(smp_image_resize_box(&source, &scaled, 8, 8), SMP_ERR_INVALID_ARG);
    SMP_CHECK_INT(smp_image_resize_box(&source, &scaled, 0, 1), SMP_ERR_INVALID_ARG);
    smp_test_end();
}

static void test_cover_discovery(void)
{
    const char *dir_path = "smp_test_covers";
    char found[SMP_PATH_MAX];

    smp_test_begin("cover discovery prefers cover.*");
    (void)smp_dir_remove(dir_path);
    SMP_CHECK_INT(smp_dir_create(dir_path), SMP_OK);

    SMP_CHECK_INT(smp_cover_find(dir_path, found, sizeof(found)), SMP_ERR_NOT_FOUND);

    {
        char *path = smp_path_join(dir_path, "album.jpg");
        FILE *file = fopen(path, "wb");

        if (file != NULL) {
            fputs("x", file);
            fclose(file);
        }
        free(path);
    }

    SMP_CHECK_INT(smp_cover_find(dir_path, found, sizeof(found)), SMP_OK);
    SMP_CHECK_STR(smp_path_basename(found), "album.jpg");

    {
        char *path = smp_path_join(dir_path, "cover.png");
        FILE *file = fopen(path, "wb");

        if (file != NULL) {
            fputs("x", file);
            fclose(file);
        }
        free(path);
    }

    SMP_CHECK_INT(smp_cover_find(dir_path, found, sizeof(found)), SMP_OK);
    SMP_CHECK_STR(smp_path_basename(found), "cover.png");

    (void)smp_file_remove("smp_test_covers/album.jpg");
    (void)smp_file_remove("smp_test_covers/cover.png");
    (void)smp_dir_remove(dir_path);
    smp_test_end();
}

void suite_cover(void)
{
    smp_test_suite("cover");
    test_image_load();
    test_image_scaling();
    test_cover_discovery();
}

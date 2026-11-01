/*
 * SMP - Swag Music Player
 * Third party single-header libraries (implementation units).
 *
 * dr_wav / dr_mp3: public domain (https://github.com/mackron/dr_libs)
 * stb_image:       MIT / public domain (https://github.com/nothings/stb)
 *
 * SPDX-License-Identifier: MIT
 */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

/* stb_vorbis is a single .c file that carries its own declarations. */
#include "stb_vorbis.c"

/* stb_vorbis leaks an internal `L` macro that collides with stb_image. */
#ifdef L
#undef L
#endif

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* Keeps the translation unit from being empty in strict configurations. */
int smp_vendor_impl_anchor(void);
int smp_vendor_impl_anchor(void)
{
    return 1;
}

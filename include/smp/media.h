/*
 * SMP - Swag Music Player
 * Audio decoding (WAV via dr_wav, MP3 via dr_mp3).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_MEDIA_H
#define SMP_MEDIA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smp/common.h"

typedef enum {
    SMP_MEDIA_WAV = 0,
    SMP_MEDIA_MP3,
    SMP_MEDIA_FLAC,
    SMP_MEDIA_OGG,
} SmpMediaKind;

typedef struct SmpMedia SmpMedia;

/* True when the extension is one of the supported audio formats. */
bool smp_media_supported(const char *path);

/* Opens `path` for decoding. Returns SMP_ERR_UNSUPPORTED for unknown formats. */
SmpStatus smp_media_open(const char *path, SmpMedia **out_media);

void smp_media_close(SmpMedia *media);

SmpMediaKind smp_media_kind(const SmpMedia *media);
const char *smp_media_kind_name(SmpMediaKind kind);

int smp_media_sample_rate(const SmpMedia *media);
int smp_media_channels(const SmpMedia *media);

/* Track duration in seconds; 0 when it cannot be determined. */
double smp_media_duration(const SmpMedia *media);

/* Total number of source frames, or 0 when unknown. */
uint64_t smp_media_total_frames(const SmpMedia *media);

/* Current position in source frames. */
uint64_t smp_media_position(const SmpMedia *media);

/*
 * Reads up to `max_frames` interleaved S16 frames. Returns the number of
 * frames actually read (0 at the end of the stream).
 */
size_t smp_media_read(SmpMedia *media, int16_t *frames, size_t max_frames);

bool smp_media_seek(SmpMedia *media, uint64_t frame);

/*
 * Estimates MP3 duration in seconds from an MPEG frame header (Xing/Info/VBRI
 * or constant bitrate). Returns 0 when the data does not contain a frame.
 * `file_size` is used for the constant bitrate estimate.
 */
double smp_mp3_duration_from_header(const uint8_t *data, size_t size, uint64_t file_size);

#endif /* SMP_MEDIA_H */

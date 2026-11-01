/*
 * SMP - Swag Music Player
 * Embedded metadata: ID3v2, ID3v1 and RIFF INFO chunks.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_TAGS_H
#define SMP_TAGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smp/common.h"

#define SMP_TAG_TEXT_MAX 192

typedef struct {
    char title[SMP_TAG_TEXT_MAX];
    char artist[SMP_TAG_TEXT_MAX];
    char album[SMP_TAG_TEXT_MAX];
} SmpTags;

typedef enum {
    SMP_TAG_FORMAT_UNKNOWN = 0,
    SMP_TAG_FORMAT_ID3V2,
    SMP_TAG_FORMAT_ID3V1,
    SMP_TAG_FORMAT_RIFF_INFO,
    SMP_TAG_FORMAT_VORBIS_COMMENT,
} SmpTagFormat;

void smp_tags_init(SmpTags *tags);

/* Size of the ID3v2 tag that starts at `data`, or 0 when there is none. */
size_t smp_id3v2_size(const uint8_t *data, size_t size);

/* Parses ID3v2 frames; `data` must start with the "ID3" header. */
SmpTagFormat smp_tags_parse_id3v2(const uint8_t *data, size_t size, SmpTags *tags);

/* Parses a trailing 128-byte ID3v1 record. */
bool smp_tags_parse_id3v1(const uint8_t *data, size_t size, SmpTags *tags);

/* Parses the RIFF "LIST"/"INFO" chunks of a WAV file. */
SmpTagFormat smp_tags_parse_riff(const uint8_t *data, size_t size, SmpTags *tags);

/*
 * Reads `path` and fills `tags` from the best available source. When no title
 * is found, the file name (without extension) is used.
 */
SmpStatus smp_tags_read_file(const char *path, SmpTags *tags);

#endif /* SMP_TAGS_H */

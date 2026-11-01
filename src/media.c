/*
 * SMP - Swag Music Player
 * Audio decoding: WAV (dr_wav), MP3 (dr_mp3), FLAC (dr_flac),
 * Ogg Vorbis (stb_vorbis).
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/media.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_flac.h"
#include "dr_mp3.h"
#include "dr_wav.h"
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#include "smp/fs.h"
#include "smp/tags.h"

#define OGG_MAX_FILE_SIZE ((size_t)256 * 1024 * 1024)

struct SmpMedia {
    SmpMediaKind kind;

    int sample_rate;
    int channels;
    double duration;
    uint64_t total_frames;
    uint64_t position;

    union {
        drwav wav;
        drmp3 mp3;
        drflac *flac;
        stb_vorbis *ogg;
    } decoder;

    void *ogg_memory; /* stb_vorbis keeps pointers into this buffer */
};

bool smp_media_supported(const char *path)
{
    return smp_path_has_extension(path, ".wav") || smp_path_has_extension(path, ".mp3")
           || smp_path_has_extension(path, ".flac") || smp_path_has_extension(path, ".ogg")
           || smp_path_has_extension(path, ".oga");
}

const char *smp_media_kind_name(SmpMediaKind kind)
{
    switch (kind) {
    case SMP_MEDIA_MP3:
        return "MP3";
    case SMP_MEDIA_FLAC:
        return "FLAC";
    case SMP_MEDIA_OGG:
        return "OGG";
    case SMP_MEDIA_WAV:
    default:
        return "WAV";
    }
}

/* ------------------------------------------------------------------------- */
/* MPEG frame header parsing (MP3 duration estimation)                       */
/* ------------------------------------------------------------------------- */

typedef struct {
    int version; /* 1 = MPEG1, 2 = MPEG2, 25 = MPEG2.5 */
    int bitrate_kbps;
    int sample_rate;
    int padding;
    int channels;
    int frame_length;
    int samples_per_frame;
    int side_info_size;
} Mp3FrameHeader;

static const int mp3_bitrate_v1_layer3[15]
    = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
static const int mp3_bitrate_v2_layer3[15]
    = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
static const int mp3_sample_rates[3][3] = {
    {44100, 48000, 32000},
    {22050, 24000, 16000},
    {11025, 12000, 8000},
};

static bool mp3_parse_header(const uint8_t *data, size_t size, Mp3FrameHeader *header)
{
    uint8_t first;
    uint8_t second;
    uint8_t third;
    int version_bits;
    int layer_bits;
    int bitrate_index;
    int rate_index;
    int rate_table;
    int padding;

    if (data == NULL || header == NULL || size < 4u) {
        return false;
    }

    first = data[0];
    second = data[1];
    third = data[2];

    if (first != 0xffu || (second & 0xe0u) != 0xe0u) {
        return false;
    }

    version_bits = (second >> 3) & 0x03;
    layer_bits = (second >> 1) & 0x03;
    if (version_bits == 1 || layer_bits != 1) {
        return false;
    }

    bitrate_index = (int)(third >> 4);
    rate_index = (int)((third >> 2) & 0x03);
    padding = (third >> 1) & 0x01;

    if (bitrate_index == 0 || bitrate_index == 15 || rate_index == 3) {
        return false;
    }

    switch (version_bits) {
    case 3:
        header->version = 1;
        rate_table = 0;
        break;
    case 2:
        header->version = 2;
        rate_table = 1;
        break;
    default:
        header->version = 25;
        rate_table = 2;
        break;
    }

    header->bitrate_kbps = header->version == 1 ? mp3_bitrate_v1_layer3[bitrate_index]
                                                : mp3_bitrate_v2_layer3[bitrate_index];
    header->sample_rate = mp3_sample_rates[rate_table][rate_index];
    header->padding = padding;
    header->channels = ((data[3] >> 6) & 0x03) == 3 ? 1 : 2;
    header->samples_per_frame = header->version == 1 ? 1152 : 576;

    if (header->version == 1) {
        header->frame_length = (144 * header->bitrate_kbps * 1000) / header->sample_rate + padding;
        header->side_info_size = header->channels == 1 ? 17 : 32;
    } else {
        header->frame_length = (72 * header->bitrate_kbps * 1000) / header->sample_rate + padding;
        header->side_info_size = header->channels == 1 ? 9 : 17;
    }

    return header->frame_length > 4 && header->bitrate_kbps > 0;
}

static uint32_t read_be32_tag(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8)
           | (uint32_t)data[3];
}

double smp_mp3_duration_from_header(const uint8_t *data, size_t size, uint64_t file_size)
{
    size_t audio_start;
    size_t offset;
    Mp3FrameHeader header;
    size_t frame_offset = (size_t)-1;

    if (data == NULL || size < 4u) {
        return 0.0;
    }

    audio_start = smp_id3v2_size(data, size);

    for (offset = audio_start; offset + 4u < size; offset++) {
        if (mp3_parse_header(data + offset, size - offset, &header)) {
            frame_offset = offset;
            break;
        }
    }

    if (frame_offset == (size_t)-1) {
        return 0.0;
    }

    {
        size_t xing_offset = frame_offset + 4u + (size_t)header.side_info_size;

        if (xing_offset + 8u <= size
            && (memcmp(data + xing_offset, "Xing", 4) == 0
                || memcmp(data + xing_offset, "Info", 4) == 0))
        {
            uint32_t flags = read_be32_tag(data + xing_offset + 4);

            if ((flags & 0x0001u) != 0 && xing_offset + 12u <= size) {
                uint32_t frames = read_be32_tag(data + xing_offset + 8);

                if (frames > 0) {
                    return (double)frames * (double)header.samples_per_frame
                           / (double)header.sample_rate;
                }
            }
        }
    }

    {
        size_t vbri_offset = frame_offset + 4u + 32u;

        if (vbri_offset + 18u <= size && memcmp(data + vbri_offset, "VBRI", 4) == 0) {
            uint32_t frames = read_be32_tag(data + vbri_offset + 14);

            if (frames > 0) {
                return (double)frames * (double)header.samples_per_frame
                       / (double)header.sample_rate;
            }
        }
    }

    if (file_size > (uint64_t)frame_offset) {
        uint64_t audio_bytes = file_size - (uint64_t)frame_offset;
        double bitrate = (double)header.bitrate_kbps * 1000.0;

        if (bitrate > 0.0) {
            return (double)audio_bytes * 8.0 / bitrate;
        }
    }

    return 0.0;
}

/* ------------------------------------------------------------------------- */
/* Opening and decoding                                                      */
/* ------------------------------------------------------------------------- */

static bool open_wav(SmpMedia *media, const char *path)
{
#ifdef _WIN32
    wchar_t *wide = smp_utf8_to_wide(path);
    drwav_bool32 ok;

    if (wide == NULL) {
        return false;
    }
    ok = drwav_init_file_w(&media->decoder.wav, wide, NULL);
    free(wide);
    return ok == DRWAV_TRUE;
#else
    return drwav_init_file(&media->decoder.wav, path, NULL) == DRWAV_TRUE;
#endif
}

static bool open_mp3(SmpMedia *media, const char *path)
{
#ifdef _WIN32
    wchar_t *wide = smp_utf8_to_wide(path);
    drmp3_bool32 ok;

    if (wide == NULL) {
        return false;
    }
    ok = drmp3_init_file_w(&media->decoder.mp3, wide, NULL);
    free(wide);
    return ok == DRMP3_TRUE;
#else
    return drmp3_init_file(&media->decoder.mp3, path, NULL) == DRMP3_TRUE;
#endif
}

static bool open_flac(SmpMedia *media, const char *path)
{
#ifdef _WIN32
    wchar_t *wide = smp_utf8_to_wide(path);

    if (wide == NULL) {
        return false;
    }
    media->decoder.flac = drflac_open_file_w(wide, NULL);
    free(wide);
#else
    media->decoder.flac = drflac_open_file(path, NULL);
#endif
    return media->decoder.flac != NULL;
}

static bool open_ogg(SmpMedia *media, const char *path)
{
    void *data = NULL;
    size_t size = 0;
    int error = 0;

    if (smp_file_read(path, OGG_MAX_FILE_SIZE, &data, &size) != SMP_OK || size == 0) {
        free(data);
        return false;
    }
    if (size > (size_t)INT_MAX) {
        free(data);
        return false;
    }

    media->decoder.ogg = stb_vorbis_open_memory((const unsigned char *)data, (int)size, &error, NULL);
    if (media->decoder.ogg == NULL) {
        free(data);
        return false;
    }

    media->ogg_memory = data;
    return true;
}

SmpStatus smp_media_open(const char *path, SmpMedia **out_media)
{
    SmpMedia *media;

    if (path == NULL || out_media == NULL) {
        return SMP_ERR_INVALID_ARG;
    }
    *out_media = NULL;

    media = (SmpMedia *)calloc(1, sizeof(*media));
    if (media == NULL) {
        return SMP_ERR_OUT_OF_MEMORY;
    }

    if (smp_path_has_extension(path, ".wav")) {
        media->kind = SMP_MEDIA_WAV;
        if (!open_wav(media, path)) {
            free(media);
            return SMP_ERR_DECODE;
        }
        media->sample_rate = (int)media->decoder.wav.sampleRate;
        media->channels = (int)media->decoder.wav.channels;
        media->total_frames = media->decoder.wav.totalPCMFrameCount;
    } else if (smp_path_has_extension(path, ".mp3")) {
        void *buffer = NULL;
        size_t buffer_size = 0;
        uint64_t file_size = 0;

        media->kind = SMP_MEDIA_MP3;
        if (!open_mp3(media, path)) {
            free(media);
            return SMP_ERR_DECODE;
        }
        media->sample_rate = (int)media->decoder.mp3.sampleRate;
        media->channels = (int)media->decoder.mp3.channels;

        (void)smp_file_size(path, &file_size);
        if (smp_file_read(path, 256u * 1024u, &buffer, &buffer_size) == SMP_OK) {
            media->duration = smp_mp3_duration_from_header((const uint8_t *)buffer,
                                                           buffer_size,
                                                           file_size);
            free(buffer);
        }
    } else if (smp_path_has_extension(path, ".flac")) {
        media->kind = SMP_MEDIA_FLAC;
        if (!open_flac(media, path)) {
            free(media);
            return SMP_ERR_DECODE;
        }
        media->sample_rate = (int)media->decoder.flac->sampleRate;
        media->channels = (int)media->decoder.flac->channels;
        media->total_frames = media->decoder.flac->totalPCMFrameCount;
    } else if (smp_path_has_extension(path, ".ogg") || smp_path_has_extension(path, ".oga")) {
        stb_vorbis_info info;

        media->kind = SMP_MEDIA_OGG;
        if (!open_ogg(media, path)) {
            free(media);
            return SMP_ERR_DECODE;
        }
        info = stb_vorbis_get_info(media->decoder.ogg);
        media->sample_rate = (int)info.sample_rate;
        media->channels = (int)info.channels;
        media->duration = (double)stb_vorbis_stream_length_in_seconds(media->decoder.ogg);
        if (media->duration > 0.0) {
            media->total_frames = (uint64_t)(media->duration * (double)media->sample_rate);
        }
    } else {
        free(media);
        return SMP_ERR_UNSUPPORTED;
    }

    if (media->sample_rate <= 0 || media->channels <= 0) {
        smp_media_close(media);
        return SMP_ERR_DECODE;
    }

    if (media->duration <= 0.0 && media->total_frames > 0 && media->sample_rate > 0) {
        media->duration = (double)media->total_frames / (double)media->sample_rate;
    }

    *out_media = media;
    return SMP_OK;
}

void smp_media_close(SmpMedia *media)
{
    if (media == NULL) {
        return;
    }

    switch (media->kind) {
    case SMP_MEDIA_WAV:
        drwav_uninit(&media->decoder.wav);
        break;
    case SMP_MEDIA_MP3:
        drmp3_uninit(&media->decoder.mp3);
        break;
    case SMP_MEDIA_FLAC:
        if (media->decoder.flac != NULL) {
            drflac_close(media->decoder.flac);
        }
        break;
    case SMP_MEDIA_OGG:
        if (media->decoder.ogg != NULL) {
            stb_vorbis_close(media->decoder.ogg);
        }
        break;
    }

    free(media->ogg_memory);
    free(media);
}

SmpMediaKind smp_media_kind(const SmpMedia *media)
{
    return media != NULL ? media->kind : SMP_MEDIA_WAV;
}

int smp_media_sample_rate(const SmpMedia *media)
{
    return media != NULL ? media->sample_rate : 0;
}

int smp_media_channels(const SmpMedia *media)
{
    return media != NULL ? media->channels : 0;
}

double smp_media_duration(const SmpMedia *media)
{
    return media != NULL ? media->duration : 0.0;
}

uint64_t smp_media_total_frames(const SmpMedia *media)
{
    return media != NULL ? media->total_frames : 0;
}

uint64_t smp_media_position(const SmpMedia *media)
{
    return media != NULL ? media->position : 0;
}

size_t smp_media_read(SmpMedia *media, int16_t *frames, size_t max_frames)
{
    uint64_t read = 0;

    if (media == NULL || frames == NULL || max_frames == 0) {
        return 0;
    }

    switch (media->kind) {
    case SMP_MEDIA_WAV:
        read = drwav_read_pcm_frames_s16(&media->decoder.wav, (drwav_uint64)max_frames, frames);
        break;
    case SMP_MEDIA_MP3:
        read = drmp3_read_pcm_frames_s16(&media->decoder.mp3, (drmp3_uint64)max_frames, frames);
        break;
    case SMP_MEDIA_FLAC:
        read = drflac_read_pcm_frames_s16(media->decoder.flac, (drflac_uint64)max_frames, frames);
        break;
    case SMP_MEDIA_OGG:
        read = (uint64_t)stb_vorbis_get_frame_short_interleaved(media->decoder.ogg,
                                                                media->channels,
                                                                frames,
                                                                (int)(max_frames * (size_t)media->channels));
        break;
    }

    media->position += read;
    return (size_t)read;
}

bool smp_media_seek(SmpMedia *media, uint64_t frame)
{
    bool ok = false;

    if (media == NULL) {
        return false;
    }

    switch (media->kind) {
    case SMP_MEDIA_WAV:
        ok = drwav_seek_to_pcm_frame(&media->decoder.wav, (drwav_uint64)frame) == DRWAV_TRUE;
        break;
    case SMP_MEDIA_MP3:
        ok = drmp3_seek_to_pcm_frame(&media->decoder.mp3, (drmp3_uint64)frame) == DRMP3_TRUE;
        break;
    case SMP_MEDIA_FLAC:
        ok = drflac_seek_to_pcm_frame(media->decoder.flac, (drflac_uint64)frame) == DRFLAC_TRUE;
        break;
    case SMP_MEDIA_OGG:
        ok = stb_vorbis_seek(media->decoder.ogg, (unsigned int)frame) != 0;
        break;
    }

    if (ok) {
        media->position = frame;
    }
    return ok;
}

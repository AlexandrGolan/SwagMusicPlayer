/*
 * SMP - Swag Music Player
 * Tests for audio decoding and MP3 header parsing.
 *
 * SPDX-License-Identifier: MIT
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dr_wav.h"
#include "smp/fs.h"
#include "smp/media.h"
#include "test.h"

static bool write_test_wav(const char *path, int sample_rate, int channels, int frames)
{
    drwav wav;
    drwav_data_format format;
    int16_t *samples;
    int index;
    bool ok;

    memset(&format, 0, sizeof(format));
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = (drwav_uint32)channels;
    format.sampleRate = (drwav_uint32)sample_rate;
    format.bitsPerSample = 16;

    if (drwav_init_file_write(&wav, path, &format, NULL) != DRWAV_TRUE) {
        return false;
    }

    samples = (int16_t *)malloc((size_t)frames * (size_t)channels * sizeof(int16_t));
    if (samples == NULL) {
        drwav_uninit(&wav);
        return false;
    }

    for (index = 0; index < frames * channels; index++) {
        samples[index] = (int16_t)((index * 37) % 3000);
    }

    ok = drwav_write_pcm_frames(&wav, (drwav_uint64)frames, samples) == (drwav_uint64)frames;
    free(samples);
    drwav_uninit(&wav);
    return ok;
}

static void test_wav_roundtrip(void)
{
    const char *path = "smp_test_audio.wav";
    SmpMedia *media = NULL;
    int16_t buffer[2 * 256];
    size_t read;

    smp_test_begin("wav metadata and decoding");
    SMP_CHECK(write_test_wav(path, 44100, 2, 4410));

    SMP_CHECK_INT(smp_media_open(path, &media), SMP_OK);
    SMP_CHECK(media != NULL);

    if (media != NULL) {
        SMP_CHECK_INT(smp_media_kind(media), SMP_MEDIA_WAV);
        SMP_CHECK_STR(smp_media_kind_name(SMP_MEDIA_WAV), "WAV");
        SMP_CHECK_INT(smp_media_sample_rate(media), 44100);
        SMP_CHECK_INT(smp_media_channels(media), 2);
        SMP_CHECK_INT(smp_media_total_frames(media), 4410);
        SMP_CHECK_NEAR(smp_media_duration(media), 0.1, 0.001);

        read = smp_media_read(media, buffer, 256);
        SMP_CHECK_INT(read, 256);
        SMP_CHECK_INT(smp_media_position(media), 256);
        SMP_CHECK_INT(buffer[0], 0);
        SMP_CHECK_INT(buffer[1], 37);

        SMP_CHECK(smp_media_seek(media, 1000));
        SMP_CHECK_INT(smp_media_position(media), 1000);

        read = smp_media_read(media, buffer, 256);
        SMP_CHECK_INT(read, 256);

        SMP_CHECK(smp_media_seek(media, 4400));
        read = smp_media_read(media, buffer, 256);
        SMP_CHECK_INT(read, 10);

        read = smp_media_read(media, buffer, 256);
        SMP_CHECK_INT(read, 0);

        smp_media_close(media);
    }

    (void)smp_file_remove(path);
    smp_test_end();
}

static void test_unsupported(void)
{
    SmpMedia *media = NULL;

    smp_test_begin("supported and missing files");
    SMP_CHECK(smp_media_supported("track.mp3"));
    SMP_CHECK(smp_media_supported("track.WAV"));
    SMP_CHECK(smp_media_supported("track.flac"));
    SMP_CHECK(smp_media_supported("track.ogg"));
    SMP_CHECK(!smp_media_supported("track.opus"));

    SMP_CHECK_INT(smp_media_open("smp_test_missing.flac", &media), SMP_ERR_DECODE);
    SMP_CHECK_INT(smp_media_open("smp_test_missing.wav", &media), SMP_ERR_DECODE);
    SMP_CHECK_INT(smp_media_open("smp_test_missing.opus", &media), SMP_ERR_UNSUPPORTED);
    smp_test_end();
}

#ifndef SMP_TEST_DATA_DIR
#define SMP_TEST_DATA_DIR "tests/data"
#endif

static const char *fixture(const char *name)
{
    static char path[512];

    snprintf(path, sizeof(path), "%s/%s", SMP_TEST_DATA_DIR, name);
    return path;
}

static void decode_fixture(const char *name, SmpMediaKind expected_kind, const char *kind_name)
{
    SmpMedia *media = NULL;
    int16_t buffer[2 * 512];
    size_t read;

    smp_test_begin(name);
    SMP_CHECK_INT(smp_media_open(fixture(name), &media), SMP_OK);

    if (media != NULL) {
        SMP_CHECK_INT(smp_media_kind(media), expected_kind);
        SMP_CHECK_STR(smp_media_kind_name(expected_kind), kind_name);
        SMP_CHECK_INT(smp_media_sample_rate(media), 48000);
        SMP_CHECK_INT(smp_media_channels(media), 2);
        SMP_CHECK_INT(smp_media_total_frames(media), 48000);
        SMP_CHECK_NEAR(smp_media_duration(media), 1.0, 0.05);

        read = smp_media_read(media, buffer, 512);
        SMP_CHECK_INT(read, 512);

        {
            int nonzero = 0;
            int index;

            for (index = 0; index < 1024; index++) {
                if (buffer[index] != 0) {
                    nonzero++;
                }
            }
            SMP_CHECK(nonzero > 500);
        }

        SMP_CHECK(smp_media_seek(media, 24000));
        SMP_CHECK_INT(smp_media_position(media), 24000);

        read = smp_media_read(media, buffer, 512);
        SMP_CHECK_INT(read, 512);

        smp_media_close(media);
    }
    smp_test_end();
}

/* Builds a minimal MPEG1 Layer III frame header (128 kbps, 44100 Hz, stereo). */
static size_t build_mp3_frame(uint8_t *buffer, size_t size, bool with_xing, uint32_t frames)
{
    size_t frame_length = 417; /* 144 * 128000 / 44100 */
    size_t xing_offset = 4u + 32u;

    memset(buffer, 0, size);
    buffer[0] = 0xffu;
    buffer[1] = 0xfbu; /* MPEG1, Layer III, no CRC */
    buffer[2] = 0x90u; /* 128 kbps, 44100 Hz, no padding */
    buffer[3] = 0x00u; /* stereo */

    if (with_xing && size >= xing_offset + 12u) {
        memcpy(buffer + xing_offset, "Xing", 4);
        buffer[xing_offset + 4] = 0x00;
        buffer[xing_offset + 5] = 0x00;
        buffer[xing_offset + 6] = 0x00;
        buffer[xing_offset + 7] = 0x01;
        buffer[xing_offset + 8] = (uint8_t)((frames >> 24) & 0xffu);
        buffer[xing_offset + 9] = (uint8_t)((frames >> 16) & 0xffu);
        buffer[xing_offset + 10] = (uint8_t)((frames >> 8) & 0xffu);
        buffer[xing_offset + 11] = (uint8_t)(frames & 0xffu);
    }

    return frame_length;
}

static void test_mp3_duration_xing(void)
{
    uint8_t buffer[512];
    double duration;

    smp_test_begin("mp3 duration from a xing header");
    (void)build_mp3_frame(buffer, sizeof(buffer), true, 1000);
    duration = smp_mp3_duration_from_header(buffer, sizeof(buffer), 1000000u);

    /* 1000 frames * 1152 samples / 44100 Hz */
    SMP_CHECK_NEAR(duration, 26.122, 0.01);
    smp_test_end();
}

static void test_mp3_duration_cbr(void)
{
    uint8_t buffer[512];
    double duration;

    smp_test_begin("mp3 duration for constant bitrate");
    (void)build_mp3_frame(buffer, sizeof(buffer), false, 0);

    /* 10 s of 128 kbit/s audio: 160000 bytes * 8 / 128000. */
    duration = smp_mp3_duration_from_header(buffer, sizeof(buffer), 160000u);
    SMP_CHECK_NEAR(duration, 10.0, 0.001);

    SMP_CHECK_NEAR(smp_mp3_duration_from_header(buffer, 2, 1000), 0.0, 0.0001);
    smp_test_end();
}

static void test_mp3_duration_skips_id3(void)
{
    uint8_t buffer[640];
    double duration;

    smp_test_begin("mp3 duration skips the id3v2 tag");
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, "ID3", 3);
    buffer[3] = 3;
    buffer[4] = 0;
    buffer[5] = 0;
    /* Syncsafe size of 10 bytes of tag body. */
    buffer[6] = 0;
    buffer[7] = 0;
    buffer[8] = 0;
    buffer[9] = 10;

    (void)build_mp3_frame(buffer + 20, sizeof(buffer) - 20, true, 500);
    duration = smp_mp3_duration_from_header(buffer, sizeof(buffer), 1000000u);

    SMP_CHECK_NEAR(duration, 500.0 * 1152.0 / 44100.0, 0.01);
    smp_test_end();
}

void suite_media(void)
{
    smp_test_suite("media");
    test_wav_roundtrip();
    test_unsupported();
    decode_fixture("sample.flac", SMP_MEDIA_FLAC, "FLAC");
    decode_fixture("sample.ogg", SMP_MEDIA_OGG, "OGG");
    test_mp3_duration_xing();
    test_mp3_duration_cbr();
    test_mp3_duration_skips_id3();
}

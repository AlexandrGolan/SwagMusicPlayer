/*
 * SMP - Swag Music Player
 * Tests for the PCM converter.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/pcm.h"
#include "test.h"

static void test_identity(void)
{
    PcmConverter *converter = pcm_converter_create(SMP_OUTPUT_RATE, 2);
    int16_t input[200];
    const int16_t *output;
    size_t frames = 0;
    int index;

    smp_test_begin("same rate keeps samples untouched");
    SMP_CHECK(converter != NULL);
    if (converter == NULL) {
        smp_test_end();
        return;
    }

    for (index = 0; index < 100; index++) {
        input[index * 2] = (int16_t)(index * 100);
        input[index * 2 + 1] = (int16_t)(-index * 100);
    }

    output = pcm_converter_process(converter, input, 100, &frames);
    SMP_CHECK(output != NULL);
    SMP_CHECK_INT(frames, 100);

    if (frames == 100) {
        SMP_CHECK_INT(output[0], 0);
        SMP_CHECK_INT(output[2 * 42], 4200);
        SMP_CHECK_INT(output[2 * 42 + 1], -4200);
        SMP_CHECK_INT(output[2 * 99 + 1], -9900);
    }

    pcm_converter_destroy(converter);
    smp_test_end();
}

static void test_mono_to_stereo(void)
{
    PcmConverter *converter = pcm_converter_create(SMP_OUTPUT_RATE, 1);
    int16_t input[3] = {1000, -2000, 3000};
    const int16_t *output;
    size_t frames = 0;

    smp_test_begin("mono is duplicated into both channels");
    SMP_CHECK(converter != NULL);
    if (converter == NULL) {
        smp_test_end();
        return;
    }

    output = pcm_converter_process(converter, input, 3, &frames);
    SMP_CHECK_INT(frames, 3);
    if (frames == 3) {
        SMP_CHECK_INT(output[0], 1000);
        SMP_CHECK_INT(output[1], 1000);
        SMP_CHECK_INT(output[4], 3000);
        SMP_CHECK_INT(output[5], 3000);
    }

    pcm_converter_destroy(converter);
    smp_test_end();
}

static void test_surround_downmix(void)
{
    PcmConverter *converter = pcm_converter_create(SMP_OUTPUT_RATE, 6);
    int16_t input[6] = {100, 200, 300, 400, 500, 600};
    const int16_t *output;
    size_t frames = 0;

    smp_test_begin("6 channels fold into stereo");
    SMP_CHECK(converter != NULL);
    if (converter == NULL) {
        smp_test_end();
        return;
    }

    output = pcm_converter_process(converter, input, 1, &frames);
    SMP_CHECK_INT(frames, 1);
    if (frames == 1) {
        /* Left: (100 + 300 + 500) / 3, right: (200 + 400 + 600) / 3 */
        SMP_CHECK_INT(output[0], 300);
        SMP_CHECK_INT(output[1], 400);
    }

    pcm_converter_destroy(converter);
    smp_test_end();
}

static void test_resample_length(void)
{
    PcmConverter *converter = pcm_converter_create(44100, 2);
    static int16_t input[44100 * 2];
    const int16_t *output;
    size_t frames = 0;

    smp_test_begin("44100 Hz is resampled to 48000 Hz");
    SMP_CHECK(converter != NULL);
    if (converter == NULL) {
        smp_test_end();
        return;
    }

    output = pcm_converter_process(converter, input, 44100, &frames);
    SMP_CHECK(output != NULL);
    /* The last output frame needs a lookahead sample, so allow a small margin. */
    SMP_CHECK(frames >= 47990u && frames <= 48000u);
    SMP_CHECK_INT(pcm_converter_consumed(converter), 44100);

    pcm_converter_destroy(converter);
    smp_test_end();
}

static void test_resample_continuity(void)
{
    PcmConverter *converter = pcm_converter_create(44100, 2);
    static int16_t block[1024 * 2];
    const int16_t *output;
    size_t total = 0;
    size_t frames = 0;
    int iteration;
    int index;

    smp_test_begin("resampling stays continuous across blocks");
    SMP_CHECK(converter != NULL);
    if (converter == NULL) {
        smp_test_end();
        return;
    }

    for (index = 0; index < 1024 * 2; index++) {
        block[index] = 1000;
    }

    for (iteration = 0; iteration < 43; iteration++) {
        output = pcm_converter_process(converter, block, 1024, &frames);
        SMP_CHECK(output != NULL);
        total += frames;
    }

    /* 43 blocks of 1024 frames at 44100 Hz last ~0.998 s -> ~47900 output frames. */
    SMP_CHECK(total > 47000u && total < 49000u);

    pcm_converter_destroy(converter);
    smp_test_end();
}

static void test_gain(void)
{
    int16_t samples[4] = {1000, -1000, 32767, -32768};

    smp_test_begin("gain scales and clamps");
    pcm_apply_gain(samples, 4, 50);
    SMP_CHECK_INT(samples[0], 500);
    SMP_CHECK_INT(samples[1], -500);
    SMP_CHECK_INT(samples[2], 16383);
    SMP_CHECK_INT(samples[3], -16384);

    pcm_apply_gain(samples, 4, 0);
    SMP_CHECK_INT(samples[0], 0);
    SMP_CHECK_INT(samples[1], 0);

    samples[0] = 1234;
    pcm_apply_gain(samples, 1, 100);
    SMP_CHECK_INT(samples[0], 1234);

    SMP_CHECK_INT(pcm_clamp_volume(-5), 0);
    SMP_CHECK_INT(pcm_clamp_volume(150), 100);
    SMP_CHECK_INT(pcm_clamp_volume(70), 70);
    smp_test_end();
}

static void test_seek(void)
{
    PcmConverter *converter = pcm_converter_create(SMP_OUTPUT_RATE, 2);
    int16_t input[4] = {0, 0, 0, 0};
    size_t frames = 0;

    smp_test_begin("seek resets the resampler");
    SMP_CHECK(converter != NULL);
    if (converter == NULL) {
        smp_test_end();
        return;
    }

    (void)pcm_converter_process(converter, input, 2, &frames);
    SMP_CHECK_INT(pcm_converter_consumed(converter), 2);

    pcm_converter_seek(converter, 1000);
    SMP_CHECK_INT(pcm_converter_consumed(converter), 1000);

    pcm_converter_destroy(converter);
    smp_test_end();
}

void suite_pcm(void)
{
    smp_test_suite("pcm");
    test_identity();
    test_mono_to_stereo();
    test_surround_downmix();
    test_resample_length();
    test_resample_continuity();
    test_gain();
    test_seek();
}

/*
 * SMP - Swag Music Player
 * PCM conversion: channel mapping, linear resampling and gain.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/pcm.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define SMP_HAVE_SSE2 1
#include <emmintrin.h>
#endif

#define PCM_MIN_VOLUME 0
#define PCM_MAX_VOLUME 100

struct PcmConverter {
    int source_channels;
    int source_rate;

    double position;      /* absolute position in source frames */
    uint64_t block_base;  /* absolute index of the first frame of the block */
    size_t block_frames;  /* frames in the current block */

    const int16_t *block;
    float last_frame[SMP_OUTPUT_CHANNELS];
    bool has_last;

    int16_t *output;
    size_t output_capacity; /* in frames */
};

static float clamp_sample(float value)
{
    if (value > 32767.0f) {
        return 32767.0f;
    }
    if (value < -32768.0f) {
        return -32768.0f;
    }
    return value;
}

/* Converts one interleaved source frame to stereo. */
static void downmix_frame(const PcmConverter *converter, const int16_t *frame, float out[2])
{
    int channels = converter->source_channels;

    if (channels == 1) {
        out[0] = (float)frame[0];
        out[1] = (float)frame[0];
        return;
    }

    if (channels == 2) {
        out[0] = (float)frame[0];
        out[1] = (float)frame[1];
        return;
    }

    /* >2 channels: fold even channels into left and odd channels into right. */
    {
        float left = 0.0f;
        float right = 0.0f;
        int left_count = 0;
        int right_count = 0;
        int channel;

        for (channel = 0; channel < channels; channel++) {
            if ((channel % 2) == 0) {
                left += (float)frame[channel];
                left_count++;
            } else {
                right += (float)frame[channel];
                right_count++;
            }
        }

        out[0] = left / (float)left_count;
        out[1] = right_count > 0 ? right / (float)right_count : out[0];
    }
}

PcmConverter *pcm_converter_create(int source_rate, int source_channels)
{
    PcmConverter *converter;

    if (source_rate <= 0 || source_channels <= 0) {
        return NULL;
    }

    converter = (PcmConverter *)calloc(1, sizeof(*converter));
    if (converter == NULL) {
        return NULL;
    }

    converter->source_rate = source_rate;
    converter->source_channels = source_channels;
    converter->position = 0.0;

    converter->output_capacity = 4096u;
    converter->output = (int16_t *)malloc(converter->output_capacity * SMP_OUTPUT_CHANNELS
                                          * sizeof(int16_t));
    if (converter->output == NULL) {
        free(converter);
        return NULL;
    }

    return converter;
}

void pcm_converter_destroy(PcmConverter *converter)
{
    if (converter == NULL) {
        return;
    }
    free(converter->output);
    free(converter);
}

static bool ensure_capacity(PcmConverter *converter, size_t frames)
{
    size_t capacity;
    int16_t *grown;

    if (frames <= converter->output_capacity) {
        return true;
    }

    capacity = converter->output_capacity;
    while (capacity < frames) {
        capacity *= 2u;
    }

    grown = (int16_t *)realloc(converter->output, capacity * SMP_OUTPUT_CHANNELS * sizeof(int16_t));
    if (grown == NULL) {
        return false;
    }

    converter->output = grown;
    converter->output_capacity = capacity;
    return true;
}

static bool frame_at(const PcmConverter *converter, int64_t index, float out[2])
{
    int64_t first = (int64_t)converter->block_base;
    int64_t last = first + (int64_t)converter->block_frames - 1;

    if (index == first - 1 && converter->has_last) {
        out[0] = converter->last_frame[0];
        out[1] = converter->last_frame[1];
        return true;
    }
    if (index < first || index > last) {
        return false;
    }

    downmix_frame(converter,
                  converter->block + (size_t)(index - first) * (size_t)converter->source_channels,
                  out);
    return true;
}

int16_t *pcm_converter_process(PcmConverter *converter,
                                     const int16_t *source_frames,
                                     size_t frame_count,
                                     size_t *out_frames)
{
    const double step = (double)converter->source_rate / (double)SMP_OUTPUT_RATE;
    size_t produced = 0;
    size_t capacity;

    if (out_frames != NULL) {
        *out_frames = 0;
    }
    if (converter == NULL || out_frames == NULL) {
        return NULL;
    }
    if (source_frames == NULL || frame_count == 0) {
        return converter->output;
    }

    converter->block = source_frames;
    converter->block_frames = frame_count;

    capacity = (size_t)((double)frame_count / step) + 8u;
    if (!ensure_capacity(converter, capacity)) {
        return NULL;
    }

    for (;;) {
        int64_t index = (int64_t)floor(converter->position);
        int64_t last_index = (int64_t)(converter->block_base + converter->block_frames) - 1;
        double fraction = converter->position - (double)index;
        float current[SMP_OUTPUT_CHANNELS];
        float next[SMP_OUTPUT_CHANNELS];
        int channel;

        if (!frame_at(converter, index, current)) {
            break;
        }

        if (frame_at(converter, index + 1, next)) {
            for (channel = 0; channel < SMP_OUTPUT_CHANNELS; channel++) {
                float sample = current[channel] + (next[channel] - current[channel]) * (float)fraction;

                converter->output[produced * SMP_OUTPUT_CHANNELS + (size_t)channel]
                    = (int16_t)clamp_sample(sample);
            }
        } else if (fraction <= 1e-9 && index == last_index) {
            /*
             * The position lands exactly on the last sample of the block and
             * there is no successor to interpolate with: emit it as is.
             * Positions with a fractional part are deferred to the next block,
             * where `last_frame` provides the missing sample.
             */
            for (channel = 0; channel < SMP_OUTPUT_CHANNELS; channel++) {
                converter->output[produced * SMP_OUTPUT_CHANNELS + (size_t)channel]
                    = (int16_t)clamp_sample(current[channel]);
            }
        } else {
            break;
        }

        produced++;
        converter->position += step;
    }

    /* Remember the last input frame so the next block can interpolate across it. */
    if (frame_count > 0) {
        downmix_frame(converter,
                      source_frames + (frame_count - 1u) * (size_t)converter->source_channels,
                      converter->last_frame);
        converter->has_last = true;
    }

    converter->block_base += (uint64_t)frame_count;
    converter->block = NULL;
    converter->block_frames = 0;

    *out_frames = produced;
    return converter->output;
}

void pcm_converter_seek(PcmConverter *converter, uint64_t source_frame)
{
    if (converter == NULL) {
        return;
    }

    converter->position = (double)source_frame;
    converter->block_base = source_frame;
    converter->has_last = false;
    converter->block = NULL;
    converter->block_frames = 0;
}

uint64_t pcm_converter_consumed(const PcmConverter *converter)
{
    if (converter == NULL) {
        return 0;
    }
    return converter->block_base;
}

void pcm_apply_gain(int16_t *samples, size_t sample_count, int volume_percent)
{
    size_t index;
    float factor;

    if (samples == NULL || volume_percent >= PCM_MAX_VOLUME) {
        return;
    }

    volume_percent = pcm_clamp_volume(volume_percent);
    if (volume_percent >= PCM_MAX_VOLUME) {
        return;
    }

    factor = (float)volume_percent / (float)PCM_MAX_VOLUME;

#if defined(SMP_HAVE_SSE2)
    /*
     * Eight samples per iteration: sign-extend to 32 bits, scale, truncate
     * towards zero and saturate back to 16 bits. `_mm_cvttps_epi32` keeps the
     * result identical to the scalar path (which also truncates), so the unit
     * tests hold for both implementations.
     */
    {
        const __m128 scale = _mm_set1_ps(factor);
        size_t vector_end = sample_count & ~(size_t)7u;

        for (index = 0; index < vector_end; index += 8) {
            __m128i packed = _mm_loadu_si128((const __m128i *)(const void *)(samples + index));
            __m128i sign = _mm_srai_epi16(packed, 15);
            __m128i low = _mm_unpacklo_epi16(packed, sign);
            __m128i high = _mm_unpackhi_epi16(packed, sign);
            __m128i scaled_low = _mm_cvttps_epi32(_mm_mul_ps(_mm_cvtepi32_ps(low), scale));
            __m128i scaled_high = _mm_cvttps_epi32(_mm_mul_ps(_mm_cvtepi32_ps(high), scale));

            _mm_storeu_si128((__m128i *)(void *)(samples + index),
                             _mm_packs_epi32(scaled_low, scaled_high));
        }
    }
#else
    index = 0;
#endif

    for (; index < sample_count; index++) {
        float scaled = (float)samples[index] * factor;

        if (scaled > 32767.0f) {
            scaled = 32767.0f;
        } else if (scaled < -32768.0f) {
            scaled = -32768.0f;
        }
        samples[index] = (int16_t)scaled;
    }
}

int pcm_clamp_volume(int volume_percent)
{
    if (volume_percent < PCM_MIN_VOLUME) {
        return PCM_MIN_VOLUME;
    }
    if (volume_percent > PCM_MAX_VOLUME) {
        return PCM_MAX_VOLUME;
    }
    return volume_percent;
}

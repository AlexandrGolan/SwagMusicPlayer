/*
 * SMP - Swag Music Player
 * PCM conversion: channel mapping, linear resampling and gain.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_PCM_H
#define SMP_PCM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smp/common.h"

/* The audio device always runs in this format. */
#define SMP_OUTPUT_RATE 48000
#define SMP_OUTPUT_CHANNELS 2

typedef struct PcmConverter PcmConverter;

/*
 * Creates a converter from `source_rate`/`source_channels` (interleaved
 * signed 16-bit) to SMP_OUTPUT_RATE / SMP_OUTPUT_CHANNELS.
 * Returns NULL on invalid arguments or allocation failure.
 */
PcmConverter *pcm_converter_create(int source_rate, int source_channels);

void pcm_converter_destroy(PcmConverter *converter);

/*
 * Converts one block of source frames and returns a pointer to the internal
 * stereo buffer. The pointer stays valid until the next call.
 * `out_frames` receives the number of stereo frames produced.
 */
int16_t *pcm_converter_process(PcmConverter *converter,
                                     const int16_t *source_frames,
                                     size_t frame_count,
                                     size_t *out_frames);

/* Discards resampler history and continues from `source_frame`. */
void pcm_converter_seek(PcmConverter *converter, uint64_t source_frame);

/* Input frames consumed so far, in source frames. */
uint64_t pcm_converter_consumed(const PcmConverter *converter);

/* Applies an integer gain in percent (0..100) in place. */
void pcm_apply_gain(int16_t *samples, size_t sample_count, int volume_percent);

/* Clamps a volume to the supported range. */
int pcm_clamp_volume(int volume_percent);

#endif /* SMP_PCM_H */

/*
 * SMP - Swag Music Player
 * Audio output abstraction.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_AUDIO_H
#define SMP_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "smp/common.h"

typedef struct SmpAudioDevice SmpAudioDevice;

typedef struct {
    int sample_rate;
    int channels;
} SmpAudioFormat;

/*
 * Opens an output device. `backend` may be NULL/"auto" for the platform
 * default, or one of "alsa", "wasapi", "null".
 */
SmpStatus smp_audio_open(const char *backend,
                         SmpAudioDevice **out_device,
                         const SmpAudioFormat *format);

void smp_audio_close(SmpAudioDevice *device);

/*
 * Writes interleaved S16 frames, blocking until the device accepted them.
 */
SmpStatus smp_audio_write(SmpAudioDevice *device, const int16_t *frames, size_t frame_count);

/* Waits until everything queued in the device has been played. */
void smp_audio_drain(SmpAudioDevice *device);

/*
 * Silences or unmutes the device immediately, without stopping it. Pausing
 * uses this so that the audio stops on the next device period instead of
 * playing out the frames that are already queued.
 */
void smp_audio_set_muted(SmpAudioDevice *device, bool muted);

const char *smp_audio_name(const SmpAudioDevice *device);

/* Prints the available output devices to stdout. */
void smp_audio_list_devices(void);

#endif /* SMP_AUDIO_H */

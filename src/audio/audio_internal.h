/*
 * SMP - Swag Music Player
 * Internal audio backend interface shared with the platform implementations.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_AUDIO_INTERNAL_H
#define SMP_AUDIO_INTERNAL_H

#include "smp/audio.h"

typedef struct {
    SmpStatus (*write)(void *impl, const int16_t *frames, size_t frame_count);
    void (*drain)(void *impl);
    void (*close)(void *impl);
    const char *name;
} SmpAudioOps;

struct SmpAudioDevice {
    const SmpAudioOps *ops;
    void *impl;
};

/* Platform entry points. */
void smp_audio_alsa_list_devices(void);
SmpStatus smp_audio_null_open(SmpAudioDevice **out_device, const SmpAudioFormat *format);
SmpStatus smp_audio_alsa_open(SmpAudioDevice **out_device, const SmpAudioFormat *format);
SmpStatus smp_audio_wasapi_open(SmpAudioDevice **out_device, const SmpAudioFormat *format);

/* Allocates a device with the given operations. */
SmpAudioDevice *smp_audio_device_create(const SmpAudioOps *ops, void *impl);

#endif /* SMP_AUDIO_INTERNAL_H */

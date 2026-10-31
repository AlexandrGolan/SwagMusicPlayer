/*
 * SMP - Swag Music Player
 * Silent audio backend used by tests and by --audio null.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdlib.h>

#include "audio_internal.h"

typedef struct {
    SmpAudioFormat format;
} NullDevice;

static SmpStatus null_write(void *impl, const int16_t *frames, size_t frame_count)
{
    NullDevice *device = (NullDevice *)impl;

    (void)frames;
    (void)frame_count;
    (void)device;
    return SMP_OK;
}

static void null_drain(void *impl)
{
    (void)impl;
}

static void null_close(void *impl)
{
    free(impl);
}

static const SmpAudioOps null_ops = {
    null_write,
    null_drain,
    null_close,
    "null",
};

SmpStatus smp_audio_null_open(SmpAudioDevice **out_device, const SmpAudioFormat *format)
{
    NullDevice *impl;
    SmpAudioDevice *device;

    if (out_device == NULL || format == NULL) {
        return SMP_ERR_INVALID_ARG;
    }

    impl = (NullDevice *)calloc(1, sizeof(*impl));
    if (impl == NULL) {
        return SMP_ERR_OUT_OF_MEMORY;
    }

    impl->format = *format;

    device = smp_audio_device_create(&null_ops, impl);
    if (device == NULL) {
        free(impl);
        return SMP_ERR_OUT_OF_MEMORY;
    }

    *out_device = device;
    return SMP_OK;
}

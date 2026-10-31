/*
 * SMP - Swag Music Player
 * Audio backend selection.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_internal.h"

SmpAudioDevice *smp_audio_device_create(const SmpAudioOps *ops, void *impl)
{
    SmpAudioDevice *device;

    if (ops == NULL) {
        return NULL;
    }

    device = (SmpAudioDevice *)calloc(1, sizeof(*device));
    if (device == NULL) {
        return NULL;
    }

    device->ops = ops;
    device->impl = impl;
    return device;
}

static bool wants(const char *requested, const char *name)
{
    return requested != NULL && strcmp(requested, name) == 0;
}

SmpStatus smp_audio_open(const char *backend,
                         SmpAudioDevice **out_device,
                         const SmpAudioFormat *format)
{
    if (out_device == NULL || format == NULL) {
        return SMP_ERR_INVALID_ARG;
    }
    *out_device = NULL;

    if (format->sample_rate <= 0 || format->channels <= 0) {
        return SMP_ERR_INVALID_ARG;
    }

    if (wants(backend, "null")) {
        return smp_audio_null_open(out_device, format);
    }

#if defined(_WIN32)
    if (backend == NULL || wants(backend, "auto") || wants(backend, "wasapi")) {
        return smp_audio_wasapi_open(out_device, format);
    }
#elif defined(__linux__)
    if (backend == NULL || wants(backend, "auto") || wants(backend, "alsa")) {
        return smp_audio_alsa_open(out_device, format);
    }
#endif

    return SMP_ERR_UNSUPPORTED;
}

void smp_audio_close(SmpAudioDevice *device)
{
    if (device == NULL) {
        return;
    }

    if (device->ops != NULL && device->ops->close != NULL) {
        device->ops->close(device->impl);
    }
    free(device);
}

SmpStatus smp_audio_write(SmpAudioDevice *device, const int16_t *frames, size_t frame_count)
{
    if (device == NULL || device->ops == NULL || device->ops->write == NULL) {
        return SMP_ERR_INVALID_ARG;
    }
    if (frames == NULL || frame_count == 0) {
        return SMP_OK;
    }
    return device->ops->write(device->impl, frames, frame_count);
}

void smp_audio_drain(SmpAudioDevice *device)
{
    if (device != NULL && device->ops != NULL && device->ops->drain != NULL) {
        device->ops->drain(device->impl);
    }
}

const char *smp_audio_name(const SmpAudioDevice *device)
{
    if (device == NULL || device->ops == NULL || device->ops->name == NULL) {
        return "unknown";
    }
    return device->ops->name;
}

void smp_audio_list_devices(void)
{
#if defined(__linux__)
    smp_audio_alsa_list_devices();
#else
    printf("Device enumeration is not implemented for this platform.\n");
    printf("The default output device is always used.\n");
#endif
}

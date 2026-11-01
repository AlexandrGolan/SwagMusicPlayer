/*
 * SMP - Swag Music Player
 * Audio output through miniaudio (WASAPI, ALSA, PulseAudio, CoreAudio, ...).
 *
 * The device runs at a fixed format (48 kHz, stereo, signed 16-bit) and pulls
 * samples from a lock-free ring buffer that the playback thread fills. That is
 * the same contract the rest of the player expects, so the backend can be
 * swapped without touching the engine.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "smp/audio.h"
#include "smp/common.h"

#define BUFFER_MILLISECONDS 250u

struct SmpAudioDevice {
    ma_context context;
    ma_device device;
    ma_pcm_rb ring;

    bool has_context;
    bool has_device;
    bool has_ring;

    ma_uint32 channels;
    ma_uint32 sample_rate;

    char name[32];
};

static void device_callback(ma_device *device, void *output, const void *input, ma_uint32 frame_count)
{
    SmpAudioDevice *impl = (SmpAudioDevice *)device->pUserData;
    ma_uint8 *cursor = (ma_uint8 *)output;
    ma_uint32 remaining = frame_count;
    ma_uint32 frame_bytes;

    SMP_UNUSED(input);

    if (impl == NULL || output == NULL) {
        return;
    }

    frame_bytes = ma_get_bytes_per_frame(device->playback.format, device->playback.channels);

    while (remaining > 0) {
        ma_uint32 available = remaining;
        void *source = NULL;

        if (ma_pcm_rb_acquire_read(&impl->ring, &available, &source) != MA_SUCCESS || available == 0) {
            break;
        }

        memcpy(cursor, source, (size_t)available * frame_bytes);
        (void)ma_pcm_rb_commit_read(&impl->ring, available);

        cursor += (size_t)available * frame_bytes;
        remaining -= available;
    }

    if (remaining > 0) {
        memset(cursor, 0, (size_t)remaining * frame_bytes);
    }
}

static bool backend_from_name(const char *name, ma_backend *out)
{
    static const struct {
        const char *name;
        ma_backend backend;
    } table[] = {
        {"wasapi", ma_backend_wasapi},
        {"alsa", ma_backend_alsa},
        {"pulse", ma_backend_pulseaudio},
        {"pulseaudio", ma_backend_pulseaudio},
        {"coreaudio", ma_backend_coreaudio},
        {"sndio", ma_backend_sndio},
        {"oss", ma_backend_oss},
        {"null", ma_backend_null},
    };
    size_t index;

    for (index = 0; index < SMP_ARRAY_LEN(table); index++) {
        if (strcmp(name, table[index].name) == 0) {
            *out = table[index].backend;
            return true;
        }
    }
    return false;
}

SmpStatus smp_audio_open(const char *backend,
                         SmpAudioDevice **out_device,
                         const SmpAudioFormat *format)
{
    SmpAudioDevice *device;
    ma_device_config config;
    ma_backend requested = ma_backend_null;
    bool explicit_backend = false;
    ma_result result;

    if (out_device == NULL || format == NULL) {
        return SMP_ERR_INVALID_ARG;
    }
    *out_device = NULL;

    device = (SmpAudioDevice *)calloc(1, sizeof(*device));
    if (device == NULL) {
        return SMP_ERR_OUT_OF_MEMORY;
    }

    if (backend != NULL && strcmp(backend, "auto") != 0) {
        if (!backend_from_name(backend, &requested)) {
            free(device);
            return SMP_ERR_UNSUPPORTED;
        }
        explicit_backend = true;
    }

    if (explicit_backend) {
        result = ma_context_init(&requested, 1, NULL, &device->context);
        if (result != MA_SUCCESS) {
            fprintf(stderr, "smp: cannot initialise the %s audio backend\n", backend);
            free(device);
            return SMP_ERR_AUDIO;
        }
        device->has_context = true;
    }

    device->channels = (ma_uint32)format->channels;
    device->sample_rate = (ma_uint32)format->sample_rate;

    result = ma_pcm_rb_init(ma_format_s16,
                            device->channels,
                            device->sample_rate * BUFFER_MILLISECONDS / 1000u,
                            NULL,
                            NULL,
                            &device->ring);
    if (result != MA_SUCCESS) {
        if (device->has_context) {
            ma_context_uninit(&device->context);
        }
        free(device);
        return SMP_ERR_OUT_OF_MEMORY;
    }
    device->has_ring = true;

    config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = device->channels;
    config.sampleRate = device->sample_rate;
    config.dataCallback = device_callback;
    config.pUserData = device;
    config.performanceProfile = ma_performance_profile_low_latency;

    result = ma_device_init(device->has_context ? &device->context : NULL, &config, &device->device);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "smp: cannot open an audio device (%s)\n", ma_result_description(result));
        smp_audio_close(device);
        return SMP_ERR_AUDIO;
    }
    device->has_device = true;

    result = ma_device_start(&device->device);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "smp: cannot start audio playback (%s)\n", ma_result_description(result));
        smp_audio_close(device);
        return SMP_ERR_AUDIO;
    }

    snprintf(device->name,
             sizeof(device->name),
             "%s",
             device->device.pContext != NULL ? ma_get_backend_name(device->device.pContext->backend)
                                             : "unknown");

    *out_device = device;
    return SMP_OK;
}

void smp_audio_close(SmpAudioDevice *device)
{
    if (device == NULL) {
        return;
    }

    if (device->has_device) {
        (void)ma_device_uninit(&device->device);
    }
    if (device->has_ring) {
        (void)ma_pcm_rb_uninit(&device->ring);
    }
    if (device->has_context) {
        (void)ma_context_uninit(&device->context);
    }
    free(device);
}

SmpStatus smp_audio_write(SmpAudioDevice *device, const int16_t *frames, size_t frame_count)
{
    size_t written = 0;
    size_t frame_bytes;
    uint64_t stalled_since = 0;

    if (device == NULL || frames == NULL || frame_count == 0) {
        return SMP_ERR_INVALID_ARG;
    }

    frame_bytes = (size_t)device->channels * sizeof(int16_t);

    while (written < frame_count) {
        ma_uint32 request = (ma_uint32)SMP_MIN(frame_count - written, (size_t)8192);
        ma_uint32 available = request;
        void *target = NULL;

        if (ma_pcm_rb_acquire_write(&device->ring, &available, &target) != MA_SUCCESS) {
            return SMP_ERR_AUDIO;
        }
        if (available == 0) {
            uint64_t now;

            (void)ma_pcm_rb_commit_write(&device->ring, 0);

            /*
             * If the device disappears (unplugged DAC, stopped service) the
             * callback never drains the ring buffer again. Give up instead of
             * blocking the playback thread forever.
             */
            now = smp_now_ms();
            if (stalled_since == 0) {
                stalled_since = now;
            } else if (now - stalled_since > 2000u) {
                return SMP_ERR_AUDIO;
            }

            smp_sleep_ms(2);
            continue;
        }

        stalled_since = 0;

        memcpy(target, frames + written * (size_t)device->channels, (size_t)available * frame_bytes);
        (void)ma_pcm_rb_commit_write(&device->ring, available);
        written += available;
    }

    return SMP_OK;
}

void smp_audio_set_muted(SmpAudioDevice *device, bool muted)
{
    if (device == NULL || !device->has_device) {
        return;
    }

    (void)ma_device_set_master_volume(&device->device, muted ? 0.0f : 1.0f);
}

void smp_audio_drain(SmpAudioDevice *device)
{
    if (device == NULL) {
        return;
    }

    while (ma_pcm_rb_available_read(&device->ring) > 0) {
        smp_sleep_ms(5);
    }
}

const char *smp_audio_name(const SmpAudioDevice *device)
{
    return device != NULL ? device->name : "unknown";
}

void smp_audio_list_devices(void)
{
    ma_context context;
    ma_device_info *playback = NULL;
    ma_uint32 playback_count = 0;
    ma_device_info *capture = NULL;
    ma_uint32 capture_count = 0;
    ma_uint32 index;

    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        printf("cannot initialise an audio context\n");
        return;
    }

    printf("audio backend: %s\n", ma_get_backend_name(context.backend));

    if (ma_context_get_devices(&context, &playback, &playback_count, &capture, &capture_count)
        != MA_SUCCESS)
    {
        printf("device enumeration failed\n");
        (void)ma_context_uninit(&context);
        return;
    }

    printf("\nplayback devices:\n");
    for (index = 0; index < playback_count; index++) {
        printf("  [%u] %s%s\n",
               index,
               playback[index].name,
               playback[index].isDefault ? "  (default)" : "");
    }
    if (playback_count == 0) {
        printf("  (none)\n");
    }

    (void)ma_context_uninit(&context);
}

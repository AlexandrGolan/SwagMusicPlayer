/*
 * SMP - Swag Music Player
 * ALSA audio backend (Linux).
 *
 * SPDX-License-Identifier: MIT
 */
#ifdef __linux__

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_internal.h"
#include "smp/common.h"

#define ALSA_LATENCY_US 200000u /* 200 ms */

typedef struct {
    snd_pcm_t *pcm;
    int channels;
} AlsaDevice;

static void alsa_close(void *impl)
{
    AlsaDevice *device = (AlsaDevice *)impl;

    if (device == NULL) {
        return;
    }

    if (device->pcm != NULL) {
        (void)snd_pcm_drain(device->pcm);
        (void)snd_pcm_close(device->pcm);
    }
    free(device);
}

static SmpStatus alsa_write(void *impl, const int16_t *frames, size_t frame_count)
{
    AlsaDevice *device = (AlsaDevice *)impl;
    size_t written = 0;

    if (device == NULL || device->pcm == NULL) {
        return SMP_ERR_AUDIO;
    }

    while (written < frame_count) {
        snd_pcm_sframes_t result = snd_pcm_writei(device->pcm,
                                                  frames + written * (size_t)device->channels,
                                                  (snd_pcm_uframes_t)(frame_count - written));

        if (result == -EPIPE || result == -ESTRPIPE || result == -EINTR) {
            if (snd_pcm_recover(device->pcm, (int)result, 1) != 0) {
                return SMP_ERR_AUDIO;
            }
            continue;
        }
        if (result < 0) {
            return SMP_ERR_AUDIO;
        }
        written += (size_t)result;
    }

    return SMP_OK;
}

static void alsa_drain(void *impl)
{
    AlsaDevice *device = (AlsaDevice *)impl;

    if (device != NULL && device->pcm != NULL) {
        (void)snd_pcm_drain(device->pcm);
    }
}

static const SmpAudioOps alsa_ops = {
    alsa_write,
    alsa_drain,
    alsa_close,
    "alsa",
};

SmpStatus smp_audio_alsa_open(SmpAudioDevice **out_device, const SmpAudioFormat *format)
{
    static const char *const candidates[] = {
        "default", "pulse", "sysdefault", "plughw:0,0", "hw:0,0",
    };
    AlsaDevice *impl;
    SmpAudioDevice *device;
    snd_pcm_t *pcm = NULL;
    int result = -1;
    size_t index;

    if (out_device == NULL || format == NULL) {
        return SMP_ERR_INVALID_ARG;
    }

    for (index = 0; index < SMP_ARRAY_LEN(candidates); index++) {
        result = snd_pcm_open(&pcm, candidates[index], SND_PCM_STREAM_PLAYBACK, 0);
        if (result == 0) {
            break;
        }
        pcm = NULL;
    }

    if (pcm == NULL) {
        fprintf(stderr, "smp: cannot open an ALSA device: %s\n", snd_strerror(result));
        return SMP_ERR_AUDIO;
    }

    result = snd_pcm_set_params(pcm,
                                SND_PCM_FORMAT_S16_LE,
                                SND_PCM_ACCESS_RW_INTERLEAVED,
                                (unsigned int)format->channels,
                                (unsigned int)format->sample_rate,
                                1, /* allow ALSA to resample if the device requires it */
                                ALSA_LATENCY_US);
    if (result < 0) {
        fprintf(stderr, "smp: cannot configure the ALSA device: %s\n", snd_strerror(result));
        (void)snd_pcm_close(pcm);
        return SMP_ERR_AUDIO;
    }

    impl = (AlsaDevice *)calloc(1, sizeof(*impl));
    if (impl == NULL) {
        (void)snd_pcm_close(pcm);
        return SMP_ERR_OUT_OF_MEMORY;
    }

    impl->pcm = pcm;
    impl->channels = format->channels;

    device = smp_audio_device_create(&alsa_ops, impl);
    if (device == NULL) {
        (void)snd_pcm_close(pcm);
        free(impl);
        return SMP_ERR_OUT_OF_MEMORY;
    }

    *out_device = device;
    return SMP_OK;
}

void smp_audio_alsa_list_devices(void)
{
    void **hints = NULL;

    printf("ALSA playback devices:\n");

    if (snd_device_name_hint(-1, "pcm", &hints) != 0 || hints == NULL) {
        printf("  (enumeration failed)\n");
        return;
    }

    for (void **hint = hints; *hint != NULL; hint++) {
        char *name = snd_device_name_get_hint(*hint, "NAME");
        char *description = snd_device_name_get_hint(*hint, "DESC");
        char *io = snd_device_name_get_hint(*hint, "IOID");

        if (name != NULL && (io == NULL || strcmp(io, "Output") == 0)) {
            printf("  %-24s %s\n", name, description != NULL ? description : "");
        }

        free(name);
        free(description);
        free(io);
    }

    snd_device_name_free_hint(hints);
}

#endif /* __linux__ */

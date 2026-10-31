/*
 * SMP - Swag Music Player
 * WASAPI audio backend (Windows).
 *
 * SPDX-License-Identifier: MIT
 */
#ifdef _WIN32

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "audio_internal.h"
#include "smp/common.h"

/* Explicit GUIDs keep the build free of libuuid symbol dependencies. */
static const CLSID smp_clsid_mmdevice_enumerator = {
    0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}
};
static const IID smp_iid_mmdevice_enumerator = {
    0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}
};
static const IID smp_iid_audio_client = {
    0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}
};
static const IID smp_iid_audio_render_client = {
    0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}
};
static const GUID smp_ksdataformat_subtype_ieee_float = {
    0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}
};

#define WASAPI_BUFFER_DURATION (2 * 10000000LL / 10) /* 200 ms in 100 ns units */

typedef struct {
    IAudioClient *client;
    IAudioRenderClient *render;
    WAVEFORMATEX *format;
    UINT32 buffer_frames;
    bool com_initialized;
    bool started;
} WasapiDevice;

static bool format_is_float(const WAVEFORMATEX *format)
{
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return true;
    }
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE *extended = (const WAVEFORMATEXTENSIBLE *)format;

        return memcmp(&extended->SubFormat,
                      &smp_ksdataformat_subtype_ieee_float,
                      sizeof(GUID))
               == 0;
    }
    return false;
}

/* Converts interleaved S16 stereo into the device mix format. */
static void convert_to_mix(const WAVEFORMATEX *format,
                           const int16_t *source,
                           size_t frames,
                           uint8_t *destination)
{
    int destination_channels = (int)format->nChannels;
    bool is_float = format_is_float(format);
    size_t frame;
    int channel;

    for (frame = 0; frame < frames; frame++) {
        int16_t left = source[frame * 2];
        int16_t right = source[frame * 2 + 1];

        for (channel = 0; channel < destination_channels; channel++) {
            float sample;

            if (destination_channels == 1) {
                sample = ((float)left + (float)right) * 0.5f;
            } else if (channel == 0) {
                sample = (float)left;
            } else if (channel == 1) {
                sample = (float)right;
            } else {
                sample = 0.0f;
            }

            if (is_float && format->wBitsPerSample == 32) {
                float *target = (float *)(destination
                                          + (frame * (size_t)destination_channels + (size_t)channel)
                                                * sizeof(float));
                *target = sample / 32768.0f;
            } else if (!is_float && format->wBitsPerSample == 16) {
                int16_t *target
                    = (int16_t *)(destination
                                  + (frame * (size_t)destination_channels + (size_t)channel) * 2u);
                *target = (int16_t)sample;
            } else if (!is_float && format->wBitsPerSample == 32) {
                int32_t *target
                    = (int32_t *)(destination
                                  + (frame * (size_t)destination_channels + (size_t)channel) * 4u);
                *target = (int32_t)sample * 65536;
            }
        }
    }
}

static void wasapi_release(WasapiDevice *device)
{
    if (device->client != NULL) {
        if (device->started) {
            device->client->lpVtbl->Stop(device->client);
        }
        device->client->lpVtbl->Release(device->client);
    }
    if (device->render != NULL) {
        device->render->lpVtbl->Release(device->render);
    }
    if (device->format != NULL) {
        CoTaskMemFree(device->format);
    }
    if (device->com_initialized) {
        CoUninitialize();
    }
}

static void wasapi_close(void *impl)
{
    WasapiDevice *device = (WasapiDevice *)impl;

    if (device == NULL) {
        return;
    }

    if (device->client != NULL && device->started) {
        device->client->lpVtbl->Stop(device->client);
        device->started = false;
    }
    wasapi_release(device);
    free(device);
}

static SmpStatus wasapi_write(void *impl, const int16_t *frames, size_t frame_count)
{
    WasapiDevice *device = (WasapiDevice *)impl;
    size_t written = 0;

    if (device == NULL || device->client == NULL || device->render == NULL) {
        return SMP_ERR_AUDIO;
    }

    while (written < frame_count) {
        UINT32 padding = 0;
        UINT32 available;
        UINT32 to_write;
        BYTE *buffer = NULL;

        if (FAILED(device->client->lpVtbl->GetCurrentPadding(device->client, &padding))) {
            return SMP_ERR_AUDIO;
        }

        available = device->buffer_frames - padding;
        if (available == 0) {
            smp_sleep_ms(5);
            continue;
        }

        to_write = (UINT32)SMP_MIN((size_t)available, frame_count - written);
        if (FAILED(device->render->lpVtbl->GetBuffer(device->render, to_write, &buffer))) {
            return SMP_ERR_AUDIO;
        }

        convert_to_mix(device->format, frames + written * 2u, to_write, buffer);

        if (FAILED(device->render->lpVtbl->ReleaseBuffer(device->render, to_write, 0))) {
            return SMP_ERR_AUDIO;
        }

        written += to_write;
    }

    return SMP_OK;
}

static void wasapi_drain(void *impl)
{
    WasapiDevice *device = (WasapiDevice *)impl;
    UINT32 padding = 0;

    if (device == NULL || device->client == NULL) {
        return;
    }

    do {
        if (FAILED(device->client->lpVtbl->GetCurrentPadding(device->client, &padding))) {
            return;
        }
        if (padding > 0) {
            smp_sleep_ms(10);
        }
    } while (padding > 0);
}

static const SmpAudioOps wasapi_ops = {
    wasapi_write,
    wasapi_drain,
    wasapi_close,
    "wasapi",
};

SmpStatus smp_audio_wasapi_open(SmpAudioDevice **out_device, const SmpAudioFormat *format)
{
    WasapiDevice *impl;
    SmpAudioDevice *device;
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDevice *endpoint = NULL;
    HRESULT status;
    bool com_initialized = false;

    (void)format; /* the shared mix format is used as is */

    if (out_device == NULL) {
        return SMP_ERR_INVALID_ARG;
    }
    *out_device = NULL;

    status = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(status)) {
        com_initialized = true;
    } else if (status != RPC_E_CHANGED_MODE) {
        return SMP_ERR_AUDIO;
    }

    impl = (WasapiDevice *)calloc(1, sizeof(*impl));
    if (impl == NULL) {
        if (com_initialized) {
            CoUninitialize();
        }
        return SMP_ERR_OUT_OF_MEMORY;
    }
    impl->com_initialized = com_initialized;

    status = CoCreateInstance(&smp_clsid_mmdevice_enumerator,
                              NULL,
                              CLSCTX_ALL,
                              &smp_iid_mmdevice_enumerator,
                              (void **)&enumerator);
    if (FAILED(status) || enumerator == NULL) {
        goto failure;
    }

    status = enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &endpoint);
    if (FAILED(status) || endpoint == NULL) {
        goto failure;
    }

    status = endpoint->lpVtbl->Activate(endpoint, &smp_iid_audio_client, CLSCTX_ALL, NULL,
                                        (void **)&impl->client);
    if (FAILED(status) || impl->client == NULL) {
        goto failure;
    }

    status = impl->client->lpVtbl->GetMixFormat(impl->client, &impl->format);
    if (FAILED(status) || impl->format == NULL) {
        goto failure;
    }

    status = impl->client->lpVtbl->Initialize(impl->client,
                                              AUDCLNT_SHAREMODE_SHARED,
                                              0,
                                              WASAPI_BUFFER_DURATION,
                                              0,
                                              impl->format,
                                              NULL);
    if (FAILED(status)) {
        goto failure;
    }

    status = impl->client->lpVtbl->GetBufferSize(impl->client, &impl->buffer_frames);
    if (FAILED(status) || impl->buffer_frames == 0) {
        goto failure;
    }

    status = impl->client->lpVtbl->GetService(impl->client,
                                              &smp_iid_audio_render_client,
                                              (void **)&impl->render);
    if (FAILED(status) || impl->render == NULL) {
        goto failure;
    }

    status = impl->client->lpVtbl->Start(impl->client);
    if (FAILED(status)) {
        goto failure;
    }
    impl->started = true;

    endpoint->lpVtbl->Release(endpoint);
    enumerator->lpVtbl->Release(enumerator);

    device = smp_audio_device_create(&wasapi_ops, impl);
    if (device == NULL) {
        wasapi_close(impl);
        return SMP_ERR_OUT_OF_MEMORY;
    }

    *out_device = device;
    return SMP_OK;

failure:
    if (endpoint != NULL) {
        endpoint->lpVtbl->Release(endpoint);
    }
    if (enumerator != NULL) {
        enumerator->lpVtbl->Release(enumerator);
    }
    wasapi_release(impl);
    free(impl);
    return SMP_ERR_AUDIO;
}

#endif /* _WIN32 */

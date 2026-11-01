/*
 * SMP - Swag Music Player
 * Spectrum analyser: FFT bands, smoothing and peak markers.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/spectrum.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "smp/fft.h"
#include "smp/thread.h"


#define SMP_SPECTRUM_DB_FLOOR (-70.0)
/*
 * Bar dynamics follow cava (cavacore.c): the bar jumps straight to a new peak,
 * falls with an accelerating "gravity" term and is then smoothed by a first
 * order integrator, which is what makes cava look fluid instead of stepped.
 */
#define SMP_CAVA_NOISE_REDUCTION 0.77f
#define SMP_CAVA_FALL_STEP 0.028f
#define SMP_CAVA_REFERENCE_RATE 66.0f
#define SMP_SPECTRUM_PEAK_FALL 0.35f
#define SMP_SPECTRUM_PEAK_HOLD 1.0f
#define SMP_SPECTRUM_FULL_SCALE 32768.0

struct SmpSpectrum {
    SmpMutex *mutex;

    int sample_rate;
    size_t window_size;
    size_t bin_limit;

    int16_t *samples;
    size_t write_index;
    size_t filled;

    float *real;
    float *imaginary;

    size_t band_low[SMP_SPECTRUM_BANDS];
    size_t band_high[SMP_SPECTRUM_BANDS];

    float bands[SMP_SPECTRUM_BANDS];
    float peaks[SMP_SPECTRUM_BANDS];
    float hold[SMP_SPECTRUM_BANDS];

    /* cava style dynamics state */
    float previous[SMP_SPECTRUM_BANDS];
    float fall[SMP_SPECTRUM_BANDS];
    float memory[SMP_SPECTRUM_BANDS];
    float peak_value[SMP_SPECTRUM_BANDS];
};

static bool spectrum_is_power_of_two(size_t value)
{
    return value >= 2 && (value & (value - 1)) == 0;
}

static size_t spectrum_bin_for(const SmpSpectrum *spectrum, double frequency)
{
    double position;

    if (!(frequency > 0.0)) {
        return 0;
    }

    position = frequency * (double)spectrum->window_size / (double)spectrum->sample_rate;
    if (position <= 0.0) {
        return 0;
    }
    if (position >= (double)spectrum->bin_limit) {
        return spectrum->bin_limit;
    }
    return (size_t)position;
}

/*
 * Splits the configured range into logarithmic bands. Frequencies above
 * Nyquist collapse onto the Nyquist bin, which matches the permission for
 * empty bands to reuse their nearest bin.
 */
static void spectrum_compute_bands(SmpSpectrum *spectrum)
{
    double nyquist = (double)spectrum->sample_rate * 0.5;
    double log_ratio = log(SMP_SPECTRUM_MAX_HZ / SMP_SPECTRUM_MIN_HZ);
    size_t band;

    for (band = 0; band < SMP_SPECTRUM_BANDS; band++) {
        double low_fraction = (double)band / (double)SMP_SPECTRUM_BANDS;
        double high_fraction = (double)(band + 1) / (double)SMP_SPECTRUM_BANDS;
        double low_hz = SMP_SPECTRUM_MIN_HZ * exp(log_ratio * low_fraction);
        double high_hz = SMP_SPECTRUM_MIN_HZ * exp(log_ratio * high_fraction);
        size_t low;
        size_t high;

        if (low_hz > nyquist) {
            low_hz = nyquist;
        }
        if (high_hz > nyquist) {
            high_hz = nyquist;
        }

        low = spectrum_bin_for(spectrum, low_hz);
        high = spectrum_bin_for(spectrum, high_hz);
        if (high < low) {
            high = low;
        }
        spectrum->band_low[band] = low;
        spectrum->band_high[band] = high;
    }
}

static float spectrum_band_level(float magnitude, size_t window_size)
{
    double normalized;
    double decibels;
    double level;

    if (!(magnitude > 0.0f)) {
        return 0.0f;
    }

    /*
     * The Hann window has coherent gain close to 0.5; for N >= 3 its values
     * sum to (N - 1) / 2, so a bin-centred sine of amplitude A has a raw FFT
     * magnitude of A * (N - 1) / 4. Scale that back to an amplitude and
     * normalise against the int16 full scale.
     */
    normalized =
        ((double)magnitude * 4.0) / ((double)(window_size - 1) * SMP_SPECTRUM_FULL_SCALE);
    if (!(normalized > 0.0)) {
        return 0.0f;
    }

    decibels = 20.0 * log10(normalized);
    if (decibels <= SMP_SPECTRUM_DB_FLOOR) {
        return 0.0f;
    }
    if (decibels >= 0.0) {
        return 1.0f;
    }

    level = (decibels - SMP_SPECTRUM_DB_FLOOR) / -SMP_SPECTRUM_DB_FLOOR;
    return (float)level;
}

static float spectrum_magnitude(const SmpSpectrum *spectrum, size_t bin)
{
    float real = spectrum->real[bin];
    float imaginary = spectrum->imaginary[bin];

    return sqrtf(real * real + imaginary * imaginary);
}

SmpSpectrum *smp_spectrum_create(int sample_rate, size_t window_size)
{
    SmpSpectrum *spectrum;

    if (sample_rate <= 0 || !spectrum_is_power_of_two(window_size)) {
        return NULL;
    }

    spectrum = (SmpSpectrum *)calloc(1, sizeof(*spectrum));
    if (spectrum == NULL) {
        return NULL;
    }

    spectrum->sample_rate = sample_rate;
    spectrum->window_size = window_size;
    spectrum->bin_limit = window_size / 2;

    spectrum->mutex = smp_mutex_create();
    spectrum->samples = (int16_t *)calloc(window_size, sizeof(*spectrum->samples));
    spectrum->real = (float *)calloc(window_size, sizeof(*spectrum->real));
    spectrum->imaginary = (float *)calloc(window_size, sizeof(*spectrum->imaginary));

    if (spectrum->mutex == NULL || spectrum->samples == NULL || spectrum->real == NULL
        || spectrum->imaginary == NULL)
    {
        smp_spectrum_destroy(spectrum);
        return NULL;
    }

    spectrum_compute_bands(spectrum);
    return spectrum;
}

void smp_spectrum_destroy(SmpSpectrum *spectrum)
{
    if (spectrum == NULL) {
        return;
    }

    free(spectrum->samples);
    free(spectrum->real);
    free(spectrum->imaginary);
    smp_mutex_destroy(spectrum->mutex);
    free(spectrum);
}

void smp_spectrum_feed(SmpSpectrum *spectrum, const int16_t *samples, size_t count)
{
    size_t index;

    if (spectrum == NULL || samples == NULL || count == 0) {
        return;
    }

    smp_mutex_lock(spectrum->mutex);
    for (index = 0; index < count; index++) {
        spectrum->samples[spectrum->write_index] = samples[index];
        spectrum->write_index++;
        if (spectrum->write_index >= spectrum->window_size) {
            spectrum->write_index = 0;
        }
        if (spectrum->filled < spectrum->window_size) {
            spectrum->filled++;
        }
    }
    smp_mutex_unlock(spectrum->mutex);
}

void smp_spectrum_update(SmpSpectrum *spectrum, float dt_seconds)
{
    float delta_time;
    size_t index;

    if (spectrum == NULL) {
        return;
    }

    delta_time = isfinite(dt_seconds) && dt_seconds > 0.0f ? dt_seconds : 0.0f;

    smp_mutex_lock(spectrum->mutex);

    memset(spectrum->real, 0, spectrum->window_size * sizeof(*spectrum->real));
    memset(spectrum->imaginary, 0, spectrum->window_size * sizeof(*spectrum->imaginary));

    if (spectrum->filled > 0) {
        size_t start = spectrum->write_index + spectrum->window_size - spectrum->filled;

        if (start >= spectrum->window_size) {
            start -= spectrum->window_size;
        }
        for (index = 0; index < spectrum->filled; index++) {
            size_t source = start + index;

            if (source >= spectrum->window_size) {
                source -= spectrum->window_size;
            }
            spectrum->real[index] = (float)spectrum->samples[source];
        }
    }

    smp_fft_hann_window(spectrum->real, spectrum->window_size);
    smp_fft(spectrum->real, spectrum->imaginary, spectrum->window_size);

    double framerate_mod;
    double gravity_mod;
    double integral_mod;

    {
        float framerate = delta_time > 0.0f ? 1.0f / delta_time : SMP_CAVA_REFERENCE_RATE;

        if (framerate < 1.0f) {
            framerate = 1.0f;
        } else if (framerate > 240.0f) {
            framerate = 240.0f;
        }
        framerate_mod = (double)SMP_CAVA_REFERENCE_RATE / (double)framerate;
        gravity_mod = pow(framerate_mod, 2.5) * 2.0 / (double)SMP_CAVA_NOISE_REDUCTION;
        integral_mod = pow(framerate_mod, 0.1);
    }

    for (index = 0; index < SMP_SPECTRUM_BANDS; index++) {
        size_t bin;
        float magnitude = 0.0f;
        float target;
        float band;
        float peak;

        for (bin = spectrum->band_low[index]; bin <= spectrum->band_high[index]; bin++) {
            float candidate = spectrum_magnitude(spectrum, bin);

            if (candidate > magnitude) {
                magnitude = candidate;
            }
        }

        target = spectrum_band_level(magnitude, spectrum->window_size);

        /*
         * cava: instant rise to the peak, accelerating gravity fall otherwise,
         * followed by an integral (memory) term. Kept in double precision to
         * avoid visible drift at 30 fps.
         */
        band = target;
        if (band < spectrum->previous[index]) {
            double gravity = (double)spectrum->peak_value[index]
                             * (1.0 - (double)spectrum->fall[index] * (double)spectrum->fall[index]
                                          * gravity_mod);

            if (gravity < 0.0) {
                gravity = 0.0;
            }
            band = (float)gravity;
            spectrum->fall[index] += SMP_CAVA_FALL_STEP;
        } else {
            spectrum->peak_value[index] = band;
            spectrum->fall[index] = 0.0f;
        }
        spectrum->previous[index] = band;

        band = spectrum->memory[index] * SMP_CAVA_NOISE_REDUCTION / (float)integral_mod + band;
        spectrum->memory[index] = band;

        if (band < 0.0f) {
            band = 0.0f;
        } else if (band > 1.0f) {
            band = 1.0f;
        }
        spectrum->bands[index] = band;

        peak = spectrum->peaks[index];
        if (band >= peak) {
            peak = band;
            spectrum->hold[index] = SMP_SPECTRUM_PEAK_HOLD;
        } else if (spectrum->hold[index] > 0.0f) {
            spectrum->hold[index] -= delta_time;
            if (spectrum->hold[index] < 0.0f) {
                spectrum->hold[index] = 0.0f;
            }
        } else {
            peak -= SMP_SPECTRUM_PEAK_FALL * delta_time;
            if (peak < band) {
                peak = band;
            }
        }
        if (peak < 0.0f) {
            peak = 0.0f;
        } else if (peak > 1.0f) {
            peak = 1.0f;
        }
        spectrum->peaks[index] = peak;
    }

    smp_mutex_unlock(spectrum->mutex);
}

size_t smp_spectrum_band_count(const SmpSpectrum *spectrum)
{
    if (spectrum == NULL) {
        return 0;
    }
    return SMP_SPECTRUM_BANDS;
}

float smp_spectrum_band(const SmpSpectrum *spectrum, size_t index)
{
    float value;

    if (spectrum == NULL || index >= SMP_SPECTRUM_BANDS) {
        return 0.0f;
    }

    smp_mutex_lock(spectrum->mutex);
    value = spectrum->bands[index];
    smp_mutex_unlock(spectrum->mutex);
    return value;
}

float smp_spectrum_peak(const SmpSpectrum *spectrum, size_t index)
{
    float value;

    if (spectrum == NULL || index >= SMP_SPECTRUM_BANDS) {
        return 0.0f;
    }

    smp_mutex_lock(spectrum->mutex);
    value = spectrum->peaks[index];
    smp_mutex_unlock(spectrum->mutex);
    return value;
}

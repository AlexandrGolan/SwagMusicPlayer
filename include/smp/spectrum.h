/*
 * SMP - Swag Music Player
 * Logarithmic FFT spectrum analyser for the bar visualiser.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_SPECTRUM_H
#define SMP_SPECTRUM_H

#include <stddef.h>
#include <stdint.h>

#define SMP_SPECTRUM_BANDS 16

typedef struct SmpSpectrum SmpSpectrum;

/*
 * Creates an analyser. `window_size` is normally 1024 and must be a power of
 * two. Returns NULL on invalid arguments or allocation failure.
 */
SmpSpectrum *smp_spectrum_create(int sample_rate, size_t window_size);

void smp_spectrum_destroy(SmpSpectrum *spectrum);

/*
 * Appends mono samples. Safe to call from the playback thread; the analyser
 * keeps the newest `window_size` samples for the next update.
 */
void smp_spectrum_feed(SmpSpectrum *spectrum, const int16_t *samples, size_t count);

/*
 * Computes the bands from the latest window; `dt_seconds` is the time since
 * the previous call. Values are in 0..1. Callable from the UI thread.
 */
void smp_spectrum_update(SmpSpectrum *spectrum, float dt_seconds);

size_t smp_spectrum_band_count(const SmpSpectrum *spectrum);

/* Current bar height in 0..1 and the falling peak marker in 0..1. */
float smp_spectrum_band(const SmpSpectrum *spectrum, size_t index);
float smp_spectrum_peak(const SmpSpectrum *spectrum, size_t index);

#endif /* SMP_SPECTRUM_H */

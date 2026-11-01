/*
 * SMP - Swag Music Player
 * Tests for the FFT and spectrum analyser.
 *
 * SPDX-License-Identifier: MIT
 */
#include <math.h>
#include <stdint.h>

#include "smp/fft.h"
#include "smp/spectrum.h"
#include "test.h"

#define TEST_RATE 48000
#define TEST_WINDOW 1024
#define TEST_PI 3.14159265358979323846
/* The bands are log spaced from 40 Hz to 16 kHz; derive the index instead of hardcoding it. */
#define TEST_440_BAND                                                                          \
    ((size_t)((log(440.0 / 40.0) / log(16000.0 / 40.0)) * (double)SMP_SPECTRUM_BANDS))
#define TEST_FAR_BAND (SMP_SPECTRUM_BANDS - 1)

void suite_spectrum(void);

static void fill_sine(int16_t *samples,
                      size_t count,
                      double frequency,
                      double amplitude,
                      size_t start_index)
{
    size_t index;

    for (index = 0; index < count; index++) {
        double position = (double)(start_index + index);
        double phase = 2.0 * TEST_PI * frequency * position / (double)TEST_RATE;

        samples[index] = (int16_t)(amplitude * sin(phase));
    }
}

static void test_fft_sine_bin(void)
{
    static float real[TEST_WINDOW];
    static float imaginary[TEST_WINDOW];
    size_t index;
    size_t best_index = 0;
    float best_magnitude = 0.0f;

    smp_test_begin("fft of a 1000 Hz sine peaks in bin 21");
    for (index = 0; index < (size_t)TEST_WINDOW; index++) {
        double phase = 2.0 * TEST_PI * 1000.0 * (double)index / (double)TEST_RATE;

        real[index] = (float)sin(phase);
        imaginary[index] = 0.0f;
    }

    smp_fft(real, imaginary, (size_t)TEST_WINDOW);

    for (index = 0; index <= (size_t)TEST_WINDOW / 2; index++) {
        float magnitude = sqrtf(real[index] * real[index] + imaginary[index] * imaginary[index]);

        if (magnitude > best_magnitude) {
            best_magnitude = magnitude;
            best_index = index;
        }
    }

    SMP_CHECK_INT(best_index, 21);
    SMP_CHECK(best_magnitude > 100.0f);
    smp_test_end();
}

static void test_fft_silence(void)
{
    static float real[TEST_WINDOW];
    static float imaginary[TEST_WINDOW];
    size_t index;
    int all_zero = 1;

    smp_test_begin("fft of silence has all-zero magnitudes");
    for (index = 0; index < (size_t)TEST_WINDOW; index++) {
        real[index] = 0.0f;
        imaginary[index] = 0.0f;
    }

    smp_fft(real, imaginary, (size_t)TEST_WINDOW);

    for (index = 0; index <= (size_t)TEST_WINDOW / 2; index++) {
        float magnitude = sqrtf(real[index] * real[index] + imaginary[index] * imaginary[index]);

        if (magnitude != 0.0f) {
            all_zero = 0;
        }
    }

    SMP_CHECK(all_zero);
    smp_test_end();
}

static void test_hann_window(void)
{
    float window[8];
    size_t count = sizeof(window) / sizeof(window[0]);
    size_t index;

    smp_test_begin("hann window tapers to zero at both ends");
    for (index = 0; index < count; index++) {
        window[index] = 1.0f;
    }

    smp_fft_hann_window(window, count);

    SMP_CHECK_NEAR(window[0], 0.0f, 1e-6);
    SMP_CHECK_NEAR(window[count - 1], 0.0f, 1e-6);
    SMP_CHECK(window[count / 2] > 0.9f);
    SMP_CHECK(window[1] > 0.0f && window[1] < 0.25f);
    SMP_CHECK_NEAR(window[count - 2], window[1], 1e-6);
    smp_test_end();
}

static void test_spectrum_silence(void)
{
    static int16_t silence[TEST_WINDOW];
    SmpSpectrum *spectrum = smp_spectrum_create(TEST_RATE, (size_t)TEST_WINDOW);
    size_t band;
    size_t count = 0;
    float loudest_band = 0.0f;
    float loudest_peak = 0.0f;
    int iteration;

    smp_test_begin("silence leaves every band and peak at zero");
    SMP_CHECK(spectrum != NULL);
    if (spectrum == NULL) {
        smp_test_end();
        return;
    }

    count = smp_spectrum_band_count(spectrum);
    SMP_CHECK_INT(count, SMP_SPECTRUM_BANDS);

    smp_spectrum_feed(spectrum, silence, (size_t)TEST_WINDOW);
    for (iteration = 0; iteration < 20; iteration++) {
        smp_spectrum_update(spectrum, 1.0f / 60.0f);
    }

    for (band = 0; band < count; band++) {
        float value = smp_spectrum_band(spectrum, band);
        float peak = smp_spectrum_peak(spectrum, band);

        if (value > loudest_band) {
            loudest_band = value;
        }
        if (peak > loudest_peak) {
            loudest_peak = peak;
        }
    }

    SMP_CHECK_NEAR(loudest_band, 0.0f, 1e-6);
    SMP_CHECK_NEAR(loudest_peak, 0.0f, 1e-6);
    SMP_CHECK_NEAR(smp_spectrum_band(spectrum, count), 0.0f, 1e-6);
    SMP_CHECK_NEAR(smp_spectrum_peak(spectrum, count + 1u), 0.0f, 1e-6);

    smp_spectrum_destroy(spectrum);

    smp_spectrum_destroy(NULL);
    smp_spectrum_feed(NULL, silence, (size_t)TEST_WINDOW);
    smp_spectrum_update(NULL, 1.0f / 60.0f);
    SMP_CHECK_NEAR(smp_spectrum_band(NULL, 0), 0.0f, 1e-6);
    SMP_CHECK_NEAR(smp_spectrum_peak(NULL, 0), 0.0f, 1e-6);
    SMP_CHECK_INT(smp_spectrum_band_count(NULL), 0);
    smp_test_end();
}

static void test_spectrum_tone_band(void)
{
    static int16_t tone[TEST_WINDOW];
    SmpSpectrum *spectrum = smp_spectrum_create(TEST_RATE, (size_t)TEST_WINDOW);
    size_t offset;
    int iteration;
    float band;
    float peak;
    float far;

    smp_test_begin("440 Hz tone lifts its logarithmic band");
    SMP_CHECK(spectrum != NULL);
    if (spectrum == NULL) {
        smp_test_end();
        return;
    }

    fill_sine(tone, (size_t)TEST_WINDOW, 440.0, 24000.0, 0);

    for (offset = 0; offset < (size_t)TEST_WINDOW; offset += 256) {
        size_t chunk = (size_t)TEST_WINDOW - offset;

        if (chunk > 256) {
            chunk = 256;
        }
        smp_spectrum_feed(spectrum, tone + offset, chunk);
    }

    for (iteration = 0; iteration < 30; iteration++) {
        smp_spectrum_update(spectrum, 1.0f / 60.0f);
    }

    band = smp_spectrum_band(spectrum, (size_t)TEST_440_BAND);
    peak = smp_spectrum_peak(spectrum, (size_t)TEST_440_BAND);
    far = smp_spectrum_band(spectrum, (size_t)TEST_FAR_BAND);

    SMP_CHECK(band > 0.5f);
    SMP_CHECK(band > far + 0.3f);
    SMP_CHECK(far < 0.05f);
    SMP_CHECK(peak >= band - 1e-4f);
    SMP_CHECK(peak <= 1.0f);

    smp_spectrum_destroy(spectrum);
    smp_test_end();
}

static void test_spectrum_chunking(void)
{
    static int16_t tone[TEST_WINDOW];
    SmpSpectrum *whole = smp_spectrum_create(TEST_RATE, (size_t)TEST_WINDOW);
    SmpSpectrum *chunked = smp_spectrum_create(TEST_RATE, (size_t)TEST_WINDOW);
    size_t offset;
    size_t band;
    int iteration;
    float max_band_difference = 0.0f;
    float max_peak_difference = 0.0f;

    smp_test_begin("feed chunking does not change the spectrum");
    SMP_CHECK(whole != NULL);
    SMP_CHECK(chunked != NULL);
    if (whole == NULL || chunked == NULL) {
        smp_spectrum_destroy(whole);
        smp_spectrum_destroy(chunked);
        smp_test_end();
        return;
    }

    fill_sine(tone, (size_t)TEST_WINDOW, 440.0, 20000.0, 0);
    smp_spectrum_feed(whole, tone, (size_t)TEST_WINDOW);

    for (offset = 0; offset < (size_t)TEST_WINDOW; offset += 137) {
        size_t chunk = (size_t)TEST_WINDOW - offset;

        if (chunk > 137) {
            chunk = 137;
        }
        smp_spectrum_feed(chunked, tone + offset, chunk);
    }

    for (iteration = 0; iteration < 5; iteration++) {
        smp_spectrum_update(whole, 1.0f / 60.0f);
        smp_spectrum_update(chunked, 1.0f / 60.0f);
    }

    for (band = 0; band < (size_t)SMP_SPECTRUM_BANDS; band++) {
        float whole_band = smp_spectrum_band(whole, band);
        float chunked_band = smp_spectrum_band(chunked, band);
        float whole_peak = smp_spectrum_peak(whole, band);
        float chunked_peak = smp_spectrum_peak(chunked, band);
        float band_difference = fabsf(whole_band - chunked_band);
        float peak_difference = fabsf(whole_peak - chunked_peak);

        if (band_difference > max_band_difference) {
            max_band_difference = band_difference;
        }
        if (peak_difference > max_peak_difference) {
            max_peak_difference = peak_difference;
        }
    }

    SMP_CHECK(max_band_difference <= 1e-3f);
    SMP_CHECK(max_peak_difference <= 1e-3f);

    smp_spectrum_destroy(whole);
    smp_spectrum_destroy(chunked);
    smp_test_end();
}

static void test_spectrum_bounds(void)
{
    static int16_t tone[TEST_WINDOW];
    static int16_t silence[TEST_WINDOW];
    SmpSpectrum *spectrum = smp_spectrum_create(TEST_RATE, (size_t)TEST_WINDOW);
    size_t band;
    int iteration;
    int bands_in_range = 1;
    int peaks_in_range = 1;

    smp_test_begin("bars and peaks stay within zero and one");
    SMP_CHECK(spectrum != NULL);
    if (spectrum == NULL) {
        smp_test_end();
        return;
    }

    fill_sine(tone, (size_t)TEST_WINDOW, 880.0, 32000.0, 0);

    for (iteration = 0; iteration < 200; iteration++) {
        if ((iteration % 8) == 0) {
            smp_spectrum_feed(spectrum, tone, (size_t)TEST_WINDOW);
        } else if ((iteration % 8) == 4) {
            smp_spectrum_feed(spectrum, silence, (size_t)TEST_WINDOW);
        }
        smp_spectrum_update(spectrum, 0.05f);
    }

    for (band = 0; band < (size_t)SMP_SPECTRUM_BANDS; band++) {
        float value = smp_spectrum_band(spectrum, band);
        float peak = smp_spectrum_peak(spectrum, band);

        if (!(value >= 0.0f && value <= 1.0f)) {
            bands_in_range = 0;
        }
        if (!(peak >= 0.0f && peak <= 1.0f)) {
            peaks_in_range = 0;
        }
        if (peak < value - 1e-5f) {
            peaks_in_range = 0;
        }
    }

    SMP_CHECK(bands_in_range);
    SMP_CHECK(peaks_in_range);

    smp_spectrum_update(spectrum, NAN);
    smp_spectrum_update(spectrum, -1.0f);
    SMP_CHECK(smp_spectrum_band(spectrum, 0) >= 0.0f);
    SMP_CHECK(smp_spectrum_peak(spectrum, 0) <= 1.0f);

    smp_spectrum_destroy(spectrum);
    smp_test_end();
}

static void test_spectrum_invalid(void)
{
    smp_test_begin("invalid analyser arguments are rejected");
    SMP_CHECK(smp_spectrum_create(0, (size_t)TEST_WINDOW) == NULL);
    SMP_CHECK(smp_spectrum_create(-TEST_RATE, (size_t)TEST_WINDOW) == NULL);
    SMP_CHECK(smp_spectrum_create(TEST_RATE, 0) == NULL);
    SMP_CHECK(smp_spectrum_create(TEST_RATE, 1) == NULL);
    SMP_CHECK(smp_spectrum_create(TEST_RATE, 1000) == NULL);
    smp_test_end();
}

static void test_gravity_decay(void)
{
    SmpSpectrum *spectrum;
    int16_t tone[TEST_WINDOW];
    int16_t silence[TEST_WINDOW];
    float loud;
    float after_one;
    float after_many;
    int iteration;

    smp_test_begin("bars fall like cava instead of snapping down");
    spectrum = smp_spectrum_create(TEST_RATE, TEST_WINDOW);
    SMP_CHECK(spectrum != NULL);
    if (spectrum == NULL) {
        smp_test_end();
        return;
    }

    fill_sine(tone, (size_t)TEST_WINDOW, 440.0, 24000.0, 0);
    fill_sine(silence, (size_t)TEST_WINDOW, 440.0, 0.0, 0);

    smp_spectrum_feed(spectrum, tone, TEST_WINDOW);
    for (iteration = 0; iteration < 30; iteration++) {
        smp_spectrum_update(spectrum, 1.0f / 60.0f);
    }
    loud = smp_spectrum_band(spectrum, TEST_440_BAND);
    SMP_CHECK(loud > 0.5f);

    smp_spectrum_feed(spectrum, silence, TEST_WINDOW);
    smp_spectrum_update(spectrum, 1.0f / 60.0f);
    after_one = smp_spectrum_band(spectrum, TEST_440_BAND);
    SMP_CHECK(after_one > loud * 0.5f);

    for (iteration = 0; iteration < 40; iteration++) {
        smp_spectrum_update(spectrum, 1.0f / 60.0f);
    }
    after_many = smp_spectrum_band(spectrum, TEST_440_BAND);
    SMP_CHECK(after_many < after_one);
    SMP_CHECK(after_many < 0.2f);

    smp_spectrum_destroy(spectrum);
    smp_test_end();
}

void suite_spectrum(void)
{
    smp_test_suite("spectrum");
    test_gravity_decay();
    test_fft_sine_bin();
    test_fft_silence();
    test_hann_window();
    test_spectrum_silence();
    test_spectrum_tone_band();
    test_spectrum_chunking();
    test_spectrum_bounds();
    test_spectrum_invalid();
}

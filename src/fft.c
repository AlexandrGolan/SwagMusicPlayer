/*
 * SMP - Swag Music Player
 * In-place iterative radix-2 Cooley-Tukey FFT and Hann window.
 *
 * SPDX-License-Identifier: MIT
 */
#include "smp/fft.h"

#include <math.h>

#define SMP_FFT_PI 3.14159265358979323846

/* Reverses the low `bit_count` bits of `value`. */
static size_t fft_reverse_bits(size_t value, unsigned int bit_count)
{
    size_t reversed = 0;
    unsigned int bit;

    for (bit = 0; bit < bit_count; bit++) {
        reversed = (reversed << 1) | (value & (size_t)1);
        value >>= 1;
    }
    return reversed;
}

void smp_fft(float *real, float *imaginary, size_t count)
{
    size_t remaining;
    unsigned int bits = 0;
    size_t index;
    size_t stage;

    if (real == NULL || imaginary == NULL || count < 2) {
        return;
    }
    if ((count & (count - 1)) != 0) {
        return;
    }

    remaining = count;
    while (remaining > 1) {
        remaining >>= 1;
        bits++;
    }

    for (index = 1; index < count; index++) {
        size_t reversed = fft_reverse_bits(index, bits);

        if (reversed > index) {
            float swap_real = real[index];
            float swap_imaginary = imaginary[index];

            real[index] = real[reversed];
            imaginary[index] = imaginary[reversed];
            real[reversed] = swap_real;
            imaginary[reversed] = swap_imaginary;
        }
    }

    for (stage = 2; stage <= count; stage <<= 1) {
        size_t half = stage / 2;
        double step_angle = -2.0 * SMP_FFT_PI / (double)stage;
        double step_real = cos(step_angle);
        double step_imaginary = sin(step_angle);
        size_t block;

        for (block = 0; block < count; block += stage) {
            double twiddle_real = 1.0;
            double twiddle_imaginary = 0.0;
            size_t offset;

            for (offset = 0; offset < half; offset++) {
                size_t even = block + offset;
                size_t odd = even + half;
                float odd_real = real[odd];
                float odd_imaginary = imaginary[odd];
                float twiddled_real = odd_real * (float)twiddle_real
                                      - odd_imaginary * (float)twiddle_imaginary;
                float twiddled_imaginary = odd_real * (float)twiddle_imaginary
                                           + odd_imaginary * (float)twiddle_real;
                double next_twiddle_real;

                real[odd] = real[even] - twiddled_real;
                imaginary[odd] = imaginary[even] - twiddled_imaginary;
                real[even] += twiddled_real;
                imaginary[even] += twiddled_imaginary;

                next_twiddle_real =
                    twiddle_real * step_real - twiddle_imaginary * step_imaginary;
                twiddle_imaginary =
                    twiddle_real * step_imaginary + twiddle_imaginary * step_real;
                twiddle_real = next_twiddle_real;
            }
        }
    }
}

void smp_fft_hann_window(float *buffer, size_t count)
{
    size_t index;

    if (buffer == NULL || count < 2) {
        return;
    }

    for (index = 0; index < count; index++) {
        double angle = 2.0 * SMP_FFT_PI * (double)index / (double)(count - 1);
        double window = 0.5 * (1.0 - cos(angle));

        buffer[index] = (float)((double)buffer[index] * window);
    }
}

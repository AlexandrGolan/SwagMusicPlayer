/*
 * SMP - Swag Music Player
 * In-place radix-2 FFT and Hann window.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_FFT_H
#define SMP_FFT_H

#include <stddef.h>

/*
 * In-place iterative radix-2 Cooley-Tukey FFT. `count` must be a power of
 * two. Both arrays must hold `count` elements. The call does nothing when
 * either array is NULL or `count` is not a supported power of two.
 */
void smp_fft(float *real, float *imaginary, size_t count);

/* Hann window applied in place. */
void smp_fft_hann_window(float *buffer, size_t count);

#endif /* SMP_FFT_H */

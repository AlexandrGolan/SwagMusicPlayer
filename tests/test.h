/*
 * SMP - Swag Music Player
 * Minimal test harness (no external dependencies).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SMP_TEST_H
#define SMP_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int smp_test_failures;
extern int smp_test_checks;

void smp_test_suite(const char *name);
void smp_test_begin(const char *name);
void smp_test_end(void);

#define SMP_CHECK(condition)                                                                       \
    do {                                                                                           \
        smp_test_checks++;                                                                         \
        if (!(condition)) {                                                                        \
            smp_test_failures++;                                                                   \
            fprintf(stderr,                                                                        \
                    "\n    FAIL %s:%d: %s\n",                                                      \
                    __FILE__,                                                                      \
                    __LINE__,                                                                      \
                    #condition);                                                                   \
        }                                                                                          \
    } while (0)

#define SMP_CHECK_INT(actual, expected)                                                            \
    do {                                                                                           \
        long long smp_actual_ = (long long)(actual);                                               \
        long long smp_expected_ = (long long)(expected);                                           \
        smp_test_checks++;                                                                         \
        if (smp_actual_ != smp_expected_) {                                                        \
            smp_test_failures++;                                                                   \
            fprintf(stderr,                                                                        \
                    "\n    FAIL %s:%d: %s == %s (got %lld, expected %lld)\n",                      \
                    __FILE__,                                                                      \
                    __LINE__,                                                                      \
                    #actual,                                                                       \
                    #expected,                                                                     \
                    smp_actual_,                                                                   \
                    smp_expected_);                                                                \
        }                                                                                          \
    } while (0)

#define SMP_CHECK_STR(actual, expected)                                                            \
    do {                                                                                           \
        const char *smp_actual_ = (actual);                                                        \
        const char *smp_expected_ = (expected);                                                    \
        smp_test_checks++;                                                                         \
        if (smp_actual_ == NULL || strcmp(smp_actual_, smp_expected_) != 0) {                      \
            smp_test_failures++;                                                                   \
            fprintf(stderr,                                                                        \
                    "\n    FAIL %s:%d: %s == \"%s\" (got \"%s\")\n",                               \
                    __FILE__,                                                                      \
                    __LINE__,                                                                      \
                    #actual,                                                                       \
                    smp_expected_,                                                                 \
                    smp_actual_ != NULL ? smp_actual_ : "(null)");                                \
        }                                                                                          \
    } while (0)

#define SMP_CHECK_NEAR(actual, expected, tolerance)                                                \
    do {                                                                                           \
        double smp_actual_ = (double)(actual);                                                     \
        double smp_expected_ = (double)(expected);                                                 \
        double smp_tolerance_ = (double)(tolerance);                                               \
        smp_test_checks++;                                                                         \
        if (smp_actual_ < smp_expected_ - smp_tolerance_                                           \
            || smp_actual_ > smp_expected_ + smp_tolerance_)                                       \
        {                                                                                          \
            smp_test_failures++;                                                                   \
            fprintf(stderr,                                                                        \
                    "\n    FAIL %s:%d: %s ~= %s (got %.4f, expected %.4f +/- %.4f)\n",            \
                    __FILE__,                                                                      \
                    __LINE__,                                                                      \
                    #actual,                                                                       \
                    #expected,                                                                     \
                    smp_actual_,                                                                   \
                    smp_expected_,                                                                 \
                    smp_tolerance_);                                                               \
        }                                                                                          \
    } while (0)

void suite_common(void);
void suite_str(void);
void suite_fs(void);
void suite_pcm(void);
void suite_tags(void);
void suite_media(void);
void suite_playlist(void);
void suite_frame(void);
void suite_cover(void);
void suite_player(void);

#endif /* SMP_TEST_H */
